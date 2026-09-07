## Context

See `proposal.md` (Why). Today one ESP32-C6 runs Wi-Fi, web, MQTT, and `ZigbeeCoordinator` on the same RF. SoftAP + Zigbee is unsupported; STA + Zigbee starves IP. Pins live in `include/pins.h` (`PIN_BOOT_BUTTON` = GPIO9). Host path stays Arduino PlatformIO C6, LittleFS, EEPROM settings, `WebConsole`, `SerialCli`.

Role strap is GPIO15. USB D−/D+ stay on GPIO12/13, so USB Serial/JTAG stays usable. GPIO15 is an ESP32-C6 strapping pin: hold it firmly GND (host) or HIGH (slave) through reset.

## Goals / Non-Goals

**Goals:**

- One `.bin`; role from GPIO15 before any radio init.
- Host: existing Wi-Fi / web / MQTT / **sole settings store** / CLI; SPI master; no 15.4 on host.
- Slave: SPI; push each log line to the host immediately; Zigbee after host `SET_SETTINGS`; no Wi-Fi stack start.
- Host: one 64 KiB rotating log for host + slave lines, labeled, every line date-timed (NTP or local timer).
- Small framed SPI protocol with CRC and an IRQ line; **all** host↔slave interactions are asynchronous (queues, IRQ, seq-matched completions).

**Non-Goals:**

- UART/I2C inter-chip (SPI only).
- Two different firmware projects or build flags that bake the role in.
- Matter, multi-PAN, or replacing MQTT with a new northbound API.
- Changing web-console feature scope (it stays host-only).
- Synchronous SPI: no `SPI.transfer` + wait-for-reply in MQTT, HTTP, CLI, or `loop`.

## Decisions

### 1. Role read first, then branch `setup()`

**Choice:** `pinMode(PIN_BOARD_ROLE, INPUT)` (host: hard LOW to GND; slave: 10 kΩ to 3V3 or hard HIGH). Branch immediately: host `setupHost()` vs slave `setupSlave()`.

**Why:** Same image, no NVS role bit that can desync from wiring.

**Alternative:** Compile-time `-DROLE_HOST` — rejected; user required one firmware.

### 2. SPI2 pinout (not GPIO8/9)

**Choice:** Host master, slave in SPI slave mode, common GND, 3.3 V logic, short wires.

| Signal | Host GPIO | Slave GPIO | Notes |
|--------|-----------|------------|--------|
| ROLE | 15 | 15 | Host GND, slave HIGH (strapping; hold through reset) |
| SCK | 6 | 6 | SPI clock |
| MOSI | 5 | 5 | Host → slave |
| MISO | 4 | 4 | Slave → host |
| CS | 7 | 7 | Active LOW |
| IRQ | 10 in | 10 out | Slave HIGH when TX queue non-empty |
| RST | 11 out | slave **EN** | Active LOW pulse; host `PIN_SLAVE_RST`. Idle HIGH. |

BOOT (GPIO9) on the host keeps “force AP”. Do not reuse GPIO8 (RGB / strap). GPIO11 is unused by SPI (CS is 7) and is not USB/ROLE.

**Alternative:** SPI on default VSPI-style pins that collide with RGB/USB — rejected.

### 3. Frame format

**Choice:** `[0xA5][ver=1][cmd:u8][seq:u8][len:u16 LE][payload][crc16:u16 LE]` over SPI. Max payload 256 bytes (fits typical ZCL reports; larger attributes chunk later if needed). CRC-16/CCITT. Unknown `cmd` → `RSP_ERR`.

Initial commands (host → slave): `PING`, `GET_STATUS`, `SET_SETTINGS`, `TIME_SYNC`, `CLEAR_LOG`, `PERMIT_JOIN`, `LEAVE`, `ZCL_CMD`, `RESET_RADIO`.  
Events (slave → host): `SLAVE_READY`, `PONG`, `STATUS`, `SETTINGS_OK`, `LOG_RECORD`, `DEVICE_JOIN`, `DEVICE_LEAVE`, `ATTR_REPORT`, `CMD_RESULT`, `ERR`.

IRQ: slave sets GPIO10 HIGH while at least one outbound frame is queued. A **host SPI task** (not product callbacks) CS-selects, clocks a dummy MOSI (0x00) or a `READ_EVENT` command to pull the next frame, then deasserts CS.

**Alternative:** JSON lines on SPI — too heavy on the slave. **Alternative:** ESP-NOW — not SPI.

### 3b. Asynchronous only

**Choice:** Two queues on each chip.

Host: product code (`ZigbeeSpiProxy`) `tryEnqueue(cmd, seq, payload)` and returns immediately. A dedicated SPI master task drains the outbound queue (DMA or short `transfer` only inside that task), watches IRQ plus a 200 ms idle poll, and posts inbound frames to an event queue. Completions match `seq`. MQTT/web/CLI register handlers; they never call into the SPI driver wait path. Timeouts fire from the SPI task as `ERR`/`TIMEOUT` events.

Slave: SPI slave ISR/DMA only copies a complete frame into an inbound queue and returns. A worker applies Zigbee (`permitJoin`, on/off, …) and pushes `CMD_RESULT` / events to an outbound queue, then asserts IRQ. Zigbee callbacks only enqueue; they do not talk to the SPI hardware.

**Why:** User requirement that every master–slave interaction is asynchronous; keeps Wi-Fi and Zigbee off each other’s critical paths.

**Alternative:** Blocking request/response in the MQTT handler — rejected.

### 4. Radio ownership

**Choice:** `ZigbeeCoordinator` runs only on the slave, start in `setupSlave()` with no Wi-Fi delay and no `esp_coex_wifi_i154` on that chip (slave never starts Wi-Fi). Host deletes coexist / “Zigbee off in AP” paths. Host talks to a `ZigbeeSpiProxy` that **enqueues** work; MQTT/web keep the same topics but wait for results only via later events, not return values from SPI.

**Why:** Matches “Zigbee always, independent of Wi-Fi mode.”

**Alternative:** Keep Zigbee on host for “backup” — rejected; one RF, same failure mode.

### 5. Slave software surface

**Choice:** Slave after reset: SPI + RAM log only, then enqueue `SLAVE_READY`. No `WiFi.begin`, no `WebConsole`, no MQTT, no settings filesystem. Coordinator `begin` runs only after `SET_SETTINGS` is applied. Host EEPROM remains the only settings store.

### 6. Host bring-up state machine (async)

**Choice:** Host GPIO11 (`PIN_SLAVE_RST`) wired to the **slave EN** pin (open-drain or push-pull, idle HIGH, pulse LOW ≥10 ms). State machine in the host SPI/bring-up task, never in a blocking `delay` on `loop`:

1. `RESET` — pulse EN, start ready timer (10 s).
2. `WAIT_READY` — host Wi-Fi/web/MQTT already running; wait for `SLAVE_READY`.
3. On timeout → back to `RESET` (no attempt limit).
4. `PUSH_SETTINGS` — enqueue `SET_SETTINGS` from host `GlobalSettings` (Zigbee channel, permit-join default, and other radio fields).
5. `NORMAL` — accept permit-join / ZCL / incoming `LOG_RECORD`. Periodic `TIME_SYNC`. If the link later goes silent (ready-watchdog), return to `RESET`.

**Why:** User-required master-owned settings and reset/retry without blocking.

**Alternative:** Soft `RESET_RADIO` only — rejected as the first action; hardware EN clears a wedged slave.

### 7. Combined host log (64 KiB), slave push

**Choice:** Product log lives **only on the host**: one `char` ring of **65536** bytes. Each record is one line:

`YYYY-MM-DD HH:MM:SS [host] …` or `YYYY-MM-DD HH:MM:SS [slave] …`

Slave logger **immediately** enqueues `LOG_RECORD` on the **same outbound queue** as `ATTR_REPORT` / join events. GPIO10 IRQ and the host SPI task treat a log frame like any other slave→host event (same header, CRC, CS clock-out). No second link. UART print on the slave is optional and local-only. Slave keeps only a short shared TX queue (a few frames), not a 64 KiB archive. Zigbee events and log lines are just different `cmd` values in that queue.

Host clock: SNTP/NTP when STA can reach a server; otherwise a local timer clock from boot (still formatted as date-time, epoch starting at 1970-01-01 plus uptime or last-known). Host includes `unix_sec` (or equivalent) in `SET_SETTINGS` and repeats `TIME_SYNC` so the slave can stamp. If the slave has never synced, it sends local `millis` and the host writes the line using the host clock and still labels `[slave]`.

Web `/api/log` and CLI read this single host buffer (both sources).

**Alternative:** 32 KiB pull-from-slave archive — rejected. **Alternative:** two separate host buffers — rejected; one 64 KiB store, labeled.

## Risks / Trade-offs

- [Role float / GPIO15 strap] → Host ties GPIO15 to GND; slave uses a hard HIGH or ≤10 kΩ to 3V3 through reset so boot strapping and role stay defined.
- [SPI slave + Zigbee ISR] → SPI ISR only queues; Zigbee and CRC/retry live in tasks. Host retries are async (re-enqueue).
- [No IRQ / missed IRQ] → Host also polls `GET_STATUS` on a slow timer (e.g. 200 ms) so events are not lost if IRQ wiring fails.
- [CS glitch / clock mismatch] → Same SPI mode 0, ≤ 8 MHz first; common GND; short wires.
- [Two-board ops] → Same flash command twice; document ROLE, RST→EN, and SPI wiring in README at apply time.
- [Slave EN vs USB] → Pulse EN only on the slave board; do not wire GPIO11 to the host EN.
- [64 KiB host RAM] → Combined log on the host only; slave does not keep a large archive.
- [Log flood] → Cap slave `LOG_RECORD` queue; drop oldest pending SPI log frames if the host is slow; host ring still wraps at 64 KiB.
- [NTP lag] → Until SNTP succeeds, use local timer date-time; switch new lines to NTP once synced; `TIME_SYNC` to slave after that.

## Migration Plan

1. Land firmware with role pin and async SPI; host still builds if slave absent (`PING` timeout event → log, MQTT/web up, no mesh).
2. Wire two DevKits; slave GPIO15 HIGH; host GPIO15 GND; host GPIO11 to slave EN; SPI (SCK 6, MOSI 5, MISO 4, CS 7) + IRQ 10.
3. Remove single-chip Zigbee start from host after proxy works.
4. Rollback: unwire SPI, leave host GPIO15 LOW, flash previous single-chip build if needed.

## Open Questions

- Exact ZCL address encoding (short vs IEEE) can match current `ZigbeeCoordinator` APIs when applying.
- Exact `SET_SETTINGS` payload fields follow current `GlobalSettings` Zigbee members at apply time.
