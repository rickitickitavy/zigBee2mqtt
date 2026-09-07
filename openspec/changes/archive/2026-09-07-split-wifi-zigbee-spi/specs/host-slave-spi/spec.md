## Purpose

Defines the SPI link so the host can command Zigbee send/receive on the slave and learn about radio events without sharing an RF path.

## ADDED Requirements

### Requirement: Host is SPI master
The host SHALL be the SPI master. The slave SHALL be the SPI slave. The link SHALL use a dedicated chip-select and a slave-to-host ready signal so the host can poll or clock out queued slave messages.

#### Scenario: Idle link
- **WHEN** both chips have completed boot and the SPI wires are connected
- **THEN** the host can enqueue a framed request and later receive a framed response or a queued event without the caller blocking on the wire

### Requirement: All master–slave interactions are asynchronous
The host SHALL NOT block Wi-Fi, MQTT, the web console, CLI, or the Arduino `loop` waiting for an SPI transfer or a slave result. Callers SHALL enqueue a command (with a sequence id) and learn the outcome later via a completion or event. The slave SHALL NOT block the Zigbee stack on SPI: inbound frames SHALL be queued from the SPI path and executed later; outbound frames SHALL be queued and signalled on IRQ. A missing or late slave reply SHALL surface as an asynchronous timeout or error, not a synchronous wait.

#### Scenario: Host command from MQTT
- **WHEN** an MQTT or CLI handler needs a Zigbee action
- **THEN** it only queues the SPI command and returns, and a later host callback or event applies `CMD_RESULT`, `ERR`, or timeout

#### Scenario: Slave handles command without stalling radio
- **WHEN** the host clocks a command into the slave
- **THEN** the SPI path only queues the frame, Zigbee work runs afterward, and any reply is queued for the host to pull when IRQ is asserted

### Requirement: Framed Zigbee commands and events
The SPI protocol SHALL carry host-to-slave commands that send Zigbee data or coordinator control (including permit-join and on/off-style device commands the host already exposes). The protocol SHALL carry slave-to-host messages for received Zigbee payloads, join/leave, and command results. Frames SHALL include a length and an integrity check so a truncated or corrupted transfer is discarded.

#### Scenario: Host sends a command
- **WHEN** the host needs to transmit or control Zigbee
- **THEN** it enqueues one framed command for the SPI master task and later receives a framed result or error matched by sequence id

#### Scenario: Slave reports inbound data
- **WHEN** the slave receives Zigbee data or a coordinator event
- **THEN** it queues a framed message and asserts the ready signal until the host has clocked the frame out

### Requirement: Slave has no product logic
The slave SHALL NOT apply settings, MQTT, or web business rules. It SHALL only execute radio send/receive and the SPI protocol. The host SHALL own mapping to MQTT topics, settings, and the operator UI.

#### Scenario: Incoming device report
- **WHEN** a Zigbee device reports state to the slave
- **THEN** the slave forwards the report over SPI and the host performs MQTT publish and any other product handling

### Requirement: Settings live only on the host
The host SHALL be the only store for product settings. The slave SHALL NOT write product settings to EEPROM, NVS, or LittleFS. After the slave signals ready, the host SHALL enqueue a settings frame (channel, permit-join default, and any other radio fields the coordinator needs) before it treats the link as in normal work.

#### Scenario: Host boot settings push
- **WHEN** the host has seen `SLAVE_READY` after reset
- **THEN** it enqueues `SET_SETTINGS` and enters normal Zigbee command work only after an asynchronous settings-accepted result

#### Scenario: Slave power-loss
- **WHEN** the slave reboots and loses RAM settings
- **THEN** it waits for a new host `SET_SETTINGS` and does not recover a local settings file

### Requirement: Host resets and brings up the slave asynchronously
The host SHALL drive a reset line (GPIO11 to the slave EN pin, active-low pulse). After releasing reset, the host SHALL wait for `SLAVE_READY` without blocking Wi-Fi, MQTT, web, or `loop`. If `SLAVE_READY` does not arrive within the ready timeout, the host SHALL pulse reset again and wait again. The host SHALL NOT use blocking delays for this wait.

#### Scenario: Slave ready then configure
- **WHEN** the host starts and pulses slave reset
- **THEN** it continues other host work and, when `SLAVE_READY` arrives, pushes settings and then accepts normal Zigbee operations

#### Scenario: Slave silent
- **WHEN** the ready timeout elapses with no `SLAVE_READY`
- **THEN** the host pulses reset again and repeats the async wait

### Requirement: Slave pushes logs immediately; host stores both
The slave SHALL enqueue a log event to the host as soon as a line is written. That event SHALL use the **same** IRQ + SPI path as Zigbee data (`ATTR_REPORT`, join/leave, `CMD_RESULT`): same frame format, same slave outbound queue, same GPIO10 ready line, same host SPI task. No extra UART, wire, or pull API. The host SHALL append both host-origin and slave-origin lines into one in-memory log of 65536 characters with wrap (oldest overwritten). Every stored line SHALL include a date-time and a source label that marks slave lines as slave (host lines labeled host). Date-time SHALL use NTP wall clock when the host has it; otherwise the host local timer (boot-based clock). The host SHALL send time to the slave (with settings and later syncs) so the slave can stamp lines before sending; if the slave has no sync yet it SHALL stamp with its local timer and the host SHALL still store the line with a host clock time if needed to keep a date-time on the record.

#### Scenario: Immediate slave line
- **WHEN** the slave logger writes a line
- **THEN** it pushes a `LOG_RECORD` onto the same outbound SPI queue as Zigbee events, asserts IRQ if needed, and the host SPI task later stores the line in the shared 64 KiB log with a slave label and a date-time

#### Scenario: Host line
- **WHEN** the host logger writes a line
- **THEN** the same 64 KiB log gets a host-labeled record with a date-time

#### Scenario: Rotation
- **WHEN** the host log exceeds 65536 characters
- **THEN** new characters overwrite the oldest in that host buffer

#### Scenario: NTP clock
- **WHEN** NTP has set the host clock
- **THEN** new records use that wall-clock date and time

#### Scenario: Local timer clock
- **WHEN** NTP is not available
- **THEN** new records use the local timer clock and still include a date and time field
