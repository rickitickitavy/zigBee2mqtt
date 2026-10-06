#!/usr/bin/env python3
"""Split a joined firmware ZIP back into host.bin and slave.bin."""

from __future__ import annotations

import argparse
import zipfile
from pathlib import Path


REQUIRED = ("slave.bin", "host.bin")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package", type=Path, help="Joined firmware ZIP")
    parser.add_argument(
        "-o",
        "--output-dir",
        type=Path,
        default=Path("."),
        help="Directory for extracted host.bin and slave.bin",
    )
    args = parser.parse_args()

    if not args.package.is_file():
        raise SystemExit(f"package not found: {args.package}")

    args.output_dir.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(args.package, "r") as archive:
        names = set(archive.namelist())
        missing = [name for name in REQUIRED if name not in names]
        if missing:
            raise SystemExit(f"missing ZIP members: {', '.join(missing)}")
        for name in REQUIRED:
            target = args.output_dir / name
            target.write_bytes(archive.read(name))
            if target.stat().st_size == 0:
                raise SystemExit(f"extracted empty member: {name}")

    print(f"Wrote {args.output_dir / 'slave.bin'} and {args.output_dir / 'host.bin'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
