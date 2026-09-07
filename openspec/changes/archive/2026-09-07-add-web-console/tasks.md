## 1. Settings groups and Wi-Fi boot

- [x] 1.1 Split `GlobalSettings` into group records (`WifiSettings` with `bsid`, `password`, `deviceName`, `mode`, `otgEnabled`; MQTT and Zigbee as their own records) and bump `GLOBAL_CURRENT_SETTINGS_VERSION`; verify `sizeof(GlobalSettings) <= 4096` still holds
- [x] 1.2 Write Wi-Fi defaults (MODE AP, OTG_ENABLED false, empty BSID/PASSWORD, default DEVICE_NAME) in `SettingsManager::applyDefaults` and map USB CLI `wifi` onto the new group; verify first-boot logs show MODE AP
- [x] 1.3 Change `WiFiController` to: unconfigured → AP; MODE AP → AP named BSID; MODE STA → 5 s join then AP named DEVICE_NAME; remove the 5-minute AP-to-STA timeout; verify STA success stays STA and STA fail becomes AP with DEVICE_NAME

## 2. Dependencies and server shell

- [x] 2.1 Add `ESP32Async/AsyncTCP` and `ESP32Async/ESPAsyncWebServer` to `platformio.ini` `lib_deps` and verify `pio run` still resolves libraries
- [x] 2.2 Add `WebConsole` (header + cpp) that owns `AsyncWebServer` on port 80, starts after `WiFiController` is constructed, and verify `pio run` succeeds
- [x] 2.3 Wire `WebConsole` into `setup()` and keep USB CLI / MQTT / Zigbee dispatch in `loop()` unchanged; verify a GET to `/` does not require a password

## 3. Static UI and tabs

- [x] 3.1 Keep `data/index.html` + `data/css/all.css` on the `web-console` skill chrome (sidebar Status/WiFi/ZigBee/Devices/System, System folder tabs); verify LittleFS serves `/index.html` and `/css/all.css`
- [x] 3.2 Serve `/` and `/css/*` from LittleFS root, with a PROGMEM fallback page if those files are missing; verify firmware-only flash still returns HTTP 200 with the fallback text
- [x] 3.3 Keep sidebar section switching and empty placeholder panels for Status, ZigBee, Devices; verify switching hides the previous panel and those three panels have no settings controls
- [x] 3.4 Add nested System tabs Update and Log with the same tab pattern; verify opening System then Log shows Log and hides Update

## 4. WiFi tab

- [x] 4.1 Add `GET`/`POST /api/wifi` for the Wi-Fi group JSON and a WiFi tab form (BSID, PASSWORD, DEVICE_NAME, MODE, OTG_ENABLED); verify GET matches EEPROM and POST persist + restart apply the boot rules
- [x] 4.2 Document AP/STA naming, the 5 s STA window, and unauthenticated OTA in README; verify README uses the field names BSID / MODE and the console URL

## 5. Logger ring and Log tab

- [x] 5.1 Extend `Logger` with a fixed in-memory ring (tens of lines) written on every printed line; verify USB CDC still prints when `CON_DEBUG` is on
- [x] 5.2 Add `GET /api/log` returning the ring as text; verify new `LOGGER.info` lines appear in the response
- [x] 5.3 Wire the Log tab to poll `/api/log` and include a Refresh control; verify the page updates without leaving the Log tab

## 6. Firmware Update tab

- [x] 6.1 Add `GET /api/version` returning `FIRMWARE_VERSION` and show it on the Update tab; verify the displayed version matches `Defines.h`
- [x] 6.2 Add `POST /update` streaming the upload into Arduino `Update` (`U_FLASH`); on success request restart via `SettingsManager`; verify a rejected/aborted upload does not restart and returns an error the page shows

## 7. On-device check

- [ ] 7.1 Flash firmware + LittleFS; verify unconfigured AP, WiFi tab save to STA, 5 s fail → AP named DEVICE_NAME, System Update/Log, and MQTT/Zigbee still run while browsing
