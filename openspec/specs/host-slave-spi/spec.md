# host-slave-spi Specification

## Purpose

Defines the SPI link so the host can command Zigbee send/receive on the slave and learn about radio events without sharing an RF path.

## Requirements

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

### Requirement: Host resets the slave first then brings it up
The host SHALL drive a reset line (GPIO11 to the slave EN pin, active-low pulse). The **first** step of host role init SHALL be that pulse in **sync** mode: the host SHALL assert reset, wait the pulse width, and release reset before it starts Wi-Fi, the web console, MQTT, LittleFS, or the host SPI task. The host SHALL NOT finish that first pulse from the SPI pump task. On **every** host boot the host SHALL perform that first-step pulse. The host SHALL NOT skip it because the slave already asserts the ready line, because IRQ is already high, or because the slave still answers from a previous run. After releasing that first pulse, the host SHALL wait for `SLAVE_READY` without blocking Wi-Fi, MQTT, web, or `loop`. If `SLAVE_READY` does not arrive within the ready timeout, the host SHALL pulse reset again and wait again. Those later retries SHALL NOT use blocking delays.

#### Scenario: Slave ready then configure
- **WHEN** the host starts and has completed the first-step slave reset
- **THEN** it continues other host work and, when `SLAVE_READY` arrives, pushes settings and then accepts normal Zigbee operations

#### Scenario: Slave silent
- **WHEN** the ready timeout elapses with no `SLAVE_READY`
- **THEN** the host pulses reset again and repeats the async wait

#### Scenario: Host boot always resets a live slave
- **WHEN** the host boots (including a warm restart) and the slave is still running from before
- **THEN** the host still performs the first-step reset pulse and does not skip it because the slave already looks ready

#### Scenario: Reset is the first host-init step
- **WHEN** the chip starts in the host role
- **THEN** the slave reset pulse is completed before Wi-Fi, the web console, MQTT, or the host SPI task start

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

### Requirement: Device commands carry destination endpoint and message
A host-to-slave device command SHALL include the destination IEEE, the destination Zigbee endpoint, and the command message text. The slave SHALL address that endpoint. When the message is an on/off/toggle command the slave SHALL send the matching ZCL on/off. The slave SHALL NOT substitute a different cached bind endpoint when a usable endpoint was provided.

#### Scenario: Command endpoint 4
- **WHEN** the host enqueues a command for an IEEE with endpoint 4 and message `off`
- **THEN** the slave transmits off to endpoint 4 of that IEEE

#### Scenario: Two gangs independently
- **WHEN** the host enqueues `off` for endpoint 1 and `on` for endpoint 3 of the same IEEE
- **THEN** the slave addresses endpoint 1 for the first command and endpoint 3 for the second

### Requirement: Attribute reports carry the device message text
A slave-to-host attribute or zone report SHALL include IEEE, source endpoint, short address, RSSI of that packet, and the message text to publish. The host SHALL use that text as the MQTT payload body (after any channels mapping).

#### Scenario: IAS report
- **WHEN** the slave reports `LEAK` from an endpoint
- **THEN** the host receives that message text with that endpoint

#### Scenario: RSSI on report
- **WHEN** the slave reports from an IEEE with RSSI −55 dBm
- **THEN** the host receives −55 dBm with that IEEE

### Requirement: Host can command a ZCL write attribute
The SPI protocol SHALL carry a host-to-slave write-attribute command that includes destination IEEE, destination endpoint, cluster id, attribute id, ZCL data type, and value. The slave SHALL transmit a ZCL write-attribute to that destination. The slave SHALL NOT substitute a different cached bind endpoint when a usable endpoint was provided. The slave SHALL NOT decide write versus on/off from MQTT or device settings; it SHALL execute the framed write as given.

#### Scenario: Write on/off attribute
- **WHEN** the host enqueues a write-attribute for an IEEE with endpoint 2, cluster `0x0006`, attribute `0x0000`, type unsigned 8-bit, value `1`
- **THEN** the slave transmits that write to endpoint 2 of that IEEE

#### Scenario: Usable endpoint is honored
- **WHEN** the host enqueues a write-attribute with endpoint 4 and the slave has a different cached bind endpoint for that IEEE
- **THEN** the slave addresses endpoint 4

### Requirement: Slave signals join window closed
When the slave permit-join / pairing window ends (timer or explicit close), the slave SHALL notify the host over SPI. The host SHALL treat pairing as inactive after that event so the console can enable Search again.

#### Scenario: Window expires
- **WHEN** the slave join window duration elapses while Search is running
- **THEN** the host learns pairing is inactive without the operator closing the dialog

### Requirement: Inbound Zigbee packets carry last RSSI
A slave-to-host inbound Zigbee packet used for device reports SHALL include RSSI of that packet. The host SHALL store that RSSI as the last received signal for that IEEE until a later packet replaces it.

#### Scenario: Report includes RSSI
- **WHEN** the slave forwards an attribute report from an IEEE with RSSI −70 dBm
- **THEN** the host stores −70 dBm as last signal for that IEEE

### Requirement: Latest device control command wins per destination
When the host has not yet delivered a device-control command (`on/off`-style or write-attribute) to the slave for a given IEEE and destination endpoint, a newer device-control command for that same IEEE and endpoint SHALL replace the pending one. The host SHALL NOT keep a FIFO of opposite ON and OFF (or mixed on/off and write-attribute) for that destination. Commands for other IEEE values, other endpoints, or non-device-control SPI commands SHALL remain queued independently. The slave SHALL likewise keep only the latest deferred device-control command per IEEE and endpoint until the radio path accepts it.

#### Scenario: Host queue of ON then OFF
- **WHEN** the host enqueues ON and then OFF for the same IEEE and endpoint before the slave has taken the first command
- **THEN** the slave is given OFF for that destination and is not given a leftover ON after OFF

#### Scenario: Mixed write and on/off
- **WHEN** the host enqueues a write-attribute and then an on/off command for the same IEEE and endpoint before the first is delivered
- **THEN** only the later on/off command is delivered for that destination

#### Scenario: Other destinations stay queued
- **WHEN** the host enqueues OFF for IEEE A endpoint 1 and ON for IEEE B endpoint 1
- **THEN** both commands remain and each destination still receives its own command

### Requirement: Join event carries classified device type
`SpiEvtDeviceJoin` SHALL include the slave’s classified device type for that IEEE after the existing IEEE, NWK, endpoint, manufacturer, and model fields. The type SHALL be one of `unknown`, `onOff`, `iasZone`, or `windowCovering`. A host that receives a join without a type field SHALL treat the type as `unknown`.

#### Scenario: Join with type
- **WHEN** the slave reports a join for an IEEE classified as `onOff`
- **THEN** the host found-device record for that IEEE has type `onOff`

#### Scenario: Short join frame
- **WHEN** a join frame arrives without a type field
- **THEN** the host stores type `unknown` for that found IEEE

### Requirement: Device sync carries persisted device type
`SpiCmdSetDevice` and the slave device dump SHALL include the stored device type as the last byte of each sync entry (`SPI_DEVICE_SYNC_ENTRY_LEN`). A shorter legacy entry SHALL leave type as `unknown` unless the host already has a known type for that IEEE, in which case the host SHALL keep the known type.

#### Scenario: Dump round-trips type
- **WHEN** a registered device has type `windowCovering` and the slave dumps the registry
- **THEN** the host list after pull still has type `windowCovering`

### Requirement: Host can send firmware to the slave over SPI

The SPI protocol SHALL carry a host-to-slave firmware-update sequence: begin, data chunks that fit in a single framed payload, and end. Each command SHALL use the existing framed length and integrity check. The host SHALL enqueue those commands asynchronously and SHALL NOT block Wi-Fi, HTTP, MQTT, or `loop` waiting for the whole image to transfer. A missing or late slave reply SHALL abort the firmware-update sequence. The slave SHALL write received firmware to its inactive application slot. On a successful end, the slave SHALL commit that slot and restart into the new application. On failure, the slave SHALL leave its running application unchanged and SHALL NOT restart.

#### Scenario: Chunked transfer

- **WHEN** the host sends a firmware image larger than one SPI payload
- **THEN** the slave receives it as multiple chunks and, after a successful end, restarts into that image

#### Scenario: Failed chunk aborts

- **WHEN** a firmware chunk is corrupted or the slave times out during an in-progress firmware update
- **THEN** the slave does not commit a new application and the host treats the firmware update as failed

