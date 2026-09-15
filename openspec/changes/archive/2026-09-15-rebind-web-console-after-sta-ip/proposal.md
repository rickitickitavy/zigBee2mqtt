## Why

On STA, ICMP and MQTT often keep working after a radio blip or DHCP renew while the HTTP web console on port 80 does not. AsyncWebServer’s listen socket stays bound to the previous netif; MQTT reconnects as an outbound client and ping never uses that socket. Operators lose the only browser path to the host without a reboot.

## What Changes

- After STA has an IPv4 address again following a drop or renew, the host MUST rebind the web console listen socket (`end` then `begin` on the existing AsyncWebServer) so `http://<sta-ip>/` answers.
- The first successful STA `begin()` after boot MUST stay as-is; that first bind MUST NOT be immediately rebound.
- MQTT, USB CLI, and Zigbee/SPI paths MUST keep working through the rebind. No new HTTP auth, routes, or UI.

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `web-console`: HTTP on port 80 MUST remain reachable on the current STA IPv4 after reconnect or GOT_IP, not only after a `WiFi.status()` connected-edge that the poller happens to see

## Impact

- Host `WiFiController` STA event / reconnect path and `WebConsole::rebind`
- `main.cpp` interface-ready handler wiring (already present)
- No MQTT, SPI, or LittleFS UI changes
