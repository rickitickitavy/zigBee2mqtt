## 1. Stop SPI flashes

- [x] 1.1 Remove `StatusRgb::isApplicationFrame` and the `pulseSend` / `pulseReceive` calls in `InterChipSlave`, and verify `pio run` still links with no SPI path referencing those symbols

## 2. Pulse API

- [x] 2.1 Replace the two-color pulse API with 100 ms green / red / blue pulses (brightness 48, last event wins, still blocked by boot/critical/pairing), and verify `StatusRgb::service()` expires a pulse back to off when no higher layer is held

## 3. Zigbee send and receive

- [x] 3.1 Pulse green after a successful `controlOnOff` radio send, and verify a rejected command (not registered / not bound) does not flash
- [x] 3.2 Pulse red or blue on inbound device events (attribute report, light-state, IAS, unregistered join) using `isRegistered`, and verify a registered report is red, an unregistered report is blue, and pairing blink is unchanged (no pulse while pairing is held)

## 4. Host always resets the slave first (sync)

- [x] 4.1 Make a blocking slave EN pulse the first step of `setupHost` (before Wi-Fi, web, MQTT, LittleFS, and `INTER_CHIP_HOST.begin`), with no skip if IRQ is already high, and verify the pulse completes (RST returns HIGH) before those later inits run
- [x] 4.2 Stop finishing that first boot pulse in the SPI pump (`pulseResetFinishIfDue` must not be what releases the boot reset), and verify a later missing-`SLAVE_READY` retry can still pulse asynchronously

## 5. Host MQTT LED pulses

- [x] 5.1 Pulse green in `MqttClient` when a subscribed device command topic is received, and verify a gateway status publish or broker keep-alive does not flash green
- [x] 5.2 Pulse red after a successful device state (or availability) MQTT publish, and verify a failed publish and `publishStatus` / `publishDevices` do not flash
