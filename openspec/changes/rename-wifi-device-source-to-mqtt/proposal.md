# Proposal

## Why

Registered devices use a transport / device-source field labeled `wifi`, but those devices are MQTT-bound endpoints, not Wi‑Fi radio peers of the gateway. The wrong name confuses operators and mixes with the gateway’s own Wi‑Fi settings. Rename the device source to `mqtt` in code and UI now; the operator will re-import device lists with the new id later (no in-firmware migration).

## What Changes

- Rename device transport id `wifi` → `mqtt` everywhere it means “device source” (JSON, APIs, console labels, C++ enum/helpers).
- Rename operator-facing strings such as **WIFI DEVICE** / “Wi‑Fi” transport to **MQTT DEVICE** / “MQTT”.
- Rename `DeviceTransportWifi` → `DeviceTransportMqtt` (keep numeric value `1` so SPI/binary layouts that store the enum byte stay stable for in-RAM sync).
- **BREAKING:** Persisted device JSON that still has `"transport":"wifi"` is **not** remapped. After flash, those records are treated as non-MQTT (same as unknown/`zigbee` fallback) until the operator restores a list with `"transport":"mqtt"`.
- Gateway Wi‑Fi settings (MODE, BSSID, `/api/wifi`, sidebar WiFi) stay named Wi‑Fi — out of scope.
- Firmware `FIRMWARE_VERSION` build is `1.0.7` for this proposal.

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `wifi-mqtt-devices`: transport `wifi` → `mqtt`; wording for MQTT devices (capability path kept).
- `web-console`: Add-device **MQTT DEVICE** control; parameter dialog shows MQTT; type editable for `mqtt` devices.
- `mqtt-device-topics`: Manual command / subscriptions / born collect refer to `mqtt` transport devices.
- `device-registry`: Stored transport values include `mqtt` instead of `wifi`.

## Impact

- `include/GlobalSettings.h` — enum + JSON id helpers
- `src/DeviceTopicMap.cpp`, `src/MqttClient.cpp`, `src/WebConsole.cpp`, `src/main.cpp`, `src/ZigbeeSpiProxy.cpp`, `src/ZigbeeCoordinator.cpp` — transport checks
- `data/index.html` — Devices UI labels and `transport: "mqtt"`
- OpenSpec main specs listed above (on sync/archive)
- No automatic rewrite of LittleFS `/devices.json` or settings export files
