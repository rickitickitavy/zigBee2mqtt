# Design

## Context

See proposal.md — Why. Today `DeviceTransportWifi = 1` serializes as JSON id `"wifi"` (`deviceTransportJsonId` / `deviceTransportFromJsonId` in `GlobalSettings.h`). Console UI, APIs, and MQTT/Zigbee branching key off that id. Gateway radio Wi‑Fi settings are unrelated. Operator will re-import device lists; firmware must not rewrite stored `"wifi"` to `"mqtt"`.

## Goals / Non-Goals

**Goals:**

- Single external transport id `mqtt` for MQTT-bound devices; labels say MQTT.
- Enum rename for clarity; keep numeric value `1`.
- Explicit non-mapping of legacy `"wifi"` strings.

**Non-Goals:**

- Migrating LittleFS `/devices.json`, settings export files, or EEPROM blobs.
- Renaming gateway Wi‑Fi settings, `/api/wifi`, or sidebar **WiFi**.
- Renaming the OpenSpec capability directory `wifi-mqtt-devices`.
- Changing MQTT topic patterns or Zigbee SPI device sync payload layout beyond the transport string/enum naming in host code.

## Decisions

1. **JSON id `mqtt` only (no `wifi` alias)**  
   `deviceTransportFromJsonId` accepts `"mqtt"` → MQTT enum; anything else (including `"wifi"`) → Zigbee.  
   **Why:** Matches “no migrate”; avoids silent half-upgrades.  
   **Alternative:** Accept both ids temporarily — rejected per operator request.

2. **Keep enum numeric value `1`**  
   `DeviceTransportMqtt = 1` replaces `DeviceTransportWifi = 1`.  
   **Why:** SPI/device sync that carries the byte stays compatible if a peer still has value 1 meaning “non-zigbee MQTT device”.  
   **Alternative:** New value `2` — unnecessary churn.

3. **UI strings**  
   Button **MQTT DEVICE**; transport field **MQTT**; error strings that say “WiFi device” for this path become “MQTT device”. Do not touch Wi‑Fi settings copy.

4. **Capability path `wifi-mqtt-devices` stays**  
   Requirements text updates; directory name left for history/links.

## Risks / Trade-offs

- **[Risk]** After flash, existing `"wifi"` devices behave like Zigbee (wrong path) until restore → **Mitigation:** document in proposal; operator restores list with `"mqtt"`.
- **[Risk]** Missed string `"wifi"` in JS/API leaves a dead create path → **Mitigation:** grep `transport == "wifi"` / `DeviceTransportWifi` / WIFI DEVICE in apply tasks.
- **[Trade-off]** Spec requirement *titles* that still say “WiFi …” stay for OpenSpec MODIFIED header match; body text uses `mqtt`.

## Migration Plan

1. Flash host (and slave if joined package) with `1.0.7`.
2. Operator exports/edits device list: replace `"transport":"wifi"` with `"mqtt"` (or recreate MQTT devices).
3. Restore devices / settings as today.
4. Rollback: previous firmware still understands `"wifi"`; lists already saved as `"mqtt"` would need reverse edit.

## Open Questions

None.
