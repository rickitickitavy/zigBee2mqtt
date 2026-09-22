# Proposal

## Why

System → Update programs only the host. The Zigbee slave has no HTTP stack, so operators cannot refresh it from the console and USB-flash both boards after every `.bin`. One firmware image already runs on both chips (role pin); the missing piece is delivering that image to the slave before the host reboots into it.

Yes, this is possible: SPI frames are 256-byte payloads, so the host MUST chunk the image. RAM cannot hold a ~1.6 MB `.bin`. LittleFS on the 16 MB partition can.

## What Changes

- Firmware upload on the host SHALL stage the `.bin`, program the **slave** over SPI first, and only then program and restart the **host**.
- If the slave transfer or slave OTA commit fails, the host SHALL keep its running application, SHALL NOT restart, and the Update tab SHALL show the failure.
- The Update tab SHALL show progress after HTTP upload (slave then host), not only the browser file-upload bar.
- The displayed firmware version SHALL move from `0.1.0` to `0.2.0` (minor bump) with this image.
- LittleFS (`/update/data`) SHALL stay host-only.
- **BREAKING** for mixed firmware: a host with this protocol and a slave without it cannot complete firmware Update until the slave is USB-flashed once with matching firmware.

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `web-console`: Firmware Update programs the slave first, then the host; UI reports that sequence; filesystem upload unchanged.
- `host-slave-spi`: Host-to-slave framed firmware OTA (begin/chunk/end, async, integrity); slave writes its inactive app slot and restarts after a successful commit.

## Impact

- `WebConsole` `/update` (not `/update/data`), Update tab JS, LittleFS staging file, `FIRMWARE_VERSION` `0.1.0` → `0.2.0`.
- `SpiProtocol`, `InterChipHost` / `InterChipSlave`, slave `Update` OTA, host restart only after slave success.
- First roll-out still needs USB flash of **both** chips so the slave understands the new commands.
