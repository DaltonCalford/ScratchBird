#!/usr/bin/env python3
import sys, re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ALLOW_TODO_DIRS = {ROOT / 'docs' / 'change_requests'}

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
    targets = []
    for p in root.rglob('*'):
        if p.is_file() and p.suffix in exts:
            targets.append(p)
    problems = []
    for p in targets:
        problems.extend(check_file(p))
    if problems:
        print("Doc lint issues:")
        for s in problems:
            print(s)
        sys.exit(1)
    print("Doc lint clean")

if __name__ == '__main__':
    main()