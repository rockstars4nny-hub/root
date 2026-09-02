#include "root_gps.h"
#include "root_config.h"
#include <HardwareSerial.h>
#include <Arduino.h>
#include <string.h>
#include <math.h>

static GpsFix gFix;
static GpsFix gInjected;
static bool gHaveInject = false;
static uint32_t gInjectMs = 0;

// Laptop / host may push a fix when the board has no GPS module.
void gpsInject(double lat, double lon, float altM, float hdop) {
  if (!isfinite(lat) || !isfinite(lon)) return;
  if (lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) return;
  if (fabs(lat) < 0.0001 && fabs(lon) < 0.0001) return;  // reject null-island
  gInjected.lat = lat;
  gInjected.lon = lon;
  gInjected.altM = altM;
  gInjected.hdop = (hdop > 0.f) ? hdop : 1.5f;
  gInjected.valid = true;
  gHaveInject = true;
  gInjectMs = millis();
}

#if ROOT_ENABLE_GPS

static HardwareSerial gGps(1);
static char gLine[96];
static size_t gLen = 0;

static double degMin(const char* s) {
  double v = atof(s);
  return (int)(v / 100) + fmod(v, 100) / 60.0;
}

static void parseGga(char* line) {
  char* t = strtok(line, ",");
  int f = 0;
  char latR[16] = {}, lonR[16] = {}, n[2] = {}, e[2] = {};
  int fixQ = 0;
  float hdop = 99, alt = 0;
  while (t) {
    if (f == 2) strncpy(latR, t, 15);
    if (f == 3) strncpy(n, t, 1);
    if (f == 4) strncpy(lonR, t, 15);
    if (f == 5) strncpy(e, t, 1);
    if (f == 6) fixQ = atoi(t);
    if (f == 8) hdop = atof(t);
    if (f == 9) alt = atof(t);
    t = strtok(nullptr, ",");
    f++;
  }
  if (fixQ < 1) {
    gFix.valid = false;
    return;
  }
  gFix.lat = degMin(latR);
  gFix.lon = degMin(lonR);
  if (n[0] == 'S') gFix.lat = -gFix.lat;
  if (e[0] == 'W') gFix.lon = -gFix.lon;
  gFix.altM = alt;
  gFix.hdop = hdop;
  gFix.valid = true;
}

static void feed(const char* line) {
  if (!strncmp(line, "$GPGGA", 6) || !strncmp(line, "$GNGGA", 6)) {
    char b[96];
    strncpy(b, line, 95);
    b[95] = 0;
    parseGga(b);
  }
}

void gpsInit() {
  gGps.begin(ROOT_GPS_BAUD, SERIAL_8N1, ROOT_GPS_RX, ROOT_GPS_TX);
  Serial.printf("root: GPS UART (RX=%d TX=%d @ %d) + laptop inject\n",
                ROOT_GPS_RX, ROOT_GPS_TX, ROOT_GPS_BAUD);
}

void gpsPoll() {
  while (gGps.available()) {
    const char c = (char)gGps.read();
    if (c == '\n' || c == '\r') {
      if (gLen > 6) {
        gLine[gLen] = 0;
        feed(gLine);
      }
      gLen = 0;
    } else if (gLen + 1 < sizeof gLine) {
      gLine[gLen++] = c;
    }
  }
}

GpsFix gpsGet() {
  // Hardware fix wins when fresh; otherwise laptop inject (TTL 30s)
  if (gFix.valid) return gFix;
  if (gHaveInject && gInjected.valid && (millis() - gInjectMs) < 30000UL) return gInjected;
  return GpsFix();
}

#else

void gpsInit() {
  Serial.println("root: GPS UART off — laptop inject via POST /api/gps");
}

void gpsPoll() {}

GpsFix gpsGet() {
  if (gHaveInject && gInjected.valid && (millis() - gInjectMs) < 30000UL) return gInjected;
  return GpsFix();
}

#endif
