## Why

The slave RGB currently flashes on SPI application frames, so keep-alive filtering still lights the LED on host commands and slave-to-host dumps that are not radio traffic. Operators need the slave LED to mean Zigbee device events and the host LED to mean MQTT device traffic. The host must also hard-reset the slave on every host boot so a leftover slave image cannot stay up across a host restart.

## What Changes

- Stop flashing the slave status RGB for SPI frames (including application commands, dumps, and logs).
- Flash **green** 0.1 s when the coordinator sends a Zigbee command event to an end device (not “any data”).
- Flash **red** 0.1 s when an event is received from a **registered** device.
- Flash **blue** 0.1 s when an event is received from an **unregistered** device.
- Keep the existing pairing **blue blink**; it still outranks activity pulses.
- Host flashes **green** 0.1 s when it receives a **device** event from MQTT (command topic).
- Host flashes **red** 0.1 s when it **publishes** a device message to MQTT (state / availability), not gateway status or broker keep-alives.
- Boot-red and critical-error red stay as they are and outrank these pulses.
- Host **always** resets the slave as the **first** step of host init, in **sync** (blocking pulse: assert, wait, release). It MUST NOT defer that boot pulse to the SPI task, and MUST NOT skip it if the slave already looks ready.
- After that pulse, waiting for `SLAVE_READY` / settings push may stay asynchronous.
- No **BREAKING** SPI, MQTT, or HTTP changes.

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `status-rgb-led`: Replace SPI RX/TX flashes with Zigbee send / registered-receive / unregistered-receive pulses on the slave; add host MQTT device-command (green) and device-publish (red) pulses; pairing blink unchanged.
- `host-slave-spi`: Host init MUST start with a synchronous slave reset pulse; later `SLAVE_READY` wait stays async.

## Impact

- `StatusRgb`: pulse colors for send / registered receive / unregistered receive; drop SPI frame classification.
- `InterChipSlave`: remove `pulseSend` / `pulseReceive` on SPI fill/decode.
- `ZigbeeCoordinator`: pulse green on a successful device command send (`controlOnOff` / equivalent); pulse red or blue on inbound device events using `isRegistered`.
- `MqttClient`: pulse green when a subscribed device command arrives; pulse red when a device state/availability publish succeeds.
- Host `setupHost`: first action is a blocking slave EN pulse; `InterChipHost` must not rely on the SPI pump to finish that first pulse.
- Main spec Purpose still mentions SPI traffic until archive/sync; requirement text is the contract.
