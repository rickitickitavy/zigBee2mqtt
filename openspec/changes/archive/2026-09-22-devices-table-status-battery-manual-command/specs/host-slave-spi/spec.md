# Spec Delta

## ADDED Requirements

### Requirement: Attribute reports update last status and battery
A slave-to-host attribute or zone report SHALL continue to include IEEE, source endpoint, short address, RSSI, and message text. The host SHALL cache the message against that IEEE and `ep` as last status, except when the message is a battery report (`BATTERY <percent>` from cluster `0x0001` attribute Battery Percentage Remaining). A battery report SHALL update last battery percent for that IEEE and SHALL NOT replace per-endpoint on/off or zone status. The slave SHALL NOT persist last status or battery.

#### Scenario: On/off report caches endpoint
- **WHEN** the slave reports `ON` from endpoint 3
- **THEN** the host last status for that IEEE endpoint 3 is `ON`

#### Scenario: Battery does not overwrite channel state
- **WHEN** endpoint 1 last status is `OFF` and a later battery report arrives for that IEEE
- **THEN** last battery is the decoded percent and endpoint 1 status remains `OFF`

### Requirement: Host can request a ZCL attribute read
The SPI protocol SHALL carry a host-to-slave read-attribute command that includes destination IEEE, destination endpoint, cluster id, and attribute id. The slave SHALL transmit a ZCL read-attribute to that destination. When the device answers, the slave SHALL deliver the value on the existing attribute-report path (IEEE, `ep`, RSSI, message). The slave SHALL NOT substitute a different cached bind endpoint when a usable endpoint was provided.

#### Scenario: Read on/off
- **WHEN** the host enqueues a read-attribute for an IEEE with endpoint 2, cluster `0x0006`, attribute `0x0000`
- **THEN** the slave transmits that read to endpoint 2

#### Scenario: Read response uses report path
- **WHEN** that device answers ON
- **THEN** the host receives an attribute report for that IEEE and endpoint with message `ON`
