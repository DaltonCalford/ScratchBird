#!/usr/bin/env python3
from __future__ import annotations

import argparse
import os
import subprocess
from pathlib import Path
from typing import Dict, List, Set

from common_io import load_structured


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Clone/update ScratchBird verification repositories.")
    p.add_argument("--config", required=True, help="repositories.yaml/json path")
    p.add_argument("--repo-root", default=None, help="override clone root")
    p.add_argument(
        "--preset",
        default="core",
        choices=("core", "full", "scratchbird"),
        help="clone preset when --groups is not provided (default: core)",
    )
    p.add_argument("--groups", default="", help="comma-separated groups to include")
    p.add_argument("--depth", type=int, default=1, help="git clone --depth value (0 = full clone)")
    p.add_argument("--update-existing", action="store_true", help="fetch existing repositories")
    return p.parse_args()


def run(cmd: List[str], cwd: Path | None = None) -> None:
    subprocess.run(cmd, cwd=str(cwd) if cwd else None, check=True)


def should_include(
    entry: Dict[str, object],
    groups: Set[str],
    preset: str,
    core_engine_ids: Set[str],
) -> bool:
    group = str(entry.get("group", ""))
    repo_id = str(entry.get("id", ""))
    if groups:
        return group in groups
    if preset == "full":
        return True
    if preset == "scratchbird":
        return group == "scratchbird"
    # core preset
    return group == "scratchbird" or repo_id in core_engine_ids


def checkout_ref(target: Path, ref: str) -> None:
    run(["git", "checkout", ref], cwd=target)


def main() -> None:
    args = parse_args()
    cfg = load_structured(Path(args.config))
    workspace_root = Path(__file__).resolve().parents[1]
    default_repo_root = Path(cfg.get("defaults", {}).get("repo_root", "repos"))
    repo_root = (
        Path(args.repo_root).resolve()
        if args.repo_root
        else Path(os.environ.get("SB_VERIFY_REPO_ROOT", str(workspace_root / default_repo_root))).resolve()
    )
    repo_root.mkdir(parents=True, exist_ok=True)

    defaults = cfg.get("defaults", {}) or {}
    groups: Set[str] = {x.strip() for x in args.groups.split(",") if x.strip()}
    core_engine_ids: Set[str] = set(defaults.get("core_engine_ids", ["mysql_server", "postgresql", "firebird"]))

    for entry in cfg.get("repositories", []):
        if not should_include(entry, groups=groups, preset=args.preset, core_engine_ids=core_engine_ids):
            continue

        target = repo_root / str(entry["path"])
        url = str(entry["url"])
        depth_args = ["--depth", str(args.depth)] if args.depth and args.depth > 0 else []
        branch = str(entry.get("branch", "")).strip()
        ref = str(entry.get("ref", "")).strip()
        branch_args = ["--branch", branch] if branch else []

        if (target / ".git").exists():
            if args.update_existing:
                print(f"[update] {entry['id']} -> {target}")
                run(["git", "fetch", "--all", "--tags"], cwd=target)
                if ref:
                    checkout_ref(target, ref)
            else:
                print(f"[skip]   {entry['id']} already exists: {target}")
            continue

        print(f"[clone]  {entry['id']} -> {target}")
        run(["git", "clone", *depth_args, *branch_args, url, str(target)])
        if ref:
            checkout_ref(target, ref)


if __name__ == "__main__":
    main()
