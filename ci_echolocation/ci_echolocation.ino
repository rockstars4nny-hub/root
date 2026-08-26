/*
 * root — passive Wi-Fi echolocation for ESP32-S3
 *
 * Listens in 802.11 promiscuous mode (no association to a foreign AP) and
 * tracks nearby radios by transmitter MAC + RSSI. Serves a live radar
 * dashboard from a local SoftAP.
 *
 * Arduino IDE:
 *   Board: ESP32S3 Dev Module (or your S3 variant)
 *   Libraries: ESPAsyncWebServer, AsyncTCP (ESP32Async)
 *   USB CDC on Boot: Enabled on native-USB S3 boards
 *
 * PlatformIO: open the parent root/ folder and flash env:esp32-s3-n16r8
 *
 * After flash:
 *   1. Serial Monitor @ 115200 — watch "root ping:" lines
 *   2. Join Wi-Fi  SSID: root   password: root-radar
 *   3. Open http://192.168.4.1
 *
 * This firmware is receive-only. It does not inject frames, deauth, or join
 * other networks. Use it only where you are authorized to observe RF.
 */

#include <WiFi.h>
#include <esp_wifi.h>
#include <math.h>
#include <ESPAsyncWebServer.h>
#include "ci_dashboard.h"

#if __has_include("esp32-hal-rgb-led.h")
#include "esp32-hal-rgb-led.h"
#define CI_HAS_NEOPIXEL 1
#else
#define CI_HAS_NEOPIXEL 0
#endif

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------
static const char* AP_SSID = "root";
static const char* AP_PASS = "root-radar";  // 8+ chars; "" for an open AP
static const uint8_t AP_CHANNEL = 6;
static const uint8_t MAX_DEVICES = 64;
static const uint32_t STALE_MS = 30000;
static const uint32_t HOP_DWELL_MS = 280;
static const int8_t PING_DELTA_DB = 5;
static const float TX_POWER_DBM = -50.0f;  // RSSI at ~1 m in the log-distance model
static const float PATH_LOSS_N = 2.0f;
static const uint8_t QUEUE_LEN = 64;

// WS2812 data pin. ESP32-S3-DevKitC-1 onboard RGB is typically GPIO 48.
// Override with -DCI_WS2812_PIN=n  or change here. Set to 255 to disable.
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

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------
enum FrameKind : uint8_t { KIND_PROBE = 0, KIND_BEACON = 1, KIND_DATA = 2, KIND_MGMT = 3 };

struct SniffHit {
  uint8_t mac[6];
  int8_t rssi;
  uint8_t channel;
  uint8_t kind;
};

struct Device {
  uint8_t mac[6];
  int8_t rssi;
  int8_t hist[3];
  uint8_t histCount;
  uint8_t histIdx;
  float avgRssi;
  uint32_t lastSeenMs;
  uint8_t channel;
  uint8_t kind;
  bool inUse;
};

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------
static QueueHandle_t gQueue = nullptr;
static SemaphoreHandle_t gMux = nullptr;
static SemaphoreHandle_t gJsonMux = nullptr;
static Device gDev[MAX_DEVICES];
static uint8_t gSelfMac[6] = {0};
static uint8_t gApMac[6] = {0};
static volatile uint8_t gChannel = AP_CHANNEL;
static volatile bool gHopEnable = true;
static uint32_t gLastHopMs = 0;
static uint32_t gLastPruneMs = 0;
static uint32_t gLastLedMs = 0;
static uint8_t gLastCount = 0;
static uint32_t gPingStartMs = 0;
static bool gPingActive = false;
static AsyncWebServer gServer(80);
static char gJson[12288];

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static bool macEq(const uint8_t* a, const uint8_t* b) {
  return memcmp(a, b, 6) == 0;
}

static bool macIgnored(const uint8_t* mac) {
  if (mac[0] & 0x01) return true;                          // multicast / broadcast
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
  return (mac[0] & 0x02) != 0;  // locally administered bit
}

static const char* kindName(uint8_t k) {
  switch (k) {
    case KIND_PROBE: return "probe";
    case KIND_BEACON: return "beacon";
    case KIND_DATA: return "data";
    default: return "mgmt";
  }
}

static const char* zoneName(float rssi) {
  if (rssi >= -55.0f) return "Near";
  if (rssi >= -75.0f) return "Mid";
  return "Far";
}

// d = 10^((TxPower - RSSI) / (20 * n))   TxPower=-50 dBm, n=2.0
static float rssiToDistance(float rssi) {
  float d = powf(10.0f, (TX_POWER_DBM - rssi) / (20.0f * PATH_LOSS_N));
  if (d < 0.1f) d = 0.1f;
  if (d > 80.0f) d = 80.0f;
  return d;
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

// ---------------------------------------------------------------------------
// Promiscuous RX — Wi-Fi task. Keep it short: parse header, enqueue, return.
// ---------------------------------------------------------------------------
static void snifferCb(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (type == WIFI_PKT_MISC || buf == nullptr || gQueue == nullptr) return;

  const wifi_promiscuous_pkt_t* pkt = static_cast<wifi_promiscuous_pkt_t*>(buf);
  const uint16_t len = pkt->rx_ctrl.sig_len;
  if (len < 16) return;

  const uint8_t* p = pkt->payload;
  const uint8_t ftype = (p[0] & 0x0C) >> 2;
  const uint8_t subtype = (p[0] & 0xF0) >> 4;
  if (ftype == 1) return;  // control frames: different header layout

  SniffHit hit;
  memcpy(hit.mac, p + 10, 6);  // addr2 = transmitter
  if (macIgnored(hit.mac)) return;

  hit.rssi = pkt->rx_ctrl.rssi;
  hit.channel = pkt->rx_ctrl.channel;
  if (ftype == 2) {
    hit.kind = KIND_DATA;
  } else if (ftype == 0 && subtype == 4) {
    hit.kind = KIND_PROBE;
  } else if (ftype == 0 && subtype == 8) {
    hit.kind = KIND_BEACON;
  } else {
    hit.kind = KIND_MGMT;
  }

  // Callback runs in the Wi-Fi task, not an ISR — never block here.
  xQueueSend(gQueue, &hit, 0);
}

// ---------------------------------------------------------------------------
// Device table
// ---------------------------------------------------------------------------
static int findDeviceLocked(const uint8_t* mac) {
  int freeSlot = -1;
  int stalest = -1;
  uint32_t oldestSeen = UINT32_MAX;
  for (int i = 0; i < MAX_DEVICES; i++) {
    if (!gDev[i].inUse) {
      if (freeSlot < 0) freeSlot = i;
      continue;
    }
    if (macEq(gDev[i].mac, mac)) return i;
    if (gDev[i].lastSeenMs < oldestSeen) {
      oldestSeen = gDev[i].lastSeenMs;
      stalest = i;
    }
  }
  if (freeSlot >= 0) return freeSlot;
  return stalest;  // cap: reuse oldest
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
  if (xSemaphoreTake(gMux, pdMS_TO_TICKS(20)) != pdTRUE) return;

  const int idx = findDeviceLocked(hit.mac);
  if (idx < 0) {
    xSemaphoreGive(gMux);
    return;
  }

  Device& d = gDev[idx];
  const bool isNew = !d.inUse || !macEq(d.mac, hit.mac);
  const int8_t prev = isNew ? hit.rssi : d.rssi;
  const bool bigMove = !isNew && abs((int)hit.rssi - (int)prev) >= PING_DELTA_DB;

  if (isNew) {
    memset(&d, 0, sizeof(Device));
    memcpy(d.mac, hit.mac, 6);
    d.inUse = true;
  }

  pushRssi(d, hit.rssi);
  d.lastSeenMs = millis();
  d.channel = hit.channel;
  d.kind = hit.kind;

  const float dist = rssiToDistance(d.avgRssi);
  const bool shouldPing = isNew || bigMove;
  char macbuf[18];
  if (shouldPing) macStr(d.mac, macbuf, sizeof(macbuf));

  xSemaphoreGive(gMux);

  if (shouldPing) {
    Serial.printf("root ping: [%s] | RSSI: %d dBm | Distance: ~%.1fm\n",
                  macbuf, (int)hit.rssi, dist);
    triggerPingLed();
  }
}

static void pruneStale() {
  const uint32_t now = millis();
  if (now - gLastPruneMs < 400) return;
  gLastPruneMs = now;

  uint8_t live = 0;
  if (xSemaphoreTake(gMux, pdMS_TO_TICKS(20)) != pdTRUE) return;
  for (int i = 0; i < MAX_DEVICES; i++) {
    if (!gDev[i].inUse) continue;
    if (now - gDev[i].lastSeenMs > STALE_MS) {
      gDev[i].inUse = false;
    } else {
      live++;
    }
  }
  xSemaphoreGive(gMux);

  if (live != gLastCount) {
    gLastCount = live;
    triggerPingLed();
  }
}

// Snapshot + sort by average RSSI (strongest first), then emit JSON.
static size_t buildJson() {
  Device snap[MAX_DEVICES];
  uint8_t n = 0;
  const uint32_t now = millis();

  if (xSemaphoreTake(gMux, pdMS_TO_TICKS(40)) != pdTRUE) {
    return snprintf(gJson, sizeof(gJson), "{\"count\":0,\"devices\":[]}");
  }
  for (int i = 0; i < MAX_DEVICES; i++) {
    if (gDev[i].inUse) snap[n++] = gDev[i];
  }
  xSemaphoreGive(gMux);

  for (uint8_t i = 1; i < n; i++) {
    Device key = snap[i];
    int j = i - 1;
    while (j >= 0 && snap[j].avgRssi < key.avgRssi) {
      snap[j + 1] = snap[j];
      j--;
    }
    snap[j + 1] = key;
  }

  size_t off = 0;
  auto app = [&](const char* s) {
    size_t L = strlen(s);
    if (off + L >= sizeof(gJson)) return;
    memcpy(gJson + off, s, L);
    off += L;
  };

  char tmp[192];
  snprintf(tmp, sizeof(tmp),
           "{\"name\":\"root\",\"uptime_ms\":%lu,\"channel\":%u,\"hopping\":%s,"
           "\"stations\":%u,\"count\":%u,\"devices\":[",
           (unsigned long)now, (unsigned)gChannel, gHopEnable ? "true" : "false",
           (unsigned)WiFi.softAPgetStationNum(), (unsigned)n);
  app(tmp);

  for (uint8_t i = 0; i < n; i++) {
    char macbuf[18];
    macStr(snap[i].mac, macbuf, sizeof(macbuf));
    const uint32_t age = now - snap[i].lastSeenMs;
    snprintf(tmp, sizeof(tmp),
             "%s{\"mac\":\"%s\",\"rssi\":%d,\"avg\":%.1f,\"distance_m\":%.2f,"
             "\"zone\":\"%s\",\"last_seen_ms\":%lu,\"ch\":%u,\"kind\":\"%s\","
             "\"rand\":%s}",
             i ? "," : "", macbuf, (int)snap[i].rssi, snap[i].avgRssi,
             rssiToDistance(snap[i].avgRssi), zoneName(snap[i].avgRssi),
             (unsigned long)age, (unsigned)snap[i].channel, kindName(snap[i].kind),
             macRandomized(snap[i].mac) ? "true" : "false");
    app(tmp);
  }
  app("]}");
  gJson[off] = 0;
  return off;
}

// ---------------------------------------------------------------------------
// Channel hop — only while nobody is on the SoftAP, so the dashboard stays up.
// ---------------------------------------------------------------------------
static void serviceHop() {
  if (!gHopEnable) return;
  if (WiFi.softAPgetStationNum() > 0) return;  // freeze so the dashboard client stays associated
  const uint32_t now = millis();
  // Linger on the home AP channel so phones can find SSID root while hopping.
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
    // Quiet phosphor idle so you know root is alive.
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

// ---------------------------------------------------------------------------
// Wi-Fi + web
// ---------------------------------------------------------------------------
static void startRadio() {
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);

  bool ok;
  if (AP_PASS && strlen(AP_PASS) >= 8) {
    ok = WiFi.softAP(AP_SSID, AP_PASS, AP_CHANNEL, 0, 4);
  } else {
    ok = WiFi.softAP(AP_SSID, nullptr, AP_CHANNEL, 0, 4);
  }

  delay(150);
  gChannel = AP_CHANNEL;

  WiFi.macAddress(gSelfMac);
  WiFi.softAPmacAddress(gApMac);

  wifi_promiscuous_filter_t filter = {};
  filter.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA;
  esp_wifi_set_promiscuous_filter(&filter);
  esp_wifi_set_promiscuous_rx_cb(&snifferCb);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(AP_CHANNEL, WIFI_SECOND_CHAN_NONE);

  Serial.println(ok ? "root: SoftAP up" : "root: SoftAP failed");
  Serial.printf("root: join '%s'  then open http://%s\n", AP_SSID, WiFi.softAPIP().toString().c_str());

  char m1[18], m2[18];
  macStr(gSelfMac, m1, sizeof(m1));
  macStr(gApMac, m2, sizeof(m2));
  Serial.printf("root: self MAC %s  AP MAC %s\n", m1, m2);
  Serial.println("root: promiscuous listen armed — waiting for pings");
}

static void startWeb() {
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Cache-Control", "no-store");

  gServer.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send_P(200, "text/html", DASHBOARD_HTML);
  });

  gServer.on("/api/devices", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (xSemaphoreTake(gJsonMux, pdMS_TO_TICKS(80)) != pdTRUE) {
      req->send(503, "application/json", "{\"error\":\"busy\"}");
      return;
    }
    buildJson();
    const String body(gJson);
    xSemaphoreGive(gJsonMux);
    req->send(200, "application/json", body);
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
    req->send(404, "text/plain", "root: not found");
  });

  gServer.begin();
}

// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println();
  Serial.println("========================================");
  Serial.println("  root  — passive Wi-Fi echolocation");
  Serial.println("========================================");

  memset(gDev, 0, sizeof(gDev));
  gMux = xSemaphoreCreateMutex();
  gJsonMux = xSemaphoreCreateMutex();
  gQueue = xQueueCreate(QUEUE_LEN, sizeof(SniffHit));
  if (!gMux || !gJsonMux || !gQueue) {
    Serial.println("root: failed to allocate queue/mutex");
    return;
  }

  ledWrite(0, 12, 4);
  startRadio();
  startWeb();
}

void loop() {
  SniffHit hit;
  while (xQueueReceive(gQueue, &hit, 0) == pdTRUE) {
    ingestHit(hit);
  }
  pruneStale();
  serviceHop();
  serviceLed();
  delay(1);
}
