## Why

Registered devices can only be commanded as on/off/toggle. Inbound Zigbee reports already expose cluster, attribute, and value (`cl`, `attr`, `val`), but MQTT command topics cannot send those fields back. Operators need a per-device full-control mode that treats the command payload as those attributes and transmits them as a Zigbee write.

## What Changes

- Add a per-device **FULL CONTROL** setting. Default **off** (existing on/off/toggle command behavior). Persist it with the registered device record. Expose it on the Devices parameter dialog as the **last** field, and on MQTT `config/device`.
- When FULL CONTROL is **off**, command handling stays as today: channels mapping, then on/off/toggle text to `SpiCmdZclOnOff`.
- When FULL CONTROL is **on**, after channels mapping the host SHALL parse the command body for message attributes (`cl`, `attr`, `val`, `ep`, `type`, and further recognized keys). Any recognized attribute that is absent SHALL use a default. If no attributes are parsed, the host SHALL use the entire remaining body as the command payload and SHALL apply defaults for the other attributes. If at least one attribute is parsed, the host SHALL send a Zigbee write-attribute with parsed values plus defaults for the rest.
- Payload grammar SHALL match inbound report text (`cl=0x0006,attr=0x0000,val=0x1`) so a reported message can be sent back. Decimal and `0x` hex SHALL both be accepted.
- The slave SHALL execute a host-supplied write-attribute radio command. It SHALL NOT read the FULL CONTROL flag; the host owns that product rule.

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `mqtt-device-topics`: full-control command bodies parse attributes with defaults; an unparsed body is sent as the whole payload
- `device-registry`: registered records store FULL CONTROL (default off)
- `web-console`: Devices parameter dialog includes the FULL CONTROL checkbox
- `host-slave-spi`: host-to-slave device command path carries a structured write-attribute request (IEEE, endpoint, cluster, attribute, type, value)

## Impact

- `DeviceTopicEntry`, LittleFS device JSON, web `POST /api/devices`, MQTT `config/device`
- Host MQTT command routing in `main` after channels mapping
- New or extended SPI command (beyond `SpiCmdZclOnOff` text)
- Slave `ZigbeeCoordinator` ZCL write-attribute transmit
- Web console Devices parameter form (`data/index.html`)
