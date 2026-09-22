# Spec Delta

## ADDED Requirements

### Requirement: Devices table column order
The registered-device table SHALL show columns in this order: online, name, type, status, battery, RSSI. The table SHALL NOT show an IEEE column. IEEE SHALL remain visible on the device parameter dialog and SHALL remain the row identity for selection, edit, delete, and Manual command.

#### Scenario: Column order
- **WHEN** the operator opens Devices and at least one device is registered
- **THEN** the table headers are online, name, type, status, battery, RSSI and there is no IEEE header

### Requirement: Devices table shows battery percent or N/A
The battery cell SHALL show an integer percentage when the registered list JSON includes a battery percent for that IEEE. When that value is absent, the cell SHALL show `N/A`. Battery is live telemetry: the console SHALL NOT present it as an editable device setting.

#### Scenario: Percent after report
- **WHEN** the list JSON for a device includes battery `67`
- **THEN** that row’s battery cell shows `67`

#### Scenario: No battery yet
- **WHEN** the list JSON for a device omits battery
- **THEN** that row’s battery cell shows `N/A`

### Requirement: Devices table shows type-specific status
The status cell SHALL show the last known state for the device type. For `onOff`, each channel SHALL be a small circle: dark for OFF and light for ON. For other types, each channel SHALL show the last report text for that endpoint. When `channels` is `1`, the cell SHALL show one indicator. When `channels` is `0` or `2` through `16`, the cell SHALL show one indicator per channel, bound to inbound reports by the packet `ep`. A channel with no report yet SHALL show an empty or unknown indicator, not a guessed ON.

#### Scenario: Single-channel on/off on
- **WHEN** a registered `onOff` device has `channels` `1` and last state ON
- **THEN** that row’s status cell shows one light circle

#### Scenario: Parse-mode two endpoints
- **WHEN** a registered `onOff` device has `channels` `0`, endpoint 1 last reported OFF, and endpoint 3 last reported ON
- **THEN** that row’s status cell shows a dark circle for endpoint 1 and a light circle for endpoint 3

#### Scenario: IAS zone text
- **WHEN** a registered `iasZone` device last reported `LEAK` on its status endpoint
- **THEN** that row’s status cell shows `LEAK`

### Requirement: Manual command terminal
Beneath the registered-device table the console SHALL provide Manual command. The button SHALL be enabled only when a registered row is selected. Activating it SHALL open a terminal dialog for that IEEE. The dialog SHALL include a read-only answers pane and, below it, a command input. Pressing Enter in the input SHALL send the typed body through the same host command path as that device’s MQTT `set` topic and SHALL NOT require a broker. When the selected device has more than one channel (`channels` `0` or `2`–`16`), the dialog SHALL include a channel dropdown; the chosen channel SHALL select the destination endpoint the same way MQTT suffix or `ch-<ep>##` mapping would. Escape SHALL dismiss the dialog without sending. New inbound messages for that IEEE SHALL append to the answers pane while the dialog is open.

#### Scenario: Send ON like MQTT set
- **WHEN** the operator selects a single-channel device, opens Manual command, types `ON`, and presses Enter
- **THEN** the host sends that body on the same path as a publish to that device’s stored command topic

#### Scenario: Channel dropdown
- **WHEN** the selected device has `channels` `4` and the operator chooses channel 3, types `OFF`, and presses Enter
- **THEN** the host sends `OFF` to endpoint 3 the same way MQTT `set/3` would

#### Scenario: Disabled without selection
- **WHEN** no registered row is selected
- **THEN** Manual command is disabled

## MODIFIED Requirements

### Requirement: Double-click opens device edit
A single click on a registered-device table row SHALL select that IEEE and open the parameter dialog for that IEEE, same as Edit. A double-click SHALL NOT be required.

#### Scenario: Double-click row
- **WHEN** the operator single-clicks a registered device row
- **THEN** that row is selected and the edit parameter dialog opens for that device
