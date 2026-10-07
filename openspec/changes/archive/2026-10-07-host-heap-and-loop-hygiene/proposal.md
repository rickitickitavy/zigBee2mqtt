# Proposal

## Why

Long-uptime host (ESP32-S3) and slave (ESP32-C6) firmware still use patterns that fragment heap and stall the Arduino loop: a shared unbounded HTTP body buffer, large Arduino `String` JSON builders, Logger copies by value, blocking Wi‑Fi scan / MQTT connect on the loop task, one Zigbee lock taken with `portMAX_DELAY`, and duplicated device-list JSON plus a README that still describes the obsolete dual-C6 / GPIO15 role model. Fixing these reduces brownouts, silent request corruption, and wedged radio/control paths without changing the SPI OTA contract.

## What Changes

- Bound and isolate HTTP POST body collection on the host web console (no shared per-char `String` append; reject oversized bodies).
- Build large device/settings/status JSON without unbounded Arduino `String` growth (stream or fixed/PSRAM scratch on the host).
- Make Logger cheap: take `const char*` / `const String&`, check log level before allocating.
- Keep the host loop responsive during STA scan/join and MQTT `connect` (non-blocking or worker-task isolation).
- Replace `esp_zb_lock_acquire(portMAX_DELAY)` on the slave ZCL send path with a timed lock and fail/retry.
- Deduplicate host/slave `devicesJson` helpers; refresh `README.md` to the ESP32-S3 host + ESP32-C6 slave, compile-time role model.
- Firmware `FIRMWARE_VERSION` build is `1.0.10` for this proposal.
- **Non-goals:** No changes to fragile zones `slave-spi-ota` / `firmware-update-process` (SPI OTA begin/chunk/end timing, joined OTA orchestration). No ESP-IDF framework migration.

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `web-console`: Bounded, request-safe HTTP bodies; large JSON APIs without unbounded `String` growth; console / SPI keepalive remain usable while STA scan or MQTT reconnect runs.
- `zigbee-slave-radio`: Device-control ZCL send MUST NOT take the Zigbee lock with an infinite wait.
- `board-role`: Operator-facing project README MUST describe S3 host + C6 slave images and MUST NOT instruct GPIO15 role selection or a single shared C6 binary.

## Impact

- `src/WebConsole.cpp`, `include/WebConsole.h` — body collection and JSON responses
- `src/DeviceTopicMap.cpp`, `src/ZigbeeCoordinator.cpp`, `src/ZigbeeSpiProxy.cpp`, `src/main.cpp` — list/status JSON
- `include/Logger.h`, `src/Logger.cpp` — log API
- `src/WiFiController.cpp`, `src/MqttClient.cpp` — scan/connect scheduling
- `src/ZigbeeCoordinator.cpp` — timed `esp_zb_lock_acquire`
- `README.md` — hardware/role documentation
- OpenSpec deltas for `web-console`, `zigbee-slave-radio`, `board-role`
- Fragile OTA/SPI paths stay untouched
