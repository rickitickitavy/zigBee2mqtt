# 0005. Auth boundary

## Context

The LAN console must require a session. The northbound and local debug paths already exist without HTTP.

## Decision

Session authentication applies to the web console (protected `/api/*` and operator chrome). USB CLI, MQTT, and Zigbee stay usable without a console session.

Do not add HTTP Basic. Do not lock MQTT or the USB CLI unless a later ADR says so.

`isAdmin` sessions ignore Remember me and expire after 10 minutes idle.

## Consequences

Anyone who can publish to the broker can still command devices. Physical USB access still has the CLI. That is accepted until a new decision.
