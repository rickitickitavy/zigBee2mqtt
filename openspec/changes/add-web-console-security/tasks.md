## 0. Version and ADR

- [x] 0.1 Confirm `FIRMWARE_VERSION` is `0.2.14` in `Defines.h` (bumped at propose)
- [x] 0.2 Add `docs/adr/0001`–`0008` (`web-console-rbac`, `two-chips-one-image`, `host-only-settings`, `spi-always-async`, `auth-boundary`, `settings-export-policy`, `settings-vs-telemetry`, `ota-slave-before-host`) with context/decision/consequences as in design.md decision 11, and verify each file exists and 0001 lists the roles plus “ask on every new feature”

## 1. User store

- [x] 1.1 Add host `UserStore` persisted at `/users.json` with user name, salt, hash, added-at, role flags, `isBlocked`, theme (cap 16, unique names), and verify create then reboot still lists the same user without a plaintext password in the file
- [x] 1.2 Call load-or-seed on every host boot after LittleFS mount so an empty or missing file creates `admin` / `admin` / `isAdmin` / dark / not blocked, and verify a second boot does not add another `admin`
- [x] 1.3 Reject `editUsers` writes that set `isAdmin`, mutate an `isAdmin` user, or delete the last `isAdmin`, and verify those writes fail while `isAdmin` can create a second admin

## 2. Sessions and API roles

- [x] 2.1 Add `POST /api/auth/login`, `POST /api/auth/logout`, and `GET /api/auth/me` with an HttpOnly session cookie (12h; Remember me 30d for non-admin; admin ignores Remember me and expires after 10 minutes idle, reset on each authenticated request), and verify wrong password, blocked user, and HTTP Basic are rejected, an idle admin is 401 after 10 minutes, and MQTT still commands a device with no cookie
- [x] 2.2 Require a session on all other `/api/*` routes (401 without cookie) and enforce roles (403): simple user status/list/log/command/own theme only; `addDevices`/`editDevices`/`removeDevices`/`editUsers` as designed; Wi-Fi/MQTT/Zigbee/hardware/OTA/export/restore `isAdmin` only, and verify a simple-user POST `/api/devices` does not persist
- [x] 2.3 Point `GET`/`POST /api/theme` at the session user’s theme (Appearance UI stays on Maintenance), leave `reserved[4]` unused, include users (hash and salt, no plaintext password) in admin settings export, replace `/users.json` on admin restore when `users` is present, and verify a non-admin cannot export/restore and the file has hashes but no user plaintext password

## 3. Web UI

- [x] 3.1 Add a dark login overlay (user name, password, Remember me, not Basic) that hides the sidebar until `GET /api/auth/me` succeeds, and verify first visit shows only that form and a valid `admin` login reveals the console
- [x] 3.2 Hide sidebar and System tabs by role (simple: Status, Devices, System Log + Maintenance Appearance only; `editUsers`: Security tab; `isAdmin`: all), hide Maintenance export/restore for non-admin, and verify a simple user never sees WiFi/MQTT/ZigBee/Update/Hardware/Security
- [x] 3.3 Gate Devices Add / parameter edit / delete by role, keep row actions and Manual command for simple users, confirm before delete, and verify `addDevices` without `editDevices` can add but cannot open edit
- [x] 3.4 Add System → Security user table and add/edit dialogs (no hash display; confirm delete; `editUsers` cannot assign or edit `isAdmin`), apply user theme after login and on Appearance Save, and verify two users keep independent themes after save and re-login

## 4. Build

- [x] 4.1 `pio run` succeeds for host and slave with `0.2.14`
