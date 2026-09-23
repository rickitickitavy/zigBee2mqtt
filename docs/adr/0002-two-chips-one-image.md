# 0002. Two chips, one image

## Context

Wi-Fi and Zigbee on one ESP32-C6 starve each other. The product uses two C6 boards.

## Decision

One firmware `.bin` implements both roles. GPIO15 sampled at boot selects the role: LOW = host, HIGH = slave. Role does not change until reset.

Host runs Wi-Fi, MQTT, settings, web console, USB CLI, and SPI master. It does not start the Zigbee radio.

Slave runs the Zigbee coordinator and SPI slave only. It does not start Wi-Fi, the web console, or MQTT. It has no product business rules.

## Consequences

Do not split into two firmware projects or bake the role into a build flag without a new ADR. Hardware must hold GPIO15 through reset (C6 strap).
