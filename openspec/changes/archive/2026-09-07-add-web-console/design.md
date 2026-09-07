## Context

See proposal.md for motivation. The firmware is PlatformIO Arduino 3 (pioarduino) on ESP32-C6: `loop()` already pumps Wi-Fi, MQTT (`PubSubClient`), Zigbee, and the USB CLI. There is no HTTP stack today. Logs go only to USB CDC via `LOGGER`. Dual OTA app slots and a large LittleFS partition already exist.

Today `GlobalSettings.network` is `{ssid, password, hostName, wifiEnabled}`. Boot picks AP if BOOT is held, Wi-Fi is disabled, or SSID is empty; otherwise it starts STA and keeps reconnecting. SoftAP SSID is `hostName + "-AP"` with PSK `00000000`, then after 5 minutes AP stops and STA starts. That timeout path is replaced by the MODE / 5 s STA rules.

Specs: `specs/web-console/spec.md`. Field name **BSSID** is one network-name string for STA and AP (not a MAC address).

```
+-----------+     HTTP :80      +------------------+
|  Browser  | <---------------> | AsyncWebServer   |
+-----------+                   |  /  /api/*       |
                                +--------+---------+
                                         |
          +------------------------------+------------------------------+
          v                              v                              v
   LittleFS www                    Update (OTA)                   Logger ring
   tabs + WiFi form                inactive app slot              + USB CDC
          |
          v
   WifiSettings record  ----->  WiFiController
   (BSID, PASSWORD,             MODE STA --5s--> STA or AP
    DEVICE_NAME, MODE,          MODE AP  -------> AP
    OTG_ENABLED)
```

```
boot
  |
  v
unconfigured? --yes--> AP (name: DEVICE_NAME if BSID empty)
  |
  no
  v
MODE == AP? --yes--> AP (name: BSID)
  |
  no (STA)
  v
join BSID/PASSWORD for 5s
  |                    \
  ok                    fail
  v                     v
 STA                    AP (name: DEVICE_NAME)
```

## Goals / Non-Goals

**Goals:**

- Keep Zigbee and MQTT runnable during normal page loads (non-blocking HTTP).
- Persist settings as one C++ record per group; Wi-Fi is the first fully specified group.
- Implement MODE AP/STA, 5 s STA attempt, and AP naming from MODE.
- Ship tab chrome; fill WiFi + System in this change.

**Non-Goals:**

- Filling Status / ZigBee / Devices.
- HTTP auth, TLS, or a reverse-proxy contract.
- Replacing USB CLI (it must write the same Wi-Fi group).
- A second visual language (do not restyle tokens; follow the `web-console` skill).
- Making OTG_ENABLED drive USB or radio (ESP32-C6 has no USB OTG). Persist and show only.
- Automatic STA retry after the 5 s window fails (stay AP until reboot or apply).
- Captive-portal DNS beyond serving `/` on the device IP.

## Decisions

### 1. ESP32Async AsyncWebServer (not sync `WebServer`, not PsychicHttp)

Use `ESP32Async/ESPAsyncWebServer` and `ESP32Async/AsyncTCP` in `lib_deps`.

- Sync `WebServer.h` handles each request on the Arduino loop. A firmware upload or a slow client would stall MQTT and the Zigbee dispatcher.
- PsychicHttp / raw `esp_http_server` fight the Arduino-Zigbee module style already used here.
- The original me-no-dev AsyncWebServer is unmaintained on Arduino 3; ESP32Async is the current Arduino-3 fork.

Request work that touches settings or OTA hops to the loop (or a flag) so flash and restart stay serialized with `SettingsManager`.

### 2. Settings groups as records

Bump `GLOBAL_CURRENT_SETTINGS_VERSION`. Replace flat / mixed layout with nested records, for example `WifiSettings`, `MqttSettings`, `ZigbeeSettings`, plus the existing device-map array. `SettingsManager::applyDefaults` writes Wi-Fi defaults: MODE AP, OTG_ENABLED false, empty BSID, PASSWORD `00000000`, AP IP `192.168.0.1`, DEVICE_NAME `z2m-gateway` (hostname only). MQTT client id uses `DEFAULT_MQTT_CLIENT_ID`, not DEVICE_NAME.

C++ member names stay meaningful (`bssid`, `password`, `deviceName`, `apIp`, `mode`, `otgEnabled`). USB CLI `wifi <ssid> <password>` maps to BSSID/PASSWORD and should set MODE STA (operator is pointing at a router). BOOT pin still forces AP for that boot without rewriting MODE.

### 3. WiFiController boot (replace AP timeout)

Remove `kApTimeoutMs` handoff to STA.

- MODE AP or empty BSSID: `startAp` using the BSSID setting (default `z2m-gateway`) and stored PASSWORD (default `00000000`).
- MODE STA and BSSID set: `startSta` using the same BSSID/PASSWORD; hostname = DEVICE_NAME only. If not `WL_CONNECTED` within 5000 ms, `startAp` with that same BSSID.
- After fallback AP, do not auto-return to STA.

SoftAP uses stored AP IP (default `192.168.0.1`) as address and gateway.

### 4. Static UI on LittleFS (`data/`)

Serve `index.html` and `css/all.css` from LittleFS root (same layout as boiler-controller). If those files are missing, serve a tiny PROGMEM fallback that says the filesystem image is missing.

WiFi section: form + `GET/POST /api/wifi` (JSON of the five fields). Save persists and requests restart (same delayed restart as settings save).

### 5. Shared web-console chrome

Follow the personal `web-console` skill (boiler-controller look): `app-shell`, left `.sidebar-nav` for Status / WiFi / ZigBee / Devices / System, folder tabs inside System (Update, Log). Copy `chrome.css` tokens; no extra CSS framework.

### 6. Logger ring + poll

Extend `Logger` with a fixed ring. `GET /api/log` returns text. Log tab polls and has Refresh. USB `CON_DEBUG` unchanged.

### 7. HTTP OTA on the Update tab

`POST /update` → Arduino `Update` `U_FLASH`. `GET /api/version` for `FIRMWARE_VERSION`. Success → `SettingsManager` restart. Failure → no restart. No ArduinoOTA in this change.

### 8. No HTTP auth

SoftAP PSK is the Wi-Fi PASSWORD (default `00000000`, same default for STA). STA is on the home LAN.

## Risks / Trade-offs

- **[Risk]** HTTP + Zigbee + Wi-Fi on one 2.4 GHz radio during a large OTA → **Mitigation**: async I/O; OTA is infrequent.
- **[Risk]** Firmware-only flash without `uploadfs` → empty UI → **Mitigation**: PROGMEM fallback; document `pio run -t uploadfs`.
- **[Risk]** EEPROM layout change bricks old settings → **Mitigation**: version bump + rewrite defaults when marker/version mismatch (existing pattern).
- **[Risk]** 5 s STA window is tight for some APs → **Mitigation**: specified by the operator; can widen later without changing MODE rules.
- **[Risk]** Unauthenticated OTA on STA → **Mitigation**: accepted for v1; README callout.
- **[Risk]** Log ring RAM → **Mitigation**: small fixed buffer; drop oldest.
- **[Risk]** Wrong OTA image → **Mitigation**: `Update` checks; USB flash recovery.

## Migration Plan

1. Flash firmware that includes grouped settings + web module (old EEPROM is reset to defaults → AP).
2. Upload LittleFS so `/www` exists.
3. Join AP (name from DEVICE_NAME or BSID per rules), open `http://192.168.0.1/`, set Wi-Fi, save.
4. Rollback: USB flash previous firmware.

## Open Questions

- Exact logger ring size and poll interval (implementation detail).
- JSON shape for later Status / ZigBee / Devices APIs — decide with those tabs.
