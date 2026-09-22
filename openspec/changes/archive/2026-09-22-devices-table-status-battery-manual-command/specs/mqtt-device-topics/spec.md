# Spec Delta

## ADDED Requirements

### Requirement: Console manual command uses the set-topic path
A Manual command body for a registered device SHALL be applied with the same channels mapping and FULL CONTROL rules as a payload received on that device’s MQTT command (`set`) topic. Suffix mode SHALL send the body to the chosen endpoint. Parse mode SHALL treat the chosen channel as `ch-<ep>##` wrapping. Single-channel SHALL send the body to the mapped endpoint as today. The host SHALL NOT require the MQTT broker to be connected. Gateway topics (`bridge/permit_join`, `bridge/config/device`) SHALL NOT accept this path.

#### Scenario: Same as MQTT ON
- **WHEN** FULL CONTROL is off and Manual command sends `ON` for a `channels` `1` device
- **THEN** the host sends the on/off command with payload `ON` to the endpoint from channels mapping

#### Scenario: Parse mode channel
- **WHEN** channels is `0` and Manual command sends `OPEN` with channel 3 selected
- **THEN** the host sends `OPEN` to endpoint 3

#### Scenario: Full control body
- **WHEN** FULL CONTROL is on and Manual command sends `cl=0x0006,attr=0x0000,val=0x1`
- **THEN** the host sends that write-attribute the same way MQTT `set` would
