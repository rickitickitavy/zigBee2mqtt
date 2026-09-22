## 0. Firmware version

- [x] 0.1 Increase `FIRMWARE_VERSION` build from `0.2.5` to `0.2.6` at propose time and verify `Defines.h` shows `0.2.6`

## 1. Host telemetry cache and list JSON

- [x] 1.1 Extend the host last-seen cache with last battery percent and last status text per usable `ep`, split `BATTERY <n>` from other report text, and verify an ON on endpoint 3 then a battery report leaves endpoint 3 `ON` and battery set
- [x] 1.2 Include optional `battery` and `status: [{ep, state}]` in `GET /api/devices` list JSON only, and verify `listStoreJson` / export / devices.json omit those fields after a cache hit

## 2. SPI read-attribute

- [x] 2.1 Add `SpiCmdZclReadAttr` `0x0F` (ieee + endpoint + cluster LE + attr LE), pack/unpack helpers, and verify a 13-byte frame round-trips endpoint 2 / cluster `0x0006` / attr `0x0000`
- [x] 2.2 Handle the command on the slave with a ZCL read to the given endpoint (no bind-endpoint substitute) and verify an ON answer arrives as the existing attribute-report message `ON`

## 3. Start-time status refresh

- [x] 3.1 After coordinator start and registry ready, enqueue paced type-specific reads plus battery (`0x0001`/`0x0021` when Power Configuration is known) using `channels` for endpoint sets, and verify a 3-channel `onOff` device is read on endpoints 1–3
- [x] 3.2 Leave start successful when a device has no short address or does not answer, and verify the coordinator stays up and that IEEE stays without status until a later report

## 4. Manual command path

- [x] 4.1 Extract the MQTT device-command apply branch and call it from `POST /api/devices/command` (`ieee`, `payload`, optional `channel`) with suffix / parse / full-control mapping, and verify `ON` on a `channels` `1` device enqueues the same on/off as MQTT `set` with the broker disconnected
- [x] 4.2 Reject unknown IEEE with 404 and empty payload with 400, and verify `bridge/permit_join` is not invoked

## 5. Devices table and dialogs

- [x] 5.1 Reorder the Devices table to online, name, type, status, battery, RSSI (no IEEE column) and verify headers and cells match `GET /api/devices`
- [x] 5.2 Render `onOff` status as dark/light circles (N placeholders when `channels` is 2–16; reported `ep`s when `channels` is 0) and other types as last text, and verify a parse-mode OFF on ep 1 and ON on ep 3 shows dark then light
- [x] 5.3 Show battery as integer percent or `N/A`, and verify missing `battery` displays `N/A` and `67` displays `67`
- [x] 5.4 Open the parameter dialog on a single row click (select + edit) and verify a click opens the same dialog as Edit without requiring a double-click
- [x] 5.5 Add Manual command under the table (disabled without selection), terminal dialog with answers pane, input, Enter send, channel dropdown when `channels` is 0 or 2–16, Escape dismiss, and verify Enter on channel 3 sends `OFF` to endpoint 3 and a later report appends to the answers pane
