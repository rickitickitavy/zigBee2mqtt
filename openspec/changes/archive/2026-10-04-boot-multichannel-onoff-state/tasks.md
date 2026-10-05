# Tasks

## 1. Boot status pass covers every registered device

- [x] 1.1 Replace the one-shot boot status flag so a Zigbee device upserted after the coordinator is already started still gets a status read, and a coordinator start reads every Zigbee device already registered. Verify with a log or probe that a second registered `onOff` device applied after the first still queues On/Off reads.
- [x] 1.2 Queue On/Off reads for endpoints 1 through `channels` when `channels` is 2–16, and send the next read for that IEEE only after the previous attribute read is answered or its in-flight timeout fires. Verify a `channels` `4` device queues endpoints 1, 2, 3, and 4, and that an operator ON to endpoint 1 still does not wait behind endpoint 3’s command slot.

## 2. Keep each channel’s answer

- [x] 2.1 Apply an On/Off read reply to the device endpoint that sent it. If the stack only reports the coordinator endpoint, use the destination endpoint of the matching in-flight status read. Verify endpoint 3 ON is stored as endpoint 3 and does not replace endpoint 1.
- [x] 2.2 Confirm the Devices table draws one circle per channel from those endpoints. Verify a `channels` `4` row with ON, OFF, ON, OFF shows light, dark, light, dark circles in that order, and a channel with no reply is not drawn as ON.

## 3. Firmware

- [x] 3.1 While applying this change, increment `FIRMWARE_VERSION` build by 1 from `0.4.2` and verify `pio run -e esp32-c6-devkitc-1` succeeds. Flash host and slave together before checking a 4-channel relay after reboot.
