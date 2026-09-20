# Spec Delta

## ADDED Requirements

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
