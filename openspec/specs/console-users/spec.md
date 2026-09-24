# console-users Specification

## Purpose

Stores console operators on the host, seeds a first admin when the table is empty, issues sessions that are not HTTP Basic, and enforces roles on HTTP APIs.

## Requirements

### Requirement: Users table fields
The host MUST persist a users table. Each user MUST have a unique user name, a password hash (never a plaintext password), an added-at timestamp, a list of roles, `isBlocked`, and a theme (`light` or `dark`). Roles are `isAdmin`, `editDevices`, `addDevices`, `removeDevices`, and `editUsers`. A user with none of those roles MUST be treated as a simple user. `isAdmin` MUST grant every console right. The persisted file MUST NOT store the password in clear text.

#### Scenario: Persist and reload a user
- **WHEN** an `isAdmin` operator creates user `alice` with role `editDevices`, theme `dark`, and a password
- **THEN** after reboot the host still has `alice` with `editDevices`, theme `dark`, the same added-at, and a stored hash that is not the typed password

#### Scenario: Simple user has no extra roles
- **WHEN** a user exists with no roles set
- **THEN** that user is a simple user and does not receive `isAdmin` or the four extra roles

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
A successful login MAY request Remember me. When the user has `isAdmin`, the host MUST ignore Remember me. An `isAdmin` session MUST expire after **10 minutes with no authenticated request**; each successful authenticated API call MUST reset that idle timer. When the user is not `isAdmin` and Remember me is set, the host MUST issue a longer-lived session that survives a browser restart.

#### Scenario: Admin checks Remember me
- **WHEN** `admin` logs in with Remember me checked
- **THEN** the session still expires after 10 minutes idle, the same as if Remember me were unchecked

#### Scenario: Admin idle timeout
- **WHEN** an `isAdmin` session has had no authenticated request for 10 minutes
- **THEN** the next protected API call is rejected and the operator must sign in again

#### Scenario: Admin activity resets idle
- **WHEN** an `isAdmin` session receives an authenticated request at 9 minutes idle
- **THEN** the idle timer restarts and the session stays valid

#### Scenario: Simple user Remember me
- **WHEN** a simple user logs in with Remember me checked
- **THEN** a later browser restart still presents a valid session until it expires

### Requirement: APIs require a session and a role
Unauthenticated requests to protected console APIs MUST be rejected (no settings or device mutation). `isAdmin` MUST be allowed every protected API. A simple user MUST be allowed status, device list, log, device actions (row actions and Manual command), and reading/saving that user’s theme. A simple user MUST NOT add, edit parameters of, or delete devices, and MUST NOT read or write Wi-Fi, MQTT, Zigbee, hardware, OTA, settings export/restore, or the users table. Extra roles MUST add only the matching APIs: `editDevices` edit parameters; `addDevices` add; `removeDevices` delete; `editUsers` list and mutate non-admin users. Only `isAdmin` MAY create a user with `isAdmin` or change an `isAdmin` user.

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
