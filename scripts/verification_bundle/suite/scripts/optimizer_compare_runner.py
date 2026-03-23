#!/usr/bin/env python3
from __future__ import annotations

import argparse
import os
import time
from pathlib import Path
from typing import Dict, List, Mapping, Tuple

from common_io import ensure_dir, load_structured, write_csv, write_json
from db_adapters import compare_assertions, resolve_client_binary, resolve_sql_path, run_sql_file
from package_scaffold import discover_engine_package_scaffold


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Optimizer donor comparison runner.")
    p.add_argument("--engines", required=True, help="optimizer_donor_engines.yaml/json")
    p.add_argument("--corpus", required=True, help="optimizer_donor_corpus.yaml/json")
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


def surface_key_for_engine(engine: Mapping[str, object]) -> str:
    family = str(engine.get("family", ""))
    mode = str(engine.get("mode", ""))
    if family == "scratchbird" and mode == "native":
        return "native"
    return lane_for_engine(engine)


def extract_prefixed_lines(text: str, prefix: str) -> List[str]:
    out: List[str] = []
    for raw in text.splitlines():
        idx = raw.find(prefix)
        if idx < 0:
            continue
        payload = raw[idx:].strip()
        if payload:
            out.append(payload)
    return out


def extract_unprefixed_plan_lines(text: str) -> List[str]:
    out: List[str] = []
    noise_prefixes = (
        "ASSERT|",
        "PLAN|",
        "METRIC|",
        "psql:",
        "warning:",
        "notice:",
        "connected",
        "database ",
        "sql>",
    )
    for raw in text.splitlines():
        line = raw.strip()
        if not line:
            continue
        if line.startswith("[") and "] [" in line:
            continue
        if line.lower().startswith(noise_prefixes):
            continue
        out.append(f"PLAN|{line}")
    return out


def normalize_lines(lines: List[str], order_sensitive: bool) -> List[str]:
    return list(lines) if order_sensitive else sorted(lines)


def run_stage(
    engine: Mapping[str, object],
    binary: str,
    sql_ref: str | None,
    workspace_root: Path,
    run_dir: Path,
    stage: str,
    timeout_seconds: int,
) -> Dict[str, object]:
    if not sql_ref:
        return {
            "status": "skipped",
            "returncode": 0,
            "sqlstate": "00000",
            "message": "",
            "stdout": "",
            "stderr": "",
            "elapsed_ms": 0,
            "assert_lines": [],
            "plan_lines": [],
            "metric_lines": [],
            "stdout_path": "",
            "stderr_path": "",
        }

    sql_path = resolve_sql_path(sql_ref, workspace_root)
    if not sql_path.exists():
        return {
            "status": "missing",
            "returncode": 0,
            "sqlstate": "00000",
            "message": f"missing sql file: {sql_path}",
            "stdout": "",
            "stderr": "",
            "elapsed_ms": 0,
            "assert_lines": [],
            "plan_lines": [],
            "metric_lines": [],
            "stdout_path": "",
            "stderr_path": "",
        }

    res = run_sql_file(engine, binary, sql_path, run_dir, stage, timeout_seconds)
    combined = f"{res.stdout}\n{res.stderr}"
    d = res.__dict__
    d["plan_lines"] = extract_prefixed_lines(combined, "PLAN|")
    if stage == "plan" and not d["plan_lines"]:
        d["plan_lines"] = extract_unprefixed_plan_lines(combined)
    d["metric_lines"] = extract_prefixed_lines(combined, "METRIC|")
    return d


def run_case_on_engine(
    engine: Mapping[str, object],
    binary: str,
    surface: Mapping[str, object],
    workspace_root: Path,
    run_dir: Path,
    timeout_seconds: int,
) -> Dict[str, object]:
    return {
        "setup": run_stage(engine, binary, surface.get("setup"), workspace_root, run_dir / "raw", "setup", timeout_seconds),
        "plan": run_stage(engine, binary, surface.get("plan"), workspace_root, run_dir / "raw", "plan", timeout_seconds),
        "exec": run_stage(engine, binary, surface.get("exec"), workspace_root, run_dir / "raw", "exec", timeout_seconds),
        "teardown": run_stage(engine, binary, surface.get("teardown"), workspace_root, run_dir / "raw", "teardown", timeout_seconds),
    }


def latency_ratio_ms(scratchbird_ms: int, donor_ms: int) -> float:
    if donor_ms <= 0:
        return 1.0
    return float(max(1, scratchbird_ms)) / float(max(1, donor_ms))


def score_pair(
    case: Mapping[str, object],
    sb_exec: Mapping[str, object],
    donor_exec: Mapping[str, object],
    sb_plan: Mapping[str, object],
    donor_plan: Mapping[str, object],
) -> Dict[str, object]:
    normalization = case.get("normalization", {}) or {}
    scoring = case.get("scoring", {}) or {}
    order_sensitive = bool(normalization.get("order_sensitive", False))
    require_plan_lines = bool(normalization.get("require_plan_lines", True))
    max_ratio = float(scoring.get("donor_competitive_ratio_max", 1.25))
    semantics_weight = float(scoring.get("semantics_weight", 0.50))
    plan_weight = float(scoring.get("plan_weight", 0.30))
    latency_weight = float(scoring.get("latency_weight", 0.20))

    semantics_ok = False
    semantics_reason = ""
    if sb_exec.get("status") == "ok" and donor_exec.get("status") == "ok":
        semantics_ok, semantics_reason = compare_assertions(
            normalize_lines(list(sb_exec.get("assert_lines", []) or []), order_sensitive),
            normalize_lines(list(donor_exec.get("assert_lines", []) or []), order_sensitive),
            order_sensitive=True,
        )
    else:
        semantics_reason = (
            f"exec status mismatch sb={sb_exec.get('status')} donor={donor_exec.get('status')}"
        )

    plan_ok = True
    if require_plan_lines:
        plan_ok = bool(sb_plan.get("plan_lines")) and bool(donor_plan.get("plan_lines"))

    ratio = latency_ratio_ms(
        int(sb_exec.get("elapsed_ms", 0)),
        int(donor_exec.get("elapsed_ms", 0)),
    )
    latency_ok = ratio <= max_ratio

    weighted_score = (
        (semantics_weight if semantics_ok else 0.0)
        + (plan_weight if plan_ok else 0.0)
        + (latency_weight if latency_ok else 0.0)
    )

    return {
        "semantics_ok": semantics_ok,
        "semantics_reason": semantics_reason,
        "plan_ok": plan_ok,
        "latency_ratio": round(ratio, 6),
        "latency_ok": latency_ok,
        "weighted_score": round(weighted_score, 6),
        "competitive": semantics_ok and plan_ok and latency_ok,
    }


def main() -> None:
    args = parse_args()
    workspace_root, repo_root = resolve_paths(args)
    engines_cfg = load_structured(Path(args.engines))
    corpus_cfg = load_structured(Path(args.corpus))
    out_dir = Path(args.out_dir)
    ensure_dir(out_dir)

    timeout_seconds = int(
        engines_cfg.get("defaults", {}).get(
            "timeout_seconds",
            corpus_cfg.get("defaults", {}).get("timeout_seconds", 300),
        )
    )
    enabled = [e for e in engines_cfg.get("engines", []) if e.get("enabled", True)]

    run_id = time.strftime("%Y%m%d_%H%M%S")
    run_dir = out_dir / run_id
    ensure_dir(run_dir / "raw")

    engine_rows: List[Dict[str, object]] = []
    pair_rows: List[Dict[str, object]] = []
    discovery_rows: List[Dict[str, object]] = []
    discovery_map: Dict[str, Dict[str, object]] = {}
    summary = {
        "total_cases": 0,
        "executed_engine_runs": 0,
        "pairwise_scores": 0,
        "package_scaffold_discoveries": 0,
        "package_scaffold_failures": 0,
    }

    eligible_engines: List[Mapping[str, object]] = []
    for engine in enabled:
        engine_id = str(engine.get("id", ""))
        discovery_row: Dict[str, object] = {
            "engine_id": engine_id,
            "status": "not_applicable",
            "parser_binary": "",
            "profile_id": "",
            "parser_package": "",
            "compiler_udr_package": "",
            "emulation_udr_package": "",
            "bundle_contract_id": "",
            "message": "",
        }
        if str(engine.get("family", "")) == "scratchbird" and str(engine.get("mode", "")) == "emulated":
            discovery_row = discover_engine_package_scaffold(engine, repo_root)
            summary["package_scaffold_discoveries"] += 1
            if discovery_row.get("status") != "ok":
                summary["package_scaffold_failures"] += 1
            else:
                eligible_engines.append(engine)
        else:
            eligible_engines.append(engine)
        discovery_rows.append(discovery_row)
        discovery_map[engine_id] = discovery_row

    enabled = eligible_engines
    scratchbird_engines = [e for e in enabled if str(e.get("family", "")) == "scratchbird"]
    reference_engines = [e for e in enabled if str(e.get("family", "")) != "scratchbird"]

    for case in corpus_cfg.get("cases", []):
        if not case.get("enabled", True):
            continue

        summary["total_cases"] += 1
        lanes = set(case.get("lanes", []))
        surfaces = case.get("surfaces", {}) or {}
        case_id = str(case["id"])
        case_intent = str(case.get("intent", ""))
        native_equivalence_note = str(case.get("native_equivalence_note", ""))

        case_runs: Dict[str, Dict[str, object]] = {}
        case_run_meta: Dict[str, Dict[str, object]] = {}
        for engine in enabled:
            lane = lane_for_engine(engine)
            surface_key = surface_key_for_engine(engine)
            if lanes and surface_key not in lanes:
                continue
            surface = surfaces.get(surface_key)
            if not surface:
                continue

            binary = resolve_client_binary(engine, repo_root)
            engine_run = run_case_on_engine(
                engine=engine,
                binary=binary,
                surface=surface,
                workspace_root=workspace_root,
                run_dir=run_dir / case_id / str(engine["id"]),
                timeout_seconds=timeout_seconds,
            )
            engine_id = str(engine["id"])
            case_runs[engine_id] = engine_run
            case_run_meta[engine_id] = {
                "engine_id": engine_id,
                "lane": lane,
                "surface_key": surface_key,
                "mode": str(engine.get("mode", "")),
                "family": str(engine.get("family", "")),
            }
            summary["executed_engine_runs"] += 1

            engine_rows.append(
                {
                    "case_id": case_id,
                    "engine_id": engine_id,
                    "lane": lane,
                    "surface_key": surface_key,
                    "setup_status": engine_run["setup"]["status"],
                    "plan_status": engine_run["plan"]["status"],
                    "exec_status": engine_run["exec"]["status"],
                    "teardown_status": engine_run["teardown"]["status"],
                    "plan_line_count": len(engine_run["plan"].get("plan_lines", []) or []),
                    "assert_line_count": len(engine_run["exec"].get("assert_lines", []) or []),
                    "metric_line_count": len(engine_run["exec"].get("metric_lines", []) or []),
                    "exec_elapsed_ms": engine_run["exec"].get("elapsed_ms", 0),
                    "package_scaffold_status": discovery_map.get(engine_id, {}).get("status", ""),
                    "package_scaffold_parser_binary": discovery_map.get(engine_id, {}).get("parser_binary", ""),
                    "package_scaffold_bundle_contract_id": discovery_map.get(engine_id, {}).get("bundle_contract_id", ""),
                }
            )
            write_json(
                run_dir / case_id / engine_id / "run.json",
                engine_run,
            )

        for donor_engine in reference_engines:
            donor_id = str(donor_engine["id"])
            donor_run = case_runs.get(donor_id)
            donor_meta = case_run_meta.get(donor_id)
            if donor_run is None or donor_meta is None:
                continue
            donor_lane = str(donor_meta["lane"])

            for sb_engine in scratchbird_engines:
                sb_id = str(sb_engine["id"])
                sb_run = case_runs.get(sb_id)
                sb_meta = case_run_meta.get(sb_id)
                if sb_run is None or sb_meta is None:
                    continue

                sb_mode = str(sb_meta["mode"])
                sb_lane = str(sb_meta["lane"])
                comparison_kind = ""
                comparison_note = ""

                if sb_mode == "emulated":
                    if sb_lane != donor_lane:
                        continue
                    comparison_kind = "emulation_parity"
                    comparison_note = (
                        "ScratchBird emulation parser/listener is compared "
                        "directly against the matching donor dialect."
                    )
                elif sb_mode == "native":
                    if "native" not in surfaces:
                        continue
                    comparison_kind = "native_intent"
                    comparison_note = native_equivalence_note
                else:
                    continue

                result = score_pair(
                    case=case,
                    sb_exec=sb_run["exec"],
                    donor_exec=donor_run["exec"],
                    sb_plan=sb_run["plan"],
                    donor_plan=donor_run["plan"],
                )
                pair_rows.append(
                    {
                        "case_id": case_id,
                        "intent": case_intent,
                        "scratchbird_engine_id": sb_id,
                        "scratchbird_lane": sb_lane,
                        "scratchbird_mode": sb_mode,
                        "donor_engine_id": donor_id,
                        "donor_lane": donor_lane,
                        "comparison_kind": comparison_kind,
                        "comparison_note": comparison_note,
                        "competitive": result["competitive"],
                        "weighted_score": result["weighted_score"],
                        "semantics_ok": result["semantics_ok"],
                        "plan_ok": result["plan_ok"],
                        "latency_ok": result["latency_ok"],
                        "latency_ratio": result["latency_ratio"],
                        "reason": result["semantics_reason"],
                    }
                )
                summary["pairwise_scores"] += 1

    write_csv(run_dir / "optimizer_engine_discovery.csv", discovery_rows)
    write_csv(run_dir / "optimizer_engine_runs.csv", engine_rows)
    write_csv(run_dir / "optimizer_pairwise_scores.csv", pair_rows)
    write_csv(run_dir / "optimizer_summary.csv", [summary])
    write_json(run_dir / "optimizer_summary.json", summary)
    print(f"optimizer comparison run complete: {run_dir}")


if __name__ == "__main__":
    main()
