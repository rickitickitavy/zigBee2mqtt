## Why

The gateway already uses four external red LEDs for traffic, but boot, MQTT-ready, slave-ready, pairing, and critical-error states still depend on the onboard WS2812 RGB. Discrete LED5 and LED6 on GPIO2 and GPIO3 replace those RGB colors so status is visible without the RGB LED.

## What Changes

- Add **LED5** on GPIO2 and **LED6** on GPIO3 on **both** host and slave (HIGH = on, same as LED1–LED4).
- **Stop using the onboard RGB** (GPIO8 WS2812). Firmware SHALL leave it unused and SHALL NOT call `rgbLedWrite` for status.
- **Host mappings**
  - Former RGB **red** (boot / critical) → **LED5** blinks with period **0.25 s** (125 ms on / 125 ms off).
  - Former RGB **green** (MQTT connected) → **LED5** held on (off while boot/critical blink runs).
  - Host LED6 stays off.
- **Slave mappings**
  - Former RGB **green** (ready, no critical) → **LED5** held on.
  - Former RGB **blue** (pairing blink) → **LED6** blink (existing pairing toggle).
  - Former RGB **red** (boot / critical) → **LED5** blinks with period **0.25 s** (125 ms on / 125 ms off). Ready-green on LED5 SHALL be off while this blink runs.
- LED1–LED4 roles stay the same (host MQTT pulses and local-broker LED4; slave Zigbee pulses). Host LED3 stays unused except it remains off.

## Capabilities

### New Capabilities

<!-- none -->

### Modified Capabilities

- `status-rgb-led`: Drive LED5/LED6 instead of RGB for host/slave status colors; RGB unused.

## Impact

- `include/pins.h`, `include/StatusRgb.h`, `src/StatusRgb.cpp` (pin table, apply/service, drop `rgbLedWrite`).
- Callers stay on `STATUS_RGB` (`main.cpp`, `MqttClient.cpp`, `MqttBroker.cpp`, `ZigbeeCoordinator.cpp`, `InterChipSlave.cpp`); pairing still toggles via `writePairingPhase`.
- `openspec/specs/status-rgb-led/spec.md`, `docs/adr/0009-local-mqtt-broker.md`, `README.md` LED pin notes.
- Firmware `FIRMWARE_VERSION` is `0.2.22` (build bump at propose). Flash host and slave together so both boards match LED5/LED6 wiring.
