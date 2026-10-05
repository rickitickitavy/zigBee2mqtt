# Proposal

## Why

Pointer-up on the dimmed overlay after a text drag closes dialogs while the operator is still selecting. Readonly fields look like editable ones. Admin sessions still end on a 10-minute wall clock from login because the cookie `Max-Age` is fixed at login and the 10-second `/api/auth/me` poll is not operator activity, so working in the console still logs the user out.

## What Changes

- Overlay dismiss SHALL run only when the pointer **down** lands on the overlay (outside the dialog panel). Pointer-up outside after a drag that started inside the panel MUST NOT dismiss.
- Readonly (and `disabled`) form controls in field rows SHALL use a slightly grayed background and muted text so they are visually distinct from editable controls.
- Session auto-logout SHALL be **idle** time, not elapsed time since login. Operator actions (pointer, keyboard, form input on the signed-in console) SHALL reset the idle timer. Background session polls and other automatic requests MUST NOT reset it. Admin still ignores Remember me and still idles out after 10 minutes of no operator activity.

Firmware `FIRMWARE_VERSION` is `0.4.11` for this proposal.

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `web-console`: Overlay click-outside becomes mousedown-outside; readonly fields are grayed; signed-in idle logout follows operator activity, not login wall-clock or poll traffic.
- `console-users`: Admin (and other sessions that use idle) expire after idle with no operator activity; authenticated polls MUST NOT count as activity; cookie lifetime MUST not force logout while the idle timer is still valid.

## Impact

- `data/index.html` overlay listeners, session watch, and activity pings.
- `data/css/all.css` readonly/disabled field styling.
- `UserStore` session touch vs validate; `WebConsole` cookie `Max-Age` on login vs activity; `GET /api/auth/me` must not refresh idle.
- Host LittleFS / embedded web assets. Flash host (and slave if shipping a combined image) after apply. No MQTT or Zigbee protocol change.
