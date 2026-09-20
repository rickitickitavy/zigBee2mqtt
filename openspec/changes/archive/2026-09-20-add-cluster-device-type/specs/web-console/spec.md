# Spec Delta

## ADDED Requirements

### Requirement: Pairing found table shows readable device type
The pairing search found table SHALL show a readable device type for each found row. Labels SHALL be: `unknown` → Unknown; `onOff` → On/Off; `iasZone` → IAS Zone; `windowCovering` → Window covering. Manufacturer and model MAY still appear. The type SHALL come from the found-device API, not from the operator.

#### Scenario: Switch appears in Search
- **WHEN** Search is open and an unregistered `onOff` device joins
- **THEN** that row shows On/Off

### Requirement: Device parameters show readonly type
The device parameter dialog SHALL show the device type as a readonly field, like IEEE. Add from Search SHALL show the found type. Edit of a registered device SHALL show the stored type. Save SHALL persist other editable fields and SHALL NOT take a new type from an editable control.

#### Scenario: Add shows found type
- **WHEN** the operator Adds a found `iasZone` device
- **THEN** the parameter dialog shows IAS Zone and the operator cannot edit that field

#### Scenario: Edit keeps stored type
- **WHEN** the operator opens parameters for a registered `onOff` device and saves a new name
- **THEN** the type remains `onOff` and the dialog still shows On/Off
