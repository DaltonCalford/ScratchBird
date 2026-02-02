#!/usr/bin/env python3
# ScratchBird
# Copyright (c) 2025-2026 Dalton Calford
#
# Licensed under the Initial Developer's Public License Version 1.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at:
# https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/

import sys, re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ALLOW_TODO_DIRS = {ROOT / 'docs' / 'change_requests'}
EXCLUDE_DIRS = {
    ROOT / 'build',
    ROOT / 'docs',
    ROOT / 'references' / 'archive',
    ROOT / 'ProjectPlan' / 'old_spec',
    ROOT / 'ProjectPlan' / 'archive',
}
EXCLUDE_FILES = {
    ROOT / 'CRITICAL_REMEDIATION_PLAN.md',
}

BAD_PATH = re.compile(r'`?/workspace/')
BAD_TODO = re.compile(r'\b(TODO|TBD|FIXME)\b')


def is_allowed(path: Path) -> bool:
    for allowed in ALLOW_TODO_DIRS:
        try:
            path.relative_to(allowed)
            return True
        except ValueError:
            continue
    return False


def is_excluded(path: Path) -> bool:
    if path in EXCLUDE_FILES:
        return True
    for d in EXCLUDE_DIRS:
        try:
            path.relative_to(d)
            return True
        except ValueError:
            continue
    return False


def check_file(path: Path) -> list[str]:
    issues = []
    try:
        txt = path.read_text(encoding='utf-8', errors='ignore')
    except Exception:
        return issues
    for i, line in enumerate(txt.splitlines(), 1):
        if BAD_PATH.search(line):
            issues.append(f"{path}:{i}: absolute /workspace/ path")
        if BAD_TODO.search(line) and not is_allowed(path):
            issues.append(f"{path}:{i}: TODO-like token outside change_requests")
    return issues


def main():
    root = ROOT
    exts = {'.md', '.rst', '.txt'}
    problems = []
    for p in root.rglob('*'):
        if not p.is_file():
            continue
        if is_excluded(p):
            continue
        if p.suffix not in exts:
            continue
        problems.extend(check_file(p))
    if problems:
        print("Doc lint issues:")
        for s in problems:
            print(s)
        sys.exit(1)
    print("Doc lint clean")

if __name__ == '__main__':
    main()