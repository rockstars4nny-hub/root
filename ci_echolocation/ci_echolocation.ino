/*
 * root — passive Wi-Fi echolocation + CC1101 sub-GHz + LR22 LoRa (ESP32-S3)
 *
 * Join Wi-Fi  SSID: root   password: root-radar
 * Dashboard: http://192.168.4.1
 * API:       /api/devices  /api/rf  /api/channel  /api/omni
 * OmniScan:  ./omni … via Serial or POST /api/omni
 * Radios:    Wi-Fi (promiscuous + SoftAP b/g/n+LR) · BLE · CC1101 · LoRa
 */

#include <WiFi.h>
#include <DNSServer.h>
#include <esp_wifi.h>
#include <math.h>
#include <unordered_map>
#include <esp_heap_caps.h>
#include <ESPAsyncWebServer.h>
#include "ci_dashboard.h"
#include "psram_alloc.h"
#include "root_config.h"
#include "rf_subghz.h"
#include "rf_lora.h"
#include "rf_ble.h"
#include "wifi_lr.h"
#include "root_gps.h"
#include "sight_log.h"
#include "geo.h"
#include "wifi_pkt.h"
#include "omni_cmd.h"

#if __has_include("esp32-hal-rgb-led.h")
#include "esp32-hal-rgb-led.h"
#define CI_HAS_NEOPIXEL 1
#else
#define CI_HAS_NEOPIXEL 0
#endif

static const char* AP_SSID = ROOT_AP_SSID;
static const char* AP_PASS = ROOT_AP_PASS;
static const uint8_t AP_CHANNEL = ROOT_AP_CHANNEL;
// Finch-style session: keep devices on the board for minutes, not 30s.
static const uint32_t STALE_MS = 300000;
static const int8_t PING_DELTA_DB = 5;
static const float TX_POWER_DBM = -50.0f;
static const float PATH_LOSS_N = 2.0f;
static const uint8_t QUEUE_LEN = 128;

#ifndef CI_WS2812_PIN
#ifdef RGB_BUILTIN
#define CI_WS2812_PIN RGB_BUILTIN
#else
#define CI_WS2812_PIN 48
#endif
#endif

#ifndef CI_LED_DISABLE
#define CI_LED_DISABLE 0
#endif

enum FrameKind : uint8_t {
  KIND_PROBE = 0,
  KIND_BEACON = 1,
  KIND_DATA = 2,
  KIND_MGMT = 3,
  KIND_DEAUTH = 4,
  KIND_SUBGHZ = 5,
  KIND_LORA = 6,
  KIND_BLE = 7
};

struct SniffHit {
  uint8_t mac[6];
  uint8_t dst[6];
  uint8_t bssid[6];
  int8_t rssi;
  uint8_t channel;
  uint8_t kind;
  uint16_t seq;
  uint16_t frameLen;
  char ssid[33];
  char band[12];
  char vendor[24];
  char encrypt[16];
};

struct Device {
  uint8_t mac[6];
  uint8_t bssid[6];
  int8_t rssi;
  int8_t hist[3];
  uint8_t histCount;
  uint8_t histIdx;
  float avgRssi;
  float pktRate;
  uint32_t lastSeenMs;
  uint32_t firstSeenMs;
  uint32_t seenCount;
  uint32_t pktProbe;
  uint32_t pktBeacon;
  uint32_t pktData;
  uint32_t pktDeauth;
  uint32_t pktMgmt;
  uint32_t pktTs[8];
  uint8_t pktTsIdx;
  uint8_t pktTsCount;
  uint8_t channel;
  uint8_t kind;
  uint16_t lastSeq;
  char ssid[33];
  char band[12];
  char vendor[24];
  char encrypt[16];
  bool inUse;
};

static QueueHandle_t gQueue = nullptr;
static SemaphoreHandle_t gMux = nullptr;
static SemaphoreHandle_t gJsonMux = nullptr;
using DevMap = std::unordered_map<uint64_t, Device, std::hash<uint64_t>,
                                  std::equal_to<uint64_t>,
                                  PSRAMAllocator<std::pair<const uint64_t, Device>>>;
static DevMap* gDev = nullptr;
static uint8_t gSelfMac[6] = {0};
static uint8_t gApMac[6] = {0};
static volatile uint8_t gChannel = AP_CHANNEL;
static volatile bool gHopEnable = false;
static uint32_t gLastHopMs = 0;
static uint32_t gLastPruneMs = 0;
static uint32_t gLastLedMs = 0;
static uint8_t gLastCount = 0;
static uint32_t gPingStartMs = 0;
static uint32_t gSessionStartMs = 0;
static uint32_t gLastPingLogMs = 0;
static bool gPingActive = false;
static AsyncWebServer gServer(80);
static DNSServer gDns;
static char* gJson = nullptr;
static size_t gJsonCap = 0;

// —— OmniScan state ——
static bool gOmniRunning = true;
static bool gBleScanIntent = true;  // onboard BLE on by default
static char gBleFilter[40] = "";
static char gLrPeer[18] = "";
static char gApSsidBuf[33] = ROOT_AP_SSID;
static uint32_t gHandshakeCount = 0;
static uint32_t gDeauthCount = 0;
static uint32_t gLoraPktCount = 0;
static struct {
  uint8_t attacker[6];
  uint8_t target[6];
  int8_t rssi;
  uint32_t ms;
  bool valid;
} gLastDeauth = {};
static struct {
  uint8_t mac[6];
  uint8_t bssid[6];
  char ssid[33];
  int8_t rssi;
  uint32_t ms;
  bool valid;
} gLastHandshake = {};

static uint64_t macKey(const uint8_t* mac) {
  uint64_t k = 0;
  memcpy(&k, mac, 6);
  return k;
}

static bool macEq(const uint8_t* a, const uint8_t* b) {
  return memcmp(a, b, 6) == 0;
}

static bool macIgnored(const uint8_t* mac) {
  if (mac[0] & 0x01) return true;
  static const uint8_t z[6] = {0};
  static const uint8_t f[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
  if (macEq(mac, z) || macEq(mac, f)) return true;
  if (macEq(mac, gSelfMac) || macEq(mac, gApMac)) return true;
  return false;
}

static bool macZero(const uint8_t* mac) {
  static const uint8_t z[6] = {0};
  return macEq(mac, z);
}

static void macStr(const uint8_t* mac, char* out, size_t n) {
  snprintf(out, n, "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static bool macRandomized(const uint8_t* mac) {
  return (mac[0] & 0x02) != 0;
}

static const char* kindName(uint8_t k) {
  switch (k) {
    case KIND_PROBE: return "probe";
    case KIND_BEACON: return "beacon";
    case KIND_DATA: return "data";
    case KIND_DEAUTH: return "deauth";
    case KIND_SUBGHZ: return "subghz";
    case KIND_LORA: return "lora";
    case KIND_BLE: return "ble";
    default: return "mgmt";
  }
}

static void extractSsid(const uint8_t* p, uint16_t len, uint8_t subtype, char* out, size_t outn) {
  out[0] = 0;
  if (outn == 0 || len < 24) return;
  size_t off = 24;
  if (subtype == 8) {
    if (len < 36) return;
    off = 36;
  }
  while (off + 2 <= len) {
    const uint8_t id = p[off];
    const uint8_t elen = p[off + 1];
    if (off + 2 + elen > len) break;
    if (id == 0) {
      size_t n = elen;
      if (n >= outn) n = outn - 1;
      for (size_t i = 0; i < n; i++) {
        char c = (char)p[off + 2 + i];
        out[i] = (c >= 32 && c <= 126) ? c : '?';
      }
      out[n] = 0;
      return;
    }
    off += 2u + elen;
  }
}

static const char* zoneName(float rssi) {
  if (rssi >= -55.0f) return "Near";
  if (rssi >= -75.0f) return "Mid";
  return "Far";
}

static float rssiToDistance(float rssi) {
  float d = powf(10.0f, (TX_POWER_DBM - rssi) / (20.0f * PATH_LOSS_N));
  if (d < 0.1f) d = 0.1f;
  if (d > 80.0f) d = 80.0f;
  return d;
}

static double jsonNum(double v) {
  if (!isfinite(v)) return 0.0;
  return v;
}

static void ensureJson(size_t need) {
  if (need <= gJsonCap) return;
  size_t nc = gJsonCap ? gJsonCap * 2 : 65536;
  if (nc < need) nc = need + 8192;
  if (nc > ROOT_JSON_MAX_BYTES) nc = ROOT_JSON_MAX_BYTES;
  char* p = (char*)heap_caps_realloc(gJson, nc, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!p) p = (char*)realloc(gJson, nc);
  if (!p) return;
  gJson = p;
  gJsonCap = nc;
}

static void ledWrite(uint8_t r, uint8_t g, uint8_t b) {
#if CI_LED_DISABLE || !CI_HAS_NEOPIXEL
  (void)r; (void)g; (void)b;
#else
  if (CI_WS2812_PIN == 255) return;
  neopixelWrite(CI_WS2812_PIN, r, g, b);
#endif
}

static void triggerPingLed() {
  gPingActive = true;
  gPingStartMs = millis();
}

static void pushRssi(Device& d, int8_t rssi) {
  d.hist[d.histIdx] = rssi;
  d.histIdx = (d.histIdx + 1) % 3;
  if (d.histCount < 3) d.histCount++;
  int sum = 0;
  for (uint8_t i = 0; i < d.histCount; i++) sum += d.hist[i];
  d.avgRssi = (float)sum / (float)d.histCount;
  d.rssi = rssi;
}

static void pushPktTs(Device& d, uint32_t nowMs) {
  d.pktTs[d.pktTsIdx] = nowMs;
  d.pktTsIdx = (d.pktTsIdx + 1) % 8;
  if (d.pktTsCount < 8) d.pktTsCount++;
  if (d.pktTsCount < 2) {
    d.pktRate = 0.0f;
    return;
  }
  const uint8_t newest = (d.pktTsIdx + 7) % 8;
  const uint8_t oldest = (d.pktTsIdx + (8 - d.pktTsCount)) % 8;
  const uint32_t span = d.pktTs[newest] - d.pktTs[oldest];
  if (span < 500) {
    d.pktRate = (float)d.pktTsCount * 1000.0f / 500.0f;
    return;
  }
  d.pktRate = (float)(d.pktTsCount - 1) * 1000.0f / (float)span;
}

static void bumpPktKind(Device& d, uint8_t kind) {
  switch (kind) {
    case KIND_PROBE: d.pktProbe++; break;
    case KIND_BEACON: d.pktBeacon++; break;
    case KIND_DATA: d.pktData++; break;
    case KIND_DEAUTH: d.pktDeauth++; break;
    default: d.pktMgmt++; break;
  }
}

static void snifferCb(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (type == WIFI_PKT_MISC || buf == nullptr || gQueue == nullptr) return;

  const wifi_promiscuous_pkt_t* pkt = static_cast<wifi_promiscuous_pkt_t*>(buf);
  const uint16_t len = pkt->rx_ctrl.sig_len;
  if (len < 16) return;

  const uint8_t* p = pkt->payload;
  const uint8_t ftype = (p[0] & 0x0C) >> 2;
  const uint8_t subtype = (p[0] & 0xF0) >> 4;
  if (ftype == 1) return;

  WifiPktMeta meta{};
  wifiParseFrame(p, len, ftype, subtype, &meta);

  SniffHit hit;
  memcpy(hit.mac, meta.src, 6);
  if (macIgnored(hit.mac)) return;

  memcpy(hit.dst, meta.dst, 6);
  memcpy(hit.bssid, meta.bssid, 6);
  hit.rssi = pkt->rx_ctrl.rssi;
  hit.channel = pkt->rx_ctrl.channel;
  hit.seq = meta.seq;
  hit.frameLen = len;
  hit.ssid[0] = 0;
  hit.vendor[0] = 0;
  hit.encrypt[0] = 0;
  strncpy(hit.band, "wifi", sizeof hit.band - 1);
  if (meta.vendor[0]) strncpy(hit.vendor, meta.vendor, sizeof hit.vendor - 1);
  if (meta.encrypt[0]) strncpy(hit.encrypt, meta.encrypt, sizeof hit.encrypt - 1);

  if (ftype == 2) {
    hit.kind = KIND_DATA;
  } else if (ftype == 0 && subtype == 4) {
    hit.kind = KIND_PROBE;
    extractSsid(p, len, subtype, hit.ssid, sizeof(hit.ssid));
  } else if (ftype == 0 && subtype == 8) {
    hit.kind = KIND_BEACON;
    extractSsid(p, len, subtype, hit.ssid, sizeof(hit.ssid));
  } else if (ftype == 0 && (subtype == 12 || subtype == 10)) {
    hit.kind = KIND_DEAUTH;
    gDeauthCount++;
    memcpy(gLastDeauth.attacker, meta.src, 6);
    memcpy(gLastDeauth.target, meta.dst, 6);
    gLastDeauth.rssi = hit.rssi;
    gLastDeauth.ms = millis();
    gLastDeauth.valid = true;
  } else {
    hit.kind = KIND_MGMT;
  }

  // Passive EAPoL (ethertype 0x888e) → handshake counter
  if (ftype == 2 && len > 32) {
    for (uint16_t i = 24; i + 1 < len && i < 48; i++) {
      if (p[i] == 0x88 && p[i + 1] == 0x8e) {
        gHandshakeCount++;
        memcpy(gLastHandshake.mac, meta.src, 6);
        memcpy(gLastHandshake.bssid, meta.bssid, 6);
        gLastHandshake.ssid[0] = 0;
        gLastHandshake.rssi = hit.rssi;
        gLastHandshake.ms = millis();
        gLastHandshake.valid = true;
        break;
      }
    }
  }

  xQueueSend(gQueue, &hit, 0);
}

static void ingestHit(const SniffHit& hit) {
  if (!gDev || xSemaphoreTake(gMux, pdMS_TO_TICKS(20)) != pdTRUE) return;

  const uint64_t key = macKey(hit.mac);
  auto it = gDev->find(key);
  const bool isNew = (it == gDev->end());
  Device& d = isNew ? (*gDev)[key] : it->second;
  const int8_t prev = isNew ? hit.rssi : d.rssi;
  const bool bigMove = !isNew && abs((int)hit.rssi - (int)prev) >= PING_DELTA_DB;
  const uint8_t prevKind = isNew ? 255 : d.kind;

  if (isNew) {
    memset(&d, 0, sizeof(Device));
    memcpy(d.mac, hit.mac, 6);
    d.inUse = true;
    d.firstSeenMs = millis();
    d.seenCount = 1;
  } else {
    d.seenCount++;
  }

  pushRssi(d, hit.rssi);
  pushPktTs(d, millis());
  bumpPktKind(d, hit.kind);
  d.lastSeenMs = millis();
  d.channel = hit.channel;
  d.kind = hit.kind;
  d.lastSeq = hit.seq;
  strncpy(d.band, hit.band, sizeof d.band - 1);
  if (!macZero(hit.bssid)) memcpy(d.bssid, hit.bssid, 6);
  if (hit.vendor[0]) {
    strncpy(d.vendor, hit.vendor, sizeof d.vendor - 1);
    d.vendor[sizeof d.vendor - 1] = 0;
  }
  if (hit.encrypt[0]) {
    strncpy(d.encrypt, hit.encrypt, sizeof d.encrypt - 1);
    d.encrypt[sizeof d.encrypt - 1] = 0;
  }
  if (hit.ssid[0]) {
    strncpy(d.ssid, hit.ssid, sizeof(d.ssid) - 1);
    d.ssid[sizeof(d.ssid) - 1] = 0;
  }

  const float dist = rssiToDistance(hit.rssi);
  sightPush(d.mac, d.lastSeenMs, hit.rssi, dist);

  const bool kindChanged = !isNew && prevKind != hit.kind;
  const bool burst = d.pktRate >= 8.0f;
  const bool shouldPing = isNew || bigMove || hit.kind == KIND_DEAUTH || kindChanged || burst;
  char macbuf[18];
  char bssidbuf[18] = {0};
  if (shouldPing) {
    macStr(d.mac, macbuf, sizeof(macbuf));
    if (!macZero(d.bssid)) macStr(d.bssid, bssidbuf, sizeof(bssidbuf));
  }

  xSemaphoreGive(gMux);

  if (shouldPing) {
    const bool quiet = WiFi.softAPgetStationNum() > 0;
    const uint32_t logNow = millis();
    if (!quiet || (logNow - gLastPingLogMs >= 2500)) {
      gLastPingLogMs = logNow;
      Sighting last;
      const bool hasS = sightLatest(d.mac, &last);
      const char* vendor = hit.vendor[0] ? hit.vendor : nullptr;
      const char* encrypt = hit.encrypt[0] ? hit.encrypt : nullptr;
      const char* bssidPart = bssidbuf[0] ? bssidbuf : nullptr;
      if (hit.ssid[0]) {
        if (hasS && last.gpsValid) {
          Serial.printf(
            "root ping: [%s] | kind: %s | band: %s | ssid: %s | ch: %u | pkts: %lu | rate: %.1f/s"
            "%s%s%s%s%s%s | RSSI: %d dBm | Distance: ~%.1fm | @ %.6f,%.6f\n",
            macbuf, kindName(hit.kind), hit.band, hit.ssid, (unsigned)hit.channel,
            (unsigned long)d.seenCount, d.pktRate,
            vendor ? " | vendor: " : "", vendor ? vendor : "",
            bssidPart ? " | bssid: " : "", bssidPart ? bssidPart : "",
            encrypt ? " | encrypt: " : "", encrypt ? encrypt : "",
            (int)hit.rssi, dist, last.lat, last.lon);
        } else {
          Serial.printf(
            "root ping: [%s] | kind: %s | band: %s | ssid: %s | ch: %u | pkts: %lu | rate: %.1f/s"
            "%s%s%s%s%s%s | RSSI: %d dBm | Distance: ~%.1fm\n",
            macbuf, kindName(hit.kind), hit.band, hit.ssid, (unsigned)hit.channel,
            (unsigned long)d.seenCount, d.pktRate,
            vendor ? " | vendor: " : "", vendor ? vendor : "",
            bssidPart ? " | bssid: " : "", bssidPart ? bssidPart : "",
            encrypt ? " | encrypt: " : "", encrypt ? encrypt : "",
            (int)hit.rssi, dist);
        }
      } else {
        if (hasS && last.gpsValid) {
          Serial.printf(
            "root ping: [%s] | kind: %s | band: %s | ch: %u | pkts: %lu | rate: %.1f/s"
            "%s%s%s%s%s%s | RSSI: %d dBm | Distance: ~%.1fm | @ %.6f,%.6f\n",
            macbuf, kindName(hit.kind), hit.band, (unsigned)hit.channel,
            (unsigned long)d.seenCount, d.pktRate,
            vendor ? " | vendor: " : "", vendor ? vendor : "",
            bssidPart ? " | bssid: " : "", bssidPart ? bssidPart : "",
            encrypt ? " | encrypt: " : "", encrypt ? encrypt : "",
            (int)hit.rssi, dist, last.lat, last.lon);
        } else {
          Serial.printf(
            "root ping: [%s] | kind: %s | band: %s | ch: %u | pkts: %lu | rate: %.1f/s"
            "%s%s%s%s%s%s | RSSI: %d dBm | Distance: ~%.1fm\n",
            macbuf, kindName(hit.kind), hit.band, (unsigned)hit.channel,
            (unsigned long)d.seenCount, d.pktRate,
            vendor ? " | vendor: " : "", vendor ? vendor : "",
            bssidPart ? " | bssid: " : "", bssidPart ? bssidPart : "",
            encrypt ? " | encrypt: " : "", encrypt ? encrypt : "",
            (int)hit.rssi, dist);
        }
      }
    }
    triggerPingLed();
  }
}

static void ingestSubGhz(const SubGhzHit& sg) {
  SniffHit hit{};
  memcpy(hit.mac, sg.mac, 6);
  hit.rssi = sg.rssi;
  hit.channel = 0;
  hit.kind = KIND_SUBGHZ;
  strncpy(hit.band, "subghz", sizeof hit.band - 1);
  snprintf(hit.ssid, sizeof hit.ssid, "%s", sg.label[0] ? sg.label : sg.band);
  ingestHit(hit);
}

static void serviceSubGhz(uint32_t nowMs) {
  subghzService(nowMs);
  SubGhzHit sg;
  while (subghzPopHit(&sg)) ingestSubGhz(sg);
  if (subghzPopActivity(&sg)) ingestSubGhz(sg);
}

static void ingestLora(const LoraHit& lh) {
  gLoraPktCount++;
  SniffHit hit{};
  memcpy(hit.mac, lh.mac, 6);
  hit.rssi = lh.rssi;
  hit.channel = 0;
  hit.kind = KIND_LORA;
  strncpy(hit.band, "lora", sizeof hit.band - 1);
  if (lh.label[0]) {
    strncpy(hit.ssid, lh.label, sizeof hit.ssid - 1);
  } else {
    strncpy(hit.ssid, "915 MHz LoRa", sizeof hit.ssid - 1);
  }
  ingestHit(hit);
}

static void serviceLora(uint32_t nowMs) {
  loraPoll(nowMs);
  LoraHit lh;
  while (loraPopHit(&lh)) ingestLora(lh);
  if (loraPopActivity(&lh)) ingestLora(lh);
}

static void ingestBle(const BleHit& bh) {
  SniffHit hit{};
  memcpy(hit.mac, bh.mac, 6);
  hit.rssi = bh.rssi;
  hit.channel = 0;
  hit.kind = KIND_BLE;
  strncpy(hit.band, "ble", sizeof hit.band - 1);
  if (bh.name[0]) {
    strncpy(hit.ssid, bh.name, sizeof hit.ssid - 1);
  } else {
    strncpy(hit.ssid, "BLE", sizeof hit.ssid - 1);
  }
  ingestHit(hit);
}

static void serviceBle(uint32_t nowMs) {
  if (!gBleScanIntent) return;
  bleService(nowMs);
  BleHit bh;
  int budget = 24;
  while (budget-- > 0 && blePopHit(&bh)) ingestBle(bh);
}

static void pruneStale() {
  const uint32_t now = millis();
  if (now - gLastPruneMs < 400) return;
  gLastPruneMs = now;

  uint32_t live = 0;
  if (!gDev || xSemaphoreTake(gMux, pdMS_TO_TICKS(20)) != pdTRUE) return;
  for (auto it = gDev->begin(); it != gDev->end();) {
    uint32_t staleMs = STALE_MS;
    if (strcmp(it->second.band, "lora") == 0) {
      staleMs = ROOT_LORA_STALE_MS;
#if ROOT_ENABLE_LORA
      static const uint8_t kLoraListener[] = {0x02, 0x4C, 0x91, 0x50, 0x00, 0x01};
      if (loraReady() && memcmp(it->second.mac, kLoraListener, 6) == 0) {
        live++;
        ++it;
        continue;
      }
#endif
    } else if (strcmp(it->second.band, "subghz") == 0) {
      staleMs = 120000;
    } else if (strcmp(it->second.band, "ble") == 0) {
      staleMs = ROOT_BLE_STALE_MS;
    }
    if (now - it->second.lastSeenMs > staleMs) {
      it = gDev->erase(it);
    } else {
      live++;
      ++it;
    }
  }
  xSemaphoreGive(gMux);

  if ((uint8_t)live != gLastCount) {
    gLastCount = (uint8_t)(live > 255 ? 255 : live);
    triggerPingLed();
  }
}

struct DevSnap {
  Device d;
};

static int cmpRssi(const void* a, const void* b) {
  const DevSnap* x = (const DevSnap*)a;
  const DevSnap* y = (const DevSnap*)b;
  if (x->d.avgRssi > y->d.avgRssi) return -1;
  if (x->d.avgRssi < y->d.avgRssi) return 1;
  return 0;
}

static void jsonEscape(const char* in, char* out, size_t outn) {
  if (!out || outn < 2) return;
  size_t j = 0;
  for (size_t i = 0; in && in[i] && j + 2 < outn; i++) {
    const char c = in[i];
    if (c == '"' || c == '\\') {
      out[j++] = '\\';
      if (j + 1 >= outn) break;
    }
    if ((unsigned char)c < 32) continue;
    out[j++] = c;
  }
  out[j] = 0;
}

static size_t buildJson() {
  const uint32_t now = millis();
  if (!gDev) {
    ensureJson(64);
    if (!gJson) return 0;
    return (size_t)snprintf(gJson, gJsonCap,
                            "{\"count\":0,\"returned\":0,\"truncated\":false,\"devices\":[]}");
  }

  DevSnap* snap = nullptr;
  size_t n = 0;
  if (xSemaphoreTake(gMux, pdMS_TO_TICKS(40)) == pdTRUE) {
    n = gDev->size();
    if (n) {
      snap = (DevSnap*)heap_caps_malloc(n * sizeof(DevSnap), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
      if (!snap) snap = (DevSnap*)malloc(n * sizeof(DevSnap));
      if (snap) {
        size_t i = 0;
        for (auto& kv : *gDev) snap[i++].d = kv.second;
      } else {
        n = 0;
      }
    }
    xSemaphoreGive(gMux);
  }

  const size_t total = n;
  if (n && snap) qsort(snap, n, sizeof(DevSnap), cmpRssi);
  if (ROOT_API_MAX_DEVICES > 0 && n > (size_t)ROOT_API_MAX_DEVICES) {
    n = (size_t)ROOT_API_MAX_DEVICES;
  }

  ensureJson(1024 + n * 480);
  if (!gJson) {
    if (snap) free(snap);
    return 0;
  }

  size_t returned = 0;
  bool truncated = false;
  char tmp[512];
  char esc[96];
  char escVendor[48];
  char escEncrypt[32];
  const GpsFix scanner = gpsGet();

  size_t off = (size_t)snprintf(
      gJson, gJsonCap,
      "{\"name\":\"root\",\"uptime_ms\":%lu,\"session_ms\":%lu,\"channel\":%u,\"hopping\":%s,"
      "\"stations\":%u,\"subghz\":%s,\"lora\":%s,\"ble\":%s,\"wifi_lr\":%s,"
      "\"scanner_gps\":{\"valid\":%s,\"lat\":%.7f,\"lon\":%.7f},\"devices\":[",
      (unsigned long)now, (unsigned long)(now - gSessionStartMs), (unsigned)gChannel,
      gHopEnable ? "true" : "false",
      (unsigned)WiFi.softAPgetStationNum(),
      subghzReady() ? "true" : "false", loraReady() ? "true" : "false",
      (bleReady() && bleEnabled()) ? "true" : "false",
      wifiLrProtocolOk() ? "true" : "false",
      scanner.valid ? "true" : "false", jsonNum(scanner.lat), jsonNum(scanner.lon));

  for (size_t i = 0; i < n; i++) {
    if (off + 520 >= gJsonCap) {
      truncated = true;
      break;
    }
    char macbuf[18];
    char bssidbuf[18] = {0};
    macStr(snap[i].d.mac, macbuf, sizeof(macbuf));
    if (!macZero(snap[i].d.bssid)) macStr(snap[i].d.bssid, bssidbuf, sizeof(bssidbuf));
    const uint32_t age = now - snap[i].d.lastSeenMs;
    const uint32_t sinceFirst = now - snap[i].d.firstSeenMs;
    Sighting last;
    const bool hasLast = sightLatest(snap[i].d.mac, &last);
    jsonEscape(snap[i].d.ssid[0] ? snap[i].d.ssid : "", esc, sizeof esc);
    jsonEscape(snap[i].d.vendor[0] ? snap[i].d.vendor : "", escVendor, sizeof escVendor);
    jsonEscape(snap[i].d.encrypt[0] ? snap[i].d.encrypt : "", escEncrypt, sizeof escEncrypt);
    const int wrote = snprintf(
        tmp, sizeof(tmp),
        "%s{\"mac\":\"%s\",\"rssi\":%d,\"avg\":%.1f,\"distance_m\":%.2f,"
        "\"zone\":\"%s\",\"last_seen_ms\":%lu,\"first_seen_ms\":%lu,"
        "\"seen_count\":%lu,\"pkt_rate\":%.2f,"
        "\"pkts\":{\"probe\":%lu,\"beacon\":%lu,\"data\":%lu,\"deauth\":%lu,\"mgmt\":%lu},"
        "\"ch\":%u,\"kind\":\"%s\",\"band\":\"%s\",\"ssid\":\"%s\","
        "\"vendor\":\"%s\",\"encrypt\":\"%s\",\"bssid\":\"%s\",\"rand\":%s,"
        "\"lat\":%.7f,\"lon\":%.7f,\"gps\":%s,\"sightings\":[]}",
        returned ? "," : "", macbuf, (int)snap[i].d.rssi, snap[i].d.avgRssi,
        rssiToDistance(snap[i].d.avgRssi), zoneName(snap[i].d.avgRssi),
        (unsigned long)age, (unsigned long)sinceFirst,
        (unsigned long)snap[i].d.seenCount, snap[i].d.pktRate,
        (unsigned long)snap[i].d.pktProbe, (unsigned long)snap[i].d.pktBeacon,
        (unsigned long)snap[i].d.pktData, (unsigned long)snap[i].d.pktDeauth,
        (unsigned long)snap[i].d.pktMgmt,
        (unsigned)snap[i].d.channel, kindName(snap[i].d.kind),
        snap[i].d.band[0] ? snap[i].d.band : "wifi",
        esc, escVendor, escEncrypt, bssidbuf,
        macRandomized(snap[i].d.mac) ? "true" : "false",
        jsonNum(hasLast && last.gpsValid ? last.lat : 0.0),
        jsonNum(hasLast && last.gpsValid ? last.lon : 0.0),
        hasLast && last.gpsValid ? "true" : "false");
    if (wrote < 0 || (size_t)wrote >= sizeof(tmp) || off + (size_t)wrote + 96 >= gJsonCap) {
      truncated = true;
      break;
    }
    memcpy(gJson + off, tmp, (size_t)wrote);
    off += (size_t)wrote;
    returned++;
  }

  if (snap) free(snap);

  const int tail = snprintf(
      tmp, sizeof(tmp),
      "],\"count\":%u,\"returned\":%u,\"truncated\":%s}",
      (unsigned)total, (unsigned)returned, (truncated && returned < total) ? "true" : "false");
  if (tail < 0 || off + (size_t)tail >= gJsonCap) return 0;
  memcpy(gJson + off, tmp, (size_t)tail);
  off += (size_t)tail;
  gJson[off] = 0;
  return off;
}

static void lockApChannel();
static void setChannelManual(uint8_t ch, bool hop);
static bool bringUpSoftAp();

static void lockApChannel() {
  if (gChannel == AP_CHANNEL) return;
  if (esp_wifi_set_channel(AP_CHANNEL, WIFI_SECOND_CHAN_NONE) == ESP_OK) {
    gChannel = AP_CHANNEL;
  }
}

// —— OmniScan hooks ——
static OmniSnapshot omniSnap() {
  OmniSnapshot s = {};
  s.running = gOmniRunning;
  s.uptimeMs = millis();
  s.wifiChannel = gChannel;
  s.wifiHopping = gHopEnable;
  s.wifiDevices = gDev ? (uint32_t)gDev->size() : 0;
  s.handshakes = gHandshakeCount;
  s.deauths = gDeauthCount;
  s.bleOn = gBleScanIntent && bleEnabled();
  s.bleDevices = bleTrackedCount();
  s.wifiLr = wifiLrProtocolOk();
  s.subghzOn = subghzEnabled();
  s.subghzHopping = subghzHopping();
  s.subghzFreqMhz = subghzCurrentMhz();
  s.subghzPackets = subghzPacketCount();
  s.loraOn = loraReady();
  s.loraFreqMhz = 915.0f;
  s.loraPackets = gLoraPktCount;
  GpsFix g = gpsGet();
  s.gpsLocked = g.valid;
  s.gpsLat = g.lat;
  s.gpsLon = g.lon;
  s.gpsAlt = g.altM;
  s.gpsSpeedKmh = g.speedKmh;
  s.gpsSats = 0;
  s.gpsHdop = g.hdop;
  s.apOn = true;
  strncpy(s.apSsid, gApSsidBuf[0] ? gApSsidBuf : AP_SSID, sizeof s.apSsid - 1);
  s.apChannel = AP_CHANNEL;
  s.apClients = (uint8_t)WiFi.softAPgetStationNum();
  macStr(gApMac, s.apMac, sizeof s.apMac);
  macStr(gSelfMac, s.wifiMac, sizeof s.wifiMac);
  bleGetMacStr(s.bleMac, sizeof s.bleMac);
  snprintf(s.apIp, sizeof s.apIp, "%s", WiFi.softAPIP().toString().c_str());
  s.lrReady = wifiLrReady();
  s.lrRssi = wifiLrRssi();
  s.lrSent = wifiLrSent();
  s.lrAcked = wifiLrAcked();
  wifiLrGetPeer(s.lrPeer, sizeof s.lrPeer);
  if (!s.lrPeer[0] && gLrPeer[0]) strncpy(s.lrPeer, gLrPeer, sizeof s.lrPeer - 1);
  s.sdMounted = false;
  s.sdFreeGb = 0;
  snprintf(s.logPath, sizeof s.logPath, "psram://session");
  s.logEntries = s.wifiDevices;
  s.logSizeMb = 0;
  s.freeHeap = ESP.getFreeHeap();
  s.freePsram = ESP.getFreePsram();
  s.psramSize = ESP.getPsramSize();
  s.cpuTempC = temperatureRead();
  return s;
}

static bool omniSetRunning(bool on) {
  gOmniRunning = on;
  if (on) {
    esp_wifi_set_promiscuous(true);
    subghzSetEnabled(true);
    gBleScanIntent = true;
    bleSetEnabled(true);
  } else {
    bleSetEnabled(false);
  }
  return true;
}
static bool omniSetWifiCh(int ch) {
  if (ch == 0) {
    gHopEnable = true;
    return true;
  }
  if (ch == -1) {
    gHopEnable = false;
    return true;
  }
  if (ch >= 1 && ch <= 11) {
    setChannelManual((uint8_t)ch, false);
    return true;
  }
  return false;
}
static bool omniSetBle(bool on) {
  gBleScanIntent = on;
  bleSetEnabled(on);
  return true;
}
static bool omniSetSg(bool on) {
  subghzSetEnabled(on);
  return true;
}
static bool omniSetSgFreq(float mhz) {
  return subghzSetFrequencyMhz(mhz);
}
static bool omniSetLora(bool on) {
  (void)on;
  return loraReady();
}
static bool omniSetLoraFreq(float mhz) {
  (void)mhz;
  return true;
}
static bool omniGpsReset() { return true; }
static bool omniSetAp(bool on) {
  (void)on;
  return true;
}
static bool omniSetApSsid(const char* ssid) {
  if (!ssid || !ssid[0]) return false;
  strncpy(gApSsidBuf, ssid, sizeof gApSsidBuf - 1);
  bringUpSoftAp();
  return true;
}
static bool omniSetLrPeer(const char* mac) {
  if (!mac) return false;
  strncpy(gLrPeer, mac, sizeof gLrPeer - 1);
  gLrPeer[sizeof gLrPeer - 1] = 0;
  return wifiLrSetPeer(mac);
}
static bool omniLrSend(const char* msg) {
  return wifiLrSend(msg);
}
static bool omniLrTest(char* out, size_t n) {
  return wifiLrTest(out, n);
}
static bool omniLogSave(char* pathOut, size_t n) {
  snprintf(pathOut, n, "psram://omniscan_%lu.json", (unsigned long)millis());
  return true;
}
static bool omniLogDump(char* out, size_t n) {
  snprintf(out, n,
           "Dumping log...\n{\"exported_at_ms\":%lu,\"name\":\"root\",\"devices\":%u}\nDone.\n",
           (unsigned long)millis(), gDev ? (unsigned)gDev->size() : 0u);
  return true;
}
static bool omniSysReset() {
  delay(200);
  ESP.restart();
  return true;
}
static bool omniWifiHs(char* out, size_t n) {
  size_t u = 0;
  auto ap = [&](const char* s) {
    size_t l = strlen(s);
    if (u + l + 1 < n) {
      memcpy(out + u, s, l);
      u += l;
      out[u] = 0;
    }
  };
  char line[160];
  ap("=== HANDSHAKE STATS ===\n");
  snprintf(line, sizeof line, "Total Captured: %lu\n", (unsigned long)gHandshakeCount);
  ap(line);
  if (gLastHandshake.valid) {
    char m[18], b[18];
    macStr(gLastHandshake.mac, m, sizeof m);
    macStr(gLastHandshake.bssid, b, sizeof b);
    snprintf(line, sizeof line, "Last Capture: %lu ms uptime\nMAC: %s\nBSSID: %s\n",
             (unsigned long)gLastHandshake.ms, m, b);
    ap(line);
    snprintf(line, sizeof line, "SSID: %s\nRSSI: %d dBm\n",
             gLastHandshake.ssid[0] ? gLastHandshake.ssid : "(unknown)",
             (int)gLastHandshake.rssi);
    ap(line);
    GpsFix g = gpsGet();
    if (g.valid) {
      snprintf(line, sizeof line, "GPS: %.7f, %.7f\n", g.lat, g.lon);
      ap(line);
    }
  } else {
    ap("No EAPoL frames observed yet (passive listen)\n");
  }
  return true;
}
static bool omniWifiDe(char* out, size_t n) {
  char line[192];
  size_t u = 0;
  auto ap = [&](const char* s) {
    size_t l = strlen(s);
    if (u + l + 1 < n) {
      memcpy(out + u, s, l);
      u += l;
      out[u] = 0;
    }
  };
  ap("=== DEAUTH ATTACKS ===\n");
  snprintf(line, sizeof line, "Total Detected: %lu\n", (unsigned long)gDeauthCount);
  ap(line);
  if (gLastDeauth.valid) {
    char a[18], t[18];
    macStr(gLastDeauth.attacker, a, sizeof a);
    macStr(gLastDeauth.target, t, sizeof t);
    snprintf(line, sizeof line,
             "Last Attack: %lu ms uptime\nAttacker: %s\nTarget: %s\nRSSI: %d dBm\n",
             (unsigned long)gLastDeauth.ms, a, t, (int)gLastDeauth.rssi);
    ap(line);
    GpsFix g = gpsGet();
    if (g.valid) {
      snprintf(line, sizeof line, "GPS: %.7f, %.7f\n", g.lat, g.lon);
      ap(line);
    }
  } else {
    ap("No deauth/disassoc frames observed yet\n");
  }
  return true;
}
static bool omniBleList(char* out, size_t n) {
  return bleListText(out, n, gBleFilter[0] ? gBleFilter : nullptr);
}
static bool omniBleFilter(const char* name, char* out, size_t n) {
  if (!name) return false;
  if (strcasecmp(name, "clear") == 0) {
    gBleFilter[0] = 0;
    snprintf(out, n, "BLE filter cleared\n");
  } else {
    strncpy(gBleFilter, name, sizeof gBleFilter - 1);
    snprintf(out, n, "BLE filter set to: %s\nShowing only devices matching \"%s\"\n",
             name, name);
  }
  return true;
}
static bool omniSgList(char* out, size_t n) { return subghzListRecent(out, n); }
static bool omniSgRaw(const char* args, char* out, size_t n) {
  return subghzRawCommand(args, out, n);
}
static bool omniLoraList(char* out, size_t n) {
  snprintf(out, n,
           "=== LORA PACKETS (last) ===\n"
           "Ready: %s · UART bytes: %lu · hits: %lu\n"
           "Listening transparent RX on E22 (default 915 MHz)\n",
           loraReady() ? "yes" : "no",
           (unsigned long)loraUartBytes(), (unsigned long)gLoraPktCount);
  return true;
}

static void omniSetupHooks() {
  static OmniHooks hooks = {
      omniSnap,        omniSetRunning, omniSetWifiCh,   omniSetBle,
      omniSetSg,       omniSetSgFreq,  omniSetLora,     omniSetLoraFreq,
      omniGpsReset,    omniSetAp,      omniSetApSsid,   omniSetLrPeer,
      omniLrSend,      omniLrTest,     omniLogSave,     omniLogDump,
      omniSysReset,    omniWifiHs,     omniWifiDe,      omniBleList,
      omniBleFilter,   omniSgList,     omniSgRaw,       omniLoraList,
  };
  omniInit(&hooks);
}

static char gOmniOut[6144];

static void omniRespondHttp(AsyncWebServerRequest* req, const char* cmd) {
  const bool ok = omniHandle(cmd, gOmniOut, sizeof gOmniOut);
  // JSON-escape output roughly
  size_t need = strlen(gOmniOut) * 2 + 128;
  char* body = (char*)malloc(need);
  if (!body) {
    req->send(500, "application/json", "{\"ok\":false,\"error\":\"oom\"}");
    return;
  }
  size_t o = 0;
  o += (size_t)snprintf(body + o, need - o, "{\"ok\":%s,\"status\":\"%s\",\"output\":\"",
                        ok ? "true" : "false", ok ? "OK" : "ERROR");
  for (const char* p = gOmniOut; *p && o + 8 < need; p++) {
    char c = *p;
    if (c == '"' || c == '\\') {
      body[o++] = '\\';
      body[o++] = c;
    } else if (c == '\n') {
      body[o++] = '\\';
      body[o++] = 'n';
    } else if (c == '\r') {
      continue;
    } else if ((uint8_t)c < 32) {
      continue;
    } else {
      body[o++] = c;
    }
  }
  o += (size_t)snprintf(body + o, need - o, "\"}");
  req->send(200, "application/json", body);
  free(body);
}


static void serviceHop() {
  if (!gHopEnable) return;
  // Never move the SoftAP while a phone/laptop is joined — hopping kills the link.
  if (WiFi.softAPgetStationNum() > 0) {
    lockApChannel();
    return;
  }
  const uint32_t now = millis();
  const uint32_t dwell =
      (gChannel == AP_CHANNEL) ? ROOT_HOP_HOME_DWELL_MS : ROOT_HOP_AWAY_DWELL_MS;
  if (now - gLastHopMs < dwell) return;
  gLastHopMs = now;

  uint8_t next = gChannel + 1;
  if (next < 1 || next > 13) next = 1;
  if (esp_wifi_set_channel(next, WIFI_SECOND_CHAN_NONE) == ESP_OK) {
    gChannel = next;
  }
}

static void setChannelManual(uint8_t ch, bool hop) {
  gHopEnable = hop;
  if (ch < 1 || ch > 13) ch = AP_CHANNEL;
  if (esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE) == ESP_OK) {
    gChannel = ch;
  }
}

static void serviceLed() {
  const uint32_t now = millis();
  if (now - gLastLedMs < 20) return;
  gLastLedMs = now;

  if (!gPingActive) {
    const float b = 0.35f + 0.25f * sinf((now / 1000.0f) * 2.0f);
    uint8_t g = (uint8_t)(b * 18.0f);
    ledWrite(0, g, g / 3);
    return;
  }

  const uint32_t dur = 420;
  const uint32_t t = now - gPingStartMs;
  if (t >= dur) {
    gPingActive = false;
    ledWrite(0, 4, 1);
    return;
  }
  const float env = sinf((3.1415926f * (float)t) / (float)dur);
  const uint8_t g = (uint8_t)(env * 170.0f);
  ledWrite(0, g, g / 5);
}

static bool captivePath(const String& path) {
  return path.indexOf("generate_204") >= 0 || path.indexOf("gen_204") >= 0 ||
         path.indexOf("connectivitycheck") >= 0 || path.indexOf("clients3.google.com") >= 0 ||
         path.endsWith("/hotspot-detect.html") || path.indexOf("connecttest") >= 0 ||
         path.endsWith("/ncsi.txt") || path.indexOf("msftncsi") >= 0 ||
         path.indexOf("msftconnecttest") >= 0 || path.indexOf("mobile/status") >= 0 ||
         path.endsWith("/canonical.html") || path.endsWith("/success.txt") ||
         path.indexOf("captive.apple.com") >= 0 || path.indexOf("library/test") >= 0;
}

static void answerCaptive(AsyncWebServerRequest* req) {
  const String path = req->url();
  // Never return 204 here — phones/Edge treat that as “full internet” then HTTPS fails.
  if (path.indexOf("generate_204") >= 0 || path.indexOf("gen_204") >= 0 ||
      path.indexOf("connectivitycheck") >= 0) {
    req->redirect("http://192.168.4.1/");
    return;
  }
  if (path.endsWith("/hotspot-detect.html") || path.indexOf("captive.apple.com") >= 0 ||
      path.indexOf("library/test") >= 0) {
    req->send(200, "text/html",
              "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
    return;
  }
  if (path.endsWith("/ncsi.txt") || path.indexOf("msftncsi") >= 0) {
    req->send(200, "text/plain", "Microsoft NCSI");
    return;
  }
  if (path.indexOf("connecttest") >= 0 || path.indexOf("msftconnecttest") >= 0) {
    req->send(200, "text/plain", "Microsoft Connect Test");
    return;
  }
  if (path.indexOf("mobile/status") >= 0 || path.endsWith("/success.txt")) {
    req->send(200, "text/plain", "OK");
    return;
  }
  req->redirect("http://192.168.4.1/");
}

static void configureApRadio() {
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  esp_wifi_set_ps(WIFI_PS_NONE);
  esp_wifi_set_inactive_time(WIFI_IF_AP, 0);

  wifi_country_t country = {};
  strncpy(country.cc, "US", sizeof(country.cc));
  country.schan = 1;
  country.nchan = 11;
  country.policy = WIFI_COUNTRY_POLICY_AUTO;
  esp_wifi_set_country(&country);

  esp_wifi_set_max_tx_power(ROOT_WIFI_TX_QUARTER_DBM);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  // b/g/n for phones; LR OR'd in wifiLrInit() after SoftAP is up
  esp_wifi_set_protocol(WIFI_IF_AP,
                        WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
  esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20);
}

static bool bringUpSoftAp() {
  WiFi.softAPdisconnect(true);
  delay(80);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1),
                    IPAddress(255, 255, 255, 0));

  bool ok = false;
  for (uint8_t attempt = 0; attempt < 3 && !ok; attempt++) {
    const char* ssid = gApSsidBuf[0] ? gApSsidBuf : AP_SSID;
    if (AP_PASS && strlen(AP_PASS) >= 8) {
      ok = WiFi.softAP(ssid, AP_PASS, AP_CHANNEL, 0, 8);
    } else {
      ok = WiFi.softAP(ssid, nullptr, AP_CHANNEL, 0, 8);
    }
    if (!ok) delay(250);
  }
  delay(200);
  return ok && WiFi.softAPIP() != IPAddress(0, 0, 0, 0);
}

static void startRadio() {
  configureApRadio();
  const bool ok = bringUpSoftAp();
  gChannel = AP_CHANNEL;
  gHopEnable = false;

  WiFi.macAddress(gSelfMac);
  WiFi.softAPmacAddress(gApMac);

  gDns.start(53, "*", WiFi.softAPIP());

  esp_wifi_set_channel(AP_CHANNEL, WIFI_SECOND_CHAN_NONE);

  wifi_promiscuous_filter_t filter = {};
  filter.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA;
  esp_wifi_set_promiscuous_filter(&filter);
  esp_wifi_set_promiscuous_rx_cb(&snifferCb);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(AP_CHANNEL, WIFI_SECOND_CHAN_NONE);

  int8_t txQuarterDbm = 0;
  esp_wifi_get_max_tx_power(&txQuarterDbm);

  Serial.println(ok ? "root: SoftAP up" : "root: SoftAP failed");
  Serial.printf("root: join '%s' (ch %u, tx %.1f dBm) → http://%s\n",
                AP_SSID, (unsigned)AP_CHANNEL, txQuarterDbm * 0.25f,
                WiFi.softAPIP().toString().c_str());
  Serial.println("root: no internet on this AP is normal — phone stays connected via captive DNS");
  Serial.println("root: hop auto moves RX only when no clients — AP stays on ch 6 most of the time");

  char m1[18], m2[18];
  macStr(gSelfMac, m1, sizeof(m1));
  macStr(gApMac, m2, sizeof(m2));
  Serial.printf("root: self MAC %s  AP MAC %s\n", m1, m2);
  Serial.println("root: promiscuous listen armed — unlimited device table (PSRAM)");

  wifiLrInit();
}

static void startWeb() {
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Cache-Control", "no-store");

  gServer.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    AsyncWebServerResponse* r = req->beginResponse_P(200, "text/html", DASHBOARD_HTML);
    r->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    r->addHeader("Pragma", "no-cache");
    r->addHeader("Expires", "0");
    req->send(r);
  });

  gServer.on("/hotspot-detect.html", HTTP_GET, answerCaptive);
  gServer.on("/generate_204", HTTP_GET, answerCaptive);
  gServer.on("/gen_204", HTTP_GET, answerCaptive);
  gServer.on("/connecttest.txt", HTTP_GET, answerCaptive);
  gServer.on("/ncsi.txt", HTTP_GET, answerCaptive);
  gServer.on("/canonical.html", HTTP_GET, answerCaptive);
  gServer.on("/success.txt", HTTP_GET, answerCaptive);
  gServer.on("/mobile/status.php", HTTP_GET, answerCaptive);

  gServer.on("/api/ping", HTTP_GET, [](AsyncWebServerRequest* req) {
    char buf[128];
    snprintf(buf, sizeof(buf),
             "{\"ok\":true,\"name\":\"root\",\"count\":%u,\"uptime_ms\":%lu,"
             "\"stations\":%u}",
             gDev ? (unsigned)gDev->size() : 0u, (unsigned long)millis(),
             (unsigned)WiFi.softAPgetStationNum());
    req->send(200, "application/json", buf);
  });

  gServer.on("/api/devices", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (xSemaphoreTake(gJsonMux, pdMS_TO_TICKS(2000)) != pdTRUE) {
      req->send(503, "application/json", "{\"error\":\"busy\"}");
      return;
    }
    const size_t len = buildJson();
    if (!gJson || len == 0) {
      xSemaphoreGive(gJsonMux);
      req->send(503, "application/json", "{\"error\":\"json\"}");
      return;
    }
    // Copy out of PSRAM before send — AsyncTCP cannot stream PSRAM safely.
    String body;
    body.reserve(len + 1);
    body.concat(gJson, len);
    xSemaphoreGive(gJsonMux);
    req->send(200, "application/json", body);
  });

  gServer.on("/api/sightings", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (!req->hasParam("mac")) {
      req->send(400, "application/json", "{\"error\":\"mac required\"}");
      return;
    }
    String mac = req->getParam("mac")->value();
    mac.trim();
    mac.toUpperCase();
    uint8_t m[6] = {0};
    unsigned a, b, c, d, e, f;
    if (sscanf(mac.c_str(), "%X:%X:%X:%X:%X:%X", &a, &b, &c, &d, &e, &f) != 6) {
      req->send(400, "application/json", "{\"error\":\"bad mac\"}");
      return;
    }
    m[0] = a; m[1] = b; m[2] = c; m[3] = d; m[4] = e; m[5] = f;
    char* buf = (char*)heap_caps_malloc(65536, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) buf = (char*)malloc(65536);
    if (!buf) {
      req->send(503, "application/json", "{\"error\":\"oom\"}");
      return;
    }
    sightBuildMacJson(m, buf, 65536);
    const String body(buf);
    free(buf);
    req->send(200, "application/json", body);
  });

  gServer.on("/api/rf", HTTP_GET, [](AsyncWebServerRequest* req) {
    char sg[256], lo[256];
    subghzStatusJson(sg, sizeof sg);
    loraStatusJson(lo, sizeof lo);
    char buf[640];
    snprintf(buf, sizeof buf, "{\"subghz\":%s,\"lora\":%s}", sg, lo);
    req->send(200, "application/json", buf);
  });

  // Laptop GPS inject — browser geolocation or gpsd on the operator machine
  gServer.on("/api/gps", HTTP_POST, [](AsyncWebServerRequest* req) {},
             NULL,
             [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
               if (!data || len == 0 || len > 512) {
                 req->send(400, "application/json", "{\"error\":\"body\"}");
                 return;
               }
               char body[513];
               size_t n = len < 512 ? len : 512;
               memcpy(body, data, n);
               body[n] = 0;
               double lat = 0, lon = 0;
               float alt = 0, hdop = 1.5f;
               // minimal parse: "lat":N "lon"/"lng":N
               const char* plat = strstr(body, "\"lat\"");
               const char* plon = strstr(body, "\"lon\"");
               if (!plon) plon = strstr(body, "\"lng\"");
               if (!plat || !plon) {
                 req->send(400, "application/json", "{\"error\":\"lat/lon required\"}");
                 return;
               }
               plat = strchr(plat, ':');
               plon = strchr(plon, ':');
               if (!plat || !plon) {
                 req->send(400, "application/json", "{\"error\":\"parse\"}");
                 return;
               }
               lat = atof(plat + 1);
               lon = atof(plon + 1);
               const char* palt = strstr(body, "\"alt\"");
               if (palt) {
                 palt = strchr(palt, ':');
                 if (palt) alt = (float)atof(palt + 1);
               }
               const char* ph = strstr(body, "\"hdop\"");
               if (!ph) ph = strstr(body, "\"accuracy\"");
               if (ph) {
                 ph = strchr(ph, ':');
                 if (ph) {
                   float a = (float)atof(ph + 1);
                   // browser accuracy is meters — map roughly to hdop
                   hdop = a > 0 ? (a / 5.0f) : 1.5f;
                 }
               }
               gpsInject(lat, lon, alt, hdop);
               const GpsFix f = gpsGet();
               char out[160];
               snprintf(out, sizeof out,
                        "{\"ok\":%s,\"lat\":%.7f,\"lon\":%.7f,\"source\":\"laptop\"}",
                        f.valid ? "true" : "false", f.lat, f.lon);
               req->send(200, "application/json", out);
             });

  gServer.on("/api/gps", HTTP_GET, [](AsyncWebServerRequest* req) {
    const GpsFix f = gpsGet();
    char out[192];
    snprintf(out, sizeof out,
             "{\"valid\":%s,\"lat\":%.7f,\"lon\":%.7f,\"alt\":%.1f,\"hdop\":%.2f}",
             f.valid ? "true" : "false", f.lat, f.lon, f.altM, f.hdop);
    req->send(200, "application/json", out);
  });

  gServer.on("/api/channel", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (req->hasParam("ch")) {
      const String v = req->getParam("ch")->value();
      if (v == "auto") {
        gHopEnable = true;
      } else {
        const int ch = v.toInt();
        if (ch >= 1 && ch <= 13) setChannelManual((uint8_t)ch, false);
      }
    }
    char buf[96];
    snprintf(buf, sizeof(buf), "{\"channel\":%u,\"hopping\":%s}",
             (unsigned)gChannel, gHopEnable ? "true" : "false");
    req->send(200, "application/json", buf);
  });

  gServer.on("/api/omni", HTTP_GET, [](AsyncWebServerRequest* req) {
    String held = "./omni status";
    if (req->hasParam("cmd")) held = req->getParam("cmd")->value();
    omniRespondHttp(req, held.c_str());
  });
  gServer.on(
      "/api/omni", HTTP_POST, [](AsyncWebServerRequest* req) {}, NULL,
      [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
        char cmd[256];
        if ((!data || len == 0) && req->hasParam("cmd", true)) {
          omniRespondHttp(req, req->getParam("cmd", true)->value().c_str());
          return;
        }
        if (!data || len == 0 || len >= sizeof cmd) {
          req->send(400, "application/json", "{\"ok\":false,\"error\":\"cmd required\"}");
          return;
        }
        size_t n = len < sizeof(cmd) - 1 ? len : sizeof(cmd) - 1;
        memcpy(cmd, data, n);
        cmd[n] = 0;
        const char* use = cmd;
        char extracted[200];
        const char* p = strstr(cmd, "\"cmd\"");
        if (p) {
          p = strchr(p + 5, '"');
          if (p) {
            p++;
            size_t i = 0;
            while (*p && *p != '"' && i + 1 < sizeof extracted) {
              if (*p == '\\' && p[1]) {
                p++;
                extracted[i++] = *p++;
              } else {
                extracted[i++] = *p++;
              }
            }
            extracted[i] = 0;
            use = extracted;
          }
        }
        omniRespondHttp(req, use);
      });

  gServer.onNotFound([](AsyncWebServerRequest* req) {
    if (req->method() == HTTP_GET) {
      if (captivePath(req->url())) {
        answerCaptive(req);
        return;
      }
      req->redirect("http://192.168.4.1/");
      return;
    }
    req->send(404, "text/plain", "root: not found");
  });

  gServer.begin();
}

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println();
  Serial.println("========================================");
  Serial.println("  root  — Wi-Fi LR + BLE + CC1101 + LR22");
  Serial.println("========================================");
  strncpy(gApSsidBuf, ROOT_AP_SSID, sizeof gApSsidBuf - 1);
  omniSetupHooks();
  Serial.println("root: OmniScan ./omni commands ready (Serial + /api/omni)");

  sightInit();
  gDev = new DevMap();
  gSessionStartMs = millis();
  gMux = xSemaphoreCreateMutex();
  gJsonMux = xSemaphoreCreateMutex();
  gQueue = xQueueCreate(QUEUE_LEN, sizeof(SniffHit));
  if (!gDev || !gMux || !gJsonMux || !gQueue) {
    Serial.println("root: failed to allocate queue/mutex/map");
    return;
  }

  ledWrite(0, 12, 4);
  startRadio();
#if ROOT_ENABLE_BLE
  bleInit();
  bleSetEnabled(gBleScanIntent);
#endif
#if ROOT_ENABLE_SUBGHZ
  subghzInit();
#endif
#if ROOT_ENABLE_LORA
  loraInit();
#endif
  gpsInit();  // UART when ROOT_ENABLE_GPS; always accepts laptop inject via /api/gps
  startWeb();
}

static void serviceSerialCmds() {
  static char line[160];
  static size_t n = 0;
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      line[n] = 0;
      n = 0;
      if (line[0] == 0) continue;
      // OmniScan ./omni … (and aliases handled inside omniHandle)
      if (strncmp(line, "./omni", 6) == 0 || strncmp(line, "omni", 4) == 0 ||
          strcmp(line, "s") == 0 || strcmp(line, "h") == 0 || strcmp(line, "d") == 0 ||
          strncmp(line, "ch ", 3) == 0 || strcmp(line, "start") == 0 ||
          strcmp(line, "stop") == 0) {
        omniHandle(line, gOmniOut, sizeof gOmniOut);
        Serial.print(gOmniOut);
        continue;
      }
      if (strcmp(line, "status") == 0) {
        Serial.printf("root: ch=%u hopping=%s devices=%u subghz=%s lora=%s omni=%s\n",
                      (unsigned)gChannel, gHopEnable ? "true" : "false",
                      gDev ? (unsigned)gDev->size() : 0u,
                      subghzReady() ? "up" : "down",
                      loraReady() ? "up" : "down",
                      gOmniRunning ? "run" : "stop");
      } else if (strncmp(line, "hop ", 4) == 0) {
        const char* arg = line + 4;
        if (strcmp(arg, "auto") == 0) {
          gHopEnable = true;
          Serial.println("root: hop auto");
        } else {
          const int ch = atoi(arg);
          if (ch >= 1 && ch <= 13) {
            setChannelManual((uint8_t)ch, false);
            Serial.printf("root: hop %d\n", ch);
          } else {
            Serial.println("root: hop needs 1-13 or auto");
          }
        }
      } else if (strcmp(line, "rf") == 0) {
        char sg[256], lo[256];
        subghzStatusJson(sg, sizeof sg);
        loraStatusJson(lo, sizeof lo);
        Serial.printf("root rf: {\"subghz\":%s,\"lora\":%s}\n", sg, lo);
      } else if (strcmp(line, "help") == 0) {
        Serial.println("root cmds: status | rf | hop auto | hop <1-13> | ./omni …");
        omniHandle("./omni system help", gOmniOut, sizeof gOmniOut);
        Serial.print(gOmniOut);
      } else {
        // Try as omni shorthand
        omniHandle(line, gOmniOut, sizeof gOmniOut);
        if (strstr(gOmniOut, "Unknown command")) {
          Serial.println("root: unknown — try help or ./omni system help");
        } else {
          Serial.print(gOmniOut);
        }
      }
      continue;
    }
    if (n + 1 < sizeof(line)) line[n++] = c;
    else n = 0;
  }
}

void loop() {
  for (int i = 0; i < 32; i++) gDns.processNextRequest();
  const uint32_t now = millis();
  gpsPoll();  // UART when ROOT_ENABLE_GPS; inject TTL handled in gpsGet()
  SniffHit hit;
  int budget = 20;
  while (budget-- > 0 && xQueueReceive(gQueue, &hit, 0) == pdTRUE) {
    ingestHit(hit);
  }

#if ROOT_ENABLE_SUBGHZ
  serviceSubGhz(now);
#endif

#if ROOT_ENABLE_LORA
  serviceLora(now);
#endif

#if ROOT_ENABLE_BLE
  serviceBle(now);
#endif

  serviceSerialCmds();
  pruneStale();
  serviceHop();
  serviceLed();
  delay(1);
}
