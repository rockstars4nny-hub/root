#include "rf_subghz.h"
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

static SPIClass gSpi(HSPI);
static CC1101 gRadio = new Module(ROOT_CC1101_CS, ROOT_CC1101_GDO0, -1, -1);
static bool gOk = false;
static bool gEnabled = true;
static bool gHopping = true;
static float gFixedMhz = 433.92f;

struct ScanSlot {
  float mhz;
  bool ook;
  const char* name;
};
// 315/433 visited as OOK (remotes) and FSK; 868/915 FSK only.
static const ScanSlot SLOTS[] = {
    {315.0f, true, "315"},  {315.0f, false, "315"},
    {433.92f, true, "433"}, {433.92f, false, "433"},
    {868.0f, false, "868"}, {915.0f, false, "915"},
};
static const uint8_t SLOT_N = sizeof(SLOTS) / sizeof(SLOTS[0]);

struct BandCal {
  float noiseEma = -110.0f;
  int8_t peakRssi = -120;
  uint32_t carrierMs = 0;
  uint16_t bursts = 0;
  uint16_t packets = 0;
  uint32_t lastHitMs = 0;
  uint32_t lastEmitMs = 0;
};

static BandCal gCal[SLOT_N];
static uint8_t gSlotIdx = 0;
static uint32_t gBandEnterMs = 0;
static uint32_t gLastStatusMs = 0;
static uint32_t gTotalBursts = 0;
static bool gCurrentOok = false;

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
  // Locally administered unicast (02:53:…) — visible in the device list.
  mac[0] = 0x02;
  mac[1] = 0x53;
  const uint8_t bucket = (uint8_t)(((int)rssi + 120) / 3);  // ~3 dB buckets
  mac[2] = (uint8_t)((freqKhz >> 16) & 0xff);
  mac[3] = (uint8_t)((freqKhz >> 8) & 0xff);
  mac[4] = (uint8_t)(freqKhz & 0xff);
  mac[5] = (uint8_t)(((det & 0x07) << 5) | (bucket & 0x1f));
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
  if (mac[5] < 4) mac[5] |= 0x10;  // avoid activity monitor slots 0-3
}

static void labelHit(SubGhzHit& h, bool ook) {
  const float mhz = h.freqKhz / 1000.0f;
  const int above = h.rssi - h.noiseFloor;
  const char* mode = ook ? "OOK" : "FSK";
  const char* kind =
      h.detect == SG_CARRIER ? "carrier" : h.detect == SG_PACKET ? "packet" : "burst";
  snprintf(h.label, sizeof(h.label), "%.2f %s %s %+ddB", mhz, mode, kind, above);
  if (!h.detail[0]) {
    snprintf(h.detail, sizeof(h.detail), "nf %d · %s", (int)h.noiseFloor, kind);
  }
}

static void configureSlot(const ScanSlot& sl) {
  gRadio.setFrequency(sl.mhz);
  gCurrentOok = sl.ook;
  gRadio.setOOK(sl.ook);
  if (sl.ook) {
    gRadio.setRxBandwidth(270.0f);
    gRadio.setBitRate(2.4f);
    gRadio.setFrequencyDeviation(0.0f);
  } else if (sl.mhz < 350.0f) {
    gRadio.setRxBandwidth(203.0f);
    gRadio.setBitRate(1.2f);
    gRadio.setFrequencyDeviation(5.2f);
  } else if (sl.mhz < 500.0f) {
    gRadio.setRxBandwidth(135.0f);
    gRadio.setBitRate(4.8f);
    gRadio.setFrequencyDeviation(25.0f);
  } else if (sl.mhz < 900.0f) {
    gRadio.setRxBandwidth(101.0f);
    gRadio.setBitRate(4.8f);
    gRadio.setFrequencyDeviation(45.0f);
  } else {
    gRadio.setRxBandwidth(81.0f);
    gRadio.setBitRate(4.8f);
    gRadio.setFrequencyDeviation(45.0f);
  }
  gRadio.startReceive();
}

static void configureBand(float mhz) {
  ScanSlot sl = {mhz, mhz < 500.0f, "fix"};
  if (mhz >= 800.0f && mhz < 900.0f) sl = {868.0f, false, "868"};
  else if (mhz >= 900.0f) sl = {915.0f, false, "915"};
  else if (mhz < 350.0f) sl = {315.0f, true, "315"};
  else sl = {433.92f, true, "433"};
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
  if (!gOk || !gEnabled) return;
  size_t len = gRadio.getPacketLength(true);
  if (len == 0 || len > 255) {
    int16_t av = gRadio.available();
    if (av <= 0) return;
    len = (size_t)av;
  }
  uint8_t buf[ROOT_SUBGHZ_RAW_BYTES];
  size_t want = len > ROOT_SUBGHZ_RAW_BYTES ? ROOT_SUBGHZ_RAW_BYTES : len;
  int16_t rd = gRadio.readData(buf, want);
  if (rd != RADIOLIB_ERR_NONE) {
    gRadio.startReceive();
    return;
  }
  if (want == 0) {
    gRadio.startReceive();
    return;
  }
  int8_t rssi = gRadio.getRSSI();
  uint8_t lqi = (uint8_t)gRadio.getLQI();
  const float mhz = SLOTS[si].mhz;
  pushRaw(mhz, rssi, lqi, buf, (uint8_t)want);
  gCal[si].packets++;
  gTotalBursts++;

  // Promote demod payload to a device hit with payload-derived MAC + hex detail
  BandCal& c = gCal[si];
  SubGhzHit h = {};
  h.freqKhz = (uint32_t)(mhz * 1000.0f);
  h.rssi = rssi;
  h.noiseFloor = (int8_t)c.noiseEma;
  h.detect = SG_PACKET;
  strncpy(h.band, SLOTS[si].name, sizeof h.band - 1);
  synthMacFromPayload(h.mac, h.freqKhz, buf, want);
  size_t di = 0;
  di += (size_t)snprintf(h.detail + di, sizeof(h.detail) - di, "%uB LQI%u ",
                         (unsigned)want, (unsigned)lqi);
  for (size_t i = 0; i < want && di + 3 < sizeof(h.detail); i++) {
    di += (size_t)snprintf(h.detail + di, sizeof(h.detail) - di, "%02X", buf[i]);
    if (i + 1 < want && di + 1 < sizeof(h.detail)) h.detail[di++] = ' ';
  }
  labelHit(h, gCurrentOok);
  pushHit(h);
  c.lastHitMs = millis();
  c.lastEmitMs = millis();
  gRadio.startReceive();
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
  gSpi.begin(ROOT_SPI_SCK, ROOT_SPI_MISO, ROOT_SPI_MOSI, ROOT_CC1101_CS);
  pinMode(ROOT_CC1101_GDO0, INPUT);
  const int16_t st = gRadio.begin(433.92);
  if (st != RADIOLIB_ERR_NONE) {
    Serial.printf("root: CC1101 init failed (%d) — check SPI CS=%d\n",
                  (int)st, ROOT_CC1101_CS);
    gOk = false;
    return;
  }
  gOk = true;
  gEnabled = true;
  gHopping = true;
  gSlotIdx = 0;
  gBandEnterMs = millis();
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

  Serial.printf("root: CC1101 scanner 315/433 OOK+FSK · 868/915 FSK (CS=%d GDO0=%d) raw_cap=%u\n",
                ROOT_CC1101_CS, ROOT_CC1101_GDO0, (unsigned)gRawCap);
}

static void emit(BandCal& c, uint8_t si, uint32_t freqKhz, int8_t rssi,
                 SubGhzDetect det) {
  const int8_t floor = (int8_t)c.noiseEma;
  const int delta = rssi - floor;
  if (delta < ROOT_SUBGHZ_BURST_DB) return;
  const uint32_t now = millis();
  if (c.lastEmitMs && (now - c.lastEmitMs) < ROOT_SUBGHZ_EMIT_GAP_MS &&
      det != SG_PACKET) {
    return;
  }

  SubGhzHit h = {};
  h.freqKhz = freqKhz;
  h.rssi = rssi;
  h.noiseFloor = floor;
  h.detect = det;
  strncpy(h.band, SLOTS[si].name, sizeof h.band - 1);
  synthMac(h.mac, freqKhz, det, rssi);
  labelHit(h, gCurrentOok);
  pushHit(h);
  c.lastHitMs = now;
  c.lastEmitMs = now;
  gTotalBursts++;

  if (det != SG_PACKET) {
    uint8_t syn[16] = {0x02, 0x53, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    syn[2] = (uint8_t)(freqKhz >> 16);
    syn[3] = (uint8_t)(freqKhz >> 8);
    syn[4] = (uint8_t)freqKhz;
    syn[5] = (uint8_t)det;
    syn[6] = (uint8_t)rssi;
    syn[7] = (uint8_t)floor;
    syn[8] = gCurrentOok ? 1 : 0;
    pushRaw(SLOTS[si].mhz, rssi, 0, syn, 12);
  }
}

static void sampleBand(uint8_t si, uint32_t nowMs) {
  BandCal& c = gCal[si];
  const uint32_t freqKhz = (uint32_t)(SLOTS[si].mhz * 1000.0f);
  int8_t peak = -120;
  uint8_t pktEdges = 0;
  bool gdoHigh = false;

  for (uint8_t s = 0; s < ROOT_SUBGHZ_SAMPLES; s++) {
    const int8_t rssi = gRadio.getRSSI();
    if (rssi > peak) peak = rssi;
    if (c.noiseEma < -99.0f) {
      c.noiseEma = (float)rssi;
    } else if (rssi <= c.noiseEma + 3) {
      c.noiseEma = c.noiseEma * 0.92f + (float)rssi * 0.08f;
    }
    if (digitalRead(ROOT_CC1101_GDO0)) {
      gdoHigh = true;
      pktEdges++;
    }
    delayMicroseconds(ROOT_SUBGHZ_SAMPLE_US);
  }

  c.peakRssi = peak;
  const int delta = peak - (int8_t)c.noiseEma;

  tryReadPacket(si);

  if (gdoHigh && pktEdges >= 2) {
    c.packets++;
    emit(c, si, freqKhz, peak, SG_PACKET);
  } else if (delta >= ROOT_SUBGHZ_BURST_DB) {
    c.bursts++;
    emit(c, si, freqKhz, peak, SG_BURST);
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
  if (!gOk || !gEnabled) return;

  if (gHopping) {
    if (nowMs - gBandEnterMs >= ROOT_SUBGHZ_DWELL_MS) {
      gSlotIdx = (gSlotIdx + 1) % SLOT_N;
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
           "\"rssi\":%d,\"noise_dbm\":%.1f,\"bursts\":%lu,\"raw\":%lu,"
           "\"burst_thresh_db\":%d,\"carrier_thresh_db\":%d}",
           gEnabled ? "true" : "false",
           gHopping ? "true" : "false",
           SLOTS[gSlotIdx].name, subghzCurrentMhz(),
           gCurrentOok ? "true" : "false",
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
    char buf[384];
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
      apf("=== SUB-GHZ RAW PACKETS (%.2f MHz, last %lu) ===\n\n", filterMhz, (unsigned long)want);
    else
      apf("=== SUB-GHZ RAW PACKETS (last %lu) ===\n\n", (unsigned long)want);
    for (uint32_t i = 0; i < gRawCount && shown < want; i++) {
      SubGhzRawPacket pk;
      if (!subghzRawGet(i, &pk)) break;
      if (filterMhz > 0 && fabsf(pk.frequencyMhz - filterMhz) > 2.0f) continue;
      shown++;
      char hex[400], asc[140], tbuf[32];
      hexAscii(pk.data, pk.length, hex, sizeof hex, asc, sizeof asc);
      fmtTime(pk.timestampMs, tbuf, sizeof tbuf);
      if (withGps && pk.gpsValid) {
        apf("[%lu] %.2f MHz %d dBm %u bytes %.7f, %.7f\nHEX: %s\nASCII: %s\nTime: %s\n\n",
            (unsigned long)shown, pk.frequencyMhz, (int)pk.rssi, (unsigned)pk.length,
            pk.lat, pk.lon, hex, asc, tbuf);
      } else {
        apf("[%lu] %.2f MHz %d dBm %u bytes\nHEX: %s\nASCII: %s\nTime: %s\n\n",
            (unsigned long)shown, pk.frequencyMhz, (int)pk.rssi, (unsigned)pk.length,
            hex, asc, tbuf);
      }
    }
    if (!shown) ap("(no packets yet)\n");
    return shown;
  };

  if (!*p) {
    uint32_t show = gRawCount < 10 ? gRawCount : 10;
    dumpList(show ? show : 10, 0, true);
    float mb = subghzRawBytesUsed() / (1024.0f * 1024.0f);
    apf("--- SUMMARY ---\nTotal Raw Packets: %lu\nLast %lu: Shown above\nBuffer: %u packets (%.2fMB used)\n",
        (unsigned long)gRawTotal, (unsigned long)(gRawCount < 10 ? gRawCount : 10),
        (unsigned)gRawCap, mb);
    return true;
  }

  if (strncasecmp(p, "last", 4) == 0) {
    SubGhzRawPacket pk;
    if (!subghzRawGet(0, &pk)) {
      ap("ERROR: No raw packets captured\n");
      return false;
    }
    char hex[400], asc[140], tbuf[32];
    hexAscii(pk.data, pk.length, hex, sizeof hex, asc, sizeof asc);
    fmtTime(pk.timestampMs, tbuf, sizeof tbuf);
    ap("=== LAST SUB-GHZ PACKET ===\n");
    apf("Frequency: %.2f MHz\nRSSI: %d dBm\nLength: %u bytes\n",
        pk.frequencyMhz, (int)pk.rssi, (unsigned)pk.length);
    apf("HEX: %s\nASCII: %s\nTime: %s\n", hex, asc, tbuf);
    if (pk.gpsValid) apf("GPS: %.7f, %.7f\n", pk.lat, pk.lon);
    else ap("GPS: (none)\n");
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
    }
    uint32_t buffered = gRawCount;
    ap("=== SUB-GHZ RAW ANALYSIS ===\n\n");
    apf("Total Packets: %lu\nUnique Patterns: %lu\nMost Common Patterns:\n",
        (unsigned long)gRawTotal, (unsigned long)uniqueish);
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
    char hex[400], asc[140], b64[160];
    hexAscii(bytes, nb, hex, sizeof hex, asc, sizeof asc);
    b64Encode(bytes, nb, b64, sizeof b64);
    apf("=== DECODING: %s ===\n\n", rawIn);
    apf("ASCII: \"%s\"\n", asc);
    ap("Binary:");
    for (uint8_t i = 0; i < nb; i++) {
      ap(" ");
      for (int bit = 7; bit >= 0; bit--) ap((bytes[i] >> bit) & 1 ? "1" : "0");
    }
    ap("\nDecimal:");
    for (uint8_t i = 0; i < nb; i++) apf(" %u", (unsigned)bytes[i]);
    apf("\nHex: %s\n", hex);
    ap("\nPossible formats:\n");
    apf("- ASCII Text: %s\n", asc);
    apf("- Base64: %s\n", b64);
    apf("- UTF-8: %s\n", asc);
    apf("- HEX: ");
    for (uint8_t i = 0; i < nb; i++) apf("%02X", bytes[i]);
    ap("\n- Binary:");
    for (uint8_t i = 0; i < nb; i++) {
      ap(" ");
      for (int bit = 7; bit >= 0; bit--) ap((bytes[i] >> bit) & 1 ? "1" : "0");
    }
    ap("\n- Decimal:");
    for (uint8_t i = 0; i < nb; i++) apf(" %u", (unsigned)bytes[i]);
    ap("\n- Octal:");
    for (uint8_t i = 0; i < nb; i++) apf(" %o", (unsigned)bytes[i]);
    ap("\n- Signed 8-bit:");
    for (uint8_t i = 0; i < nb; i++) apf(" %d", (int)(int8_t)bytes[i]);
    ap("\n- Unsigned 8-bit:");
    for (uint8_t i = 0; i < nb; i++) apf(" %u", (unsigned)bytes[i]);
    ap("\n- Signed 16-bit:");
    for (uint8_t i = 0; i + 1 < nb; i += 2) {
      int16_t v = (int16_t)(((uint16_t)bytes[i] << 8) | bytes[i + 1]);
      apf(" %d", (int)v);
    }
    ap("\n- Unsigned 16-bit:");
    for (uint8_t i = 0; i + 1 < nb; i += 2) {
      uint16_t v = ((uint16_t)bytes[i] << 8) | bytes[i + 1];
      apf(" %u", (unsigned)v);
    }
    ap("\n");
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
    ap("ERROR: count must be 1-50 (or last|filter|clear|save|decode|analyze)\n");
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
