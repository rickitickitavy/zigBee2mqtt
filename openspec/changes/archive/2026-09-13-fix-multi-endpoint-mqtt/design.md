## Context

See proposal.md for why. Attr reports already carry endpoint on SPI (`payload[8]`); the host `onLightState` callback receives it and ignores it, then `publishDeviceState` uses `entry->stateTopic` alone. Command subscribe is exact `commandTopic`. `SpiCmdZclOnOff` is 9 bytes (IEEE + action). The slave `controlOnOff` uses `BoundZigbeeDevice.endpoint` (one slot per IEEE, last bind/report wins).

Stored `DeviceTopicEntry` topics stay prefixes. Channel id is the Zigbee endpoint as decimal (same as slave `ep=`).

## Goals / Non-Goals

**Goals:**
- Publish and command per endpoint via `{prefix}/{ep}`
- Leave availability prefix unchanged
- Carry endpoint on SPI on/off so the slave hits the right gang
- Keep one registry row per IEEE

**Non-Goals:**
- Separate UI/map slots per gang
- JSON state payloads or Home Assistant discovery
- Changing how availability is published if it is not published today
- Renaming stored topics in LittleFS / EEPROM

## Decisions

1. **Suffix at runtime, not in saved settings**  
   Settings keep `z2m/switcher_1/state`. The host builds `z2m/switcher_1/state/3`.  
   Alternative: four stored topic triples per device — rejected; 128-slot map and dialog stay IEEE-scoped.

2. **Subscribe `{commandTopic}/+` once per device**  
   One wildcard per registered command prefix (still ≤128 device subscriptions). Parse the last path segment as `1`–`254`. Exact `commandTopic` is not subscribed.  
   Alternative: subscribe each seen ep — more churn and missed first command before a report.

3. **SPI on/off payload becomes 10 bytes**  
   `ieee[8] + action + endpoint`. Old 9-byte frames are invalid (`length >= 10`). Host and slave must flash together.  
   Alternative: slave keeps last-report endpoint — rejected; MQTT would still not choose the gang.

4. **Always suffix, including `ep=1`**  
   Matches the spec and avoids a special case that hides a gang on a 4-line switch. Consumers must move to `…/state/1`.

## Risks / Trade-offs

- [Existing MQTT automations on unsuffixed topics go silent] → Document the suffix; no silent fallback publish.
- [Command topic already 63 chars; `/{ep}` is built in a `String`] → Fail the publish/subscribe with a log if the built topic is empty or the prefix is empty.
- [Host-only or slave-only flash] → Commands would be dropped (`length >= 10`). Flash both.
- [Wildcard `+` matches any last segment] → Reject non-integer / out-of-range and log.

## Migration Plan

1. Flash **both** host and slave with the same firmware.
2. Leave device settings as they are (prefixes).
3. Point MQTT clients at `{state}/1`…`{state}/N` and `{command}/N`.
4. Rollback: flash previous firmware; clients return to unsuffixed topics.

## Open Questions

None. Availability publish remains whatever the host already does, on the unsuffixed topic.
