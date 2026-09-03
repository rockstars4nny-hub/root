#include "rf_ble.h"
#include "root_config.h"

#if ROOT_ENABLE_BLE

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#ifndef ROOT_BLE_HIT_CAP
#define ROOT_BLE_HIT_CAP 48
#endif

static BLEScan* gScan = nullptr;
static bool gReady = false;
static bool gEnabled = true;
static bool gScanBusy = false;
static uint32_t gLastScanMs = 0;
static uint8_t gSelfMac[6] = {0};
static char gSelfMacStr[18] = "";

static BleHit gHits[ROOT_BLE_HIT_CAP];
static uint8_t gHitHead = 0;
static uint8_t gHitTail = 0;
static uint8_t gHitCount = 0;

struct BleCache {
  uint8_t mac[6];
  int8_t rssi;
  char name[32];
  uint32_t lastMs;
  bool inUse;
};
static BleCache gCache[96];
static uint16_t gCacheUsed = 0;

static bool macEq(const uint8_t* a, const uint8_t* b) {
  return memcmp(a, b, 6) == 0;
}

static void pushHit(const uint8_t* mac, int8_t rssi, const char* name) {
  if (!mac) return;
  if (macEq(mac, gSelfMac)) return;  // never track our own BLE address
  BleHit& h = gHits[gHitHead];
  memcpy(h.mac, mac, 6);
  h.rssi = rssi;
  h.name[0] = 0;
  if (name && name[0]) {
    strncpy(h.name, name, sizeof h.name - 1);
    h.name[sizeof h.name - 1] = 0;
  }
  gHitHead = (uint8_t)((gHitHead + 1) % ROOT_BLE_HIT_CAP);
  if (gHitCount < ROOT_BLE_HIT_CAP) gHitCount++;
  else gHitTail = (uint8_t)((gHitTail + 1) % ROOT_BLE_HIT_CAP);

  // upsert display cache
  int freeSlot = -1;
  for (int i = 0; i < (int)(sizeof gCache / sizeof gCache[0]); i++) {
    if (gCache[i].inUse && macEq(gCache[i].mac, mac)) {
      gCache[i].rssi = rssi;
      gCache[i].lastMs = millis();
      if (name && name[0]) {
        strncpy(gCache[i].name, name, sizeof gCache[i].name - 1);
        gCache[i].name[sizeof gCache[i].name - 1] = 0;
      }
      return;
    }
    if (!gCache[i].inUse && freeSlot < 0) freeSlot = i;
  }
  if (freeSlot >= 0) {
    BleCache& c = gCache[freeSlot];
    memset(&c, 0, sizeof c);
    memcpy(c.mac, mac, 6);
    c.rssi = rssi;
    c.lastMs = millis();
    c.inUse = true;
    if (name && name[0]) {
      strncpy(c.name, name, sizeof c.name - 1);
    }
    gCacheUsed++;
  }
}

static void parseAddr(const char* s, uint8_t* mac) {
  memset(mac, 0, 6);
  if (!s) return;
  unsigned a[6] = {0};
  if (sscanf(s, "%02x:%02x:%02x:%02x:%02x:%02x",
             &a[0], &a[1], &a[2], &a[3], &a[4], &a[5]) == 6 ||
      sscanf(s, "%02X:%02X:%02X:%02X:%02X:%02X",
             &a[0], &a[1], &a[2], &a[3], &a[4], &a[5]) == 6) {
    for (int i = 0; i < 6; i++) mac[i] = (uint8_t)a[i];
  }
}

static const char* classifyName(const char* name) {
  if (!name || !name[0]) return "Unknown";
  char lower[40];
  size_t n = 0;
  for (; name[n] && n + 1 < sizeof lower; n++) lower[n] = (char)tolower((unsigned char)name[n]);
  lower[n] = 0;
  if (strstr(lower, "airpod") || strstr(lower, "buds") || strstr(lower, "headset") ||
      strstr(lower, "speaker") || strstr(lower, "bose") || strstr(lower, "sony"))
    return "Audio";
  if (strstr(lower, "watch") || strstr(lower, "fitbit") || strstr(lower, "band") ||
      strstr(lower, "garmin") || strstr(lower, "oura"))
    return "Wearable";
  if (strstr(lower, "iphone") || strstr(lower, "pixel") || strstr(lower, "galaxy") ||
      strstr(lower, "phone"))
    return "Phone";
  if (strstr(lower, "tv") || strstr(lower, "roku") || strstr(lower, "chromecast"))
    return "Media";
  if (strstr(lower, "tile") || strstr(lower, "airtag") || strstr(lower, "tracker"))
    return "Tracker";
  return "BLE";
}

class RootBleCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice d) override {
    uint8_t mac[6];
    parseAddr(d.getAddress().toString().c_str(), mac);
    if (mac[0] == 0 && mac[1] == 0 && mac[2] == 0 && mac[3] == 0 && mac[4] == 0 &&
        mac[5] == 0)
      return;
    const char* name = d.haveName() ? d.getName().c_str() : "";
    pushHit(mac, (int8_t)d.getRSSI(), name);
  }
};

static void onScanDone(BLEScanResults) {
  gScanBusy = false;
  if (gScan) gScan->clearResults();
}

static void startScanPass() {
  if (!gReady || !gEnabled || !gScan || gScanBusy) return;
  gScanBusy = true;
  gScan->clearResults();
  // async: returns immediately; callback clears busy
  const bool ok = gScan->start(ROOT_BLE_SCAN_SEC, onScanDone, false);
  if (!ok) gScanBusy = false;
}

void bleInit() {
  memset(gCache, 0, sizeof gCache);
  gCacheUsed = 0;
  gHitHead = gHitTail = gHitCount = 0;
  gScanBusy = false;
  gEnabled = true;

  BLEDevice::init("");
  BLEAddress addr = BLEDevice::getAddress();
  parseAddr(addr.toString().c_str(), gSelfMac);
  snprintf(gSelfMacStr, sizeof gSelfMacStr, "%02X:%02X:%02X:%02X:%02X:%02X",
           gSelfMac[0], gSelfMac[1], gSelfMac[2], gSelfMac[3], gSelfMac[4],
           gSelfMac[5]);

  gScan = BLEDevice::getScan();
  if (!gScan) {
    Serial.println("root: BLE scan init failed");
    gReady = false;
    return;
  }
  gScan->setAdvertisedDeviceCallbacks(new RootBleCallbacks(), true);
  gScan->setActiveScan(true);
  gScan->setInterval(160);  // 100 ms
  gScan->setWindow(120);    // 75 ms — leave airtime for SoftAP/Wi-Fi
  gReady = true;
  gLastScanMs = millis() - ROOT_BLE_INTERVAL_MS;
  Serial.printf("root: BLE scanner ready (MAC %s)\n", gSelfMacStr);
}

void bleSetEnabled(bool on) {
  gEnabled = on;
  if (!on && gScan && gScanBusy) {
    gScan->stop();
    gScanBusy = false;
  }
}

bool bleEnabled() { return gEnabled; }
bool bleReady() { return gReady; }

void bleService(uint32_t nowMs) {
  if (!gReady || !gEnabled) return;
  if (gScanBusy) return;
  if (nowMs - gLastScanMs < (uint32_t)ROOT_BLE_INTERVAL_MS) return;
  gLastScanMs = nowMs;
  startScanPass();
}

bool blePopHit(BleHit* out) {
  if (!out || gHitCount == 0) return false;
  *out = gHits[gHitTail];
  gHitTail = (uint8_t)((gHitTail + 1) % ROOT_BLE_HIT_CAP);
  gHitCount--;
  return true;
}

uint32_t bleTrackedCount() {
  uint32_t n = 0;
  const uint32_t now = millis();
  for (size_t i = 0; i < sizeof gCache / sizeof gCache[0]; i++) {
    if (!gCache[i].inUse) continue;
    if (now - gCache[i].lastMs > (uint32_t)ROOT_BLE_STALE_MS) {
      gCache[i].inUse = false;
      if (gCacheUsed) gCacheUsed--;
      continue;
    }
    n++;
  }
  return n;
}

void bleGetMacStr(char* out, size_t n) {
  if (!out || !n) return;
  strncpy(out, gSelfMacStr[0] ? gSelfMacStr : "", n - 1);
  out[n - 1] = 0;
}

bool bleGetMac(uint8_t out[6]) {
  if (!out) return false;
  static const uint8_t z[6] = {0};
  if (macEq(gSelfMac, z)) return false;
  memcpy(out, gSelfMac, 6);
  return true;
}

bool bleIsSelfMac(const uint8_t* mac) {
  if (!mac) return false;
  static const uint8_t z[6] = {0};
  if (macEq(gSelfMac, z)) return false;
  return macEq(mac, gSelfMac);
}

bool bleListText(char* out, size_t n, const char* nameFilter) {
  if (!out || !n) return false;
  size_t u = 0;
  auto ap = [&](const char* s) {
    size_t l = strlen(s);
    if (u + l + 1 < n) {
      memcpy(out + u, s, l);
      u += l;
      out[u] = 0;
    }
  };
  char line[128];
  const uint32_t now = millis();
  // collect matching indices sorted by RSSI (simple insert)
  int idx[96];
  int count = 0;
  for (int i = 0; i < (int)(sizeof gCache / sizeof gCache[0]); i++) {
    if (!gCache[i].inUse) continue;
    if (now - gCache[i].lastMs > (uint32_t)ROOT_BLE_STALE_MS) continue;
    if (nameFilter && nameFilter[0]) {
      if (!gCache[i].name[0]) continue;
      // case-insensitive substring
      char hay[40], needle[40];
      size_t hn = 0, nn = 0;
      for (; gCache[i].name[hn] && hn + 1 < sizeof hay; hn++)
        hay[hn] = (char)tolower((unsigned char)gCache[i].name[hn]);
      hay[hn] = 0;
      for (; nameFilter[nn] && nn + 1 < sizeof needle; nn++)
        needle[nn] = (char)tolower((unsigned char)nameFilter[nn]);
      needle[nn] = 0;
      if (!strstr(hay, needle)) continue;
    }
    idx[count++] = i;
  }
  for (int a = 0; a < count; a++) {
    for (int b = a + 1; b < count; b++) {
      if (gCache[idx[b]].rssi > gCache[idx[a]].rssi) {
        int t = idx[a];
        idx[a] = idx[b];
        idx[b] = t;
      }
    }
  }
  snprintf(line, sizeof line, "=== BLE DEVICES (%d) ===\n", count);
  ap(line);
  if (!gReady) ap("BLE radio not ready\n");
  else if (!gEnabled) ap("BLE scanning: OFF\n");
  const int show = count > 32 ? 32 : count;
  for (int i = 0; i < show; i++) {
    const BleCache& c = gCache[idx[i]];
    char mac[18];
    snprintf(mac, sizeof mac, "%02X:%02X:%02X:%02X:%02X:%02X", c.mac[0], c.mac[1],
             c.mac[2], c.mac[3], c.mac[4], c.mac[5]);
    const char* nm = c.name[0] ? c.name : "(no name)";
    snprintf(line, sizeof line, "[%d] %s %d dBm %s [%s]\n", i + 1, mac, (int)c.rssi, nm,
             classifyName(c.name));
    ap(line);
  }
  if (count > show) {
    snprintf(line, sizeof line, "... +%d more\n", count - show);
    ap(line);
  }
  return true;
}

#else  // !ROOT_ENABLE_BLE

void bleInit() {}
void bleSetEnabled(bool) {}
bool bleEnabled() { return false; }
bool bleReady() { return false; }
void bleService(uint32_t) {}
bool blePopHit(BleHit*) { return false; }
uint32_t bleTrackedCount() { return 0; }
void bleGetMacStr(char* out, size_t n) {
  if (out && n) out[0] = 0;
}
bool bleGetMac(uint8_t out[6]) {
  if (out) memset(out, 0, 6);
  return false;
}
bool bleIsSelfMac(const uint8_t*) { return false; }
bool bleListText(char* out, size_t n, const char*) {
  if (out && n) snprintf(out, n, "=== BLE DEVICES ===\nBLE disabled in build\n");
  return true;
}

#endif
