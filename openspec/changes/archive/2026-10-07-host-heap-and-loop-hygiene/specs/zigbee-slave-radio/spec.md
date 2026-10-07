# Spec Delta

## ADDED Requirements

### Requirement: ZCL send uses a timed Zigbee lock
When the slave sends a cluster-specific ZCL command for a host device-control request, it MUST acquire the Zigbee stack lock with a finite timeout. If the lock is not obtained in time, that send MUST fail without stopping the coordinator and MUST leave SPI drain and the slave main loop able to continue. The slave MUST NOT wait forever on that lock for this path.

#### Scenario: Lock busy fails one send
- **WHEN** a host cluster-command request runs while the Zigbee lock is held longer than the send timeout
- **THEN** that send fails, the coordinator keeps running, and later SPI or radio work can still proceed

#### Scenario: Lock free sends command
- **WHEN** a host cluster-command request runs and the Zigbee lock is available within the timeout
- **THEN** the slave sends the cluster-specific ZCL command as before
