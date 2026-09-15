## 1. STA events arm rebind

- [x] 1.1 Handle STA disconnect and GOT_IP in `WiFiController` (not only AP events in `main`), set `staNeedsWebRebind` for later GOT_IP and disconnect, and verify boot join does not leave the flag set before `WebConsole::begin`
- [x] 1.2 Call `interfaceReadyHandler` from `update()` when STA has IPv4 and `staNeedsWebRebind` is set, never from the Wi-Fi event callback, and verify a log line `Rebinding web console after Wi-Fi change` appears after reconnect, not immediately after the first boot bind

## 2. Keep existing fallback

- [x] 2.1 Keep the `WL_CONNECTED` drop/restore path arming the same handler, and verify a polled disconnect still triggers one rebind after the station is connected again

## 3. Check host build

- [x] 3.1 Build the host firmware and verify `pio run` succeeds
