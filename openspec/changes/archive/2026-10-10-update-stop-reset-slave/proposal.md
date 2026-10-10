# Proposal

## Why

Operators flash the Zigbee slave over **USB** while the host is still powered. The host otherwise keeps pulsing slave EN (SPI link loss, Zigbee start timeout, keepalive silence, OTA verify retries), which interrupts USB programming. They need a short, deliberate window where the host will not reset the slave.

## What Changes

- System → Update card gains a **Wait for update slave** control (primary use: USB slave flash window).
- Activating it suppresses **all host-driven slave EN/RST pulses** for **120 seconds**, leaves EN released (not held in reset), and shows a live countdown on the button.
- Opening the Update tab while the pause is active shows the **remaining** time (server truth, not only client memory).
- Pressing the button again while the pause is active **cancels** the mode immediately.
- When the 120 s window ends (or is cancelled), automatic slave reset behavior returns to today’s rules.

## Capabilities

### New Capabilities

- _(none)_

### Modified Capabilities

- `web-console`: Update tab UI and APIs for the stop-reset control, countdown, and toggle-off.
- `host-slave-spi`: Host MUST honor a temporary “do not reset slave” window for every EN/RST path (async bringup resets and synchronous `resetSlaveSynchronous`, including OTA verify resets).

## Impact

- Host: `InterChipHost` reset entry points, small WebConsole API(s), embedded `data/index.html` Update card.
- Admin-only (same as other System → Update controls).
- Does not change SPI frame protocol or Zigbee radio behavior on the slave; only host EN/RST gating.
- Fragile influence: `InterChipHost` (ask/allow at apply if editing those paths). During pause, OTA version-verify resets are also suppressed—operator chooses that trade-off.
