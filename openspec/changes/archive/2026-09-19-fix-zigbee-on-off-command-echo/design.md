# Design

## Context

See proposal.md for why. Specs add latest-command-wins on SPI and one in-flight ZCL command per IEEE + endpoint on the slave radio.

Today the host can hold up to eight outbound SPI frames (`InterChipHost::kOutQueue`). `SpiCmdZclOnOff` and `SpiCmdZclWriteAttr` do not wait for a reply (`commandExpectsReply` is false), so a burst of MQTT ON/OFF all sit in that queue and are clocked to the slave a few milliseconds apart. The slave copies each into a **single** global deferred on/off or write slot (`onOffPending` / `writeAttrPending`) and `applyDeferredRadioCommands` in `loop` calls `ZigbeeCoordinator::controlOnOff` / `writeAttribute`, which immediately call `zigbeeSwitch.lightOn` / `lightOff` / `lightToggle` or `esp_zb_zcl_write_attr_cmd_req`. Arduino `ZigbeeSwitch` releases its command lock right after queueing the ZCL request, so the stack can keep several APS retries of ON and OFF in the air. `onCoordinatorDefaultResponse` only pulses LED3; it does not gate the next send.

The IEEE-addressed `lightOn(endpoint, ieee)` path is already unicast (not bind-table `DST_ADDR_ENDP_NOT_PRESENT`). Binding fan-out is not the observed echo.

## Goals / Non-Goals

**Goals:**

- Collapse pending device-control frames per IEEE + endpoint on the host before they leave for the slave.
- On the slave, one in-flight on/off or write-attribute per IEEE + endpoint, with a next-pending slot that a newer command overwrites.
- Pulse LED4 only when `lightOn` / `lightOff` / `lightToggle` / write-attr actually goes to the stack.

**Non-Goals:**

- Changing SPI command ids or payload layout.
- MQTT debounce, retained-command topics, or Home Assistant automations.
- Serializing permit-join, settings, or device-map SPI.
- Turning off ZCL default responses (LED3 still means ACK).
- A global single-flight lock across all devices (would stall gang 3 while gang 1 is in flight).

## Decisions

1. **Coalesce on the host outbound queue**  
   When enqueueing `SpiCmdZclOnOff` or `SpiCmdZclWriteAttr`, if an unused queued frame is already a device-control command for the same IEEE (payload bytes 0–7) and endpoint (byte 8), replace that frame’s command, length, and payload in place. Do not add a second slot. A frame already in `hasPending` transfer is not rewritten.  
   Alternative: coalesce only on the slave — rejected as the only fix; eight stacked frames still hit the radio 5 ms apart.

2. **Coordinator owns per-destination flight, not SPI defer flags**  
   Keep SPI deferral so Zigbee is not called from the SPI task. Change the deferred slots so on/off and write-attribute for the **same** IEEE + endpoint share one “latest deferred” record (a write then an on/off before `loop` runs becomes only on/off). Different destinations need **separate** deferred records (small table, bound to `kMaxBoundDevices` or a handful of slots). `ZigbeeCoordinator` then: if that dest is in flight, store/replace `next` and return without calling `lightOn`; if idle, send and mark in flight.  
   Alternative: one global in-flight for the whole coordinator — rejected; multi-endpoint devices would serialize.

3. **Completion is default-response or timeout**  
   Treat ZCL default response for On/Off or the write cluster, matching the in-flight dest endpoint, as completion. The Arduino global callback does not pass IEEE; track the in-flight IEEE + endpoint we last sent and match on endpoint + cluster, or ignore unmatched ACKs. If no ACK within **1500 ms**, clear in-flight and send `next` if any (covers sleepy or missing default response).  
   Alternative: disable default response and only use a timer — rejected; we already use ACK for LED3 and it is a better completion than a blind delay.

4. **Do not switch to bind-table addressing**  
   Keep IEEE + endpoint unicast. Bind-table / group send would multiply the echo.

## Risks / Trade-offs

- [Default-response callback cannot name IEEE] → Match the dest we marked in flight; ignore extra ACKs; timeout still releases the slot.
- [1500 ms timeout feels laggy on a lost ACK] → Next command still sends after timeout; better than unbounded echo.
- [Host rewrite of a queued frame keeps the old seq] → Fine; slave does not key device control on seq.
- [MQTT still flooding the command topic] → Out of scope; host coalesce only helps while frames are still queued.

## Migration Plan

Flash host and slave together so both coalesce. Rollback is the previous image. No EEPROM or protocol version bump.

## Open Questions

None. Timeout 1500 ms is a recorded default; it can be tuned in code without changing the specs if field devices are slower.
