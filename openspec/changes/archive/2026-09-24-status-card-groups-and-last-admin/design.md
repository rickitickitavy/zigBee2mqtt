## Context

See proposal.md — Why and specs for Status groups plus the last-unlocked-admin rule.

Today Status in `data/index.html` is a column of `.field` cards (label above value). `GET /api/status` from `hostGatewayStatusJson` already has `devices`, `online`, `packetsRx`, `packetsTx`, `version`, `masterVersion`, `slaveVersion`, `pairingActive`, `onlineSec`. `formatOnlineTime` is `h m s` only. System → Log repeats `<h3>Log</h3>` under a Log tab.

`UserStore::adminCount` counts every `isAdmin`, including blocked. `updateUser` / `deleteUser` only block demote/delete when `adminCount() <= 1`. `replaceFromExportJson` requires `adminCount() >= 1`, so an all-blocked-admin file still restores. There is no `docs/adr/0010`.

PicoMQTT `AuthServer` already has `clients.size()` for the listen cap. `WiFiController` knows AP vs STA; `WiFi.RSSI()` is logged on STA ready. This project’s Wi-Fi **BSSID** field is the network name (same as the WiFi card).

## Goals / Non-Goals

**Goals:**
- One status JSON + compact grouped Status chrome.
- Host rejects any users-table result with zero unlocked `isAdmin`.
- ADR `0010` records that invariant.

**Non-Goals:**
- Changing MQTT SERVER TYPE, broker listen rules, or Wi-Fi join behavior.
- A new Status poll rate or pairing-found UI.
- Moving Security off its current sidebar/section.
- Persisting Wi-Fi RSSI or MQTT connection counts (telemetry only).

## Decisions

1. **Extend `/api/status`, do not add new URLs**  
   Add JSON fields the Status groups need. Simple users already may call `/api/status`.  
   Alternative: extra `/api/wifi-status` — rejected; Status already polls one document.

2. **Status JSON shape (additive)**  
   Keep existing keys. Add:
   - MQTT: `mqttLocalStatus` (`unused` | `down` | `listening`), `mqttTopicCount` (number), `mqttConnectionCount` (number)
   - Wi-Fi: `wifiMode` (`AP` | `STA` | `AP+STA`), `wifiBssid` (stored network name), `wifiRssiDbm` (number when STA-associated, else omit or `null`)  
   UI maps `null`/missing RSSI to `—`.  
   Alternative: nest `mqtt` / `wifi` objects — fine if apply prefers that, as long as Status can bind the same values.

3. **MQTT numbers**  
   - `unused` when SERVER TYPE is not `local`; `listening` when the onboard broker is up; `down` when SERVER TYPE is `local` but the broker is not listening.  
   - `mqttConnectionCount`: if listening, PicoMQTT `clients.size()` (host loopback client plus remotes). If not listening, `1` when the host MQTT client is connected to a remote broker, else `0`.  
   - `mqttTopicCount`: unique registered-device command + state topics from the host topic map (active gateway topics). Do not scrape PicoMQTT’s `#` wildcard as “one topic.”  
   Alternative: broker retained-message count — rejected; PicoMQTT does not expose that cleanly.

4. **Online time**  
   Keep `onlineSec` from `millis()/1000`. JS: `Dd Hh Mm Ss` on one row (days = floor(sec/86400)). Always show all four units.

5. **Status chrome**  
   Replace stacked `.field` cards with `.status-group` / `.status-row` (flex, space-between, values `text-align: right`). `max-width` about `24rem` (not `100%`). Online time is a single row above the groups (same row style, no group heading or a one-line label). Use existing `--accent` / `.group-title`. Do not introduce a second CSS framework.

6. **System → Log**  
   Remove the inner `upload-title` “Log”. Keep hint, textarea, Refresh, Download.

7. **Last unlocked admin on the host**  
   Replace `adminCount` with `unlockedAdminCount` (`isAdmin && !isBlocked`). Before commit of `updateUser`, `deleteUser`, and `replaceFromExportJson`, evaluate the would-be table; if `unlockedAdminCount < 1`, return forbidden / false and do not persist.  
   Existing last-admin demote/delete stays, but now a blocked admin does not satisfy the floor.  
   SPI `upsertFromRecord` / `replaceFrom` on the slave replica is not a console login surface; do not add a second policy there this change.

8. **ADR `0010`**  
   Short file `docs/adr/0010-last-unlocked-admin.md`: at least one unlocked `isAdmin` after every successful users write (including block and restore). Point `0001` consequences at `0010` in one sentence.  
   Alternative: only a spec delta — rejected; the user asked for an ADR.

9. **Security UI**  
   Map `UserWriteForbidden` from last-admin to a visible save error. Do not rely on UI-only disable (host still rejects).

## Risks / Trade-offs

- [PicoMQTT `clients` is protected] → Add a small `MqttBroker::connectionCount()` on `AuthServer` rather than reaching into the library.  
- [AP-only RSSI] → Show `—`; do not invent a client RSSI.  
- [Topic count ≠ broker subscription table] → Operators see device topics, not `#`. Document in the MQTT group label “Active topics”.  
- [Corrupt restore] → Keep reject; do not auto-seed during restore (boot seed still applies only when the table is empty).

## Migration Plan

Flash host (and slave if the same image) `0.2.21`. LittleFS `index.html` / `all.css` must match (filesystem upload). No EEPROM settings version bump. Rollback: previous image; users file format unchanged.

## Open Questions

None. MQTT “active topics” is defined as unique device command + state topics.
