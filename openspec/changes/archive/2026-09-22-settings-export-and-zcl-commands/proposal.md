## Why

Operators cannot copy MQTT, Zigbee, hardware, and the device list to another gateway without also copying Wi-Fi. Many devices (window covering `0x0102` in particular) only move when they receive a ZCL **cluster command**, not an attribute write or on/off.

## What Changes

- System → Maintenance gets stacked **Export settings** and **Restore settings** controls that are compact (`btn-inline`), not full-width bars.
- Export downloads one JSON file with MQTT, Zigbee, hardware (SPI speed), and the persisted device list. The file MUST NOT include Wi-Fi (BSSID, password, MODE, AP IP, hostname, OTG).
- Restore applies that file after confirm: MQTT, Zigbee, hardware, and devices replace those groups. Wi-Fi is left unchanged even if a `wifi` object is present in the file.
- Every registered device’s existing MQTT command (`set`) topic, and Manual command, SHALL accept ZCL cluster commands in addition to today’s on/off and FULL CONTROL write-attribute bodies. Suffix and parse channel mapping stay as they are. Window covering shortcuts (`OPEN`/`UP`, `CLOSE`/`DOWN`, `STOP`) SHALL send cluster `0x0102` move commands. Generic `cl=` / `cmd=` SHALL work for any device type.
- Firmware version build is `0.2.8`.

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `web-console`: Maintenance export/restore; compact buttons; confirm before restore.
- `mqtt-device-topics`: Command topic and Manual command send ZCL cluster commands using existing channel naming.
- `host-slave-spi`: Host-to-slave ZCL cluster-command frame (IEEE, endpoint, cluster, command id, optional payload).
- `zigbee-slave-radio`: Slave transmits that cluster-specific ZCL command to the given endpoint.

## Impact

Host `WebConsole` (Maintenance, new settings JSON routes), `SettingsManager` / device store (export omit Wi-Fi; restore skip Wi-Fi), MQTT apply path and `POST /api/devices/command`, SPI protocol (new command after `0x0F`), slave `ZigbeeCoordinator` send path. Devices Export/Restore on the Devices page stay. No new persisted device field. MQTT broker is not required for Manual command.
