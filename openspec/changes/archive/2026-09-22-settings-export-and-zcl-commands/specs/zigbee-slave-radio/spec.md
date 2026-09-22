## ADDED Requirements

### Requirement: Radio sends cluster-specific ZCL commands
When the slave executes a host cluster-command request, it SHALL send a cluster-specific ZCL command (not a write-attribute and not On/Off cluster-specific unless that cluster and command were given) to the given IEEE and endpoint. One in-flight device-control command per IEEE and endpoint SHALL include this send the same way as on/off and write-attribute. A missing short address SHALL fail that send without stopping the coordinator.

#### Scenario: Window covering move
- **WHEN** the slave is asked to send cluster `0x0102` command `0x00` to a known short address and endpoint
- **THEN** that device receives the Up/Open covering command on the air

#### Scenario: No short address
- **WHEN** the slave is asked to send a cluster command to a registered IEEE that has no short address
- **THEN** the coordinator stays up and that command is not delivered
