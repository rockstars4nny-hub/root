#include "rf_subghz.h"
#include "root_config.h"

#if ROOT_ENABLE_SUBGHZ

#include <SPI.h>
#include <RadioLib.h>

static SPIClass gSpi(HSPI);
static CC1101 gRadio = new Module(ROOT_CC1101_CS, ROOT_CC1101_GDO0, -1, -1);
static bool gOk = false;

static const float BANDS_MHZ[] = {315.0f, 433.92f, 868.0f, 915.0f};
static const char* BAND_NAMES[] = {"315", "433", "868", "915"};
static const uint8_t BAND_N = 4;

struct BandCal {
  float noiseEma = -110.0f;
  int8_t peakRssi = -120;
  uint32_t carrierMs = 0;
  uint16_t bursts = 0;
  uint16_t packets = 0;
  uint32_t lastHitMs = 0;
};

static BandCal gCal[BAND_N];
static uint8_t gBandIdx = 0;
static uint32_t gBandEnterMs = 0;
static uint32_t gLastStatusMs = 0;
static uint32_t gTotalBursts = 0;

static SubGhzHit gHitQ[16];
static uint8_t gHitHead = 0;
static uint8_t gHitTail = 0;
static const uint8_t kActivityMac[6] = {0x02, 0x53, 0x00, 0x00, 0x00, 0x01};

static void pushHit(const SubGhzHit& h) {
  gHitQ[gHitHead] = h;
  gHitHead = (gHitHead + 1) % 16;
  if (gHitHead == gHitTail) gHitTail = (gHitTail + 1) % 16;
}

static void synthMac(uint8_t* mac, uint32_t freqKhz, SubGhzDetect det, uint8_t slot) {
  mac[0] = 0x02;
  mac[1] = 0x53;
  mac[2] = (uint8_t)((freqKhz >> 16) & 0xff);
  mac[3] = (uint8_t)((freqKhz >> 8) & 0xff);
  mac[4] = (uint8_t)(det);
  mac[5] = slot;
}

static void labelHit(SubGhzHit& h, BandCal& c) {
  const float mhz = h.freqKhz / 1000.0f;
  const int above = h.rssi - h.noiseFloor;
  switch (h.detect) {
    case SG_CARRIER:
      snprintf(h.label, sizeof(h.label), "%.0f MHz fixed emitter +%ddB",
               mhz, above);
      break;
    case SG_PACKET:
      snprintf(h.label, sizeof(h.label), "%.0f MHz modulated +%ddB",
               mhz, above);
      break;
  case SG_BURST:
    default:
      snprintf(h.label, sizeof(h.label), "%.0f MHz burst +%ddB",
               mhz, above);
      break;
  }
}

static void configureBand(float mhz) {
  gRadio.setFrequency(mhz);
  if (mhz < 350.0f) {
    gRadio.setRxBandwidth(203.0f);
    gRadio.setBitRate(1.2f);
    gRadio.setFrequencyDeviation(5.2f);
  } else if (mhz < 500.0f) {
    gRadio.setRxBandwidth(101.0f);
    gRadio.setBitRate(3.0f);
    gRadio.setFrequencyDeviation(20.0f);
  } else if (mhz < 900.0f) {
    gRadio.setRxBandwidth(101.0f);
    gRadio.setBitRate(4.8f);
    gRadio.setFrequencyDeviation(45.0f);
  } else {
    gRadio.setRxBandwidth(58.0f);
    gRadio.setBitRate(4.8f);
    gRadio.setFrequencyDeviation(45.0f);
  }
  gRadio.setOOK(false);
  gRadio.startReceive();
}

bool subghzReady() { return gOk; }

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
  gBandIdx = 0;
  gBandEnterMs = millis();
  configureBand(BANDS_MHZ[0]);
  Serial.printf("root: CC1101 scanner 315/433/868/915 MHz (CS=%d GDO0=%d)\n",
                ROOT_CC1101_CS, ROOT_CC1101_GDO0);
}

static void emit(BandCal& c, uint8_t bi, uint32_t freqKhz, int8_t rssi,
                 SubGhzDetect det) {
  const int8_t floor = (int8_t)c.noiseEma;
  const int delta = rssi - floor;
  if (delta < ROOT_SUBGHZ_BURST_DB) return;

  SubGhzHit h = {};
  h.freqKhz = freqKhz;
  h.rssi = rssi;
  h.noiseFloor = floor;
  h.detect = det;
  strncpy(h.band, BAND_NAMES[bi], sizeof h.band - 1);
  synthMac(h.mac, freqKhz, det, bi);
  labelHit(h, c);
  pushHit(h);
  c.lastHitMs = millis();
  gTotalBursts++;
}

static void sampleBand(uint8_t bi, uint32_t nowMs) {
  BandCal& c = gCal[bi];
  const uint32_t freqKhz = (uint32_t)(BANDS_MHZ[bi] * 1000.0f);
  int8_t peak = -120;
  uint8_t pktEdges = 0;
  bool gdoHigh = false;

  for (uint8_t s = 0; s < ROOT_SUBGHZ_SAMPLES; s++) {
    const int8_t rssi = gRadio.getRSSI();
    if (rssi > peak) peak = rssi;
    // Track quiet-floor only — don't chase active signals or bursts never cross threshold.
    if (c.noiseEma < -99.0f) {
      c.noiseEma = (float)rssi;
    } else if (rssi <= c.noiseEma + 2) {
      c.noiseEma = c.noiseEma * 0.90f + (float)rssi * 0.10f;
    }
    if (digitalRead(ROOT_CC1101_GDO0)) {
      gdoHigh = true;
      pktEdges++;
    }
    delayMicroseconds(ROOT_SUBGHZ_SAMPLE_US);
  }

  c.peakRssi = peak;
  const int delta = peak - (int8_t)c.noiseEma;

  if (gdoHigh && pktEdges >= 2) {
    c.packets++;
    emit(c, bi, freqKhz, peak, SG_PACKET);
  } else if (delta >= ROOT_SUBGHZ_BURST_DB) {
    c.bursts++;
    emit(c, bi, freqKhz, peak, SG_BURST);
  }

  if (delta >= ROOT_SUBGHZ_CARRIER_DB) {
    if (c.carrierMs == 0) c.carrierMs = nowMs;
    if (nowMs - c.carrierMs >= ROOT_SUBGHZ_CARRIER_MS) {
      emit(c, bi, freqKhz, peak, SG_CARRIER);
      c.carrierMs = nowMs;
    }
  } else {
    c.carrierMs = 0;
  }
}

void subghzService(uint32_t nowMs) {
  if (!gOk) return;

  if (nowMs - gBandEnterMs >= ROOT_SUBGHZ_DWELL_MS) {
    gBandIdx = (gBandIdx + 1) % BAND_N;
    configureBand(BANDS_MHZ[gBandIdx]);
    gBandEnterMs = nowMs;
  }

  sampleBand(gBandIdx, nowMs);
  gLastStatusMs = nowMs;
}

bool subghzPopHit(SubGhzHit* out) {
  if (!out || gHitHead == gHitTail) return false;
  *out = gHitQ[gHitTail];
  gHitTail = (gHitTail + 1) % 16;
  return true;
}

bool subghzPopActivity(SubGhzHit* out) {
  if (!out || !gOk) return false;

  uint8_t bestBi = gBandIdx;
  int8_t bestPeak = gCal[gBandIdx].peakRssi;
  const int8_t floor = (int8_t)gCal[gBandIdx].noiseEma;
  int bestDelta = bestPeak - floor;

  for (uint8_t i = 0; i < BAND_N; i++) {
    const int8_t peak = gCal[i].peakRssi;
    const int delta = peak - (int8_t)gCal[i].noiseEma;
    if (delta > bestDelta || (delta == bestDelta && peak > bestPeak)) {
      bestDelta = delta;
      bestPeak = peak;
      bestBi = i;
    }
  }

  if (bestPeak < ROOT_SUBGHZ_ACTIVITY_RSSI && bestDelta < ROOT_SUBGHZ_BURST_DB) return false;

  SubGhzHit h = {};
  h.freqKhz = (uint32_t)(BANDS_MHZ[bestBi] * 1000.0f);
  h.rssi = bestPeak;
  h.noiseFloor = (int8_t)gCal[bestBi].noiseEma;
  h.detect = SG_BURST;
  strncpy(h.band, BAND_NAMES[bestBi], sizeof h.band - 1);
  memcpy(h.mac, kActivityMac, 6);
  h.mac[5] = bestBi;
  snprintf(h.label, sizeof(h.label), "%.0f MHz scan %ddBm (+%ddB)",
           BANDS_MHZ[bestBi], (int)bestPeak, bestDelta);
  *out = h;
  return true;
}

void subghzStatusJson(char* out, size_t n) {
  if (!out || n < 8) return;
  snprintf(out, n,
           "{\"ready\":true,\"scan\":\"315,433,868,915\","
           "\"band\":\"%s\",\"freq_mhz\":%.2f,"
           "\"rssi\":%d,\"noise_dbm\":%.1f,\"bursts\":%lu,"
           "\"burst_thresh_db\":%d,\"carrier_thresh_db\":%d}",
           BAND_NAMES[gBandIdx], BANDS_MHZ[gBandIdx],
           (int)gCal[gBandIdx].peakRssi, gCal[gBandIdx].noiseEma,
           (unsigned long)gTotalBursts,
           (int)ROOT_SUBGHZ_BURST_DB, (int)ROOT_SUBGHZ_CARRIER_DB);
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

#endif
