# Design

## Context

Today (ADR `0002`): one ESP32-C6 `.bin` for both roles; GPIO15 selects host vs slave; web OTA stages that same file, SPI-programs the slave, then Update.writes the host. That works only while both chips run the same architecture and image.

Target: **ESP32-S3-N16R8 host** + **ESP32-C6 slave**, new PCB, same SPI/IRQ/EN topology, all LEDs kept.

## Goals / Non-Goals

**Goals**

- Separate host and slave firmwares; no role pin.
- One web Update upload that updates both chips.
- Documented host pinout for the new PCB; slave may keep current C6 SPI/LED pins.
- Join and split tools for the package.

**Non-Goals**

- Redesigning the SPI frame protocol.
- Moving Zigbee onto the S3.
- Changing product RBAC / consoles behavior beyond OTA packaging.

## Decisions

### 1) Role pin is not needed

With different SoCs, the product ships **two images**. The S3 always runs host code; the C6 always runs slave code. GPIO15 role sampling and “same image both chips” are retired. (Today’s **GPIO10** is IRQ, not the role strap—role was **GPIO15**.)

### 2) Build layout

- `env:esp32-s3-host` — Arduino, no `ZIGBEE_MODE_ZCZR`, host-only sources/flags (`BOARD_ROLE_HOST`).
- `env:esp32-c6-slave` — existing Zigbee ZCZR link flags, slave-only (`BOARD_ROLE_SLAVE`).
- Shared protocol/stores where possible; host-only web/Wi-Fi/MQTT; slave-only coordinator.

### 3) Joined package = ZIP

| Option | Pros | Cons |
|--------|------|------|
| Concat + custom magic | Tiny parser | Poor DX for join/split |
| **ZIP** (`slave.bin`, `host.bin`) | `zip`/`unzip`, clear names | Need on-device unzip |
| tar | Similar to zip | Less universal on Windows |

**Choice: ZIP** with members exactly `slave.bin` and `host.bin` (order in central directory MUST NOT matter; names matter).

**Compression:** DEFLATE MAY be used. Typical ESP app `.bin` entropy is high, so size reduction is often **small** (single-digit to low teens %). ZIP is primarily for **comfortable join/split**, not for large flash savings. If on-device inflate cost is too high, allow ZIP **STORE** (no compression) with the same member names—tools still join/split easily.

**Host OTA flow**

1. Upload ZIP to staging (same `/ota/…` path or dedicated path).
2. Validate ZIP (local headers / central directory); require both members.
3. Extract `slave.bin` → existing SPI slave OTA path.
4. On slave success, extract `host.bin` → existing host `Update` apply → reboot.

**Tools (repo scripts)**

- `scripts/firmware_join.py` — inputs host.bin + slave.bin → `firmware-joined.zip`.
- `scripts/firmware_split.py` — input joined zip → writes `host.bin` + `slave.bin`.

### 4) Host pin map (ESP32-S3-N16R8)

Avoid GPIO **26–37** (octal flash/PSRAM), **0 / 3 / 45 / 46** (strapping), and USB **19 / 20** when CDC is used.

| Signal | Host GPIO (S3) | Notes |
|--------|----------------|-------|
| `PIN_SPI_SCK` | **12** | HSPI/FSPI-friendly |
| `PIN_SPI_MOSI` | **11** | |
| `PIN_SPI_MISO` | **13** | |
| `PIN_SPI_CS` | **10** | |
| `PIN_SPI_IRQ` | **14** | Slave → host |
| `PIN_SLAVE_RST` | **9** | Host → slave EN, active-low pulse |
| `PIN_BOOT_BUTTON` | **41** | Not strap GPIO0 |
| `PIN_LED1` | **4** | |
| `PIN_LED2` | **5** | |
| `PIN_LED3` | **6** | |
| `PIN_LED4` | **7** | |
| `PIN_LED5` | **15** | |
| `PIN_LED6` | **16** | |
| Role strap | **removed** | |

**Slave C6 (keep current unless PCB forces change):** SPI MISO/MOSI/SCK/CS = 4/5/6/7, IRQ = 10, LEDs/button as in current `pins.h`; role pin unused / not connected.

PCB nets cross-map S3 host pins ↔ C6 slave pins one-to-one for SPI/IRQ/EN.

### 5) ADR

Supersede **`docs/adr/0002-two-chips-one-image.md`** with a new ADR: two chips, two images, one joined Update package.

## Risks / Trade-offs

- **OTA size / FS:** Staging must hold the ZIP (and possibly extracted bins). S3 N16 helps; still need free LittleFS headroom checks.
- **Unzip code size / RAM:** Prefer a small unzip (e.g. miniz); fall back to STORE-only ZIP if needed.
- **Wrong member flash:** Validate chip/magic or image headers before apply where practical; at minimum require both named members and non-empty sizes.
- **Pin mistakes:** Flash/PSRAM GPIOs would brick usability—map above excludes 26–37.

## Migration Plan

1. Add S3 host env and pin header; C6 slave env without role pin.
2. Joined ZIP OTA + join/split scripts.
3. Update specs/ADR; flash host+slave from joined package once.
4. Retire single-image / GPIO15 strap documentation.
