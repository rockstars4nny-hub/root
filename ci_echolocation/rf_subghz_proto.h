#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/** Passive Sub-GHz protocol classify + decode (CC1101 OOK bit samples).
 *  Coverage aligned with Flipper Zero Sub-GHz registry + sensors/TPMS/RKE. */

enum SgProtoId : uint8_t {
  SG_PROTO_UNKNOWN = 0,
  SG_PROTO_NOISE,
  // Fixed-code remotes / outlets
  SG_PROTO_PRINCETON,      // PT2262 / EV1527 / SC2262
  SG_PROTO_HOLTEK,         // Holtek HT12E 40-bit style
  SG_PROTO_HOLTEK_HT12X,   // HT12X 12-bit
  SG_PROTO_CAME,           // Came 12/18/24
  SG_PROTO_CAME_ATOMO,     // Came Atomo rolling
  SG_PROTO_CAME_TWEE,      // Came TWEE
  SG_PROTO_NICE_FLO,       // Nice Flo 12/24
  SG_PROTO_NICE_FLOR_S,    // Nice Flor-S rolling
  SG_PROTO_LINEAR,         // Linear 10-bit
  SG_PROTO_LINEAR_DELTA3,  // Linear Delta-3
  SG_PROTO_MEGACODE,       // Linear MegaCode 24
  SG_PROTO_GATE_TX,        // GateTX 24
  SG_PROTO_NERO_SKETCH,    // Nero Sketch
  SG_PROTO_NERO_RADIO,     // Nero Radio
  SG_PROTO_ANSONIC,        // Ansonic
  SG_PROTO_SMC5326,        // SMC5326
  SG_PROTO_CLEMSA,         // Clemsa
  SG_PROTO_BETT,           // BETT
  SG_PROTO_INTERTECHNO,    // Intertechno V3
  SG_PROTO_MAGELLAN,       // Magellan
  SG_PROTO_HONEYWELL_WDB,  // Honeywell doorbell
  SG_PROTO_LEGRAND,        // Legrand
  SG_PROTO_DICKERT,        // Dickert MAHS
  SG_PROTO_MASTERCODE,     // Mastercode
  SG_PROTO_POWER_SMART,    // Power Smart
  SG_PROTO_PHOENIX_V2,     // Phoenix V2
  SG_PROTO_DOITRAND,       // Doitrand
  SG_PROTO_DOOYA,          // Dooya blinds
  SG_PROTO_GANGQI,         // GangQi
  SG_PROTO_HOLLARM,        // Hollarm
  SG_PROTO_HAY21,          // Hay21
  SG_PROTO_FERON,          // Feron
  SG_PROTO_ROGER,          // Roger
  SG_PROTO_REVERS_RB2,     // Revers RB2
  // Rolling / auto / garage
  SG_PROTO_KEELOQ,         // Microchip HCS / KeeLoq
  SG_PROTO_STAR_LINE,      // StarLine
  SG_PROTO_FAAC_SLH,       // FAAC SLH
  SG_PROTO_SOMFY_TELIS,    // Somfy Telis RTS
  SG_PROTO_SOMFY_KEYTIS,   // Somfy Keytis
  SG_PROTO_SECPLUS_V1,     // Chamberlain Security+ 1.0
  SG_PROTO_SECPLUS_V2,     // Chamberlain Security+ 2.0
  SG_PROTO_CHAMB_CODE,     // Chamberlain classic
  SG_PROTO_HORMANN,        // Hörmann HSM
  SG_PROTO_MARANTEC,       // Marantec
  SG_PROTO_MARANTEC24,     // Marantec24
  SG_PROTO_ALUTECH,        // Alutech AT-4N
  SG_PROTO_KINGGATES,      // KingGates Stylo4k
  SG_PROTO_IDO,            // IDO
  SG_PROTO_KIA,            // KIA car
  SG_PROTO_SCHER_KHAN,     // Scher-Khan
  SG_PROTO_AUTO_RKE,       // generic automotive RKE
  // Sensors / telemetry
  SG_PROTO_TPMS,           // tire pressure
  SG_PROTO_WEATHER,        // weather stations (Oregon/Acurite-class)
  SG_PROTO_SENSOR,         // door/PIR/leak sensors
  SG_PROTO_FSK_DATA,       // structured FSK unknown
  SG_PROTO_BIN_RAW,        // binary raw (structured but unnamed)
  SG_PROTO_GATE,           // generic EU gate fallback
  SG_PROTO_COUNT
};

struct SgProtoResult {
  SgProtoId id;
  uint8_t confidence;   // 0–100
  uint16_t bits;        // estimated or decoded bit length
  uint16_t teUs;       // timing element µs
  bool decoded;         // true if keyBits extracted
  uint8_t keyBits;      // valid bits in key[]
  uint8_t key[12];      // decoded payload (MSB first)
  const char* name;
  const char* brands;
  const char* family;   // fixed|rolling|sensor|auto|unknown
  char keyHex[28];      // printable key
  char summary[32];     // device list label
  char detail[48];      // encrypt/detail line
};

#ifndef ROOT_SUBGHZ_BIT_US
#define ROOT_SUBGHZ_BIT_US 25
#endif

bool sgClassify(const uint8_t* data, uint8_t len, float mhz, bool ook,
                SgProtoResult* out);

bool sgProtoCatalog(char* out, size_t n);

bool sgFormatIdentify(const uint8_t* data, uint8_t len, float mhz, bool ook,
                      char* out, size_t n);

/** Short name for histogram / UI (never null). */
const char* sgProtoName(SgProtoId id);
