# Spec Delta

## ADDED Requirements

### Requirement: Classify device type from in-clusters
When the slave identifies a joining or newly bound device, it SHALL inspect that IEEE’s advertised **in-clusters** (simple descriptor on each usable endpoint) and assign exactly one device type for the IEEE. Classification SHALL use this priority, first match wins across all those in-clusters: IAS Zone (`0x0500`) → `iasZone`; Window Covering (`0x0102`) → `windowCovering`; On/Off (`0x0006`) → `onOff`. If none of those clusters are present, the type SHALL be `unknown`. Manufacturer name, model, and friendly name SHALL NOT decide the type. The slave SHALL classify before or with the join report that the host uses for pairing search. Type-specific command behavior is not required in this change.

#### Scenario: On/Off switch
- **WHEN** a joining device advertises in-cluster `0x0006` and does not advertise `0x0500` or `0x0102`
- **THEN** the slave classifies that IEEE as `onOff`

#### Scenario: Leak or IAS sensor
- **WHEN** a joining device advertises in-cluster `0x0500` and also `0x0006`
- **THEN** the slave classifies that IEEE as `iasZone`

#### Scenario: No matching cluster
- **WHEN** a joining device advertises only clusters outside the classification list
- **THEN** the slave classifies that IEEE as `unknown`
