#!/usr/bin/env python3
"""Reject duplicate command IDs that silently kill features.

Command IDs are dispatched by a linear scan of COMMAND_DISPATCH_TABLE, so the
first matching entry wins and any later entry sharing the same number is dead
code that never runs. That is exactly how "task-book manager" stopped opening
anything: CLOCK_IDM_TODO_BOARDS was defined as 151, the same value as the
pre-existing CLOCK_IDM_24HOUR_FORMAT, and the 24-hour handler swallowed the
click.

Only CLOCK_IDM_* command ids are checked. CLOCK_IDC_*/IDC_* are control ids
scoped to one dialog, so two dialogs may legitimately reuse a number.

Two checks:
  1. Two *used* CLOCK_IDM_* names must not share a number.
  2. COMMAND_DISPATCH_TABLE must not contain two entries with the same id.
"""

from __future__ import annotations

import re
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]

DEFINE_RE = re.compile(
    r"^\s*#\s*define\s+(CLOCK_IDM_[A-Za-z0-9_]*)\s+(\d+)\b",
    re.MULTILINE,
)
DISPATCH_ROW_RE = re.compile(
    r"\{\s*([A-Za-z_][A-Za-z0-9_]*)\s*,\s*[A-Za-z_][A-Za-z0-9_]*\s*\}",
)

SOURCE_SUFFIXES = {".c", ".h", ".rc", ".py", ".cmake", ".txt", ".md"}

# Shared numbers that are intentional range boundaries rather than collisions.
ALLOWED_SHARED = {
    "4000": "CLOCK_IDM_ANIMATIONS_END is the exclusive end of the animation "
    "range and equals CLOCK_IDM_PLUGINS_BASE, the inclusive start of the "
    "plugin range; lookups use '< END' vs '>= BASE' so id 4000 is unique",
}


def definition_headers() -> list[Path]:
    headers = []
    for header in sorted((ROOT / "resource").rglob("*.h")):
        headers.append(header)
    return headers


def gather_definitions() -> dict[str, list[tuple[str, str]]]:
    """value -> [(name, file)] for every CLOCK_IDM_* define."""
    by_value: dict[str, list[tuple[str, str]]] = defaultdict(list)
    for header in definition_headers():
        text = header.read_text(encoding="utf-8", errors="replace")
        rel = header.relative_to(ROOT).as_posix()
        for name, value in DEFINE_RE.findall(text):
            by_value[value].append((name, rel))
    return by_value


def gather_references(names: set[str]) -> set[str]:
    """CLOCK_IDM_* names used outside the resource definition headers."""
    used: set[str] = set()
    patterns = {name: re.compile(r"\b" + re.escape(name) + r"\b") for name in names}
    for root_name in ("src", "include", "tests", "resource"):
        root = ROOT / root_name
        if not root.is_dir():
            continue
        for path in root.rglob("*"):
            if not path.is_file() or path.suffix not in SOURCE_SUFFIXES:
                continue
            rel = path.relative_to(ROOT)
            # The definition headers are the only place a name may appear
            # without being a real use site.
            if rel.parts[0] == "resource" and path.suffix == ".h":
                continue
            text = path.read_text(encoding="utf-8", errors="replace")
            for name, pattern in patterns.items():
                if name not in used and pattern.search(text):
                    used.add(name)
    return used


def check_definitions() -> list[str]:
    by_value = gather_definitions()
    names = {name for entries in by_value.values() for name, _ in entries}
    used = gather_references(names)

    problems: list[str] = []
    for value, entries in sorted(by_value.items(), key=lambda kv: int(kv[0])):
        live = sorted({name for name, _ in entries if name in used})
        if len(live) < 2 or value in ALLOWED_SHARED:
            continue
        files = sorted({f for name, f in entries if name in live})
        problems.append(
            "id %s is shared by %d live command names (%s) in %s"
            % (value, len(live), ", ".join(live), ", ".join(files))
        )
    return problems


def check_dispatch_table() -> list[str]:
    path = ROOT / "src/window_procedure/window_command_dispatch.c"
    if not path.is_file():
        return ["missing %s" % path.relative_to(ROOT)]

    defines: dict[str, str] = {}
    for header in definition_headers():
        for name, value in DEFINE_RE.findall(
            header.read_text(encoding="utf-8", errors="replace")
        ):
            defines.setdefault(name, value)

    seen: dict[str, int] = defaultdict(int)
    for name in DISPATCH_ROW_RE.findall(path.read_text(encoding="utf-8", errors="replace")):
        seen[name] += 1

    problems = [
        "dispatch table lists %s more than once" % name
        for name, count in sorted(seen.items())
        if count > 1
    ]

    by_id: dict[str, list[str]] = defaultdict(list)
    for name in sorted(seen):
        if name in defines:
            by_id[defines[name]].append(name)
    for value, list_names in sorted(by_id.items(), key=lambda kv: int(kv[0])):
        if len(list_names) > 1:
            problems.append(
                "dispatch table entries %s all resolve to id %s"
                % (", ".join(list_names), value)
            )
    return problems


def main() -> int:
    problems = check_definitions() + check_dispatch_table()
    if problems:
        print("Command id policy failed:")
        for problem in problems:
            print("  - %s" % problem)
        print(
            "\nGive each command its own number (see the CLOCK_IDM_* band in "
            "resource/resource_app_ids.h); a duplicate makes the later entry "
            "unreachable."
        )
        return 1
    print("Command id policy passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
