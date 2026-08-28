#pragma once
#include <stdint.h>
#include <stddef.h>

enum SubGhzDetect : uint8_t {
  SG_NONE = 0,
  SG_BURST = 1,      // short transmission (remotes, TPMS, sensors)
  SG_CARRIER = 2,    // continuous / fixed emitter (repeaters, links)
  SG_PACKET = 3      // GDO0 demod activity
};

struct SubGhzHit {
  uint32_t freqKhz;
  int8_t rssi;
  int8_t noiseFloor;
  uint8_t mac[6];
  char band[8];
  char label[40];
  SubGhzDetect detect;
};

void subghzInit();
void subghzService(uint32_t nowMs);
bool subghzPopHit(SubGhzHit* out);
bool subghzPopActivity(SubGhzHit* out);
bool subghzReady();
void subghzStatusJson(char* out, size_t n);
