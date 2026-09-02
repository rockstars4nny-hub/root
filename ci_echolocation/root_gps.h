#pragma once
#include "geo.h"

void gpsInit();
void gpsPoll();
GpsFix gpsGet();

/** Push a fix from the operator laptop (browser / gpsd). Works with or without onboard GPS. */
void gpsInject(double lat, double lon, float altM = 0, float hdop = 1.5f);
