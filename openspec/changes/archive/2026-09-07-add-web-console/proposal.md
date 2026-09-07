## Why

USB CLI is the only way to reach this gateway today. A browser console on the device’s own IP lets someone configure and maintain it from a phone or laptop—especially when the radio is in AP mode because STA was never set or the router did not come up.

## What Changes

- Add an HTTP web console served by the firmware whenever Wi-Fi AP or STA is up.
- Main navigation is the shared **web-console** chrome (left sidebar sections, folder-style inner tabs): **Status**, **WiFi**, **ZigBee**, **Devices**, **System**. Status, ZigBee, and Devices are empty placeholders in this change (layout only).
- **WiFi** is a real settings form: BSSID (one name for STA and AP, default `z2m-gateway`), PASSWORD (default `00000000` for AP and STA), DEVICE_NAME (Wi-Fi hostname only), AP IP (AP mode only, default `192.168.0.1`), MODE (AP or STA, default AP), OTG_ENABLED (default false).
- Persist settings as **groups**: `GlobalSettings` holds one record per group. This change introduces the grouping layout and the Wi-Fi group record (other groups are existing fields moved into records; their behavior is unchanged except Wi-Fi).
- Boot Wi-Fi: MODE AP → AP. MODE STA → try the router for 5 seconds, then fall back to AP. STA and AP use the same BSSID setting. DEVICE_NAME is not used as an SSID.
- **System** has two nested classical tabs: **Update** and **Log**.
- **Update** accepts a firmware `.bin` over HTTP and writes it through the existing dual OTA partitions, then restarts.
- **Log** shows recent firmware log lines (same stream as USB `LOGGER` output), refreshable in the page.
- No HTTP authentication in this change. Visual language is the personal `web-console` skill (boiler-controller tokens and tab chrome).
- **BREAKING** for stored EEPROM layout: settings version must bump; old `NetworkSettings` (`ssid` / `hostName` / `wifiEnabled`) is replaced by the Wi-Fi group. The previous “AP for 5 minutes then STA” timeout is removed.

## Capabilities

### New Capabilities

- `web-console`: Browser UI shell, HTTP serving, Wi-Fi settings tab, System Update (OTA), and System Log viewer.

### Modified Capabilities

- (none)

## Impact

- New web module and static UI files; `setup()` starts the server after `WiFiController` is constructed.
- New library: async HTTP stack (ESP32Async ESPAsyncWebServer + AsyncTCP), chosen so HTTP does not block Zigbee or MQTT.
- `GlobalSettings` / `SettingsManager` regroup fields; `WiFiController` implements MODE, 5 s STA window, and AP naming rules. USB CLI `wifi` must write the new group.
- `Logger` gains an in-memory ring so the Log tab can read lines that already went to USB CDC.
- OTA uses the existing `app0`/`app1` + `otadata` layout in `partitions/zigbee_zczr_16MB.csv`. LittleFS image is `data/index.html` + `data/css/all.css`.
- MQTT and Zigbee control paths stay as they are aside from reading regrouped setting records.
