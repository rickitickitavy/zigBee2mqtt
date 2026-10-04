# Design

## Context

See proposal.md — Why. Today every `DeviceTopicEntry` is Zigbee-shaped: host publishes state via `MqttClient::publishDeviceState`, subscribes only to command topics, syncs slots to the slave, and boot refresh is `ZigbeeCoordinator::enqueueRegisteredStatusReads()` (host path mirrors via SPI proxy). Cluster type lives in `zigbeeType` and is readonly in the parameter dialog. MQTT settings are `MqttSettings` (server, port, credentials, `baseTopic`, …) with no birth/announce fields. Firmware after propose bump is `0.3.7`.

## Goals / Non-Goals

**Goals:**
- One registry for Zigbee and Wi‑Fi devices with a persisted transport flag.
- Opposite MQTT direction for Wi‑Fi state (subscribe) vs Zigbee (publish).
- Server-born handshake: listen → Zigbee status collect; host announce after 2 min and every N minutes.
- Console flows for WIFI DEVICE create and transport/type field rules.

**Non-Goals:**
- Auto-discovery of arbitrary Wi‑Fi devices (no mDNS/HA discovery parser).
- Changing Home Assistant’s own birth topic protocol beyond “any message on configured topic”.
- Storing Wi‑Fi runtime status in EEPROM/`devices.json`.
- Local-broker subscription redesign beyond what remote subscribe already does (follow existing local vs remote patterns).

## Decisions

1. **JSON/API field `transport` with values `zigbee` | `wifi`**
   Stored on `DeviceTopicEntry` (new `uint8_t` enum). UI label “DEVICE TYPE” or “TRANSPORT” showing Zigbee/Wi‑Fi, placed immediately before NAME. Cluster classification stays the existing `type` / `zigbeeType` field labeled TYPE. Alternative: overload `type` with `wifi` — rejected; it already means onOff/iasZone/….

2. **Settings version bump for MQTT born fields + device transport**
   Add `char serverBornTopic[64]` and `uint8_t bornIntervalMin` to `MqttSettings` (default topic empty, interval 20). Bump `GLOBAL_CURRENT_SETTINGS_VERSION` and migrate: missing transport → `zigbee`; missing born fields → defaults. Keep dirty-byte EEPROM writes. Alternative: LittleFS-only born config — rejected; other MQTT fields already live in the main settings group.

3. **Wi‑Fi identity reuses IEEE slot key**
   WIFI DEVICE create makes IEEE editable; validate unique parseable 8-byte IEEE text (same parser as Zigbee). After first save, IEEE readonly like Zigbee. Alternative: separate string id — rejected; list/delete/command APIs are IEEE-keyed today.

4. **Subscribe table shares command-subscription machinery**
   Extend `MqttClient` so remote mode tracks state-topic subscriptions for `wifi` entries (base + `/+` when suffix channels), rebuild on map changes like `subscribeDeviceCommands`. Inbound state messages resolve via `findByStateTopic` (new) and update host telemetry cache used by `/api/devices` (same shape as Zigbee status entries). Alternative: second PubSubClient — rejected as heavy.

5. **Commands for `wifi` publish MQTT only**
   In `applyManualDeviceCommand` / MQTT set dispatch, if `transport == wifi`, publish mapped payload to the command topic (retain false) and return; skip SPI/Zigbee. Type-action menu can keep using the same path. If MQTT disconnected, fail the action visibly.

6. **Slave sync filters `wifi`**
   `onDeviceUpserted` / registry dump / delete-to-slave skip `wifi`. Host delete still clears the slot and unsubscribes MQTT. Online/RSSI for Wi‑Fi: show offline/N/A unless availability topic traffic is added later (out of scope unless availability already updates something — default: no Zigbee online bit).

7. **Born listen and announce**
   When `serverBornTopic` non-empty and MQTT connected: subscribe exact topic; any message calls the existing Zigbee status-collect entry point filtered to `transport == zigbee`. Host announce: after `millis()` ≥ 2 minutes from boot (and MQTT up), publish payload `online` once; then every `bornIntervalMin` minutes. Empty topic disables both. Alternative: retain birth messages — not required; non-retained is enough for live HA.

8. **Cluster TYPE editability**
   UI: `zigbee` → readonly text (today). `wifi` → `<select>` of the four labels; POST includes `type`; host accepts type changes only when transport is `wifi`.

## Risks / Trade-offs

- [Subscription budget] → Wi‑Fi state subs use the same finite table as commands; mitigate by counting wifi+command topics against `kMaxCommandSubscriptions` (raise if needed) and logging overflow.
- [Born storms] → HA and gateway both publishing on the same topic could loop collect; mitigate by not treating host’s own announce as requiring special ignore if collect is idempotent/cheap, or ignore messages with payload exactly matching last host announce within a short window if storms appear.
- [IEEE collisions] → Operator-entered Wi‑Fi IEEE could collide with a Zigbee device; reject save on duplicate IEEE.
- [Local MQTT broker] → Today command subscribe is skipped for local broker; state listen and born must follow the same local delivery path used for other local MQTT traffic (extend consistently, do not invent a second model).

## Migration Plan

1. Ship firmware `0.3.7+` with settings version migrate: all existing devices → `transport=zigbee`; MQTT born defaults.
2. Operators add Wi‑Fi devices via WIFI DEVICE; configure SERVER BORN TOPIC to the broker/HA status topic they use.
3. Rollback: older firmware ignores unknown JSON fields; new EEPROM fields sit in migrated main block — downgrade may need settings reset if version check rejects newer images (same as prior settings bumps).

## Open Questions

- None that block implementation; availability-topic-driven online for Wi‑Fi can be a follow-up if operators need it.
