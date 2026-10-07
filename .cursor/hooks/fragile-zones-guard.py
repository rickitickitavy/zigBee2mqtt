#!/usr/bin/env python3
"""Deny agent edits to hard-frozen fragile zones unless temporarily allowed."""

from __future__ import annotations

import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
ZONES_PATH = ROOT / ".cursor" / "fragile-zones.json"
ALLOW_PATH = ROOT / ".cursor" / "fragile-zones.allow"


def load_zones() -> list[dict]:
    if not ZONES_PATH.is_file():
        return []
    data = json.loads(ZONES_PATH.read_text(encoding="utf-8"))
    return list(data.get("zones") or [])


def load_allows() -> set[str]:
    if not ALLOW_PATH.is_file():
        return set()
    allowed: set[str] = set()
    for raw in ALLOW_PATH.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        allowed.add(line)
    return allowed


def resolve_hard_paths(zones: list[dict]) -> dict[str, str]:
    """Map relative hard path -> zone id (first owner wins for messaging)."""
    by_id = {zone.get("id"): zone for zone in zones if zone.get("id")}
    path_to_zone: dict[str, str] = {}

    def add_zone(zone_id: str, seen: set[str]) -> None:
        if zone_id in seen:
            return
        seen.add(zone_id)
        zone = by_id.get(zone_id)
        if zone is None:
            return
        for included in zone.get("includesZones") or []:
            add_zone(str(included), seen)
        for rel in zone.get("hardPaths") or []:
            path_to_zone.setdefault(str(rel).replace("\\", "/"), zone_id)

    for zone_id in by_id:
        add_zone(zone_id, set())
    return path_to_zone


def normalize_repo_path(raw: str | None) -> str | None:
    if not raw:
        return None
    text = str(raw).strip().replace("\\", "/")
    if not text:
        return None
    path = Path(text)
    try:
        if path.is_absolute():
            resolved = path.resolve()
            return str(resolved.relative_to(ROOT.resolve())).replace("\\", "/")
    except Exception:
        pass
    while text.startswith("./"):
        text = text[2:]
    prefix = str(ROOT.resolve()).replace("\\", "/").rstrip("/") + "/"
    if text.startswith(prefix):
        text = text[len(prefix) :]
    return text.lstrip("/")


def extract_paths(payload: dict) -> list[str]:
    tool_input = payload.get("tool_input") or payload.get("input") or {}
    if not isinstance(tool_input, dict):
        tool_input = {}
    candidates = [
        tool_input.get("path"),
        tool_input.get("file_path"),
        tool_input.get("filePath"),
        tool_input.get("target_notebook"),
    ]
    paths: list[str] = []
    for item in candidates:
        normalized = normalize_repo_path(item if isinstance(item, str) else None)
        if normalized:
            paths.append(normalized)
    return paths


def main() -> int:
    try:
        payload = json.load(sys.stdin)
    except Exception:
        # Fail open on unreadable input so unrelated tools are not bricked.
        print(json.dumps({"permission": "allow"}))
        return 0

    tool_name = str(payload.get("tool_name") or payload.get("toolName") or "")
    if tool_name and tool_name not in ("Write", "StrReplace", "Delete", "EditNotebook"):
        print(json.dumps({"permission": "allow"}))
        return 0

    path_to_zone = resolve_hard_paths(load_zones())
    allows = load_allows()
    for rel_path in extract_paths(payload):
        zone_id = path_to_zone.get(rel_path)
        if zone_id is None:
            continue
        if zone_id in allows or rel_path in allows:
            continue
        message = (
            f"Fragile zone `{zone_id}` is frozen. Refusing edit to `{rel_path}`. "
            f"Ask the human for approval, then add `{zone_id}` or `{rel_path}` to "
            f".cursor/fragile-zones.allow (see docs/FRAGILE_ZONES.md)."
        )
        print(
            json.dumps(
                {
                    "permission": "deny",
                    "user_message": message,
                    "agent_message": message,
                }
            )
        )
        return 0

    print(json.dumps({"permission": "allow"}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
