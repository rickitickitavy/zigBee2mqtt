## Why

A single ESP32-C6 cannot run a usable Wi-Fi STA/SoftAP and a Zigbee coordinator on one RF path. Time-sharing leaves STA “connected” with dead ping/HTTP, SoftAP join failures, and weak RSSI. The product needs a reliable gateway: Wi-Fi always usable, Zigbee always on.

## What Changes

- **BREAKING:** The gateway becomes **two ESP32-C6** boards with **one firmware image**. Role is chosen at boot from **GPIO15**: **LOW = Wi-Fi host**, **HIGH = Zigbee slave**.
- Host owns Wi-Fi (AP/STA), web console, USB CLI, **all settings** (EEPROM/LittleFS), MQTT, and all product logic. The slave SHALL NOT persist product settings.
- On host boot the host **resets the slave** (GPIO11 → slave EN), **asynchronously** waits for `SLAVE_READY`, **pushes settings**, then enters normal work. If the slave does not become ready in time, the host resets it again.
- Slave owns only the Zigbee radio (after settings arrive) and SPI. The slave **pushes each log line to the host immediately**. The host keeps one **64 KiB** rotating log for **both** chips; slave lines are labeled. Every record has date and time (NTP when available, otherwise the local timer). Zigbee stays up **regardless of host Wi-Fi mode** once configured.
- Boards talk over **SPI** (host master, slave SPI-slave). **Every** master–slave exchange is **asynchronous** (queue + IRQ + later completion/event). No Zigbee stack on the host; no Wi-Fi/MQTT/web on the slave.
- Remove single-chip coexist workarounds as the operational architecture (channel pairing, i154 + Wi-Fi on one radio, Zigbee-off-in-AP).

## Capabilities

### New Capabilities

- `board-role`: Boot-time host vs slave from GPIO15 (LOW host, HIGH slave).
- `host-slave-spi`: Asynchronous SPI framing (commands, events, settings push, immediate slave log push) plus host-driven slave reset and bring-up.
- `zigbee-slave-radio`: Slave runs coordinator RX/TX continuously, independent of host Wi-Fi mode.

### Modified Capabilities

- (none in `openspec/specs/` yet; web-console behavior stays host-only and is not restated here)

## Impact

- `pins.h`, `src/main.cpp`, `ZigbeeCoordinator`, `WiFiController`, `WebConsole`, `MqttClient`, `SerialCli`, `platformio.ini`.
- New host/slave SPI modules and a small binary protocol.
- Hardware: two C6 boards, GPIO15 strap, host GPIO11 to slave EN (reset), SPI wires (SCK 6, MOSI 5, MISO 4, CS 7, slave IRQ 10), common GND.
- Operators flash the **same** `.bin` to both chips; wiring selects the role.
