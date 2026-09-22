## Why

The Devices table shows identity and RSSI but not live per-channel state or battery, so the operator cannot see what the mesh is doing without MQTT. Editing still needs a double-click, and there is no console path that sends the same payload as a device `set` topic. After a gateway start, last known states stay empty until devices happen to report.

## What Changes

- Devices table columns SHALL be, in this order: online, name, type, status, battery, RSSI. IEEE SHALL leave the table (it remains on the parameter dialog and as the row key).
- Battery SHALL show an integer percent when the host has a valid Power Configuration (`0x0001`) Battery Percentage Remaining report for that IEEE, otherwise `N/A`. Battery is runtime telemetry: it MUST NOT be stored as a device setting.
- Status SHALL show the last type-specific state. Multi-channel devices (`channels` `0` or `2`–`16`) SHALL show one indicator per channel; inbound reports SHALL bind to a channel using the packet `ep`. `onOff` SHALL use small dark (OFF) and light (ON) circles. Other types SHALL show the last report text for that endpoint.
- A single click on a registered-device row SHALL select it and open the parameter dialog (replacing double-click-to-edit).
- A Manual command button SHALL sit under the table. It SHALL open a terminal dialog for the selected device: a read-only answers pane, a command input that sends on Enter using the same host path as that device’s MQTT `set` topic, and a channel dropdown when the device has more than one channel.
- When the Zigbee coordinator starts, the slave SHALL request current status (and battery when the device advertises cluster `0x0001`) from every registered device so the table can fill without waiting for unsolicited reports.
- Firmware version build SHALL move from `0.2.5` to `0.2.6` with this change (`FIRMWARE_VERSION`).

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `web-console`: Devices table columns, live status and battery, single-click edit, Manual command dialog
- `device-registry`: registered list JSON carries runtime status and battery without persisting them
- `host-slave-spi`: last per-endpoint status and battery from reports; host-to-slave ZCL read so the coordinator can ask for current attributes
- `zigbee-slave-radio`: after coordinator start, read current status (and battery when present) for all registered devices
- `mqtt-device-topics`: Manual command uses the existing `set` mapping (suffix / parse / full control) without a second command grammar

## Impact

Host console (`data/index.html`, `data/css/all.css`, `WebConsole.cpp`), `DeviceTopicMap::listJson`, `ZigbeeSpiProxy` last-seen cache, host MQTT command path reuse for HTTP, new SPI read-attribute command, slave `ZigbeeCoordinator` read-after-start and existing `0x0001` / `0x0021` battery decode. `FIRMWARE_VERSION` is `0.2.6`. Flash host and slave together. Existing MQTT state publishes stay; battery may still publish as `BATTERY <percent>` when a report arrives.
