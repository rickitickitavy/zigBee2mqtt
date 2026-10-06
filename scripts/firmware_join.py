#!/usr/bin/env python3
"""Join host.bin + slave.bin into a firmware-joined.zip for System → Update."""

from __future__ import annotations

import argparse
import zipfile
from pathlib import Path


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
    args = parser.parse_args()

    if not args.host.is_file() or args.host.stat().st_size == 0:
        raise SystemExit(f"host binary missing or empty: {args.host}")
    if not args.slave.is_file() or args.slave.stat().st_size == 0:
        raise SystemExit(f"slave binary missing or empty: {args.slave}")

    compression = zipfile.ZIP_DEFLATED if args.deflate else zipfile.ZIP_STORED
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(args.output, "w", compression=compression) as archive:
        archive.write(args.slave, arcname="slave.bin")
        archive.write(args.host, arcname="host.bin")

    print(f"Wrote {args.output} (slave.bin + host.bin)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
