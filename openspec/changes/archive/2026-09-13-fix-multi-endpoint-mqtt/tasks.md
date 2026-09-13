## 1. Host MQTT topics

- [x] 1.1 Publish device state to `{storedStateTopic}/{ep}` with `ON`/`OFF` and never to the unsuffixed state topic, and verify a simulated attr report for ep=3 logs `MQTT send topic=…/state/3`
- [x] 1.2 Subscribe each registered device as `{commandTopic}/+` (not the bare command topic), and verify reconnect logs `MQTT subscribe topic=…/set/+`
- [x] 1.3 Route an MQTT command by parsing the last topic segment as endpoint 1–254, ignore bare or invalid suffixes, and verify `…/set/2` plus `off` enqueues SPI on/off for that IEEE and endpoint 2
- [x] 1.4 Leave availability on the stored topic with no suffix, and verify no publish or subscribe appends `/{ep}` to availability

## 2. SPI on/off endpoint

- [x] 2.1 Extend host `SpiCmdZclOnOff` to 10 bytes (`ieee` + action + endpoint) and reject shorter frames on the slave, and verify `pio run` succeeds
- [x] 2.2 Decode the endpoint on the slave and pass it into coordinator on/off so ZCL uses that endpoint, and verify a command for ep=4 logs `ep=4` and does not use another cached bind endpoint

## 3. Docs and flash

- [x] 3.1 Update the README MQTT table so state and command show the `/{ep}` suffix and availability does not, and verify the examples match the stored-prefix rule
- [ ] 3.2 Flash host and slave together and toggle two gangs on a multi-channel device, and verify host logs distinct `…/state/1` vs `…/state/2` (or 3/4) and a command to `…/set/3` only changes that gang
