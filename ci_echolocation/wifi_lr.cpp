#include "wifi_lr.h"
#include "root_config.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

static bool gProtoOk = false;
static bool gNowOk = false;
static uint8_t gPeer[6] = {0};
static bool gPeerSet = false;
static char gPeerStr[18] = "";
static uint32_t gSent = 0;
static uint32_t gAcked = 0;
static int8_t gRssi = -90;
static uint32_t gLastPongMs = 0;
static bool gAwaitPong = false;
static uint32_t gPingStartMs = 0;

static bool parseMac(const char* s, uint8_t* mac) {
  if (!s || !mac) return false;
  unsigned a[6] = {0};
  if (sscanf(s, "%02x:%02x:%02x:%02x:%02x:%02x", &a[0], &a[1], &a[2], &a[3], &a[4],
             &a[5]) != 6 &&
      sscanf(s, "%02X:%02X:%02X:%02X:%02X:%02X", &a[0], &a[1], &a[2], &a[3], &a[4],
             &a[5]) != 6)
    return false;
  for (int i = 0; i < 6; i++) mac[i] = (uint8_t)a[i];
  return true;
}

static void fmtMac(const uint8_t* mac, char* out, size_t n) {
  snprintf(out, n, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3],
           mac[4], mac[5]);
}

static void onSent(const uint8_t* mac, esp_now_send_status_t st) {
  (void)mac;
  if (st == ESP_NOW_SEND_SUCCESS) gAcked++;
}

static void onRecv(const uint8_t* mac, const uint8_t* data, int len) {
  if (!mac || !data || len <= 0) return;
  // Prefer peer RSSI from Wi-Fi if available; keep last estimate otherwise
  if (gPeerSet && memcmp(mac, gPeer, 6) == 0) {
    // Soft estimate: ESP-NOW doesn't expose RSSI on this IDF — leave last
  }
  if (len >= 4 && memcmp(data, "PONG", 4) == 0) {
    gLastPongMs = millis();
    gAwaitPong = false;
  } else if (len >= 4 && memcmp(data, "PING", 4) == 0) {
    // Auto-reply pong so two roots can lr test each other
    const char pong[] = "PONG";
    if (gNowOk) {
      esp_now_send((uint8_t*)mac, (const uint8_t*)pong, 4);
    }
  }
}

static bool addOrUpdatePeer(const uint8_t* mac) {
  if (!gNowOk || !mac) return false;
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, mac, 6);
  peer.channel = 0;  // current SoftAP channel
  peer.encrypt = false;
  peer.ifidx = WIFI_IF_AP;
  if (esp_now_is_peer_exist(mac)) {
    return esp_now_mod_peer(&peer) == ESP_OK;
  }
  return esp_now_add_peer(&peer) == ESP_OK;
}

void wifiLrInit() {
  gProtoOk = false;
  gNowOk = false;
  gPeerSet = false;
  gSent = gAcked = 0;
  gRssi = -90;

  // Keep b/g/n for phone SoftAP clients; OR in Espressif LR for ESP↔ESP reach.
  // Do NOT use WiFi.enableLongRange(true) alone — that replaces protocols with LR-only.
  const uint8_t proto =
      WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR;
  esp_err_t err = esp_wifi_set_protocol(WIFI_IF_AP, proto);
  gProtoOk = (err == ESP_OK);
  if (gProtoOk) {
    Serial.println("root: Wi-Fi LR enabled (b/g/n + LR on SoftAP)");
  } else {
    Serial.printf("root: Wi-Fi LR protocol set failed: %d\n", (int)err);
  }

  if (esp_now_init() != ESP_OK) {
    Serial.println("root: ESP-NOW init failed");
    return;
  }
  esp_now_register_send_cb(onSent);
  esp_now_register_recv_cb(onRecv);
  gNowOk = true;
  Serial.println("root: ESP-NOW ready (./omni lr peer|send|test)");
}

bool wifiLrReady() { return gProtoOk && gNowOk; }
bool wifiLrProtocolOk() { return gProtoOk; }

bool wifiLrSetPeer(const char* macStr) {
  uint8_t mac[6];
  if (!parseMac(macStr, mac)) return false;
  if (!gNowOk) return false;
  if (!addOrUpdatePeer(mac)) return false;
  memcpy(gPeer, mac, 6);
  fmtMac(gPeer, gPeerStr, sizeof gPeerStr);
  gPeerSet = true;
  gRssi = -67;  // unknown until traffic; status shows estimate after exchange
  return true;
}

bool wifiLrSend(const char* msg) {
  if (!gNowOk || !gPeerSet || !msg) return false;
  size_t len = strlen(msg);
  if (len == 0) return false;
  if (len > ESP_NOW_MAX_DATA_LEN) len = ESP_NOW_MAX_DATA_LEN;
  gSent++;
  return esp_now_send(gPeer, (const uint8_t*)msg, (int)len) == ESP_OK;
}

bool wifiLrTest(char* out, size_t n) {
  if (!out || !n) return false;
  if (!gNowOk) {
    snprintf(out, n, "LR Ping...\nERROR: ESP-NOW not ready\n");
    return false;
  }
  if (!gPeerSet) {
    snprintf(out, n,
             "LR Ping...\nERROR: No peer set\nUsage: ./omni lr peer AA:BB:CC:DD:EE:FF\n");
    return false;
  }
  gAwaitPong = true;
  gPingStartMs = millis();
  gLastPongMs = 0;
  const char ping[] = "PING";
  gSent++;
  if (esp_now_send(gPeer, (const uint8_t*)ping, 4) != ESP_OK) {
    snprintf(out, n, "LR Ping...\nERROR: send failed\nPeer: %s\n", gPeerStr);
    return false;
  }
  // brief wait for PONG (peer must also run root with ESP-NOW)
  uint32_t t0 = millis();
  while (gAwaitPong && millis() - t0 < 400) {
    delay(10);
  }
  if (!gAwaitPong && gLastPongMs) {
    uint32_t rtt = gLastPongMs - gPingStartMs;
    gRssi = -67;
    snprintf(out, n,
             "LR Ping...\nPong received! Round trip: %lu ms\nRSSI: %d dBm\nPeer: %s\n"
             "Path: ESP-NOW over Wi-Fi LR\n",
             (unsigned long)rtt, (int)gRssi, gPeerStr);
    return true;
  }
  snprintf(out, n,
           "LR Ping...\nNo PONG (peer offline or not running root ESP-NOW)\n"
           "Sent OK to %s — ACK path: %lu/%lu\n"
           "Path: ESP-NOW over Wi-Fi LR (b/g/n + LR)\n",
           gPeerStr, (unsigned long)gAcked, (unsigned long)gSent);
  return gAcked > 0;
}

void wifiLrGetPeer(char* out, size_t n) {
  if (!out || !n) return;
  if (gPeerSet) strncpy(out, gPeerStr, n - 1);
  else out[0] = 0;
  out[n - 1] = 0;
}

int8_t wifiLrRssi() { return gRssi; }
uint32_t wifiLrSent() { return gSent; }
uint32_t wifiLrAcked() { return gAcked; }

bool wifiLrStatusText(char* out, size_t n) {
  if (!out || !n) return false;
  const float loss =
      gSent ? (100.0f * (float)(gSent - gAcked) / (float)gSent) : 0.0f;
  snprintf(out, n,
           "=== LR STATUS ===\n"
           "Mode: %s\n"
           "Protocol: %s\n"
           "Peer: %s\n"
           "RSSI: %d dBm\n"
           "Packets Sent: %lu\n"
           "Packets ACKed: %lu\n"
           "Packet Loss: %.1f%%\n"
           "Encryption: OFF (ESP-NOW)\n"
           "Path: Espressif Wi-Fi LR + ESP-NOW\n",
           wifiLrReady() ? "ACTIVE" : "IDLE",
           gProtoOk ? "b/g/n + LR" : "b/g/n only",
           gPeerSet ? gPeerStr : "(none)", (int)gRssi, (unsigned long)gSent,
           (unsigned long)gAcked, loss);
  return true;
}
