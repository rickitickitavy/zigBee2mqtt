## Context

See proposal.md — Why. Host already has per-group HTTP APIs (`/api/mqtt`, `/api/zigbee`, `/api/hardware`) and a device list/store. SPI already has on/off (`0x08`), write-attr (`0x0D`), and read-attr (`0x0F`). Window covering move is a cluster-specific command, not those. Maintenance tab is empty. Device command topic is the stored `set` topic; suffix mode already suffixes every usable endpoint including `1`.

## Goals / Non-Goals

**Goals:**
- One JSON snapshot for MQTT + Zigbee + hardware + devices, never Wi-Fi.
- Same apply path for MQTT `set` and Manual command, with ZCL cluster commands first-class.
- New SPI/radio send that is cluster-specific (not write-attr).

**Non-Goals:**
- New persisted device topic field (reuse `commandTopic` / `set`).
- Changing Devices page Export/Restore.
- Copying Wi-Fi between gateways.
- Manufacturer-specific ZCL framing beyond optional raw payload bytes.

## Decisions

1. **Reuse the existing command topic**  
   The operator already has COMMAND/`set`. A fourth topic would need a full persist/sync/UI lifecycle. Channel suffix/parse already apply to `set`.  
   Alternative: add `{base}/{slug}/command` as a new stored field — rejected for this change.

2. **Parse order on the host**  
   After channel mapping: FULL CONTROL write-attr (`attr`/`val`) → ZCL command (`cl`+`cmd` or covering shortcut) → on/off text. Shortcuts are only the five covering words so `ON`/`OFF` stay on/off.

3. **SPI command `0x11` (`SpiCmdZclCommand`)**  
   Next free host-to-slave id after `0x0F`. Payload: IEEE (8) + endpoint (1) + cluster LE (2) + command id (1) + payload length (1) + payload (0–n, fit remaining SPI payload). Slave `sendZclToDevice` with `clusterSpecific=true`. Include this kind in the per-destination latest-wins queue with on/off and write-attr.

4. **Settings file shape**  
   `{ "version": "<FIRMWARE_VERSION>", "mqtt": {...}, "zigbee": {...}, "hardware": { "spiSpeedHz": n }, "devices": [ ...listStoreJson ] }`. Host `GET /api/settings/export` builds it. `POST /api/settings/restore` applies groups via the same persist paths as the individual Save buttons, then replaces the device list from `devices` (upsert each, delete IEEEs not in the file). Wi-Fi keys are ignored.

5. **Restore confirm**  
   Overlay confirm (same pattern as device delete) before POST. Compact `btn-inline` stack on Maintenance.

6. **MQTT restore restart**  
   Applying MQTT/Zigbee from restore follows those groups’ existing Save rules (Zigbee already restarts; MQTT reconnects). Hardware SPI apply without reboot as today.

## Risks / Trade-offs

- [Restore deletes devices not in the file] → Confirm copy states that devices will match the file; Devices-only Restore remains add-only.
- [Generic `cl`/`cmd` can send nonsense to a switch] → Allowed on purpose (“all devices”); operator/HA is responsible.
- [Covering shortcut on a non-covering device] → Still sends `0x0102` move; device will ignore if it lacks the cluster.

## Migration Plan

Flash host and slave together (`0.2.8`). Old MQTT `ON`/`OFF` and FULL CONTROL writes stay valid. No settings-version bump. Rollback is the previous image; leftover export files remain readable.

## Open Questions

None.
