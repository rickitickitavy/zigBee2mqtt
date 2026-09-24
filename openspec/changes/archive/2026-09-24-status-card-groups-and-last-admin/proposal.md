## Why

Status is a stack of full-width stacked fields, so related live values are hard to scan and online time never shows days. A blocked last `isAdmin` can still lock the console: delete/demote already guards “last admin,” but block and restore still count blocked admins as enough.

## What Changes

- Rebuild the Status section as compact groups (not full-width cards). Attribute name and value stay on one row; values in a group align right.
- Status order: **online time** (single row, includes days), then **Version** (master, slave), then **Devices** (device count, online count, packets sent, packets received), then **MQTT** (local status, active topics count, connections count including remote clients), then **Wi-Fi** (mode, BSSID, signal strength).
- Extend `GET /api/status` with the MQTT and Wi-Fi live fields needed for those groups. Status stays read-only (no settings forms).
- Remove the duplicate inner **Log** heading on System → Log (the tab already says Log).
- Security: after every user-table mutation (create/update/delete, including block and settings restore), **at least one unlocked `isAdmin` MUST remain**. The host rejects the write; the Security dialog shows the error. Record this as ADR `0010` (and tighten `0001` if needed).
- Firmware build **0.2.21** (bumped at propose).

## Capabilities

### New Capabilities

- None.

### Modified Capabilities

- `web-console`: Status groups, online-time format, `/api/status` fields, System → Log heading.
- `console-users`: last unlocked admin invariant on all user writes and restore.

## Impact

- Host: `hostGatewayStatusJson` / `GET /api/status`, `UserStore` last-unlocked-admin checks (update, delete, `replaceFromExportJson`), `MqttBroker` / `MqttClient` / `WiFiController` readouts for status JSON.
- Console: `data/index.html` Status markup and `formatOnlineTime`; `data/css/all.css` status group rows; System → Log card title; Security save error when the last unlocked admin would disappear.
- `docs/adr/0010` last unlocked admin; firmware `FIRMWARE_VERSION` `0.2.21`.
