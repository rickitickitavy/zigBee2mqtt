## Why

The Devices and Status console still force extra clicks, hide restore/export on a System sub-tab, leave pairing Search stuck after the slave join window ends, and show no radio or firmware summary. Operators need faster device edit, slave-format backup on the Devices page, Search tied to real pairing, last-packet signal, and a Status card with counts, traffic, and version.

## What Changes

- Changing NAME in the device parameter dialog SHALL rewrite state, command, and availability topics to the default `{base}/{slug}/…` pattern for that name (including when editing an existing device).
- Double-clicking a registered-device table row SHALL open that device’s edit dialog.
- Devices SHALL gain Export (download the slave `devices.json` store) and Restore (same file restore flow as System → devices.json) under the table.
- System inner tabs SHALL be Log, devices.json, Update, Hardware in that order; Log is the default System panel.
- Opening the pairing dialog SHALL NOT start a slave join window. Search SHALL start pairing only on Search click. Search SHALL stay disabled until the slave join window actually ends (or the operator closes the dialog).
- The Devices table SHALL show last received signal strength when the host has a value from the last inbound Zigbee packet for that IEEE.
- Status SHALL show registered-device count, online count, packets received, packets sent, and firmware version (no longer an empty placeholder).

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `web-console`: Devices table/actions, pairing Search vs slave join, System tab order, Status summary
- `host-slave-spi`: last-packet RSSI on inbound reports; join-window-closed event so Search can re-enable

## Impact

Host web console (`data/index.html`, `WebConsole.cpp`), device list JSON, host SPI receive path and slave ATTR_REPORT / join-close signaling, Status RGB pairing already tracks join window on the slave. MQTT topic strings change when the operator edits NAME in the dialog; the store is still one-record CRUD to the slave on Save.
