## Why

A registered device is one IEEE with one state topic and one command topic. Multi-gang switches report each gang as a Zigbee endpoint (`ep` in the slave log), but the host publishes every gang to the same topic and commands the last remembered endpoint. Operators cannot tell which channel changed or which channel a command should hit.

## What Changes

- **BREAKING**: device **state** and **command** MQTT traffic SHALL use the stored topic plus a `/{channelId}` suffix, where `channelId` is the Zigbee endpoint (`ep`). Example: stored `z2m/switcher_1/state` becomes `z2m/switcher_1/state/1` for endpoint 1. Payload stays `ON` / `OFF` (or `on` / `off` / `toggle` on command).
- **Availability** SHALL stay the stored availability topic with **no** channel suffix (one topic for the whole device).
- Stored device settings stay one IEEE and one prefix triple (state / command / availability). The host appends the suffix at publish/subscribe time. No extra map slots per gang.
- Host command subscribe SHALL listen on `{commandTopic}/+` (or the equivalent per-channel topics) and SHALL send the ZCL on/off to the endpoint taken from the last topic segment.
- SPI on/off commands SHALL carry the destination endpoint. The slave SHALL use that endpoint instead of the single cached bind endpoint.

## Capabilities

### New Capabilities

- `mqtt-device-topics`: how the host maps a registered device’s stored MQTT prefixes to per-channel state/command topics and a single availability topic

### Modified Capabilities

- `host-slave-spi`: `SpiCmdZclOnOff` must include the destination endpoint so the slave can address the correct gang

## Impact

- Host `MqttClient` publish/subscribe and `main` command routing
- `ZigbeeSpiProxy::controlOnOff`, `InterChipSlave` ZCL decode, `ZigbeeCoordinator::controlOnOff`
- SPI payload for `SpiCmdZclOnOff` (IEEE + action + endpoint)
- README MQTT topic table
- Existing Home Assistant / MQTT consumers that subscribe to unsuffixed state/command topics must switch to `…/state/{ep}` and `…/set/{ep}`
