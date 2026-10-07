# Design

## Context

See proposal.md — Why. Today `StatusRgb` configures six GPIOs as digital outputs and writes HIGH/LOW in `writeLevels`. Hardware settings in host `GlobalSettings.reserved` hold only `spiSpeedHz` (and UI theme at offset 4). The Hardware tab and `/api/hardware` expose SPI speed only. `SET_SETTINGS` carries channel, permit-join, and unix time and starts Zigbee on the slave; it must not be reused for brightness-only updates. Devices row actions and Manual command are available to simple users; login Enter submits even with an empty password; save plates stay visible until the next action.

Observed color wiring (compile-time maps, not runtime RGB):

| LED | Host (master) | Slave |
|-----|---------------|-------|
| 1 | blue | red |
| 2 | green | blue |
| 3 | green | green |
| 4 | green | blue |
| 5 | green | green |
| 6 | red | blue |

## Goals / Non-Goals

**Goals:**

- Encode the color map per `BOARD_ROLE_*`.
- PWM-drive blue/green when on; keep red digital.
- Persist and UI-edit blue/green brightness 1–100 on the host; apply on both boards.
- Login Enter with empty password focuses password only.
- Gate Devices-table command UI + APIs with `controlDevices`.
- Auto-hide error/success plates after 10 s.

**Non-Goals:**

- Runtime recoloring or per-LED brightness (only per color).
- PWM on red LEDs.
- Changing status meanings (which LED index means boot, MQTT, pairing, etc.).
- Slave-local persistence of brightness.
- Gating control-console widget commands with `controlDevices` (Devices table / Manual command only).

## Decisions

1. **Compile-time color table in `StatusRgb` (or `pins.h` companion)**  
   Each role gets `LedColor { Red, Green, Blue }` for LED1–6. Rationale: wiring is fixed hardware; no UI to reassign. Alternative: JSON config — rejected as out of scope.

2. **ESP32 LEDC for blue/green; `digitalWrite` for red**  
   Attach LEDC channels only to green/blue pins at `begin()`. On logical on: duty = `brightnessPercent * maxDuty / 100` (clamp 1–100). On off: duty 0. Red unchanged. Alternative: bit-bang soft PWM — rejected (LEDC is available on S3 and C6).

3. **Store brightness in `reserved` after existing fields**  
   Offsets: keep `spiSpeedHz` at 0–3, theme at 4; add `blueLedBrightness` and `greenLedBrightness` as `uint8_t` at reserved offsets 5 and 6 (named constants in `Defines.h`). Default 100. Clamp 1–100 on get/set. Alternative: bump `GlobalSettings` version / new struct field — heavier than needed for two bytes.

4. **Dedicated SPI command for brightness (not `SET_SETTINGS`)**  
   New host→slave command (e.g. `SpiCmdSetLedBrightness`) with two payload bytes. Push after settings OK / in normal work, and again on Hardware save/restore. Slave applies to `STATUS_RGB` only. Rationale: `SET_SETTINGS` starts Zigbee; brightness must not restart the radio. Alternative: extend `SET_SETTINGS` and branch “brightness-only” — more fragile.

5. **Hardware apply path**  
   Extend hardware save/restore handlers to update `SettingsManager`, call `STATUS_RGB.setColorBrightness(blue, green)`, set SPI clock as today, and enqueue brightness to the slave when linked. JSON keys: `blueLedBrightness`, `greenLedBrightness`.

6. **Default before first push**  
   Slave boots at 100/100 until the host delivers values (matches factory default and prior full-on feel).

7. **Role id `controlDevices`, label “Other device control”**  
   Follow existing camelCase role ids (`editDevices`, …). Persist on `ConsoleUser`, Security checkbox, session JSON, and SPI user flags (new bit alongside `monitor`). `isAdmin` implies the right. UI hides `…` and Manual command without it; command APIs used by those controls reject without it. Control-console view commands stay as today.

8. **Login Enter**  
   Intercept login form submit / Enter: if password input is empty, `preventDefault`, `focus()` password, return. Do not special-case empty user name beyond existing validation.

9. **Plate auto-hide**  
   Central helper when showing `.save-error` / `.save-success`: clear any prior timer for that element, show it, schedule hide at 10_000 ms. Apply wherever plates are revealed (including login error).

## Risks / Trade-offs

- [LEDC channel conflicts with other peripherals] → Pick unused LEDC channels; document in code comments; keep frequency moderate (e.g. ~5 kHz) to avoid audible buzz if any path is audible.
- [Active-high LEDs with PWM] → Confirm duty maps to “on” the same polarity as today’s HIGH; if any board is active-low, invert duty (current code uses HIGH = on).
- [Joined OTA must update both images] → Same as other dual-board LED changes; call out in tasks.
- [Existing simple users lose Devices actions] → Expected; admins assign `controlDevices`. Document in Security UI.
- [SPI user flag bit space] → Use next free `SPI_USER_FLAG_*`; verify pack/unpack round-trip.

## Migration Plan

- Existing EEPROM without brightness bytes: treat 0 as unset → default 100.
- Old export JSON without brightness: restore keeps current host brightness (spec’d).
- Users without `controlDevices` in stored JSON: flag false; Devices actions hidden until assigned (or `isAdmin`).
- Rollback: revert firmware; reserved bytes 5–6 and new role flag become unused again.

## Open Questions

None — color map, brightness range, role label/id, Manual command gated with row actions, and 10 s plate hide are fixed.
