#!/usr/bin/env python3
from __future__ import annotations

import argparse
import os
import time
from pathlib import Path
from typing import Dict, List, Mapping, Tuple

from common_io import ensure_dir, load_structured, write_csv, write_json
from db_adapters import (
    compare_assertions,
    resolve_client_binary,
    resolve_sql_path,
    run_sql_file,
)


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Differential SQL result comparer.")
    p.add_argument("--engines", required=True, help="engines.yaml/json")
    p.add_argument("--cases", required=True, help="case_index.yaml/json")
    p.add_argument("--out-dir", required=True, help="result output directory")
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


def resolve_paths(args: argparse.Namespace) -> Tuple[Path, Path]:
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
    return workspace_root, repo_root


def lane_for_engine(engine: Mapping[str, object]) -> str:
    if engine.get("lane"):
        return str(engine["lane"])
    if engine.get("emulates"):
        return str(engine["emulates"])
    if engine.get("engine"):
        return str(engine["engine"])
    return str(engine.get("mode", "unknown"))


def build_pairs(engines: List[Mapping[str, object]]) -> List[Tuple[Mapping[str, object], Mapping[str, object], str]]:
    refs_by_lane: Dict[str, Mapping[str, object]] = {}
    for e in engines:
        if str(e.get("family")) == "reference":
            refs_by_lane[lane_for_engine(e)] = e

    pairs: List[Tuple[Mapping[str, object], Mapping[str, object], str]] = []
    for e in engines:
        if str(e.get("family")) != "scratchbird":
            continue
        if str(e.get("mode")) != "emulated":
            continue
        lane = lane_for_engine(e)
        ref = refs_by_lane.get(lane)
        if ref:
            pairs.append((e, ref, lane))
    return pairs


def case_surface(case: Mapping[str, object], lane: str) -> Mapping[str, object] | None:
    lanes = case.get("lanes", [])
    if lanes and lane not in lanes:
        return None
    surfaces = case.get("surfaces", {})
    return surfaces.get(lane)


def evaluate_expectation(
    expectation: str,
    order_sensitive: bool,
    sb_exec: Mapping[str, object],
    ref_exec: Mapping[str, object],
) -> Tuple[bool, str]:
    sb_status = str(sb_exec.get("status", "error"))
    ref_status = str(ref_exec.get("status", "error"))
    sb_sqlstate = str(sb_exec.get("sqlstate", ""))
    ref_sqlstate = str(ref_exec.get("sqlstate", ""))
    sb_asserts = list(sb_exec.get("assert_lines", []) or [])
    ref_asserts = list(ref_exec.get("assert_lines", []) or [])

    if expectation == "must_fail_same_class":
        if sb_status == "ok" or ref_status == "ok":
            return False, f"expected both fail, got sb={sb_status} ref={ref_status}"
        if sb_sqlstate[:2] != ref_sqlstate[:2]:
            return False, f"sqlstate class mismatch sb={sb_sqlstate} ref={ref_sqlstate}"
        return True, ""

    if expectation == "must_match":
        if sb_status != "ok" or ref_status != "ok":
            return False, f"expected both ok, got sb={sb_status} ref={ref_status}"
        ok, reason = compare_assertions(sb_asserts, ref_asserts, order_sensitive=order_sensitive)
        if not ok:
            return False, reason
        return True, ""

    if expectation == "sb_extension":
        return True, "sb_extension skipped"

    return False, f"unsupported expectation: {expectation}"


def run_case_on_engine(
    engine: Mapping[str, object],
    binary: str,
    surface: Mapping[str, object],
    workspace_root: Path,
    run_dir: Path,
    timeout_seconds: int,
) -> Dict[str, object]:
    setup_sql = resolve_sql_path(str(surface["setup"]), workspace_root)
    exec_sql = resolve_sql_path(str(surface["exec"]), workspace_root)
    teardown_sql = resolve_sql_path(str(surface["teardown"]), workspace_root)

    setup_res = run_sql_file(engine, binary, setup_sql, run_dir / "raw", "setup", timeout_seconds)
    exec_res = run_sql_file(engine, binary, exec_sql, run_dir / "raw", "exec", timeout_seconds)
    teardown_res = run_sql_file(engine, binary, teardown_sql, run_dir / "raw", "teardown", timeout_seconds)

    return {
        "setup": setup_res.__dict__,
        "exec": exec_res.__dict__,
        "teardown": teardown_res.__dict__,
    }


def write_junit(path: Path, summary: Mapping[str, int], failures: List[Mapping[str, object]]) -> None:
    ensure_dir(path.parent)
    tests = int(summary.get("total_comparisons", 0))
    failed = int(summary.get("failed", 0))
    lines = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        f'<testsuite name="compat_diff" tests="{tests}" failures="{failed}" errors="0" skipped="0">',
    ]
    if not failures:
        lines.append('<testcase classname="compat_diff" name="all_passed"/>')
    else:
        for i, row in enumerate(failures, start=1):
            lines.append(f'<testcase classname="compat_diff" name="failure_{i}">')
            msg = str(row.get("reason", "unknown failure")).replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
            lines.append(f'<failure message="{msg}"/>')
            lines.append("</testcase>")
    lines.append("</testsuite>")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    args = parse_args()
    workspace_root, repo_root = resolve_paths(args)
    engines_cfg = load_structured(Path(args.engines))
    cases_cfg = load_structured(Path(args.cases))
    out_dir = Path(args.out_dir)
    ensure_dir(out_dir)

    timeout_seconds = int(engines_cfg.get("defaults", {}).get("timeout_seconds", 300))
    enabled = [e for e in engines_cfg.get("engines", []) if e.get("enabled", True)]
    pairs = build_pairs(enabled)

    run_id = time.strftime("%Y%m%d_%H%M%S")
    run_dir = out_dir / run_id
    ensure_dir(run_dir / "raw")

    raw_rows: List[Dict[str, object]] = []
    fail_rows: List[Dict[str, object]] = []
    summary = {"total_comparisons": 0, "passed": 0, "failed": 0, "pairs": len(pairs)}

    for sb_engine, ref_engine, lane in pairs:
        sb_bin = resolve_client_binary(sb_engine, repo_root)
        ref_bin = resolve_client_binary(ref_engine, repo_root)

        for case in cases_cfg.get("cases", []):
            expectation = str(case.get("expectation", "must_match"))
            if expectation == "sb_extension":
                continue

            surface = case_surface(case, lane)
            if not surface:
                continue

            summary["total_comparisons"] += 1
            order_sensitive = bool(case.get("compare", {}).get("order_sensitive", False))

            case_dir = run_dir / "raw" / str(case["id"]) / lane
            ensure_dir(case_dir)

            sb_run = run_case_on_engine(sb_engine, sb_bin, surface, workspace_root, case_dir / "sb", timeout_seconds)
            ref_run = run_case_on_engine(ref_engine, ref_bin, surface, workspace_root, case_dir / "ref", timeout_seconds)

            sb_setup_status = str(sb_run["setup"].get("status", "error"))
            ref_setup_status = str(ref_run["setup"].get("status", "error"))
            sb_teardown_status = str(sb_run["teardown"].get("status", "error"))
            ref_teardown_status = str(ref_run["teardown"].get("status", "error"))

            if sb_setup_status != "ok" or ref_setup_status != "ok":
                ok, reason = False, (
                    f"setup failed sb={sb_setup_status} ref={ref_setup_status}"
                )
            else:
                ok, reason = evaluate_expectation(
                    expectation=expectation,
                    order_sensitive=order_sensitive,
                    sb_exec=sb_run["exec"],
                    ref_exec=ref_run["exec"],
                )
                if ok and (sb_teardown_status != "ok" or ref_teardown_status != "ok"):
                    ok, reason = False, (
                        f"teardown failed sb={sb_teardown_status} ref={ref_teardown_status}"
                    )

            if ok:
                summary["passed"] += 1
            else:
                summary["failed"] += 1
                fail_rows.append(
                    {
                        "case_id": case["id"],
                        "lane": lane,
                        "sb_engine": sb_engine["id"],
                        "ref_engine": ref_engine["id"],
                        "reason": reason,
                    }
                )

            detail = {
                "case_id": case["id"],
                "lane": lane,
                "expectation": expectation,
                "sb_engine": sb_engine["id"],
                "ref_engine": ref_engine["id"],
                "result": "pass" if ok else "fail",
                "reason": reason,
                "sb_exec_status": sb_run["exec"]["status"],
                "ref_exec_status": ref_run["exec"]["status"],
                "sb_setup_status": sb_setup_status,
                "ref_setup_status": ref_setup_status,
                "sb_teardown_status": sb_teardown_status,
                "ref_teardown_status": ref_teardown_status,
                "sb_exec_sqlstate": sb_run["exec"]["sqlstate"],
                "ref_exec_sqlstate": ref_run["exec"]["sqlstate"],
                "sb_exec_elapsed_ms": sb_run["exec"]["elapsed_ms"],
                "ref_exec_elapsed_ms": ref_run["exec"]["elapsed_ms"],
                "sb_assert_count": len(sb_run["exec"]["assert_lines"]),
                "ref_assert_count": len(ref_run["exec"]["assert_lines"]),
            }
            raw_rows.append(detail)

            write_json(case_dir / "sb_run.json", sb_run)
            write_json(case_dir / "ref_run.json", ref_run)
            write_json(case_dir / "comparison.json", detail)

    write_csv(run_dir / "diff_summary.csv", [summary])
    write_csv(run_dir / "diff_results.csv", raw_rows)
    write_csv(run_dir / "diff_failures.csv", fail_rows)
    write_json(run_dir / "diff_results.json", {"summary": summary, "results": raw_rows, "failures": fail_rows})
    write_junit(run_dir / "diff.xml", summary, fail_rows)


if __name__ == "__main__":
    main()
