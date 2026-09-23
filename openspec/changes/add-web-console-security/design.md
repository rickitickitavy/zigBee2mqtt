## Context

See proposal.md — Why. Today `WebConsole` serves `/` and `/api/*` with no operator identity (`openspec/specs/web-console` still says authentication MUST NOT be required). Devices persist on host LittleFS as `/devices.json` via `DeviceTopicMap`. Theme is a host byte in `GlobalSettings.reserved[4]` (`UI_THEME_*`) and `GET`/`POST /api/theme`. System folder tabs are Log, Update, Hardware, Maintenance. There is no `docs/adr/` yet.

## Goals / Non-Goals

**Goals:**
- Host-side user file + boot seed + session cookie + role checks on APIs and chrome.
- Login overlay in existing console chrome (dark), not a second CSS framework.
- Record RBAC as ADR 0001 so later features name a role or add one.

**Non-Goals:**
- HTTP Basic, OAuth, LDAP, or per-MQTT credentials.
- Auth on USB CLI or the Zigbee/MQTT data path.
- Fancy login art; only user name, password, Remember me this change.
- Moving the Theme picker off System → Maintenance → Appearance.

## Decisions

1. **Capability split: `console-users` + `web-console`**  
   Store, seed, hash, session, and API deny rules live in a new host module. Chrome, login form, Security tab, and hidden controls stay in the console spec.  
   Alternative: one mega `web-console` spec — rejected; device-registry is already split the same way.

2. **LittleFS `/users.json` (host only)**  
   Same pattern as `DEVICES_STORE_PATH`. JSON array of users. Cap the table (16) so the file stays small. Slave LittleFS is unchanged.  
   Alternative: EEPROM struct — rejected; variable user count and hashes do not fit the reserved settings blob cleanly.

3. **Password: per-user salt + SHA-256 (mbedtls)**  
   Persist `passwordSalt` and `passwordHash` only. Compare in constant-ish time.  
   Alternative: plaintext — rejected. Alternative: PBKDF2 with high rounds — nicer, heavier on ESP32-C6 login; revisit if we add a stronger KDF later.

4. **Session cookie, not Basic**  
   `POST /api/auth/login` `{ userName, password, rememberMe }` sets an HttpOnly cookie with a random token. Host keeps a small in-RAM session table (token hash, user name, expiry / last activity). `POST /api/auth/logout` clears it. `GET /api/auth/me` returns user name, roles, theme for chrome.  
   Non-admin without Remember me: 12 hours. Remember me (non-admin): 30 days. `isAdmin`: ignore Remember me; expire after **10 minutes idle**; each authenticated request refreshes `lastActivity`.  
   Alternative: `Authorization: Basic` — rejected by the request. Alternative: JWT — unnecessary crypto and no clock source we want to depend on. Alternative: 12-hour admin session — rejected; admin idle is 10 minutes.

5. **Public vs protected HTTP**  
   Public: `GET /`, `GET /index.html`, `GET /css/*`, `POST /api/auth/login`. Everything else under `/api/` requires a valid session, then a role. Static HTML may still download; APIs and privileged UI stay closed.  
   Filter in `WebConsole` before each handler (or one `onNotFound`/middleware-style wrapper). Return 401 without a session, 403 when the role is missing.

6. **Role bits (not string ACL lists)**  
   Stored flags: `isAdmin`, `editDevices`, `addDevices`, `removeDevices`, `editUsers`. Empty flags = simple user. `isAdmin` short-circuits to allow.  
   Simple user APIs: `/api/status`, `/api/version`, `/api/devices` GET, `/api/log`, `/api/devices/command`, `/api/theme` GET/POST (own user), `/api/auth/me`, logout.  
   Map: `addDevices` → POST add; `editDevices` → POST update parameters; `removeDevices` → DELETE; `editUsers` → `/api/users*`; Wi-Fi/MQTT/Zigbee/hardware/OTA/export/restore → `isAdmin` only this change (those tabs are not in the simple-user list).  
   Alternative: new roles for Wi-Fi/MQTT now — rejected until a feature asks; ADR forces that question next time.

7. **`editUsers` vs `isAdmin`**  
   `editUsers` may CRUD users who are not `isAdmin` and may not set `isAdmin`. Cannot delete the last remaining `isAdmin` even as admin (avoid lockout except by wiping `/users.json` and reboot seed). Host enforces this even if the UI is bypassed.

8. **Theme is per user; the picker stays on Appearance**  
   After login, `GET /api/theme` and the existing System → Maintenance → Appearance Save read/write the session user’s `theme`. Do not move that row to Security or elsewhere. Login page forces `html[data-theme="dark"]` before session. Host `reserved[4]` stays as an unused leftover (do not bump settings version). Export `ui.theme` remains the signed-in admin’s theme for older restore files.

9. **Security is a System folder tab**  
   Label **Security** (correct spelling). Table + add/edit dialogs; delete confirms (web-ui rule). Password field only on create/reset, never echo hash.  
   Alternative: top-level sidebar “Security” — rejected; user called it a tab and System already groups operator tools.

10. **Boot seed in host `setup` after LittleFS mount**  
    `UserStore::loadOrSeed()` every boot. Missing file = empty = seed `admin`/`admin`/`isAdmin`/dark. Corrupt file: log, treat as empty, seed (same lockout recovery as delete file).

11. **ADR set `docs/adr/0001`–`0008`**  
    Short “do not silently reverse” records (not a second spec). Each file: context, decision, consequences.  
    - `0001-web-console-rbac.md` — new console/API feature MUST name an existing role or add a role; propose MUST ask which.  
    - `0002-two-chips-one-image.md` — GPIO15 host/slave; one `.bin`; slave is radio+SPI only.  
    - `0003-host-only-settings.md` — devices, users, and product settings persist on the host; slave waits for `SET_SETTINGS`.  
    - `0004-spi-always-async.md` — MQTT/web/CLI/`loop` never block on SPI.  
    - `0005-auth-boundary.md` — session on the web console only; USB CLI and MQTT stay unauthenticated unless a later ADR says otherwise.  
    - `0006-settings-export-policy.md` — no Wi-Fi in the file; users included with hash/salt only; export/restore `isAdmin` only.  
    - `0007-settings-vs-telemetry.md` — persist identity/settings (including readonly IEEE/type); do not persist RSSI, online, last packet, in-flight LED.  
    - `0008-ota-slave-before-host.md` — stage on LittleFS; flash slave over SPI first, then host; first factory flash remains USB both chips.  
    Do not ADR pin tables, SPI opcodes, or chrome tokens — those stay in specs/skills.

12. **Users in settings export (admin only)**  
    Export JSON includes a `users` array with every stored field except plaintext password (hash + salt are included; that is the accepted trade-off). Restore as `isAdmin` replaces `/users.json` from that array when present. Non-admin never reaches export/restore APIs. Wi-Fi stays omitted.

## Risks / Trade-offs

- [Default `admin`/`admin`] → Document change-on-first-use in Security UI as a later polish; first version still seeds as specified.
- [Session only in RAM] → Reboot drops sessions; operators sign in again. Avoids persisting tokens next to password hashes.
- [HTML is public] → APIs still 401; do not put secrets in `index.html`.
- [SHA-256 not slow] → Acceptable for a LAN gateway; salt stops a copied file from showing the default password in clear text.
- [Simple user can still command devices] → Intentional; MQTT can too.

## Migration Plan

Flash host **0.2.14** and filesystem (login HTML/CSS). First boot creates `admin`. Old theme byte is ignored. Rollback: previous firmware ignores `/users.json`. To re-seed: delete `/users.json` and reboot.

## Open Questions

None. Login chrome details beyond the three fields are deferred on purpose.
