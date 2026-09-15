## ADDED Requirements

### Requirement: Device parameter dialog includes FULL CONTROL
The Devices parameter dialog SHALL include a FULL CONTROL checkbox as the last setting field (after IEEE, name, channels, and the MQTT topic fields). The control SHALL load from the stored record and SHALL be unchecked when the flag is omitted or off. Save SHALL persist the checkbox with the device. Binary device settings SHALL use a checkbox, not a text field or select of on/off strings.

#### Scenario: New device shows off at the end
- **WHEN** the operator opens the parameter dialog for a newly added device
- **THEN** FULL CONTROL is unchecked and appears after AVAILABILITY

#### Scenario: Load stored on
- **WHEN** the operator opens parameters for a device stored with FULL CONTROL on
- **THEN** the FULL CONTROL checkbox is checked and still last in the form

#### Scenario: Save turns it on
- **WHEN** the operator checks FULL CONTROL and saves
- **THEN** the stored record has FULL CONTROL on
