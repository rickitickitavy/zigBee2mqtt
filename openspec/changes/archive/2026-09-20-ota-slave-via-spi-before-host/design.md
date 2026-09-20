# Design

## Context

See proposal.md for motivation. Today `POST /update` streams into Arduino `Update` on the **host** and `requestRestart()` as soon as the HTTP body ends. The slave never sees that image. SPI payloads are 256 bytes (`SPI_MAX_PAYLOAD`); host outbound queue is 8 frames; all SPI work is already asynchronous (`host-slave-spi`). The 16 MB map has ~8 MB LittleFS (`spiffs`) and 4 MB OTA slots — enough to stage a ~1.6 MB `firmware.bin`, not enough RAM to buffer it.

## Goals / Non-Goals

**Goals:**
- Stage the uploaded application image on the host without committing the host OTA slot until the slave has committed.
- Pump the image to the slave as one-in-flight SPI chunks; slave `Update` write; host OTA + restart only after slave success.
- Let the Update tab poll status after the HTTP POST returns so a minutes-long SPI copy does not depend on one open upload request.

**Non-Goals:**
- Pushing LittleFS (`/update/data`) to the slave.
- ArduinoOTA / USB `pio run -t upload` changes.
- Separate host vs slave binaries; one image, role pin, as today.
- Growing SPI payload size.

## Decisions

1. **Stage on LittleFS, then SPI, then host `Update`.**  
   HTTP `/update` writes a staging file (for example `/ota/firmware.bin`). Do not call host `Update.begin` during the upload. After the file is complete, a host loop/state machine streams it to the slave. Only after slave end+commit success does the host `Update` from that file and restart.  
   *Alternative:* Write the host OTA slot during HTTP, then read it back for SPI — rejected: `Update.end` would mark the new host slot bootable before the slave is done.  
   *Alternative:* SPI each HTTP chunk in the upload callback — rejected: blocks AsyncWebServer and violates async SPI.

2. **One in-flight OTA chunk.**  
   Reuse the file-chunk style (`SPI_FILE_FIRST` / data / `SPI_FILE_LAST`) on a new command id (next free after `SpiCmdZclWriteAttr`). Wait for `CMD_RESULT` (or timeout) before the next enqueue. Host queue depth is 8; filling it with 1.6 MB of chunks would stall Zigbee.  
   *Alternative:* Pipeline several chunks — possible later; first version stays one-in-flight.

3. **Slave commits and restarts before the host.**  
   Slave `Update.end(true)` then slave restart. Host then programs itself. Host boot still pulses slave EN, which is acceptable after the slave is already on the new image.  
   *Alternative:* Hold slave restart until host is also programmed — rejected: spec requires slave first.

4. **HTTP POST returns when the file is staged, not when both chips are done.**  
   Avoid browser/proxy timeouts on a multi-minute SPI copy. `GET` status (phase plus upload/slave/host percents) drives the Update tab after upload. Before host restart the page shows a 30 s reboot wait and pings `/api/version` until the router answers or the wait times out.  
   *Alternative:* Hold the POST until slave+host finish — rejected as fragile on STA.

5. **First deploy is USB both chips.**  
   Old slaves ignore unknown commands; mixed pairs fail firmware Update until the slave is flashed once.

6. **Firmware string `0.2.0`.**  
   Bump `FIRMWARE_VERSION` minor (`0.1.0` → `0.2.0`) in this image so Status and Update match the SPI-OTA behavior. Not an EEPROM settings version.

## Risks / Trade-offs

- [SPI OTA takes minutes] → One-in-flight chunks at existing SPI clock; UI polls; do not block `loop`.
- [Staging file fills LittleFS] → Reject upload if free space < image size; delete staging on success or failure.
- [Host dies after slave OTA, before host OTA] → Slave is new, host is old; operator retries Update (same image) or USB-flashes the host. Prefer this over a new host and an old slave.
- [Slave OTA write stalls Zigbee] → Slave queues SPI; apply `Update.write` off the SPI ISR, same as other commands.
- [HTTP OTA during SPI OTA] → Reject a second firmware upload while a transfer is active.

## Migration Plan

1. USB-flash host and slave together with the first build that includes SPI OTA (same as other protocol growth).
2. Later updates: System → Update firmware form only. Status and Update SHALL show `0.2.0`.
3. Rollback: USB previous image on both chips; old host firmware never starts SPI OTA.

## Open Questions

None. ArduinoOTA remains host-only until a later change.
