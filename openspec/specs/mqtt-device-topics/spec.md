# mqtt-device-topics Specification

## Purpose

Maps each registered device’s stored MQTT topics using a per-device channels setting: single-channel as today, suffix topics for extra endpoints, or one topic with a `ch-<ep>##` payload prefix.

## Requirements

### Requirement: Channels setting selects topic mapping
Each registered device SHALL store a channels value of `0` (multichannel with parsing), or `1` through `16`. The default SHALL be `1`. Value `1` SHALL use the stored state and command topics with no endpoint suffix. Values `2` through `16` SHALL use suffix mapping. Value `0` SHALL use parse mapping. Availability SHALL always use the stored availability topic with no suffix and no payload prefix.

#### Scenario: Default single channel
- **WHEN** a device is saved without a channels value
- **THEN** channels is `1` and state publishes go to the stored state topic

#### Scenario: Availability unchanged
- **WHEN** a registered device has stored availability `z2m/switcher_1/availability`
- **THEN** availability traffic uses that topic exactly

### Requirement: Suffix mode uses endpoint in the topic except ep 1
When channels is `2` through `16` and the host publishes or receives device traffic, endpoint `1` SHALL use the stored topic with no suffix. A usable endpoint other than `1` SHALL use `{storedTopic}/{ep}`. Endpoint `255` SHALL NOT be used as a topic suffix. The payload body SHALL be the device message as received or as published, not restricted to `ON` or `OFF`.

#### Scenario: Gang 3 in suffix mode
- **WHEN** a device with channels `4` and stored state `z2m/switcher_1/state` reports `ON` from endpoint 3
- **THEN** the host publishes `ON` to `z2m/switcher_1/state/3`

#### Scenario: Gang 1 stays unsuffixed
- **WHEN** that same device reports `OFF` from endpoint 1
- **THEN** the host publishes `OFF` to `z2m/switcher_1/state`

#### Scenario: Command suffix
- **WHEN** MQTT receives any payload on `z2m/switcher_1/set/2` for that device
- **THEN** the host sends that payload to IEEE endpoint 2

### Requirement: Parse mode prefixes the payload with the endpoint
When channels is `0`, the host SHALL publish and subscribe on the stored state and command topics with no suffix. An inbound device message SHALL be published as `ch-<ep>##` followed by the original message. An outbound MQTT payload `ch-<ep>##<message>` SHALL send `<message>` to that endpoint. The host SHALL NOT require the message body to be `ON` or `OFF`.

#### Scenario: Parse publish
- **WHEN** a parse-mode device reports `LEAK` from endpoint 3
- **THEN** the host publishes `ch-3##LEAK` to the stored state topic

#### Scenario: Parse command
- **WHEN** MQTT receives `ch-3##OPEN` on the stored command topic
- **THEN** the host sends `OPEN` to endpoint 3

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

### Requirement: Command topic sends ZCL cluster commands
After channels mapping, a body on a registered device’s MQTT command (`set`) topic SHALL be applied in this order: FULL CONTROL write-attribute when that parse succeeds; else a ZCL cluster command when the body is a covering shortcut or contains `cl` and `cmd`; else today’s on/off/toggle text. Shortcuts SHALL be case-insensitive: `OPEN` and `UP` send cluster `0x0102` command `0x00`; `CLOSE` and `DOWN` send `0x0102` command `0x01`; `STOP` sends `0x0102` command `0x02`. Generic bodies SHALL accept `cl` (cluster id) and `cmd` (ZCL command id) as decimal or `0x` hex, optional `payload` bytes, and SHALL work for every registered device type. Suffix and parse channel mapping SHALL stay as they are, including suffix on the first channel when channels is `2`–`16`. Gateway topics SHALL NOT accept this path.

#### Scenario: Covering open
- **WHEN** MQTT receives `OPEN` on a `windowCovering` device’s command topic with channels `1`
- **THEN** the host sends a ZCL cluster command for cluster `0x0102` command `0x00` to the mapped endpoint

#### Scenario: Generic command any type
- **WHEN** MQTT receives `cl=0x0102,cmd=0x02` on an `onOff` device’s command topic
- **THEN** the host sends cluster `0x0102` command `0x02` to the mapped endpoint

#### Scenario: Suffix first channel
- **WHEN** a device has channels `2` and MQTT receives `STOP` on `{commandTopic}/1`
- **THEN** the host sends cluster `0x0102` command `0x02` to endpoint 1

#### Scenario: On/off unchanged
- **WHEN** FULL CONTROL is off and MQTT receives `ON` on a `channels` `1` device
- **THEN** the host still sends the on/off command with payload `ON`

### Requirement: Console manual command uses the set-topic path
A Manual command body for a registered device SHALL be applied with the same channels mapping, FULL CONTROL, ZCL cluster-command, and on/off rules as a payload received on that device’s MQTT command (`set`) topic. Suffix mode SHALL send the body to the chosen endpoint. Parse mode SHALL treat the chosen channel as `ch-<ep>##` wrapping. Single-channel SHALL send the body to the mapped endpoint as today. The host SHALL NOT require the MQTT broker to be connected. Gateway topics (`bridge/permit_join`, `bridge/config/device`) SHALL NOT accept this path.

#### Scenario: Same as MQTT ON
- **WHEN** FULL CONTROL is off and Manual command sends `ON` for a `channels` `1` device
- **THEN** the host sends the on/off command with payload `ON` to the endpoint from channels mapping

#### Scenario: Parse mode channel
- **WHEN** channels is `0` and Manual command sends `OPEN` with channel 3 selected
- **THEN** the host sends `OPEN` to endpoint 3 using the same ZCL covering command as MQTT

#### Scenario: Full control body
- **WHEN** FULL CONTROL is on and Manual command sends `cl=0x0006,attr=0x0000,val=0x1`
- **THEN** the host sends that write-attribute the same way MQTT `set` would

#### Scenario: Covering stop from Command tab
- **WHEN** Manual command sends `STOP` for a `windowCovering` device
- **THEN** the host sends cluster `0x0102` command `0x02` the same way MQTT `set` would
