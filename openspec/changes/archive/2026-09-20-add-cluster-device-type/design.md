# Design

## Context

See proposal.md for why. Joins today are `SpiEvtDeviceJoin` (75 bytes: IEEE, NWK, endpoint, manufacturer[32], model[32]). The found list and parameter dialog show those strings plus IEEE. `DeviceTopicEntry` has no type. The slave already learns endpoints via On/Off match/bind and IAS callbacks, but it does not read a simple descriptor for classification and does not send a type.

`SpiCmdSetDevice` / device dump SHALL carry the persisted type as the last byte of the sync entry. Readonly in the UI does not mean omit the field from JSON or SPI: type is a stored device attribute, not live telemetry.

## Goals / Non-Goals

**Goals:**

- Classify one type per IEEE from in-clusters, carry it on join, persist it on the host, show it in Search and as a readonly parameter.
- Keep SPI device-map sync and MQTT command behavior unchanged.

**Non-Goals:**

- Type-specific MQTT payloads, HA discovery, or radio command routing.
- Storing the full cluster list in EEPROM.
- Per-endpoint types (one type per IEEE).
- Expanding classification beyond IAS Zone, Window Covering, and On/Off in this change.

## Decisions

1. **Stable ids, display labels in the UI**  
   Persist and API-encode `unknown` | `onOff` | `iasZone` | `windowCovering`. SPI uses one byte: `0`/`1`/`2`/`3`. Console maps to Unknown / On/Off / IAS Zone / Window covering.  
   Alternative: store HA device id (`0x0100` on/off light, etc.) — rejected; clusters are what the radio advertised and what future actions will switch on.

2. **Priority, not a cluster bitmask**  
   First match: `0x0500` → `iasZone`, `0x0102` → `windowCovering`, `0x0006` → `onOff`, else `unknown`. A 4-gang switch with only On/Off is `onOff`. A Tuya leak sensor that also has On/Off is `iasZone`.  
   Alternative: multiple types per device — rejected; settings need one readonly value.

3. **Simple descriptor, then one join (retry if late)**  
   After a usable IEEE + endpoint is known, request simple descriptor(s) for discovered endpoints (active-endpoint scan when the stack provides it; otherwise the bind/IAS endpoint we already have). Wait up to a short timeout (e.g. 2 s) then send `SpiEvtDeviceJoin` with the best type so far. If a later descriptor upgrades `unknown` to a known type, send another join for the same IEEE; the found list already overwrites by IEEE.  
   Alternative: classify only from On/Off bind success vs IAS enroll — rejected; that misses window covering and mis-labels IAS devices that also bind On/Off.

4. **Join payload: append type at offset 75**  
   New length 76. Host: if `length >= 76`, read type; else `unknown`. Manufacturer/model offsets stay 11 and 43.  
   Alternative: new SPI event — rejected; pairing already keys off `SpiEvtDeviceJoin`.

5. **Host persistence on `DeviceTopicEntry`**  
   Add `uint8_t zigbeeType`. LittleFS `devices.json` / `listJson` include `"type":"onOff"`. `POST /api/devices` copies type from the found slot on first save of that IEEE; later POSTs ignore a client `type` field when stored type is not `unknown`. If the IEEE is already registered as `unknown` and a join arrives with a real type, update and save without putting it on the found list (found list still skips registered IEEEs).  
   Alternative: put type in the 256-byte main reserved tail — rejected; it belongs with the device record.

6. **Readonly UI**  
   Parameter dialog: `<input readonly>` TYPE, filled from found JSON on Add and from `/api/devices` on edit. Found table: Type column. Devices list table is unchanged.  
   Alternative: hidden field only — rejected; the operator must see the type while pairing.

## Risks / Trade-offs

- [Simple descriptor delayed past join] → Timeout still sends `unknown`; a second join upgrades found/registered `unknown`.
- [Old host + new slave (76-byte join)] → Extra byte ignored if host only parses 75; type lost until both flashed. Document flash together.
- [Old slave + new host] → length 75 → `unknown`; re-pair after flash.
- [Wrong priority for a weird combo device] → Extend the table in a later change; do not let the operator override.

## Migration Plan

Flash host and slave together. Existing `devices.json` without `type` loads as `unknown`. Rollback: previous firmware ignores extra join byte and extra JSON key.

## Open Questions

None. Adding more cluster→type rows later does not change this change’s SPI layout or JSON key.
