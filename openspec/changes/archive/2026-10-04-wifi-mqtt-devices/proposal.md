# Proposal

## Why

The gateway today only registers Zigbee devices and publishes their state to MQTT. Operators also run Wi‑Fi devices that already publish their own state on MQTT; the console and birth/refresh handshake with Home Assistant (and similar) need those devices in the same registry, with the host listening instead of inventing Zigbee traffic.

## What Changes

- Add a persisted per-device **transport** attribute (`zigbee` | `wifi`), set at create time and readonly afterward. Existing and newly paired Zigbee devices are `zigbee`.
- Keep Zigbee register/search/command/publish behavior as today for `zigbee` devices.
- Add a **WIFI DEVICE** path on the Add-device search dialog that opens the usual parameter dialog with transport locked to `wifi`.
- For `wifi` devices, the host **subscribes** to their state topics (and channel variants) to learn runtime status; it does not publish Zigbee-originated state for them.
- For `wifi` devices, cluster **TYPE** (`unknown` / `onOff` / `iasZone` / `windowCovering`) is operator-editable on create and edit (only the operator knows the type).
- Extend MQTT settings with a **server-born topic** and a **born announce interval** (minutes, 5–120, default 20): listen for any message to republish/collect Zigbee states as at boot; after host boot wait 2 minutes then publish on that topic, and republish on the configured interval so Wi‑Fi devices (and servers) refresh status.
- Device parameter dialog shows transport before NAME; Zigbee keeps cluster TYPE readonly.

## Capabilities

### New Capabilities

- `wifi-mqtt-devices`: Host support for registered Wi‑Fi MQTT devices (listen to state, exclude from Zigbee slave sync/commands, operator-chosen cluster type).

### Modified Capabilities

- `device-registry`: Persist transport on each registered slot; Zigbee create path sets `zigbee`; Wi‑Fi create path sets `wifi`; migration of legacy records to `zigbee`.
- `mqtt-device-topics`: Subscribe to Wi‑Fi state topics for status; server-born topic listen/publish and periodic announce; Zigbee-only birth-triggered status collect.
- `web-console`: WIFI DEVICE button; transport field before NAME; editable cluster TYPE for Wi‑Fi; MQTT born-topic and interval fields.

## Impact

- Host: `DeviceTopicEntry` / settings version, `DeviceTopicMap`, `MqttClient` subscribe/publish paths, `main` MQTT dispatch, boot/birth timers, WebConsole `/api/mqtt` and `/api/devices`, LittleFS `devices.json` export/restore.
- Slave / SPI: Wi‑Fi records MUST NOT be upserted to the Zigbee slave registry; Zigbee sync stays Zigbee-only.
- Web UI: `data/index.html` Devices search/parameter dialogs and MQTT settings form.
- Firmware version build bumped to `0.3.7` during propose.
