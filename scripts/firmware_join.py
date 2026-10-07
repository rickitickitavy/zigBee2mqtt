#!/usr/bin/env python3
"""Join host.bin + slave.bin into a firmware-joined.zip for System → Update."""

from __future__ import annotations

import argparse
import zipfile
from pathlib import Path


def read_firmware_version(defines_path: Path) -> str:
    text = defines_path.read_text(encoding="utf-8")
    for line in text.splitlines():
        if "FIRMWARE_VERSION" not in line or '"' not in line:
            continue
        start = line.find('"')
        end = line.find('"', start + 1)
        if start >= 0 and end > start:
            return line[start + 1 : end]
    raise SystemExit(f"FIRMWARE_VERSION not found in {defines_path}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", required=True, type=Path, help="Host application .bin")
    parser.add_argument("--slave", required=True, type=Path, help="Slave application .bin")
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        default=Path("firmware-joined.zip"),
        help="Output ZIP path (default: firmware-joined.zip)",
    )
    parser.add_argument(
        "--deflate",
        action="store_true",
        help="Use ZIP DEFLATE (default is STORE for fast on-device extract)",
    )
    parser.add_argument(
        "--version",
        default=None,
        help="Package version string written as version.txt (default: FIRMWARE_VERSION from Defines.h)",
    )
    parser.add_argument(
        "--defines",
        type=Path,
        default=Path("include/Defines.h"),
        help="Path to Defines.h when --version is omitted",
    )
    args = parser.parse_args()

    if not args.host.is_file() or args.host.stat().st_size == 0:
        raise SystemExit(f"host binary missing or empty: {args.host}")
    if not args.slave.is_file() or args.slave.stat().st_size == 0:
        raise SystemExit(f"slave binary missing or empty: {args.slave}")

    version = args.version.strip() if args.version else read_firmware_version(args.defines)
    if not version or len(version) > 15:
        raise SystemExit(f"version must be 1-15 chars, got {version!r}")

    compression = zipfile.ZIP_DEFLATED if args.deflate else zipfile.ZIP_STORED
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(args.output, "w", compression=compression) as archive:
        archive.write(args.slave, arcname="slave.bin")
        archive.write(args.host, arcname="host.bin")
        archive.writestr("version.txt", version + "\n")

    print(f"Wrote {args.output} (slave.bin + host.bin + version.txt={version})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
