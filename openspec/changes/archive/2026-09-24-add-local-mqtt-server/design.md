## Context

See proposal.md — Why. The host is an MQTT **client** only (`PubSubClient` in `MqttClient.cpp`). `MqttSettings.enabled` is a bool; factory `applyDefaults()` currently sets it **true**, while this change’s product default is SERVER TYPE **disable**. `MqttClient::dispatch` runs only when `enabled` and `wifiController->isStaConnected()`. Host RGB green is `StatusRgb::setMqttConnected`. Settings EEPROM version is 5; `enabled` is one byte after `clientTimeoutMs`. GET/POST `/api/mqtt` and export JSON use `"enabled"`. USB CLI uses `mqtten`. ADRs `0002` (host duties) and `0005` (MQTT has no console session) need updates.

## Goals / Non-Goals

**Goals:**
- One stored enum replaces `enabled` without growing `MqttSettings`.
- Local broker listens on `0.0.0.0:port` once AP or STA has an address; host client uses `127.0.0.1`.
- Remote mode stays STA + external `server:port`.
- Broker CONNECT auth only when both username and password are non-empty.

**Non-Goals:**
- MQTT TLS / WebSocket / MQTT 5 features beyond what the chosen lightweight broker already provides.
- Running the broker on the slave.
- Changing device topic mapping (`mqtt-device-topics`).
- Locking MQTT behind a web-console session.

## Decisions

1. **`MqttServerType` in the existing `enabled` byte**  
   `0` = disable, `1` = remote, `2` = local. Load: byte `0`/`1` keep meaning (legacy bool); `2` is local; any other value → disable. Factory default **disable**. No `GLOBAL_CURRENT_SETTINGS_VERSION` bump (same layout).  
   Alternative: add a new field in `reserved` and keep `enabled` — rejected; the UI removes ENABLED.

2. **JSON `serverType`: `"disable"` | `"remote"` | `"local"`**  
   `/api/mqtt` GET/POST and settings export. Restore: prefer `serverType`; else map `enabled` bool. POST for `local` does not require `server`. CLI: `mqtttype disable|remote|local` (keep `mqtten on/off` as aliases for remote/disable only if needed for old scripts — prefer one command; implement `mqtttype` and accept `mqtten` as remote/disable).  
   Alternative: numeric API — rejected; matches MODE-style strings.

3. **Embedded broker library on the host Arduino env**  
   Add a small ESP32-capable MQTT 3.1.1 broker to `lib_deps` (evaluate PicoMQTT broker mode first; fall back to another Arduino broker that binds all interfaces and supports optional user/password). Cap concurrent remote sessions (about 4) so HTTP + SPI stay responsive. Service the broker from the host `loop` next to `MqttClient::dispatch`.  
   Alternative: custom packet parser — rejected (scope). Alternative: require an external broker only — rejected by the request.

4. **When each piece starts**  
   - `disable`: no broker, no client, RGB not green/blue for MQTT.  
   - `remote`: no broker; client as today (`setServer(server, port)` only if STA connected).  
   - `local`: start broker when any Wi-Fi IPv4 exists (AP or STA); client `setServer("127.0.0.1", port)` without requiring STA.  
   Save MQTT still restarts the host (same as today).

5. **Auth**  
   Broker: require CONNECT user/password iff both stored fields are non-empty. Host client: keep today’s rule (username non-empty ⇒ send user and password).  
   Alternative: always require credentials in local mode — rejected; user asked for fill-or-not.

6. **Host RGB blue**  
   `StatusRgb::setMqttBrokerListening(bool)`. In `apply()`, after boot/critical and before pairing (host has no pairing blink), if broker listening write RGB `(0,0,brightness)`; else existing green for remote client.  
   Alternative: light host LED3 — rejected; LED3 is reserved off on host; user asked for blue LED (WS2812 blue).

7. **ADRs**  
   - Amend `0002`: host may run an optional MQTT broker when SERVER TYPE is local.  
   - Amend `0005`: console session still not required for MQTT; local broker MAY require CONNECT credentials from settings.  
   - Add `0009-local-mqtt-broker.md`: optional onboard broker as HA replacement; remote bind; auth rule; blue LED.

## Risks / Trade-offs

- [Broker RAM/CPU vs HTTP and SPI] → Limit client count; keep broker loop short; do not block SPI.  
- [Library API mismatch on ESP32-C6] → Spike the chosen lib during apply; switch lib if bind-all or auth is missing.  
- [Operators with today’s factory `enabled=true`] → EEPROM `1` stays **remote**; only wiped/default boards become disable.  
- [Local + empty credentials on a STA LAN] → Anyone on the LAN can publish; accepted until the operator sets user and password (document in ADR 0009).  
- [Green vs blue] → Blue wins while the broker listens so “local server up” is visible even after the host client connects.

## Migration Plan

Flash host **0.2.18**. Existing `enabled` false/true becomes disable/remote. Rollback: previous image reads byte `0`/`1` as bool; byte `2` is clamped to enabled=true by today’s `rawMqtt > 1` check (local boards rolled back would act as MQTT enabled/remote).

## Open Questions

None.
