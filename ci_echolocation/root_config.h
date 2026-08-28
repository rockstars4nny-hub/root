#pragma once
/*
 * root hardware — ESP32-S3-N16R8 + CC1101 (SPI) + LR22/E22 915 MHz (UART)
 * Override pins with -DROOT_CC1101_CS=… etc. in platformio build_flags.
 */

#ifndef ROOT_ENABLE_SUBGHZ
#define ROOT_ENABLE_SUBGHZ 1
#endif

#ifndef ROOT_ENABLE_LORA
#define ROOT_ENABLE_LORA 1
#endif

// CC1101 on HSPI (common S3 breakout wiring)
#ifndef ROOT_CC1101_CS
#define ROOT_CC1101_CS 10
#endif
#ifndef ROOT_CC1101_GDO0
#define ROOT_CC1101_GDO0 9
#endif
#ifndef ROOT_SPI_MOSI
#define ROOT_SPI_MOSI 11
#endif
#ifndef ROOT_SPI_MISO
#define ROOT_SPI_MISO 13
#endif
#ifndef ROOT_SPI_SCK
#define ROOT_SPI_SCK 12
#endif

// LR22 / Ebyte E22-900 UART (ESP RX ← module TX, ESP TX → module RX)
#ifndef ROOT_LR22_RX
#define ROOT_LR22_RX 18
#endif
#ifndef ROOT_LR22_TX
#define ROOT_LR22_TX 17
#endif
// E22 factory default is 9600; set 115200 in build_flags if you configured the module
#ifndef ROOT_LR22_BAUD
#define ROOT_LR22_BAUD 9600
#endif
// Mode 0 (transparent RX): tie M0/M1 LOW or drive from ESP (255 = not wired)
#ifndef ROOT_LR22_M0
#define ROOT_LR22_M0 8
#endif
#ifndef ROOT_LR22_M1
#define ROOT_LR22_M1 7
#endif
// ms silence before treating UART burst as one LoRa packet
#ifndef ROOT_LORA_IDLE_MS
#define ROOT_LORA_IDLE_MS 30
#endif

#define ROOT_SUBGHZ_DWELL_MS 320
#define ROOT_SUBGHZ_SAMPLE_US 2000
#define ROOT_SUBGHZ_SAMPLES 16
#define ROOT_SUBGHZ_BURST_DB 4
#define ROOT_SUBGHZ_CARRIER_DB 8
#define ROOT_SUBGHZ_CARRIER_MS 1200
#define ROOT_SUBGHZ_RSSI_FLOOR -110
// Show live band scan on dashboard when peak RSSI exceeds this (dBm)
#define ROOT_SUBGHZ_ACTIVITY_RSSI -102
#define ROOT_SUBGHZ_STALE_MS 120000
#define ROOT_LORA_STALE_MS 120000

// GPS NEO-6M / M10 (Serial1 — optional, off by default)
#ifndef ROOT_ENABLE_GPS
#define ROOT_ENABLE_GPS 0
#endif
#ifndef ROOT_GPS_RX
#define ROOT_GPS_RX 4
#endif
#ifndef ROOT_GPS_TX
#define ROOT_GPS_TX 5
#endif
#ifndef ROOT_GPS_BAUD
#define ROOT_GPS_BAUD 9600
#endif

// 0 = no API cap (return every tracked device)
#ifndef ROOT_API_MAX_DEVICES
#define ROOT_API_MAX_DEVICES 0
#endif
#ifndef ROOT_JSON_MAX_BYTES
#define ROOT_JSON_MAX_BYTES 2097152
#endif
#ifndef ROOT_API_SIGHTINGS
#define ROOT_API_SIGHTINGS 0
#endif
#ifndef ROOT_SIGHTINGS_PER_DEV
#define ROOT_SIGHTINGS_PER_DEV 48
#endif
