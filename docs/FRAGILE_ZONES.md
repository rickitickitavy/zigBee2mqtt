# Fragile zones (frozen)

These areas took substantial optimization and are easy to regress. They are **frozen**.

## Hard rule

1. **Do not change** a fragile zone (or anything that can influence it) unless a human explicitly agrees in the current conversation.
2. Agreement must name the zone id (for example: `I approve fragile zone slave-spi-ota`).
3. Agents MUST stop and ask before planning or editing when the work touches a frozen path or its influence surface.
4. Cursor **hooks deny** edits to hard-frozen paths until a human writes a temporary allow entry (see below). Rules alone are not enough.

Related ADRs (why the design exists — not a substitute for this freeze):

- [0008 — OTA slave before host](adr/0008-ota-slave-before-host.md)
- [0003 — Two chips, two images, joined ZIP](adr/0003-two-chips-two-images-joined-zip.md)
- [0011 — Fragile zone freeze policy](adr/0011-fragile-zone-freeze.md)

Machine-readable path list used by the hook: [`.cursor/fragile-zones.json`](../.cursor/fragile-zones.json).

---

## Zone `slave-spi-ota`

**Title:** SPI programming of the slave firmware image  

**Why frozen:** Host→slave SPI OTA (begin / chunk / end, ACK, retry, timeouts, in-flight gating, apply-after-ACK drain) was optimized over many iterations. Small timing or queue changes break updates silently.

### Hard-frozen paths (hook-enforced)

- `src/FirmwareOta.cpp`
- `include/FirmwareOta.h`

### Influence surface (ask human; may not be hook-blocked)

Anything that changes SPI OTA behavior without editing the files above, including but not limited to:

- `include/SpiProtocol.h` — `SpiCmdFirmwareOta`, `SPI_OTA_*`, pack/unpack helpers
- `src/InterChipHost.cpp`, `include/InterChipHost.h` — `holdForFirmwareOta`, OTA reply timeouts, queue gating while `isUpdatingSlave`
- `src/InterChipSlave.cpp`, `include/InterChipSlave.h` — slave handling of `SpiCmdFirmwareOta`, ACK drain / transfer-done hooks
- SPI clock override used only for slave OTA (`beginSlaveSpiClockOverride` / restore)
- Changing SPI payload size, outbound queue depth, or IRQ/ready timing in ways that affect OTA throughput or ACKs

---

## Zone `firmware-update-process`

**Title:** Full joined firmware update (HTTP → slave SPI → host apply → reboot)  

**Why frozen:** End-to-end update is one pipeline. Breaking staging, ZIP layout, progress API, or UI sequencing can brick or desync chips. Includes zone `slave-spi-ota`.

### Hard-frozen paths (hook-enforced)

- All hard-frozen paths from `slave-spi-ota`
- `src/JoinedFirmwareZip.cpp`
- `include/JoinedFirmwareZip.h`
- `scripts/firmware_join.py`
- `scripts/firmware_split.py`

### Influence surface (ask human; may not be hook-blocked)

- `src/WebConsole.cpp` / `include/WebConsole.h` — `/update`, `/api/update/status`, staging handlers
- `data/index.html` — System → Update tab, progress polling, reboot wait dialog
- Joined package layout / member names (`slave.bin`, `host.bin`) and OTA LittleFS staging paths
- Partition / Update API usage that changes how host or slave commits the inactive slot
- ADR 0008 order (slave first, then host) — do not invert without a new ADR **and** human approval of this zone

---

## Temporary allow (human only)

When a human has approved a change in chat, create or edit:

`.cursor/fragile-zones.allow`

One entry per line (comments start with `#`):

```text
# Allow editing the SPI OTA zone for this session
slave-spi-ota
```

Or allow a single relative path:

```text
src/FirmwareOta.cpp
```

Delete or clear the file when the approved work is finished. Agents MUST NOT create or widen this file without the human’s explicit instruction in the same turn.
