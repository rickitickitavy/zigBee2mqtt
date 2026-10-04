# Spec Delta

## ADDED Requirements

### Requirement: Registered devices store transport
Each registered device record SHALL store a transport of `zigbee` or `wifi`. Transport is a persisted device setting: it SHALL appear in `devices.json`, list JSON after reboot, and device APIs. Transport SHALL be set when the record is created and SHALL NOT change on later saves. A save that omits transport on a new Zigbee (found-list) record SHALL store `zigbee`. A save that creates a Wi‑Fi device SHALL store `wifi`. Records loaded without a transport field SHALL be treated as `zigbee`.

#### Scenario: Found Zigbee save
- **WHEN** the operator saves a device from the pairing found list
- **THEN** that record has transport `zigbee`

#### Scenario: WiFi create save
- **WHEN** the operator saves a new device from the WIFI DEVICE path
- **THEN** that record has transport `wifi`

#### Scenario: Legacy record
- **WHEN** the host loads a stored device that has no transport field
- **THEN** list JSON reports transport `zigbee`

#### Scenario: Transport cannot change
- **WHEN** a registered device already has transport `zigbee` and a save request includes transport `wifi`
- **THEN** the record still has transport `zigbee`

## MODIFIED Requirements

### Requirement: Registered devices store cluster device type
Each registered device record SHALL store a device type of `unknown`, `onOff`, `iasZone`, or `windowCovering`. Type is a persisted device setting: it SHALL appear in `devices.json`, in `SpiCmdSetDevice` / dump payloads for `zigbee` devices, and in list JSON after reboot. For `zigbee` devices, readonly UI SHALL prevent the operator from editing type; it SHALL NOT skip persistence. A new save from a found device SHALL copy the type from that found record. A save that omits type on a new record SHALL store `unknown`. For `zigbee` devices, after a record has a type other than `unknown`, later operator saves SHALL leave that type unchanged even if the request body includes a different type. If a `zigbee` stored type is `unknown` and the host later receives a join classification other than `unknown` for the same IEEE, the host SHALL update the stored type to that classification. The slave SHALL persist the same type on its device dump so a host pull cannot replace a known type with `unknown`. For `wifi` devices, the operator SHALL be allowed to set and later change type among the same four values; join classification SHALL NOT overwrite a `wifi` type; the host SHALL NOT require the slave to store `wifi` types.

#### Scenario: Save from found list
- **WHEN** the operator saves a found device whose join type is `iasZone`
- **THEN** the registered record has type `iasZone`

#### Scenario: Operator cannot overwrite a known type
- **WHEN** a registered `zigbee` device already has type `onOff` and the operator saves parameters with a different type in the request
- **THEN** the record still has type `onOff`

#### Scenario: Unknown fills in later
- **WHEN** a registered `zigbee` IEEE is `unknown` and a later join classifies it as `windowCovering`
- **THEN** the registered record becomes `windowCovering`

#### Scenario: Operator sets WiFi type
- **WHEN** a registered `wifi` device has type `unknown` and the operator saves type `onOff`
- **THEN** the record has type `onOff`
