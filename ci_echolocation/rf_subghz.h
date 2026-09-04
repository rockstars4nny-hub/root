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
  char label[48];
  char detail[64];   // hex / mode snippet for UI
  SubGhzDetect detect;
};

#ifndef ROOT_SUBGHZ_RAW_BYTES
#define ROOT_SUBGHZ_RAW_BYTES 128
#endif
#ifndef ROOT_SUBGHZ_RAW_CAP
#define ROOT_SUBGHZ_RAW_CAP 2048
#endif
/** GDO0 async bit sample period (µs) — packed MSB-first into raw bytes */
#ifndef ROOT_SUBGHZ_BIT_US
#define ROOT_SUBGHZ_BIT_US 25
#endif

struct SubGhzRawPacket {
  uint32_t timestampMs;
  float frequencyMhz;
  int8_t rssi;
  uint8_t lqi;
  uint8_t length;
  uint8_t gpsValid;
  double lat;
  double lon;
  uint8_t data[ROOT_SUBGHZ_RAW_BYTES];
};

void subghzInit();
void subghzService(uint32_t nowMs);
bool subghzPopHit(SubGhzHit* out);
bool subghzPopActivity(SubGhzHit* out);
bool subghzReady();
void subghzStatusJson(char* out, size_t n);

/** OmniScan controls */
void subghzSetEnabled(bool on);
bool subghzEnabled();
void subghzSetHopping(bool hop);
bool subghzHopping();
bool subghzSetFrequencyMhz(float mhz);
float subghzCurrentMhz();
uint32_t subghzPacketCount();
/** Set OOK dwell (ms) for one band (315/433/868/915) or all if bandMhz≈0. Clamped 100–60000. */
bool subghzSetOokDwellMs(float bandMhz, uint16_t ms);
/** Format current per-band OOK/FSK dwell table into out. */
bool subghzFormatDwell(char* out, size_t n);

/** Raw hex buffer (CC1101 readData when packets land) */
uint32_t subghzRawCount();
uint32_t subghzRawTotal();
void subghzRawClear();
bool subghzRawGet(uint32_t newestIndex, SubGhzRawPacket* out);  // 0 = newest
size_t subghzRawBytesUsed();
/** Format last N (1-50) raw packets; args: "", "last", "5", "filter 433.92", "clear",
 *  "save", "decode HEX", "analyze", "id [mhz] [hex]", "protocols",
 *  "formats [N]", "all [N]" — every payload view (HEX/ASCII/BITS/BASE64/DEC/…) */
bool subghzRawCommand(const char* args, char* out, size_t n);
bool subghzListRecent(char* out, size_t n);
