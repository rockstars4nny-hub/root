/*
 * root — passive Wi-Fi echolocation + CC1101 sub-GHz + LR22 LoRa (ESP32-S3)
 *
 * Join Wi-Fi  SSID: root   password: root-radar
 * Dashboard: http://192.168.4.1
 * API:       /api/devices  /api/rf  /api/channel
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
#include "root_gps.h"
#include "sight_log.h"
#include "geo.h"

#if __has_include("esp32-hal-rgb-led.h")
#include "esp32-hal-rgb-led.h"
#define CI_HAS_NEOPIXEL 1
#else
#define CI_HAS_NEOPIXEL 0
#endif

static const char* AP_SSID = "root";
static const char* AP_PASS = "root-radar";
static const uint8_t AP_CHANNEL = 6;
// Finch-style session: keep devices on the board for minutes, not 30s.
static const uint32_t STALE_MS = 300000;
static const uint32_t HOP_DWELL_MS = 280;
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
  KIND_LORA = 6
};

struct SniffHit {
  uint8_t mac[6];
  int8_t rssi;
  uint8_t channel;
  uint8_t kind;
  char ssid[33];
  char band[12];
};

struct Device {
  uint8_t mac[6];
  int8_t rssi;
  int8_t hist[3];
  uint8_t histCount;
  uint8_t histIdx;
  float avgRssi;
  uint32_t lastSeenMs;
  uint32_t firstSeenMs;
  uint32_t seenCount;
  uint8_t channel;
  uint8_t kind;
  char ssid[33];
  char band[12];
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

static void snifferCb(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (type == WIFI_PKT_MISC || buf == nullptr || gQueue == nullptr) return;

  const wifi_promiscuous_pkt_t* pkt = static_cast<wifi_promiscuous_pkt_t*>(buf);
  const uint16_t len = pkt->rx_ctrl.sig_len;
  if (len < 16) return;

  const uint8_t* p = pkt->payload;
  const uint8_t ftype = (p[0] & 0x0C) >> 2;
  const uint8_t subtype = (p[0] & 0xF0) >> 4;
  if (ftype == 1) return;

  SniffHit hit;
  memcpy(hit.mac, p + 10, 6);
  if (macIgnored(hit.mac)) return;

  hit.rssi = pkt->rx_ctrl.rssi;
  hit.channel = pkt->rx_ctrl.channel;
  hit.ssid[0] = 0;
  strncpy(hit.band, "wifi", sizeof hit.band - 1);
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
  } else {
    hit.kind = KIND_MGMT;
  }

  xQueueSend(gQueue, &hit, 0);
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

static void ingestHit(const SniffHit& hit) {
  if (!gDev || xSemaphoreTake(gMux, pdMS_TO_TICKS(20)) != pdTRUE) return;

  const uint64_t key = macKey(hit.mac);
  auto it = gDev->find(key);
  const bool isNew = (it == gDev->end());
  Device& d = isNew ? (*gDev)[key] : it->second;
  const int8_t prev = isNew ? hit.rssi : d.rssi;
  const bool bigMove = !isNew && abs((int)hit.rssi - (int)prev) >= PING_DELTA_DB;

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
  d.lastSeenMs = millis();
  d.channel = hit.channel;
  d.kind = hit.kind;
  strncpy(d.band, hit.band, sizeof d.band - 1);
  if (hit.ssid[0]) {
    strncpy(d.ssid, hit.ssid, sizeof(d.ssid) - 1);
    d.ssid[sizeof(d.ssid) - 1] = 0;
  }

  const float dist = rssiToDistance(hit.rssi);
  sightPush(d.mac, d.lastSeenMs, hit.rssi, dist);

  const bool shouldPing = isNew || bigMove || hit.kind == KIND_DEAUTH;
  char macbuf[18];
  if (shouldPing) macStr(d.mac, macbuf, sizeof(macbuf));

  xSemaphoreGive(gMux);

  if (shouldPing) {
    const bool quiet = WiFi.softAPgetStationNum() > 0;
    const uint32_t logNow = millis();
    if (!quiet || (logNow - gLastPingLogMs >= 2500)) {
      gLastPingLogMs = logNow;
      Sighting last;
      const bool hasS = sightLatest(d.mac, &last);
      if (hit.ssid[0]) {
        if (hasS && last.gpsValid) {
          Serial.printf(
            "root ping: [%s] | kind: %s | band: %s | ssid: %s | ch: %u | RSSI: %d dBm | "
            "Distance: ~%.1fm | @ %.6f,%.6f\n",
            macbuf, kindName(hit.kind), hit.band, hit.ssid, (unsigned)hit.channel,
            (int)hit.rssi, dist, last.lat, last.lon);
        } else if (hasS) {
          Serial.printf(
            "root ping: [%s] | kind: %s | band: %s | ssid: %s | ch: %u | RSSI: %d dBm | "
            "Distance: ~%.1fm (est)\n",
            macbuf, kindName(hit.kind), hit.band, hit.ssid, (unsigned)hit.channel,
            (int)hit.rssi, dist);
        } else {
          Serial.printf(
            "root ping: [%s] | kind: %s | band: %s | ssid: %s | ch: %u | RSSI: %d dBm | Distance: ~%.1fm\n",
            macbuf, kindName(hit.kind), hit.band, hit.ssid, (unsigned)hit.channel, (int)hit.rssi, dist);
        }
      } else {
        if (hasS && last.gpsValid) {
          Serial.printf(
            "root ping: [%s] | kind: %s | band: %s | ch: %u | RSSI: %d dBm | "
            "Distance: ~%.1fm | @ %.6f,%.6f\n",
            macbuf, kindName(hit.kind), hit.band, (unsigned)hit.channel,
            (int)hit.rssi, dist, last.lat, last.lon);
        } else if (hasS) {
          Serial.printf(
            "root ping: [%s] | kind: %s | band: %s | ch: %u | RSSI: %d dBm | "
            "Distance: ~%.1fm (est)\n",
            macbuf, kindName(hit.kind), hit.band, (unsigned)hit.channel,
            (int)hit.rssi, dist);
        } else {
          Serial.printf(
            "root ping: [%s] | kind: %s | band: %s | ch: %u | RSSI: %d dBm | Distance: ~%.1fm\n",
            macbuf, kindName(hit.kind), hit.band, (unsigned)hit.channel, (int)hit.rssi, dist);
        }
      }
    }
    triggerPingLed();
  }
}

static void ingestSubGhz(const SubGhzHit& sg) {
  SniffHit hit;
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
  SniffHit hit;
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
    } else if (strcmp(it->second.band, "subghz") == 0) staleMs = 120000;
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

  ensureJson(1024 + n * 320);
  if (!gJson) {
    if (snap) free(snap);
    return 0;
  }

  size_t returned = 0;
  bool truncated = false;
  char tmp[320];
  char esc[96];
  const GpsFix scanner = gpsGet();

  size_t off = (size_t)snprintf(
      gJson, gJsonCap,
      "{\"name\":\"root\",\"uptime_ms\":%lu,\"session_ms\":%lu,\"channel\":%u,\"hopping\":%s,"
      "\"stations\":%u,\"subghz\":%s,\"lora\":%s,"
      "\"scanner_gps\":{\"valid\":%s,\"lat\":%.7f,\"lon\":%.7f},\"devices\":[",
      (unsigned long)now, (unsigned long)(now - gSessionStartMs), (unsigned)gChannel,
      gHopEnable ? "true" : "false",
      (unsigned)WiFi.softAPgetStationNum(),
      subghzReady() ? "true" : "false", loraReady() ? "true" : "false",
      scanner.valid ? "true" : "false", jsonNum(scanner.lat), jsonNum(scanner.lon));

  for (size_t i = 0; i < n; i++) {
    if (off + 360 >= gJsonCap) {
      truncated = true;
      break;
    }
    char macbuf[18];
    macStr(snap[i].d.mac, macbuf, sizeof(macbuf));
    const uint32_t age = now - snap[i].d.lastSeenMs;
    const uint32_t sinceFirst = now - snap[i].d.firstSeenMs;
    Sighting last;
    const bool hasLast = sightLatest(snap[i].d.mac, &last);
    jsonEscape(snap[i].d.ssid[0] ? snap[i].d.ssid : "", esc, sizeof esc);
    const int wrote = snprintf(
        tmp, sizeof(tmp),
        "%s{\"mac\":\"%s\",\"rssi\":%d,\"avg\":%.1f,\"distance_m\":%.2f,"
        "\"zone\":\"%s\",\"last_seen_ms\":%lu,\"first_seen_ms\":%lu,"
        "\"seen_count\":%lu,\"ch\":%u,\"kind\":\"%s\","
        "\"band\":\"%s\",\"ssid\":\"%s\",\"rand\":%s,"
        "\"lat\":%.7f,\"lon\":%.7f,\"gps\":%s,\"sightings\":[]}",
        returned ? "," : "", macbuf, (int)snap[i].d.rssi, snap[i].d.avgRssi,
        rssiToDistance(snap[i].d.avgRssi), zoneName(snap[i].d.avgRssi),
        (unsigned long)age, (unsigned long)sinceFirst,
        (unsigned long)snap[i].d.seenCount,
        (unsigned)snap[i].d.channel, kindName(snap[i].d.kind),
        snap[i].d.band[0] ? snap[i].d.band : "wifi",
        esc,
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

static void serviceHop() {
  if (!gHopEnable) return;
  if (WiFi.softAPgetStationNum() > 0) return;
  const uint32_t now = millis();
  const uint32_t dwell = (gChannel == AP_CHANNEL) ? 1200 : HOP_DWELL_MS;
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
         path.endsWith("/hotspot-detect.html") || path.indexOf("connecttest") >= 0 ||
         path.endsWith("/ncsi.txt") || path.indexOf("mobile/status") >= 0 ||
         path.endsWith("/canonical.html") || path.endsWith("/success.txt");
}

static void answerCaptive(AsyncWebServerRequest* req) {
  const String path = req->url();
  if (path.indexOf("generate_204") >= 0 || path.indexOf("gen_204") >= 0) {
    req->send(204);
    return;
  }
  if (path.endsWith("/hotspot-detect.html")) {
    req->send(200, "text/html",
              "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
    return;
  }
  if (path.indexOf("connecttest") >= 0 || path.endsWith("/ncsi.txt")) {
    req->send(200, "text/plain", "Microsoft Connect Test");
    return;
  }
  if (path.indexOf("mobile/status") >= 0 || path.endsWith("/success.txt")) {
    req->send(200, "text/plain", "OK");
    return;
  }
  req->redirect("http://192.168.4.1/");
}

static void startRadio() {
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  esp_wifi_set_ps(WIFI_PS_NONE);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));

  bool ok;
  if (AP_PASS && strlen(AP_PASS) >= 8) {
    ok = WiFi.softAP(AP_SSID, AP_PASS, AP_CHANNEL, 0, 8);
  } else {
    ok = WiFi.softAP(AP_SSID, nullptr, AP_CHANNEL, 0, 8);
  }

  delay(150);
  gChannel = AP_CHANNEL;

  WiFi.macAddress(gSelfMac);
  WiFi.softAPmacAddress(gApMac);

  gDns.start(53, "*", WiFi.softAPIP());

  wifi_promiscuous_filter_t filter = {};
  filter.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA;
  esp_wifi_set_promiscuous_filter(&filter);
  esp_wifi_set_promiscuous_rx_cb(&snifferCb);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(AP_CHANNEL, WIFI_SECOND_CHAN_NONE);

  Serial.println(ok ? "root: SoftAP up" : "root: SoftAP failed");
  Serial.printf("root: join '%s'  then open http://%s\n", AP_SSID, WiFi.softAPIP().toString().c_str());
  Serial.println("root: no internet on this AP is normal — phone stays connected via captive DNS");

  char m1[18], m2[18];
  macStr(gSelfMac, m1, sizeof(m1));
  macStr(gApMac, m2, sizeof(m2));
  Serial.printf("root: self MAC %s  AP MAC %s\n", m1, m2);
  Serial.println("root: promiscuous listen armed — unlimited device table (PSRAM)");
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
  Serial.println("  root  — Wi-Fi + CC1101 + LR22");
  Serial.println("========================================");

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
#if ROOT_ENABLE_SUBGHZ
  subghzInit();
#endif
#if ROOT_ENABLE_LORA
  loraInit();
#endif
#if ROOT_ENABLE_GPS
  gpsInit();
#endif
  startWeb();
}

static void serviceSerialCmds() {
  static char line[48];
  static size_t n = 0;
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      line[n] = 0;
      n = 0;
      if (line[0] == 0) continue;
      if (strcmp(line, "status") == 0) {
        Serial.printf("root: ch=%u hopping=%s devices=%u subghz=%s lora=%s\n",
                      (unsigned)gChannel, gHopEnable ? "true" : "false",
                      gDev ? (unsigned)gDev->size() : 0u,
                      subghzReady() ? "up" : "down",
                      loraReady() ? "up" : "down");
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
        Serial.println("root cmds: status | rf | hop auto | hop <1-13>");
      }
      continue;
    }
    if (n + 1 < sizeof(line)) line[n++] = c;
    else n = 0;
  }
}

void loop() {
  for (int i = 0; i < 4; i++) gDns.processNextRequest();
  const uint32_t now = millis();
#if ROOT_ENABLE_GPS
  gpsPoll();
#endif
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

  serviceSerialCmds();
  pruneStale();
  serviceHop();
  serviceLed();
  delay(1);
}
