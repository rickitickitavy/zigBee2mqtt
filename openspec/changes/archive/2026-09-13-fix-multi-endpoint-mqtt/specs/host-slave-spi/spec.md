## ADDED Requirements

### Requirement: Device commands carry destination endpoint and message
A host-to-slave device command SHALL include the destination IEEE, the destination Zigbee endpoint, and the command message text. The slave SHALL address that endpoint. When the message is an on/off/toggle command the slave SHALL send the matching ZCL on/off. The slave SHALL NOT substitute a different cached bind endpoint when a usable endpoint was provided.

#### Scenario: Command endpoint 4
- **WHEN** the host enqueues a command for an IEEE with endpoint 4 and message `off`
- **THEN** the slave transmits off to endpoint 4 of that IEEE

#### Scenario: Two gangs independently
- **WHEN** the host enqueues `off` for endpoint 1 and `on` for endpoint 3 of the same IEEE
- **THEN** the slave addresses endpoint 1 for the first command and endpoint 3 for the second

### Requirement: Attribute reports carry the device message text
A slave-to-host attribute or zone report SHALL include IEEE, source endpoint, short address, and the message text to publish. The host SHALL use that text as the MQTT payload body (after any channels mapping).

#### Scenario: IAS report
- **WHEN** the slave reports `LEAK` from an endpoint
- **THEN** the host receives that message text with that endpoint
