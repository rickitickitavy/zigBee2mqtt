## ADDED Requirements

### Requirement: Host can command a ZCL write attribute
The SPI protocol SHALL carry a host-to-slave write-attribute command that includes destination IEEE, destination endpoint, cluster id, attribute id, ZCL data type, and value. The slave SHALL transmit a ZCL write-attribute to that destination. The slave SHALL NOT substitute a different cached bind endpoint when a usable endpoint was provided. The slave SHALL NOT decide write versus on/off from MQTT or device settings; it SHALL execute the framed write as given.

#### Scenario: Write on/off attribute
- **WHEN** the host enqueues a write-attribute for an IEEE with endpoint 2, cluster `0x0006`, attribute `0x0000`, type unsigned 8-bit, value `1`
- **THEN** the slave transmits that write to endpoint 2 of that IEEE

#### Scenario: Usable endpoint is honored
- **WHEN** the host enqueues a write-attribute with endpoint 4 and the slave has a different cached bind endpoint for that IEEE
- **THEN** the slave addresses endpoint 4
