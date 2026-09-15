## Context

See proposal.md (Why). `WebConsole::begin()` runs once after `WiFiController` already has AP or STA. `WebConsole::rebind()` is `server.end()`, short delay, `server.begin()`, and keeps route handlers. `WiFiController::update()` sets `staNeedsWebRebind` only when `WiFi.status()` leaves `WL_CONNECTED`, then calls `interfaceReadyHandler` on the next connected edge. Arduino Wi-Fi events in `main.cpp` log SoftAP only. MQTT uses an outbound `WiFiClient`; ping is ICMP. Both survive a dead AsyncTCP listen PCB.

AsyncWebServer MUST be rebound from Arduino `loop` (`update()`), not from the Wi-Fi event task.

## Goals / Non-Goals

**Goals:**
- Rebind the console listen socket whenever STA has an IPv4 address after the first post-boot bind, including GOT_IP without a polled disconnect.
- Keep the existing connected-edge rebind as a backup.
- Leave routes, UI, MQTT, and SoftAP start order unchanged.

**Non-Goals:**
- Fixing AsyncTCP connection-slot exhaustion from a stuck browser tab.
- Rebinding SoftAP on AP restart (out of this failure mode).
- Changing first-boot `begin()` timing relative to Zigbee (Zigbee is not on the host).

## Decisions

1. **Arm rebind from STA events; run it in `update()`**  
   Handle `ARDUINO_EVENT_WIFI_STA_DISCONNECTED` (or lost IP) and `ARDUINO_EVENT_WIFI_STA_GOT_IP` by setting `staNeedsWebRebind`. When STA is connected (has IPv4) and the handler is set, `update()` calls `WebConsole::rebind()` and clears the flag.  
   Alternative: call `rebind()` inside `WiFi.onEvent`. Rejected: AsyncTCP listen close/open from the event task is unsafe.

2. **Skip the boot GOT_IP**  
   Boot join completes in the `WiFiController` constructor, then `WebConsole::begin()`. That first GOT_IP MUST NOT set `staNeedsWebRebind` (or the flag MUST be cleared before `update()` can run the handler). Later GOT_IP (reconnect, roam, DHCP renew) MUST set it even if `WiFi.status()` never read disconnected.  
   Alternative: compare last rebound IP to `WiFi.localIP()` and skip same-address GOT_IP. Rejected: a renew can keep the same address while the listen PCB is already dead.

3. **Keep the `WL_CONNECTED` edge**  
   If events are missed, the existing `staWasConnected` path still arms and runs the same handler.

4. **Handler stays in `main.cpp`**  
   `setInterfaceReadyHandler` already calls `webConsole->rebind()`. Do not add a static `WebConsole` only so a Wi-Fi callback can reach `this`.

## Risks / Trade-offs

- **[Risk]** Frequent GOT_IP flaps close in-flight HTTP uploads → **Mitigation**: OTA is rare; accept a dropped POST. Do not debounce so long that the console stays dead after a real renew.
- **[Risk]** `end()`/`begin()` while a client is connected looks like a hang for that tab → **Mitigation**: operator reloads; MQTT/ping stay up.
- **[Risk]** Event fires before `webConsole` exists → **Mitigation**: constructor must not arm rebind; handler is registered after `begin()`.

## Migration Plan

Flash host firmware. No settings or MQTT topic migration. Rollback is the previous host image.

## Open Questions

(none)
