#include "rf_subghz.h"
#include "rf_subghz_proto.h"
#include "root_config.h"

#if ROOT_ENABLE_SUBGHZ

#include <SPI.h>
#include <RadioLib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <esp_heap_caps.h>
#include "root_gps.h"

static SPIClass gSpi(FSPI);
static Module* gMod = nullptr;
static CC1101* gRadio = nullptr;
static bool gOk = false;
static bool gEnabled = true;
static bool gHopping = true;
static float gFixedMhz = 433.92f;

struct ScanSlot {
  float mhz;
  bool ook;
  uint16_t dwellMs;  // longer on OOK — key fobs are short bursts
  const char* name;
};
// OOK energy dwell on 315 / 433.92 / 868 / 915, then short FSK passes.
// dwellMs is mutable via ./omni subghz dwell (automotive park / long listen).
static ScanSlot SLOTS[] = {
    {315.00f, true, 1100, "315"},
    {433.92f, true, 1100, "433"},
    {868.00f, true, 1100, "868"},
    {915.00f, true, 1100, "915"},
    {315.00f, false, 200, "315"},
    {433.92f, false, 200, "433"},
    {868.00f, false, 200, "868"},
    {915.00f, false, 200, "915"},
};
static const uint8_t SLOT_N = sizeof(SLOTS) / sizeof(SLOTS[0]);

static int bandIndexOok(float mhz) {
  if (mhz < 1.0f) return -1;  // all
  if (mhz < 350.0f) return 0;
  if (mhz < 500.0f) return 1;
  if (mhz < 900.0f) return 2;
  return 3;
}

struct BandCal {
  float noiseEma = -110.0f;
  int8_t peakRssi = -120;
  int8_t eventPeak = -120;
  uint32_t carrierMs = 0;
  uint16_t bursts = 0;
  uint16_t packets = 0;
  uint32_t lastHitMs = 0;
  uint32_t lastEmitMs = 0;
  bool inBurst = false;
  bool emittedThisBurst = false;
  uint8_t hotSamples = 0;
};

static BandCal gCal[SLOT_N];
static uint8_t gSlotIdx = 0;
static uint32_t gBandEnterMs = 0;
static uint32_t gLastStatusMs = 0;
static uint32_t gTotalBursts = 0;
static bool gCurrentOok = false;
static uint32_t gHitSeq = 0;

static SubGhzHit gHitQ[24];
static uint8_t gHitHead = 0;
static uint8_t gHitTail = 0;
static const uint8_t kActivityMac[6] = {0x02, 0x53, 0x00, 0x00, 0x00, 0x01};

static SubGhzRawPacket* gRaw = nullptr;
static uint16_t gRawCap = 0;
static uint16_t gRawHead = 0;
static uint16_t gRawCount = 0;
static uint32_t gRawTotal = 0;
static float gRawFilterMhz = 0;  // 0 = none
static uint8_t* gSaveBlob = nullptr;
static size_t gSaveBlobLen = 0;
static char gSavePath[64] = "";

static void pushHit(const SubGhzHit& h) {
  gHitQ[gHitHead] = h;
  gHitHead = (gHitHead + 1) % 24;
  if (gHitHead == gHitTail) gHitTail = (gHitTail + 1) % 24;
}

static void synthMac(uint8_t* mac, uint32_t freqKhz, SubGhzDetect det, int8_t rssi) {
  // Unique-ish per detection so presses don't collapse into one fake constant-RSSI row.
  gHitSeq++;
  mac[0] = 0x02;
  mac[1] = 0x53;
  mac[2] = (uint8_t)((freqKhz >> 8) & 0xff);
  mac[3] = (uint8_t)(freqKhz & 0xff);
  mac[4] = (uint8_t)(((det & 0x03) << 6) | (((uint8_t)rssi) & 0x3f));
  mac[5] = (uint8_t)(gHitSeq & 0xff);
  if (mac[5] < 4) mac[5] |= 0x10;
}

static void synthMacFromPayload(uint8_t* mac, uint32_t freqKhz, const uint8_t* data,
                                size_t len) {
  uint32_t h = 2166136261u ^ freqKhz;
  for (size_t i = 0; i < len; i++) {
    h ^= data[i];
    h *= 16777619u;
  }
  mac[0] = 0x02;
  mac[1] = 0x53;
  mac[2] = (uint8_t)(h >> 24);
  mac[3] = (uint8_t)(h >> 16);
  mac[4] = (uint8_t)(h >> 8);
  mac[5] = (uint8_t)h;
  if (mac[5] < 4) mac[5] |= 0x10;
}

static void labelHit(SubGhzHit& h, bool ook, const SgProtoResult* proto) {
  const float mhz = h.freqKhz / 1000.0f;
  const int above = (int)h.rssi - (int)h.noiseFloor;
  const char* mode = ook ? "OOK" : "FSK";
  if (proto && proto->summary[0]) {
    // Prefer protocol/brand summary (fits device ssid ≤32)
    snprintf(h.label, sizeof(h.label), "%s", proto->summary);
  } else {
    const char* kind =
        h.detect == SG_CARRIER ? "carrier" : h.detect == SG_PACKET ? "packet" : "burst";
    snprintf(h.label, sizeof(h.label), "%.2f %s %s %ddBm", mhz, mode, kind, (int)h.rssi);
  }
  if (!h.detail[0]) {
    if (proto && proto->detail[0]) {
      snprintf(h.detail, sizeof(h.detail), "%s", proto->detail);
    } else {
      snprintf(h.detail, sizeof(h.detail), "nf %d | %+ddB | %s", (int)h.noiseFloor, above,
               mode);
    }
  }
}

static int8_t readRssiDbm() {
  if (!gRadio) return -120;
  // After receiveDirectAsync(), getRSSI() reads the live RSSI register.
  // In packet mode it returns a stale last-packet cache (identical fake values).
  float r = gRadio->getRSSI();
  if (!isfinite(r)) return -120;
  int v = (int)lroundf(r);
  if (v < -128) v = -128;
  if (v > 0) v = 0;
  return (int8_t)v;
}

static void configureSlot(const ScanSlot& sl) {
  if (!gRadio) return;
  gCurrentOok = sl.ook;
  gRadio->standby();
  gRadio->setFrequency(sl.mhz);
  gRadio->setOOK(sl.ook);
  // Wide RX BW for remotes. Always direct RX so RSSI is live (packet-mode RSSI is cached junk).
  if (sl.ook) {
    gRadio->setRxBandwidth(sl.mhz >= 800.0f ? 406.0f : 650.0f);
    gRadio->setBitRate(4.8f);
    gRadio->setFrequencyDeviation(0.0f);
  } else if (sl.mhz < 350.0f) {
    gRadio->setRxBandwidth(203.0f);
    gRadio->setBitRate(1.2f);
    gRadio->setFrequencyDeviation(5.2f);
  } else if (sl.mhz < 500.0f) {
    gRadio->setRxBandwidth(270.0f);
    gRadio->setBitRate(4.8f);
    gRadio->setFrequencyDeviation(25.0f);
  } else {
    // 868 / 915 FSK
    gRadio->setRxBandwidth(sl.mhz >= 900.0f ? 162.0f : 203.0f);
    gRadio->setBitRate(4.8f);
    gRadio->setFrequencyDeviation(45.0f);
  }
  gRadio->setCrcFiltering(false);
  // Direct async RX: live RSSI + OOK energy for key fobs (no FIFO sync needed)
  gRadio->receiveDirectAsync();
}

static void configureBand(float mhz) {
  ScanSlot sl = {mhz, true, 1100, "fix"};
  if (mhz >= 800.0f && mhz < 900.0f) sl = {868.0f, true, 1100, "868"};
  else if (mhz >= 900.0f) sl = {915.0f, true, 1100, "915"};
  else if (mhz < 350.0f) sl = {315.0f, true, 1100, "315"};
  else sl = {433.92f, true, 1100, "433"};
  configureSlot(sl);
}

static void pushRaw(float mhz, int8_t rssi, uint8_t lqi, const uint8_t* data, uint8_t len) {
  if (!gRaw || !gRawCap || !data || !len) return;
  SubGhzRawPacket& p = gRaw[gRawHead];
  p.timestampMs = millis();
  p.frequencyMhz = mhz;
  p.rssi = rssi;
  p.lqi = lqi;
  p.length = len > ROOT_SUBGHZ_RAW_BYTES ? ROOT_SUBGHZ_RAW_BYTES : len;
  GpsFix g = gpsGet();
  p.gpsValid = g.valid ? 1 : 0;
  p.lat = g.valid ? g.lat : 0;
  p.lon = g.valid ? g.lon : 0;
  memcpy(p.data, data, p.length);
  if (p.length < ROOT_SUBGHZ_RAW_BYTES) {
    memset(p.data + p.length, 0, ROOT_SUBGHZ_RAW_BYTES - p.length);
  }
  gRawHead = (uint16_t)((gRawHead + 1) % gRawCap);
  if (gRawCount < gRawCap) gRawCount++;
  gRawTotal++;
}

static void tryReadPacket(uint8_t si) {
  // Hopping scanner stays in direct RX for live RSSI / OOK energy.
  // Packet FIFO demod needs fixed-freq startReceive — skip during energy scan.
  (void)si;
}

bool subghzReady() { return gOk; }

void subghzSetEnabled(bool on) { gEnabled = on; }
bool subghzEnabled() { return gEnabled && gOk; }
void subghzSetHopping(bool hop) {
  gHopping = hop;
  if (!hop) configureBand(gFixedMhz);
}
bool subghzHopping() { return gHopping; }
bool subghzSetFrequencyMhz(float mhz) {
  if (mhz <= 0.1f) {
    gHopping = true;
    return true;
  }
  uint8_t best = 0;
  float bestD = 1e9f;
  for (uint8_t i = 0; i < SLOT_N; i++) {
    float d = fabsf(SLOTS[i].mhz - mhz);
    if (d < bestD) {
      bestD = d;
      best = i;
    }
  }
  gFixedMhz = SLOTS[best].mhz;
  gHopping = false;
  gSlotIdx = best;
  if (gOk) {
    configureSlot(SLOTS[best]);
    gBandEnterMs = millis();
  }
  return true;
}
float subghzCurrentMhz() { return gHopping ? SLOTS[gSlotIdx].mhz : gFixedMhz; }
uint32_t subghzPacketCount() { return gRawTotal ? gRawTotal : gTotalBursts; }

bool subghzSetOokDwellMs(float bandMhz, uint16_t ms) {
  if (ms < 100) ms = 100;
  if (ms > 60000) ms = 60000;
  const int bi = bandIndexOok(bandMhz);
  for (uint8_t i = 0; i < SLOT_N; i++) {
    if (!SLOTS[i].ook) continue;
    const int si = bandIndexOok(SLOTS[i].mhz);
    if (bi < 0 || si == bi) SLOTS[i].dwellMs = ms;
  }
  // Keep FSK passes short relative to OOK (max 1/4 of OOK, clamp 100–2000ms)
  uint16_t fsk = (uint16_t)(ms / 4);
  if (fsk < 100) fsk = 100;
  if (fsk > 2000) fsk = 2000;
  for (uint8_t i = 0; i < SLOT_N; i++) {
    if (SLOTS[i].ook) continue;
    const int si = bandIndexOok(SLOTS[i].mhz);
    if (bi < 0 || si == bi) SLOTS[i].dwellMs = fsk;
  }
  return true;
}

bool subghzFormatDwell(char* out, size_t n) {
  if (!out || n < 32) return false;
  size_t u = 0;
  auto ap = [&](const char* s) {
    size_t l = strlen(s);
    if (u + l + 1 >= n) l = n > u + 1 ? n - u - 1 : 0;
    if (l) {
      memcpy(out + u, s, l);
      u += l;
    }
    out[u] = 0;
  };
  auto apf = [&](const char* fmt, ...) {
    char buf[96];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof buf, fmt, args);
    va_end(args);
    ap(buf);
  };
  ap("=== SUB-GHZ DWELL ===\n");
  apf("Mode: %s\n", gHopping ? "HOPPING" : "FIXED");
  if (!gHopping) apf("Locked: %.2f MHz\n", gFixedMhz);
  ap("Band   OOK(ms)  FSK(ms)\n");
  for (uint8_t b = 0; b < 4; b++) {
    uint16_t ook = 0, fsk = 0;
    const char* name = "?";
    for (uint8_t i = 0; i < SLOT_N; i++) {
      if (bandIndexOok(SLOTS[i].mhz) != (int)b) continue;
      name = SLOTS[i].name;
      if (SLOTS[i].ook) ook = SLOTS[i].dwellMs;
      else fsk = SLOTS[i].dwellMs;
    }
    apf("%-5s  %-7u  %u\n", name, (unsigned)ook, (unsigned)fsk);
  }
  ap("\nLock one band:  ./omni subghz freq 315|433|868|915\n");
  ap("Hop all bands:  ./omni subghz hop\n");
  ap("Set dwell:      ./omni subghz dwell <ms>\n");
  ap("Per-band:       ./omni subghz dwell <315|433|868|915> <ms>\n");
  ap("(OOK park time while hopping — use long dwell for automotive RKE)\n");
  return true;
}

uint32_t subghzRawCount() { return gRawCount; }
uint32_t subghzRawTotal() { return gRawTotal; }
size_t subghzRawBytesUsed() {
  return (size_t)gRawCount * sizeof(SubGhzRawPacket);
}
void subghzRawClear() {
  gRawHead = 0;
  gRawCount = 0;
  if (gSaveBlob) {
    free(gSaveBlob);
    gSaveBlob = nullptr;
    gSaveBlobLen = 0;
    gSavePath[0] = 0;
  }
}

bool subghzRawGet(uint32_t newestIndex, SubGhzRawPacket* out) {
  if (!out || !gRaw || gRawCount == 0 || newestIndex >= gRawCount) return false;
  // newest is last written: head-1
  int idx = (int)gRawHead - 1 - (int)newestIndex;
  while (idx < 0) idx += gRawCap;
  *out = gRaw[idx % gRawCap];
  return true;
}

void subghzInit() {
  pinMode(ROOT_CC1101_GDO0, INPUT);
  gSpi.begin(ROOT_SPI_SCK, ROOT_SPI_MISO, ROOT_SPI_MOSI, ROOT_CC1101_CS);

  if (gRadio) {
    delete gRadio;
    gRadio = nullptr;
  }
  if (gMod) {
    delete gMod;
    gMod = nullptr;
  }
  // Must pass the same SPI bus we began — default SPI was a silent miss on S3.
  gMod = new Module(ROOT_CC1101_CS, ROOT_CC1101_GDO0, RADIOLIB_NC, RADIOLIB_NC, gSpi);
  gRadio = new CC1101(gMod);

  const int16_t st = gRadio->begin(433.92);
  if (st != RADIOLIB_ERR_NONE) {
    Serial.printf("root: CC1101 init failed (%d) — check SPI CS=%d SCK=%d MOSI=%d MISO=%d\n",
                  (int)st, ROOT_CC1101_CS, ROOT_SPI_SCK, ROOT_SPI_MOSI, ROOT_SPI_MISO);
    gOk = false;
    return;
  }
  gOk = true;
  gEnabled = true;
  gHopping = true;
  gSlotIdx = 0;
  gBandEnterMs = millis();
  for (uint8_t i = 0; i < SLOT_N; i++) gCal[i] = BandCal{};
  configureSlot(SLOTS[0]);

  gRawCap = ROOT_SUBGHZ_RAW_CAP;
  gRaw = (SubGhzRawPacket*)heap_caps_malloc(sizeof(SubGhzRawPacket) * gRawCap,
                                            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!gRaw) {
    gRaw = (SubGhzRawPacket*)malloc(sizeof(SubGhzRawPacket) * gRawCap);
  }
  if (!gRaw) {
    gRawCap = 8;
    gRaw = (SubGhzRawPacket*)malloc(sizeof(SubGhzRawPacket) * gRawCap);
  }
  gRawHead = 0;
  gRawCount = 0;
  gRawTotal = 0;

  // Sanity: RSSI must move a little in RX; stuck value ⇒ SPI/wiring still wrong.
  int8_t r0 = readRssiDbm();
  delay(2);
  int8_t r1 = readRssiDbm();
  Serial.printf("root: CC1101 OK · OOK energy 315/433/868/915 (1.1s each) + FSK · "
                "RSSI now %d/%d dBm (CS=%d GDO0=%d)\n",
                (int)r0, (int)r1, ROOT_CC1101_CS, ROOT_CC1101_GDO0);
}

static void emit(BandCal& c, uint8_t si, uint32_t freqKhz, int8_t rssi,
                 SubGhzDetect det) {
  const int8_t floor = (int8_t)c.noiseEma;
  const int delta = (int)rssi - (int)floor;
  if (delta < ROOT_SUBGHZ_BURST_DB) return;
  const uint32_t now = millis();
  if (c.lastEmitMs && (now - c.lastEmitMs) < ROOT_SUBGHZ_EMIT_GAP_MS &&
      det != SG_PACKET) {
    return;
  }

  // Sample GDO0 async data line → raw hex (OOK remotes / energy bursts)
  uint8_t raw[ROOT_SUBGHZ_RAW_BYTES];
  uint8_t rawLen = 0;
  if (gCurrentOok || det == SG_BURST) {
    uint8_t acc = 0;
    uint8_t bit = 0;
    // ROOT_SUBGHZ_BIT_US per sample — finer TE for PWM/Manchester decode
    const size_t wantBits = (size_t)ROOT_SUBGHZ_RAW_BYTES * 8;
    for (size_t i = 0; i < wantBits && rawLen < ROOT_SUBGHZ_RAW_BYTES; i++) {
      if (digitalRead(ROOT_CC1101_GDO0)) acc |= (uint8_t)(1u << (7 - bit));
      if (++bit >= 8) {
        raw[rawLen++] = acc;
        acc = 0;
        bit = 0;
      }
      delayMicroseconds(ROOT_SUBGHZ_BIT_US);
    }
  }

  SubGhzHit h = {};
  h.freqKhz = freqKhz;
  h.rssi = rssi;
  h.noiseFloor = floor;
  h.detect = det;
  strncpy(h.band, SLOTS[si].name, sizeof h.band - 1);

  SgProtoResult proto = {};
  bool haveProto = false;
  if (rawLen) {
    haveProto = sgClassify(raw, rawLen, SLOTS[si].mhz, gCurrentOok, &proto);
    synthMacFromPayload(h.mac, freqKhz, raw, rawLen);
    pushRaw(SLOTS[si].mhz, rssi, 0, raw, rawLen);
    // detail: protocol line preferred; keep short hex if unknown
    if (haveProto && proto.id != SG_PROTO_UNKNOWN && proto.id != SG_PROTO_NOISE) {
      snprintf(h.detail, sizeof(h.detail), "%s", proto.detail);
    } else {
      size_t di = 0;
      di += (size_t)snprintf(h.detail + di, sizeof(h.detail) - di, "%uB ",
                             (unsigned)rawLen);
      for (uint8_t i = 0; i < rawLen && di + 3 < sizeof(h.detail); i++) {
        di += (size_t)snprintf(h.detail + di, sizeof(h.detail) - di, "%02X", raw[i]);
        if (i + 1 < rawLen && di + 1 < sizeof(h.detail) && i < 15) h.detail[di++] = ' ';
        if (i >= 15) {
          if (di + 3 < sizeof(h.detail)) {
            h.detail[di++] = '.';
            h.detail[di++] = '.';
            h.detail[di] = 0;
          }
          break;
        }
      }
    }
  } else {
    synthMac(h.mac, freqKhz, det, rssi);
    // Energy-only: band prior still useful for the device list label
    haveProto = sgClassify(nullptr, 0, SLOTS[si].mhz, gCurrentOok, &proto);
    if (gCurrentOok && (SLOTS[si].mhz < 360.0f ||
                        (SLOTS[si].mhz > 400.0f && SLOTS[si].mhz < 500.0f) ||
                        (SLOTS[si].mhz >= 800.0f && SLOTS[si].mhz < 920.0f))) {
      // Prefer auto/gate wording over bare "Unknown" when we only saw energy
      snprintf(proto.summary, sizeof proto.summary, "%s OOK remote?",
               SLOTS[si].mhz >= 900 ? "915" : SLOTS[si].mhz >= 800 ? "868"
                                           : SLOTS[si].mhz >= 400  ? "433"
                                                                   : "315");
      proto.name = "OOK burst";
      proto.brands = "key fob · gate · garage · sensor (press again for ID)";
      proto.family = "unknown";
      snprintf(proto.detail, sizeof proto.detail, "energy only · mash fob for bits");
      haveProto = true;
    }
    snprintf(h.detail, sizeof(h.detail), "nf %d | %+ddB | %s energy", (int)floor, delta,
             gCurrentOok ? "OOK" : "FSK");
  }
  labelHit(h, gCurrentOok, haveProto ? &proto : nullptr);
  pushHit(h);
  c.lastHitMs = now;
  c.lastEmitMs = now;
  c.bursts++;
  gTotalBursts++;

  Serial.printf("root: subghz HIT %.2f %s %s peak=%d nf=%d (+%d) proto=%s hex=%s\n",
                SLOTS[si].mhz, gCurrentOok ? "OOK" : "FSK",
                det == SG_CARRIER ? "carrier" : det == SG_PACKET ? "packet" : "burst",
                (int)rssi, (int)floor, delta,
                haveProto && proto.name ? proto.name : "?", h.detail);
}

static void sampleBand(uint8_t si, uint32_t nowMs) {
  if (!gRadio) return;
  BandCal& c = gCal[si];
  const uint32_t freqKhz = (uint32_t)lroundf(SLOTS[si].mhz * 1000.0f);
  int8_t peak = -120;
  int8_t quiet = 127;
  uint8_t above = 0;

  // ~8 ms energy window (SAMPLES × SAMPLE_US) — catch short key-fob chirps
  for (uint8_t s = 0; s < ROOT_SUBGHZ_SAMPLES; s++) {
    const int8_t rssi = readRssiDbm();
    if (rssi > peak) peak = rssi;
    if (rssi < quiet) quiet = rssi;

    if (c.noiseEma < -99.0f) {
      c.noiseEma = (float)rssi;
    } else if (rssi <= (int8_t)(c.noiseEma + 2.0f)) {
      // Only quiet samples train the floor — never chase a TX peak upward
      c.noiseEma = c.noiseEma * 0.90f + (float)rssi * 0.10f;
    }

    if (rssi >= (int8_t)(c.noiseEma + ROOT_SUBGHZ_BURST_DB)) above++;
    delayMicroseconds(ROOT_SUBGHZ_SAMPLE_US);
  }

  c.peakRssi = peak;
  const int delta = (int)peak - (int)c.noiseEma;

  // FSK packet RX only (does not thrash OOK energy RX)
  if (!gCurrentOok) tryReadPacket(si);

  if (delta >= ROOT_SUBGHZ_BURST_DB && above >= 3) {
    if (peak > c.eventPeak) c.eventPeak = peak;
    if (!c.inBurst) {
      c.inBurst = true;
      c.emittedThisBurst = false;
      c.hotSamples = 1;
    } else if (c.hotSamples < 255) {
      c.hotSamples++;
    }
    // Require 2 consecutive hot windows (~16 ms) so register flicker doesn't fire
    if (!c.emittedThisBurst && c.hotSamples >= 2) {
      emit(c, si, freqKhz, c.eventPeak > peak ? c.eventPeak : peak, SG_BURST);
      c.emittedThisBurst = true;
    }
  } else {
    c.inBurst = false;
    c.emittedThisBurst = false;
    c.eventPeak = -120;
    c.hotSamples = 0;
  }

  if (delta >= ROOT_SUBGHZ_CARRIER_DB) {
    if (c.carrierMs == 0) c.carrierMs = nowMs;
    if (nowMs - c.carrierMs >= ROOT_SUBGHZ_CARRIER_MS) {
      emit(c, si, freqKhz, peak, SG_CARRIER);
      c.carrierMs = nowMs;
    }
  } else {
    c.carrierMs = 0;
  }
}

void subghzService(uint32_t nowMs) {
  if (!gOk || !gEnabled || !gRadio) return;

  if (gHopping) {
    const uint16_t dwell = SLOTS[gSlotIdx].dwellMs ? SLOTS[gSlotIdx].dwellMs
                                                   : (uint16_t)ROOT_SUBGHZ_DWELL_MS;
    if (nowMs - gBandEnterMs >= dwell) {
      gSlotIdx = (gSlotIdx + 1) % SLOT_N;
      gCal[gSlotIdx].inBurst = false;
      gCal[gSlotIdx].emittedThisBurst = false;
      gCal[gSlotIdx].eventPeak = -120;
      gCal[gSlotIdx].hotSamples = 0;
      configureSlot(SLOTS[gSlotIdx]);
      gBandEnterMs = nowMs;
    }
  }

  sampleBand(gSlotIdx, nowMs);
  gLastStatusMs = nowMs;
}

bool subghzPopHit(SubGhzHit* out) {
  if (!out || gHitHead == gHitTail) return false;
  *out = gHitQ[gHitTail];
  gHitTail = (gHitTail + 1) % 24;
  return true;
}

bool subghzPopActivity(SubGhzHit* out) {
  if (!out || !gOk || !gEnabled) return false;

  uint8_t bestSi = gSlotIdx;
  int8_t bestPeak = gCal[gSlotIdx].peakRssi;
  int bestDelta = bestPeak - (int8_t)gCal[gSlotIdx].noiseEma;

  for (uint8_t i = 0; i < SLOT_N; i++) {
    const int8_t peak = gCal[i].peakRssi;
    const int delta = peak - (int8_t)gCal[i].noiseEma;
    if (delta > bestDelta || (delta == bestDelta && peak > bestPeak)) {
      bestDelta = delta;
      bestPeak = peak;
      bestSi = i;
    }
  }

  if (bestPeak < ROOT_SUBGHZ_ACTIVITY_RSSI && bestDelta < ROOT_SUBGHZ_BURST_DB) return false;

  SubGhzHit h = {};
  h.freqKhz = (uint32_t)(SLOTS[bestSi].mhz * 1000.0f);
  h.rssi = bestPeak;
  h.noiseFloor = (int8_t)gCal[bestSi].noiseEma;
  h.detect = SG_BURST;
  strncpy(h.band, SLOTS[bestSi].name, sizeof h.band - 1);
  memcpy(h.mac, kActivityMac, 6);
  // Monitor slot 0-3 by nominal band (315/433/868/915), not OOK/FSK index
  uint8_t mon = 0;
  if (SLOTS[bestSi].mhz >= 900) mon = 3;
  else if (SLOTS[bestSi].mhz >= 800) mon = 2;
  else if (SLOTS[bestSi].mhz >= 400) mon = 1;
  h.mac[5] = mon;
  snprintf(h.label, sizeof(h.label), "%.0f MHz scan %ddBm (+%ddB)",
           SLOTS[bestSi].mhz, (int)bestPeak, bestDelta);
  snprintf(h.detail, sizeof(h.detail), "%s monitor", SLOTS[bestSi].ook ? "OOK" : "FSK");
  *out = h;
  return true;
}

void subghzStatusJson(char* out, size_t n) {
  if (!out || n < 8) return;
  snprintf(out, n,
           "{\"ready\":true,\"enabled\":%s,\"scan\":\"315,433,868,915\","
           "\"hopping\":%s,\"band\":\"%s\",\"freq_mhz\":%.2f,\"ook\":%s,"
           "\"dwell_ook_ms\":[%u,%u,%u,%u],\"dwell_fsk_ms\":[%u,%u,%u,%u],"
           "\"rssi\":%d,\"noise_dbm\":%.1f,\"bursts\":%lu,\"raw\":%lu,"
           "\"burst_thresh_db\":%d,\"carrier_thresh_db\":%d}",
           gEnabled ? "true" : "false",
           gHopping ? "true" : "false",
           SLOTS[gSlotIdx].name, subghzCurrentMhz(),
           gCurrentOok ? "true" : "false",
           (unsigned)SLOTS[0].dwellMs, (unsigned)SLOTS[1].dwellMs,
           (unsigned)SLOTS[2].dwellMs, (unsigned)SLOTS[3].dwellMs,
           (unsigned)SLOTS[4].dwellMs, (unsigned)SLOTS[5].dwellMs,
           (unsigned)SLOTS[6].dwellMs, (unsigned)SLOTS[7].dwellMs,
           (int)gCal[gSlotIdx].peakRssi, gCal[gSlotIdx].noiseEma,
           (unsigned long)gTotalBursts, (unsigned long)gRawTotal,
           (int)ROOT_SUBGHZ_BURST_DB, (int)ROOT_SUBGHZ_CARRIER_DB);
}

static void hexAscii(const uint8_t* d, uint8_t len, char* hex, size_t hn, char* asc, size_t an) {
  size_t hi = 0, ai = 0;
  for (uint8_t i = 0; i < len; i++) {
    if (hi + 3 < hn) hi += (size_t)snprintf(hex + hi, hn - hi, "%02X%s", d[i], i + 1 < len ? " " : "");
    if (ai + 1 < an) {
      char c = (d[i] >= 32 && d[i] < 127) ? (char)d[i] : '.';
      asc[ai++] = c;
    }
  }
  hex[hi] = 0;
  asc[ai] = 0;
}

static void b64Encode(const uint8_t* in, uint8_t n, char* out, size_t outn) {
  static const char* T =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  size_t o = 0;
  for (uint8_t i = 0; i < n && o + 4 < outn; i += 3) {
    uint32_t v = ((uint32_t)in[i]) << 16;
    if (i + 1 < n) v |= ((uint32_t)in[i + 1]) << 8;
    if (i + 2 < n) v |= in[i + 2];
    out[o++] = T[(v >> 18) & 63];
    out[o++] = T[(v >> 12) & 63];
    out[o++] = (i + 1 < n) ? T[(v >> 6) & 63] : '=';
    out[o++] = (i + 2 < n) ? T[v & 63] : '=';
  }
  out[o] = 0;
}

/** Every payload view the backend can produce — no client-side decode needed.
 *  Bits MSB-first per byte (CC1101 GDO0 sample packing). */
template <typename ApFn>
static void appendPayloadViews(ApFn ap, const uint8_t* d, uint8_t len) {
  char chunk[220];
  const unsigned bits = (unsigned)len * 8u;

  ap("--- PAYLOAD FORMATS ---\n");

  // HEX spaced
  ap("HEX: ");
  for (uint8_t i = 0; i < len; i++) {
    snprintf(chunk, sizeof chunk, "%02X%s", d[i], i + 1 < len ? " " : "");
    ap(chunk);
  }
  ap("\n");

  // HEX contiguous
  ap("HEX_CONTIG: ");
  for (uint8_t i = 0; i < len; i++) {
    snprintf(chunk, sizeof chunk, "%02X", d[i]);
    ap(chunk);
  }
  ap("\n");

  // Printable ASCII
  ap("ASCII: \"");
  for (uint8_t i = 0; i < len; i++) {
    char c = (d[i] >= 32 && d[i] < 127) ? (char)d[i] : '.';
    chunk[0] = c;
    chunk[1] = 0;
    ap(chunk);
  }
  ap("\"\n");

  // UTF-8 view (same printable escape — raw bytes may not be valid UTF-8)
  ap("UTF8: \"");
  for (uint8_t i = 0; i < len; i++) {
    char c = (d[i] >= 32 && d[i] < 127) ? (char)d[i] : '.';
    chunk[0] = c;
    chunk[1] = 0;
    ap(chunk);
  }
  ap("\"\n");

  // Base64
  {
    char b64[200];
    b64Encode(d, len, b64, sizeof b64);
    ap("BASE64: ");
    ap(b64);
    ap("\n");
  }

  // Bits byte-grouped
  ap("BITS: ");
  for (uint8_t i = 0; i < len; i++) {
    char* p = chunk;
    for (int b = 7; b >= 0; b--) *p++ = ((d[i] >> b) & 1) ? '1' : '0';
    *p++ = (i + 1 < len) ? ' ' : 0;
    *p = 0;
    ap(chunk);
  }
  ap("\n");

  // Bits contiguous
  ap("BITS_CONTIG: ");
  for (uint8_t i = 0; i < len; i++) {
    char* p = chunk;
    for (int b = 7; b >= 0; b--) *p++ = ((d[i] >> b) & 1) ? '1' : '0';
    *p = 0;
    ap(chunk);
  }
  ap("\n");

  // Nibbles
  ap("NIBBLES: ");
  for (uint8_t i = 0; i < len; i++) {
    snprintf(chunk, sizeof chunk, "%X %X%s", (d[i] >> 4) & 0xF, d[i] & 0xF,
             i + 1 < len ? " " : "");
    ap(chunk);
  }
  ap("\n");

  // Decimal
  ap("DEC: ");
  for (uint8_t i = 0; i < len; i++) {
    snprintf(chunk, sizeof chunk, "%u%s", (unsigned)d[i], i + 1 < len ? " " : "");
    ap(chunk);
  }
  ap("\n");

  // Octal
  ap("OCT: ");
  for (uint8_t i = 0; i < len; i++) {
    snprintf(chunk, sizeof chunk, "%o%s", (unsigned)d[i], i + 1 < len ? " " : "");
    ap(chunk);
  }
  ap("\n");

  // Signed / unsigned 8-bit
  ap("S8: ");
  for (uint8_t i = 0; i < len; i++) {
    snprintf(chunk, sizeof chunk, "%d%s", (int)(int8_t)d[i], i + 1 < len ? " " : "");
    ap(chunk);
  }
  ap("\n");
  ap("U8: ");
  for (uint8_t i = 0; i < len; i++) {
    snprintf(chunk, sizeof chunk, "%u%s", (unsigned)d[i], i + 1 < len ? " " : "");
    ap(chunk);
  }
  ap("\n");

  // 16-bit BE / LE
  ap("S16_BE: ");
  for (uint8_t i = 0; i + 1 < len; i += 2) {
    int16_t v = (int16_t)(((uint16_t)d[i] << 8) | d[i + 1]);
    snprintf(chunk, sizeof chunk, "%d%s", (int)v, i + 2 < len ? " " : "");
    ap(chunk);
  }
  ap("\n");
  ap("U16_BE: ");
  for (uint8_t i = 0; i + 1 < len; i += 2) {
    uint16_t v = ((uint16_t)d[i] << 8) | d[i + 1];
    snprintf(chunk, sizeof chunk, "%u%s", (unsigned)v, i + 2 < len ? " " : "");
    ap(chunk);
  }
  ap("\n");
  ap("S16_LE: ");
  for (uint8_t i = 0; i + 1 < len; i += 2) {
    int16_t v = (int16_t)(((uint16_t)d[i + 1] << 8) | d[i]);
    snprintf(chunk, sizeof chunk, "%d%s", (int)v, i + 2 < len ? " " : "");
    ap(chunk);
  }
  ap("\n");
  ap("U16_LE: ");
  for (uint8_t i = 0; i + 1 < len; i += 2) {
    uint16_t v = ((uint16_t)d[i + 1] << 8) | d[i];
    snprintf(chunk, sizeof chunk, "%u%s", (unsigned)v, i + 2 < len ? " " : "");
    ap(chunk);
  }
  ap("\n");

  // 32-bit BE / LE
  ap("S32_BE: ");
  for (uint8_t i = 0; i + 3 < len; i += 4) {
    int32_t v = (int32_t)(((uint32_t)d[i] << 24) | ((uint32_t)d[i + 1] << 16) |
                          ((uint32_t)d[i + 2] << 8) | d[i + 3]);
    snprintf(chunk, sizeof chunk, "%ld%s", (long)v, i + 4 < len ? " " : "");
    ap(chunk);
  }
  ap("\n");
  ap("U32_BE: ");
  for (uint8_t i = 0; i + 3 < len; i += 4) {
    uint32_t v = ((uint32_t)d[i] << 24) | ((uint32_t)d[i + 1] << 16) |
                 ((uint32_t)d[i + 2] << 8) | d[i + 3];
    snprintf(chunk, sizeof chunk, "%lu%s", (unsigned long)v, i + 4 < len ? " " : "");
    ap(chunk);
  }
  ap("\n");
  ap("S32_LE: ");
  for (uint8_t i = 0; i + 3 < len; i += 4) {
    int32_t v = (int32_t)(((uint32_t)d[i + 3] << 24) | ((uint32_t)d[i + 2] << 16) |
                          ((uint32_t)d[i + 1] << 8) | d[i]);
    snprintf(chunk, sizeof chunk, "%ld%s", (long)v, i + 4 < len ? " " : "");
    ap(chunk);
  }
  ap("\n");
  ap("U32_LE: ");
  for (uint8_t i = 0; i + 3 < len; i += 4) {
    uint32_t v = ((uint32_t)d[i + 3] << 24) | ((uint32_t)d[i + 2] << 16) |
                 ((uint32_t)d[i + 1] << 8) | d[i];
    snprintf(chunk, sizeof chunk, "%lu%s", (unsigned long)v, i + 4 < len ? " " : "");
    ap(chunk);
  }
  ap("\n");

  snprintf(chunk, sizeof chunk,
           "BITS_META: %u bytes · %u bits · sample %u us/bit · window ~%u us · "
           "MSB-first GDO0\n",
           (unsigned)len, bits, (unsigned)ROOT_SUBGHZ_BIT_US,
           bits * (unsigned)ROOT_SUBGHZ_BIT_US);
  ap(chunk);
}

static void fmtTime(uint32_t ms, char* out, size_t n) {
  snprintf(out, n, "T+%lu ms", (unsigned long)ms);
}

bool subghzListRecent(char* out, size_t n) {
  if (!out || n < 16) return false;
  size_t u = 0;
  auto ap = [&](const char* s) {
    size_t l = strlen(s);
    if (u + l + 1 >= n) l = n > u + 1 ? n - u - 1 : 0;
    if (l) {
      memcpy(out + u, s, l);
      u += l;
      out[u] = 0;
    }
  };
  char line[160];
  snprintf(line, sizeof line, "=== SUB-GHZ PACKETS (last 10) ===\n");
  ap(line);
  uint32_t show = gRawCount < 10 ? gRawCount : 10;
  if (!show) {
    ap("(no packets yet)\n");
    return true;
  }
  for (uint32_t i = 0; i < show; i++) {
    SubGhzRawPacket p;
    if (!subghzRawGet(i, &p)) break;
    snprintf(line, sizeof line, "[%lu] %.2f MHz %d dBm %u bytes\n",
             (unsigned long)(i + 1), p.frequencyMhz, (int)p.rssi, (unsigned)p.length);
    ap(line);
  }
  return true;
}

static int hexNibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static bool buildSaveBlob(char* err, size_t errn) {
  if (err && errn) err[0] = 0;
  if (gSaveBlob) {
    free(gSaveBlob);
    gSaveBlob = nullptr;
    gSaveBlobLen = 0;
  }
  // Header 16 + per packet: ts(4)+freq(4)+rssi(1)+lqi(1)+len(1)+gps(1)+lat(8)+lon(8)+data[len]
  size_t need = 16;
  for (uint32_t i = 0; i < gRawCount; i++) {
    SubGhzRawPacket pk;
    if (!subghzRawGet(i, &pk)) break;
    need += 28 + pk.length;
  }
  gSaveBlob = (uint8_t*)heap_caps_malloc(need, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!gSaveBlob) gSaveBlob = (uint8_t*)malloc(need);
  if (!gSaveBlob) {
    if (err) snprintf(err, errn, "oom");
    return false;
  }
  memset(gSaveBlob, 0, need);
  memcpy(gSaveBlob, "SUBGHZ", 6);
  gSaveBlob[6] = 0x01;
  uint32_t count = gRawCount;
  memcpy(gSaveBlob + 7, &count, 4);
  size_t off = 16;
  // store oldest→newest for file order
  for (int i = (int)gRawCount - 1; i >= 0; i--) {
    SubGhzRawPacket pk;
    if (!subghzRawGet((uint32_t)i, &pk)) continue;
    memcpy(gSaveBlob + off, &pk.timestampMs, 4); off += 4;
    memcpy(gSaveBlob + off, &pk.frequencyMhz, 4); off += 4;
    gSaveBlob[off++] = (uint8_t)pk.rssi;
    gSaveBlob[off++] = pk.lqi;
    gSaveBlob[off++] = pk.length;
    gSaveBlob[off++] = pk.gpsValid;
    memcpy(gSaveBlob + off, &pk.lat, 8); off += 8;
    memcpy(gSaveBlob + off, &pk.lon, 8); off += 8;
    memcpy(gSaveBlob + off, pk.data, pk.length); off += pk.length;
  }
  uint32_t totalSize = (uint32_t)off;
  memcpy(gSaveBlob + 11, &totalSize, 4);
  gSaveBlobLen = off;
  snprintf(gSavePath, sizeof gSavePath, "psram://subghz_raw_%lu.bin", (unsigned long)millis());
  return true;
}

bool subghzRawCommand(const char* args, char* out, size_t n) {
  if (!out || n < 32) return false;
  out[0] = 0;
  size_t u = 0;
  auto ap = [&](const char* s) {
    size_t l = strlen(s);
    if (u + l + 1 >= n) l = n > u + 1 ? n - u - 1 : 0;
    if (l) {
      memcpy(out + u, s, l);
      u += l;
      out[u] = 0;
    }
  };
  auto apf = [&](const char* fmt, ...) {
    char buf[1536];
    va_list a;
    va_start(a, fmt);
    vsnprintf(buf, sizeof buf, fmt, a);
    va_end(a);
    ap(buf);
  };

  const char* p = args ? args : "";
  while (*p && isspace((unsigned char)*p)) p++;

  auto dumpList = [&](uint32_t want, float filterMhz, bool withGps) {
    uint32_t shown = 0;
    if (filterMhz > 0)
      apf("=== SUB-GHZ RAW (%.2f MHz, last %lu) ===\n"
          "CC1101 GDO0 OOK samples · MSB-first · %u us/bit\n\n",
          filterMhz, (unsigned long)want, (unsigned)ROOT_SUBGHZ_BIT_US);
    else
      apf("=== SUB-GHZ RAW (last %lu) ===\n"
          "CC1101 GDO0 OOK samples · MSB-first · %u us/bit\n\n",
          (unsigned long)want, (unsigned)ROOT_SUBGHZ_BIT_US);
    for (uint32_t i = 0; i < gRawCount && shown < want; i++) {
      SubGhzRawPacket pk;
      if (!subghzRawGet(i, &pk)) break;
      if (filterMhz > 0 && fabsf(pk.frequencyMhz - filterMhz) > 2.0f) continue;
      shown++;
      char tbuf[32];
      fmtTime(pk.timestampMs, tbuf, sizeof tbuf);
      if (withGps && pk.gpsValid) {
        apf("[%lu] %.2f MHz %d dBm %uB (%u bits) @ %.5f,%.5f\n",
            (unsigned long)shown, pk.frequencyMhz, (int)pk.rssi, (unsigned)pk.length,
            (unsigned)pk.length * 8u, pk.lat, pk.lon);
      } else {
        apf("[%lu] %.2f MHz %d dBm %uB (%u bits)\n",
            (unsigned long)shown, pk.frequencyMhz, (int)pk.rssi, (unsigned)pk.length,
            (unsigned)pk.length * 8u);
      }
      appendPayloadViews(ap, pk.data, pk.length);
      {
        SgProtoResult pr;
        sgClassify(pk.data, pk.length, pk.frequencyMhz, true, &pr);
        if (pr.decoded && pr.keyHex[0]) {
          apf("ID:  %s (%u%%) · %s · Key %s (%ub)\n", pr.name, (unsigned)pr.confidence,
              pr.family, pr.keyHex, (unsigned)pr.keyBits);
          if (pr.keyBits) {
            ap("KEY_BITS: ");
            for (uint8_t kb = 0; kb < pr.keyBits && kb < 96; kb++) {
              const uint8_t byte = pr.key[kb / 8];
              const uint8_t mask = (uint8_t)(1u << (7 - (kb % 8)));
              ap((byte & mask) ? "1" : "0");
            }
            ap("\n");
          }
        } else {
          apf("ID:  %s (%u%%) · %s · ~%ub TE%uus\n", pr.name, (unsigned)pr.confidence,
              pr.family, (unsigned)pr.bits, (unsigned)pr.teUs);
        }
        apf("     %s\n", pr.brands);
      }
      apf("Time: %s\n\n", tbuf);
    }
    if (!shown) {
      ap("(no raw captures yet)\n");
      ap("Press a 315/433/868/915 remote near the board, then run again.\n");
      ap("Also try: GET http://192.168.4.1/api/subghz/raw?n=20\n");
    }
    return shown;
  };

  if (!*p) {
    uint32_t show = gRawCount < 10 ? gRawCount : 10;
    if (!show) {
      // Always print live RF status so Omni tab is never empty white noise
      char st[220];
      subghzStatusJson(st, sizeof st);
      ap("=== SUB-GHZ RAW ===\n(no packets in ring yet)\n");
      apf("Status: %s\n", st);
      ap("Tip: mash a key fob on 315/433 while this band is dwelling,\n"
         "then ./omni subghz raw again — or open /api/subghz/raw?n=20\n");
      return true;
    }
    dumpList(show, 0, true);
    float mb = subghzRawBytesUsed() / (1024.0f * 1024.0f);
    apf("--- SUMMARY ---\nTotal Raw Packets: %lu\nShown: %lu\nBuffer: %u slots (%.2fMB)\n",
        (unsigned long)gRawTotal, (unsigned long)show, (unsigned)gRawCap, mb);
    return true;
  }

  if (strncasecmp(p, "last", 4) == 0) {
    SubGhzRawPacket pk;
    if (!subghzRawGet(0, &pk)) {
      char st[220];
      subghzStatusJson(st, sizeof st);
      ap("ERROR: No raw packets captured\n");
      apf("Status: %s\n", st);
      return false;
    }
    char tbuf[32];
    fmtTime(pk.timestampMs, tbuf, sizeof tbuf);
    ap("=== LAST SUB-GHZ PACKET ===\n");
    apf("Frequency: %.2f MHz\nRSSI: %d dBm\nLength: %u bytes (%u bits)\n"
        "Sample: %u us/bit (CC1101 GDO0 async OOK)\n",
        pk.frequencyMhz, (int)pk.rssi, (unsigned)pk.length, (unsigned)pk.length * 8u,
        (unsigned)ROOT_SUBGHZ_BIT_US);
    {
      char idbuf[400];
      if (sgFormatIdentify(pk.data, pk.length, pk.frequencyMhz, true, idbuf, sizeof idbuf))
        ap(idbuf);
      ap("\n");
    }
    appendPayloadViews(ap, pk.data, pk.length);
    {
      SgProtoResult pr;
      sgClassify(pk.data, pk.length, pk.frequencyMhz, true, &pr);
      if (pr.decoded && pr.keyBits) {
        apf("KEY_HEX: %s\nKEY_BITS: ", pr.keyHex);
        for (uint8_t kb = 0; kb < pr.keyBits && kb < 96; kb++) {
          const uint8_t byte = pr.key[kb / 8];
          const uint8_t mask = (uint8_t)(1u << (7 - (kb % 8)));
          ap((byte & mask) ? "1" : "0");
        }
        ap("\n");
      }
    }
    apf("Time: %s\n", tbuf);
    if (pk.gpsValid) apf("GPS: %.7f, %.7f\n", pk.lat, pk.lon);
    else ap("GPS: (none)\n");
    return true;
  }

  if (strncasecmp(p, "protocols", 9) == 0) {
    return sgProtoCatalog(out, n);
  }

  if (strncasecmp(p, "id", 2) == 0 && (p[2] == 0 || isspace((unsigned char)p[2]))) {
    p += 2;
    while (*p && isspace((unsigned char)*p)) p++;
    uint8_t bytes[96];
    uint8_t nb = 0;
    float mhz = 433.92f;
    if (*p) {
      // optional leading freq: id 315 FE00...  OR just hex
      if ((*p >= '0' && *p <= '9') || *p == '.') {
        char* end = nullptr;
        float f = strtof(p, &end);
        if (end && end != p && f > 100.0f) {
          mhz = f;
          p = end;
          while (*p && isspace((unsigned char)*p)) p++;
        }
      }
      const char* h = p;
      while (*h && nb < 96) {
        while (*h && (isspace((unsigned char)*h) || *h == ':' || *h == '-')) h++;
        if (!*h) break;
        int a = hexNibble(*h++);
        if (a < 0) break;
        int b = hexNibble(*h);
        if (b < 0) {
          bytes[nb++] = (uint8_t)a;
          break;
        }
        h++;
        bytes[nb++] = (uint8_t)((a << 4) | b);
      }
      if (!nb) {
        ap("ERROR: Usage: ./omni subghz raw id [mhz] <hex>\n"
           "       ./omni subghz raw id          (last packet)\n");
        return false;
      }
    } else {
      SubGhzRawPacket pk;
      if (!subghzRawGet(0, &pk)) {
        ap("ERROR: No raw packets — press a fob, then ./omni subghz raw id\n");
        return false;
      }
      memcpy(bytes, pk.data, pk.length < 96 ? pk.length : 96);
      nb = pk.length < 96 ? pk.length : 96;
      mhz = pk.frequencyMhz;
      apf("=== SUB-GHZ IDENTIFY (last @ %.2f MHz) ===\n", mhz);
    }
    if (!out[0]) ap("=== SUB-GHZ IDENTIFY ===\n");
    char idbuf[400];
    sgFormatIdentify(bytes, nb, mhz, true, idbuf, sizeof idbuf);
    ap(idbuf);
    return true;
  }

  if (strncasecmp(p, "clear", 5) == 0) {
    uint32_t was = gRawCount;
    subghzRawClear();
    apf("Sub-GHz raw buffer cleared (%lu packets freed)\n", (unsigned long)was);
    return true;
  }

  if (strncasecmp(p, "save", 4) == 0) {
    ap("Saving sub-GHz raw buffer...\n");
    char err[32];
    if (!gRawCount) {
      ap("ERROR: No packets to save\n");
      return false;
    }
    if (!buildSaveBlob(err, sizeof err)) {
      apf("ERROR: save failed (%s)\n", err[0] ? err : "unknown");
      return false;
    }
    apf("File: %s\nSize: %.2fKB\nPackets: %lu\nDone.\n",
        gSavePath, gSaveBlobLen / 1024.0f, (unsigned long)gRawCount);
    ap("(No SD card on this kit — blob held in PSRAM until clear/reboot)\n");
    return true;
  }

  if (strncasecmp(p, "analyze", 7) == 0) {
    uint32_t c315 = 0, c433 = 0, c868 = 0, c915 = 0;
    uint32_t byteHist[256] = {};
    int8_t rssiVals[64];
    uint8_t rssiN = 0;
    struct Pat {
      uint8_t pref[6];
      uint8_t plen;
      uint32_t count;
      bool used;
    } pats[32] = {};
    uint32_t uniqueish = 0;
    uint32_t protoCount[SG_PROTO_COUNT] = {};

    for (uint32_t i = 0; i < gRawCount; i++) {
      SubGhzRawPacket pk;
      if (!subghzRawGet(i, &pk)) break;
      if (pk.frequencyMhz < 350) c315++;
      else if (pk.frequencyMhz < 500) c433++;
      else if (pk.frequencyMhz < 900) c868++;
      else c915++;
      for (uint8_t b = 0; b < pk.length; b++) byteHist[pk.data[b]]++;
      if (rssiN < 64) rssiVals[rssiN++] = pk.rssi;
      uint8_t plen = pk.length < 6 ? pk.length : 6;
      int slot = -1;
      for (int j = 0; j < 32; j++) {
        if (!pats[j].used) {
          if (slot < 0) slot = j;
          continue;
        }
        if (pats[j].plen == plen && memcmp(pats[j].pref, pk.data, plen) == 0) {
          pats[j].count++;
          slot = -2;
          break;
        }
      }
      if (slot >= 0) {
        pats[slot].used = true;
        pats[slot].plen = plen;
        memcpy(pats[slot].pref, pk.data, plen);
        pats[slot].count = 1;
        uniqueish++;
      }
      SgProtoResult pr;
      sgClassify(pk.data, pk.length, pk.frequencyMhz, true, &pr);
      if ((uint8_t)pr.id < SG_PROTO_COUNT) protoCount[pr.id]++;
    }
    uint32_t buffered = gRawCount;
    ap("=== SUB-GHZ RAW ANALYSIS ===\n\n");
    apf("Total Packets: %lu\nUnique Patterns: %lu\n\n", (unsigned long)gRawTotal,
        (unsigned long)uniqueish);
    ap("Protocol IDs:\n");
    {
      bool any = false;
      for (uint8_t id = 1; id < SG_PROTO_COUNT; id++) {
        if (!protoCount[id]) continue;
        any = true;
        apf("  %s: %lu\n", sgProtoName((SgProtoId)id), (unsigned long)protoCount[id]);
      }
      if (!any) ap("  (none classified yet)\n");
    }
    ap("\nMost Common Patterns:\n");
    // top 3 patterns by count
    for (int rank = 0; rank < 3; rank++) {
      int best = -1;
      uint32_t bestC = 0;
      for (int j = 0; j < 32; j++) {
        if (pats[j].used && pats[j].count > bestC) {
          bestC = pats[j].count;
          best = j;
        }
      }
      if (best < 0) break;
      char hx[40];
      size_t hi = 0;
      for (uint8_t k = 0; k < pats[best].plen; k++)
        hi += (size_t)snprintf(hx + hi, sizeof hx - hi, "%02X%s", pats[best].pref[k],
                               k + 1 < pats[best].plen ? " " : "");
      const char* note = "";
      if (pats[best].plen >= 2 && pats[best].pref[0] == 0x02 && pats[best].pref[1] == 0x53)
        note = " - CC1101 scan";
      else if (pats[best].plen >= 5 && pats[best].pref[0] == 0x48 && pats[best].pref[1] == 0x65)
        note = " - ASCII-ish";
      apf("%d. %s (%lu times)%s\n", rank + 1, hx, (unsigned long)bestC, note);
      pats[best].used = false;
    }
    float tot = buffered ? (float)buffered : 1.0f;
    ap("\nFrequency Distribution:\n");
    apf("315 MHz: %lu packets (%.1f%%)\n", (unsigned long)c315, 100.0f * c315 / tot);
    apf("433 MHz: %lu packets (%.1f%%)\n", (unsigned long)c433, 100.0f * c433 / tot);
    apf("868 MHz: %lu packets (%.1f%%)\n", (unsigned long)c868, 100.0f * c868 / tot);
    apf("915 MHz: %lu packets (%.1f%%)\n", (unsigned long)c915, 100.0f * c915 / tot);
    ap("\nTop RSSI Values:\n");
    // simple unique rssi counts from sample
    for (int pass = 0; pass < 5; pass++) {
      int8_t bestR = 0;
      uint32_t bestC = 0;
      bool any = false;
      for (uint8_t i = 0; i < rssiN; i++) {
        if (rssiVals[i] == 127) continue;
        uint32_t c = 0;
        for (uint8_t j = 0; j < rssiN; j++)
          if (rssiVals[j] == rssiVals[i]) c++;
        if (!any || c > bestC) {
          bestC = c;
          bestR = rssiVals[i];
          any = true;
        }
      }
      if (!any) break;
      apf("%d dBm: %lu packets\n", (int)bestR, (unsigned long)bestC);
      for (uint8_t i = 0; i < rssiN; i++)
        if (rssiVals[i] == bestR) rssiVals[i] = 127;
    }
    ap("\nCommon Byte Values:\n");
    for (int pass = 0; pass < 5; pass++) {
      uint32_t bestC = 0;
      int bestB = -1;
      for (int b = 0; b < 256; b++) {
        if (byteHist[b] > bestC) {
          bestC = byteHist[b];
          bestB = b;
        }
      }
      if (bestB < 0 || bestC == 0) break;
      apf("0x%02X: %lu times\n", bestB, (unsigned long)bestC);
      byteHist[bestB] = 0;
    }
    return true;
  }

  if (strncasecmp(p, "decode", 6) == 0) {
    p += 6;
    while (*p && isspace((unsigned char)*p)) p++;
    if (!*p) {
      ap("ERROR: Usage: ./omni subghz raw decode <hex>\n");
      return false;
    }
    const char* rawIn = p;
    uint8_t bytes[96];
    uint8_t nb = 0;
    const char* h = p;
    while (*h && nb < 96) {
      while (*h && (isspace((unsigned char)*h) || *h == ':' || *h == '-')) h++;
      if (!*h) break;
      int a = hexNibble(*h++);
      if (a < 0) break;
      int b = hexNibble(*h);
      if (b < 0) {
        bytes[nb++] = (uint8_t)a;
        break;
      }
      h++;
      bytes[nb++] = (uint8_t)((a << 4) | b);
    }
    apf("=== DECODING: %s ===\n\n", rawIn);
    {
      char idbuf[400];
      sgFormatIdentify(bytes, nb, 433.92f, true, idbuf, sizeof idbuf);
      ap("--- PROTOCOL / BRAND ---\n");
      ap(idbuf);
      ap("\n");
    }
    appendPayloadViews(ap, bytes, nb);
    return true;
  }

  // Explicit: dump last packet with every backend format (no client decode)
  if (strncasecmp(p, "formats", 7) == 0 || strncasecmp(p, "all", 3) == 0) {
    const char* q = p;
    while (*q && !isspace((unsigned char)*q)) q++;
    while (*q && isspace((unsigned char)*q)) q++;
    uint32_t want = 1;
    if (*q) {
      int c = atoi(q);
      if (c >= 1 && c <= 20) want = (uint32_t)c;
    }
    if (!gRawCount) {
      ap("ERROR: No raw packets — press a fob, then ./omni subghz raw formats\n");
      return false;
    }
    apf("=== SUB-GHZ ALL PAYLOAD FORMATS (last %lu) ===\n"
        "Backend-rendered · no client decode needed\n\n",
        (unsigned long)want);
    dumpList(want, 0, true);
    return true;
  }

  if (strncasecmp(p, "filter", 6) == 0) {
    p += 6;
    while (*p && isspace((unsigned char)*p)) p++;
    float f = atof(p);
    if (f < 100) {
      ap("ERROR: Usage: ./omni subghz raw filter <freq>\n");
      return false;
    }
    gRawFilterMhz = f;
    dumpList(5, f, false);
    return true;
  }

  int count = atoi(p);
  if (count < 1 || count > 50) {
    ap("ERROR: count must be 1-50 (or last|formats|all|filter|clear|save|decode|analyze|id|protocols)\n");
    return false;
  }
  dumpList((uint32_t)count, 0, false);
  return true;
}

#else

bool subghzReady() { return false; }
void subghzInit() {}
void subghzService(uint32_t) {}
bool subghzPopHit(SubGhzHit*) { return false; }
bool subghzPopActivity(SubGhzHit*) { return false; }
void subghzStatusJson(char* out, size_t n) {
  if (out && n) snprintf(out, n, "{\"ready\":false}");
}
void subghzSetEnabled(bool) {}
bool subghzEnabled() { return false; }
void subghzSetHopping(bool) {}
bool subghzHopping() { return false; }
bool subghzSetFrequencyMhz(float) { return false; }
float subghzCurrentMhz() { return 0; }
uint32_t subghzPacketCount() { return 0; }
bool subghzSetOokDwellMs(float, uint16_t) { return false; }
bool subghzFormatDwell(char* out, size_t n) {
  if (out && n) snprintf(out, n, "ERROR: Sub-GHz disabled\n");
  return false;
}
uint32_t subghzRawCount() { return 0; }
uint32_t subghzRawTotal() { return 0; }
size_t subghzRawBytesUsed() { return 0; }
void subghzRawClear() {}
bool subghzRawGet(uint32_t, SubGhzRawPacket*) { return false; }
bool subghzRawCommand(const char*, char* out, size_t n) {
  if (out && n) snprintf(out, n, "ERROR: Sub-GHz disabled in this build\n");
  return false;
}
bool subghzListRecent(char* out, size_t n) {
  if (out && n) snprintf(out, n, "ERROR: Sub-GHz disabled\n");
  return false;
}

#endif
