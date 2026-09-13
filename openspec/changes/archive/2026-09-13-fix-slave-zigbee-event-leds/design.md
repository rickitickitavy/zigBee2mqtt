## Context

See proposal.md for motivation and the delta spec for the flash contract.

`StatusRgb` already owns layers (critical > boot > pairing > 100 ms pulse > off) and pairing blink. `InterChipSlave::fillHardwareQueue` / `serviceSpi` call `pulseSend` / `pulseReceive` on application SPI cmds. Inbound Zigbee already has IEEE + `isRegistered` in `handleAttributeReport`, `handleLightStateWithSource`, IAS handlers, and join (`offerPairingIfNeeded`). Outbound device commands go through `controlOnOff` (`lightOn` / `lightOff` / `lightToggle`). `InterChipHost::begin` currently starts an **async** reset (`enterReset` + `pulseResetFinishIfDue` in the SPI task) and is called **late** in `setupHost` (after Wi-Fi, web, MQTT). This change moves a **blocking** reset pulse to the **first** host-init step.

## Goals / Non-Goals

**Goals:**

- Pulse only on Zigbee device command send and device-originated events (slave).
- Color from registry: registered receive = red, unregistered receive = blue, send = green.
- Host: green on inbound MQTT device command; red on outbound MQTT device publish.
- Leave pairing blink, boot-red, and critical-error red alone except that pulses still yield to them.
- On every host boot, the first host-init step is a **synchronous** slave reset pulse (complete before Wi-Fi, web, MQTT, or the SPI task start).

**Non-Goals:**

- Flashing on permit-join, settings, device-map sync, SPI dumps, MQTT `status`/`devices` topics, or broker pings.
- Changing how `isRegistered` is stored.
- Changing reset-pulse width (`kRstPulseMs`). Timeout retries for a missing `SLAVE_READY` stay async after the first pulse.

## Decisions

### 1. Remove SPI pulses entirely

Delete `StatusRgb::isApplicationFrame` and the two call sites in `InterChipSlave`. Alternative: keep SPI flashes behind a flag — rejected; the spec forbids link traffic flashes.

### 2. Three pulse colors on `StatusRgb`

Replace boolean receive/send with an explicit color: green / red / blue at brightness `48`, still 100 ms, last event wins. `allowsActivityPulse()` stays `!critical && !boot && !pairing`.

### 3. Green = successful `controlOnOff` radio send

Pulse after `lightOn` / `lightOff` / `lightToggle` (or the same path if another device command is added later). Do not pulse when the command is rejected (not started, not bound, not registered). Alternative: pulse on `SpiCmdZclOnOff` in the SPI handler — rejected; that is host data, not a confirmed send to the device.

### 4. Receive pulse at the coordinator event handlers

After IEEE is resolved, call `isRegistered(ieee)` and pulse red or blue. Cover the same paths that already log a device event: attribute reports, on/off light-state callback, IAS status/enroll, and unregistered join. Do not pulse on host-only SPI forwarding. During pairing, `setPairingHeld(true)` already blocks pulses so unregistered joins do not interrupt the blue blink.

### 5. First host-init step is a synchronous slave reset

At the start of `setupHost` (before LittleFS, Wi-Fi, web, MQTT, CLI, or `INTER_CHIP_HOST.begin()`), call a blocking reset: drive `PIN_SLAVE_RST` LOW, `delay` the existing pulse width, drive HIGH. Do not finish that first pulse from `hostSpiTask`. Do not skip because IRQ is high or the slave still looks alive.

`begin()` then starts SPI and the async `SLAVE_READY` wait / settings push. It MUST NOT issue a second boot pulse if the sync pulse already ran this boot (later timeout retries may still pulse). Alternative: move async `enterReset` earlier — rejected; the user requires sync mode and first-step ordering. Alternative: block all of `setupHost` until `SLAVE_READY` — rejected unless we later ask; only the reset pulse is synchronous so Wi-Fi can still start while the slave comes up.

### 6. Host MQTT pulses (device topics only)

After host boot-red is cleared, `MqttClient::onMessage` pulses **green** when the topic matches a registered device command topic (the same path that becomes a Zigbee command). `publishDeviceState` (and device availability if that helper publishes) pulses **red** after a successful `publish`. Do not pulse on `publishStatus`, `publishDevices`, time/status retain, or failed publish. `allowsActivityPulse()` on the host is `!boot` (no pairing layer). Same 100 ms / brightness 48 / last-wins API as the slave.

## Risks / Trade-offs

- [Unknown IEEE on a report] → treat as unregistered (blue) rather than inventing a fourth color.
- [Chatty sensors] → 100 ms last-wins may look almost solid red; that is intended “device is talking.”
- [Permit-join has no green] → only end-device commands flash green; opening the join window stays pairing-blink only.
- [Reset on every host boot drops in-flight slave radio state] → intended; the host then waits for `SLAVE_READY` and pushes settings again.
- [Chatty MQTT state] → 100 ms last-wins red; gateway `status`/`devices` stays dark so boot chatter does not flash.

## Migration Plan

Same firmware image on host and slave. No EEPROM or protocol version. Rollback is the previous image.
