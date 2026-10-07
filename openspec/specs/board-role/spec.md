# board-role Specification

## Purpose

Selects host or slave by which firmware image is flashed: ESP32-S3 always runs as Wi-Fi/SPI host; ESP32-C6 always runs as Zigbee/SPI slave. Role is not chosen by a GPIO strap.

## Requirements

### Requirement: Role pin sampled at boot
The product SHALL NOT sample a GPIO role strap to choose host vs slave. The ESP32-S3 firmware image SHALL always run as the Wi-Fi/SPI host. The ESP32-C6 firmware image SHALL always run as the Zigbee/SPI slave. Role SHALL be fixed by which image is flashed to which chip.

#### Scenario: Host image on S3
- **WHEN** the ESP32-S3 host image boots
- **THEN** the chip runs as host (Wi-Fi, settings, web, MQTT, SPI master) and does not start the Zigbee radio

#### Scenario: Slave image on C6
- **WHEN** the ESP32-C6 slave image boots
- **THEN** the chip runs as slave (Zigbee radio and SPI slave only) and does not start Wi-Fi, the web console, or MQTT

### Requirement: Same image both chips
The project SHALL produce **two** firmware application images: one for the ESP32-S3 host and one for the ESP32-C6 slave. A developer or CI step SHALL be able to combine those images into one joined Update package for the web console. Flashing SHALL NOT rely on GPIO wiring to select role.

#### Scenario: Distinct binaries
- **WHEN** the host and slave images are built
- **THEN** they are different binaries suitable only for their respective chips

#### Scenario: Joined package for field update
- **WHEN** an operator uses System → Update with one joined package file
- **THEN** the gateway can update both the slave and the host from that single upload

### Requirement: README documents S3 host and C6 slave roles
The project `README.md` MUST describe the product as two firmware images: ESP32-S3 Wi‑Fi/SPI host and ESP32-C6 Zigbee/SPI slave, with role fixed by which image is flashed. It MUST NOT instruct operators to select host vs slave with a GPIO15 strap or to flash one shared ESP32-C6 binary to both chips.

#### Scenario: README role model matches firmware
- **WHEN** a developer opens `README.md` at the repo root
- **THEN** the hardware and role sections name ESP32-S3 host and ESP32-C6 slave and do not require GPIO15 for role

#### Scenario: README does not claim one binary for both chips
- **WHEN** a developer follows the flash instructions in `README.md`
- **THEN** those instructions describe distinct host and slave images (or a joined package built from them), not one C6 `.bin` for both boards
