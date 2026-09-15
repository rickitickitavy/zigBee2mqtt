## ADDED Requirements

### Requirement: Full control parses command payload into write fields
When a registered device has FULL CONTROL on, the host SHALL apply channels mapping first, then parse the remaining command body as comma-separated `key=value` pairs. Recognized keys SHALL include `cl` (cluster id), `attr` (attribute id), `val` (value), `ep` (destination endpoint), and `type` (ZCL attribute data type). Values SHALL accept decimal or `0x` hexadecimal. Unknown keys SHALL be ignored. A key is parsed only when it appears as `key=value`. When at least one recognized key is parsed, the host SHALL send a Zigbee write-attribute using parsed values and defaults for every recognized attribute that was absent: `cl` `0x0006`, `attr` `0x0000`, `val` `0`, `ep` the endpoint from channels mapping, `type` unsigned 8-bit unless `val` needs a wider unsigned type.

#### Scenario: Report-shaped payload
- **WHEN** FULL CONTROL is on and MQTT receives `cl=0x0006,attr=0x0000,val=0x1` on that device’s command topic
- **THEN** the host sends a write-attribute for cluster `0x0006`, attribute `0x0000`, value `1` to the endpoint from channels mapping

#### Scenario: Absent attributes use defaults
- **WHEN** FULL CONTROL is on and MQTT receives `cl=0x0102,val=0x1`
- **THEN** the host sends a write-attribute for cluster `0x0102`, attribute `0x0000`, value `1`, type unsigned 8-bit, to the endpoint from channels mapping

#### Scenario: Endpoint override in payload
- **WHEN** FULL CONTROL is on, channels mapping chose endpoint 1, and the body is `cl=0x0006,attr=0x0000,val=0,ep=3`
- **THEN** the host sends the write to endpoint 3

#### Scenario: Parse mode then full control
- **WHEN** channels is `0`, FULL CONTROL is on, and MQTT receives `ch-2##cl=0x0006,attr=0x0000,val=1`
- **THEN** the host writes cluster `0x0006` attribute `0x0000` value `1` to endpoint 2

### Requirement: Full control unparsed body is the command payload
When FULL CONTROL is on and the remaining command body contains no parsed recognized attributes, the host SHALL send that entire body as the command payload (same on/off/toggle text path as today) and SHALL apply defaults for the other attributes (`ep` from channels mapping).

#### Scenario: Plain ON with full control
- **WHEN** FULL CONTROL is on and MQTT receives `ON` on that device’s command topic
- **THEN** the host sends the on/off command with payload `ON` to the endpoint from channels mapping

#### Scenario: Parse mode plain body
- **WHEN** channels is `0`, FULL CONTROL is on, and MQTT receives `ch-3##OFF`
- **THEN** the host sends payload `OFF` to endpoint 3

### Requirement: Full control off keeps on/off commands
When FULL CONTROL is off or unset, the host SHALL keep existing command behavior: channels mapping, then on/off/toggle text to the device.

#### Scenario: Default device still toggles
- **WHEN** a device was saved without FULL CONTROL and MQTT receives `toggle` on its command topic
- **THEN** the host sends the on/off toggle command as today
