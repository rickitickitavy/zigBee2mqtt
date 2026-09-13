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
