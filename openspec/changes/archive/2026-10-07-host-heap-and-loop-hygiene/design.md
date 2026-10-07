# Design

## Context

See proposal.md — Why. Host `esp32-s3-host` runs AsyncWebServer, MQTT, and SPI master; slave `esp32-c6-slave` runs Zigbee ZCZR and SPI slave. Observed hotspots: `WebConsole::appendRequestBody` (shared `String`, per-char `+=`), duplicated `devicesJson` in `ZigbeeCoordinator` / `ZigbeeSpiProxy`, `Logger::{error,warning,info,debug}(String)`, `WiFi.scanNetworks(false, …)` and `MqttClient::reconnect()` on the loop task, `esp_zb_lock_acquire(portMAX_DELAY)` at the ZCL send path, obsolete dual-C6 README. Fragile zones `slave-spi-ota` and `firmware-update-process` stay out of scope. Firmware propose bump is `1.0.10`.

## Goals / Non-Goals

**Goals:**
- Heap-stable HTTP body and large JSON paths on the S3 host
- Non-blocking (or off-loop) STA scan and MQTT connect so HTTP/SPI progress
- Fail-soft Zigbee lock on slave ZCL send
- One shared device-list JSON builder; README matches `board-role`

**Non-Goals:**
- Migrating off Arduino / PubSubClient / AsyncWebServer
- Changing SPI frame layout, OTA chunk timing, or joined ZIP flow
- Full structured logging or ArduinoJson adoption everywhere

## Decisions

1. **Per-request body buffer with hard cap**  
   Prefer attaching a small body collector on the `AsyncWebServerRequest` (or a short-lived request-local buffer) instead of `WebConsole::requestBody`. Cap at a named constant (e.g. restore/settings ~256–512 KiB max, smaller for typical JSON posts). Append with `concat`/`memcpy` of the chunk, never per-char. On overflow: mark failed, discard, return 413/400.  
   *Alternatives:* keep shared `String` + mutex (still races with AsyncTCP); stream to LittleFS (overkill for settings JSON).

2. **Scratch buffer / chunked response for large JSON**  
   Introduce a shared helper that fills a reusable `char` buffer (host: prefer PSRAM via `heap_caps_malloc` / existing PSRAM patterns; fall back internal) or writes through `AsyncResponseStream` / chunked send. Reuse one builder for host proxy and slave coordinator device lists so field order/set stay aligned. Keep MQTT `bridge/devices` on the same helper where it currently builds a full list string.  
   *Alternatives:* ArduinoJson `JsonDocument` in PSRAM (heavier dependency surface); keep `String` + `reserve` only (helps but still fragmented under growth).

3. **Logger overloads, level-first**  
   Change API to `const char*` and `const String&`; check `logLevel` before formatting/allocating. Call sites that build with `+` may keep a local `String` only when the level passes, or use a small stack/`snprintf` path for hot SPI lines over time.  
   *Alternatives:* macro wrappers (less consistent with existing class style).

4. **Wi‑Fi scan and MQTT connect off the critical loop**  
   - STA: use async scan (`WiFi.scanNetworks(true, …)`) + state machine in `WiFiController::update`, or run the blocking scan on a dedicated low-priority task that posts results back. Prefer async scan if the Arduino-ESP32 3.x API on S3 is reliable.  
   - MQTT: drive `client->connect` from a short worker task or split connect across `update()` with a “connecting” flag so `loop` still pumps SPI deferred work and HTTP. Preserve existing reconnect backoff.  
   *Alternatives:* pin SPI task to core 1 and accept loop stalls (does not meet the responsiveness requirement).

5. **Timed Zigbee lock**  
   Replace `portMAX_DELAY` on the cluster-command send path with a timeout in the same order as nearby paths (tens of ms, e.g. 50–200 ms). On failure: log, return send failure to host path, do not tear down the coordinator. Retry at the existing command-queue layer only if one already exists; do not invent unbounded spin.  
   *Alternatives:* move all ZCL sends onto the Zigbee task exclusively (larger refactor).

6. **README rewrite for ADR 0003 / board-role**  
   Replace GPIO15 / dual-C6 / single-binary wording with S3-N16R8 host + C6 slave, two PlatformIO envs, joined package for Update. Keep LED and SPI pin tables accurate to current `pins.h` (edit only role/flash narrative if pin tables already match).

## Risks / Trade-offs

- **[Risk] Async scan API differences on S3** → Mitigation: feature-test in implement; fall back to worker task with same external behavior.  
- **[Risk] PSRAM scratch used for JSON while DMA/SPI needs internal RAM** → Mitigation: JSON scratch only; SPI frames stay on existing internal buffers.  
- **[Risk] Per-request body raises peak heap under many parallel posts** → Mitigation: hard cap + reject; AsyncWebServer rarely has many large admin posts.  
- **[Risk] Timed lock increases rare ZCL send failures under load** → Mitigation: fail one command; MQTT/console can retry; better than wedging SPI drain.  
- **[Trade-off]** Logger API change touches many call sites → do overloads so existing `String` temporaries still compile via `const String&`.

## Migration Plan

1. Flash host and slave together after apply (behavior changes on both images).  
2. No EEPROM/schema migration.  
3. Rollback: prior firmware images; no settings format change.

## Open Questions

- Exact HTTP body max bytes per endpoint class (small JSON vs restore) — choose constants during apply and document in code; not a product UX change beyond “reject too large.”
