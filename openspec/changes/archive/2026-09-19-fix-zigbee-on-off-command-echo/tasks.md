# Tasks

## 1. Host SPI coalesce

- [x] 1.1 In `InterChipHost::tryEnqueue` / `enqueueInternal`, when the command is `SpiCmdZclOnOff` or `SpiCmdZclWriteAttr`, replace an unused queued device-control frame with the same IEEE (payload 0–7) and endpoint (payload 8) instead of occupying a new slot, and verify a second enqueue for that dest does not grow used slots
- [x] 1.2 Leave permit-join, settings, ping, and other cmds as a normal FIFO, and verify two different IEEE values still both stay queued

## 2. Slave deferred latest-wins

- [x] 2.1 Replace the single global on/off and write-attribute deferred flags with per-IEEE+endpoint latest-pending records that share one slot when on/off and write-attribute target the same dest, and verify a write then an on/off before `applyDeferredRadioCommands` results in only the on/off being applied
- [x] 2.2 Keep independent deferred records for different endpoints, and verify endpoint 1 OFF and endpoint 3 ON both still get applied

## 3. Radio one-in-flight

- [x] 3.1 In `ZigbeeCoordinator`, mark a dest in flight when `lightOn` / `lightOff` / `lightToggle` or write-attribute is actually issued, stash/replace a next command if that dest is already in flight, and verify a second `controlOnOff` for the same dest does not call the switch helper until the first completes
- [x] 3.2 On ZCL default response for the in-flight dest (endpoint + On/Off or write cluster) or after 1500 ms with no ACK, clear in-flight, send next if present, pulse LED4 only on the actual send, and verify LED4 does not pulse for a superseded command that never went on air
- [x] 3.3 Keep IEEE unicast addressing (do not switch to bind-table `DST_ADDR_ENDP_NOT_PRESENT`), and verify `controlOnOff` still targets the requested endpoint

## 4. Device check

- [x] 4.1 Flash host and slave together, send ON/OFF about 3–6 times quickly to one registered device, then stop, and verify the device settles on the last command and does not keep randomly switching
- [x] 4.2 Repeat a slower ON then wait then OFF, and verify each command still switches the device once with LED4 on send and LED3 on ACK when the device answers
