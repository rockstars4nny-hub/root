#include "rf_subghz_proto.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>

// ---------------------------------------------------------------------------
// Knowledge base
// ---------------------------------------------------------------------------

struct ProtoKb {
  SgProtoId id;
  const char* name;
  const char* brands;
  const char* family;
  const char* bands;
  const char* notes;
  uint8_t typBitsLo;
  uint8_t typBitsHi;
};

static const ProtoKb KNOWLEDGE[] = {
    {SG_PROTO_PRINCETON, "Princeton",
     "PT2262 · EV1527 · SC2262 · DIY outlets · Amazon fobs · cheap alarms", "fixed", "315/433",
     "24-bit PWM. Same key every press.", 20, 28},
    {SG_PROTO_HOLTEK, "Holtek", "HT12E remotes · DIY · toys · older alarms", "fixed", "315/433",
     "Holtek up to ~40-bit.", 32, 48},
    {SG_PROTO_HOLTEK_HT12X, "Holtek HT12X", "HT12X 12-bit address remotes", "fixed", "315/433",
     "HT12X 12-bit.", 10, 14},
    {SG_PROTO_CAME, "Came", "Came TOP · TAM · Space · EU sliding gates", "fixed", "433",
     "Came 12/18/24-bit fixed.", 10, 26},
    {SG_PROTO_CAME_ATOMO, "Came Atomo", "Came Atomo rolling gate remotes", "rolling", "433",
     "Came Atomo ~62-bit rolling.", 56, 68},
    {SG_PROTO_CAME_TWEE, "Came TWEE", "Came TWEE remotes", "rolling", "433", "Came TWEE ~54-bit.",
     48, 60},
    {SG_PROTO_NICE_FLO, "Nice Flo", "Nice Flo · Flor · One · Era gates", "fixed", "433/868",
     "Nice Flo 12/24-bit fixed.", 10, 26},
    {SG_PROTO_NICE_FLOR_S, "Nice Flor-S", "Nice Flor-S rolling", "rolling", "433/868",
     "Flor-S ~52-bit rolling.", 48, 56},
    {SG_PROTO_LINEAR, "Linear", "Linear Multicode · older US garage", "fixed", "300/310/390",
     "Linear 10-bit.", 8, 12},
    {SG_PROTO_LINEAR_DELTA3, "Linear Delta3", "Linear Delta-3 garage/gate", "fixed", "310/390",
     "Delta-3 ~8-bit.", 6, 10},
    {SG_PROTO_MEGACODE, "MegaCode", "Linear MegaCode US garage", "fixed", "318/390",
     "MegaCode ~24-bit.", 20, 28},
    {SG_PROTO_GATE_TX, "GateTX", "Generic GateTX 24-bit clones", "fixed", "433", "GateTX 24-bit.",
     20, 28},
    {SG_PROTO_NERO_SKETCH, "Nero Sketch", "Nero Sketch gate remotes", "fixed", "433",
     "Nero Sketch ~40-bit.", 36, 44},
    {SG_PROTO_NERO_RADIO, "Nero Radio", "Nero Radio gate remotes", "fixed", "433",
     "Nero Radio ~56-bit.", 48, 60},
    {SG_PROTO_ANSONIC, "Ansonic", "Ansonic remotes", "fixed", "433", "Ansonic 12-bit.", 10, 14},
    {SG_PROTO_SMC5326, "SMC5326", "SMC5326 / PT2260 family", "fixed", "315/433", "SMC5326 25-bit.",
     22, 28},
    {SG_PROTO_CLEMSA, "Clemsa", "Clemsa gate remotes", "fixed", "433", "Clemsa 18-bit.", 16, 20},
    {SG_PROTO_BETT, "BETT", "BETT remotes", "fixed", "433", "BETT 18-bit.", 16, 20},
    {SG_PROTO_INTERTECHNO, "Intertechno", "Intertechno V3 home automation", "fixed", "433",
     "IT V3 32-bit.", 28, 36},
    {SG_PROTO_MAGELLAN, "Magellan", "Magellan security sensors/remotes", "fixed", "433",
     "Magellan 32-bit.", 28, 36},
    {SG_PROTO_HONEYWELL_WDB, "Honeywell", "Honeywell wireless doorbells Series 3/5/9", "fixed",
     "434.5", "Honeywell WDB ~48-bit.", 40, 56},
    {SG_PROTO_LEGRAND, "Legrand", "Legrand lighting remotes", "fixed", "433", "Legrand frames.", 24,
     48},
    {SG_PROTO_DICKERT, "Dickert", "Dickert MAHS gate", "fixed", "433", "Dickert MAHS.", 20, 40},
    {SG_PROTO_MASTERCODE, "Mastercode", "Mastercode remotes", "fixed", "433", "Mastercode 36-bit.",
     32, 40},
    {SG_PROTO_POWER_SMART, "PowerSmart", "Power Smart outlets/remotes", "fixed", "433",
     "PowerSmart ~64-bit.", 56, 72},
    {SG_PROTO_PHOENIX_V2, "Phoenix V2", "Phoenix V2 remotes", "rolling", "433",
     "Phoenix V2 ~52-bit.", 48, 56},
    {SG_PROTO_DOITRAND, "Doitrand", "Doitrand remotes", "fixed", "433", "Doitrand 37-bit.", 32, 40},
    {SG_PROTO_DOOYA, "Dooya", "Dooya / Motics blinds", "rolling", "433", "Dooya 40-bit.", 36, 44},
    {SG_PROTO_GANGQI, "GangQi", "GangQi remotes", "fixed", "433", "GangQi frames.", 20, 40},
    {SG_PROTO_HOLLARM, "Hollarm", "Hollarm alarms", "fixed", "433", "Hollarm frames.", 20, 40},
    {SG_PROTO_HAY21, "Hay21", "Hay21 remotes", "fixed", "433", "Hay21 frames.", 16, 32},
    {SG_PROTO_FERON, "Feron", "Feron lighting", "fixed", "433", "Feron frames.", 16, 40},
    {SG_PROTO_ROGER, "Roger", "Roger Technology gates", "fixed", "433", "Roger frames.", 20, 48},
    {SG_PROTO_REVERS_RB2, "Revers RB2", "Revers RB2", "fixed", "433", "Revers RB2.", 20, 40},
    {SG_PROTO_KEELOQ, "KeeLoq/HCS",
     "Microchip HCS · Chrysler/Jeep · VW older · aftermarket alarms · clone car fobs", "rolling",
     "315/433/868", "~64-bit encrypted rolling (serial needs mfg key).", 60, 72},
    {SG_PROTO_STAR_LINE, "StarLine", "StarLine car alarms", "rolling", "433/868",
     "StarLine ~64-bit.", 60, 72},
    {SG_PROTO_FAAC_SLH, "FAAC SLH", "FAAC SLH gate rolling", "rolling", "433/868",
     "FAAC SLH 64-bit.", 60, 72},
    {SG_PROTO_SOMFY_TELIS, "Somfy Telis", "Somfy Telis · Situo · RTS blinds", "rolling", "433.42",
     "Somfy RTS 56-bit.", 48, 64},
    {SG_PROTO_SOMFY_KEYTIS, "Somfy Keytis", "Somfy Keytis", "rolling", "433.42",
     "Somfy Keytis 80-bit.", 72, 88},
    {SG_PROTO_SECPLUS_V1, "Sec+ 1.0", "Chamberlain · LiftMaster · Craftsman Security+ 1.0",
     "rolling", "310/315/390", "Security+ v1 ~21-bit.", 18, 28},
    {SG_PROTO_SECPLUS_V2, "Sec+ 2.0", "Chamberlain · LiftMaster Security+ 2.0", "rolling",
     "310/315/390", "Security+ v2 ~62-bit.", 56, 68},
    {SG_PROTO_CHAMB_CODE, "Chamberlain", "Chamberlain classic 7/8/9-bit", "fixed", "390",
     "Classic Chamberlain short codes.", 6, 12},
    {SG_PROTO_HORMANN, "Hormann", "Hormann HSM / BiSecur (ID)", "rolling", "868",
     "Hormann HSM ~44-bit.", 40, 48},
    {SG_PROTO_MARANTEC, "Marantec", "Marantec garage/gate", "rolling", "433/868",
     "Marantec ~49-bit.", 44, 56},
    {SG_PROTO_MARANTEC24, "Marantec24", "Marantec 24-bit family", "fixed", "433", "Marantec24.", 20,
     28},
    {SG_PROTO_ALUTECH, "Alutech", "Alutech AT-4N", "rolling", "433", "Alutech 72-bit.", 64, 80},
    {SG_PROTO_KINGGATES, "KingGates", "KingGates Stylo 4K", "rolling", "433", "KingGates ~89-bit.",
     80, 96},
    {SG_PROTO_IDO, "IDO", "IDO remotes", "rolling", "433", "IDO frames.", 40, 72},
    {SG_PROTO_KIA, "KIA", "KIA / Hyundai older RKE", "rolling", "315/433", "KIA ~61-bit.", 56, 68},
    {SG_PROTO_SCHER_KHAN, "Scher-Khan", "Scher-Khan car alarms", "rolling", "433/868",
     "Scher-Khan frames.", 48, 72},
    {SG_PROTO_AUTO_RKE, "Auto RKE",
     "OEM car fobs · Honda/Toyota/Ford/GM/Nissan · aftermarket RKE", "auto", "315/433/868",
     "Automotive RKE — FSK or KeeLoq-class OOK.", 16, 128},
    {SG_PROTO_TPMS, "TPMS", "Schrader · Continental · Pacific · OEM tire sensors", "sensor",
     "315/433/868", "Short FSK bursts from wheels.", 8, 80},
    {SG_PROTO_WEATHER, "Weather", "Oregon · Acurite · Ambient · La Crosse", "sensor", "433/915",
     "Periodic weather telemetry.", 24, 128},
    {SG_PROTO_SENSOR, "Sensor", "Door/window · PIR · water leak · 433 IoT", "sensor", "315/433/915",
     "Periodic sensor frames.", 16, 128},
    {SG_PROTO_FSK_DATA, "FSK data", "Unknown FSK · industrial · some auto/TPMS", "unknown",
     "315-915", "Structured FSK without named match.", 8, 255},
    {SG_PROTO_BIN_RAW, "BinRAW", "Structured binary (unnamed)", "unknown", "any",
     "Edges present; no named decoder.", 8, 255},
    {SG_PROTO_GATE, "Gate remote", "FAAC · BFT · Beninca · generic EU gate", "fixed", "433/868",
     "EU gate when Came/Nice uncertain.", 8, 28},
    {SG_PROTO_NOISE, "Noise", "-", "unknown", "-", "All-zero / all-one / no edges.", 0, 0},
    {SG_PROTO_UNKNOWN, "Unknown", "unclassified emitter", "unknown", "-",
     "No fingerprint matched.", 0, 0},
};

static const ProtoKb* kbFind(SgProtoId id) {
  for (size_t i = 0; i < sizeof(KNOWLEDGE) / sizeof(KNOWLEDGE[0]); i++) {
    if (KNOWLEDGE[i].id == id) return &KNOWLEDGE[i];
  }
  return &KNOWLEDGE[sizeof(KNOWLEDGE) / sizeof(KNOWLEDGE[0]) - 1];
}

const char* sgProtoName(SgProtoId id) { return kbFind(id)->name; }

// ---------------------------------------------------------------------------
// Run-length bitstream
// ---------------------------------------------------------------------------

struct Run {
  uint8_t level;
  uint16_t n;
};

static uint16_t medianU16(uint16_t* v, uint8_t n) {
  if (!n) return 0;
  for (uint8_t i = 1; i < n; i++) {
    uint16_t t = v[i];
    int j = i - 1;
    while (j >= 0 && v[j] > t) {
      v[j + 1] = v[j];
      j--;
    }
    v[j + 1] = t;
  }
  return v[n / 2];
}

static uint8_t buildRuns(const uint8_t* data, uint8_t len, Run* runs, uint8_t maxRuns) {
  if (!data || !len || !runs || !maxRuns) return 0;
  uint8_t rc = 0;
  uint8_t cur = (data[0] >> 7) & 1;
  uint16_t cnt = 0;
  for (uint8_t bi = 0; bi < len; bi++) {
    for (int b = 7; b >= 0; b--) {
      uint8_t bit = (data[bi] >> b) & 1;
      if (bit == cur) {
        if (cnt < 0xfffe) cnt++;
      } else {
        if (rc < maxRuns) {
          runs[rc].level = cur;
          runs[rc].n = cnt ? cnt : 1;
          rc++;
        }
        cur = bit;
        cnt = 1;
      }
    }
  }
  if (rc < maxRuns && cnt) {
    runs[rc].level = cur;
    runs[rc].n = cnt;
    rc++;
  }
  return rc;
}

static uint16_t estimateTeSamples(const Run* runs, uint8_t rc) {
  uint16_t shorts[64];
  uint8_t ns = 0;
  for (uint8_t i = 0; i < rc && ns < 64; i++) {
    if (runs[i].level == 1 && runs[i].n >= 2 && runs[i].n <= 24) shorts[ns++] = runs[i].n;
  }
  if (ns < 3) {
    ns = 0;
    for (uint8_t i = 0; i < rc && ns < 64; i++) {
      if (runs[i].level == 1 && runs[i].n >= 1 && runs[i].n <= 40) shorts[ns++] = runs[i].n;
    }
  }
  uint16_t med = medianU16(shorts, ns);
  return med ? med : 7;
}

static uint8_t dutyPercent(const Run* runs, uint8_t rc) {
  uint32_t hi = 0, tot = 0;
  for (uint8_t i = 0; i < rc; i++) {
    tot += runs[i].n;
    if (runs[i].level) hi += runs[i].n;
  }
  return tot ? (uint8_t)((hi * 100) / tot) : 0;
}

static bool mostlyConstant(const uint8_t* data, uint8_t len) {
  if (!len) return true;
  uint8_t z = 0, f = 0;
  for (uint8_t i = 0; i < len; i++) {
    if (data[i] == 0x00) z++;
    else if (data[i] == 0xff) f++;
  }
  return (z * 10 >= len * 9) || (f * 10 >= len * 9);
}

static void keyToHex(const uint8_t* key, uint8_t keyBits, char* out, size_t n) {
  if (!out || !n) return;
  out[0] = 0;
  if (!keyBits) return;
  uint8_t nbytes = (keyBits + 7) / 8;
  size_t u = 0;
  for (uint8_t i = 0; i < nbytes && u + 3 < n; i++) {
    u += (size_t)snprintf(out + u, n - u, "%02X", key[i]);
  }
}

static void pushBit(uint8_t* key, uint8_t* keyBits, uint8_t maxBits, uint8_t bit) {
  if (*keyBits >= maxBits) return;
  uint8_t i = (*keyBits) / 8;
  uint8_t b = 7 - ((*keyBits) % 8);
  if (bit) key[i] |= (uint8_t)(1u << b);
  (*keyBits)++;
}

// ---------------------------------------------------------------------------
// Decoders — PWM (Princeton-style 0=1H3L, 1=3H1L) and Came-style (0=short,1=long)
// ---------------------------------------------------------------------------

struct DecodeTry {
  SgProtoId id;
  int score;
  uint16_t teUs;
  uint8_t keyBits;
  uint8_t key[12];
  bool ok;
};

static bool near(uint16_t v, uint16_t target, uint16_t tol) {
  if (v + tol < target) return false;
  if (target + tol < v) return false;
  return true;
}

/** Princeton / EV1527 / GateTX / SMC-like PWM */
static DecodeTry tryPwmPrinceton(const Run* runs, uint8_t rc, uint16_t teS, SgProtoId id,
                                 uint8_t expectLo, uint8_t expectHi) {
  DecodeTry t = {};
  t.id = id;
  if (teS < 2) return t;
  uint8_t bits = 0;
  uint8_t key[12] = {};
  int good = 0, bad = 0;
  for (uint8_t i = 0; i + 1 < rc; i++) {
    if (runs[i].level != 1 || runs[i + 1].level != 0) continue;
    uint16_t hi = runs[i].n;
    uint16_t lo = runs[i + 1].n;
    if (lo >= teS * 18) continue;  // sync gap
    bool zero = near(hi, teS, teS / 2 + 1) && near(lo, teS * 3, teS + 1);
    bool one = near(hi, teS * 3, teS + 1) && near(lo, teS, teS / 2 + 1);
    if (zero || one) {
      pushBit(key, &bits, 96, one ? 1 : 0);
      good++;
    } else if (hi >= teS / 2 && lo >= teS / 2) {
      bad++;
    }
  }
  if (bits < expectLo || bits > expectHi + 8) return t;
  if (good < 8) return t;
  int score = 40 + (good * 50) / (good + bad + 1);
  if (bits >= expectLo && bits <= expectHi) score += 15;
  // Prefer exact common lengths
  if (id == SG_PROTO_PRINCETON && bits == 24) score += 20;
  if (id == SG_PROTO_GATE_TX && bits == 24) score += 12;
  if (id == SG_PROTO_SMC5326 && (bits == 24 || bits == 25)) score += 10;
  t.ok = true;
  t.score = score;
  t.teUs = (uint16_t)(teS * ROOT_SUBGHZ_BIT_US);
  t.keyBits = bits > 96 ? 96 : bits;
  memcpy(t.key, key, sizeof t.key);
  return t;
}

/** Came / Nice Flo / Ansonic — short high = 0, long high = 1 */
static DecodeTry tryCameStyle(const Run* runs, uint8_t rc, uint16_t teS, SgProtoId id,
                              uint8_t expectLo, uint8_t expectHi) {
  DecodeTry t = {};
  t.id = id;
  if (teS < 2) return t;
  uint8_t bits = 0;
  uint8_t key[12] = {};
  int good = 0;
  for (uint8_t i = 0; i < rc; i++) {
    if (runs[i].level != 1) continue;
    uint16_t hi = runs[i].n;
    if (hi >= teS * 10) continue;
    bool zero = near(hi, teS, teS / 2 + 1);
    bool one = near(hi, teS * 2, teS) || near(hi, teS * 3, teS + 1);
    if (zero || one) {
      pushBit(key, &bits, 96, one ? 1 : 0);
      good++;
    }
  }
  if (bits < expectLo || bits > expectHi + 6) return t;
  if (good < 8) return t;
  int score = 35 + good;
  if (bits == 12) score += 20;
  if (bits == 18 || bits == 24) score += 12;
  if (id == SG_PROTO_CAME && bits == 12) score += 10;
  if (id == SG_PROTO_NICE_FLO && bits == 12) score += 8;
  t.ok = true;
  t.score = score > 100 ? 100 : score;
  t.teUs = (uint16_t)(teS * ROOT_SUBGHZ_BIT_US);
  t.keyBits = bits;
  memcpy(t.key, key, sizeof t.key);
  return t;
}

/** Linear 10-bit / Delta-3 — short frames, US bands */
static DecodeTry tryLinearShort(const Run* runs, uint8_t rc, uint16_t teS, SgProtoId id,
                                uint8_t expectBits) {
  DecodeTry t = tryCameStyle(runs, rc, teS, id, expectBits - 2, expectBits + 2);
  if (!t.ok) return t;
  if (t.keyBits == expectBits) t.score += 15;
  return t;
}

/** KeeLoq / long rolling — collect PWM bits until ~64 */
static DecodeTry tryLongPwm(const Run* runs, uint8_t rc, uint16_t teS, SgProtoId id,
                            uint8_t lo, uint8_t hi) {
  DecodeTry t = tryPwmPrinceton(runs, rc, teS, id, lo, hi);
  if (!t.ok) {
    // also try came-style long
    t = tryCameStyle(runs, rc, teS, id, lo, hi);
  }
  if (!t.ok) return t;
  if (t.keyBits >= lo && t.keyBits <= hi) t.score += 10;
  if (id == SG_PROTO_KEELOQ && t.keyBits >= 64 && t.keyBits <= 66) t.score += 15;
  return t;
}

/** Manchester-ish (Somfy): duty ~50%, TE longer */
static DecodeTry tryManchester(const Run* runs, uint8_t rc, uint16_t teS, SgProtoId id,
                               uint8_t lo, uint8_t hi) {
  DecodeTry t = {};
  t.id = id;
  if (teS < 2 || rc < 20) return t;
  uint8_t bits = 0;
  uint8_t key[12] = {};
  // Mid-bit transitions: pair equal runs ~1 TE
  for (uint8_t i = 0; i + 1 < rc && bits < 96; i += 1) {
    if (runs[i].n > teS * 4) continue;
    if (near(runs[i].n, teS, teS / 2 + 1) && near(runs[i + 1].n, teS, teS / 2 + 1)) {
      // 01 -> 0, 10 -> 1 (IEEE Manchester)
      uint8_t bit = (runs[i].level == 0 && runs[i + 1].level == 1) ? 0 : 1;
      if (runs[i].level == 1 && runs[i + 1].level == 0) bit = 1;
      if (runs[i].level == 0 && runs[i + 1].level == 1) bit = 0;
      pushBit(key, &bits, 96, bit);
      i++;  // consumed pair
    }
  }
  if (bits < lo || bits > hi + 10) return t;
  t.ok = true;
  t.score = 40 + (bits >= lo && bits <= hi ? 20 : 0);
  if (id == SG_PROTO_SOMFY_TELIS && bits >= 48 && bits <= 64) t.score += 15;
  t.teUs = (uint16_t)(teS * ROOT_SUBGHZ_BIT_US);
  t.keyBits = bits;
  memcpy(t.key, key, sizeof t.key);
  return t;
}

// ---------------------------------------------------------------------------
// Fill result + classify
// ---------------------------------------------------------------------------

static void fillResult(SgProtoResult* out, const DecodeTry* best, float mhz, uint16_t teUs,
                       uint16_t estBits, int confFallback) {
  SgProtoId id = best && best->ok ? best->id : SG_PROTO_UNKNOWN;
  const ProtoKb* kb = kbFind(id);
  out->id = id;
  out->teUs = best && best->ok ? best->teUs : teUs;
  out->bits = best && best->ok ? best->keyBits : estBits;
  out->decoded = best && best->ok && best->keyBits > 0;
  out->keyBits = out->decoded ? best->keyBits : 0;
  memset(out->key, 0, sizeof out->key);
  if (out->decoded) memcpy(out->key, best->key, sizeof out->key);
  keyToHex(out->key, out->keyBits, out->keyHex, sizeof out->keyHex);
  out->name = kb->name;
  out->brands = kb->brands;
  out->family = kb->family;
  out->confidence =
      best && best->ok ? (uint8_t)(best->score > 100 ? 100 : best->score) : (uint8_t)confFallback;

  const char* band =
      mhz >= 900 ? "915" : mhz >= 800 ? "868" : mhz >= 400 ? "433" : mhz >= 300 ? "315" : "?";

  if (out->decoded && out->keyHex[0]) {
    snprintf(out->summary, sizeof out->summary, "%s %s %s", band, kb->name, out->keyHex);
    // keep summary ≤31
    out->summary[31] = 0;
  } else {
    snprintf(out->summary, sizeof out->summary, "%s %s", band, kb->name);
  }

  if (out->decoded && out->keyHex[0]) {
    snprintf(out->detail, sizeof out->detail, "%s %ub %s TE%u", kb->name, (unsigned)out->keyBits,
             out->keyHex, (unsigned)out->teUs);
  } else {
    snprintf(out->detail, sizeof out->detail, "%s · %s · ~%ub · TE%u · %u%%", kb->name, kb->family,
             (unsigned)out->bits, (unsigned)out->teUs, (unsigned)out->confidence);
  }
  out->detail[47] = 0;
}

static void consider(DecodeTry* best, const DecodeTry& cand) {
  if (!cand.ok) return;
  if (!best->ok || cand.score > best->score) *best = cand;
}

bool sgClassify(const uint8_t* data, uint8_t len, float mhz, bool ook, SgProtoResult* out) {
  if (!out) return false;
  memset(out, 0, sizeof(*out));
  DecodeTry none = {};
  fillResult(out, &none, mhz, 0, 0, 10);

  if (!data || !len) {
    fillResult(out, &none, mhz, 0, 0, 5);
    out->id = SG_PROTO_UNKNOWN;
    out->name = kbFind(SG_PROTO_UNKNOWN)->name;
    out->brands = kbFind(SG_PROTO_UNKNOWN)->brands;
    out->family = "unknown";
    snprintf(out->summary, sizeof out->summary, "%.0f unknown", mhz);
    return true;
  }
  if (mostlyConstant(data, len)) {
    DecodeTry n = {};
    n.ok = true;
    n.id = SG_PROTO_NOISE;
    n.score = 85;
    fillResult(out, &n, mhz, 0, 0, 85);
    return true;
  }

  Run runs[128];
  uint8_t rc = buildRuns(data, len, runs, 128);
  uint16_t teS = estimateTeSamples(runs, rc);
  uint16_t teUs = (uint16_t)(teS * ROOT_SUBGHZ_BIT_US);
  uint8_t duty = dutyPercent(runs, rc);
  uint16_t edges = rc > 1 ? (uint16_t)(rc - 1) : 0;
  uint16_t estBits = edges / 2;

  const bool b315 = (mhz >= 300.f && mhz < 350.f);
  const bool b433 = (mhz >= 400.f && mhz < 500.f);
  const bool b868 = (mhz >= 800.f && mhz < 900.f);
  const bool b915 = (mhz >= 900.f);
  const bool bUsGarage = b315 || (mhz >= 380.f && mhz < 400.f);

  DecodeTry best = {};

  if (!ook) {
    // FSK heuristics
    if ((b315 || b433 || b868) && edges >= 8) {
      DecodeTry t = {};
      t.ok = true;
      t.teUs = teUs;
      t.keyBits = 0;
      if (estBits <= 48 && duty > 15 && duty < 85) {
        t.id = SG_PROTO_TPMS;
        t.score = 50;
      } else {
        t.id = SG_PROTO_AUTO_RKE;
        t.score = 55;
      }
      // stash raw prefix as "key" for display
      uint8_t copy = len > 8 ? 8 : len;
      memcpy(t.key, data, copy);
      t.keyBits = (uint8_t)(copy * 8);
      best = t;
    } else if (edges >= 6) {
      DecodeTry t = {};
      t.ok = true;
      t.id = SG_PROTO_FSK_DATA;
      t.score = 40;
      t.teUs = teUs;
      best = t;
    }
    fillResult(out, best.ok ? &best : &none, mhz, teUs, estBits, 20);
    return true;
  }

  // --- OOK named decoders ---
  consider(&best, tryPwmPrinceton(runs, rc, teS, SG_PROTO_PRINCETON, 20, 28));
  consider(&best, tryPwmPrinceton(runs, rc, teS, SG_PROTO_GATE_TX, 20, 28));
  consider(&best, tryPwmPrinceton(runs, rc, teS, SG_PROTO_SMC5326, 22, 28));
  consider(&best, tryPwmPrinceton(runs, rc, teS, SG_PROTO_MEGACODE, 20, 28));
  consider(&best, tryPwmPrinceton(runs, rc, teS, SG_PROTO_MARANTEC24, 20, 28));

  consider(&best, tryCameStyle(runs, rc, teS, SG_PROTO_CAME, 10, 26));
  consider(&best, tryCameStyle(runs, rc, teS, SG_PROTO_NICE_FLO, 10, 26));
  consider(&best, tryCameStyle(runs, rc, teS, SG_PROTO_ANSONIC, 10, 14));
  consider(&best, tryCameStyle(runs, rc, teS, SG_PROTO_HOLTEK_HT12X, 10, 14));
  consider(&best, tryCameStyle(runs, rc, teS, SG_PROTO_CLEMSA, 16, 20));
  consider(&best, tryCameStyle(runs, rc, teS, SG_PROTO_BETT, 16, 20));

  if (bUsGarage) {
    consider(&best, tryLinearShort(runs, rc, teS, SG_PROTO_LINEAR, 10));
    consider(&best, tryLinearShort(runs, rc, teS, SG_PROTO_LINEAR_DELTA3, 8));
    consider(&best, tryLinearShort(runs, rc, teS, SG_PROTO_CHAMB_CODE, 9));
    consider(&best, tryLongPwm(runs, rc, teS, SG_PROTO_SECPLUS_V1, 18, 28));
    consider(&best, tryLongPwm(runs, rc, teS, SG_PROTO_SECPLUS_V2, 56, 68));
  }

  consider(&best, tryLongPwm(runs, rc, teS, SG_PROTO_HOLTEK, 32, 48));
  consider(&best, tryLongPwm(runs, rc, teS, SG_PROTO_KEELOQ, 60, 72));
  consider(&best, tryLongPwm(runs, rc, teS, SG_PROTO_STAR_LINE, 60, 72));
  consider(&best, tryLongPwm(runs, rc, teS, SG_PROTO_FAAC_SLH, 60, 72));
  consider(&best, tryLongPwm(runs, rc, teS, SG_PROTO_NICE_FLOR_S, 48, 56));
  consider(&best, tryLongPwm(runs, rc, teS, SG_PROTO_CAME_ATOMO, 56, 68));
  consider(&best, tryLongPwm(runs, rc, teS, SG_PROTO_CAME_TWEE, 48, 60));
  consider(&best, tryLongPwm(runs, rc, teS, SG_PROTO_NERO_SKETCH, 36, 44));
  consider(&best, tryLongPwm(runs, rc, teS, SG_PROTO_NERO_RADIO, 48, 60));
  consider(&best, tryLongPwm(runs, rc, teS, SG_PROTO_HORMANN, 40, 48));
  consider(&best, tryLongPwm(runs, rc, teS, SG_PROTO_MARANTEC, 44, 56));
  consider(&best, tryLongPwm(runs, rc, teS, SG_PROTO_PHOENIX_V2, 48, 56));
  consider(&best, tryLongPwm(runs, rc, teS, SG_PROTO_DOOYA, 36, 44));
  consider(&best, tryLongPwm(runs, rc, teS, SG_PROTO_DOITRAND, 32, 40));
  consider(&best, tryLongPwm(runs, rc, teS, SG_PROTO_MASTERCODE, 32, 40));
  consider(&best, tryLongPwm(runs, rc, teS, SG_PROTO_POWER_SMART, 56, 72));
  consider(&best, tryLongPwm(runs, rc, teS, SG_PROTO_INTERTECHNO, 28, 36));
  consider(&best, tryLongPwm(runs, rc, teS, SG_PROTO_MAGELLAN, 28, 36));
  consider(&best, tryLongPwm(runs, rc, teS, SG_PROTO_HONEYWELL_WDB, 40, 56));
  consider(&best, tryLongPwm(runs, rc, teS, SG_PROTO_ALUTECH, 64, 80));
  consider(&best, tryLongPwm(runs, rc, teS, SG_PROTO_KINGGATES, 80, 96));
  consider(&best, tryLongPwm(runs, rc, teS, SG_PROTO_KIA, 56, 68));
  consider(&best, tryLongPwm(runs, rc, teS, SG_PROTO_IDO, 40, 72));
  consider(&best, tryLongPwm(runs, rc, teS, SG_PROTO_SCHER_KHAN, 48, 72));
  consider(&best, tryLongPwm(runs, rc, teS, SG_PROTO_LEGRAND, 24, 48));
  consider(&best, tryLongPwm(runs, rc, teS, SG_PROTO_DICKERT, 20, 40));
  consider(&best, tryLongPwm(runs, rc, teS, SG_PROTO_GANGQI, 20, 40));
  consider(&best, tryLongPwm(runs, rc, teS, SG_PROTO_HOLLARM, 20, 40));
  consider(&best, tryLongPwm(runs, rc, teS, SG_PROTO_HAY21, 16, 32));
  consider(&best, tryLongPwm(runs, rc, teS, SG_PROTO_FERON, 16, 40));
  consider(&best, tryLongPwm(runs, rc, teS, SG_PROTO_ROGER, 20, 48));
  consider(&best, tryLongPwm(runs, rc, teS, SG_PROTO_REVERS_RB2, 20, 40));

  if (duty >= 40 && duty <= 62 && teUs >= 350) {
    consider(&best, tryManchester(runs, rc, teS, SG_PROTO_SOMFY_TELIS, 48, 64));
    consider(&best, tryManchester(runs, rc, teS, SG_PROTO_SOMFY_KEYTIS, 72, 88));
  }

  // Band-boost
  if (best.ok) {
    if (b433 && (best.id == SG_PROTO_CAME || best.id == SG_PROTO_NICE_FLO ||
                 best.id == SG_PROTO_PRINCETON))
      best.score += 5;
    if (bUsGarage && (best.id == SG_PROTO_LINEAR || best.id == SG_PROTO_MEGACODE ||
                      best.id == SG_PROTO_SECPLUS_V1 || best.id == SG_PROTO_SECPLUS_V2))
      best.score += 8;
    if (b868 && (best.id == SG_PROTO_HORMANN || best.id == SG_PROTO_FAAC_SLH)) best.score += 6;
  }

  // Fallbacks when structure exists but no named decode
  if (!best.ok && edges >= 12) {
    DecodeTry t = {};
    t.ok = true;
    t.teUs = teUs;
    t.keyBits = 0;
    if (estBits >= 48 && (b315 || b433 || b868)) {
      t.id = SG_PROTO_AUTO_RKE;
      t.score = 32;
    } else if ((b433 || b915) && duty < 35) {
      t.id = SG_PROTO_WEATHER;
      t.score = 30;
    } else if ((b433 || b868) && estBits <= 28) {
      t.id = SG_PROTO_GATE;
      t.score = 28;
    } else if (duty < 40) {
      t.id = SG_PROTO_SENSOR;
      t.score = 26;
    } else {
      t.id = SG_PROTO_BIN_RAW;
      t.score = 25;
    }
    // expose raw prefix
    uint8_t copy = len > 6 ? 6 : len;
    memcpy(t.key, data, copy);
    t.keyBits = (uint8_t)(copy * 8);
    best = t;
  }

  fillResult(out, best.ok ? &best : &none, mhz, teUs, estBits, 15);
  return true;
}

bool sgProtoCatalog(char* out, size_t n) {
  if (!out || n < 32) return false;
  size_t u = 0;
  auto ap = [&](const char* s) {
    size_t l = strlen(s);
    if (u + l + 1 >= n) l = n > u + 1 ? n - u - 1 : 0;
    if (l) {
      memcpy(out + u, s, l);
      u += l;
      out[u] = 0;
    }
  };
  auto apf = [&](const char* fmt, ...) {
    char buf[360];
    va_list a;
    va_start(a, fmt);
    vsnprintf(buf, sizeof buf, fmt, a);
    va_end(a);
    ap(buf);
  };

  ap("=== SUB-GHZ PROTOCOLS / FOB BRANDS ===\n");
  ap("Passive classify+decode from CC1101 OOK samples (25us).\n");
  ap("Fixed codes show Key=hex. Rolling codes show ciphertext only.\n");
  ap("KeeLoq/Sec+/Somfy serial decrypt needs manufacturer keys (not on kit).\n\n");

  const char* lastFam = "";
  for (size_t i = 0; i < sizeof(KNOWLEDGE) / sizeof(KNOWLEDGE[0]); i++) {
    const ProtoKb& k = KNOWLEDGE[i];
    if (k.id == SG_PROTO_UNKNOWN || k.id == SG_PROTO_NOISE) continue;
    if (strcmp(lastFam, k.family) != 0) {
      lastFam = k.family;
      apf("-- %s --\n", k.family);
    }
    apf("[%s] bits~%u-%u  bands=%s\n", k.name, (unsigned)k.typBitsLo, (unsigned)k.typBitsHi,
        k.bands);
    apf("  brands: %s\n", k.brands);
    apf("  %s\n\n", k.notes);
  }
  ap("Cmds: ./omni subghz raw id | decode <hex> | analyze\n");
  return true;
}

bool sgFormatIdentify(const uint8_t* data, uint8_t len, float mhz, bool ook, char* out,
                      size_t n) {
  if (!out || n < 32) return false;
  SgProtoResult r;
  sgClassify(data, len, mhz, ook, &r);
  const ProtoKb* kb = kbFind(r.id);
  if (r.decoded && r.keyHex[0]) {
    snprintf(out, n,
             "Protocol: %s (%u%%)\n"
             "Family:   %s\n"
             "Brands:   %s\n"
             "Key:      %s (%u bits)\n"
             "TE:       %u us\n"
             "Bands:    %s\n"
             "Notes:    %s\n",
             r.name, (unsigned)r.confidence, r.family, r.brands, r.keyHex, (unsigned)r.keyBits,
             (unsigned)r.teUs, kb->bands, kb->notes);
  } else {
    snprintf(out, n,
             "Protocol: %s (%u%%)\n"
             "Family:   %s\n"
             "Brands:   %s\n"
             "Estimate: ~%u bits · TE %u us\n"
             "Bands:    %s\n"
             "Notes:    %s\n",
             r.name, (unsigned)r.confidence, r.family, r.brands, (unsigned)r.bits,
             (unsigned)r.teUs, kb->bands, kb->notes);
  }
  return true;
}
