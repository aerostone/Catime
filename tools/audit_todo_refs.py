#!/usr/bin/env python3
"""Static reference audit for the TODO/sticky modules.

No Windows toolchain is available locally, so instead of compiling we
check, per translation unit:

1. every project-style call (Todo*/Board*/Sticky*/Cmd*/...) is either
   defined in a project .c file or declared in a reachable project
   header,
2. every project-style macro (TODO_*/BOARD_*/STICKY_*/IDC_TODO_*/...) is
   defined somewhere in the project,
3. no stale references to files that were removed.

Exit code 0 = clean.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC_DIRS = ["src", "include", "resource"]

PREFIXES = (
    "Todo", "Task", "Board", "Sticky", "Cmd", "BuildTodo", "Dialog_",
    "DialogModern", "HandleTodo", "TodoDlg", "TodoSync", "TodoStore",
    "TodoConflict", "TodoRowMark", "TodoTxt", "TodoNormalize", "TodoUiDebug",
    "TodoFilter", "TodoTask", "TodoSticky", "TodoStickies", "TodoBoard",
    "InputBox", "Language_",
)
ENUM_RE = re.compile('typedef\\s+enum\\b[^{]*\\{(.*?)\\}', re.S)
ENUM_MEMBER_RE = re.compile(r"\b([A-Z][A-Z0-9_]{2,})\b")
MACRO_RE = re.compile(r"^#define\s+([A-Z][A-Z0-9_]{2,})\b", re.M)
DEF_RE = re.compile(
    r"^[ \t]*[A-Za-z_][\w \t\*]*?\b([A-Z][A-Za-z0-9_]*)\s*\(", re.M)
CALL_RE = re.compile(r"\b([A-Z][A-Za-z0-9_]*)\s*\(")
INC_RE = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.M)


def collect(paths, pattern):
    out = set()
    for p in paths:
        try:
            txt = open(p, encoding="utf-8", errors="replace").read()
        except OSError:
            continue
        out.update(pattern.findall(txt))
    return out


def walk():
    c_files, h_files, all_files = [], [], []
    for d in SRC_DIRS:
        base = os.path.join(ROOT, d)
        for root, _dirs, files in os.walk(base):
            for f in files:
                p = os.path.join(root, f)
                all_files.append(p)
                if f.endswith(".c"):
                    c_files.append(p)
                elif f.endswith(".h"):
                    h_files.append(p)
    return c_files, h_files, all_files


def includes_of(path, all_files):
    """Direct project includes of one file, resolved to absolute paths."""
    txt = open(path, encoding="utf-8", errors="replace").read()
    out = []
    for inc in INC_RE.findall(txt):
        tail = inc.replace("\\", "/")
        matches = [p for p in all_files if p.replace("\\", "/").endswith(
            "/" + tail) or os.path.basename(p) == os.path.basename(tail)]
        if matches:
            out.append(matches[0])
    return out


def reachable_headers(path, all_files, _seen=None):
    seen = _seen if _seen is not None else set()
    for inc in includes_of(path, all_files):
        if inc in seen or not inc.endswith(".h"):
            continue
        seen.add(inc)
        reachable_headers(inc, all_files, seen)
    return seen


def main():
    c_files, h_files, all_files = walk()
    defined = collect(c_files, DEF_RE)
    declared = collect(h_files, DEF_RE)
    macros = collect(all_files, MACRO_RE)
    for h in h_files:
        txt = open(h, encoding="utf-8", errors="replace").read()
        for block in ENUM_RE.findall(txt):
            macros |= set(ENUM_MEMBER_RE.findall(block))

    known = defined | declared
    errors = []

    todo_tus = []
    for p in c_files:
        txt = open(p, encoding="utf-8", errors="replace").read()
        if ("Todo" in txt or "Sticky" in txt or "todo" in os.path.basename(p)
                or "/todo/" in p):
            todo_tus.append(p)
    for tu in todo_tus:
        txt = open(tu, encoding="utf-8", errors="replace").read()
        hdrs = reachable_headers(tu, all_files)
        local_decl = set()
        for h in hdrs:
            local_decl |= set(DEF_RE.findall(
                open(h, encoding="utf-8", errors="replace").read()))
        for name in sorted(set(CALL_RE.findall(txt))):
            if not name.startswith(PREFIXES):
                continue
            own = set(DEF_RE.findall(txt))
            if name in local_decl or name in own:
                continue
            where = "defined elsewhere without a reachable declaration" \
                if name in defined else "no definition/declaration"
            errors.append(f"{os.path.relpath(tu, ROOT)}: call {name}() "
                          f"{where}")
        for name in sorted(set(re.findall(r"\b(TODO_[A-Z0-9_]+|BOARD_[A-Z0-9_]+"
                                          r"|STICKY_[A-Z0-9_]+|IDC_TODO_[A-Z0-9_]+"
                                          r"|DIALOG_INSTANCE_[A-Z0-9_]+)\b", txt))):
            if name in macros:
                continue
            errors.append(f"{os.path.relpath(tu, ROOT)}: macro {name} "
                          f"undefined in project")

    # stale file references
    stale = ["todo_sticky_rows.h", "todo_stickies_render", "todo_stickies_pin",
             "todo_stickies_edit"]
    for p in todo_tus:
        txt = open(p, encoding="utf-8", errors="replace").read()
        for s in stale:
            if s in txt:
                errors.append(f"{os.path.relpath(p, ROOT)}: stale ref {s}")

    if errors:
        print("\n".join(errors))
        print(f"\nAUDIT_FAIL {len(errors)} issues")
        return 1
    print(f"AUDIT_OK {len(todo_tus)} TUs, {len(known)} project symbols")
    return 0


if __name__ == "__main__":
    sys.exit(main())
