# Spec Delta

## ADDED Requirements

### Requirement: MQTT settings store server-born topic and interval
The MQTT settings group SHALL store a server-born topic string and a born announce interval in minutes. The interval SHALL clamp to 5–120 inclusive; the factory default SHALL be 20. The server-born topic MAY be empty. Empty topic SHALL disable born listen and born publish. Settings GET/POST, export, and restore SHALL include both fields.

#### Scenario: Default interval
- **WHEN** MQTT settings are initialized to defaults
- **THEN** born announce interval is 20 and server-born topic is empty

#### Scenario: Clamp interval
- **WHEN** the operator saves born interval 2
- **THEN** the stored interval is 5

#### Scenario: Empty topic disables born
- **WHEN** server-born topic is empty and MQTT is connected
- **THEN** the host does not subscribe for born and does not publish born announces

### Requirement: Server-born listen refreshes Zigbee states
When the server-born topic is non-empty and MQTT is connected, the host SHALL subscribe to that exact topic. On receipt of any payload on that topic, the host SHALL collect states of all registered `zigbee` devices the same way it does at boot (status reads that lead to state publishes when replies arrive). The host SHALL NOT require a particular payload body.

#### Scenario: Any born message
- **WHEN** MQTT delivers any message on the configured server-born topic
- **THEN** the host starts Zigbee status collect for registered `zigbee` devices

#### Scenario: WiFi excluded from radio collect
- **WHEN** a born message arrives and the map has both `zigbee` and `wifi` devices
- **THEN** only `zigbee` devices are included in the Zigbee status collect

### Requirement: Host announces on server-born topic after boot and on interval
When the server-born topic is non-empty and MQTT is connected, the host SHALL publish a message to that topic once at least 2 minutes after host boot, then again every configured born announce interval. The publish SHALL use a non-empty payload. The purpose is to tell Wi‑Fi devices (and other listeners) to refresh their statuses.

#### Scenario: First announce after boot
- **WHEN** MQTT is connected, server-born topic is set, and 2 minutes have passed since host boot without a prior host born publish in this run
- **THEN** the host publishes once to the server-born topic

#### Scenario: Periodic announce
- **WHEN** the configured interval is 20 and the host already published a born announce
- **THEN** the host publishes again about 20 minutes later while MQTT stays connected

### Requirement: Host subscribes to WiFi state topics
For each registered `wifi` device with a non-empty state topic, while MQTT is connected the host SHALL subscribe to that state topic and, when channels use suffix mapping, the suffix wildcard needed for extra endpoints. Inbound payloads on those topics SHALL update that device’s runtime status using the same channels mapping rules as Zigbee publishes use for topic↔endpoint.

#### Scenario: Subscribe single channel
- **WHEN** MQTT connects and a `wifi` device has channels `1` and state topic `z2m/plug/state`
- **THEN** the host is subscribed to `z2m/plug/state`

#### Scenario: Suffix subscribe
- **WHEN** MQTT connects and a `wifi` device has channels `3` and state topic `z2m/gang/state`
- **THEN** the host is subscribed to `z2m/gang/state` and `z2m/gang/state/+`

## MODIFIED Requirements

### Requirement: Console manual command uses the set-topic path
A Manual command body for a registered `zigbee` device SHALL be applied with the same channels mapping, FULL CONTROL, ZCL cluster-command, and on/off rules as a payload received on that device’s MQTT command (`set`) topic. Suffix mode SHALL send the body to the chosen endpoint. Parse mode SHALL treat the chosen channel as `ch-<ep>##` wrapping. Single-channel SHALL send the body to the mapped endpoint as today. The host SHALL NOT require the MQTT broker to be connected for `zigbee` Manual command. Gateway topics (`bridge/permit_join`, `bridge/config/device`) SHALL NOT accept this path. For a registered `wifi` device, Manual command SHALL publish the body to that device’s MQTT command topic with channels mapping and SHALL NOT send Zigbee radio commands.

#### Scenario: Same as MQTT ON
- **WHEN** FULL CONTROL is off and Manual command sends `ON` for a `channels` `1` `zigbee` device
- **THEN** the host sends the on/off command with payload `ON` to the endpoint from channels mapping

#### Scenario: Parse mode channel
- **WHEN** channels is `0` and Manual command sends `OPEN` with channel 3 selected for a `zigbee` device
- **THEN** the host sends `OPEN` to endpoint 3 using the same ZCL covering command as MQTT

#### Scenario: Full control body
- **WHEN** FULL CONTROL is on and Manual command sends `cl=0x0006,attr=0x0000,val=0x1` for a `zigbee` device
- **THEN** the host sends that write-attribute the same way MQTT `set` would

#### Scenario: Covering stop from Command tab
- **WHEN** Manual command sends `STOP` for a `windowCovering` `zigbee` device
- **THEN** the host sends cluster `0x0102` command `0x02` the same way MQTT `set` would

#### Scenario: WiFi Manual publishes set
- **WHEN** Manual command sends `ON` for a `wifi` device with channels `1` and MQTT is connected
- **THEN** the host publishes `ON` to that device’s command topic and does not send Zigbee on/off
