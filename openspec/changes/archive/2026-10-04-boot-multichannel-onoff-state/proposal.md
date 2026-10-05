# Proposal

## Why

After a reboot, a multi-channel On/Off relay shows only the first channel’s real state. A 4-channel relay that was ON on channels 1 and 3 comes back with channel 1 ON and channel 3 looking off, even though channel 3 is still ON. The operator needs every channel’s real state at boot.

## What Changes

- After the slave coordinator starts and the registered Zigbee list for this boot is in place, read On/Off on every configured channel endpoint of each registered `onOff` device, not only endpoint 1.
- Keep each answer on the endpoint that sent it, and show that value on the matching status circle.
- Do not finish the boot status pass on the first device record if later records for this boot have not been applied yet.
- Send those reads so a later endpoint is not dropped when several endpoints of the same device are requested together.
- Leave Wi-Fi devices, single-channel devices, and non-On/Off types on their current boot behavior.

## Capabilities

### New Capabilities

- None.

### Modified Capabilities

- `zigbee-slave-radio`: Boot status reads must return a separate On/Off value for every configured channel of a registered `onOff` device, including channels 1 and 3 of a 4-channel relay.
- `web-console`: After that boot read, the Devices table status cell shows each channel’s own ON or OFF, including a light circle for channel 3 when that endpoint answered ON.

## Impact

- Slave boot status refresh in `ZigbeeCoordinator` (`enqueueRegisteredStatusReads`, one-shot `statusRefreshStarted`, per-endpoint read queue).
- Host status cache and Devices table already store and draw per-endpoint On/Off; they must show the boot replies without collapsing them onto endpoint 1.
- Firmware `FIRMWARE_VERSION` build is `0.4.2` for this proposal. Flash the slave with the host so the boot reads run on the radio chip.
- Assumption: a 4-channel relay is a registered `onOff` device with `channels` `4`, and channel N is Zigbee endpoint N. Parse mode (`channels` `0`) is out of this fix unless that device is also missing endpoint 3 at boot for the same read-drop reason.
