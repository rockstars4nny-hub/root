#pragma once
#include <stddef.h>
#include <stdint.h>

#ifndef ROOT_ENABLE_BLE
#define ROOT_ENABLE_BLE 1
#endif

#ifndef ROOT_BLE_SCAN_SEC
#define ROOT_BLE_SCAN_SEC 2
#endif
#ifndef ROOT_BLE_INTERVAL_MS
#define ROOT_BLE_INTERVAL_MS 8000
#endif
#ifndef ROOT_BLE_STALE_MS
#define ROOT_BLE_STALE_MS 90000
#endif

struct BleHit {
  uint8_t mac[6];
  int8_t rssi;
  char name[32];
};

void bleInit();
void bleSetEnabled(bool on);
bool bleEnabled();
bool bleReady();
void bleService(uint32_t nowMs);
bool blePopHit(BleHit* out);
uint32_t bleTrackedCount();
void bleGetMacStr(char* out, size_t n);
bool bleListText(char* out, size_t n, const char* nameFilter);
