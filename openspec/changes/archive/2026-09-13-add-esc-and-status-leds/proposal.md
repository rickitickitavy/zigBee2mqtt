## Why

Overlay dialogs on the web console ignore Escape, so operators must hunt for Cancel. Status on both DevKitC boards is almost invisible: the onboard RGB stays off after boot except a blue pairing blink on the slave, so it is hard to see whether bring-up finished, whether SPI is moving, or whether the slave is stuck in a fatal error.

## What Changes

- Escape dismisses the front-most visible overlay dialog on the web console (same outcome as that dialog’s Cancel/Close, never Save or Confirm delete).
- Host and slave turn the onboard RGB **red** as soon as that chip starts, and turn that boot-red off only when that chip’s own preparation is finished.
- Slave flashes **green** for 0.1 s when it receives a valid SPI frame from the host, and **red** for 0.1 s when it sends a valid SPI frame to the host.
- Slave holds **red** for the whole time a critical (fatal / unrecoverable) error is present.
- No **BREAKING** protocol, MQTT, or HTTP API changes.

## Capabilities

### New Capabilities

- `status-rgb-led`: Onboard WS2812 (`PIN_STATUS_RGB`) meaning for host boot-red, slave boot-red, slave SPI activity flashes, and slave critical-error red, including priority against the existing pairing blink.

### Modified Capabilities

- `web-console`: Overlay dialogs (search, parameters, restore-skipped, delete confirm) MUST close on Escape using the same dismiss path as Cancel/Close.

## Impact

- `data/index.html`: one document-level keydown handler for Escape; reuse existing close helpers.
- Host `setup` / Wi-Fi / SPI bring-up: hold then clear boot-red.
- Slave `setup`, SPI pump, Zigbee start, and fatal-error paths: boot-red, activity flashes, sticky critical red.
- Existing pairing blink on the slave (`ZigbeeCoordinator::updatePairingLed`) must share the same LED without fighting boot-red or critical-error red.
- No new HTTP routes, MQTT topics, or SPI commands.
