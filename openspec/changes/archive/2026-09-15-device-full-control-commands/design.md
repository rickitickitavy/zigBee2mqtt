## Context

See proposal.md for why. Today MQTT commands after channels mapping always call `ZigbeeSpiProxy::controlOnOff`, which enqueues `SpiCmdZclOnOff` (IEEE + endpoint + text). The slave defers that to `ZigbeeCoordinator::controlOnOff`, which only maps `on` / `off` / `toggle` (and aliases) to switch commands; other text is logged and dropped. Inbound non-on/off reports already publish bodies like `cl=0x0006,attr=0x0000,val=0x1`. `DeviceTopicEntry` has IEEE, name, three topics, `channelCount`, and `used`, stored as JSON on LittleFS. The Devices parameter dialog already has CHANNELS plus topic fields. SPI device-sync does not need product flags; the slave must not apply MQTT or UI rules.

## Goals / Non-Goals

**Goals:**
- Persist FULL CONTROL on the host device record and expose it on the parameter dialog and `config/device`
- Parse report-shaped command bodies on the host when the flag is on, filling absent attributes with defaults
- If no attributes parse, send the whole remaining body as the command payload
- Send a structured write-attribute over SPI when at least one attribute parsed
- Keep channels mapping (`ch-<ep>##` and topic suffix) in front of full-control parse

**Non-Goals:**
- JSON command payloads (`{"cl":6,...}`) in this change
- Cluster commands other than write-attribute (`cmd=` cluster command id)
- Changing inbound report formatting
- Syncing FULL CONTROL to the slave device map
- Home Assistant discovery or typed MQTT schemas

## Decisions

1. **Host parses; slave writes**  
   FULL CONTROL stays on `DeviceTopicEntry` on the host. The host parses keys and enqueues a new SPI command. The slave only runs ZCL write-attribute.  
   Alternative: reuse `SpiCmdZclOnOff` text and parse on the slave — rejected; that puts product grammar on the radio chip.

2. **New SPI command `SpiCmdZclWriteAttr` (0x0D)**  
   Payload: `ieee[8] + endpoint[1] + cluster[2 LE] + attr[2 LE] + type[1] + value[4 LE]` (18 bytes). Keep `SpiCmdZclOnOff` for non-full-control devices.  
   Alternative: overload on/off frames with a type byte — rejected; mixed lengths and meaning.

3. **Payload grammar matches inbound reports; absent keys get defaults**  
   After channels unwrap, split on commas, then `key=value`. Keys are case-insensitive. At least one recognized key (`cl`, `attr`, `val`, `ep`, `type`) means write-attribute. Fill omissions: `cl` `0x0006`, `attr` `0x0000`, `val` `0`, `ep` from channels mapping, `type` U8 (or U16/U32 from `val` width). `ep` in the body overrides the mapped endpoint when usable.  
   Alternative: JSON-only — rejected; operators can paste the published state body.

4. **No recognized keys: whole body is the payload**  
   If the remaining body has no `key=value` for a recognized attribute, enqueue `SpiCmdZclOnOff` with that entire body as the command text and the mapped endpoint (same as FULL CONTROL off). Do not treat the body as `val=0`.  
   Alternative: force a write-attribute with default `val` `0` — rejected; `ON`/`OFF`/`toggle` would become a zero write.

5. **Storage is JSON, not SPI sync**  
   Add `fullControl` bool to `DeviceTopicEntry`, `listJson` / `replaceFromJson` / upsert / web POST / MQTT config. Missing key means off. Do not extend `SPI_DEVICE_SYNC_ENTRY_LEN`.  
   Alternative: extra sync byte — unused on the slave.

6. **Web UI is a checkbox at the end of the dialog**  
   Label FULL CONTROL, same `field-check` pattern as MQTT enabled / OTG. Place it after AVAILABILITY (last field before the dialog buttons). Default unchecked. Load and save with the parameter POST body as `fullControl: true|false`.

## Risks / Trade-offs

- [Wrong `type` bricks or no-ops a write] → Default from value width; allow explicit `type=`.
- [Value wider than 4 bytes] → Reject and log; inbound reports already cap at 4 bytes.
- [Host-only flash] → Slave ignores unknown cmd; full-control commands fail until both chips match.
- [SPI message max 64 still unused for this path] → Structured 18-byte frame avoids the text cap.

## Migration Plan

1. Flash host and slave together.
2. Existing LittleFS device JSON without `fullControl` loads as off; on/off MQTT unchanged.
3. Operators enable FULL CONTROL per device. Plain `ON`/`OFF`/`toggle` still uses the command payload path. Attribute-shaped bodies (`cl=...`) use write-attribute with defaults for omitted keys.
4. Rollback: previous firmware ignores the JSON field; devices behave as on/off-only again.

## Open Questions

None. Default `cl`/`attr` for a partial parse are On/Off `0x0006` / `0x0000`. JSON payloads stay out of this change.
