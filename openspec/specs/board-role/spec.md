# board-role Specification

## Purpose

Selects host or slave at boot from GPIO15 so one firmware image can run on both ESP32-C6 chips.

## Requirements

### Requirement: Role pin sampled at boot
The firmware SHALL read GPIO15 once at startup, before starting Wi-Fi or Zigbee. The firmware SHALL treat a LOW level as the Wi-Fi host role and a HIGH level as the Zigbee slave role. The firmware SHALL NOT change role until the next reset.

#### Scenario: Host strap
- **WHEN** GPIO15 is LOW at startup
- **THEN** the chip runs as host (Wi-Fi, settings, web, MQTT, SPI master) and does not start the Zigbee radio

#### Scenario: Slave strap
- **WHEN** GPIO15 is HIGH at startup
- **THEN** the chip runs as slave (Zigbee radio and SPI slave only) and does not start Wi-Fi, the web console, or MQTT

### Requirement: Same image both chips
The project SHALL produce one firmware image that implements both roles. Hardware wiring of GPIO15 SHALL be the only required difference that selects the role.

#### Scenario: Flash same binary
- **WHEN** the same firmware is flashed to both chips and GPIO15 is wired LOW on one board and HIGH on the other
- **THEN** one board becomes host and the other becomes slave
