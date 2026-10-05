# Design

## Context

See proposal.md — Why. The slave already loops `channels` 2–16 and calls an On/Off attribute read for endpoints 1..N from `enqueueRegisteredStatusReads`. The host status cache and the Devices table already keep one On/Off circle per endpoint. A 4-channel relay still shows only endpoint 1 after reboot.

Two behaviors in the current slave path explain that. `markRegistryReady` runs on every device-sync record and `maybeStartStatusRefresh` runs the pass once, so a pass that starts before the rest of the boot list is applied never runs again. The pass then transmits every endpoint read immediately. Each endpoint has its own in-flight slot, but the Zigbee send path takes one output buffer at a time and disables the default response, so a later endpoint can leave the queue without a retry. A reply that is not tagged with the device endpoint is stored on endpoint 1, which fills channel 1 and leaves channel 3 dark.

## Goals / Non-Goals

**Goals:**

- After boot, a registered `onOff` device with `channels` 4 shows the On/Off answer from endpoints 1, 2, 3, and 4 on those channels.
- A device record that arrives after the first boot sync record still gets that read.
- Operator on/off commands to two endpoints of the same IEEE stay independent.

**Non-Goals:**

- Changing parse mode (`channels` 0) endpoint discovery.
- Persisting live status across reboot.
- Wi-Fi device state, measurement formatting, or MQTT topic names.

## Decisions

1. **Run the boot status pass again for devices applied after the first record.** Keep a per-device “status read issued this boot” flag instead of one global flag that closes the pass forever. When the coordinator is already started, an upsert of a Zigbee `onOff` device queues its channel reads. When the coordinator starts later, it queues reads for every registered Zigbee device then present. Alternative: add a host “list complete” SPI flag. Rejected because the slave can refresh each device as it arrives without a new frame type.

2. **Serialize status attribute reads per IEEE.** For the boot (and the same helper used by a later status collect), send one attribute read to a device, and hold the next endpoint or battery read until that read is answered or the in-flight timeout fires, then send the next. Operator on/off and write-attribute commands keep today’s per-endpoint slots. Alternative: fire all endpoints at once and only retry when `zb_buf_get_out` fails. Rejected because a send that returns success can still be the only transaction the device answers.

3. **Store the reply on the device endpoint.** Prefer the ZCL source endpoint when it is a usable device endpoint (1–240) and is not the coordinator’s own endpoint. If the stack reports only the coordinator endpoint, apply the value to the destination endpoint of the in-flight status read for that IEEE and cluster. Do not copy endpoint 1’s value onto other channels.

## Risks / Trade-offs

- [A sleepy relay never answers endpoint 3] → That circle stays unknown (dark, not a copied ON). The coordinator still finishes startup.
- [Serial reads make boot status slower on a 4-gang] → Four reads plus battery are still one device. Accept the delay so channel 3 is real.
- [In-flight timeout is shorter than the device’s reply] → The next endpoint still goes out, and a late reply for the previous endpoint must still match that endpoint rather than the new in-flight one. Match a late reply by source endpoint when the stack provides it.

## Migration Plan

Flash the slave and the host together. No settings or device-list migration. Rollback is the previous firmware image. After boot, channels 1 and 3 of a relay that is ON on those endpoints show light circles, and channels that are OFF show dark circles.

## Open Questions

None.
