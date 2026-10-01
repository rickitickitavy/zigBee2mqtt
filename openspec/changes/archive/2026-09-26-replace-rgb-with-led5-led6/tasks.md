## 0. Version

- [x] 0.1 Confirm `FIRMWARE_VERSION` is `0.2.22` in `Defines.h` (bumped at propose)

## 1. Pins and driver

- [x] 1.1 Add `PIN_LED5` GPIO2 and `PIN_LED6` GPIO3 in `pins.h` and include them in `StatusRgb` pin table (`kLedCount` 6), and verify both roles configure those GPIOs as outputs (HIGH = on) at `begin()`
- [x] 1.2 Read `PIN_BOARD_ROLE` inside `StatusRgb::begin()` so host vs slave mapping is known before Serial, and verify the first `apply()` already uses the correct boot LED
- [x] 1.3 Remove status `rgbLedWrite` (optional one-time RGB off at `begin()`), and verify `PIN_STATUS_RGB` is not driven afterward

## 2. Host and slave mappings

- [x] 2.1 Host boot/critical hold LED6 on and LED5 off, MQTT connected hold LED5 on after boot, and verify LED1–LED4 pulses and LED4 broker-listening are unchanged
- [x] 2.2 Slave ready hold LED5 solid; slave boot/critical blink LED5 at 0.5 s period (250 ms on / 250 ms off) in `service()`, and verify solid ready is off while blinking
- [x] 2.3 Slave pairing drive LED6 via existing `writePairingPhase` (not RGB), and verify pairing does not run while boot/critical is held

## 3. Docs

- [x] 3.1 Update `README.md` and `docs/adr/0009-local-mqtt-broker.md` LED notes to LED5/LED6 (no RGB status), and verify they match the delta spec

## 4. Build

- [x] 4.1 `pio run` succeeds for host and slave with `0.2.22`
