#include "wifi_pkt.h"
#include <string.h>
#include <stdio.h>

static void macCopy(uint8_t* dst, const uint8_t* src) {
  memcpy(dst, src, 6);
}

static bool macZero(const uint8_t* m) {
  static const uint8_t z[6] = {0};
  return memcmp(m, z, 6) == 0;
}

static const char* ouiLookup(uint8_t b0, uint8_t b1, uint8_t b2) {
  // Common OUIs — IoT / phone / AP vendors seen in field scans.
  if (b0 == 0x00 && b1 == 0x03 && b2 == 0x93) return "Apple";
  if (b0 == 0x00 && b1 == 0x0A && b2 == 0x27) return "Apple";
  if (b0 == 0x00 && b1 == 0x0D && b2 == 0x93) return "Apple";
  if (b0 == 0x28 && b1 == 0x37 && b2 == 0x37) return "Apple";
  if (b0 == 0x3C && b1 == 0x06 && b2 == 0x30) return "Apple";
  if (b0 == 0x94 && b1 == 0xE9 && b2 == 0x6A) return "Apple";
  if (b0 == 0x00 && b1 == 0x12 && b2 == 0xFB) return "Samsung";
  if (b0 == 0x8C && b1 == 0x77 && b2 == 0x12) return "Samsung";
  if (b0 == 0x94 && b1 == 0xEB && b2 == 0x2C) return "Google";
  if (b0 == 0xF4 && b1 == 0xF5 && b2 == 0xD8) return "Google";
  if (b0 == 0x00 && b1 == 0x24 && b2 == 0xE4) return "Espressif";
  if (b0 == 0x24 && b1 == 0x6F && b2 == 0x28) return "Espressif";
  if (b0 == 0x30 && b1 == 0xAE && b2 == 0xA4) return "Espressif";
  if (b0 == 0x3C && b1 == 0x61 && b2 == 0x05) return "Espressif";
  if (b0 == 0x84 && b1 == 0xCC && b2 == 0xA8) return "Espressif";
  if (b0 == 0x00 && b1 == 0x17 && b2 == 0x88) return "TP-Link";
  if (b0 == 0x50 && b1 == 0xC7 && b2 == 0xBF) return "TP-Link";
  if (b0 == 0x00 && b1 == 0x1A && b2 == 0x70) return "Netgear";
  if (b0 == 0x00 && b1 == 0x14 && b2 == 0x6C) return "Netgear";
  if (b0 == 0x00 && b1 == 0x0F && b2 == 0x66) return "Linksys";
  if (b0 == 0x00 && b1 == 0x25 && b2 == 0x9C) return "Linksys";
  if (b0 == 0x00 && b1 == 0x1E && b2 == 0x58) return "D-Link";
  if (b0 == 0x00 && b1 == 0x26 && b2 == 0x5A) return "D-Link";
  if (b0 == 0x00 && b1 == 0x0C && b2 == 0x43) return "Ruckus";
  if (b0 == 0x00 && b1 == 0x04 && b2 == 0x96) return "Arlo";
  if (b0 == 0x00 && b1 == 0x1D && b2 == 0xC9) return "Arlo";
  if (b0 == 0xB0 && b1 == 0x4A && b2 == 0x39) return "Beijing Xiaomi";
  if (b0 == 0x64 && b1 == 0x09 && b2 == 0x80) return "Xiaomi";
  if (b0 == 0x00 && b1 == 0xE0 && b2 == 0x4C) return "Realtek";
  if (b0 == 0x00 && b1 == 0x1B && b2 == 0x11) return "Intel";
  if (b0 == 0x00 && b1 == 0x21 && b2 == 0x5C) return "Intel";
  if (b0 == 0x00 && b1 == 0x50 && b2 == 0xF2) return "Microsoft";
  if (b0 == 0x00 && b1 == 0x15 && b2 == 0x5D) return "Microsoft";
  if (b0 == 0x00 && b1 == 0x0C && b2 == 0x29) return "VMware";
  if (b0 == 0x00 && b1 == 0x50 && b2 == 0x56) return "VMware";
  if (b0 == 0x00 && b1 == 0x16 && b2 == 0x3E) return "Cisco";
  if (b0 == 0x00 && b1 == 0x1B && b2 == 0x0D) return "Cisco";
  if (b0 == 0x00 && b1 == 0x18 && b2 == 0xE7) return "Ubiquiti";
  if (b0 == 0x24 && b1 == 0xA4 && b2 == 0x3C) return "Ubiquiti";
  if (b0 == 0x00 && b1 == 0x27 && b2 == 0x22) return "Amazon";
  if (b0 == 0x00 && b1 == 0xFC && b2 == 0x8B) return "Amazon";
  if (b0 == 0x34 && b1 == 0xD2 && b2 == 0x70) return "Amazon";
  if (b0 == 0x00 && b1 == 0x04 && b2 == 0x4B) return "Nest";
  if (b0 == 0x18 && b1 == 0xB4 && b2 == 0x30) return "Nest";
  if (b0 == 0x00 && b1 == 0x24 && b2 == 0x36) return "Ring";
  if (b0 == 0x00 && b1 == 0x1C && b2 == 0xBF) return "Wyze";
  if (b0 == 0x00 && b1 == 0x62 && b2 == 0x6E) return "Raspberry Pi";
  if (b0 == 0xDC && b1 == 0xA6 && b2 == 0x32) return "Raspberry Pi";
  if (b0 == 0x00 && b1 == 0x11 && b2 == 0x32) return "Synology";
  return nullptr;
}

static void vendorFromMac(const uint8_t* mac, char* out, size_t n) {
  if (!out || n < 4) return;
  const char* name = ouiLookup(mac[0], mac[1], mac[2]);
  if (name) {
    strncpy(out, name, n - 1);
    out[n - 1] = 0;
    return;
  }
  snprintf(out, n, "%02X:%02X:%02X", mac[0], mac[1], mac[2]);
}

static void scanEncryptIes(const uint8_t* p, uint16_t len, size_t bodyOff, char* out, size_t n,
                           bool privacyBit) {
  if (!out || n < 5) return;
  if (privacyBit) {
    strncpy(out, "protected", n - 1);
    out[n - 1] = 0;
  } else {
    strncpy(out, "open", n - 1);
    out[n - 1] = 0;
  }

  size_t off = bodyOff;
  while (off + 2 <= len) {
    const uint8_t id = p[off];
    const uint8_t elen = p[off + 1];
    if (off + 2 + elen > len) break;
    if (id == 48 && elen >= 2) {
      strncpy(out, "WPA2/WPA3", n - 1);
      out[n - 1] = 0;
      return;
    }
    if (id == 221 && elen >= 4 && p[off + 2] == 0x00 && p[off + 3] == 0x50 &&
        p[off + 4] == 0xF2 && p[off + 5] == 0x01) {
      strncpy(out, "WPA", n - 1);
      out[n - 1] = 0;
    }
    off += 2u + elen;
  }
}

void wifiParseFrame(const uint8_t* p, uint16_t len, uint8_t ftype, uint8_t subtype, WifiPktMeta* out) {
  if (!out) return;
  memset(out, 0, sizeof(WifiPktMeta));
  out->subtype = subtype;
  if (!p || len < 24) return;

  const uint8_t fc1 = p[1];
  out->toDs = (fc1 >> 0) & 1;
  out->fromDs = (fc1 >> 1) & 1;
  out->seq = ((uint16_t)(p[22] & 0x0F) << 8) | p[23];

  const uint8_t* a1 = p + 4;
  const uint8_t* a2 = p + 10;
  const uint8_t* a3 = p + 16;

  if (ftype == 2) {
    // Data — address field order depends on DS bits.
    if (!out->toDs && !out->fromDs) {
      macCopy(out->dst, a1);
      macCopy(out->src, a2);
      macCopy(out->bssid, a3);
    } else if (!out->toDs && out->fromDs) {
      macCopy(out->dst, a1);
      macCopy(out->bssid, a2);
      macCopy(out->src, a3);
    } else if (out->toDs && !out->fromDs) {
      macCopy(out->bssid, a1);
      macCopy(out->src, a2);
      macCopy(out->dst, a3);
    } else {
      macCopy(out->dst, a1);
      macCopy(out->src, a2);
    }
  } else {
    // Management / control — Addr1=DA, Addr2=SA, Addr3=BSSID.
    macCopy(out->dst, a1);
    macCopy(out->src, a2);
    macCopy(out->bssid, a3);
  }

  out->hasDst = !macZero(out->dst);
  out->hasBssid = !macZero(out->bssid);
  vendorFromMac(out->src, out->vendor, sizeof out->vendor);

  if (ftype == 0 && (subtype == 8 || subtype == 5) && len >= 36) {
    const size_t body = 24;
    const uint16_t cap = (uint16_t)p[body + 10] | ((uint16_t)p[body + 11] << 8);
    const bool privacy = (cap & 0x0010) != 0;
    scanEncryptIes(p, len, body + 12, out->encrypt, sizeof out->encrypt, privacy);
  }
}
