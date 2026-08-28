#pragma once
#include <math.h>
#include <stdint.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct GpsFix {
  double lat = 0;
  double lon = 0;
  float altM = 0;
  float hdop = 99;
  float courseDeg = 0;
  float speedKmh = 0;
  bool valid = false;
};

struct GeoEstimate {
  double lat = 0;
  double lon = 0;
  float accuracyM = 9999;
};

inline float macBearingDeg(const uint8_t* mac) {
  uint32_t h = 2166136261u;
  for (int i = 0; i < 6; i++) {
    h ^= mac[i];
    h *= 16777619u;
  }
  return (float)(h % 360);
}

inline void polarToLocal(float bearingDeg, float distM, float& eastM, float& northM) {
  const double br = bearingDeg * M_PI / 180.0;
  eastM = (float)(sin(br) * distM);
  northM = (float)(cos(br) * distM);
}

inline GeoEstimate fuseGeo(const GpsFix& scanner, float bearingDeg, float distanceM) {
  GeoEstimate out;
  if (!scanner.valid || distanceM <= 0) return out;

  const double br = bearingDeg * M_PI / 180.0;
  const double lat1 = scanner.lat * M_PI / 180.0;
  const double lon1 = scanner.lon * M_PI / 180.0;
  const double d = distanceM / 6378137.0;

  const double lat2 = asin(sin(lat1) * cos(d) + cos(lat1) * sin(d) * cos(br));
  const double lon2 = lon1 + atan2(sin(br) * sin(d) * cos(lat1),
                                    cos(d) - sin(lat1) * sin(lat2));
  out.lat = lat2 * 180.0 / M_PI;
  out.lon = lon2 * 180.0 / M_PI;
  out.accuracyM = distanceM * 0.35f + scanner.hdop * 3.0f;
  return out;
}
