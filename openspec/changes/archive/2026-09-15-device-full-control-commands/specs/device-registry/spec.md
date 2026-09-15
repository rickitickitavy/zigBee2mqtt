## ADDED Requirements

### Requirement: Registered devices store FULL CONTROL
Each registered device record SHALL store a FULL CONTROL flag. The default SHALL be off, which SHALL keep existing command behavior (channels mapping, then on/off/toggle). A save that omits the flag SHALL leave a new record off and SHALL NOT turn an existing record on. The host SHALL persist the flag with the device list. The slave SHALL NOT be required to persist or interpret this flag.

#### Scenario: New device default
- **WHEN** the operator saves a device without a FULL CONTROL value
- **THEN** that record has FULL CONTROL off and MQTT commands still use on/off/toggle

#### Scenario: Flag survives host reload
- **WHEN** a device is saved with FULL CONTROL on and the host reloads settings
- **THEN** that device still has FULL CONTROL on
