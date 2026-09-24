# 0004. SPI is always asynchronous

## Context

A blocking SPI transfer in MQTT, HTTP, CLI, or `loop` stalls Wi-Fi or the radio.

## Decision

All host–slave SPI interactions are asynchronous. Callers enqueue a framed command with a sequence id and learn the result later. The host MUST NOT block those product paths waiting on a transfer. The slave MUST NOT block the Zigbee stack on SPI.

## Consequences

New SPI commands get a queue path and a completion/event, not `transfer` + wait in a handler. Timeouts surface as async errors.
