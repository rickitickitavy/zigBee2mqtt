# Spec Delta

## MODIFIED Requirements

### Requirement: Users table fields

The host MUST persist a users table. Each user MUST have a unique user name, a password hash (never a plaintext password), an added-at timestamp, a list of roles, `isBlocked`, a theme (`light` or `dark`), and `consoles` (zero or more control-console ids). Roles are `isAdmin`, `editDevices`, `addDevices`, `removeDevices`, `editUsers`, `editConsoles`, and `controlDevices` (Other device control). A user with none of those roles MUST be treated as a simple user. `isAdmin` MUST grant every console right, including `editConsoles` and `controlDevices`, even when those flags are false on that record. The persisted file MUST NOT store the password in clear text.

#### Scenario: Persist and reload a user

- **WHEN** an `isAdmin` operator creates user `alice` with role `editDevices`, theme `dark`, and a password
- **THEN** after reboot the host still has `alice` with `editDevices`, theme `dark`, the same added-at, and a stored hash that is not the typed password

#### Scenario: Simple user has no extra roles

- **WHEN** a user exists with no roles set
- **THEN** that user is a simple user and does not receive `isAdmin` or the extra roles

#### Scenario: Persist consoles assignment

- **WHEN** an operator saves user `bob` with `consoles` containing `Kitchen` and `Hall`
- **THEN** after reboot `bob` still has those two consoles assigned

#### Scenario: Persist Console editor role

- **WHEN** an operator saves user `eve` with `editConsoles`
- **THEN** after reboot `eve` still has `editConsoles`

#### Scenario: Admin has Console editor without the flag

- **WHEN** user `admin` has `isAdmin` and `editConsoles` is false
- **THEN** that user may still create and save control consoles

#### Scenario: Persist Other device control

- **WHEN** an operator saves user `cara` with `controlDevices`
- **THEN** after reboot `cara` still has `controlDevices`

### Requirement: APIs require a session and a role

Unauthenticated requests to protected console APIs MUST be rejected (no settings or device mutation). `isAdmin` MUST be allowed every protected API. A simple user MUST be allowed device list, reading the control consoles assigned to that user, and reading/saving that user’s theme. A simple user MUST NOT send Devices-table device commands (row actions or Manual command), MUST NOT add, edit parameters of, or delete devices, MUST NOT mutate consoles, and MUST NOT read or write Wi-Fi, MQTT, Zigbee, hardware, OTA, settings export/restore, or the users table. Extra roles MUST add only the matching APIs: `editDevices` edit parameters; `addDevices` add; `removeDevices` delete; `editUsers` list and mutate non-admin users; `editConsoles` list and mutate consoles; `controlDevices` Devices-table command paths (row actions and Manual command). Only `isAdmin` MAY create a user with `isAdmin` or change an `isAdmin` user.

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

#### Scenario: Without controlDevices cannot command from Devices

- **WHEN** a session lacks `controlDevices` and `isAdmin` and posts a Devices-table device command
- **THEN** the host rejects the command

#### Scenario: controlDevices can command from Devices

- **WHEN** a session has `controlDevices` and not `isAdmin` and posts a Devices-table device command
- **THEN** the host applies the command the same way as for an admin
