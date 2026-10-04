# Tasks

## 1. Data model and settings

- [x] 1.1 Add `transport` to `DeviceTopicEntry` (`zigbee`/`wifi`), defaults, JSON/list serialization in `DeviceTopicMap`, and legacy load → `zigbee`; verify a saved Zigbee device round-trips `transport` in list JSON and after reboot
- [x] 1.2 Add `serverBornTopic` and `bornIntervalMin` to `MqttSettings` with defaults (empty, 20), clamp 5–120, bump settings version/migrate; verify GET/POST `/api/mqtt` and settings export/restore include both fields
- [x] 1.3 Accept `transport` and Wi‑Fi `type` rules on device save API (create locks transport; Zigbee type immutable when known; Wi‑Fi type editable); verify rejected transport change and accepted Wi‑Fi type change

## 2. MQTT listen, commands, and born

- [x] 2.1 Subscribe to `wifi` state topics (and suffix `/+` when needed), resolve inbound state into runtime status/telemetry; verify a published MQTT state updates `/api/devices` status for that Wi‑Fi IEEE
- [x] 2.2 Route Manual command / MQTT set / type-actions for `wifi` to MQTT command publish only (no SPI); verify ON for a Wi‑Fi device hits the command topic and never calls Zigbee on/off
- [x] 2.3 Skip slave upsert/dump/delete for `wifi` records; verify saving or deleting a Wi‑Fi device does not enqueue slave registry frames
- [x] 2.4 Subscribe to server-born topic when non-empty; on any message run Zigbee-only status collect (same as boot); verify a born message triggers reads for Zigbee devices only
- [x] 2.5 Publish born announce (`online`) once ≥2 minutes after boot when topic set and MQTT up, then every `bornIntervalMin`; verify first fire after 2 minutes and a second fire after the interval (or accelerated test hook)

## 3. Web console

- [x] 3.1 Add WIFI DEVICE on the search dialog that opens parameter dialog with transport locked to Wi‑Fi and editable IEEE; verify create+save shows a `wifi` row in the Devices table
- [x] 3.2 Show readonly transport immediately before NAME; keep Zigbee cluster TYPE readonly; make Wi‑Fi cluster TYPE a select and persist it; verify field order and edit rules on Zigbee vs Wi‑Fi dialogs
- [x] 3.3 Add SERVER BORN TOPIC and BORN INTERVAL fields on the MQTT card wired to `/api/mqtt`; verify load/save matches stored settings

## 4. Integration checks

- [x] 4.1 End-to-end: register one Zigbee and one Wi‑Fi device, confirm Zigbee still publishes state / accepts set, Wi‑Fi status updates from MQTT state, and born listen/announce behave with a configured topic
- [x] 4.2 Confirm firmware reports `0.3.7` or higher build after apply bump if source changed again during apply
