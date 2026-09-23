## Why

The console is light-only. Operators who work at night or prefer a darker dashboard cannot switch the chrome, and a refresh always returns to gray/white. Theme belongs with the other host preferences on System → Maintenance, not as a one-off browser trick.

## What Changes

- Add a **Theme** control on System → Maintenance: **Light** (today’s chrome) and **Dark**.
- Changing the picker SHALL restyle the whole console immediately (page, sidebar, cards, fields, dialogs, buttons, tables, logs).
- Persist only when the operator clicks a small **Save** icon immediately to the right of the picker. Reload and other browsers SHALL use the last saved theme (default Light).
- Dark tokens follow a cool slate-night palette (near-black blue-gray surfaces, cool off-white text, steel-blue primary buttons; no magenta or pink) while keeping the existing layout and folder tabs.
- Settings export/restore SHALL include the saved theme and SHALL still omit Wi-Fi.

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `web-console`: Maintenance theme picker with immediate preview and icon Save; dark chrome; export/restore include theme.

## Impact

- `data/css/all.css` token remap (`html[data-theme="dark"]`), leftover hardcoded light grays.
- `data/index.html` Maintenance Theme row + GET/POST `/api/theme`.
- `WebConsole` routes; `SettingsManager` byte in `GlobalSettings.reserved` after SPI speed (no settings-version bump).
- Export JSON gains `ui.theme`; restore applies it. Firmware **0.2.9**.
