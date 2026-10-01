# ESP32-C6 Zigbee–MQTT gateway

PlatformIO / Arduino firmware for **two ESP32-C6-DevKitC-1 N16** boards (same `.bin`). GPIO15 selects the role: **LOW = Wi-Fi host**, **HIGH = Zigbee slave**. The host publishes On/Off state to **per-device MQTT topics**. The slave runs the Zigbee coordinator after the host pushes settings.

This is not a port of the Node.js Zigbee2MQTT converter database.

## Hardware / USB

- Use the **native USB-C** port on the DevKit (USB Serial/JTAG CDC).
- ESP32-C6 does **not** implement S2/S3-style USB OTG host/HID. CDC is for flash, monitor, and the USB CLI.
- Hold **BOOT (GPIO9)** LOW at reset on the **host** to force AP for that boot (does not change stored MODE). GPIO8 onboard RGB is unused for status.
- **GPIO15** is a C6 strapping pin: hold host to **GND** and slave to **3.3 V** through reset.

### Status LEDs (six reds)

External red LEDs (GPIO HIGH = on): LED1 GPIO18, LED2 GPIO19, LED3 GPIO20, LED4 GPIO21, LED5 GPIO2, LED6 GPIO3. Host **LED3** and **LED6** stay off.

| Indicator | Host | Slave |
|-----------|------|--------|
| LED1 | 0.1 s: MQTT device command received | 0.1 s: packet from an **unknown** device |
| LED2 | 0.1 s: MQTT device state published | 0.1 s: packet from a **known** device |
| LED3 | Unused | 0.1 s: radio ACK that our command was delivered |
| LED4 | On while the local MQTT broker is listening | 0.1 s: **only** command actually sent on the radio |
| LED5 | Solid while MQTT is connected; blinks (0.25 s) for boot or lost slave | Solid when ready; blinks (0.25 s) for boot/critical |
| LED6 | Unused | Blinks while pairing/join is open |

### Two-board wiring (3.3 V, common GND)

| Signal | Host | Slave |
|--------|------|--------|
| ROLE | GPIO15 → GND | GPIO15 → 3.3 V |
| RST | GPIO11 | slave **EN** (active LOW pulse) |
| SCK | GPIO6 | GPIO6 |
| MOSI | GPIO5 | GPIO5 |
| MISO | GPIO4 | GPIO4 |
| CS | GPIO7 | GPIO7 |
| IRQ | GPIO10 in | GPIO10 out (HIGH = slave has a frame) |

Host→slave uses CS + SCK + MOSI (no IRQ). Slave→host (Zigbee events and logs) uses the same SPI plus IRQ. Flash the same firmware on both chips.

## CLion

1. Install the PlatformIO plugin.
2. Open this folder. `pio project init --ide clion` generates CMake files if they are missing.
3. Build: `pio run`. Upload: `pio run -t upload`. Filesystem (web UI): `pio run -t uploadfs`. Monitor: `pio device monitor` (115200).

First Zigbee flash: erase recommended so `zb_storage` is clean:

```bash
pio run -t erase
pio run -t upload
```

## Configure over USB CLI

Type `help`. Typical first run:

```
wifi MySsid MyPassword
mqtt 192.168.1.10 1883
save
```

(`wifi` writes BSSID/PASSWORD, sets MODE STA, and schedules a restart.)

## Web console

With AP or STA up, open `http://192.168.0.1/` (AP) or `http://<sta-ip>/`. There is **no HTTP password**. Firmware upload on **System → Update** is unauthenticated; recover with USB flash if needed.

Flash `data/` after firmware so `/index.html` and `/css/all.css` exist (`pio run -t uploadfs`). Firmware-only flash still serves a short “filesystem missing” page.

Wi-Fi group (also the **WiFi** sidebar form):

| Field | Meaning |
|-------|---------|
| **BSSID** | One network name for STA join and AP advertise (default `z2m-gateway`) |
| **PASSWORD** | PSK for STA and AP. Default `00000000` |
| **DEVICE_NAME** | Wi-Fi hostname only (not the AP/STA network name) |
| **AP IP** | SoftAP address, AP mode only. Default `192.168.0.1` |
| **MODE** | `AP` (default) or `STA` |
| **OTG_ENABLED** | Stored only (ESP32-C6 has no USB OTG) |

Boot: MODE AP (default) → AP named **BSSID** at **AP IP**. MODE STA → join **BSSID** for **5 seconds**, then AP named the same **BSSID** if the router does not answer. After that fallback the device stays AP until the next boot or Save. Join the AP with PSK `00000000` unless you changed PASSWORD.

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

The host never starts 802.15.4. Zigbee runs only on the slave and stays up in host SoftAP or STA after `SET_SETTINGS`. Default channel is **15** (host settings). Logs: one 64 KiB ring on the host, lines stamped `YYYY-MM-DD HH:MM:SS [host]|[slave]` (NTP on STA when available, else local timer). Slave lines are pushed over IRQ + SPI immediately. CLI `log` and `GET /api/log` read that ring.

## Build notes

- pioarduino `espressif32` (Arduino 3.x), `-DZIGBEE_MODE_ZCZR`
- Custom table: `partitions/zigbee_zczr_16MB.csv` (`zb_storage`, `zb_fct`, dual OTA app, LittleFS)
