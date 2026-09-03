#pragma once
#include <stddef.h>
#include <stdint.h>

/** Espressif 802.11 Long-Range (WIFI_PROTOCOL_LR) + ESP-NOW peer path */

void wifiLrInit();                 // ESP-NOW after SoftAP (keeps SoftAP b/g/n)
bool wifiLrEnableLongRange(bool on);  // optional LR phy — conflicts with BLE coex on S3
bool wifiLrReady();                // ESP-NOW up
bool wifiLrProtocolOk();           // AP has LR bit set with b/g/n
bool wifiLrSetPeer(const char* macStr);
bool wifiLrSend(const char* msg);
bool wifiLrTest(char* out, size_t n);
void wifiLrGetPeer(char* out, size_t n);
int8_t wifiLrRssi();
uint32_t wifiLrSent();
uint32_t wifiLrAcked();
bool wifiLrStatusText(char* out, size_t n);
