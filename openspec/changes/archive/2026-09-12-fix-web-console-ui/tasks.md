## 1. System Update filesystem

- [x] 1.1 Add a LittleFS/filesystem upload card on System → Update and a `POST /update/data` handler, and verify a valid filesystem image is accepted without writing the app OTA slot
- [x] 1.2 Keep the existing firmware `/update` form working, and verify a firmware `.bin` still programs the inactive app slot

## 2. System Log layout

- [x] 2.1 Make the log textarea the only scroller on System → Log and left-align a compact Refresh button, and verify Refresh stays visible at typical desktop and phone widths

## 3. MQTT data

- [x] 3.1 Load MQTT fields from `GET /api/mqtt` on page load and when the MQTT section is opened, show an error if the request fails, and verify stored server/port/user/topic appear in the inputs
- [x] 3.2 Escape MQTT JSON so a password or topic with quotes still parses, and verify those values fill the form

## 4. WiFi scroll and Reset

- [x] 4.1 Let the WiFi card scroll (remove hidden overflow) and keep Save plus Reset in view after scroll or in a pinned footer, and verify both buttons are clickable when the card is taller than the panel
- [x] 4.2 Make Reset call `GET /api/wifi` (no settings write) and verify edited fields return to stored values

## 5. Verify UI

- [ ] 5.1 Exercise Update (both forms), Log, MQTT, and WiFi in the browser on the host and confirm each of the four bugs is gone
