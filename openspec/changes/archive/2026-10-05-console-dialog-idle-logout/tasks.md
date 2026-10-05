# Tasks

## 1. Overlay dismiss

- [x] 1.1 Bind overlay dismiss to `mousedown` on `.dialog-overlay` (target === currentTarget) and remove dismiss-on-`click`; verify drag-select that pointer-ups on the dimmed area leaves the dialog open, and pointer-down on the overlay still dismisses like Cancel.

## 2. Readonly fields

- [x] 2.1 Style `.field` readonly and disabled inputs/selects/textareas with a slightly grayed background and muted text; verify Zigbee IEEE/transport/type look distinct from NAME on the parameter dialog in Light and Dark.

## 3. Idle logout

- [x] 3.1 Add `sessionUser` `touchActivity` (false for `GET /api/auth/me`, status, log, and other automatic polls); verify those GETs still 401 after true idle and do not extend `lastActivityMs`.
- [x] 3.2 Issue admin sessions without cookie `Max-Age` (idle enforced in `sessionUser` only); verify an admin who keeps using the console past 10 minutes from login still has a cookie and is not sent to login.
- [x] 3.3 Add throttled operator-activity `POST /api/auth/touch` from signed-in `pointerdown`/`keydown`/`input`; verify unused admin is logged out after 10 minutes despite session polls, and a click at 9 minutes idle keeps the session.

## 4. Version

- [x] 4.1 Confirm `FIRMWARE_VERSION` is `0.4.12` (propose was `0.4.11`; apply source bump) and `pio run -e esp32-c6-devkitc-1` succeeds.
