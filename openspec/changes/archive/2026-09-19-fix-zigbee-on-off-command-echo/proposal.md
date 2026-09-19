# Proposal

## Why

After a few rapid ON then OFF cycles, the target device keeps switching as if it is still receiving those commands, then the extra switching fades. The slave already fires every queued on/off (and write-attribute) into the Zigbee stack immediately, so in-flight APS retries of earlier ON and OFF overlap and look like a fading network echo. Operators cannot stop the device with a last command while old ones are still in the air.

## What Changes

- Keep only the **latest** pending on/off or write-attribute for a given IEEE + endpoint on the host SPI queue and on the slave deferred radio slot, instead of stacking a burst of opposite commands.
- On the slave radio, send at most **one** in-flight ZCL command to that destination at a time; wait for completion or a short timeout, then send only the latest queued command.
- Do **not** change MQTT topic mapping, FULL CONTROL parse, or LED meanings. LED4 still pulses only when a command is actually transmitted, not for dropped superseded commands.
- No **BREAKING** protocol change: same SPI command ids and payloads.

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `zigbee-slave-radio`: The coordinator MUST NOT leave overlapping on/off (or write-attribute) transactions in flight to the same IEEE + endpoint; a later command MUST replace earlier ones that have not completed.
- `host-slave-spi`: Pending host-to-slave device control frames for the same IEEE + endpoint MUST collapse to the latest command so the radio never receives a queue of opposite ON/OFF.

## Impact

- Host: `InterChipHost` outbound queue (`kOutQueue` is 8) when enqueueing `SpiCmdZclOnOff` and `SpiCmdZclWriteAttr`.
- Slave: `InterChipSlave` single deferred on/off and write-attribute slots; `ZigbeeCoordinator::controlOnOff` / `writeAttribute` and Arduino `ZigbeeSwitch::lightOn` / `lightOff` / `lightToggle` (`esp_zb_zcl_on_off_cmd_req`).
- Unchanged: MQTT subscribe/publish, web console, LED pin map, SPI frame layout.
- Assumption: the echo is overlapping radio sends (and stacked SPI commands), not Home Assistant republishing onto the command topic. If MQTT itself keeps sending after the operator stops, that is out of this change.
