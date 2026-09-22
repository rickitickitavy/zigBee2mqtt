# zigbee-slave-radio Specification

## Purpose

Keeps the Zigbee coordinator on the slave independent of host Wi-Fi mode, after the host has pushed settings for that boot.

## Requirements

### Requirement: Zigbee independent of Wi-Fi mode
After the host has accepted a settings-applied result for the current slave boot, the slave SHALL keep the Zigbee coordinator running while powered and not in a fatal radio error, including while the host is in SoftAP, STA, or recovery AP. Until that settings apply, the slave SHALL run SPI and the RAM log only. The host SHALL NOT start a Zigbee radio on its own chip.

#### Scenario: Host in SoftAP
- **WHEN** the host is serving SoftAP (including recovery AP) and slave settings have been applied
- **THEN** the slave Zigbee coordinator remains up and continues to receive and send

#### Scenario: Host STA connected
- **WHEN** the host is joined to a router and slave settings have been applied
- **THEN** the slave Zigbee coordinator remains up and continues to receive and send

#### Scenario: Before settings
- **WHEN** the slave has booted but has not yet applied host `SET_SETTINGS`
- **THEN** it does not start the coordinator and only runs SPI plus the RAM log

### Requirement: Host Wi-Fi stays usable
With the two-chip split, the host SHALL run Wi-Fi (SoftAP or STA) without enabling IEEE 802.15.4 coexist on the host radio. Operator-visible services on the host (ping, HTTP web console, MQTT client) SHALL remain usable while the slave Zigbee radio is active.

#### Scenario: Web while mesh is up
- **WHEN** the slave coordinator is started and the host has a usable Wi-Fi interface
- **THEN** the host web console and ICMP to the host address remain reachable without requiring Zigbee to be stopped

### Requirement: One in-flight command per destination
The slave SHALL transmit at most one Zigbee on/off or write-attribute command at a time to a given IEEE and destination endpoint. While that command is in flight, a newer on/off or write-attribute for the same IEEE and endpoint SHALL replace the pending next command and SHALL NOT be put on the air until the in-flight command finishes or a send timeout expires. After finish or timeout, the slave SHALL send only that latest pending command, or none if none remains. Distinct endpoints of the same IEEE MAY have independent in-flight commands. Permit-join and other non-device-control radio work SHALL NOT be serialized behind this rule.

#### Scenario: Rapid ON then OFF
- **WHEN** the operator sends ON then OFF to the same IEEE and endpoint faster than the first command completes on air
- **THEN** the device receives at most the in-flight command plus a later OFF, and does not keep switching after OFF has been applied and in-flight work has finished

#### Scenario: Echo fades to last command
- **WHEN** ON and OFF are repeated several times to one endpoint and then commands stop
- **THEN** the device settles on the last commanded state and does not continue random ON/OFF from leftover earlier commands

#### Scenario: Two endpoints stay independent
- **WHEN** endpoint 1 is commanded OFF and endpoint 3 of the same IEEE is commanded ON before endpoint 1 completes
- **THEN** both destinations still receive their own commands

### Requirement: Classify device type from in-clusters
When the slave identifies a joining or newly bound device, it SHALL inspect that IEEE’s advertised **in-clusters** (simple descriptor on each usable endpoint) and assign exactly one device type for the IEEE. Classification SHALL use this priority, first match wins across all those in-clusters: IAS Zone (`0x0500`) → `iasZone`; Window Covering (`0x0102`) → `windowCovering`; On/Off (`0x0006`) → `onOff`. If none of those clusters are present, the type SHALL be `unknown`. Manufacturer name, model, and friendly name SHALL NOT decide the type. The slave SHALL classify before or with the join report that the host uses for pairing search. Type-specific command behavior is not required in this change.

#### Scenario: On/Off switch
- **WHEN** a joining device advertises in-cluster `0x0006` and does not advertise `0x0500` or `0x0102`
- **THEN** the slave classifies that IEEE as `onOff`

#### Scenario: Leak or IAS sensor
- **WHEN** a joining device advertises in-cluster `0x0500` and also `0x0006`
- **THEN** the slave classifies that IEEE as `iasZone`

#### Scenario: No matching cluster
- **WHEN** a joining device advertises only clusters outside the classification list
- **THEN** the slave classifies that IEEE as `unknown`

### Requirement: Coordinator start reads current device status
After the slave Zigbee coordinator has started with host settings, the slave SHALL request current status from every registered device it knows. For `onOff`, it SHALL read On/Off (`0x0006` / `0x0000`) on each usable channel endpoint for that device. For `iasZone`, it SHALL read the type’s current zone status. For `windowCovering`, it SHALL read the type’s current covering status. When a registered device advertises Power Configuration (`0x0001`), the slave SHALL also read Battery Percentage Remaining. Sleepy devices that do not answer SHALL leave cached status empty; the coordinator start SHALL still complete. Reads SHALL use the same inbound report path as unsolicited reports.

#### Scenario: Switch after start
- **WHEN** the coordinator starts and a registered `onOff` device with `channels` `1` answers ON
- **THEN** the host receives an `ON` report for that IEEE so the Devices table can show a light circle

#### Scenario: Multi-channel after start
- **WHEN** the coordinator starts and a registered `onOff` device has `channels` `3`
- **THEN** the slave reads On/Off on endpoints 1, 2, and 3

#### Scenario: Battery-capable device
- **WHEN** the coordinator starts and a registered device advertises in-cluster `0x0001` and answers 67 percent
- **THEN** the host receives a battery report that decodes to 67 percent

#### Scenario: Sleepy no answer
- **WHEN** the coordinator starts and a registered device does not answer the read
- **THEN** the coordinator remains running and that device’s status and battery stay unknown until a later report

