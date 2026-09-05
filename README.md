# ESP32-C6 Zigbee–MQTT gateway

PlatformIO / Arduino firmware for **ESP32-C6-DevKitC-1 N16** (16 MB). The chip is a Zigbee coordinator on its native 802.15.4 radio and publishes On/Off state to **per-device MQTT topics**.

This is not a port of the Node.js Zigbee2MQTT converter database.

## Hardware / USB

- Use the **native USB-C** port on the DevKit (USB Serial/JTAG CDC).
- ESP32-C6 does **not** implement S2/S3-style USB OTG host/HID. CDC is for flash, monitor, and the USB CLI.
- Hold **BOOT (GPIO9)** LOW at reset to force SoftAP (`z2m-gateway-AP` / `00000000`, 192.168.0.1). GPIO8 is the RGB LED / strapping pin — not used as the AP button.

## CLion

1. Install the PlatformIO plugin.
2. Open this folder. `pio project init --ide clion` generates CMake files if they are missing.
3. Build: `pio run`. Upload: `pio run -t upload`. Monitor: `pio device monitor` (115200).

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

(`wifi` schedules a restart.)

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
| *per-device state topic* | publish | `ON` / `OFF` |
| *per-device command topic* | subscribe | `on` / `off` / `toggle` |

Assign topics per IEEE. Unmapped devices still show up in `bridge/devices`.

## Zigbee + WiFi

Both use 2.4 GHz. Default Zigbee channel is **15**. Change with `channel 20` (restarts). Keep MQTT traffic modest.

## Build notes

- pioarduino `espressif32` (Arduino 3.x), `-DZIGBEE_MODE_ZCZR`
- Custom table: `partitions/zigbee_zczr_16MB.csv` (`zb_storage`, `zb_fct`, dual OTA app, LittleFS)
