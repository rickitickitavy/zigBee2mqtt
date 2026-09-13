## 1. Escape on overlay dialogs

- [x] 1.1 Add a document `keydown` handler in `data/index.html` that dismisses the last visible `.dialog-overlay` via the existing close helpers, and verify Escape closes search (pairing stop), parameters, restore-skipped, and delete-confirm without Save or delete
- [x] 1.2 Verify Escape with no overlay open leaves the page unchanged (no navigation, no dialog toggle)

## 2. Status RGB helper

- [x] 2.1 Add `StatusRgb` (header + cpp) with critical > boot > pairing > 100 ms activity > off, colors red `(48,0,0)` / green `(0,48,0)` / pairing blue `(0,0,48)`, and verify `pio run` links the new files
- [x] 2.2 Call `StatusRgb::service()` from host and slave `loop`, and verify a 100 ms pulse expires back to the current layer without leaving a stuck color

## 3. Boot red

- [x] 3.1 Turn boot-red on at the start of `setupHost` and `setupSlave` (remove the current end-of-setup off), and verify both boards show red immediately after reset
- [x] 3.2 Latch host ready when Wi-Fi has SoftAP or STA and `INTER_CHIP_HOST.isNormal()`, then clear boot-red, and verify the host LED goes dark after AP/STA plus slave normal and stays off if STA later drops
- [x] 3.3 Clear slave boot-red only after settings apply and `ZigbeeCoordinator::isStarted()`, and verify the slave stays red until the coordinator is up

## 4. Slave activity and faults

- [x] 4.1 Pulse green/red from `InterChipSlave::serviceSpi` for application frames only (exclude ping/status/time-sync/`ReadEvent`/`SlaveReady`/`LOG_RECORD`), and verify a device command or report flashes 0.1 s while a poll/ping does not
- [x] 4.2 Gate pairing blink and activity on `StatusRgb` so boot-red and critical-error red win, and verify pairing blinks only after ready and never during critical red
- [x] 4.3 Set critical-error red on SPI slave init failure and `Zigbee.begin` failure, and verify the LED stays red and does not flash green while that flag is set
