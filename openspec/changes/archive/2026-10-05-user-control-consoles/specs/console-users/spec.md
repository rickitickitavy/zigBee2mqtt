# Spec Delta

## MODIFIED Requirements

### Requirement: Users table fields
The host MUST persist a users table. Each user MUST have a unique user name, a password hash (never a plaintext password), an added-at timestamp, a list of roles, `isBlocked`, a theme (`light` or `dark`), and `consoles` (zero or more control-console ids). Roles are `isAdmin`, `editDevices`, `addDevices`, `removeDevices`, `editUsers`, and `editConsoles`. A user with none of those roles MUST be treated as a simple user. `isAdmin` MUST grant every console right, including `editConsoles` (mutate consoles and open Consoles), even when `editConsoles` is false on that record. The persisted file MUST NOT store the password in clear text.

#### Scenario: Persist and reload a user
- **WHEN** an `isAdmin` operator creates user `alice` with role `editDevices`, theme `dark`, and a password
- **THEN** after reboot the host still has `alice` with `editDevices`, theme `dark`, the same added-at, and a stored hash that is not the typed password

#### Scenario: Simple user has no extra roles
- **WHEN** a user exists with no roles set
- **THEN** that user is a simple user and does not receive `isAdmin` or the five extra roles

#### Scenario: Persist consoles assignment
- **WHEN** an operator saves user `bob` with `consoles` containing `Kitchen` and `Hall`
- **THEN** after reboot `bob` still has those two consoles assigned

#### Scenario: Persist Console editor role
- **WHEN** an operator saves user `eve` with `editConsoles`
- **THEN** after reboot `eve` still has `editConsoles`

#### Scenario: Admin has Console editor without the flag
- **WHEN** user `admin` has `isAdmin` and `editConsoles` is false
- **THEN** that user may still create and save control consoles

### Requirement: APIs require a session and a role
Unauthenticated requests to protected console APIs MUST be rejected (no settings or device mutation). `isAdmin` MUST be allowed every protected API. A simple user MUST be allowed device list, device actions (row actions and Manual command), reading the control consoles assigned to that user, and reading/saving that user’s theme. A simple user MUST NOT add, edit parameters of, or delete devices, MUST NOT mutate consoles, and MUST NOT read or write Wi-Fi, MQTT, Zigbee, hardware, OTA, settings export/restore, or the users table. Extra roles MUST add only the matching APIs: `editDevices` edit parameters; `addDevices` add; `removeDevices` delete; `editUsers` list and mutate non-admin users; `editConsoles` list and mutate consoles. Only `isAdmin` MAY create a user with `isAdmin` or change an `isAdmin` user.

#### Scenario: No session
- **WHEN** a client calls a protected API without a session
- **THEN** the host does not apply the change and does not return privileged settings

#### Scenario: Simple user cannot add a device
- **WHEN** a simple-user session posts a new registered device
- **THEN** the host does not persist the device

#### Scenario: editUsers cannot grant admin
- **WHEN** a session that has `editUsers` but not `isAdmin` creates or updates a user with `isAdmin`
- **THEN** the host rejects the write

#### Scenario: editUsers cannot change an admin
- **WHEN** a session that has `editUsers` but not `isAdmin` updates or deletes a user who has `isAdmin`
- **THEN** the host rejects the write

#### Scenario: Simple user cannot mutate consoles
- **WHEN** a simple-user session posts a console update
- **THEN** the host does not persist it

#### Scenario: editConsoles can save a console
- **WHEN** a session that has `editConsoles` and not `isAdmin` saves a console layout
- **THEN** the host persists it

#### Scenario: Admin can save a console without editConsoles
- **WHEN** a session that has `isAdmin` and not `editConsoles` saves a console layout
- **THEN** the host persists it

## ADDED Requirements

### Requirement: User consoles assignment round-trips
`consoles` and `editConsoles` MUST round-trip through single-user save, the users list, settings export/restore, and reboot. They are settings, not live telemetry. Host↔slave user sync MUST carry `editConsoles`. Assigned console ids MUST live on the host; unpack of a slave user frame MUST merge existing host `consoles` for that user name so sync MUST NOT drop assignments.

#### Scenario: List after save
- **WHEN** `alice` is saved with `editConsoles` and two assigned consoles
- **THEN** the users list JSON for `alice` includes `editConsoles` true and those console ids

#### Scenario: Sync keeps assignment
- **WHEN** the host syncs users with the slave after `alice` has assigned consoles
- **THEN** `alice` still has those consoles on the host
