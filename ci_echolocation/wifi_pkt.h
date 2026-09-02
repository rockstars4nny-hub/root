#pragma once
#include <stdint.h>
#include <stddef.h>

struct WifiPktMeta {
  uint8_t dst[6];
  uint8_t src[6];
  uint8_t bssid[6];
  uint16_t seq;
  uint8_t subtype;
  uint8_t toDs;
  uint8_t fromDs;
  char vendor[24];
  char encrypt[16];
  bool hasDst;
  bool hasBssid;
};

// Parse 802.11 MAC header + basic IE hints from a promiscuous payload.
void wifiParseFrame(const uint8_t* p, uint16_t len, uint8_t ftype, uint8_t subtype, WifiPktMeta* out);
