## ADDED Requirements

### Requirement: MQTT SERVER TYPE replaces ENABLED
The MQTT section MUST show SERVER TYPE as a three-option control (`disable`, `remote`, `local`) and MUST NOT show an ENABLED checkbox. GET `/api/mqtt` MUST return `serverType` as one of those strings. POST `/api/mqtt` MUST persist `serverType` and MUST NOT require a boolean `enabled`. Factory default SERVER TYPE MUST be `disable`. Settings export and restore MUST include `mqtt.serverType`. Restore of a file that still has `mqtt.enabled` and no `serverType` MUST map `false` to `disable` and `true` to `remote`.

#### Scenario: Open MQTT card
- **WHEN** the operator opens the MQTT section
- **THEN** SERVER TYPE shows the stored value and ENABLED is not shown

#### Scenario: Save local
- **WHEN** the operator sets SERVER TYPE to `local` and saves
- **THEN** GET `/api/mqtt` returns `"serverType":"local"`

#### Scenario: Restore legacy enabled
- **WHEN** an admin restores a file whose MQTT object has `"enabled":true` and no `serverType`
- **THEN** stored SERVER TYPE is `remote`

### Requirement: SERVER field hidden when type is local
When SERVER TYPE is `local`, the MQTT SERVER field MUST be hidden and POST `/api/mqtt` MUST NOT require `server`. When SERVER TYPE is `disable` or `remote`, the SERVER field MUST be visible. Empty SERVER on save in those modes MAY keep the stored default host string.

#### Scenario: Local hides SERVER
- **WHEN** the operator selects SERVER TYPE `local`
- **THEN** the SERVER field is not shown

#### Scenario: Remote shows SERVER
- **WHEN** the operator selects SERVER TYPE `remote`
- **THEN** the SERVER field is shown
