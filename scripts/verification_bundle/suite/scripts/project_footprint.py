#!/usr/bin/env python3
from __future__ import annotations

import argparse
import os
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable, List

from common_io import load_structured, write_csv, write_json


LANG_BY_EXT = {
    ".c": "C",
    ".cc": "C++",
    ".cpp": "C++",
    ".cxx": "C++",
    ".h": "C/C++ Header",
    ".hpp": "C/C++ Header",
    ".rs": "Rust",
    ".go": "Go",
    ".java": "Java",
    ".kt": "Kotlin",
    ".swift": "Swift",
    ".cs": "C#",
    ".py": "Python",
    ".js": "JavaScript",
    ".ts": "TypeScript",
    ".sql": "SQL",
}


@dataclass
class RepoStats:
    name: str
    path: Path
    total_files: int = 0
    source_files: int = 0
    test_files: int = 0
    doc_files: int = 0
    source_loc: int = 0
    test_loc: int = 0
    doc_loc: int = 0


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Compute project footprint metrics.")
    p.add_argument("--config", required=True, help="Path to projects.yaml/json")
    p.add_argument("--out-dir", required=True, help="Output directory for reports")
    p.add_argument(
        "--workspace-root",
        default=None,
        help="Verification workspace root (default: parent of scripts dir)",
    )
    p.add_argument(
        "--repo-root",
        default=None,
        help="Repository clone root. Defaults to SB_VERIFY_REPO_ROOT or <workspace-root>/repos",
    )
    return p.parse_args()


def is_test_file(rel: str, hints: dict) -> bool:
    lower = rel.lower()
    for marker in hints.get("dir_patterns", []):
        if marker.lower() in lower:
            return True
    for marker in hints.get("file_patterns", []):
        if marker.lower() in lower:
            return True
    return False


def count_lines(path: Path) -> int:
    try:
        with path.open("r", encoding="utf-8", errors="ignore") as fh:
            return sum(1 for _ in fh)
    except Exception:
        return 0


def iter_files(root: Path, ignore_dirs: Iterable[str]) -> Iterable[Path]:
    ignore = set(ignore_dirs)
    for p in root.rglob("*"):
        if not p.is_file():
            continue
        if any(part in ignore for part in p.parts):
            continue
        yield p


def main() -> None:
    args = parse_args()
    workspace_root = (
        Path(args.workspace_root).resolve()
        if args.workspace_root
        else Path(__file__).resolve().parents[1]
    )
    repo_root = (
        Path(args.repo_root).resolve()
        if args.repo_root
        else Path(os.environ.get("SB_VERIFY_REPO_ROOT", str(workspace_root / "repos"))).resolve()
    )

    out_dir = Path(args.out_dir)
    cfg = load_structured(Path(args.config))

    source_globs: List[str] = cfg.get("source_globs", [])
    source_exts = {Path(g).suffix.lower() for g in source_globs if Path(g).suffix}
    test_hints = cfg.get("test_hints", {})
    ignore_dirs = cfg.get("ignore_dirs", [])

    repo_rows = []
    lang_rows: List[Dict[str, object]] = []
    summary = {
        "total_files": 0,
        "source_files": 0,
        "test_files": 0,
        "doc_files": 0,
        "source_loc": 0,
        "test_loc": 0,
        "doc_loc": 0,
    }

    for repo_cfg in cfg.get("projects", []):
        configured_path = Path(repo_cfg["path"])
        path = configured_path if configured_path.is_absolute() else repo_root / configured_path
        repo = RepoStats(name=repo_cfg["name"], path=path)
        if not repo.path.exists():
            repo_rows.append(
                {
                    "project": repo.name,
                    "path": str(repo.path),
                    "total_files": 0,
                    "source_files": 0,
                    "test_files": 0,
                    "doc_files": 0,
                    "source_loc": 0,
                    "test_loc": 0,
                    "doc_loc": 0,
                    "test_to_source_loc_ratio": 0.0,
                    "status": "missing",
                }
            )
            continue

        lang_totals: Dict[str, Dict[str, int]] = {}

        for f in iter_files(repo.path, ignore_dirs):
            repo.total_files += 1
            rel = str(f.relative_to(repo.path))
            ext = f.suffix.lower()
            loc = count_lines(f)

            if ext in source_exts:
                if is_test_file(rel, test_hints):
                    repo.test_files += 1
                    repo.test_loc += loc
                    bucket = "test"
                else:
                    repo.source_files += 1
                    repo.source_loc += loc
                    bucket = "source"
                lang = LANG_BY_EXT.get(ext, ext or "unknown")
                lang_totals.setdefault(lang, {"source_files": 0, "source_loc": 0, "test_files": 0, "test_loc": 0})
                lang_totals[lang][f"{bucket}_files"] += 1
                lang_totals[lang][f"{bucket}_loc"] += loc
            elif ext in {".md", ".rst", ".txt", ".adoc"}:
                repo.doc_files += 1
                repo.doc_loc += loc

        repo_rows.append(
            {
                "project": repo.name,
                "path": str(repo.path),
                "total_files": repo.total_files,
                "source_files": repo.source_files,
                "test_files": repo.test_files,
                "doc_files": repo.doc_files,
                "source_loc": repo.source_loc,
                "test_loc": repo.test_loc,
                "doc_loc": repo.doc_loc,
                "test_to_source_loc_ratio": (
                    round(repo.test_loc / repo.source_loc, 4) if repo.source_loc else 0.0
                ),
                "status": "ok",
            }
        )

        for lang, counts in sorted(lang_totals.items()):
            lang_rows.append(
                {
                    "project": repo.name,
                    "language": lang,
                    **counts,
                }
            )

        summary["total_files"] += repo.total_files
        summary["source_files"] += repo.source_files
        summary["test_files"] += repo.test_files
        summary["doc_files"] += repo.doc_files
        summary["source_loc"] += repo.source_loc
        summary["test_loc"] += repo.test_loc
        summary["doc_loc"] += repo.doc_loc

    write_csv(out_dir / "footprint_by_repo.csv", repo_rows)
    write_csv(out_dir / "footprint_by_language.csv", lang_rows)
    write_csv(
        out_dir / "footprint_summary.csv",
        [
            {
                **summary,
                "test_to_source_loc_ratio": (
                    round(summary["test_loc"] / summary["source_loc"], 4) if summary["source_loc"] else 0.0
                ),
            }
        ],
    )
    write_json(out_dir / "footprint_snapshot.json", {"summary": summary, "by_repo": repo_rows, "by_language": lang_rows})


if __name__ == "__main__":
    main()
