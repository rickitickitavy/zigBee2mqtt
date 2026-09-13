## Why

The Devices card is still a placeholder. Operators can only register Zigbee endpoints through MQTT `config/device`, and the host already owns `DeviceTopicEntry` in EEPROM. Pairing from the browser needs a search-and-add flow, a durable device list on the Wi-Fi host, and EEPROM writes that do not rewrite the whole 4 KiB blob on every save.

## What Changes

- **BREAKING** (settings layout, version bump): split storage so wifi/mqtt/zigbee (and a 256-byte reserved tail, 128-byte aligned) sit in the **EEPROM main** block; the **device list** (128 slots) starts after that reserve in a separate host file. Existing v4 EEPROM device slots migrate on first boot.
- Registered device records stay **only on the host**. The slave still reports joins over SPI; it does not store the operator map.
- Saving settings writes **only the EEPROM bytes that changed** (at least the dirty region / slot), not the entire settings image.
- Devices web card: table of registered devices plus **Add device**.
- Add-device search dialog: **Search** opens pairing on the slave; found-but-unregistered devices appear in the table in real time. Operator selects one and presses **Add**.
- Parameter dialog: fill friendly name and MQTT topics for that IEEE, **Save** persists one device slot and the row appears in the Devices table. Cancel leaves the map unchanged.
- Search dialog closes when Add is pressed or the operator dismisses it; pairing window ends when the search dialog closes or the join timer expires.

## Capabilities

### New Capabilities

- `device-registry`: host-owned registered-device list, EEPROM main-block reserve + device-list placement, partial flash writes, in-memory found-device list, and pairing search/add save rules

### Modified Capabilities

- `web-console`: Devices section is no longer a placeholder; it presents the registered table and the two add-device dialogs

## Impact

- `GlobalSettings`, `SettingsManager` EEPROM read/write/migrate, `DeviceTopicMap`
- Host: RAM list of recent `SpiEvtDeviceJoin` frames; `WebConsole` APIs for registered devices, found devices, start/stop search (permit join)
- Slave: existing permit-join + join events (no new device store)
- `data/index.html` Devices card and dialogs
- MQTT `config/device` remains valid; web Save uses the same host map
