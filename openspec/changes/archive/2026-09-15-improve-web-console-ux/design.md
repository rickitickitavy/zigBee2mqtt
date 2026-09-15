## Context

See proposal.md for motivation. Today NAME only auto-fills topics when `topicsTouched` is false (edit sets it true). Search already waits for a click to POST `/api/devices/search`, but the button stays busy until dialog close; the slave join window (`permitJoinOnBootSec`) ends without a host event. ATTR_REPORT has IEEE, endpoint, NWK, message — no RSSI. Status is an empty section. System tabs default to Update. Slave store JSON is GET `/api/devices/store`. Version is GET `/api/version`. Online is already on GET `/api/devices`. Packet counters do not exist.

## Goals / Non-Goals

**Goals:**
- Console UX listed in the proposal, with Search bound to slave join-window state
- Last RSSI from inbound Zigbee packets on the host table
- Host Status JSON for counts and packet totals since boot

**Non-Goals:**
- Persisting RSSI or packet counters across reboot
- Changing one-record device CRUD or bulk overwrite of the slave store
- LQI, or RSSI from outbound command ACKs (last *received* packet only)

## Decisions

1. **Topic rewrite always follows NAME**  
   Drop the “topics already touched” skip while NAME is being edited. Each NAME input rebuilds the three default topics from MQTT `baseTopic` + slug. Alternative: replace only the old slug substring — more surprising if topics were customized.

2. **Join-closed SPI event**  
   Slave already knows `pairingUntilMs`. When the window ends (or `closeJoin`), enqueue a small SPI event (new cmd or a status bit). Host `pairingActive` drives Search disabled. Alternative: host timer equal to permit-join seconds — drifts from the slave.

3. **RSSI on ATTR_REPORT**  
   Append signed RSSI (int8 dBm) to the existing report payload (after NWK, before or after message — keep message parse compatible by placing RSSI in a fixed header slot). Host cache per IEEE on the same path as `noteSeen` / online. Join frames MAY also update RSSI if the stack supplies it; table still requires at least one inbound packet.

4. **Packet counters**  
   Host increments *received* on inbound Zigbee SPI events (ATTR_REPORT, JOIN, LEAVE). Host increments *sent* when it successfully enqueues ZCL on/off or write-attribute. GET `/api/status` returns `{devices, online, packetsRx, packetsTx, version, pairingActive}`. Alternative: count only MQTT — weaker for radio.

5. **Export / Restore on Devices**  
   Export: download GET `/api/devices/store` as `devices.json`. Restore: reuse the existing restore function (sequential POST). System devices.json card stays.

6. **System tab order**  
   DOM and default active class: Log, devices.json, Update, Hardware.

## Risks / Trade-offs

- [ATTR_REPORT length change] → Host and slave must both understand the extra RSSI byte; old host without it would mis-parse message text. Flash both chips together.
- [Join-closed missed on SPI drop] → Closing the dialog still POSTs search/stop; Search can also re-enable if pairingActive is false on `/api/status` poll.
- [NAME rewrite overwrites custom topics] → Matches the requested NAME→topics behavior; operator can edit topics after the last NAME keystroke.

## Migration Plan

Flash host and slave together. No settings migration. Packet counters and RSSI start empty until the next packets.

## Open Questions

None that block the specs or task split.
