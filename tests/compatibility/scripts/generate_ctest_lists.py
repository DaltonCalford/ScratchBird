#!/usr/bin/env python3
"""
Generate curated/expanded/full CTest list files for compatibility lanes.

Outputs:
  - tests/compatibility/<engine>/config/ctest_list_expanded.txt
  - tests/compatibility/<engine>/config/ctest_list_full.txt
  - tests/compatibility/CTEST_LIST_SUMMARY.md
"""

from __future__ import annotations

import argparse
import json
import math
import re
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Dict, Iterable, List, Sequence, Tuple


@dataclass(frozen=True)
class EngineConfig:
    name: str
    converted_dir: Path
    config_dir: Path
    curated_list: Path
    expanded_list: Path
    full_list: Path
    results_dir: Path


@dataclass
class RuntimeModel:
    seconds_per_test: float
    source: str
    latest_run: str


FALLBACK_SECONDS_PER_TEST: Dict[str, float] = {
    "firebird": 0.9,
    "mysql": 10.0,
    "postgresql": 12.0,
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate expanded/full compatibility ctest lists.")
    parser.add_argument(
        "--compat-root",
        default=str(Path(__file__).resolve().parents[1]),
        help="Path to tests/compatibility root.",
    )
    parser.add_argument(
        "--expanded-ratio",
        type=float,
        default=0.35,
        help="Per-suite ratio for expanded lists (0 < ratio <= 1).",
    )
    parser.add_argument(
        "--expanded-max-per-suite",
        type=int,
        default=400,
        help="Cap for expanded list entries per top-level suite.",
    )
    return parser.parse_args()


def load_list(path: Path) -> List[str]:
    if not path.exists():
        return []
    entries: List[str] = []
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        entries.append(line)
    return entries


def is_sql_compatible_test(engine: str, sql_file: Path) -> bool:
    if engine != "firebird":
        return True

    test_type_pattern = re.compile(r"^\s*--\s*Test Type:\s*(.+?)\s*$", re.IGNORECASE)
    try:
        with sql_file.open("r", encoding="utf-8", errors="replace") as handle:
            for idx, line in enumerate(handle):
                if idx > 160:
                    break
                match = test_type_pattern.match(line)
                if not match:
                    continue
                test_type = match.group(1).strip().lower()
                return "sql" in test_type
    except OSError:
        return False
    return True


def discover_sql_tests(engine: str, converted_dir: Path) -> List[str]:
    if not converted_dir.exists():
        return []

    rel_paths: List[str] = []
    for sql_file in sorted(converted_dir.rglob("*.sql")):
        if not is_sql_compatible_test(engine, sql_file):
            continue
        rel = sql_file.relative_to(converted_dir).as_posix()
        if "/expected/" in f"/{rel}/":
            continue
        rel_paths.append(rel)
    return rel_paths


def top_level_suite(rel_path: str) -> str:
    parts = rel_path.split("/")
    if len(parts) <= 1:
        return "main"
    return parts[0]


def build_expanded_list(
    all_tests: Sequence[str],
    curated: Sequence[str],
    ratio: float,
    max_per_suite: int,
) -> List[str]:
    by_suite: Dict[str, List[str]] = {}
    for test in all_tests:
        by_suite.setdefault(top_level_suite(test), []).append(test)

    selected = set(curated)
    for suite, tests in sorted(by_suite.items()):
        count = max(1, math.ceil(len(tests) * ratio))
        count = min(count, max_per_suite, len(tests))
        for test in tests[:count]:
            selected.add(test)

    return sorted(selected)


def latest_results_dir(results_root: Path) -> Path | None:
    if not results_root.exists():
        return None
    candidates = [p for p in results_root.iterdir() if p.is_dir()]
    if not candidates:
        return None
    return sorted(candidates, reverse=True)[0]


def mtime_bounds(path: Path) -> Tuple[float | None, float | None]:
    mtimes: List[float] = []
    for item in path.rglob("*"):
        try:
            if item.is_file():
                mtimes.append(item.stat().st_mtime)
        except FileNotFoundError:
            continue
    if not mtimes:
        return None, None
    return min(mtimes), max(mtimes)


def derive_runtime_model(engine: str, results_root: Path) -> RuntimeModel:
    latest = latest_results_dir(results_root)
    if latest is None:
        return RuntimeModel(
            seconds_per_test=FALLBACK_SECONDS_PER_TEST[engine],
            source="fallback",
            latest_run="n/a",
        )

    manifest_path = latest / "RUN_MANIFEST.json"
    if not manifest_path.exists():
        return RuntimeModel(
            seconds_per_test=FALLBACK_SECONDS_PER_TEST[engine],
            source="fallback",
            latest_run=latest.name,
        )

    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError:
        return RuntimeModel(
            seconds_per_test=FALLBACK_SECONDS_PER_TEST[engine],
            source="fallback",
            latest_run=latest.name,
        )

    listed_tests = int(manifest.get("listed_tests", 0) or 0)
    start_mtime, end_mtime = mtime_bounds(latest)

    if listed_tests > 0 and start_mtime is not None and end_mtime is not None:
        elapsed = end_mtime - start_mtime
        if elapsed > 0:
            per_test = max(0.01, elapsed / listed_tests)
            return RuntimeModel(
                seconds_per_test=per_test,
                source="latest_run",
                latest_run=latest.name,
            )

    return RuntimeModel(
        seconds_per_test=FALLBACK_SECONDS_PER_TEST[engine],
        source="fallback",
        latest_run=latest.name,
    )


def format_duration(seconds: float) -> str:
    total = int(round(seconds))
    hours, rem = divmod(total, 3600)
    minutes, secs = divmod(rem, 60)
    if hours:
        return f"{hours}h {minutes}m {secs}s"
    if minutes:
        return f"{minutes}m {secs}s"
    return f"{secs}s"


def write_list(path: Path, header: Sequence[str], entries: Iterable[str]) -> None:
    lines = list(header)
    lines.extend(entries)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    args = parse_args()
    compat_root = Path(args.compat_root).resolve()

    if not (0 < args.expanded_ratio <= 1):
        raise SystemExit("--expanded-ratio must be > 0 and <= 1")
    if args.expanded_max_per_suite <= 0:
        raise SystemExit("--expanded-max-per-suite must be > 0")

    engines: List[EngineConfig] = []
    for engine in ("firebird", "mysql", "postgresql"):
        config_dir = compat_root / engine / "config"
        engines.append(
            EngineConfig(
                name=engine,
                converted_dir=compat_root / engine / "converted",
                config_dir=config_dir,
                curated_list=config_dir / "ctest_list.txt",
                expanded_list=config_dir / "ctest_list_expanded.txt",
                full_list=config_dir / "ctest_list_full.txt",
                results_dir=compat_root / engine / "results" / "ctest",
            )
        )

    now = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    report_rows: List[Tuple[str, int, int, int, RuntimeModel]] = []

    for cfg in engines:
        cfg.config_dir.mkdir(parents=True, exist_ok=True)

        all_tests = discover_sql_tests(cfg.name, cfg.converted_dir)
        curated = load_list(cfg.curated_list)

        expanded = build_expanded_list(
            all_tests=all_tests,
            curated=curated,
            ratio=args.expanded_ratio,
            max_per_suite=args.expanded_max_per_suite,
        )

        full_list = sorted(set(all_tests))

        expanded_header = [
            f"# {cfg.name} compatibility expanded CTest list",
            "# Auto-generated by scripts/generate_ctest_lists.py",
            f"# generated_utc={now}",
            f"# expanded_ratio={args.expanded_ratio}",
            f"# expanded_max_per_suite={args.expanded_max_per_suite}",
            f"# total_entries={len(expanded)}",
            "",
        ]
        full_header = [
            f"# {cfg.name} compatibility full CTest list",
            "# Auto-generated by scripts/generate_ctest_lists.py",
            f"# generated_utc={now}",
            f"# total_entries={len(full_list)}",
            "",
        ]

        write_list(cfg.expanded_list, expanded_header, expanded)
        write_list(cfg.full_list, full_header, full_list)

        runtime_model = derive_runtime_model(cfg.name, cfg.results_dir)
        report_rows.append((cfg.name, len(curated), len(expanded), len(full_list), runtime_model))

    summary_path = compat_root / "CTEST_LIST_SUMMARY.md"
    summary_lines: List[str] = []
    summary_lines.append("# Compatibility CTest List Summary")
    summary_lines.append("")
    summary_lines.append(f"Generated: {now}")
    summary_lines.append(f"Expanded ratio: `{args.expanded_ratio}`")
    summary_lines.append(f"Expanded max per suite: `{args.expanded_max_per_suite}`")
    summary_lines.append("")
    summary_lines.append("| Engine | Curated | Expanded | Full | Runtime Model | Est. Curated | Est. Expanded | Est. Full |")
    summary_lines.append("|--------|--------:|---------:|-----:|---------------|-------------:|--------------:|----------:|")

    for name, curated_count, expanded_count, full_count, model in report_rows:
        est_curated = format_duration(curated_count * model.seconds_per_test)
        est_expanded = format_duration(expanded_count * model.seconds_per_test)
        est_full = format_duration(full_count * model.seconds_per_test)
        model_desc = f"{model.source} ({model.latest_run}, {model.seconds_per_test:.2f}s/test)"
        summary_lines.append(
            f"| {name} | {curated_count} | {expanded_count} | {full_count} | {model_desc} | {est_curated} | {est_expanded} | {est_full} |"
        )

    summary_lines.append("")
    summary_lines.append("Notes:")
    summary_lines.append("- Runtime estimates are rough planning values, not hard guarantees.")
    summary_lines.append("- Models use latest engine run data when available; otherwise fallback constants.")
    summary_lines.append("- CTest list mode is selected in runners via `SCRATCHBIRD_COMPAT_CTEST_LIST_MODE` or per-engine override.")

    summary_path.write_text("\n".join(summary_lines) + "\n", encoding="utf-8")

    print("Generated expanded/full CTest lists:")
    for cfg in engines:
        print(f"  - {cfg.expanded_list}")
        print(f"  - {cfg.full_list}")
    print(f"Summary: {summary_path}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
