# 0011. Fragile zone freeze

## Context

Some firmware paths (especially slave SPI OTA and the full joined update pipeline) were expensive to stabilize. Agents and opportunistic refactors can regress them without noticing. ADR 0008 records OTA *order*, but does not stop edits to the implementation.

## Decision

Maintain a living freeze list in `docs/FRAGILE_ZONES.md` plus `.cursor/fragile-zones.json`.

- Cursor project rule (always apply): agents must ask a human before changing a zone or its influence surface.
- Cursor project hook: deny Write/StrReplace/Delete on hard-frozen paths unless `.cursor/fragile-zones.allow` lists the zone id or path.
- Architectural “why” stays in ADRs (0008, 0003); operational “do not touch” stays in the fragile-zones docs and hook.

## Consequences

New frozen areas are added to `FRAGILE_ZONES.md` and `fragile-zones.json` together. Removing a freeze requires human agreement and an ADR update if the architectural decision changes.
