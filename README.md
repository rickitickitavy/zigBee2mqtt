# ESP32 Zigbee–MQTT gateway

PlatformIO / Arduino firmware for **two chips, two images**:

| Role | Chip | PlatformIO env | Responsibility |
|------|------|----------------|----------------|
| **Host** | ESP32-S3-N16R8 | `esp32-s3-host` | Wi‑Fi (AP/STA), web console, MQTT, device registry, SPI master, joined OTA |
| **Slave** | ESP32-C6 | `esp32-c6-slave` | Zigbee ZCZR coordinator, SPI slave |

Role is fixed by **which image is flashed** to which chip (compile-time `BOARD_ROLE_HOST` / `BOARD_ROLE_SLAVE`). There is **no GPIO15 role strap** and **no shared binary** for both boards. Field updates use one **joined package** (ZIP with `slave.bin` + `host.bin`) from the web console **System → Update**.

The host publishes device state to **per-device MQTT topics**. The slave runs the Zigbee coordinator after the host pushes settings.

This is not a port of the Node.js Zigbee2MQTT converter database.

## Hardware / USB

- Flash and monitor over each board’s **native USB-C** (USB Serial/JTAG CDC).
- Hold **BOOT** LOW at reset on the **host** to force AP for that boot (does not change stored MODE). Host boot button is GPIO41; slave boot button is GPIO9.
- Slave onboard RGB is GPIO8 (status); do not use it as a boot-AP strap.

### Status LEDs (six reds)

GPIO HIGH = on. Pin maps differ by chip (`include/pins.h`):

| LED | Host (S3) | Slave (C6) |
|-----|-----------|------------|
| LED1 | GPIO4 | GPIO18 |
| LED2 | GPIO5 | GPIO19 |
| LED3 | GPIO6 | GPIO20 |
| LED4 | GPIO7 | GPIO21 |
| LED5 | GPIO15 | GPIO2 |
| LED6 | GPIO16 | GPIO3 |

| Indicator | Host | Slave |
|-----------|------|--------|
| LED1 | 0.1 s: MQTT device command received | 0.1 s: packet from an **unknown** device |
| LED2 | 0.1 s: MQTT device state published | 0.1 s: packet from a **known** device |
| LED3 | Unused | 0.1 s: radio ACK that our command was delivered |
| LED4 | On while the local MQTT broker is listening | 0.1 s: **only** command actually sent on the radio |
| LED5 | Solid while MQTT is connected; blinks (0.25 s) for boot only (not errors) | Solid when ready; blinks (0.25 s) for boot/critical |
| LED6 | Solid for critical (e.g. lost slave); 10 Hz blink while firmware update is busy | Blinks while pairing/join is open |

### Two-board wiring (3.3 V, common GND)

| Signal | Host (S3) | Slave (C6) |
|--------|-----------|------------|
| RST | GPIO9 → slave **EN** (active LOW pulse) | EN |
| SCK | GPIO12 | GPIO6 |
| MOSI | GPIO11 | GPIO5 |
| MISO | GPIO13 | GPIO4 |
| CS | GPIO10 | GPIO7 |
| IRQ | GPIO14 in | GPIO10 out (HIGH = slave has a frame) |

Host→slave uses CS + SCK + MOSI (no IRQ). Slave→host (Zigbee events and logs) uses the same SPI plus IRQ.

## Build and flash

```bash
pio run -e esp32-s3-host -e esp32-c6-slave
pio run -e esp32-s3-host -t upload
pio run -e esp32-c6-slave -t upload
```

Optional joined Update package (scripts in repo / CI): combine `host.bin` and `slave.bin` into the ZIP the console expects.

A pre-build script gzips `data/index.html` and `data/css/all.css` into the host firmware image, so a separate filesystem flash is not required for the web console.

First Zigbee flash on the slave: erase recommended so `zb_storage` is clean:

```bash
pio run -e esp32-c6-slave -t erase
pio run -e esp32-c6-slave -t upload
```

## CLion

1. Install the PlatformIO plugin.
2. Open this folder. `pio project init --ide clion` generates CMake files if they are missing.
3. Build selected envs; upload/monitor as above (115200).

## Configure over USB CLI

Type `help` on the **host**. Typical first run:

```
wifi MySsid MyPassword
mqtt 192.168.1.10 1883
save
```

(`wifi` writes BSSID/PASSWORD, sets MODE STA, and schedules a restart.)

## Web console

With AP or STA up on the host, open `http://192.168.0.1/` (AP) or `http://<sta-ip>/`. Sign in with a console user (default seed `admin` / `admin` when the user store is empty). Firmware upload on **System → Update** requires an admin session; recover with USB flash if needed.

Firmware-only flash is enough for the console: HTML/CSS are embedded in the host `firmware.bin`. LittleFS remains for OTA staging and other non-UI data — do not use `uploadfs` to refresh the web UI.

Wi-Fi group (also the **WiFi** sidebar form):

| Field | Meaning |
|-------|---------|
| **BSSID** | One network name for STA join and AP advertise (default `z2m-gateway`) |
| **PASSWORD** | PSK for STA and AP. Default `00000000` |
| **DEVICE_NAME** | Wi-Fi hostname only (not the AP/STA network name) |
| **AP IP** | SoftAP address, AP mode only. Default `192.168.0.1` |
| **MODE** | `AP` (default) or `STA` |
| **OTG_ENABLED** | Stored only |

Boot: MODE AP (default) → AP named **BSSID** at **AP IP**. MODE STA → join **BSSID** for the STA join window, then AP named the same **BSSID** if the router does not answer. After that fallback the device stays AP until the next boot or Save. Join the AP with PSK `00000000` unless you changed PASSWORD.

```
permit 180
devices
map AA:BB:CC:DD:EE:FF:00:11 home/kitchen/light/state home/kitchen/light/set
```

## MQTT (base topic default `z2m`)

| Topic | Direction | Payload |
|-------|-----------|---------|
| `z2m/bridge/status` | publish | `online`, `join_open`, `join_closed` |
| `z2m/bridge/devices` | publish | JSON list of bound devices + mapped topics |
| `z2m/bridge/permit_join` | subscribe | `on`, `off`, or seconds (`180`) |
| `z2m/bridge/config/device` | subscribe | `{"ieee":"...","name":"...","state":"...","command":"...","availability":"..."}` |
| *per-device state topic* | publish | device message as received (see channels) |
| *per-device command topic* | subscribe | device command as published (see channels) |

Assign topics per IEEE. Device **channels**: `1` (default) uses those topics as today; `2`–`16` publish/command `{topic}/{ep}` except `ep=1` stays unsuffixed; `0` (multichannel with parsing) keeps the stored topics and uses payload `ch-<ep>##<message>`. The message body is passed through unchanged. Availability is always the stored topic. Unmapped devices still show up in `bridge/devices`.

## Zigbee + WiFi

The host never starts 802.15.4. Zigbee runs only on the slave and stays up in host SoftAP or STA after settings apply. Default channel is **15** (host settings). Logs: host PSRAM log ring (internal fallback), lines stamped `YYYY-MM-DD HH:MM:SS [host]|[slave]` (NTP on STA when available, else local timer). Slave lines are pushed over IRQ + SPI immediately. CLI `log` and `GET /api/log` read that ring.

## Build notes

- pioarduino `espressif32` (Arduino 3.x)
- Host: `partitions/host_s3_16MB.csv`, PSRAM (`BOARD_HAS_PSRAM`)
- Slave: `-DZIGBEE_MODE_ZCZR`, `partitions/zigbee_zczr_16MB.csv` (`zb_storage`, `zb_fct`, dual OTA app, LittleFS)
