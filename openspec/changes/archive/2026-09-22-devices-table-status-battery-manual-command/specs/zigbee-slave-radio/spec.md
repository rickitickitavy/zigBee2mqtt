# Spec Delta

## ADDED Requirements

### Requirement: Coordinator start reads current device status
After the slave Zigbee coordinator has started with host settings, the slave SHALL request current status from every registered device it knows. For `onOff`, it SHALL read On/Off (`0x0006` / `0x0000`) on each usable channel endpoint for that device. For `iasZone`, it SHALL read the type’s current zone status. For `windowCovering`, it SHALL read the type’s current covering status. When a registered device advertises Power Configuration (`0x0001`), the slave SHALL also read Battery Percentage Remaining. Sleepy devices that do not answer SHALL leave cached status empty; the coordinator start SHALL still complete. Reads SHALL use the same inbound report path as unsolicited reports.

#### Scenario: Switch after start
- **WHEN** the coordinator starts and a registered `onOff` device with `channels` `1` answers ON
- **THEN** the host receives an `ON` report for that IEEE so the Devices table can show a light circle

#### Scenario: Multi-channel after start
- **WHEN** the coordinator starts and a registered `onOff` device has `channels` `3`
- **THEN** the slave reads On/Off on endpoints 1, 2, and 3

#### Scenario: Battery-capable device
- **WHEN** the coordinator starts and a registered device advertises in-cluster `0x0001` and answers 67 percent
- **THEN** the host receives a battery report that decodes to 67 percent

#### Scenario: Sleepy no answer
- **WHEN** the coordinator starts and a registered device does not answer the read
- **THEN** the coordinator remains running and that device’s status and battery stay unknown until a later report
