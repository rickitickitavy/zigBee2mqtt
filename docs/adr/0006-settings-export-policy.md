# 0006. Settings export policy

## Context

Operators need a backup file. Wi-Fi secrets and plaintext user passwords must not travel in that file. Users themselves are part of the backup.

## Decision

Settings export and restore are `isAdmin` only.

The file includes MQTT, Zigbee, hardware, `ui.theme` (signed-in user), devices, and the users table (user name, added-at, roles, blocked, theme, password hash and salt).

The file MUST NOT include a Wi-Fi group or a plaintext user password.

Restore applies those groups. A `users` array replaces the host users table. `ui.theme` applies to the signed-in user only. A `wifi` object in the file is ignored.

## Consequences

A stolen export can still be used for offline hash attacks. That trade-off is accepted. Non-admin never reaches these APIs.
