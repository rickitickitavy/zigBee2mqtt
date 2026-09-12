## Why

The host web console is usable enough to open, but four cards are broken in daily use: Update cannot flash LittleFS, Log hides Refresh, MQTT shows empty fields, and WiFi clips Save/Reset. Operators cannot finish setup from the browser.

## What Changes

- System → Update: add a **filesystem** (LittleFS) upload form next to the existing firmware `.bin` form.
- System → Log: remove the extra page scroller that hides Refresh; left-align Refresh and give it a normal (content) width.
- MQTT: load and show current settings in the fields (and keep Save reachable).
- WiFi: allow the card to scroll so **Save** and **Reset** stay reachable.

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `web-console`: Update includes filesystem upload; Log Refresh stays visible and compact; MQTT fields show stored values; WiFi card scrolls and exposes Save/Reset.

## Impact

- `data/index.html`, `data/css/all.css`, `WebConsole` HTTP update handler (firmware vs LittleFS).
- Operators must re-flash LittleFS after HTML/CSS changes (`pio run -t uploadfs`).
