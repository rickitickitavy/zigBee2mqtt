# Design

## Context

See proposal.md — Why. Overlay dismiss is `click` on `.dialog-overlay` (`onDialogOverlayClick` in `data/index.html`): a `click` fires on pointer-up on the overlay, so a text drag that started in the panel and ended on the dimmed area dismisses. Field inputs share the same `--bg` fill; there is no `:read-only` / `:disabled` gray. `UserStore::startSession` sets admin cookie `Max-Age` to `AUTH_ADMIN_IDLE_MS / 1000` (600 s) at login. `sessionUser` refreshes `lastActivityMs` on every successful lookup. `GET /api/auth/me` uses `requireUser` (touches activity) every 10 s from `startSessionWatch`, and `/api/status` polling also authenticates. The browser still drops the admin cookie 10 minutes after login even while the operator clicks. Firmware is `0.4.11`.

## Goals / Non-Goals

**Goals:**

- Dismiss overlays only on pointer-down on the overlay target.
- Gray readonly/disabled field controls without changing layout or tokens family.
- Idle logout based on operator activity; polls do not count; admin cookie must not wall-clock expire at 10 minutes from login.

**Non-Goals:**

- Changing idle duration (`AUTH_ADMIN_IDLE_MS` stays 10 minutes) or Remember-me lengths.
- Login-form overlay dismiss (not a `.dialog-overlay`).
- MQTT/Zigbee or role-matrix changes.

## Decisions

1. **Overlay: `mousedown` (and `pointerdown` if already used) on overlay, not `click`.**  
   Bind `mousedown` on `.dialog-overlay`; keep `event.target === event.currentTarget`. Ignore `click` for dismiss. Alternative: record mousedown target and only dismiss on click if both down and up were overlay — extra state; mousedown-only matches the request.

2. **Readonly styling:** `.field input:read-only`, `.field input:disabled`, `.field select:disabled`, `.field textarea:read-only` use `background` a mix toward `--surface`/`--muted` and `color: var(--muted)`. Do not gray checkboxes that are merely unchecked. Alternative: `opacity` on the whole `.field` — would fade labels too much.

3. **`sessionUser` gains `touchActivity`.**  
   Default true for mutating/operator-driven APIs via `requireUser`. `GET /api/auth/me`, status, log, and other automatic GETs validate the session (idle expiry still applies) but pass `touchActivity = false`. Alternative: never touch in `sessionUser` and only touch from a dedicated endpoint — then every Save would need an extra call; better to touch on explicit operator HTTP (POST/PUT that follows a click) **and** a client activity ping.

4. **Client activity ping.**  
   Signed-in console listens for `pointerdown`, `keydown`, `input` on `window` (throttled, e.g. at most once per 30 s) and `POST /api/auth/touch` (or equivalent) which validates the cookie and sets `lastActivityMs`. Status poll must not call touch. Alternative: only reset on POST APIs — clicking around Status without Save would still idle out; the request is any operator action.

5. **Admin cookie `Max-Age`.**  
   Issue admin sessions as a session cookie (`maxAgeSec = 0`, omit `Max-Age`) like non-remember simple users, and enforce 10-minute idle only in `sessionUser`. Re-issue `Set-Cookie` on touch if a future Max-Age is used; first implementation: omit Max-Age for admin so the browser does not drop the cookie at 10 minutes from login. Alternative: refresh Max-Age=600 on every touch — also works but duplicates idle in the browser.

## Risks / Trade-offs

- [Status poll 401 after idle] → Session watch already exists; on 401 show login. Touch and me GET must handle 401 the same way.
- [Throttled ping vs 10 min idle] → 30 s throttle is well under 10 minutes; do not set throttle ≥ idle.
- [Firmware-update reboot wait] → Keep existing skip of session watch during `firmwareRebootWaitActive`; do not treat that as operator activity.

## Migration Plan

Flash host with rebuilt LittleFS/web assets. Existing admin cookies with Max-Age 600 still expire at login+10 min until the operator signs in again after the update. Rollback: previous image restores click-dismiss and login-wall-clock cookie.

## Open Questions

None.
