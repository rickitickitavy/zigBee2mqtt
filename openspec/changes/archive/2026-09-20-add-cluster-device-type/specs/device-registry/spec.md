# Spec Delta

## ADDED Requirements

### Requirement: Registered devices store cluster device type
Each registered device record SHALL store a device type of `unknown`, `onOff`, `iasZone`, or `windowCovering`. Type is a persisted device setting: it SHALL appear in `devices.json`, in `SpiCmdSetDevice` / dump payloads, and in list JSON after reboot. Readonly UI SHALL prevent the operator from editing type; it SHALL NOT skip persistence. A new save from a found device SHALL copy the type from that found record. A save that omits type on a new record SHALL store `unknown`. After a record has a type other than `unknown`, later operator saves SHALL leave that type unchanged even if the request body includes a different type. If the stored type is `unknown` and the host later receives a join classification other than `unknown` for the same IEEE, the host SHALL update the stored type to that classification. The slave SHALL persist the same type on its device dump so a host pull cannot replace a known type with `unknown`.

#### Scenario: Save from found list
- **WHEN** the operator saves a found device whose join type is `iasZone`
- **THEN** the registered record has type `iasZone`

#### Scenario: Operator cannot overwrite a known type
- **WHEN** a registered device already has type `onOff` and the operator saves parameters with a different type in the request
- **THEN** the record still has type `onOff`

#### Scenario: Unknown fills in later
- **WHEN** a registered IEEE is `unknown` and a later join classifies it as `windowCovering`
- **THEN** the registered record becomes `windowCovering`

### Requirement: Device list JSON includes type
Host device list APIs and the persisted device store SHALL include the stored type for each registered device so the console and future actions can read it.

#### Scenario: List after save
- **WHEN** the operator requests the registered device list and a device was saved as `onOff`
- **THEN** that device’s JSON includes type `onOff`

## MODIFIED Requirements

### Requirement: Search opens pairing and lists found unregistered devices
Starting a device search from the host SHALL open the slave join window. While search is active, each slave join report for an IEEE that is not already registered SHALL appear in the host found-device list without waiting for a page reload. A found entry SHALL include at least IEEE and a device type (`unknown` if the slave did not send one), and, when the slave sent them, manufacturer and model.

#### Scenario: Search then join
- **WHEN** the operator starts Search and a new unregistered device joins
- **THEN** that device appears in the found list while Search is still open

#### Scenario: Already registered ignored
- **WHEN** a join report arrives for an IEEE that is already registered
- **THEN** that IEEE is not added to the found list

#### Scenario: Found type from join
- **WHEN** an unregistered device joins with classified type `onOff`
- **THEN** the found list entry for that IEEE includes type `onOff`
