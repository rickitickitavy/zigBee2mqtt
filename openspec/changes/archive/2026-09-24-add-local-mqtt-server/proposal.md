## Why

The gateway today is only an MQTT **client**. Replacing Home Assistant as the LAN broker requires an optional onboard broker so devices, HA, and other clients can publish and subscribe on this host without a separate MQTT machine.

## What Changes

- Add an optional lightweight MQTT **server** on the host. It starts only when MQTT **SERVER TYPE** is **local**.
- **BREAKING:** Replace MQTT settings attribute **ENABLED** with **SERVER TYPE**: `disable`, `remote`, `local`. Default is **disable** (MQTT off). Stored `enabled=false` migrates to `disable`; stored `enabled=true` migrates to `remote`.
- When SERVER TYPE is **local**, the host starts the local broker, binds so **remote clients can connect** (not loopback-only), and the web UI **hides SERVER**. The host MQTT client still connects to that local broker (loopback) using PORT, CLIENT_ID, and credentials.
- Local broker authorization: if **USERNAME and PASSWORD are both non-empty**, CONNECT must authenticate with those credentials; if either is empty, anonymous CONNECT is allowed. The host client uses the same rule as today.
- While the local MQTT server is listening, the **master onboard RGB is held blue** (boot/critical red still outranks).
- Remote mode keeps today’s client-to-external-broker behavior (including STA required for that client). Disable starts neither client nor broker.
- Update ADRs: host duties now include an optional broker; MQTT CONNECT auth on the local broker is independent of the web-console session.
- Firmware build **0.2.18** (bumped at propose).

## Capabilities

### New Capabilities

- `mqtt-broker`: optional host MQTT 3.1.1 broker; start/stop from SERVER TYPE; listen for remote clients; optional user/password on CONNECT.

### Modified Capabilities

- `web-console`: MQTT card uses SERVER TYPE instead of ENABLED; hide SERVER when type is local; GET/POST `/api/mqtt` and settings export/restore use `serverType`.
- `status-rgb-led`: host RGB blue while the local broker is listening.

## Impact

- Host only: new broker module, `MqttSettings.enabled` → `serverType`, `MqttClient` / `main` start paths (local works with AP or STA; remote still needs STA), `StatusRgb`, `WebConsole` `/api/mqtt` and export/restore, `data/index.html` MQTT form, USB CLI `mqtten` → server type.
- `platformio.ini` gains a lightweight broker library.
- `docs/adr/0002`, `0005`, and a new ADR for the optional local broker.
- Slave, Zigbee topics, and console RBAC stay the same.
