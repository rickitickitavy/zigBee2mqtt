## Context

See proposal.md for why and the delta specs for behavior. Today `GET /api/devices` includes IEEE, name, topics, `channels`, `fullControl`, `type`, `online`, and last RSSI from `ZigbeeSpiProxy` last-seen. The Devices table columns are online, IEEE, name, type, RSSI. Row click selects; double-click opens edit. Inbound `SpiEvtAttrReport` already carries `ep` and message text; On/Off becomes `ON`/`OFF`, Power Configuration `0x0021` becomes `BATTERY <percent>` (raw remaining / 2). There is no host cache of per-endpoint status or battery, no HTTP command path, and no ZCL read on the SPI link. The slave starts Zigbee in `ZigbeeCoordinator::begin` after host `SET_SETTINGS` and later calls `markRegistryReady` when the registered dump is applied.

## Goals / Non-Goals

**Goals:**
- Cache last per-`ep` status and last battery on the host as RAM telemetry (same lifetime idea as RSSI / online)
- Expose that cache on list JSON and render the new table columns
- Reuse the existing MQTT `set` apply function for Manual command
- Add a slave-side ZCL read and a paced start-time refresh of registered devices

**Non-Goals:**
- Persisting battery or status in LittleFS, EEPROM, or `devices.json`
- Home Assistant discovery or new MQTT topic names
- Changing how `BATTERY <percent>` is published on the state topic
- Configuring ZCL reporting intervals
- A second command grammar besides MQTT `set` / FULL CONTROL

## Decisions

1. **Host RAM cache, not device records**  
   Extend the existing per-IEEE last-seen slot (`ZigbeeSpiProxy` cache) with last battery percent (optional) and last status text per usable endpoint (up to 16). `listJson` gains optional `battery` and `status: [{ep, state}]`. `listStoreJson` / export / restore stay settings-only.  
   Alternative: persist last state — rejected; it is telemetry, same class as RSSI.

2. **Battery vs status split on message text**  
   Host treats inbound `BATTERY <n>` as battery only. Every other report text (`ON`, `OFF`, `LEAK`, `DRY`, covering text, attr dump) updates `status` for that `ep`.  
   Alternative: cache by cluster id on SPI — rejected; the report frame is already text + `ep`.

3. **New SPI command `SpiCmdZclReadAttr` (0x0F)**  
   Payload: `ieee[8] + endpoint[1] + cluster[2 LE] + attr[2 LE]` (13 bytes). Slave sends ZCL Read Attributes. Responses reuse `handleAttributeReport` / IAS handlers and `SpiEvtAttrReport`. Next free command id after OTA `0x0E` (`0x10` is already `SpiCmdReadEvent`).  
   Alternative: host-only refresh via existing write — rejected; write changes state.

4. **Slave starts the refresh after coordinator + registry**  
   When `started && registryReady` first become true, enqueue reads for each registered IEEE: type-specific attribute on each channel endpoint, plus `0x0001` / `0x0021` if that IEEE advertised Power Configuration during interview (or always attempt battery read once on endpoint 1 and ignore failures). Channel endpoints: `channels` `1` → ep 1; `2`–`16` → ep `1..N`; `0` → every usable endpoint already known from bind / simple descriptor. Pace through the existing one-in-flight-per-destination rule; do not block coordinator start on answers.  
   Alternative: host walks the map and enqueues reads — possible later; the spec assigns this to the slave so it can use radio-side endpoint knowledge.

5. **Type-specific read attributes**  
   `onOff`: cluster `0x0006` attr `0x0000`. `iasZone`: cluster `0x0500` Zone Status (`0x0002`), decoded as today (`LEAK`/`DRY` or equivalent). `windowCovering`: cluster `0x0102` Current Position Lift Percentage (`0x0008`) as text. `unknown`: skip type read; still try battery if cluster `0x0001` is known.

6. **Manual command is HTTP into the MQTT apply path**  
   `POST /api/devices/command` with JSON `ieee`, `payload`, and optional `channel` (1–16). Host builds the same topic/body the MQTT callback would see: suffix → apply as `commandTopic/channel`; parse → apply as `ch-<channel>##payload` on `commandTopic`; single → `commandTopic` + payload. Extract the current `main.cpp` command-topic branch into one function used by MQTT and HTTP. 404 if IEEE is unknown; 400 if body empty. Broker MAY be down.  
   Alternative: browser publishes MQTT — rejected; console must work in AP without a broker.

7. **Answers pane is polled list telemetry**  
   While the dialog is open, the existing 2 s Devices poll (or a slightly faster poll) appends new `status`/`battery` strings for that IEEE to the read-only pane. Sending also appends a `>` line with the typed body. No new websocket.  
   Alternative: stream `/api/log` — noisier and not device-scoped.

8. **Table click opens edit**  
   Replace `ondblclick` with the same handler on click: select IEEE, then `openEditSelectedDevice()`. Footer Edit / Delete / Manual command still use `selectedRegisteredIeee`. Operator can Cancel the dialog and keep the selection for Manual command.  
   Alternative: click selects only — rejected; the request replaces double-click.

9. **Parse-mode status cells**  
   `channels` `0`: render one indicator per `status[]` entry, sorted by `ep` (gaps allowed). `channels` `2`–`16`: render exactly N placeholders; fill from `status` where `ep` matches the channel number.

## Risks / Trade-offs

- [Start-time read storm] → Pace with existing per-IEEE/endpoint in-flight replacement; skip devices with no short address yet; retry is not required in this change.
- [Sleepy end devices ignore reads] → Table stays `N/A` / empty until they report; start still succeeds.
- [Single-click always opens edit] → Extra Cancel when the operator only wanted to select for Delete or Manual command; selection remains after Cancel.
- [Host/slave version skew] → Old slave ignores `0x0F`; table still updates from unsolicited reports.
- [Battery published on MQTT state] → Unchanged; operators already see `BATTERY n` on the state topic.

## Migration Plan

1. Flash slave then host (or the existing OTA order) so `0x0F` is understood before the host relies on start reads.
2. Existing device JSON unchanged; first boot after upgrade shows `N/A` / empty status until reports or start reads land.
3. Rollback: previous firmware ignores extra list JSON keys and the new HTTP route; table loses the new columns only after a UI rollback.

## Open Questions

None. IAS and window-covering read attribute ids are fixed above. Parse-mode table layout uses reported endpoints only.
