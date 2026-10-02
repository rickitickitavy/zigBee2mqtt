## Context

See proposal.md — Why. Today `StatusRgb::faultHeld()` treats `bootHeld || criticalHeld` the same: both blink LED5 at 0.25 s and suppress LED1–LED4 (and host LED6). Host lost-slave sets `setCritical` from `main.cpp`; slave critical paths are unchanged. `FirmwareOta::busy()` is true for Receiving / Slave / Host / Rebooting. Firmware is `0.3.1` (propose bump). Host LED6 pin is already in `kLedPins`.

## Goals / Non-Goals

**Goals:**
- Split host boot (LED5 blink) from host critical (LED6 solid) and host OTA activity (LED6 10 Hz).
- Keep host LED5 free of error meanings after this change.
- Leave slave LED semantics as they are.

**Non-Goals:**
- Changing slave pairing/critical LEDs.
- New OTA UX in the web console.
- Reintroducing the onboard WS2812.

## Decisions

1. **Host critical = LED6 solid**  
   Blink is reserved for the 10 Hz update cycle. Alternative: blink critical on LED6 — rejected; user asked solid critical and blink only for update.

2. **Update cycle = `FirmwareOta::busy()`**  
   Drive a `StatusRgb` update-held flag from the host loop (or OTA phase changes) whenever `busy()` is true. Alternative: only Slave+Host apply phases — rejected; operator should see activity during HTTP receive too.

3. **Decouple boot vs critical in `apply()`**  
   Stop using a single `faultHeld()` path for host LED5. On host: `bootHeld` → LED5 0.25 s blink; `criticalHeld` → LED6 solid; `updateHeld && !criticalHeld` → LED6 10 Hz. On slave: keep existing LED5 blink for boot or critical. Alternative: two completely separate classes — rejected; one `apply()` already owns priority.

4. **MQTT / broker visible during host critical**  
   After boot clears, host LED5 MQTT and LED4 broker stay available while LED6 shows critical. Alternative: blank everything on critical — rejected; user said LED5 must not show errors, not that MQTT must hide.

5. **Pulses still suppressed on critical (and boot)**  
   Keep `allowsActivityPulse()` false while critical or boot so traffic flicker does not compete with LED6 critical. OTA blink alone does not need to suppress pulses unless it hurts readability — suppress pulses while updateHeld as well for a clean 10 Hz on LED6.

## Risks / Trade-offs

- **[Risk] Operators used to LED5 blink for lost slave** → Mitigation: update README LED table; LED6 solid is unambiguous once documented.
- **[Trade-off] 10 Hz needs a 50 ms half-period timer** → Separate from the existing 125 ms fault blink timer; host-only.
- **[Risk] Critical during OTA hides update blink** → Accepted; critical outranks by requirement.

## Migration Plan

1. Flash host (and slave if convenient; slave behavior unchanged).
2. Confirm boot: LED5 blink → then MQTT on LED5; force lost slave → LED6 solid, LED5 stays MQTT if connected.
3. Run System → Update: LED6 10 Hz for the busy window.
4. Rollback: prior firmware restores LED5 combined boot/critical blink.
