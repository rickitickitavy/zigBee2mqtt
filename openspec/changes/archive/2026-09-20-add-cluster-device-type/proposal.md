# Proposal

## Why

Pairing and device settings show IEEE plus manufacturer/model when the slave happens to have them, but not a stable, readable **device type**. Operators cannot tell a switch from a leak sensor at a glance, and later type-specific actions have nothing to key off. Zigbee already advertises **in-clusters** on each endpoint; that is the right source of type, not a guess from the friendly name.

## What Changes

- On join, the slave reads the device’s simple descriptor (in-clusters) and classifies a single **device type** for that IEEE.
- The join event to the host carries that type. The pairing found table shows a readable type (in addition to manufacturer/model).
- Saving a found device stores the type on the host registered record. Device settings show the type as **readonly** (same treatment as IEEE). Later saves MUST NOT let the operator change it.
- Existing registered devices without a type stay `unknown` until the slave classifies them (for example on a later join). The host MAY fill `unknown` from a new classification; it MUST NOT overwrite a known type.
- Type-specific MQTT or radio **actions are out of scope**. This change only classifies, displays, and persists the type so later work can use it.

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `zigbee-slave-radio`: classify a device type from advertised in-clusters when a device joins or is identified.
- `host-slave-spi`: `SpiEvtDeviceJoin` includes the classified type.
- `device-registry`: persist device type on the host registered record; keep it operator-immutable after it is known.
- `web-console`: show readable type in pairing search and as a readonly field in device parameters.

## Impact

- Slave: simple-descriptor / cluster inspection at join; extend `enqueueDeviceJoin` payload.
- Host: `FoundDeviceList`, `DeviceTopicEntry` + LittleFS `devices.json`, `/api/devices` and `/api/devices/found`.
- Console: found table Type column; parameter dialog readonly TYPE.
- SPI join frames grow by one type byte (length 76). Host and slave must be flashed together. Device sync (`SpiCmdSetDevice` / dump) also carries type so a reboot pull cannot wipe it.
- No MQTT topic or command-behavior change.
