## ADDED Requirements

### Requirement: Slave signals join window closed
When the slave permit-join / pairing window ends (timer or explicit close), the slave SHALL notify the host over SPI. The host SHALL treat pairing as inactive after that event so the console can enable Search again.

#### Scenario: Window expires
- **WHEN** the slave join window duration elapses while Search is running
- **THEN** the host learns pairing is inactive without the operator closing the dialog

### Requirement: Inbound Zigbee packets carry last RSSI
A slave-to-host inbound Zigbee packet used for device reports SHALL include RSSI of that packet. The host SHALL store that RSSI as the last received signal for that IEEE until a later packet replaces it.

#### Scenario: Report includes RSSI
- **WHEN** the slave forwards an attribute report from an IEEE with RSSI −70 dBm
- **THEN** the host stores −70 dBm as last signal for that IEEE

## MODIFIED Requirements

### Requirement: Attribute reports carry the device message text
A slave-to-host attribute or zone report SHALL include IEEE, source endpoint, short address, RSSI of that packet, and the message text to publish. The host SHALL use that text as the MQTT payload body (after any channels mapping).

#### Scenario: IAS report
- **WHEN** the slave reports `LEAK` from an endpoint
- **THEN** the host receives that message text with that endpoint

#### Scenario: RSSI on report
- **WHEN** the slave reports from an IEEE with RSSI −55 dBm
- **THEN** the host receives −55 dBm with that IEEE
