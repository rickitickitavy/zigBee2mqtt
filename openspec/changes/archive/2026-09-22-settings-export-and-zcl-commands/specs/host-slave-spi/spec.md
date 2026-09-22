## ADDED Requirements

### Requirement: Host can send a ZCL cluster command
The SPI protocol SHALL carry a host-to-slave cluster-specific ZCL command that includes destination IEEE, destination endpoint, cluster id, ZCL command id, and optional command payload bytes. The slave SHALL transmit that cluster command to that destination. The slave SHALL NOT substitute a different cached bind endpoint when a usable endpoint was provided. The slave SHALL NOT treat this frame as write-attribute or on/off.

#### Scenario: Covering stop
- **WHEN** the host enqueues a cluster command for an IEEE with endpoint 1, cluster `0x0102`, command `0x02`, and no payload
- **THEN** the slave transmits ZCL command `0x02` on cluster `0x0102` to endpoint 1

#### Scenario: Usable endpoint is honored
- **WHEN** the host enqueues a cluster command with endpoint 3 and the slave has a different cached bind endpoint for that IEEE
- **THEN** the slave addresses endpoint 3
