# Spec Delta

## ADDED Requirements

### Requirement: Host honors a temporary slave-reset pause

The host SHALL support a runtime slave-reset pause of at most 120 seconds (or until cancelled) so the slave can be programmed over USB. While active, the host MUST NOT assert slave EN/RST for any host-initiated reset, including sync helpers, and MUST leave EN released (not held in reset). Pause state and remaining time MUST be queryable by the admin Update APIs. After the pause ends, normal reset rules resume.

#### Scenario: Link-loss reset suppressed

- **WHEN** the slave-reset pause is active and the host would otherwise reset the slave for SPI link loss or keepalive silence
- **THEN** the host does not assert EN/RST for that recovery and continues without that reset pulse

#### Scenario: Synchronous OTA verify reset suppressed

- **WHEN** the slave-reset pause is active and firmware OTA would call a synchronous slave reset for version verification
- **THEN** the host does not pulse EN/RST for that verify attempt

#### Scenario: Pause ends

- **WHEN** the pause expires or is cancelled
- **THEN** a later link-loss or bringup failure MAY reset the slave again under existing rules

#### Scenario: Boot sync reset out of scope

- **WHEN** the host performs its first sync slave reset during boot before the console is available
- **THEN** that boot reset is not gated by the Update-tab pause (pause applies only after the console can start it)
