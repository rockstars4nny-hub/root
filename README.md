# root

**Wi‑Fi · BLE · Sub‑GHz recon** — passive field scanner for nearby wireless devices.

`root` is the ESP32 firmware + dashboard for **Wi‑Fi promiscuous** and **CC1101 sub‑GHz** (plus optional 915 MHz LoRa). **BLE** runs on your operator machine via **[kitdd](https://github.com/rockstars4nny-hub)** / DD — same session, merged exports.

Power the board, join its AP, and collect MACs, RSSI, SSIDs, sub‑GHz bursts, and BLE advertisements in one recon workflow. Distance on the dashboard is an **RSSI estimate** only — no fake direction.

## Capabilities

| Area | What it does |
|------|----------------|
| **Wi‑Fi** | Promiscuous 802.11 — probes, beacons, data, deauth; MAC, RSSI, SSID, channel, vendor OUI, BSSID, encryption, per-type packet counts |
| **Sub‑GHz** | CC1101 scan — 315 / 433 / 868 / 915 MHz (remotes, sensors, TPMS-class bursts) |
| **LoRa** | Optional E22 UART 915 MHz transparent RX |
| **BLE** | Host-side via `kitdd ble` — active scan, GATT, JSON export to `~/dd-sessions` |
| **Dashboard** | Signal range map (RSSI distance rings), device cards, field notes, whitelist/blacklist, copy/JSON |
| **HTTP API** | `GET /api/devices`, `/api/rf`, `/api/sightings`, `/api/ping` — PSRAM-backed, no device cap |
| **GPS inject** | Operator laptop pushes fix via `POST /api/gps` (ARIA / browser); onboard UART GPS optional |
| **ARIA** | Device pull into Stem · Radar; join AP `root` / `root-radar` @ `192.168.4.1` |
| **DD / kitdd** | `kitdd wifi`, `subghz`, `ble`, `radar`, `session` — merged field exports |

**Honesty:** RSSI and packet fields are measured; distance is estimated; bearing/direction is **not** provided.

| Band | Where | How |
|------|--------|-----|
| **Wi‑Fi** | ESP32 (this repo) | Promiscuous sniff — probes, beacons, data, deauth |
| **Sub‑GHz** | ESP32 (this repo) | CC1101 — 315 / 433 / 868 / 915 MHz |
| **BLE** | Host PC + `kitdd ble` | CSR / Windows BT adapter — active scan, GATT, export |
| **LoRa** | ESP32 (optional) | E22 UART 915 MHz RX |

## Features

| Layer | What you get |
|-------|----------------|
| **Wi‑Fi** | Probes, beacons, data, deauth — MAC, RSSI, SSID, channel, zone, vendor OUI, BSSID, encryption, per-type packet counts |
| **Sub‑GHz** | CC1101 scan — remotes, sensors, fixed emitters, TPMS-class bursts |
| **BLE** | Via `kitdd ble` — names, services, RSSI, parsed JSON to `~/dd-sessions` |
| **LoRa** | E22 UART 915 MHz transparent RX |
| **Dashboard** | Signal range map, device cards with field notes, copy/JSON, whitelist/blacklist |
| **API** | JSON over HTTP — no device cap (PSRAM-backed hash table) |

## Hardware

**Board:** ESP32-S3-N16R8 (16 MB flash, 8 MB OPI PSRAM)

| Module | Signal | GPIO |
|--------|--------|------|
| **CC1101** | MOSI / MISO / SCK / CS / GDO0 | 11 / 13 / 12 / 10 / 9 |
| **LR22 / E22** | TX → ESP RX, RX ← ESP TX, M0, M1 | 18 / 17 / 8 / 7 |

- **M0 + M1 → GND** for LoRa receive (transparent mode).
- LoRa UART default: **9600 baud** (E22 factory). For 115200 add `-DROOT_LR22_BAUD=115200` to `platformio.ini`.
- Pin overrides: edit `ci_echolocation/root_config.h` or add `-DROOT_CC1101_CS=…` etc. in `build_flags`.

GPS is **off** by default on the ESP (`ROOT_ENABLE_GPS=0`). You can still geotag from the **operator laptop**:

1. Browser Geolocation (ARIA) or **gpsd** on the laptop
2. ARIA / browser pushes to Root: `POST http://192.168.4.1/api/gps` `{"lat":…,"lon":…}`
3. Root serves that fix as `scanner_gps` on `/api/devices` for ~30s (refresh by watching GPS in ARIA Radar)

Onboard UART GPS still wins when `ROOT_ENABLE_GPS=1` and a module has a fix.

## Build & flash

```bash
cd root
pio run -e esp32-s3-n16r8
pio run -e esp32-s3-n16r8 -t upload
```

Monitor serial at **115200**:

```bash
pio device monitor -b 115200
```

Serial commands: `status` · `rf` · `hop auto` · `hop <1-13>` · `help`

## Use

1. Power the board.
2. On your phone, join Wi‑Fi **`root`** / password **`root-radar`**.
3. Open **http://192.168.4.1**

### Dashboard

- **Signal range map** — dot **radius** = estimated distance from RSSI. Position around each ring is layout only, **not** measured direction.
- **Device list** — each card explains every field (MAC, RSSI, band, type, etc.) and what you can do with it.
- **Copy / JSON** — per device or all visible; **Clear** resets the session list.
- Badge **`live/session`** — devices heard in the last 60s vs total this session.

### HTTP API

| Endpoint | Description |
|----------|-------------|
| `GET /api/ping` | Liveness |
| `GET /api/devices` | All tracked devices (JSON) |
| `GET /api/sightings?mac=AA:BB:…` | RSSI/distance history for one MAC |
| `GET /api/rf` | Sub‑GHz + LoRa driver status |
| `GET /api/channel?ch=auto` or `?ch=6` | Wi‑Fi channel / hop mode |

Example:

```bash
curl -s http://192.168.4.1/api/rf | jq .
curl -s http://192.168.4.1/api/devices | jq '.count, .devices[0]'
```

### With DD / kitdd (BLE + merged session)

```bash
export KIT_ROOT=http://192.168.4.1

kitdd wifi              # pull /api/devices from root
kitdd subghz            # sub-GHz snapshot
kitdd ble               # BLE scan on host adapter → ~/dd-sessions
kitdd ble -60           # 60s active BLE scan
kitdd radar             # wifi + ble combined
kitdd session           # wifi + ble + subghz one-shot
```

BLE does **not** run on the ESP32 — use a USB BT dongle (CSR) or Windows Bluetooth with `kitdd` on the same laptop/WSL session as root.

## What's real vs estimated

| Data | Source |
|------|--------|
| MAC, SSID, channel, packet type, vendor, BSSID, encryption | Measured from promiscuous 802.11 capture |
| Packet counts / rate (`pkts`, `pkt_rate`) | Measured from captured frame types |
| RSSI | Measured at antenna |
| Distance (`distance_m`) | **Estimated** from RSSI (walls/fade skew it) |
| Direction / bearing | **Not provided** — no fake angles in API or radar |
| GPS lat/lon | Only when GPS hardware enabled and fixed |

## Project layout

```
ci_echolocation/
  ci_echolocation.ino   Main firmware
  ci_dashboard.h        Embedded web UI (PROGMEM)
  wifi_pkt.cpp          802.11 frame parser (addr, OUI, encryption IEs)
  rf_subghz.cpp           CC1101 (RadioLib)
  rf_lora.cpp             E22 UART LoRa
  root_config.h           Pins & thresholds
  sight_log.cpp           Per-device RSSI history
  psram_alloc.h           PSRAM STL allocator
platformio.ini            Build env (default: esp32-s3-n16r8)
scripts/ch343-attach.sh   WSL USB serial helper
```

## License & authorized use

**Authorized use only.** Use root on networks, devices, and RF environments you **own** or have **explicit written permission** to monitor (your lab, your home, signed pentest/audit scope, etc.).

Passive monitoring only — comply with local RF, privacy, and computer-access laws. Do not use this tool to intercept communications you are not authorized to observe.

Firmware in this repo — use and modify for your own kit.
