# Proposal

## Why

External status LEDs are no longer a uniform red set: each board wires LED1–LED6 as fixed blue, green, or red parts. Full GPIO HIGH on blue/green is often too bright, so operators need a Hardware-tab brightness control while red indicators stay simple digital on/off. The same change also tightens console UX: login Enter behavior, a role that gates device commands on the Devices table, and auto-hiding save plates.

## What Changes

- Document and encode a fixed per-role color map for LED1–LED6 (host/master and slave).
- Drive blue and green LEDs with PWM when on; red LEDs keep the existing digital HIGH/LOW drive.
- Add System → Hardware controls for **BLUE** and **GREEN** LED brightness (1–100%), persist them with hardware settings, and apply them to every LED of that color on host and slave.
- Extend `/api/hardware` GET/POST (and settings export/restore) with the two brightness fields.
- Sync brightness to the slave so slave blue/green LEDs match the Hardware tab.
- On the login form, Enter with an empty password moves focus to the password field only (no submit).
- Add role **Other device control** (`controlDevices`): users without it (and without `isAdmin`) MUST NOT see Devices table action controls (`…` / type actions) or Manual command; APIs that send those device commands MUST reject them.
- Hide error and success plates in the web UI after 10 seconds.

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `status-rgb-led`: Fixed LED colors per board role; PWM on for blue/green; red remains digital; brightness settings drive blue/green duty.
- `web-console`: Hardware brightness fields; login Enter → password focus; Devices actions gated by `controlDevices`; auto-hide error/success plates after 10 s.
- `host-slave-spi`: Host pushes blue/green LED brightness to the slave without treating it as Zigbee radio settings that restart the radio.
- `console-users`: Persist and enforce `controlDevices` (“Other device control”) for Devices table command paths.

## Impact

- `StatusRgb` pin setup and `writeLevels` (LEDC/PWM vs `digitalWrite`).
- Host `SettingsManager` reserved hardware fields; `WebConsole` hardware API and `data/index.html` Hardware tab.
- Host↔slave SPI path to deliver brightness to the slave `StatusRgb`.
- Spec purpose text that still assumes six red LEDs.
- Joined host/slave firmware rebuild for both boards.
- `UserStore` / Security dialog / Devices UI and device-command API checks; login form key handling; shared toast hide timer in `data/index.html`.
