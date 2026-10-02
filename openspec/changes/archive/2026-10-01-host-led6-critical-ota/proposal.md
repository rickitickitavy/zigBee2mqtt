## Why

Host critical errors (former RGB red / current LED5 blink) share LED5 with MQTT-connected status, so a fault and a healthy MQTT link fight for the same lamp. Host LED6 is unused; put critical and firmware-update activity there so LED5 stays boot + MQTT only.

## What Changes

- Host **LED5** keeps **boot** blink (0.25 s) and MQTT-connected solid; it SHALL NOT indicate any error (including lost slave).
- Host **LED6** is **solid on** while a critical error is present (same meaning as former RGB red for critical).
- Host **LED6** blinks at **10 Hz** (50 ms on / 50 ms off) while a firmware update cycle is active (`FirmwareOta` busy: receive / slave SPI OTA / host apply).
- Priority on host LED6: critical solid outranks update blink; both outrank idle off.
- Slave LED map stays as today (LED5 boot/critical blink, LED6 pairing).
- Docs (`README.md` LED table) match the new host map.
- Firmware build already bumped to `0.3.1` at propose.

## Capabilities

### New Capabilities

<!-- none -->

### Modified Capabilities

- `status-rgb-led`: Split host boot vs critical vs OTA onto LED5/LED6; stop using host LED5 for errors; drive host LED6 for critical (solid) and update-cycle blink (10 Hz).

## Impact

- `StatusRgb` apply/service priority and host LED5/LED6 mapping (`include/StatusRgb.h`, `src/StatusRgb.cpp`)
- Host main loop / OTA path wires update-active into status LEDs (`src/main.cpp`, `FirmwareOta` busy phases)
- Existing `setCritical` call sites (lost slave, etc.) keep meaning; only the lamp changes on host
- `README.md` status LED table; `openspec/specs/status-rgb-led`
