# 0003. Two chips, two images, one joined Update ZIP

## Status

Accepted. Supersedes [0002. Two chips, one image](0002-two-chips-one-image.md).

## Context

Wi-Fi host and Zigbee slave run on separate MCUs. ADR 0002 used one ESP32-C6 `.bin` and GPIO15 to pick the role. Moving the host to ESP32-S3-N16R8 makes that impossible: different architectures need different binaries, while System → Update must still accept one file.

## Decision

- Ship **two** application images: ESP32-S3 host and ESP32-C6 slave.
- Role is fixed by which image is flashed; **no** GPIO role strap.
- Field updates use one **joined ZIP** with members `slave.bin` and `host.bin` (DEFLATE or STORE). The host programs the slave over SPI first, then applies `host.bin`.
- Repository scripts join and split that ZIP offline.

## Consequences

- Two PlatformIO environments (`esp32-s3-host`, `esp32-c6-slave`).
- Host pin map is S3-specific; slave may keep its C6 SPI/LED map.
- ADR 0002’s “do not split images / bake role into a build flag” guidance no longer applies.
