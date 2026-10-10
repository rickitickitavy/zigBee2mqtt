# Design

## Context

See proposal.md — Why (USB slave flash while host stays up). Today every host slave reset goes through `InterChipHost::enterReset` / `pulseResetStart` or `resetSlaveSynchronous` (boot, link loss, ready timeout, Zigbee start timeout, keepalive silence, OTA version verify). System → Update is an admin `upload-card` with version + joined ZIP upload only (`data/index.html` `#system-tab-update`).

## Goals / Non-Goals

**Goals:**

- One admin toggle on the Update card: start 120 s pause for a USB slave programming window, show live remaining, cancel on second press, refresh remaining when the tab is shown.
- Single host gate so **all** EN/RST pulses honor the pause (async + sync).
- On pause start: if a reset pulse is in progress, finish releasing EN high; never hold EN low for the pause duration.
- Server is source of truth for remaining time (survives tab leave/re-enter; UI polls or reads on tab show).

**Non-Goals:**

- Changing SPI frame protocol, OTA chunk algorithm, or slave firmware.
- Implementing the USB flash tool itself (operator uses esptool / IDE separately).
- Blocking host reboot or joined ZIP OTA upload itself.
- Persisting the pause across host reboot.
- Pausing resets that already completed before the button was pressed.

## Decisions

1. **Gate at EN/RST helpers, not each caller**  
   Check pause in `enterReset` / `pulseResetStart` / `resetSlaveSynchronous` (one shared `isSlaveResetPaused()`).  
   *Alternative:* flag each call site — easy to miss OTA verify.

2. **Deadline in millis on the host**  
   Store `slaveResetPauseUntilMs`; active while `millis()` is before that; cancel clears it; start sets `millis() + 120000`. Remaining = until − now.  
   *Alternative:* FreeRTOS timer object — heavier for no gain.

3. **Small admin HTTP API**  
   - `GET /api/slave-reset-pause` → `{ "active": bool, "remainingMs": number }`  
   - `POST /api/slave-reset-pause` with `{ "active": true|false }` → start (true) or cancel (false); response includes remaining.  
   Only `isAdmin`.  
   *Alternative:* fold into `/api/update/status` — couples pause to OTA polling.

4. **UI on Update card**  
   Button label inactive: `Wait for update slave`. Short hint that this is for USB slave programming. Active: countdown text e.g. `Wait for update slave (1:45)` or remaining seconds, updated ~1 Hz while the Update tab is visible; on tab show, GET remaining. Second click POSTs cancel.  
   *Alternative:* separate status line — user asked for timer on the button.

5. **OTA during pause**  
   Version-verify resets are suppressed like other resets. OTA may wait/fail under existing verify timeouts; that is acceptable for a debug control. Document in UI hint if space allows.

6. **Fragile zones**  
   Apply will touch `InterChipHost` (influence of `slave-spi-ota` / `firmware-update-process`). Require human allow at apply time; do not change `SpiProtocol` or OTA chunk packing.

## Risks / Trade-offs

- **[Risk]** Pause hides a dead slave (no auto-reset) → Mitigation: 120 s hard cap; cancel; log skipped resets.  
- **[Risk]** OTA verify stuck while paused → Mitigation: operator must cancel pause or wait; same as “any way” request.  
- **[Risk]** millis wrap → Mitigation: use signed `(int32_t)(until - now)` style compare already used elsewhere.  
- **[Trade-off]** Sync boot reset in `begin()` stays outside pause (console not up yet).

## Migration Plan

Ship in firmware image with embedded console. No EEPROM migration. Rollback = previous firmware build.
