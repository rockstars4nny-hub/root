# root

**Wi‑Fi · BLE · Sub‑GHz recon** — passive field scanner for nearby wireless devices.

`root` is the ESP32 firmware + dashboard for **Wi‑Fi promiscuous** (with Espressif **Wi‑Fi LR**), **onboard BLE**, and **CC1101 sub‑GHz** (plus optional 915 MHz LoRa). SoftAP stays phone-friendly (b/g/n) while LR is OR’d for ESP↔ESP reach via ESP-NOW.

Power the board, join its AP, and collect MACs, RSSI, SSIDs, BLE advertisers, and sub‑GHz bursts in one recon workflow. Distance on the dashboard is an **RSSI estimate** only — no fake direction.

## What you can do with root

Carry an ESP32-S3, power it up, join its Wi‑Fi, and **see every wireless device talking near you** — without connecting to their networks.

### Wi‑Fi — hear everything on the air

- **Passive promiscuous sniff** — capture probes, beacons, data frames, and deauth bursts without joining target networks.
- **Espressif Wi‑Fi LR** — SoftAP runs `b/g/n + LR` so phones still join while ESP↔ESP long-range / ESP-NOW (`./omni lr`) works.
- **Identify devices** — MAC, vendor (OUI), BSSID, SSID, channel, encryption type, and **how many packets** each device sent.
- **Estimate distance** from RSSI (honest: it's a guess, not a laser rangefinder).
- **No fake bearings** — the map shows proximity rings, not made-up direction arrows.

### BLE — onboard ESP32-S3

- **Active advertise scan** on the kit radio (`./omni ble scan on|off`, `list`, `filter`).
- Devices show up in `/api/devices` with `"band":"ble"` and on the dashboard BLE pill/filter.

### Sub‑GHz + LoRa — remotes, sensors, bursts

- **CC1101 scan** across 315 / 433 / 868 / 915 MHz — garage remotes, TPMS-class bursts, fixed sensors.
- **Optional LoRa RX** on 915 MHz via E22 module.

### Field dashboard @ `192.168.4.1`

- **Signal range map** — dots sized by estimated distance; live vs session device counts.
- **Device cards** with field notes, whitelist/blacklist, copy-one or copy-all JSON.
- **HTTP API** for automation — unlimited device table backed by PSRAM.

### Works with your laptop stack

- **ARIA** Omni tab drives `./omni` over `POST /api/root/omni` and pulls Root devices into Stem · Radar.
- **Laptop GPS inject** still works via `POST /api/gps`.
- Optional **`kitdd`** can still merge host-side extras if you want deeper GATT on a USB BT dongle.

Join **`root` / `root-radar`** → open **http://192.168.4.1**

## OmniScan (`./omni`)

Full command reference: **[OMNI.md](OMNI.md)** — Serial, HTTP `/api/omni`, ARIA Omni tab, BLE, Sub‑GHz raw, Wi‑Fi LR / ESP-NOW.

```text
./omni start
./omni status
./omni ble list
./omni subghz raw
./omni system help
```

## Recon stack

| Band | Where | How |
|------|--------|-----|
| **Wi‑Fi** | ESP32 (this repo) | Promiscuous sniff — probes, beacons, data, deauth |
| **Wi‑Fi LR** | ESP32 (this repo) | `WIFI_PROTOCOL_LR` + ESP-NOW peer (`./omni lr`) |
| **BLE** | ESP32 (this repo) | Onboard advertise scan → `band=ble` |
| **Sub‑GHz** | ESP32 (this repo) | CC1101 — 315 / 433 / 868 / 915 MHz |
| **LoRa** | ESP32 (optional) | E22 UART 915 MHz RX |

## Features

| Layer | What you get |
|-------|----------------|
| **Wi‑Fi** | Probes, beacons, data, deauth — MAC, RSSI, SSID, channel, zone, vendor OUI, BSSID, encryption, per-type packet counts |
| **Wi‑Fi LR** | SoftAP b/g/n+LR · ESP-NOW peer send/ping |
| **BLE** | Onboard scan — address, name, RSSI, classify tags |
| **Sub‑GHz** | CC1101 scan — remotes, sensors, fixed emitters, TPMS-class bursts |
| **LoRa** | E22 UART 915 MHz transparent RX |
| **Dashboard** | Signal range map, device cards with field notes, copy/JSON, whitelist/blacklist |
| **API** | JSON over HTTP — `ble` + `wifi_lr` flags on `/api/devices` |

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

Serial commands: `status` · `rf` · `hop auto` · `hop <1-13>` · `./omni …` · `help`

### OmniScan (`./omni`)

Interactive command interface over **Serial** and **HTTP** `POST /api/omni` `{"cmd":"./omni status"}`.

Also available in **ARIA → Omni tab** (tool capabilities + live console).

Examples: `./omni start` · `./omni status` · `./omni subghz raw` · `./omni wifi handshake` · `./omni system help`

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
| `POST /api/omni` `{"cmd":"./omni status"}` | OmniScan command interface |
| `GET /api/omni?cmd=./omni%20status` | Same via query string |

Example:

```bash
curl -s http://192.168.4.1/api/rf | jq .
curl -s http://192.168.4.1/api/devices | jq '.count, .devices[0]'
```

### With DD / kitdd (optional host merge)

Onboard BLE is already in `/api/devices` (`band=ble`). kitdd remains useful for host GATT extras or offline session merge:

```bash
export KIT_ROOT=http://192.168.4.1

kitdd wifi              # pull /api/devices from root (includes ble + wifi_lr flags)
kitdd subghz            # sub-GHz snapshot
kitdd ble               # optional deeper host BT adapter scan → ~/dd-sessions
kitdd session           # wifi + ble + subghz one-shot
```

### Omni BLE / Wi‑Fi LR

```text
./omni ble scan on
./omni ble list
./omni lr status
./omni lr peer AA:BB:CC:DD:EE:FF
./omni lr test
```

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
