# Spec Delta

## ADDED Requirements

### Requirement: README documents S3 host and C6 slave roles
The project `README.md` MUST describe the product as two firmware images: ESP32-S3 Wi‑Fi/SPI host and ESP32-C6 Zigbee/SPI slave, with role fixed by which image is flashed. It MUST NOT instruct operators to select host vs slave with a GPIO15 strap or to flash one shared ESP32-C6 binary to both chips.

#### Scenario: README role model matches firmware
- **WHEN** a developer opens `README.md` at the repo root
- **THEN** the hardware and role sections name ESP32-S3 host and ESP32-C6 slave and do not require GPIO15 for role

#### Scenario: README does not claim one binary for both chips
- **WHEN** a developer follows the flash instructions in `README.md`
- **THEN** those instructions describe distinct host and slave images (or a joined package built from them), not one C6 `.bin` for both boards
