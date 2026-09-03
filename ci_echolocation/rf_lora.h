#pragma once
#include <stdint.h>
#include <stddef.h>

struct LoraHit {
  int8_t rssi;
  uint8_t mac[6];
  char label[40];
  char detail[48];  // hex preview
  uint16_t len;
  bool activity;    // true = status-only heartbeat (do not count as packet)
};

void loraInit();
void loraPoll(uint32_t nowMs);
bool loraPopHit(LoraHit* out);
bool loraPopActivity(LoraHit* out);
bool loraReady();
uint32_t loraUartBytes();
void loraStatusJson(char* out, size_t n);
