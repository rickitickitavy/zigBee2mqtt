# Proposal

## Why

Wi-Fi host work on ESP32-C6 shares silicon with the Zigbee slave path on a second C6. Moving the **master (host)** to **ESP32-S3-N16R8** keeps Zigbee on a dedicated C6 slave while giving the host more flash/RAM and a cleaner Wi-Fi/web/MQTT platform. One shared C6 image selected by a role strap can no longer work: the chips are different architectures and need different binaries, while the web Update tab MUST still accept **one** file.

## What Changes

- Port the **host** role to **ESP32-S3-N16R8** (new PCB). Keep the **slave** on **ESP32-C6** (Zigbee ZCZR).
- Produce **two PlatformIO environments / images**: host-only (S3) and slave-only (C6). Compile-time role; **no runtime role strap**.
- Remove **`PIN_BOARD_ROLE` / GPIO15 role sampling** from product requirements (ADR `0002` superseded). GPIO10 remains an inter-chip **IRQ** candidate unless remapped with the new host pinout.
- Keep the inter-chip link model: host SPI master, **CS + SCK + MOSI + MISO**, slave→host **IRQ**, host→slave **EN/reset**.
- Keep **boot button + LED1–LED6** on the host (and slave LEDs as today); remap host pins for S3 (avoid flash/PSRAM and strapping pins).
- Web console Update MUST still upload a **single** package. Introduce a **joined firmware package** that contains both `slave.bin` and `host.bin`, plus **join** and **split** tooling for developers.
- Prefer a **ZIP** container (`slave.bin` + `host.bin` members) for comfort with standard zip/unzip. Compression of ESP app binaries is usually **modest**; ZIP is chosen for tooling UX, not large size savings. Host OTA extracts both members, programs the slave over SPI first, then applies the host image (same order as today).
- Firmware `FIRMWARE_VERSION` set to **`0.5.0`** for this port (main version bump).

## Capabilities

### New Capabilities

- `joined-firmware-ota`: Single-file joined package format, join/split tools, and host Update flow that extracts slave then host images from that package.

### Modified Capabilities

- `board-role`: Drop same-image/GPIO15 role selection; host and slave are distinct builds/images.
- `host-slave-spi`: Host runs on ESP32-S3 with a new GPIO map for SPI/IRQ/EN; link protocol and async rules stay.
- `web-console`: Update tab accepts the joined package (still one upload); behavior remains slave-then-host apply.
- `status-rgb-led`: Host LED and boot-button GPIOs remapped for ESP32-S3-N16R8.

## Impact

New PCB wiring (pins decided in design). PlatformIO split envs, partition tables per chip, host `pins.h` map, remove role strap from `main`, OTA staging/parse for ZIP joined package, scripts to join/split packages, supersede ADR `0002`, update `host-slave-spi` pin documentation. Slave Zigbee and SPI protocol stay; slave may keep its current C6 pin map so only the host side of the PCB changes for those nets.
