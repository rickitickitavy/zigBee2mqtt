# Tasks

## 1. Bounded per-request HTTP body

- [x] 1.1 Replace shared `WebConsole::requestBody` / per-char `appendRequestBody` with a request-isolated collector and named size caps (small JSON vs restore/export) — verify oversized POST returns an error without applying settings and concurrent POSTs do not share one body
- [x] 1.2 Wire all JSON POST routes to the new collector and reject overflow before handlers parse — verify login, wifi/mqtt/zigbee save, and devices restore still succeed with normal-sized bodies on `esp32-s3-host`

## 2. Large JSON without unbounded String builders

- [x] 2.1 Add a reusable device-list JSON builder (fixed/PSRAM scratch or chunked response) used by host paths that currently `String +=` large lists — verify Devices table and settings export still load/parse in the console
- [x] 2.2 Point slave `ZigbeeCoordinator::devicesJson` and host `ZigbeeSpiProxy::devicesJson` (and MQTT bridge devices list if it duplicates the same shape) at that helper — verify field set matches for the same registry snapshot and `rg "String Zigbee.*::devicesJson"` no longer has two divergent implementations

## 3. Cheap Logger API

- [x] 3.1 Change `Logger::{error,warning,info,debug}` to `const char*` / `const String&` and check `logLevel` before any allocation or ring write — verify filtered-out debug calls do not grow heap and existing call sites still compile on both envs

## 4. Non-blocking STA scan and MQTT connect

- [x] 4.1 Move `WiFiController` STA scan/join off a long synchronous block on the Arduino loop (async scan state machine or worker task) — verify `GET /api/status` answers within a few seconds while STA is rescanning
- [x] 4.2 Drive `MqttClient::reconnect` / `connect` so it does not stall the loop for the full TCP timeout (worker or sliced connect); keep backoff — verify Status/HTTP still answers within a few seconds while the broker is down and reconnect is active

## 5. Timed Zigbee lock on ZCL send

- [x] 5.1 Replace `esp_zb_lock_acquire(portMAX_DELAY)` on the slave cluster-command send path with a finite timeout and fail-soft return — verify a successful send still works when the lock is free, and a busy lock fails that send without stopping the coordinator (`esp32-c6-slave`)

## 6. Deduplicate docs and role README

- [x] 6.1 Rewrite root `README.md` hardware/role/flash sections for ESP32-S3 host + ESP32-C6 slave, two images / joined package, no GPIO15 role strap — verify README no longer claims one shared C6 binary or GPIO15 role select and matches `board-role` / `pins.h`

## 7. Integration

- [x] 7.1 While applying, bump `FIRMWARE_VERSION` build by 1 from the propose value `1.0.10` if source changed; `pio run -e esp32-s3-host -e esp32-c6-slave` succeeds — verify version string and both images build; do not edit fragile OTA/SPI paths
