## ADDED Requirements

### Requirement: Devices card lists registered devices and can add one
The Devices section SHALL show a table of registered devices (at least IEEE and friendly name) and an Add device control. Opening Devices SHALL load the registered list from the host. After a successful parameter Save, the table SHALL include the new row without requiring a full page reload.

#### Scenario: Open Devices
- **WHEN** the operator opens the Devices section and the host has at least one registered device
- **THEN** that device appears in the table

#### Scenario: Empty map
- **WHEN** the operator opens Devices and no devices are registered
- **THEN** the table is empty and Add device is still available

### Requirement: Add device uses a search dialog then a parameter dialog
Add device SHALL open a search dialog that contains a table of found-but-unregistered devices and a Search control. Search SHALL start pairing on the slave and SHALL update the found table as the host learns new joins. Add SHALL be enabled only when one found row is selected. Add SHALL close the search dialog and open a parameter dialog for that device. The parameter dialog SHALL show IEEE (not editable) and SHALL accept friendly name and MQTT topic fields. Save SHALL persist the device and close the parameter dialog. Dismissing the search dialog SHALL stop pairing search.

#### Scenario: Search fills the found table
- **WHEN** the search dialog is open, the operator clicks Search, and an unregistered device joins
- **THEN** that device appears in the found table without closing the dialog

#### Scenario: Add then Save
- **WHEN** the operator selects a found device, clicks Add, fills the parameter form, and clicks Save
- **THEN** the dialogs are closed and the Devices table shows the new registered device

#### Scenario: Cancel search
- **WHEN** the operator closes the search dialog without Add
- **THEN** no new registered device is written and pairing search stops

## MODIFIED Requirements

### Requirement: Main navigation uses sidebar sections
The console MUST present six main sections labeled Status, WiFi, MQTT, ZigBee, Devices, and System as a left sidebar strip (horizontal folder strip on a narrow viewport). Selecting a main section MUST show that section’s panel and hide the others without a full page reload. Status MUST render as an empty placeholder panel until that card is implemented. Devices MUST show the registered-device table and add-device flow.

#### Scenario: Switch main section
- **WHEN** the operator selects WiFi after Status was visible
- **THEN** the WiFi panel is shown and the Status panel is hidden

#### Scenario: Placeholder tabs have no forms
- **WHEN** the operator opens Status
- **THEN** the panel has no configuration controls and does not change device settings
