## 0. Version

- [x] 0.1 Confirm `FIRMWARE_VERSION` is `0.2.21` in `Defines.h` (bumped at propose)

## 1. Status API

- [x] 1.1 Add `MqttBroker` connection count (PicoMQTT `clients.size()` while listening, else 0) and verify a listening broker with the host client plus one LAN client reports 2
- [x] 1.2 Count unique registered-device command + state topics for `mqttTopicCount` and verify three devices with both topics yield 6
- [x] 1.3 Extend `hostGatewayStatusJson` with `mqttLocalStatus` (`unused`/`down`/`listening`), `mqttTopicCount`, `mqttConnectionCount`, `wifiMode`, `wifiBssid` (stored network name), and `wifiRssiDbm` (STA RSSI or null), and verify `GET /api/status` matches live Wi-Fi and MQTT and still includes `onlineSec`, versions, device and packet counters

## 2. Status and Log UI

- [x] 2.1 Replace Status stacked fields with online time (one row, `Dd Hh Mm Ss`) plus Version, Devices, MQTT, and WiFi groups (name and value on one row, values right-aligned, group width not full panel), and verify Status poll fills every attribute and groups stay narrower than the main panel
- [x] 2.2 Remove the inner System → Log card title and verify the tab still says Log and the viewer, Refresh, and Download remain
- [x] 2.3 Upload/serve the new LittleFS files and verify Status and Log after a filesystem update (or host rebuild that packs `data/`)

## 3. Last unlocked admin

- [x] 3.1 Change `UserStore` last-admin checks to `unlockedAdminCount` (`isAdmin && !isBlocked`) on update, delete, and `replaceFromExportJson`, and verify blocking or deleting the only unlocked admin is rejected while a second unlocked admin can still be blocked
- [x] 3.2 Reject settings restore whose users list has no unlocked `isAdmin`, and verify the current table is unchanged
- [x] 3.3 Show a Security dialog error when the host rejects a last-unlocked-admin write, and verify the table still shows that admin unblocked

## 4. ADRs

- [x] 4.1 Add `docs/adr/0010-last-unlocked-admin.md` and add one sentence in `0001` pointing at it, and verify both files state at least one unlocked `isAdmin` must remain

## 5. Build

- [x] 5.1 `pio run` succeeds for host and slave with `0.2.21`
