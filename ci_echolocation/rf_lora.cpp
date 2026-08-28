#include "rf_lora.h"
#include "root_config.h"

#if ROOT_ENABLE_LORA

#include <HardwareSerial.h>

static HardwareSerial gLora(2);
static bool gOk = false;
static uint32_t gPktCount = 0;
static uint32_t gUartBytes = 0;
static uint32_t gLastUartMs = 0;
static int8_t gLastRssi = -120;
static char gLastLabel[33] = {0};
static uint8_t gRxBuf[256];
static size_t gRxLen = 0;
static uint32_t gLastRxMs = 0;

static LoraHit gHitQ[16];
static uint8_t gHitHead = 0;
static uint8_t gHitTail = 0;

static const uint8_t kActivityMac[6] = {0x02, 0x4C, 0x91, 0x50, 0x00, 0x01};

static void pushHit(const LoraHit& h) {
  gHitQ[gHitHead] = h;
  gHitHead = (gHitHead + 1) % 16;
  if (gHitHead == gHitTail) gHitTail = (gHitTail + 1) % 16;
}

static uint32_t fnv1a(const uint8_t* p, size_t n) {
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < n; i++) {
    h ^= p[i];
    h *= 16777619u;
  }
  return h;
}

static void synthMac(uint8_t* mac, const uint8_t* payload, size_t len) {
  const uint32_t h = fnv1a(payload, len);
  mac[0] = 0x02;
  mac[1] = 0x4C;
  mac[2] = (uint8_t)(h >> 24);
  mac[3] = (uint8_t)(h >> 16);
  mac[4] = (uint8_t)(h >> 8);
  mac[5] = (uint8_t)h;
}

static int8_t rssiFromByte(uint8_t b) {
  if (b == 0) return -120;
  return (int8_t)(-120 + (int)((b * 60) / 255));
}

static void emitPacket(const uint8_t* payload, size_t len, int8_t rssi) {
  if (len < 1) return;
  gPktCount++;
  gLastRssi = rssi;
  gLastUartMs = millis();

  LoraHit hit = {};
  hit.rssi = rssi;
  synthMac(hit.mac, payload, len);
  const size_t n = len < 32 ? len : 32;
  for (size_t i = 0; i < n; i++) {
    const char c = (char)payload[i];
    hit.label[i] = (c >= 32 && c <= 126) ? c : '.';
  }
  hit.label[n] = 0;
  strncpy(gLastLabel, hit.label, sizeof gLastLabel - 1);
  pushHit(hit);

  char macbuf[18];
  snprintf(macbuf, sizeof(macbuf), "%02X:%02X:%02X:%02X:%02X:%02X",
           hit.mac[0], hit.mac[1], hit.mac[2], hit.mac[3], hit.mac[4], hit.mac[5]);
  Serial.printf("root ping: [%s] | kind: lora | band: lora | RSSI: %d dBm | label: %s\n",
                macbuf, (int)rssi, hit.label[0] ? hit.label : "(binary)");
}

static void flushRx(int8_t defaultRssi) {
  if (gRxLen < 1) return;

  const uint8_t* payload = gRxBuf;
  size_t len = gRxLen;
  int8_t rssi = defaultRssi;

  if (len >= 2) {
    const uint8_t tail = gRxBuf[len - 1];
    if (tail > 0 && tail < 250) {
      rssi = rssiFromByte(tail);
      len--;
    }
  }

  emitPacket(payload, len, rssi);
  gRxLen = 0;
}

static void tryEbyteFrame() {
  while (gRxLen >= 4 && gRxBuf[0] == 0xC0) {
    size_t end = 1;
    while (end < gRxLen && gRxBuf[end] != 0xC0) end++;
    if (end >= gRxLen) return;
    const size_t frameLen = end + 1;
    if (frameLen >= 6) {
      const uint8_t* p = gRxBuf + 1;
      const size_t plen = frameLen - 2;
      const int8_t rssi = rssiFromByte(p[plen > 0 ? plen - 1 : 0]);
      emitPacket(p, plen, rssi);
    }
    memmove(gRxBuf, gRxBuf + frameLen, gRxLen - frameLen);
    gRxLen -= frameLen;
  }
}

static void pushByte(uint8_t b) {
  if (gRxLen >= sizeof gRxBuf) gRxLen = 0;
  gRxBuf[gRxLen++] = b;
  gUartBytes++;
  gLastUartMs = millis();
}

static void setModePins() {
#if ROOT_LR22_M0 != 255
  pinMode(ROOT_LR22_M0, OUTPUT);
  digitalWrite(ROOT_LR22_M0, LOW);
#endif
#if ROOT_LR22_M1 != 255
  pinMode(ROOT_LR22_M1, OUTPUT);
  digitalWrite(ROOT_LR22_M1, LOW);
#endif
}

bool loraReady() { return gOk; }

uint32_t loraUartBytes() { return gUartBytes; }

void loraInit() {
  setModePins();
  delay(80);
  gLora.begin(ROOT_LR22_BAUD, SERIAL_8N1, ROOT_LR22_RX, ROOT_LR22_TX);
  gLora.setTimeout(5);
  while (gLora.available() > 0) (void)gLora.read();
  gOk = true;
  Serial.printf("root: LR22 LoRa UART (RX=%d TX=%d @ %d, M0=%d M1=%d mode=transparent)\n",
                ROOT_LR22_RX, ROOT_LR22_TX, ROOT_LR22_BAUD,
                (int)ROOT_LR22_M0, (int)ROOT_LR22_M1);
}

void loraPoll(uint32_t nowMs) {
  if (!gOk) return;

  if (gLora.available() > 0) {
    gLastRxMs = nowMs;
    while (gLora.available() > 0) pushByte((uint8_t)gLora.read());
    if (gRxBuf[0] == 0xC0) tryEbyteFrame();
  } else if (gRxLen > 0 && gLastRxMs && (nowMs - gLastRxMs) >= ROOT_LORA_IDLE_MS) {
    flushRx(-95);
    gLastRxMs = 0;
  }
}

bool loraPopHit(LoraHit* out) {
  if (!out || gHitHead == gHitTail) return false;
  *out = gHitQ[gHitTail];
  gHitTail = (gHitTail + 1) % 16;
  return true;
}

bool loraPopActivity(LoraHit* out) {
  if (!out || !gOk) return false;

  LoraHit h = {};
  memcpy(h.mac, kActivityMac, 6);
  const uint32_t now = millis();
  const uint32_t idleMs = gLastUartMs ? (now - gLastUartMs) : 999999;
  h.rssi = (idleMs < 5000) ? gLastRssi : -120;

  if (gPktCount > 0 && gLastLabel[0]) {
    snprintf(h.label, sizeof h.label, "LR22 · %lu pkts · %s",
             (unsigned long)gPktCount, gLastLabel);
  } else if (gUartBytes > 0) {
    snprintf(h.label, sizeof h.label, "LR22 · %lu uart B · %lu pkts",
             (unsigned long)gUartBytes, (unsigned long)gPktCount);
  } else {
    strncpy(h.label, "LR22 · 915 MHz · listening", sizeof h.label - 1);
  }
  *out = h;
  return true;
}

void loraStatusJson(char* out, size_t n) {
  if (!out || n < 8) return;
  snprintf(out, n,
           "{\"ready\":%s,\"band\":\"915\",\"packets\":%lu,\"uart_bytes\":%lu,"
           "\"last_rssi\":%d,\"last_label\":\"%s\",\"baud\":%d,"
           "\"active_ms\":%lu}",
           gOk ? "true" : "false",
           (unsigned long)gPktCount,
           (unsigned long)gUartBytes,
           (int)gLastRssi,
           gLastLabel,
           ROOT_LR22_BAUD,
           gLastUartMs ? (unsigned long)(millis() - gLastUartMs) : 0ul);
}

#else

bool loraReady() { return false; }
uint32_t loraUartBytes() { return 0; }
void loraInit() {}
void loraPoll(uint32_t) {}
bool loraPopHit(LoraHit*) { return false; }
bool loraPopActivity(LoraHit*) { return false; }
void loraStatusJson(char* out, size_t n) {
  if (out && n) snprintf(out, n, "{\"ready\":false}");
}

#endif
