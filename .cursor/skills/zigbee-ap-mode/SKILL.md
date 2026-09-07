---
name: zigbee-ap-mode
description: >-
  Two-chip gateway: Zigbee runs on the slave after host settings, including
  while the host is in Wi-Fi AP or STA. Use when starting Zigbee, SoftAP,
  Wi-Fi MODE AP/STA, coordinator boot, or host/slave bring-up.
---

# Zigbee vs AP (two ESP32-C6)

Do **not** start Zigbee on the Wi-Fi host. The slave coordinator starts only after host `SET_SETTINGS` and then stays on regardless of host SoftAP, recovery AP, or STA.

- Never call `Zigbee.begin()` or coordinator `begin()` on the host board (`role=host`, GPIO15 LOW).
- On the slave, do not start the coordinator until `SET_SETTINGS` arrives.
- Do not re-enable single-chip IEEE 802.15.4 coexist or “Zigbee off in AP” as the product architecture.
- Host GPIO11 pulses slave EN if `SLAVE_READY` is late.
