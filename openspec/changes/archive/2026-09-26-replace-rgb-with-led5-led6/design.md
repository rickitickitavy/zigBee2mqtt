## Context

See proposal.md — Why. `StatusRgb` already owns LED1–LED4 (`PIN_LED1`–`PIN_LED4`, GPIO18–21) and the WS2812 on `PIN_STATUS_RGB` (GPIO8) via `rgbLedWrite`. `apply()` holds RGB red when `bootHeld` or `criticalHeld`, blue when `pairingHeld`, else green when `mqttConnected` or `readyGreen`. Host LED4 is forced on for `mqttBrokerListening`. `STATUS_RGB.begin()` runs in `setup()` **before** `readBoardRole()`, so role-specific LED5/LED6 mapping must read the role strap itself (GPIO15, same as `readBoardRole`) or it will guess wrong for the first frames. GPIO2 and GPIO3 are unused today (SPI is 4–7/10; USB stays 12/13). Firmware is `0.2.22`.

## Goals / Non-Goals

**Goals:**

- Drive LED5 (GPIO2) and LED6 (GPIO3) on both chips with HIGH = on.
- Map former RGB colors as specified; stop calling `rgbLedWrite`.
- Keep existing `STATUS_RGB` setters and pulse APIs so MQTT/Zigbee/SPI call sites stay put.

**Non-Goals:**

- Renaming `StatusRgb` / `STATUS_RGB` or changing SPI/MQTT/HTTP.
- Changing LED1–LED4 pulse meanings or host LED4 broker-listening.
- Changing pairing toggle timing (already 250 ms, 0.5 s period).

## Decisions

1. **Pins in `pins.h`**  
   `PIN_LED5 = 2`, `PIN_LED6 = 3`. Keep `PIN_STATUS_RGB` defined but unused, or drop it when no callers remain. Alternative: reuse GPIO8 as a discrete LED — rejected; user asked not to use RGB.

2. **One driver, six GPIOs**  
   Raise `kLedCount` from 4 to 6 and extend `kLedPins`. Pulses stay on indices 0–3. LED5/LED6 are level-held or timed in `apply()`/`service()`, not 0.1 s pulses. Alternative: a second class — rejected; priority logic already lives in `StatusRgb::apply()`.

3. **Role at `begin()`**  
   `begin()` configures LED1–LED6, then reads `PIN_BOARD_ROLE` (INPUT) to set host vs slave. Host boot/critical → LED6 solid. Slave boot/critical → LED5 0.5 s blink in `service()`. Alternative: `setHostRole` after `readBoardRole` — rejected because boot indication must be correct from the first `apply()`.

4. **Slave LED5 dual use**  
   Ready (`readyGreen`) is solid LED5. Boot/critical outranks and replaces that with blink (period 500 ms). Same pin, mutually exclusive, matching former red-over-green.

5. **RGB off**  
   Remove `writeRgb` / last-RGB cache. Optionally write RGB (0,0,0) once at `begin()` then never again so a leftover pixel is dark. Prefer a single off write at `begin()` then delete the path.

## Risks / Trade-offs

- [Host and slave LED5/LED6 wired differently] → Same GPIOs and polarity on both boards; flash both images.
- [GPIO2/3 electrical] → Treat as ordinary outputs like LED1–LED4; HIGH = on.
- [Slave LED5 blink vs ready] → Operators see blink vs solid; do not overlap.

## Migration Plan

Flash host then slave (or both). No EEPROM or protocol change. Rollback is the previous firmware plus unplugging LED5/LED6.

## Open Questions

None.
