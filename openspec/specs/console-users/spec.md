# console-users Specification

## Purpose

Stores console operators on the host, seeds a first admin when the table is empty, issues sessions that are not HTTP Basic, and enforces roles on HTTP APIs.

## Requirements

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

### Requirement: Seed admin when the table is empty
On every host boot the host MUST load the users table. If the table has zero users, the host MUST create user name `admin`, password `admin`, role `isAdmin`, not blocked, and theme `dark`. If at least one user already exists, the host MUST NOT add this seed user again.

#### Scenario: First boot empty table
- **WHEN** the host boots and the users table is empty
- **THEN** user `admin` exists with password `admin` and `isAdmin`

#### Scenario: Later boot with users
- **WHEN** the host boots and the table already has one or more users
- **THEN** the host does not insert another seed `admin`

### Requirement: Session login is not HTTP Basic
The host MUST accept a login request with user name and password and, on success, MUST start a session (cookie or bearer token). The host MUST NOT use HTTP Basic authentication. A blocked user MUST NOT receive a session. A wrong user name or password MUST fail without creating a session. USB CLI, MQTT, and Zigbee MUST remain usable without a console session.

#### Scenario: Valid login
- **WHEN** user `admin` posts the correct password to login
- **THEN** later API calls that carry that session succeed

#### Scenario: Blocked user
- **WHEN** a user is blocked and posts the correct password
- **THEN** the host does not start a session

#### Scenario: MQTT without session
- **WHEN** a broker publishes a device command and no console session exists
- **THEN** the host still applies that MQTT command as it does today

### Requirement: Remember me is ignored for admin
A successful login MAY request Remember me. When the user has `isAdmin`, the host MUST ignore Remember me. An `isAdmin` session MUST expire after **10 minutes with no operator activity**; operator activity MUST reset that idle timer. Automatic authenticated requests (session-alive polls, status polls, log polls, and other requests the console issues without a pointer or keyboard action) MUST NOT reset the idle timer. The session cookie MUST remain valid in the browser for at least the remaining idle window so cookie `Max-Age` from login MUST NOT log the operator out while idle time has not elapsed. When the user is not `isAdmin` and Remember me is set, the host MUST issue a longer-lived session that survives a browser restart.

#### Scenario: Admin checks Remember me
- **WHEN** `admin` logs in with Remember me checked
- **THEN** the session still expires after 10 minutes idle, the same as if Remember me were unchecked

#### Scenario: Admin idle timeout
- **WHEN** an `isAdmin` session has had no operator activity for 10 minutes
- **THEN** the next protected API call is rejected and the operator must sign in again

#### Scenario: Admin activity resets idle
- **WHEN** an `isAdmin` operator uses the signed-in console at 9 minutes idle
- **THEN** the idle timer restarts and the session stays valid

#### Scenario: Admin poll does not reset idle
- **WHEN** an `isAdmin` session sits unused for 10 minutes while the console still polls session status
- **THEN** the session expires and the operator must sign in again

#### Scenario: Admin cookie does not expire on login wall-clock
- **WHEN** an `isAdmin` operator uses the console continuously for more than 10 minutes after login
- **THEN** the browser still sends the session and the host still accepts it

#### Scenario: Simple user Remember me
- **WHEN** a simple user logs in with Remember me checked
- **THEN** a later browser restart still presents a valid session until it expires

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

### Requirement: At least one unlocked admin remains
After every successful users-table write the table MUST still contain at least one user who has `isAdmin` and is not blocked. The host MUST reject create, update, delete, and settings-restore replacement when the resulting table would have zero unlocked `isAdmin` users. Unlocking or adding another `isAdmin` MUST still be allowed. Counting MUST treat a blocked `isAdmin` as not satisfying this rule.

#### Scenario: Block last unlocked admin
- **WHEN** the table has one `isAdmin` who is not blocked and an operator sets that user to blocked
- **THEN** the host rejects the write and the user stays unblocked

#### Scenario: Delete last unlocked admin
- **WHEN** the table has one unlocked `isAdmin` and that user is deleted
- **THEN** the host rejects the delete

#### Scenario: Demote last unlocked admin
- **WHEN** the table has one unlocked `isAdmin` and that user’s `isAdmin` is cleared
- **THEN** the host rejects the write

#### Scenario: Restore without unlocked admin
- **WHEN** settings restore supplies a users list with only blocked admins or no admins
- **THEN** the host does not replace the users table

#### Scenario: Second admin can be blocked
- **WHEN** the table has two unlocked `isAdmin` users and one of them is blocked
- **THEN** the host accepts the write and the other `isAdmin` stays unlocked

### Requirement: User consoles assignment round-trips
`consoles` and `editConsoles` MUST round-trip through single-user save, the users list, settings export/restore, and reboot. They are settings, not live telemetry. Host↔slave user sync MUST carry `editConsoles`. Assigned console ids MUST live on the host; unpack of a slave user frame MUST merge existing host `consoles` for that user name so sync MUST NOT drop assignments.

#### Scenario: List after save
- **WHEN** `alice` is saved with `editConsoles` and two assigned consoles
- **THEN** the users list JSON for `alice` includes `editConsoles` true and those console ids

#### Scenario: Sync keeps assignment
- **WHEN** the host syncs users with the slave after `alice` has assigned consoles
- **THEN** `alice` still has those consoles on the host
