## 0. Version

- [x] 0.1 Confirm `FIRMWARE_VERSION` is `0.2.18` in `Defines.h` (bumped at propose)

## 1. Settings and APIs

- [x] 1.1 Replace `MqttSettings.enabled` with `MqttServerType` (`0` disable, `1` remote, `2` local) in the same byte, default factory `disable`, clamp unknown bytes to disable, keep EEPROM `0`/`1` meaning, and verify a wiped EEPROM then GET `/api/mqtt` returns `"serverType":"disable"`
- [x] 1.2 Change GET/POST `/api/mqtt` to `serverType` strings, drop required `enabled`, allow POST without `server` when type is `local`, and verify POST `local` then GET shows `local` and a `remote` POST still requires/stores server
- [x] 1.3 Include `mqtt.serverType` in settings export and map restore `enabled` true/false when `serverType` is absent, and verify an old export with `"enabled":true` restores as `remote`
- [x] 1.4 Add USB CLI `mqtttype disable|remote|local` (keep `mqtten` as remote/disable aliases) and verify `mqtttype local` then save persists local

## 2. Broker and client

- [x] 2.1 Add a lightweight ESP32 MQTT 3.1.1 broker to `lib_deps` (PicoMQTT first) that binds `0.0.0.0:port` with a small client cap, and verify `pio run` links it on the host env
- [x] 2.2 Start the broker only when SERVER TYPE is `local` and AP or STA has an IPv4 address, stop it for disable/remote, and verify a LAN MQTT client can open TCP to the host IP:PORT in local and cannot in remote
- [x] 2.3 Require CONNECT user/password only when both stored fields are non-empty, and verify wrong password is rejected and empty username accepts anonymous CONNECT
- [x] 2.4 Point `MqttClient` at `127.0.0.1:port` in local (no STA required) and keep STA + stored server in remote, and verify host publishes device state on the local broker and remote mode still needs STA

## 3. LED and UI

- [x] 3.1 Add `StatusRgb::setMqttBrokerListening` so host RGB is blue while the broker listens (outranks green; red still wins) and LED3 stays off, and verify ready+local is blue and ready+remote+connected is green
- [x] 3.2 Replace MQTT ENABLED with a SERVER TYPE select (`disable`/`remote`/`local`) and hide SERVER when local, and verify the card never shows ENABLED and SERVER disappears as soon as local is selected
- [x] 3.3 Trace `serverType` through MQTT GET/POST, export/restore, reboot, and the MQTT card (no leftover `enabled` on those surfaces), and verify after save+reboot the select still shows the stored type

## 4. ADRs

- [x] 4.1 Amend `docs/adr/0002` so the host may run an optional MQTT broker when SERVER TYPE is local, and verify the file states the broker is host-only
- [x] 4.2 Amend `docs/adr/0005` so MQTT still needs no console session and the local broker may require CONNECT credentials from settings, and verify the file says that
- [x] 4.3 Add `docs/adr/0009-local-mqtt-broker.md` (optional HA replacement, remote bind, fill-or-not auth, blue LED, LAN exposure if credentials empty), and verify the file exists with those decisions

## 5. Build

- [x] 5.1 `pio run` succeeds for host and slave with `0.2.18`
