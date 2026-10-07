# Spec Delta

## MODIFIED Requirements

### Requirement: WiFi devices publish state; host listens

A registered device with transport `mqtt` SHALL report runtime state by publishing to its stored state topic (and channel variants per channels mapping). The host SHALL subscribe to those state topics and update last-known status for the console from inbound MQTT payloads. The host SHALL NOT publish Zigbee-originated state to an `mqtt` device’s state topic.

#### Scenario: State message updates console

- **WHEN** an `mqtt` device with channels `1` publishes `ON` to its stored state topic
- **THEN** that device’s registered list JSON includes a status entry reflecting `ON`

#### Scenario: Host does not invent WiFi state

- **WHEN** the host collects Zigbee statuses after a server-born message
- **THEN** it does not publish synthetic state to any `mqtt` device’s state topic as part of that collect

### Requirement: WiFi devices stay off the Zigbee slave

The host SHALL NOT upsert, dump-replace, or delete an `mqtt` transport record on the Zigbee slave registry. Slave join, online, RSSI, and Zigbee command paths SHALL ignore `mqtt` records.

#### Scenario: Save WiFi device

- **WHEN** the operator saves a new `mqtt` device
- **THEN** the host persists the record and does not send a slave device upsert for that IEEE

#### Scenario: Birth collect skips WiFi radio reads

- **WHEN** the host runs Zigbee status collect after a server-born message
- **THEN** it does not issue Zigbee attribute reads for `mqtt` devices

### Requirement: WiFi commands use MQTT set topics

For an `mqtt` device, an inbound MQTT command on its set topic, Manual command, and type-action shortcuts SHALL be published to that device’s MQTT command topic with channels mapping applied. The host SHALL NOT send Zigbee on/off, write-attribute, or cluster commands for an `mqtt` device.

#### Scenario: Manual ON for WiFi

- **WHEN** Manual command sends `ON` for an `mqtt` device with channels `1`
- **THEN** the host publishes `ON` to that device’s stored command topic and does not send a Zigbee on/off

### Requirement: WiFi create uses operator identity and cluster type

Creating an `mqtt` device SHALL require a unique IEEE-like identity and a friendly name. IEEE SHALL be editable while creating a new `mqtt` device and SHALL be readonly after the record exists. Cluster type SHALL default to `unknown` and SHALL accept an operator-chosen value among `unknown`, `onOff`, `iasZone`, and `windowCovering` on create and later edit.

#### Scenario: Create WiFi with type

- **WHEN** the operator creates an `mqtt` device with IEEE `AA:BB:CC:DD:EE:FF:00:01`, name `plug1`, and type `onOff`
- **THEN** the registered record has transport `mqtt`, that IEEE, and type `onOff`

#### Scenario: Edit WiFi cluster type

- **WHEN** a stored `mqtt` device has type `unknown` and the operator saves type `iasZone`
- **THEN** the record type becomes `iasZone`

## ADDED Requirements

### Requirement: Legacy wifi transport id is not remapped

The host SHALL NOT translate persisted or uploaded `"transport":"wifi"` into `mqtt`. Only the id `mqtt` SHALL select MQTT-device behavior. A record whose transport string is `wifi` or any other unrecognized value SHALL be treated as `zigbee` for clamp/fallback purposes until the operator replaces the device list.

#### Scenario: Old wifi string is not MQTT

- **WHEN** the host loads a device JSON entry with `"transport":"wifi"`
- **THEN** that entry is not treated as an `mqtt` device (no MQTT-only subscribe/command path for that record solely because of the legacy string)
