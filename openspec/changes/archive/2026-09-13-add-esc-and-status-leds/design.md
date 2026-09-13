## Context

See proposal.md for motivation and `specs/` for the behavior contract.

Both roles already drive `PIN_STATUS_RGB` with `rgbLedWrite`. Host turns it off at the end of `setupHost`. Slave pairing blinks blue in `ZigbeeCoordinator::updatePairingLed`. Host SPI polls every 50 ms (`ReadEvent`, plus ping / time-sync), so flashing on every decoded frame would look like a solid LED.

Overlay dialogs are `.dialog-overlay` nodes in `data/index.html` with existing close helpers (`closeSearchDialog`, `closeParameterDialog`, `closeRestoreSkippedDialog`, `closeDeleteConfirmDialog`). They share `z-index: 20`; when two are visible, later DOM order is the front-most.

## Goals / Non-Goals

**Goals:**

- One document Escape handler that only calls those dismiss helpers.
- One small status-LED owner both roles can call, with a fixed priority so pairing, boot, activity, and faults do not fight.
- Latch “preparation finished” per boot so a later STA drop does not turn host boot-red back on.

**Non-Goals:**

- MQTT-connected as a host ready condition.
- Separate RX/TX GPIOs or a second LED.
- Changing SPI framing, poll period, or pairing duration.
- Flashing on host (host is boot-red only).

## Decisions

### 1. Escape: one keydown, reuse close helpers

Listen on `document` for `Escape` / `Esc`. Collect visible `.dialog-overlay:not([hidden])` and dismiss the last one in document order (delete confirm and restore-skipped sit after search/parameters). Call the matching close function so search still POSTs stop. Ignore when the list is empty. Do not `preventDefault` unless a dialog closed.

Alternative: per-dialog listeners — rejected; easy to miss a new overlay.

### 2. Shared `StatusRgb` with priority layers

Add a tiny helper (new `.h`/`.cpp`) both `setupHost` / `setupSlave` and `loop` use:

- Critical (slave) > Boot > Pairing blink > Activity pulse (100 ms) > Off
- Colors match pairing brightness (`48`): red `(48,0,0)`, green `(0,48,0)`, pairing stays `(0,0,48)`
- `loop` (or an existing slave/host tick) calls `StatusRgb::service()` to expire pulses and re-apply the top layer

Alternative: each site calling `rgbLedWrite` — rejected; pairing already overwrites the pin every 250 ms.

### 3. Host ready = Wi-Fi address + `INTER_CHIP_HOST.isNormal()`, latched

Turn boot-red on at the first line of `setupHost` (before the current off at the end). In host `loop`, once `WiFiController` has SoftAP up or STA associated **and** `isNormal()`, clear boot and latch. Do not re-enter boot-red if STA later drops.

Alternative: require MQTT — rejected; proposal/specs exclude it.

### 4. Slave ready = settings applied and `ZigbeeCoordinator::isStarted()`

Turn boot-red on at the start of `setupSlave` (replace the current off). Clear when `onSlaveSettings` has started the coordinator successfully (`slaveZigbeeStarted` / `isStarted()`). SPI-only time before `SET_SETTINGS` stays boot-red, matching `zigbee-slave-radio`.

### 5. Activity = application commands/events, not keep-alives or logs

Treat as **not** a flash: `Ping`/`Pong`, `GetStatus`/`Status`, `TimeSync`, `ReadEvent`, `SlaveReady`, `LOG_RECORD`.

Treat as a flash: remaining host commands (`SetSettings`, permit-join, ZCL, device sync, file dump, …) and remaining slave events (`DeviceJoin`/`Leave`, `AttrReport`, `CmdResult`, `DeviceMap`, `DevicesFile`, `Err`).

`LOG_RECORD` is excluded so the continuous slave log stream does not hold red. Pulse from `InterChipSlave::serviceSpi` when a decoded inbound cmd or a `takeOutbound` frame is in the flash set, only if `StatusRgb` is in the activity layer.

Alternative: every CRC-valid frame — rejected; 50 ms poll + 100 ms pulse = solid light.

### 6. Critical error is a sticky flag, not every `LOGGER.error`

Set critical on SPI slave `spi_slave_initialize` failure and `Zigbee.begin` failure (coordinator never starts). Leave it set for the rest of that boot unless a later path truly recovers (today those paths do not). Do not set it for queue-full or decode misses.

`updatePairingLed` must no-op unless `StatusRgb` allows pairing.

## Risks / Trade-offs

- [Host never reaches `isNormal`] → boot-red stays on; that is the intended “still preparing / slave missing” signal.
- [Excluding logs hides SPI traffic that is only logs] → operator still sees join/report/command flashes; if that is too quiet, add `LOG_RECORD` later without changing boot or Escape.
- [Pairing and activity share one LED] → pairing wins while the join window is open; flashes resume when pairing stops.

## Migration Plan

Flash the same image to host and slave. No EEPROM version bump, no SPI version change. Rollback is the previous firmware; UI Escape is LittleFS `index.html` and needs a filesystem upload with the firmware if they are deployed separately.
