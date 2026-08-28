#pragma once
#include <stddef.h>
#include <stdint.h>

struct Sighting {
  uint32_t atMs;
  int8_t rssi;
  float distanceM;
  float bearingDeg;
  float eastM;
  float northM;
  double lat;
  double lon;
  double scannerLat;
  double scannerLon;
  uint8_t gpsValid;
};

void sightInit();
void sightPush(const uint8_t* mac, uint32_t atMs, int8_t rssi, float distanceM);
uint16_t sightCount(const uint8_t* mac);
bool sightLatest(const uint8_t* mac, Sighting* out);
void sightAppendJson(const uint8_t* mac, char* out, size_t cap, size_t* off, uint8_t maxPoints);
void sightBuildMacJson(const uint8_t* mac, char* out, size_t cap);
