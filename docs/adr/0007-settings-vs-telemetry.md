# 0007. Settings vs telemetry

## Context

Live radio fields look like device properties in the UI. Storing them as settings overwrites identity on reboot or list replace.

## Decision

Persist settings identity: IEEE, classified type, name, topics, flags, user roles, theme, and other non-runtime attributes. They MUST round-trip save, list/sync (including host↔slave device dump), reboot, tables, and dialogs.

Readonly in the UI does not mean skip persist. IEEE and type stay in JSON even when the dialog is readonly.

Do not persist runtime-only telemetry: RSSI, online/liveness, in-flight LED, last packet time, and similar live radio/link state.

## Consequences

New entity fields must be classified as settings or telemetry before they are added. Settings fields appear on every list and dialog that shows that entity.
