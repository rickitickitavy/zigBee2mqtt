## 1. Role and pins

- [x] 1.1 Add `PIN_BOARD_ROLE` (GPIO15), `PIN_SLAVE_RST` (GPIO11), and SPI pins (MISO 4, MOSI 5, SCK 6, CS 7, IRQ 10) from `design.md` to `include/pins.h` and verify a compile of the existing target still succeeds (`pio run`)
- [x] 1.2 Split `src/main.cpp` into host vs slave after reading GPIO15 once, and verify serial log prints `role=host` or `role=slave` matching the pin at reset

## 2. SPI protocol

- [x] 2.1 Add shared frame encode/decode (sync, version, cmd, seq, length, CRC-16) and verify encode→decode round-trip on a known fixture payload
- [x] 2.2 Implement a host SPI **task** (outbound queue, IRQ, 200 ms poll) so `PING` is enqueue-only, and verify `PONG` arrives later on the event queue (caller does not block)
- [x] 2.3 Implement slave SPI ISR→inbound queue, worker→outbound queue, and IRQ while TX is non-empty, and verify a host `PING` yields an async `PONG` and IRQ drops after the queue is empty
- [x] 2.4 Add seq-matched timeout/error events when the slave does not reply, and verify a disconnected slave produces an async timeout without stalling `loop` or MQTT

## 3. Host bring-up, settings, and slave log

- [x] 3.1 Pulse host GPIO11 (slave EN) on host start and on ready-timeout, and verify the slave reboots (UART/reset log) without the host `loop` blocking
- [x] 3.2 Implement async `WAIT_READY` → `SET_SETTINGS` → `NORMAL`, and verify Zigbee commands are not enqueued until `SETTINGS_OK` and a silent slave causes another reset
- [x] 3.3 Store all product settings only on the host and verify the slave has no settings save path
- [x] 3.4 Stamp every log line with date-time (NTP when the host has it, else local timer) and `[host]` / `[slave]`, and verify a line from each source appears in the host 65536-char ring
- [x] 3.5 Push each slave log line as `LOG_RECORD` on the same IRQ + SPI queue as Zigbee events, and verify the host buffer updates without a pull and wraps at 64 KiB

## 4. Slave Zigbee only

- [x] 4.1 Start `ZigbeeCoordinator` on the slave only after `SET_SETTINGS`, with no Wi-Fi init, and verify coordinator start follows settings and `WiFi.status()` never leaves idle
- [x] 4.2 Map SPI commands to `permitJoin`, `closeJoin`, `controlOnOff`, and inbound light/join events, and verify a permit-join command from a host-side test frame changes coordinator join window
- [x] 4.3 Keep coordinator running for the slave lifetime after settings, and verify it stays started after the host is cycled through SoftAP and STA (slave power stays on)

## 5. Host product path

- [x] 5.1 Stop starting Zigbee and IEEE 802.15.4 coexist on the host, and verify host boot never calls coordinator `begin` (log + no 15.4 enable on host)
- [x] 5.2 Replace host call sites (`MqttClient`, CLI, web if any) with an async SPI proxy (enqueue only), and verify permit-join and on/off from MQTT/CLI return immediately and still produce slave SPI commands
- [x] 5.3 Forward slave `ATTR_REPORT` / join events into existing MQTT publish and device JSON, and verify a simulated slave event publishes the same topics as today
- [x] 5.4 Confirm host SoftAP and STA stay pingable and the web console answers HTTP while the slave radio is up (no host Zigbee start)
- [x] 5.5 Point host CLI/web `/api/log` at the combined 64 KiB host buffer, and verify both `[host]` and `[slave]` dated lines appear without a log-pull command

## 6. Docs and ops

- [x] 6.1 Document ROLE strap (host GPIO15 GND, slave HIGH), host GPIO11 to slave EN, and SPI wiring (MISO 4, MOSI 5, SCK 6, CS 7, IRQ 10), and verify the pin table matches `pins.h`
- [x] 6.2 Update `zigbee-ap-mode` / coexist notes so they describe two-chip (Zigbee always on slave after host settings), and verify the skill no longer requires Zigbee off in AP as the product architecture
