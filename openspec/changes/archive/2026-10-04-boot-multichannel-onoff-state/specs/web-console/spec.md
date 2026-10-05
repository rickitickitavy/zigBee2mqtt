# Spec Delta

## MODIFIED Requirements

### Requirement: Devices table shows type-specific status
The status cell SHALL show the last known state for the device type. For `onOff`, each channel SHALL be a small circle: dark for OFF and light for ON. For other types, each channel SHALL show the last report text for that endpoint. When `channels` is `1`, the cell SHALL show one indicator. When `channels` is `0` or `2` through `16`, the cell SHALL show one indicator per channel, bound to inbound reports by the packet `ep`. A channel with no report yet SHALL show an empty or unknown indicator, not a guessed ON. After a boot status read, each `onOff` channel SHALL show the ON or OFF reported for that same endpoint.

#### Scenario: Single-channel on/off on
- **WHEN** a registered `onOff` device has `channels` `1` and last state ON
- **THEN** that row’s status cell shows one light circle

#### Scenario: Parse-mode two endpoints
- **WHEN** a registered `onOff` device has `channels` `0`, endpoint 1 last reported OFF, and endpoint 3 last reported ON
- **THEN** that row’s status cell shows a dark circle for endpoint 1 and a light circle for endpoint 3

#### Scenario: IAS zone text
- **WHEN** a registered `iasZone` device last reported `LEAK` on its status endpoint
- **THEN** that row’s status cell shows `LEAK`

#### Scenario: Four-channel relay after boot
- **WHEN** a registered `onOff` device has `channels` `4` and the boot read reported ON on endpoints 1 and 3 and OFF on endpoints 2 and 4
- **THEN** that row’s status cell shows light, dark, light, dark circles in channel order
