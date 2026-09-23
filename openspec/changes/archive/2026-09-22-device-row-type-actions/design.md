## Context

See proposal.md — Why. Rows already `onclick` → edit. Commands already go through `POST /api/devices/command` with `ieee`, `payload`, and optional `channel`. Types: `onOff`, `windowCovering`, `iasZone`, `unknown`. Channel mapping matches Manual command (`1`; `2`–`16` suffix; `0` parse / `ch-<ep>##`).

## Goals / Non-Goals

**Goals:**
- Hover **…** on the row right; type- and channel-aware menu; same send path as Manual command.

**Non-Goals:**
- New persisted fields, new SPI commands, or replacing Manual command / edit dialog.
- Edit/Delete inside this menu.
- Position/`cl=` items (Command tab still covers those).

## Decisions

1. **Overlay, not a column**  
   Position **…** absolutely on the row (`position: relative` on `tr`). Avoids changing the column-order spec. Hide until `:hover` or `:focus-within`; on `max-width: 600px` always show so touch works.

2. **Reuse `send` via existing POST**  
   Menu items call the same payload strings as MQTT (`ON`/`OFF`/`TOGGLE`, `OPEN`/`CLOSE`/`STOP`) plus `channel` when not a single-channel device.

3. **Action groups**  
   `channels` `2`–`16` (or parse-mode with several `ep`s): top level is the action; submenu items are channel numbers only. `channels` `1`: flat ON/OFF/TOGGLE or OPEN/CLOSE/STOP.

4. **`stopPropagation` on … and menu**  
   Row click stays edit. Click outside or Escape closes the menu.

5. **No … when no actions**  
   `iasZone` / `unknown` stay display-only.

## Risks / Trade-offs

- [Polling rebuilds the table every 2s and can close the menu] → Keep the open menu’s IEEE/channel across `renderDeviceTable`, or skip replacing the open row’s menu node.
- [16×3 items] → Nested action → channel-number submenu, not one flat list of 48 rows.

## Migration Plan

Flash host **0.2.10** (web UI). No settings migration.

## Open Questions

None.
