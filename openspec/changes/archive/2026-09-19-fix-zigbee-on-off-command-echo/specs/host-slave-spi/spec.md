# Spec Delta

## ADDED Requirements

### Requirement: Latest device control command wins per destination
When the host has not yet delivered a device-control command (`on/off`-style or write-attribute) to the slave for a given IEEE and destination endpoint, a newer device-control command for that same IEEE and endpoint SHALL replace the pending one. The host SHALL NOT keep a FIFO of opposite ON and OFF (or mixed on/off and write-attribute) for that destination. Commands for other IEEE values, other endpoints, or non-device-control SPI commands SHALL remain queued independently. The slave SHALL likewise keep only the latest deferred device-control command per IEEE and endpoint until the radio path accepts it.

#### Scenario: Host queue of ON then OFF
- **WHEN** the host enqueues ON and then OFF for the same IEEE and endpoint before the slave has taken the first command
- **THEN** the slave is given OFF for that destination and is not given a leftover ON after OFF

#### Scenario: Mixed write and on/off
- **WHEN** the host enqueues a write-attribute and then an on/off command for the same IEEE and endpoint before the first is delivered
- **THEN** only the later on/off command is delivered for that destination

#### Scenario: Other destinations stay queued
- **WHEN** the host enqueues OFF for IEEE A endpoint 1 and ON for IEEE B endpoint 1
- **THEN** both commands remain and each destination still receives its own command
