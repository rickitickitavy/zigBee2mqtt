# 0008. OTA slave before host

## Context

Two chips must stay protocol-compatible. Flashing the host first can leave a live slave on the old SPI contract.

## Decision

Web firmware OTA stages the image on host LittleFS, flashes the slave over SPI first, then updates the host. The HTTP POST returns when the file is staged, not when both chips are done.

The first factory deploy remains USB flash on both chips.

## Consequences

Do not invert the OTA order without a new ADR. Recover a failed mid-OTA pair with USB on each chip.
