#include "sight_log.h"
#include "root_config.h"
#include "geo.h"
#include "root_gps.h"
#include "psram_alloc.h"
#include <unordered_map>
#include <esp_heap_caps.h>
#include <string.h>
#include <stdio.h>

#ifndef ROOT_SIGHTINGS_PER_DEV
#define ROOT_SIGHTINGS_PER_DEV 48
#endif

struct Ring {
  Sighting pts[ROOT_SIGHTINGS_PER_DEV];
  uint8_t head = 0;
  uint8_t count = 0;
};

static std::unordered_map<uint64_t, Ring*, std::hash<uint64_t>,
                           std::equal_to<uint64_t>,
                           PSRAMAllocator<std::pair<const uint64_t, Ring*>>>* gRings = nullptr;
static uint64_t macKey(const uint8_t* mac) {
  uint64_t k = 0;
  memcpy(&k, mac, 6);
  return k;
}

void sightInit() {
  if (!gRings) {
    gRings = new std::unordered_map<uint64_t, Ring*, std::hash<uint64_t>,
                                    std::equal_to<uint64_t>,
                                    PSRAMAllocator<std::pair<const uint64_t, Ring*>>>();
  }
}

static Ring* ringFor(const uint8_t* mac) {
  if (!gRings) sightInit();
  const uint64_t k = macKey(mac);
  auto it = gRings->find(k);
  if (it != gRings->end()) return it->second;
  Ring* r = (Ring*)heap_caps_malloc(sizeof(Ring), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!r) r = (Ring*)malloc(sizeof(Ring));
  if (!r) return nullptr;
  memset(r, 0, sizeof(Ring));
  (*gRings)[k] = r;
  return r;
}

void sightPush(const uint8_t* mac, uint32_t atMs, int8_t rssi, float distanceM) {
  Ring* r = ringFor(mac);
  if (!r) return;

  Sighting s = {};
  s.atMs = atMs;
  s.rssi = rssi;
  s.distanceM = distanceM;
  s.bearingDeg = macBearingDeg(mac);
  polarToLocal(s.bearingDeg, distanceM, s.eastM, s.northM);

  const GpsFix gps = gpsGet();
  if (gps.valid) {
    s.scannerLat = gps.lat;
    s.scannerLon = gps.lon;
    const GeoEstimate est = fuseGeo(gps, s.bearingDeg, distanceM);
    if (est.lat != 0 || est.lon != 0) {
      s.lat = est.lat;
      s.lon = est.lon;
      s.gpsValid = 1;
    }
  }

  r->pts[r->head] = s;
  r->head = (r->head + 1) % ROOT_SIGHTINGS_PER_DEV;
  if (r->count < ROOT_SIGHTINGS_PER_DEV) r->count++;
}

uint16_t sightCount(const uint8_t* mac) {
  if (!gRings) return 0;
  const uint64_t k = macKey(mac);
  auto it = gRings->find(k);
  if (it == gRings->end() || !it->second) return 0;
  return it->second->count;
}

static void appendPoint(const Sighting& s, char* out, size_t cap, size_t* off, bool first) {
  char tmp[220];
  if (s.gpsValid) {
    snprintf(tmp, sizeof(tmp),
             "%s{\"at_ms\":%lu,\"rssi\":%d,\"distance_m\":%.2f,"
             "\"lat\":%.7f,\"lon\":%.7f,\"scanner_lat\":%.7f,\"scanner_lon\":%.7f,\"gps\":true}",
             first ? "" : ",", (unsigned long)s.atMs, (int)s.rssi, s.distanceM,
             s.lat, s.lon, s.scannerLat, s.scannerLon);
  } else {
    snprintf(tmp, sizeof(tmp),
             "%s{\"at_ms\":%lu,\"rssi\":%d,\"distance_m\":%.2f,\"gps\":false}",
             first ? "" : ",", (unsigned long)s.atMs, (int)s.rssi, s.distanceM);
  }
  const size_t L = strlen(tmp);
  if (*off + L >= cap) return;
  memcpy(out + *off, tmp, L);
  *off += L;
}

void sightAppendJson(const uint8_t* mac, char* out, size_t cap, size_t* off, uint8_t maxPoints) {
  if (!gRings || !out || !off) return;
  const uint64_t k = macKey(mac);
  auto it = gRings->find(k);
  if (it == gRings->end() || !it->second || it->second->count == 0) {
    const char* empty = ",\"sightings\":[]";
    const size_t L = strlen(empty);
    if (*off + L < cap) {
      memcpy(out + *off, empty, L);
      *off += L;
    }
    return;
  }

  Ring* r = it->second;
  const char* hdr = ",\"sightings\":[";
  const size_t hL = strlen(hdr);
  if (*off + hL < cap) {
    memcpy(out + *off, hdr, hL);
    *off += hL;
  }

  const uint8_t total = r->count;
  uint8_t n = total;
  if (maxPoints > 0 && n > maxPoints) n = maxPoints;
  const uint8_t skip = total - n;
  bool first = true;
  const uint8_t start = (r->head + ROOT_SIGHTINGS_PER_DEV - total + skip) % ROOT_SIGHTINGS_PER_DEV;
  for (uint8_t i = 0; i < n; i++) {
    const uint8_t idx = (start + i) % ROOT_SIGHTINGS_PER_DEV;
    appendPoint(r->pts[idx], out, cap, off, first);
    first = false;
  }

  if (*off + 1 < cap) out[(*off)++] = ']';
}

bool sightLatest(const uint8_t* mac, Sighting* out) {
  if (!out || !gRings) return false;
  const uint64_t k = macKey(mac);
  auto it = gRings->find(k);
  if (it == gRings->end() || !it->second || it->second->count == 0) return false;
  Ring* r = it->second;
  const uint8_t idx = (r->head + ROOT_SIGHTINGS_PER_DEV - 1) % ROOT_SIGHTINGS_PER_DEV;
  *out = r->pts[idx];
  return true;
}

void sightBuildMacJson(const uint8_t* mac, char* out, size_t cap) {
  if (!out || cap < 16) return;
  size_t off = 0;
  const char* hdr = "{\"mac\":\"";
  memcpy(out, hdr, strlen(hdr));
  off = strlen(hdr);
  char macbuf[18];
  snprintf(macbuf, sizeof(macbuf), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  snprintf(out + off, cap - off, "%s\"", macbuf);
  off = strlen(out);
  sightAppendJson(mac, out, cap, &off, 0);
  if (off + 2 < cap) {
    out[off++] = '}';
    out[off] = 0;
  }
}
