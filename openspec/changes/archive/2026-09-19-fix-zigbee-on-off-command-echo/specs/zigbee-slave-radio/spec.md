# Spec Delta

## ADDED Requirements

### Requirement: One in-flight command per destination
The slave SHALL transmit at most one Zigbee on/off or write-attribute command at a time to a given IEEE and destination endpoint. While that command is in flight, a newer on/off or write-attribute for the same IEEE and endpoint SHALL replace the pending next command and SHALL NOT be put on the air until the in-flight command finishes or a send timeout expires. After finish or timeout, the slave SHALL send only that latest pending command, or none if none remains. Distinct endpoints of the same IEEE MAY have independent in-flight commands. Permit-join and other non-device-control radio work SHALL NOT be serialized behind this rule.

#### Scenario: Rapid ON then OFF
- **WHEN** the operator sends ON then OFF to the same IEEE and endpoint faster than the first command completes on air
- **THEN** the device receives at most the in-flight command plus a later OFF, and does not keep switching after OFF has been applied and in-flight work has finished

#### Scenario: Echo fades to last command
- **WHEN** ON and OFF are repeated several times to one endpoint and then commands stop
- **THEN** the device settles on the last commanded state and does not continue random ON/OFF from leftover earlier commands

#### Scenario: Two endpoints stay independent
- **WHEN** endpoint 1 is commanded OFF and endpoint 3 of the same IEEE is commanded ON before endpoint 1 completes
- **THEN** both destinations still receive their own commands
