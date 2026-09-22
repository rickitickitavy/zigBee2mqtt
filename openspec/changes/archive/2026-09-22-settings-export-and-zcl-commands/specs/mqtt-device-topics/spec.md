## ADDED Requirements

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

## MODIFIED Requirements

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
