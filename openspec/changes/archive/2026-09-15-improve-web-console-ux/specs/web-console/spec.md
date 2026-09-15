## ADDED Requirements

### Requirement: NAME edits rewrite default MQTT topics
While the device parameter dialog is open, each change to NAME SHALL rewrite the state, command, and availability fields to the default topics for that name and the stored MQTT base topic (`{base}/{slug}/state`, `{base}/{slug}/set`, `{base}/{slug}/availability`). This SHALL apply when adding and when editing an existing device.

#### Scenario: Rename rewrites topics
- **WHEN** the operator edits NAME from `switch_1` to `kitchen_lamp`
- **THEN** the three topic fields update to the default pattern that uses `kitchen_lamp`

### Requirement: Double-click opens device edit
A double-click on a registered-device table row SHALL open the parameter dialog for that IEEE, same as Edit.

#### Scenario: Double-click row
- **WHEN** the operator double-clicks a registered device row
- **THEN** the edit parameter dialog opens for that device

### Requirement: Devices Export downloads the slave store
Beneath the registered-device table the console SHALL provide Export. Export SHALL download the slave devices store JSON (the same document as System devices.json), for later Restore.

#### Scenario: Export file
- **WHEN** the operator clicks Export
- **THEN** the browser downloads the current slave `devices.json` content

### Requirement: Devices Restore matches System restore
Beneath the registered-device table the console SHALL provide Restore that uses the same file-pick and restore path as System devices.json Restore (one device record at a time to the slave).

#### Scenario: Restore from Devices
- **WHEN** the operator chooses Restore on Devices and selects a valid devices JSON array
- **THEN** missing devices are restored the same way as System devices.json Restore

### Requirement: Search follows the slave join window
Opening the pairing dialog SHALL NOT start pairing on the slave. Search SHALL start the slave join window only when the operator clicks Search. While that window is open, Search SHALL stay disabled. When the slave join window ends, Search SHALL become enabled again even if the dialog stays open. Closing the dialog SHALL still stop pairing.

#### Scenario: Open does not search
- **WHEN** the operator opens Add device
- **THEN** the pairing dialog is visible and the slave join window is not started

#### Scenario: Search until slave ends
- **WHEN** the operator clicks Search and the slave join window later closes
- **THEN** Search is enabled again without requiring the dialog to close

### Requirement: Devices table shows last packet RSSI
The registered-device table SHALL include last received signal strength for each IEEE when the host has stored RSSI from the last inbound Zigbee packet for that device. A device with no received packet yet SHALL show an empty signal cell.

#### Scenario: RSSI after report
- **WHEN** the host has received a packet for a registered IEEE with RSSI −62 dBm
- **THEN** that row shows −62 dBm (or an equivalent dBm display of that value)

### Requirement: Status shows counts, traffic, and version
The Status section SHALL show registered device count, online device count, packets received, packets sent, and firmware version. Status SHALL NOT contain settings forms that write configuration.

#### Scenario: Open Status with devices
- **WHEN** the operator opens Status and three devices are registered with one online
- **THEN** Status shows device count 3, online count 1, firmware version from the running image, and the packet counters

## MODIFIED Requirements

### Requirement: Main navigation uses sidebar sections

The console MUST present six main sections labeled Status, WiFi, MQTT, ZigBee, Devices, and System as a left sidebar strip (horizontal folder strip on a narrow viewport). Selecting a main section MUST show that section’s panel and hide the others without a full page reload. Status MUST show the live summary (counts, packet counters, version) without settings forms. Devices MUST show the registered-device table and add-device flow.

#### Scenario: Switch main section

- **WHEN** the operator selects WiFi after Status was visible
- **THEN** the WiFi panel is shown and the Status panel is hidden

#### Scenario: Placeholder tabs have no forms

- **WHEN** the operator opens Status
- **THEN** the panel has no configuration controls and does not change device settings

### Requirement: Devices card lists registered devices and can add one
The Devices section SHALL show a table of registered devices (at least IEEE, friendly name, last RSSI when known) and Add device, Export, and Restore. Opening Devices SHALL load the registered list from the host. After a successful parameter Save, the table SHALL include the new row without requiring a full page reload.

#### Scenario: Open Devices
- **WHEN** the operator opens the Devices section and the host has at least one registered device
- **THEN** that device appears in the table

#### Scenario: Empty map
- **WHEN** the operator opens Devices and no devices are registered
- **THEN** the table is empty and Add device is still available

### Requirement: Add device uses a search dialog then a parameter dialog
Add device SHALL open a search dialog that contains a table of found-but-unregistered devices and a Search control. Opening the dialog SHALL NOT start pairing. Search SHALL start pairing on the slave only when clicked and SHALL update the found table as the host learns new joins. Add SHALL be enabled only when one found row is selected. Add SHALL close the search dialog and open a parameter dialog for that device. The parameter dialog SHALL show IEEE (not editable) and SHALL accept friendly name and MQTT topic fields. Save SHALL persist the device and close the parameter dialog. Dismissing the search dialog SHALL stop pairing search.

#### Scenario: Search fills the found table
- **WHEN** the search dialog is open, the operator clicks Search, and an unregistered device joins
- **THEN** that device appears in the found table without closing the dialog

#### Scenario: Add then Save
- **WHEN** the operator selects a found device, clicks Add, fills the parameter form, and clicks Save
- **THEN** the dialogs are closed and the Devices table shows the new registered device

#### Scenario: Cancel search
- **WHEN** the operator closes the search dialog without Add
- **THEN** no new registered device is written and pairing search stops

### Requirement: System has Update and Log tabs

When System is selected, the console MUST present nested folder-style tabs in this order: Log, devices.json, Update, Hardware. The default System panel MUST be Log. Selecting a tab MUST show that panel and hide the others without a full page reload.

#### Scenario: Nested System tabs

- **WHEN** the operator opens System and then selects Log
- **THEN** the Log panel is shown and the Update panel is hidden

#### Scenario: Default System tab is Log
- **WHEN** the operator opens System
- **THEN** the Log tab is selected first
