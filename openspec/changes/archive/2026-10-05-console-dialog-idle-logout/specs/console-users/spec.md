# Spec Delta

## MODIFIED Requirements

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
