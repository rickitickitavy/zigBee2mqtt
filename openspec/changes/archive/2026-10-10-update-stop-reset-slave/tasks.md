# Tasks

## 1. Host slave-reset pause gate

- [x] 1.1 Add pause deadline state and `isSlaveResetPaused` / start / cancel / remaining helpers on `InterChipHost` (120000 ms window) and verify unit-style millis remaining math (active, expired, cancel clears)
- [x] 1.2 Gate `enterReset` / `pulseResetStart` / `resetSlaveSynchronous` so paused host never asserts EN/RST; on pause start release EN high if a pulse was in progress; log a skip; verify link-loss and a forced sync reset path do not pulse while paused (needed for USB slave flash), and resume after cancel/expiry
- [x] 1.3 Confirm OTA version-verify path that calls `resetSlaveSynchronous` is covered by the same gate (no separate bypass) and verify a paused host skips that reset

## 2. Admin API

- [x] 2.1 Add admin-only `GET` and `POST /api/slave-reset-pause` returning `{ active, remainingMs }` and verify non-admin gets 401/403 and start/cancel/expire responses match host state

## 3. Update card UI

- [x] 3.1 Add **Wait for update slave** button on System → Update firmware card with a short USB-flash hint; start pause on click when inactive and show live countdown on the button; verify countdown tracks GET remaining
- [x] 3.2 On Update tab show, GET pause state and display real remaining if active; verify leaving and re-entering the tab does not reset to a full 120 s
- [x] 3.3 Second click while active cancels via POST; verify button returns to inactive label (**Wait for update slave**) and host remaining is zero/inactive

## Workflow follow-up

- Archive the change after review when implementation is done.
