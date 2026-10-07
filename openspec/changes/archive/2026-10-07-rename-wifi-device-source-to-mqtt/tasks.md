# Tasks

## 1. Transport enum and JSON id

- [x] 1.1 Rename `DeviceTransportWifi` → `DeviceTransportMqtt` (value still `1`) and change JSON helpers so id `"mqtt"` maps to MQTT and `"wifi"` does **not** — verify `deviceTransportJsonId(DeviceTransportMqtt) == "mqtt"` and `deviceTransportFromJsonId("wifi")` returns Zigbee fallback
- [x] 1.2 Replace all C++ uses of `DeviceTransportWifi` with `DeviceTransportMqtt` across host/slave sources that branch on transport — verify `rg DeviceTransportWifi` is empty and `pio run -e esp32-s3-host -e esp32-c6-slave` succeeds

## 2. Web console API and UI

- [x] 2.1 Update Devices create/edit paths in `WebConsole` (messages, transport parse) for `mqtt` — verify POST create with `"transport":"mqtt"` persists and `"transport":"wifi"` does not create an MQTT device
- [x] 2.2 Rename console copy and JS: **MQTT DEVICE**, transport label MQTT, `transport: "mqtt"` in `data/index.html` — verify Add device dialog shows MQTT DEVICE and saved list JSON has `"transport":"mqtt"`

## 3. Integration check

- [x] 3.1 Confirm `FIRMWARE_VERSION` is `1.0.8` and dual-env build succeeds; smoke that a new MQTT device stays off the slave registry and a Zigbee device is unchanged — verify no Wi‑Fi settings tab strings were renamed
