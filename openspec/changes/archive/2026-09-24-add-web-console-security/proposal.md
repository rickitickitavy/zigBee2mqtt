## Why

The web console is open to anyone who can reach the host IP. That is unsafe on STA and on a long-lived SoftAP. The gateway needs a first security layer: stored users, hashed passwords, sessions (not HTTP Basic), and roles that hide tabs and block privileged APIs. Future features MUST declare which role may use them, or introduce a new role, and that rule belongs in a project ADR.

## What Changes

- Persist a host users table (LittleFS). Each user has user name, password **hash only**, added-at, roles, `isBlocked`, and theme. A user with no extra roles is a simple user.
- Roles: `isAdmin` (all rights), `editDevices`, `addDevices`, `removeDevices`, `editUsers`.
- On every host boot, if the users table is empty, create user `admin` / password `admin` with `isAdmin`.
- Add a session authorization method (cookie/token, not HTTP Basic). Blocked users cannot sign in. USB CLI, MQTT, and Zigbee stay unauthenticated.
- **BREAKING:** Opening the console URL without a valid session shows a dark login form (user name, password, Remember me). Login UI details beyond those fields come later. `isAdmin` ignores Remember me; an admin session lasts **10 minutes of idle** (any authenticated request resets the timer).
- After login, the console hides sections the role cannot use and the host rejects those APIs.
  - Simple user: Status, Devices, System → Log, System → Maintenance → Appearance. Device row/manual actions allowed. No add / edit parameters / remove devices.
  - `editDevices`: edit device parameters.
  - `addDevices`: add devices (not edit unless `editDevices`).
  - `removeDevices`: delete devices (confirm in UI).
  - `editUsers`: System → Security; may edit users **except** any `isAdmin` user; cannot grant `isAdmin`.
  - `isAdmin`: all tabs and operations; only this role may create or edit `isAdmin` users.
- Theme is **per user**. The Theme control stays on System → Maintenance → Appearance (same place as today); Save writes the signed-in user’s theme.
- Settings export/restore stay `isAdmin` only and **include the users table** (hashes and salts, never plaintext passwords).
- Add the first project ADR set under `docs/adr/`:
  - `0001` console RBAC (ask role or new role on every new feature)
  - `0002` two chips, one image (GPIO15; host vs slave duties)
  - `0003` host is the only settings store
  - `0004` SPI is always asynchronous
  - `0005` auth boundary (console session only; USB/MQTT stay open)
  - `0006` settings export policy (no Wi-Fi; users with hash/salt; admin only)
  - `0007` settings vs telemetry (persist identity; do not persist RSSI/online/live radio)
  - `0008` OTA order (slave over SPI first, then host)
- Firmware build **0.2.14** (bumped at propose).

## Capabilities

### New Capabilities

- `console-users`: user store, boot seed, password hashing, sessions, role checks on HTTP APIs.

### Modified Capabilities

- `web-console`: login gate (not Basic); role-visible tabs; Security tab; Appearance stays on Maintenance and saves the signed-in user’s theme; export/restore include users (hashes only); current “no HTTP authentication” requirement is replaced.

## Impact

- Host only: new `UserStore` / session helper, `/users.json` on LittleFS, `WebConsole` auth filter on `/api/*` and privileged routes.
- `data/index.html` + `data/css/all.css`: login overlay, Security tab, hide Add/Edit/Delete by role.
- `docs/adr/0001`–`0008` (RBAC, two-chip split, host settings, async SPI, auth boundary, export policy, settings vs telemetry, OTA order).
- Settings export/restore: `isAdmin` only; JSON includes users with hashes/salts and no plaintext passwords.
- Existing web-console spec line “MUST NOT require HTTP authentication” is superseded.
