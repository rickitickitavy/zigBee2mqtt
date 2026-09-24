## Context

See proposal.md — Why. Chrome tokens live in `data/css/all.css` `:root`. Several chrome colors are still hardcoded (`#dcdcdc` idle tabs, button whites, error/success boxes). SPI speed already occupies the first 4 bytes of `GlobalSettings.reserved`. Settings export is `{version,mqtt,zigbee,hardware,devices}`.

## Goals / Non-Goals

**Goals:**
- Two themes, token-driven, apply on picker change.
- Persist only on the icon Save; default Light.
- Dark palette is cool slate-night (no magenta/pink): `#0E141B` page, `#18222D` surface, `#6A9FD8` primary, `#E4EAF0` text, `#8A97A6` muted; primary button dark label on steel blue.

**Non-Goals:**
- Follow-system / auto theme.
- Extra themes or per-section colors.
- Moving Theme off Maintenance or onto Hardware Save.

## Decisions

1. **`html[data-theme="light"|"dark"]` plus CSS variables**  
   Light keeps today’s `:root` values. Dark overrides the same names (`--bg`, `--surface`, `--text`, `--muted`, `--accent`, `--accent-soft`, `--border`) and new tokens for idle chrome / buttons (`--chrome-idle`, `--btn-primary-fg`). Map leftover hex in `all.css` to those tokens so dark actually covers sidebar, tabs, and buttons.  
   Alternative: second stylesheet — rejected (two files to flash).

2. **`GET`/`POST /api/theme` `{ "theme": "light"|"dark" }`**  
   Page load fetches GET and sets `data-theme` before or with first paint. Picker change only sets the attribute. Save POSTs and `saveMain(false)` (no restart).  
   Alternative: only `localStorage` — rejected; operator asked for a host setting.

3. **Persist in `reserved[4]` after SPI speed**  
   `0` = light, `1` = dark. No `GLOBAL_CURRENT_SETTINGS_VERSION` bump; unused reserved stays zero = light. Treat as a UI group, not a new `SettingsMainCore` field (that would shift EEPROM).

4. **Export `ui: { "theme": "light"|"dark" }`**  
   Restore applies `ui.theme` when present. Missing `ui` leaves the current saved theme.

5. **Save control**  
   Same-row flex: label + `<select>` + icon button (`aria-label="Save theme"`). Compact square (~44px tap) with a disk/check glyph, not another `btn-primary` bar.

## Risks / Trade-offs

- [First paint flash] → Apply GET theme as early as the console script runs; default Light matches today’s first paint.
- [Hardcoded hex survives] → Task to replace idle/button/error hex with tokens before claiming dark is done.
- [Export older files without `ui`] → Restore skips theme.

## Migration Plan

Flash host **0.2.9**. Existing reserved bytes stay Light. Rollback is the previous image.

## Open Questions

None.
