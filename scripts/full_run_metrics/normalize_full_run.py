#!/usr/bin/env python3

from __future__ import annotations

import argparse
import csv
import datetime as dt
import json
import math
import os
import platform as platform_module
import re
import shutil
import socket
import statistics
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, Iterable, List, Mapping, Sequence, Tuple


SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import generate_unified_comparison_csv  # noqa: E402


SUITE_ORDER = [
    "full-gate",
    "public-beta",
    "regression",
    "stress",
    "acid",
    "engine-differential",
    "index-comparison",
    "emulation-comparison",
    "native-comparative-regression",
    "native-v3-inet",
    "performance",
    "optimizer-donor-compare",
]

VERIFICATION_MAX_STALENESS_SECONDS = 36 * 60 * 60

HISTORY_KEY_METRICS = {
    ("full-gate", "summary.total_time_sec"),
    ("full-gate", "summary.failed"),
    ("public-beta", "summary.failed"),
    ("public-beta", "summary.total_steps"),
    ("regression", "totals.failed"),
    ("regression", "matrix.duration_seconds"),
    ("stress", "summary.failed"),
    ("stress", "matrix.duration_seconds"),
    ("acid", "summary.failed"),
    ("acid", "matrix.duration_seconds"),
    ("engine-differential", "summary.failed"),
    ("engine-differential", "matrix.duration_seconds"),
    ("index-comparison", "summary.failed"),
    ("index-comparison", "summary.score"),
    ("index-comparison", "matrix.duration_seconds"),
    ("emulation-comparison", "summary.failed"),
    ("emulation-comparison", "summary.total"),
    ("native-comparative-regression", "summary.exec_failed"),
    ("native-comparative-regression", "summary.avg_exec_elapsed_ms"),
    ("performance", "summary.avg_latency_p95_ms"),
    ("performance", "summary.avg_throughput_tps"),
    ("optimizer-donor-compare", "summary.avg_exec_elapsed_ms"),
}

EMULATION_COMPARE_DIALECTS = ("firebird", "mysql", "postgresql")
EMULATION_COMPARE_CONTRACT_ID = "compatibility-emulation-compare-v1"
EMULATION_COMPARE_HARNESS = "compatibility_converted_sql_ctest"
NATIVE_COMPARATIVE_CONTRACT_ID = "native-v3-comparative-regression-v1"
NATIVE_COMPARATIVE_HARNESS = "static_translated_regression_corpus"


@dataclass
class SuiteArtifact:
    engine: str
    suite: str
    status: str
    exit_code: int
    started_at: str
    duration_seconds: float
    output_dir: Path
    summary_path: Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Normalize a ScratchBird full build/test evidence set into a "
            "benchmark-compatible matrix so current runs can be compared against "
            "upstream baselines and recent ScratchBird history."
        )
    )
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--run-id", default=None, help="Normalized output run id. Defaults to current UTC timestamp.")
    parser.add_argument(
        "--output-root",
        type=Path,
        default=None,
        help="Output directory. Defaults to <repo-root>/tests/results/full_run_metrics/<run-id>.",
    )
    parser.add_argument(
        "--full-gate-dir",
        type=Path,
        default=None,
        help="Full-gate artifact directory (contains RUN_STATUS.txt or configure/build/ctest logs).",
    )
    parser.add_argument(
        "--public-beta-dir",
        type=Path,
        default=None,
        help="Required public beta result directory.",
    )
    parser.add_argument(
        "--compatibility-root",
        type=Path,
        default=None,
        help="Compatibility root. Defaults to <repo-root>/tests/compatibility.",
    )
    parser.add_argument(
        "--v3-native-inet-dir",
        type=Path,
        default=None,
        help="V3 native inet result directory.",
    )
    parser.add_argument(
        "--v3-native-comparative-dir",
        type=Path,
        default=None,
        help="V3 native comparative regression result directory.",
    )
    parser.add_argument(
        "--verification-root",
        type=Path,
        default=None,
        help="Verification bundle result root. Defaults to <repo-root>/scripts/verification_bundle/suite/results.",
    )
    parser.add_argument(
        "--benchmarks-root",
        type=Path,
        default=None,
        help="ScratchBird-Benchmarks results root or specific matrix run directory.",
    )
    parser.add_argument(
        "--history-root",
        type=Path,
        default=None,
        help="Normalized ScratchBird full-run metrics root. Defaults to <repo-root>/tests/results/full_run_metrics.",
    )
    parser.add_argument("--history-limit", type=int, default=5)
    parser.add_argument("--skip-system-info", action="store_true")
    return parser.parse_args()


def utc_now_stamp() -> str:
    return time.strftime("%Y%m%dT%H%M%SZ", time.gmtime())


def utc_now_iso() -> str:
    return time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())


def ensure_dir(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)


def clean_output_root(path: Path) -> None:
    if not path.exists():
        return
    for child in path.iterdir():
        if child.is_dir():
            shutil.rmtree(child)
        else:
            child.unlink()


def load_json(path: Path) -> Dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(path: Path, payload: Mapping[str, Any]) -> None:
    ensure_dir(path.parent)
    path.write_text(json.dumps(payload, indent=2, sort_keys=False) + "\n", encoding="utf-8")


def parse_key_value_file(path: Path) -> Dict[str, str]:
    out: Dict[str, str] = {}
    for raw in path.read_text(encoding="utf-8").splitlines():
        if "=" not in raw:
            continue
        key, value = raw.split("=", 1)
        out[key.strip()] = value.strip()
    return out


def resolve_relative(base: Path, maybe_relative: str | None) -> Path | None:
    if not maybe_relative:
        return None
    candidate = Path(maybe_relative)
    if candidate.is_absolute():
        return candidate
    return (base / candidate).resolve()


def latest_subdir(root: Path, require: str | None = None) -> Path | None:
    if not root.exists():
        return None
    candidates = [item for item in root.iterdir() if item.is_dir()]
    if require:
        candidates = [item for item in candidates if (item / require).exists()]
    if not candidates:
        return None
    return sorted(candidates, key=lambda item: item.name)[-1]


def latest_file(glob_root: Path, pattern: str) -> Path | None:
    matches = sorted(glob_root.glob(pattern))
    if not matches:
        return None
    return matches[-1]


def copy_tree_replace(source: Path, destination: Path) -> None:
    ensure_dir(destination.parent)
    if destination.exists():
        shutil.rmtree(destination)
    shutil.copytree(source, destination)


def parse_timestamp_name(name: str) -> float | None:
    for fmt in ("%Y%m%d_%H%M%S", "%Y%m%dT%H%M%SZ", "%Y%m%d-%H%M%S"):
        try:
            return dt.datetime.strptime(name, fmt).replace(tzinfo=dt.timezone.utc).timestamp()
        except ValueError:
            continue
    return None


def parse_utc_timestamp(value: str | None) -> float | None:
    if not value:
        return None
    for fmt in ("%Y-%m-%dT%H:%M:%SZ", "%Y-%m-%dT%H:%M:%S"):
        try:
            return dt.datetime.strptime(value, fmt).replace(tzinfo=dt.timezone.utc).timestamp()
        except ValueError:
            continue
    return None


def safe_float(value: Any) -> float | None:
    if value in ("", None):
        return None
    try:
        return float(value)
    except (TypeError, ValueError):
        return None


def safe_int(value: Any) -> int | None:
    if value in ("", None):
        return None
    try:
        return int(str(value))
    except (TypeError, ValueError):
        return None


def parse_ctest_log(path: Path) -> Dict[str, Any]:
    text = path.read_text(encoding="utf-8", errors="replace")
    summary_match = re.search(
        r"(?P<pct>\d+)% tests passed,\s+(?P<failed>\d+) tests failed out of (?P<total>\d+)",
        text,
    )
    all_pass_match = re.search(
        r"100% tests passed,\s+0 tests failed out of (?P<total>\d+)",
        text,
    )
    total = failed = None
    if summary_match:
        total = int(summary_match.group("total"))
        failed = int(summary_match.group("failed"))
    elif all_pass_match:
        total = int(all_pass_match.group("total"))
        failed = 0

    skipped = 0
    did_not_run_match = re.search(r"The following tests did not run:\n((?:\t.+\n?)*)", text)
    if did_not_run_match:
        skipped = len([line for line in did_not_run_match.group(1).splitlines() if line.strip()])

    total_time_sec = None
    total_time_match = re.search(r"Total Test time \(real\) = ([0-9.]+) sec", text)
    if total_time_match:
        total_time_sec = float(total_time_match.group(1))

    label_rows: List[Dict[str, Any]] = []
    for match in re.finditer(r"^([A-Za-z0-9_./-]+)\s+=\s+([0-9.]+) sec\*proc \((\d+) tests?\)$", text, re.MULTILINE):
        label_rows.append(
            {
                "label": match.group(1),
                "sec_proc": float(match.group(2)),
                "tests": int(match.group(3)),
            }
        )

    passed = None
    if total is not None and failed is not None:
        passed = max(total - failed - skipped, 0)

    return {
        "total": total,
        "failed": failed,
        "passed": passed,
        "skipped": skipped,
        "total_time_sec": total_time_sec,
        "labels": label_rows,
    }


def build_regression_summary(manifest: Mapping[str, Any], source: Path, output_path: Path) -> Dict[str, Any]:
    listed_tests = int(manifest.get("listed_tests", 0))
    failure_count = int(manifest.get("failure_count", 0))
    status = str(manifest.get("status", "unknown"))
    skipped = listed_tests if status == "skipped" else 0
    passed = max(listed_tests - failure_count - skipped, 0)
    totals = {
        "total": listed_tests,
        "passed": passed,
        "failed": failure_count if status != "skipped" else 0,
        "skipped": skipped,
        "errors": 0,
        "pass_rate": round((passed / listed_tests) if listed_tests else 0.0, 6),
    }
    payload = {
        "source": str(source),
        "manifest": manifest,
        "totals": totals,
    }
    write_json(output_path, payload)
    return payload


def manifest_flag_truthy(value: Any) -> bool:
    return str(value).strip().lower() in ("1", "true", "yes", "on")


def normalize_list_identity(compat_root: Path, dialect: str, raw_value: Any) -> str:
    raw = str(raw_value or "").strip()
    if not raw:
        return ""
    lane_root = compat_root / dialect
    candidate = Path(raw)
    if candidate.is_absolute():
        try:
            return str(candidate.resolve().relative_to(lane_root.resolve()))
        except ValueError:
            return candidate.name
    try:
        return str(candidate.relative_to(lane_root))
    except ValueError:
        return raw


def infer_emulation_comparison_role(manifest: Mapping[str, Any]) -> str:
    explicit = str(manifest.get("comparison_target_role", "")).strip().lower()
    if explicit in ("original", "emulation"):
        return explicit
    execution_mode = str(manifest.get("execution_mode", "")).strip()
    if execution_mode == "scratchbird_fb_emulation_client":
        return "emulation"
    if execution_mode == "native_firebird_client":
        return "original"
    return "emulation" if manifest_flag_truthy(manifest.get("require_sb_emulation_marker")) else "original"


def infer_emulation_comparison_harness(manifest: Mapping[str, Any]) -> str:
    explicit = str(manifest.get("comparison_harness", "")).strip()
    if explicit:
        return explicit
    execution_mode = str(manifest.get("execution_mode", "")).strip()
    if execution_mode in ("native_firebird_client", "scratchbird_fb_emulation_client", "converted_sql_ctest"):
        return EMULATION_COMPARE_HARNESS
    return execution_mode or "unknown"


def infer_emulation_comparison_target_id(manifest: Mapping[str, Any], dialect: str) -> str:
    explicit = str(manifest.get("comparison_target_id", "")).strip()
    if explicit:
        return explicit
    role = infer_emulation_comparison_role(manifest)
    prefix = "scratchbird" if role == "emulation" else "upstream"
    return f"{prefix}-{dialect}"


def build_emulation_comparison_contract(
    compat_root: Path,
    dialect: str,
    target_role: str,
    target_engine: str,
    manifest: Mapping[str, Any] | None,
) -> Dict[str, Any]:
    if manifest is None:
        return {
            "suite_family": "emulation-comparison",
            "contract_id": EMULATION_COMPARE_CONTRACT_ID,
            "expected_harness": EMULATION_COMPARE_HARNESS,
            "dialect_family": dialect,
            "target_role": target_role,
            "target_engine": target_engine,
            "contract_state": "missing_same_contract_source",
            "pairwise_eligible": False,
            "protocol_surface": "",
            "parser_core": "",
            "parser_mode": "",
            "ctest_list_mode": "",
            "ctest_list_identity": "",
            "listed_tests": 0,
            "execution_mode": "",
        }

    harness = infer_emulation_comparison_harness(manifest)
    contract_id = str(manifest.get("comparison_contract_id", EMULATION_COMPARE_CONTRACT_ID))
    return {
        "suite_family": "emulation-comparison",
        "contract_id": contract_id,
        "expected_harness": EMULATION_COMPARE_HARNESS,
        "dialect_family": dialect,
        "target_role": target_role,
        "target_engine": target_engine,
        "contract_state": "eligible" if harness == EMULATION_COMPARE_HARNESS and contract_id == EMULATION_COMPARE_CONTRACT_ID else "ineligible",
        "pairwise_eligible": harness == EMULATION_COMPARE_HARNESS and contract_id == EMULATION_COMPARE_CONTRACT_ID,
        "protocol_surface": str(manifest.get("protocol_surface", "")),
        "parser_core": str(manifest.get("parser_core", "")),
        "parser_mode": str(manifest.get("parser_mode", "")),
        "ctest_list_mode": str(manifest.get("ctest_list_mode", "")),
        "ctest_list_identity": normalize_list_identity(compat_root, dialect, manifest.get("ctest_list_file", "")),
        "listed_tests": int(manifest.get("listed_tests", 0)),
        "execution_mode": str(manifest.get("execution_mode", "")),
    }


def summarize_status_counts(manifest: Mapping[str, Any] | None) -> Dict[str, Any]:
    if manifest is None:
        return {
            "total": 0,
            "passed": 0,
            "failed": 0,
            "skipped": 0,
            "pass_rate": 0.0,
            "result": "missing",
        }
    listed_tests = int(manifest.get("listed_tests", 0))
    failure_count = int(manifest.get("failure_count", 0))
    status = str(manifest.get("status", "unknown"))
    skipped = listed_tests if status == "skipped" else 0
    passed = max(listed_tests - failure_count - skipped, 0)
    return {
        "total": listed_tests,
        "passed": passed,
        "failed": failure_count if status != "skipped" else 0,
        "skipped": skipped,
        "pass_rate": round((passed / listed_tests) if listed_tests else 0.0, 6),
        "result": status,
    }


def latest_compat_manifest_for_role(
    compat_root: Path,
    dialect: str,
    target_role: str,
) -> tuple[Mapping[str, Any] | None, Path | None, Mapping[str, Any] | None, Path | None]:
    latest_any_manifest: Mapping[str, Any] | None = None
    latest_any_path: Path | None = None
    latest_eligible_manifest: Mapping[str, Any] | None = None
    latest_eligible_path: Path | None = None
    manifest_paths = sorted((compat_root / dialect / "results" / "ctest").glob("*/RUN_MANIFEST.json"), reverse=True)
    for manifest_path in manifest_paths:
        manifest = load_json(manifest_path)
        if infer_emulation_comparison_role(manifest) != target_role:
            continue
        if latest_any_manifest is None:
            latest_any_manifest = manifest
            latest_any_path = manifest_path
        if infer_emulation_comparison_harness(manifest) == EMULATION_COMPARE_HARNESS:
            latest_eligible_manifest = manifest
            latest_eligible_path = manifest_path
            break
    return latest_eligible_manifest, latest_eligible_path, latest_any_manifest, latest_any_path


def build_emulation_comparison_summary(
    compat_root: Path,
    dialect: str,
    target_role: str,
    target_engine: str,
    manifest: Mapping[str, Any] | None,
    manifest_path: Path | None,
    fallback_manifest: Mapping[str, Any] | None,
    fallback_manifest_path: Path | None,
    output_root: Path,
) -> tuple[SuiteArtifact, Dict[str, Any]]:
    suite_dir = output_root / target_engine / "emulation-comparison"
    ensure_dir(suite_dir)
    summary_path = suite_dir / f"emulation-comparison-{target_engine}-summary.json"
    contract = build_emulation_comparison_contract(compat_root, dialect, target_role, target_engine, manifest)
    payload = {
        "metadata": {
            "dialect_family": dialect,
            "target_role": target_role,
            "target_engine": target_engine,
            "source_result_dir": str(manifest_path.parent) if manifest_path else "",
            "source_manifest": str(manifest_path) if manifest_path else "",
            "latest_seen_result_dir": str(fallback_manifest_path.parent) if fallback_manifest_path else "",
            "latest_seen_manifest": str(fallback_manifest_path) if fallback_manifest_path else "",
        },
        "comparison_contract": contract,
        "summary": summarize_status_counts(manifest),
        "results": {
            "manifest": manifest or {},
            "latest_seen_manifest": fallback_manifest or {},
        },
    }
    write_json(summary_path, payload)
    artifact_status = str(payload["summary"]["result"])
    artifact = SuiteArtifact(
        engine=target_engine,
        suite="emulation-comparison",
        status=artifact_status,
        exit_code=0 if artifact_status in ("passed", "skipped") else 1,
        started_at=str((manifest or fallback_manifest or {}).get("timestamp_utc", utc_now_iso())),
        duration_seconds=0.0,
        output_dir=suite_dir,
        summary_path=summary_path,
    )
    return artifact, payload


def write_emulation_comparison_pairwise(
    output_root: Path,
    payloads: Mapping[str, Mapping[str, Dict[str, Any]]],
) -> None:
    json_path = output_root / "emulation-comparison-pairwise.json"
    md_path = output_root / "emulation-comparison-pairwise.md"
    pairs: List[Dict[str, Any]] = []
    for dialect in EMULATION_COMPARE_DIALECTS:
        original_payload = payloads[dialect]["original"]
        emulation_payload = payloads[dialect]["emulation"]
        original_contract = original_payload.get("comparison_contract", {})
        emulation_contract = emulation_payload.get("comparison_contract", {})
        contract_checks: List[Dict[str, Any]] = []
        comparable = True
        for key in (
            "contract_id",
            "expected_harness",
            "protocol_surface",
            "parser_core",
            "parser_mode",
            "ctest_list_mode",
            "ctest_list_identity",
            "listed_tests",
        ):
            original_value = original_contract.get(key, "")
            emulation_value = emulation_contract.get(key, "")
            match = original_value == emulation_value
            comparable = comparable and match
            contract_checks.append(
                {
                    "field": key,
                    "original": original_value,
                    "emulation": emulation_value,
                    "match": match,
                }
            )
        comparable = comparable and bool(original_contract.get("pairwise_eligible")) and bool(emulation_contract.get("pairwise_eligible"))
        verdict = "comparable"
        if not original_contract.get("pairwise_eligible") or not emulation_contract.get("pairwise_eligible"):
            verdict = "missing-or-ineligible-source"
        elif not comparable:
            verdict = "mismatched-contract"
        pairs.append(
            {
                "dialect_family": dialect,
                "original_target": original_contract.get("target_engine", f"upstream-{dialect}"),
                "emulation_target": emulation_contract.get("target_engine", f"scratchbird-{dialect}"),
                "original_status": original_payload.get("summary", {}).get("result", ""),
                "emulation_status": emulation_payload.get("summary", {}).get("result", ""),
                "comparable": comparable,
                "verdict": verdict,
                "contract_checks": contract_checks,
                "original_summary_json": str(Path(original_payload["metadata"]["target_engine"]) / "emulation-comparison" / f"emulation-comparison-{original_payload['metadata']['target_engine']}-summary.json"),
                "emulation_summary_json": str(Path(emulation_payload["metadata"]["target_engine"]) / "emulation-comparison" / f"emulation-comparison-{emulation_payload['metadata']['target_engine']}-summary.json"),
            }
        )
    write_json(
        json_path,
        {
            "suite_family": "emulation-comparison",
            "contract_id": EMULATION_COMPARE_CONTRACT_ID,
            "pairs": pairs,
        },
    )
    lines = [
        "# Emulation Comparison Pairwise Verdicts",
        "",
        "| Dialect | Original | Emulation | Verdict | Original Status | Emulation Status |",
        "|---|---|---|---|---|---|",
    ]
    for item in pairs:
        lines.append(
            f"| `{item['dialect_family']}` | `{item['original_target']}` | `{item['emulation_target']}` | `{item['verdict']}` | `{item['original_status']}` | `{item['emulation_status']}` |"
        )
    md_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def parse_emulation_comparison_manifests(compat_root: Path, output_root: Path) -> List[SuiteArtifact]:
    artifacts: List[SuiteArtifact] = []
    payloads: Dict[str, Dict[str, Dict[str, Any]]] = {}
    for dialect in EMULATION_COMPARE_DIALECTS:
        payloads[dialect] = {}
        for target_role, target_engine in (("original", f"upstream-{dialect}"), ("emulation", f"scratchbird-{dialect}")):
            manifest, manifest_path, fallback_manifest, fallback_manifest_path = latest_compat_manifest_for_role(
                compat_root,
                dialect,
                target_role,
            )
            artifact, payload = build_emulation_comparison_summary(
                compat_root,
                dialect,
                target_role,
                target_engine,
                manifest,
                manifest_path,
                fallback_manifest,
                fallback_manifest_path,
                output_root,
            )
            artifacts.append(artifact)
            payloads[dialect][target_role] = payload
    write_emulation_comparison_pairwise(output_root, payloads)
    return artifacts


def parse_public_beta(result_dir: Path, output_root: Path) -> SuiteArtifact | None:
    category_summary = result_dir / "category_summary.txt"
    step_results = result_dir / "step_results.txt"
    if not category_summary.exists() or not step_results.exists():
        return None

    categories: Dict[str, Dict[str, int]] = {}
    for raw in category_summary.read_text(encoding="utf-8").splitlines():
        parts = raw.split("|")
        if len(parts) != 4 or parts[0] != "CATEGORY_SUMMARY":
            continue
        categories[parts[1]] = {"passed": int(parts[2]), "failed": int(parts[3])}

    steps: List[Dict[str, Any]] = []
    started_at = utc_now_iso()
    for raw in step_results.read_text(encoding="utf-8").splitlines():
        parts = raw.split("|")
        if len(parts) != 5 or parts[0] != "STEP_RESULT":
            continue
        steps.append(
            {
                "category": parts[1],
                "step_id": parts[2],
                "result": parts[3],
                "log": parts[4],
            }
        )

    passed = sum(1 for item in steps if item["result"] == "PASS")
    failed = sum(1 for item in steps if item["result"] != "PASS")
    suite_dir = output_root / "scratchbird" / "public-beta"
    ensure_dir(suite_dir)
    summary_path = suite_dir / "public-beta-scratchbird-summary.json"
    payload = {
        "metadata": {
            "source_result_dir": str(result_dir),
            "summary_markdown": str(result_dir / "SUMMARY.md"),
        },
        "summary": {
            "total_steps": len(steps),
            "passed": passed,
            "failed": failed,
            "by_category": categories,
        },
        "results": steps,
    }
    write_json(summary_path, payload)
    return SuiteArtifact(
        engine="scratchbird",
        suite="public-beta",
        status="passed" if failed == 0 else "failed",
        exit_code=0 if failed == 0 else 1,
        started_at=started_at,
        duration_seconds=0.0,
        output_dir=suite_dir,
        summary_path=summary_path,
    )


def parse_full_gate(run_dir: Path, repo_root: Path, output_root: Path) -> SuiteArtifact | None:
    run_status_path = run_dir / "RUN_STATUS.txt"
    configure_log = run_dir / "configure.log"
    build_log = run_dir / "build.log"
    ctest_log = run_dir / "ctest.log"

    status_map: Dict[str, str] = {}
    if run_status_path.exists():
        status_map = parse_key_value_file(run_status_path)
        configure_log = resolve_relative(repo_root, status_map.get("configure_log")) or configure_log
        build_log = resolve_relative(repo_root, status_map.get("build_log")) or build_log
        ctest_log = resolve_relative(repo_root, status_map.get("ctest_log")) or ctest_log

    if not ctest_log.exists():
        return None

    ctest = parse_ctest_log(ctest_log)
    configure_exit = safe_int(status_map.get("configure_exit"))
    build_exit = safe_int(status_map.get("build_exit"))
    ctest_exit = safe_int(status_map.get("ctest_exit"))
    if ctest_exit is None:
        ctest_exit = safe_int(status_map.get("ctest_rc"))
    if ctest_exit is None:
        ctest_exit = 0 if (ctest.get("failed") or 0) == 0 else 1

    suite_dir = output_root / "scratchbird" / "full-gate"
    ensure_dir(suite_dir)
    summary_path = suite_dir / "full-gate-scratchbird-summary.json"
    payload = {
        "metadata": {
            "source_run_dir": str(run_dir),
            "run_status_path": str(run_status_path) if run_status_path.exists() else "",
            "configure_log": str(configure_log) if configure_log.exists() else "",
            "build_log": str(build_log) if build_log.exists() else "",
            "ctest_log": str(ctest_log),
        },
        "summary": {
            "configure_exit": configure_exit,
            "build_exit": build_exit,
            "ctest_exit": ctest_exit,
            "total_tests": ctest.get("total"),
            "passed": ctest.get("passed"),
            "failed": ctest.get("failed"),
            "skipped": ctest.get("skipped"),
            "total_time_sec": ctest.get("total_time_sec"),
        },
        "results": {
            "labels": ctest.get("labels", []),
        },
    }
    write_json(summary_path, payload)
    return SuiteArtifact(
        engine="scratchbird",
        suite="full-gate",
        status="passed" if ctest_exit == 0 else "failed",
        exit_code=ctest_exit,
        started_at=utc_now_iso(),
        duration_seconds=float(ctest.get("total_time_sec") or 0.0),
        output_dir=suite_dir,
        summary_path=summary_path,
    )


def anchor_timestamp_for_run(full_gate_dir: Path | None) -> float | None:
    if full_gate_dir is None:
        return None
    run_status_path = full_gate_dir / "RUN_STATUS.txt"
    if run_status_path.exists():
        status_map = parse_key_value_file(run_status_path)
        for key in ("started_utc", "start_utc", "ctest_start_utc", "finished_utc"):
            parsed = parse_utc_timestamp(status_map.get(key))
            if parsed is not None:
                return parsed
    return parse_timestamp_name(full_gate_dir.name)


def verification_run_is_fresh(run_dir: Path, anchor_timestamp: float | None) -> bool:
    if anchor_timestamp is None:
        return True
    run_timestamp = parse_timestamp_name(run_dir.name)
    if run_timestamp is None:
        return True
    return abs(anchor_timestamp - run_timestamp) <= VERIFICATION_MAX_STALENESS_SECONDS


def parse_compatibility_manifests(compat_root: Path, output_root: Path) -> List[SuiteArtifact]:
    lane_to_engine = {
        "scratchbird": "scratchbird",
        "firebird": "firebird",
        "mysql": "mysql",
        "postgresql": "postgresql",
    }
    artifacts: List[SuiteArtifact] = []
    for lane, engine in lane_to_engine.items():
        manifest = latest_file(compat_root / lane / "results" / "ctest", "*/RUN_MANIFEST.json")
        if manifest is None:
            continue
        manifest_json = load_json(manifest)
        suite_dir = output_root / engine / "regression"
        ensure_dir(suite_dir)
        summary_path = suite_dir / f"regression-{engine}-summary.json"
        build_regression_summary(manifest_json, manifest.parent, summary_path)
        listed = int(manifest_json.get("listed_tests", 0))
        failures = int(manifest_json.get("failure_count", 0))
        status = str(manifest_json.get("status", "unknown"))
        artifacts.append(
            SuiteArtifact(
                engine=engine,
                suite="regression",
                status=status,
                exit_code=0 if status in ("passed", "skipped") else 1,
                started_at=str(manifest_json.get("timestamp_utc", utc_now_iso())),
                duration_seconds=0.0,
                output_dir=suite_dir,
                summary_path=summary_path,
            )
        )
    return artifacts


def parse_v3_native_inet(result_dir: Path, output_root: Path) -> SuiteArtifact | None:
    manifest_path = result_dir / "RUN_MANIFEST.json"
    if not manifest_path.exists():
        return None

    manifest = load_json(manifest_path)
    case_status = result_dir / "case_status.txt"
    results: List[Dict[str, Any]] = []
    if case_status.exists():
        for raw in case_status.read_text(encoding="utf-8").splitlines():
            parts = raw.split("|")
            if len(parts) == 3 and parts[0] == "CASE":
                results.append({"case_name": parts[1], "result": parts[2]})

    passed = sum(1 for item in results if item["result"] == "PASS")
    failed = sum(1 for item in results if item["result"] != "PASS")
    total_cases = len(results) if results else int(manifest.get("listed_tests", 0))
    suite_dir = output_root / "scratchbird" / "native-v3-inet"
    ensure_dir(suite_dir)
    summary_path = suite_dir / "native-v3-inet-scratchbird-summary.json"
    payload = {
        "metadata": {
            "source_result_dir": str(result_dir),
            "manifest": manifest,
        },
        "summary": {
            "total_cases": total_cases,
            "passed": passed if results else max(total_cases - int(manifest.get("failure_count", 0)), 0),
            "failed": failed if results else int(manifest.get("failure_count", 0)),
        },
        "results": results,
    }
    write_json(summary_path, payload)
    return SuiteArtifact(
        engine="scratchbird",
        suite="native-v3-inet",
        status=str(manifest.get("status", "unknown")),
        exit_code=0 if str(manifest.get("status", "")) in ("passed", "skipped") else 1,
        started_at=str(manifest.get("timestamp_utc", utc_now_iso())),
        duration_seconds=0.0,
        output_dir=suite_dir,
        summary_path=summary_path,
    )


def comparative_engine_name(engine_id: str) -> str:
    mapping = {
        "scratchbird_native": "scratchbird-native",
        "ref_firebird": "upstream-firebird",
        "ref_mysql": "upstream-mysql",
        "ref_postgresql": "upstream-postgresql",
    }
    return mapping.get(engine_id, engine_id)


def write_native_comparative_pairwise(
    output_root: Path,
    pair_rows: List[Dict[str, str]],
    translation_contract: Mapping[str, Any] | None = None,
) -> None:
    json_path = output_root / "native-comparative-regression-pairwise.json"
    md_path = output_root / "native-comparative-regression-pairwise.md"
    by_dialect: Dict[str, List[Dict[str, str]]] = {}
    for row in pair_rows:
        by_dialect.setdefault(str(row.get("dialect_family", "unknown")), []).append(row)

    pairs: List[Dict[str, Any]] = []
    for dialect, rows in sorted(by_dialect.items()):
        total = len(rows)
        passed = sum(1 for row in rows if row.get("result") == "pass")
        failed = total - passed
        ratios = [safe_float(row.get("latency_ratio")) for row in rows]
        ratio_values = [value for value in ratios if value is not None]
        pairs.append(
            {
                "dialect_family": dialect,
                "total_pairs": total,
                "passed_pairs": passed,
                "failed_pairs": failed,
                "avg_latency_ratio": round(statistics.mean(ratio_values), 6) if ratio_values else 0.0,
                "verdict": "comparable" if failed == 0 else "failed",
            }
        )

    write_json(
        json_path,
        {
            "suite_family": "native-comparative-regression",
            "contract_id": NATIVE_COMPARATIVE_CONTRACT_ID,
            "translation_contract": dict(translation_contract or {}),
            "pairs": pairs,
        },
    )
    lines = [
        "# Native Comparative Regression Pairwise Verdicts",
        "",
        "| Dialect | Total | Passed | Failed | Avg Latency Ratio | Verdict |",
        "|---|---:|---:|---:|---:|---|",
    ]
    for item in pairs:
        lines.append(
            f"| `{item['dialect_family']}` | {item['total_pairs']} | {item['passed_pairs']} | {item['failed_pairs']} | {item['avg_latency_ratio']} | `{item['verdict']}` |"
        )
    md_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def parse_native_comparative(result_dir: Path, output_root: Path) -> List[SuiteArtifact]:
    manifest_path = result_dir / "RUN_MANIFEST.json"
    artifact_dir = result_dir
    summary_json = artifact_dir / "comparative_summary.json"
    engine_csv = artifact_dir / "comparative_engine_runs.csv"
    pairwise_csv = artifact_dir / "comparative_pairwise_scores.csv"
    if not summary_json.exists() or not engine_csv.exists():
        nested_artifact_dir = latest_subdir(result_dir, require="comparative_summary.json")
        if nested_artifact_dir is not None:
            artifact_dir = nested_artifact_dir
            summary_json = artifact_dir / "comparative_summary.json"
            engine_csv = artifact_dir / "comparative_engine_runs.csv"
            pairwise_csv = artifact_dir / "comparative_pairwise_scores.csv"
    if not manifest_path.exists() or not summary_json.exists() or not engine_csv.exists():
        return []

    manifest = load_json(manifest_path)
    summary_payload = load_json(summary_json)
    rows = read_csv_rows(engine_csv)
    by_engine: Dict[str, List[Dict[str, str]]] = {}
    for row in rows:
        by_engine.setdefault(str(row.get("engine_id", "unknown")), []).append(row)

    artifacts: List[SuiteArtifact] = []
    suite_manifest_status = str(manifest.get("status", "unknown"))
    for engine_id, engine_rows in by_engine.items():
        engine = comparative_engine_name(engine_id)
        exec_ok = sum(1 for row in engine_rows if row.get("exec_status") == "ok")
        exec_error = sum(1 for row in engine_rows if row.get("exec_status") == "error")
        total_cases = len(engine_rows)
        exec_times = [safe_float(row.get("exec_elapsed_ms")) or 0.0 for row in engine_rows]
        positive_cases = sum(1 for row in engine_rows if row.get("expectation") == "must_match")
        negative_cases = total_cases - positive_cases
        exec_unexpected = sum(
            1
            for row in engine_rows
            if row.get("exec_status") not in ("ok", "error")
        )
        suite_dir = output_root / engine / "native-comparative-regression"
        ensure_dir(suite_dir)
        summary_path = suite_dir / f"native-comparative-regression-{engine}-summary.json"
        payload = {
            "metadata": {
                "source_result_dir": str(result_dir),
                "artifact_dir": str(artifact_dir),
                "manifest": str(manifest_path),
                "summary_json": str(summary_json),
                "engine_csv": str(engine_csv),
            },
            "comparison_contract": {
                "suite_family": "native-comparative-regression",
                "contract_id": NATIVE_COMPARATIVE_CONTRACT_ID,
                "expected_harness": NATIVE_COMPARATIVE_HARNESS,
                "translation_contract": dict(summary_payload.get("translation_contract", {}) or {}),
                "target_engine": engine,
                "target_role": "native" if engine_id == "scratchbird_native" else "original",
                "parser_core": "v3",
                "parser_mode": "native_core" if engine_id == "scratchbird_native" else "donor_reference",
            },
            "summary": {
                "total_cases": total_cases,
                "exec_ok": exec_ok,
                "exec_error": exec_error,
                "exec_failed": exec_unexpected,
                "positive_cases": positive_cases,
                "negative_cases": negative_cases,
                "avg_exec_elapsed_ms": round(statistics.mean(exec_times), 6) if exec_times else 0.0,
            },
            "results": engine_rows,
        }
        write_json(summary_path, payload)
        artifacts.append(
            SuiteArtifact(
                engine=engine,
                suite="native-comparative-regression",
                status="passed" if suite_manifest_status in ("passed", "skipped") and exec_unexpected == 0 else "failed",
                exit_code=0 if suite_manifest_status in ("passed", "skipped") and exec_unexpected == 0 else 1,
                started_at=str(manifest.get("timestamp_utc", utc_now_iso())),
                duration_seconds=0.0,
                output_dir=suite_dir,
                summary_path=summary_path,
            )
        )

    if pairwise_csv.exists():
        write_native_comparative_pairwise(
            output_root,
            read_csv_rows(pairwise_csv),
            summary_payload.get("translation_contract", {}) or {},
        )
    return artifacts


def read_csv_rows(path: Path) -> List[Dict[str, str]]:
    with path.open(newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def parse_verification_perf(verification_root: Path, output_root: Path, anchor_timestamp: float | None) -> List[SuiteArtifact]:
    summary_csv = latest_file(verification_root / "perf", "*/perf_summary.csv")
    if summary_csv is None:
        return []
    run_dir = summary_csv.parent
    if not verification_run_is_fresh(run_dir, anchor_timestamp):
        return []
    perf_all = run_dir / "perf_all.csv"
    if not perf_all.exists():
        return []

    rows = read_csv_rows(perf_all)
    by_engine: Dict[str, List[Dict[str, str]]] = {}
    for row in rows:
        by_engine.setdefault(str(row.get("engine_id", "unknown")), []).append(row)

    artifacts: List[SuiteArtifact] = []
    for engine, engine_rows in by_engine.items():
        throughput_values = [safe_float(row.get("throughput_tps")) or 0.0 for row in engine_rows]
        p95_values = [safe_float(row.get("latency_p95_ms")) or 0.0 for row in engine_rows]
        error_rates = [safe_float(row.get("error_rate")) or 0.0 for row in engine_rows]
        passed = sum(1 for row in engine_rows if row.get("result") == "pass")
        failed = len(engine_rows) - passed
        suite_dir = output_root / engine / "performance"
        ensure_dir(suite_dir)
        summary_path = suite_dir / f"performance-{engine}-summary.json"
        payload = {
            "metadata": {
                "source_run_dir": str(run_dir),
                "summary_csv": str(summary_csv),
                "all_csv": str(perf_all),
            },
            "summary": {
                "total_runs": len(engine_rows),
                "passed": passed,
                "failed": failed,
                "avg_throughput_tps": round(statistics.mean(throughput_values), 6) if throughput_values else 0.0,
                "avg_latency_p95_ms": round(statistics.mean(p95_values), 6) if p95_values else 0.0,
                "max_latency_p95_ms": round(max(p95_values), 6) if p95_values else 0.0,
                "avg_error_rate": round(statistics.mean(error_rates), 6) if error_rates else 0.0,
            },
            "results": engine_rows,
        }
        write_json(summary_path, payload)
        artifacts.append(
            SuiteArtifact(
                engine=engine,
                suite="performance",
                status="passed" if failed == 0 else "failed",
                exit_code=0 if failed == 0 else 1,
                started_at=utc_now_iso(),
                duration_seconds=0.0,
                output_dir=suite_dir,
                summary_path=summary_path,
            )
        )
    return artifacts


def parse_optimizer_compare(verification_root: Path, output_root: Path, anchor_timestamp: float | None) -> List[SuiteArtifact]:
    summary_json = latest_file(verification_root / "optimizer_donor_compare", "*/optimizer_summary.json")
    if summary_json is None:
        return []
    run_dir = summary_json.parent
    if not verification_run_is_fresh(run_dir, anchor_timestamp):
        return []
    engine_csv = run_dir / "optimizer_engine_runs.csv"
    if not engine_csv.exists():
        return []

    rows = read_csv_rows(engine_csv)
    by_engine: Dict[str, List[Dict[str, str]]] = {}
    for row in rows:
        by_engine.setdefault(str(row.get("engine_id", "unknown")), []).append(row)

    artifacts: List[SuiteArtifact] = []
    for engine, engine_rows in by_engine.items():
        exec_ok = sum(1 for row in engine_rows if row.get("exec_status") == "ok")
        plan_ok = sum(1 for row in engine_rows if row.get("plan_status") == "ok")
        exec_times = [safe_float(row.get("exec_elapsed_ms")) or 0.0 for row in engine_rows]
        suite_dir = output_root / engine / "optimizer-donor-compare"
        ensure_dir(suite_dir)
        summary_path = suite_dir / f"optimizer-donor-compare-{engine}-summary.json"
        payload = {
            "metadata": {
                "source_run_dir": str(run_dir),
                "summary_json": str(summary_json),
                "engine_csv": str(engine_csv),
            },
            "summary": {
                "total_cases": len(engine_rows),
                "exec_ok": exec_ok,
                "plan_ok": plan_ok,
                "avg_exec_elapsed_ms": round(statistics.mean(exec_times), 6) if exec_times else 0.0,
            },
            "results": engine_rows,
        }
        write_json(summary_path, payload)
        artifacts.append(
            SuiteArtifact(
                engine=engine,
                suite="optimizer-donor-compare",
                status="passed" if exec_ok == len(engine_rows) else "failed",
                exit_code=0 if exec_ok == len(engine_rows) else 1,
                started_at=utc_now_iso(),
                duration_seconds=0.0,
                output_dir=suite_dir,
                summary_path=summary_path,
            )
        )
    return artifacts


def collect_system_info() -> Dict[str, Any]:
    hostname = socket.gethostname()
    ip_address = ""
    try:
        ip_address = socket.gethostbyname(hostname)
    except OSError:
        ip_address = ""

    cpu_model = "Unknown"
    cpu_vendor = "Unknown"
    cpu_flags: List[str] = []
    if Path("/proc/cpuinfo").exists():
        text = Path("/proc/cpuinfo").read_text(encoding="utf-8", errors="replace")
        model_match = re.search(r"model name\s*:\s*(.+)", text)
        vendor_match = re.search(r"vendor_id\s*:\s*(.+)", text)
        flags_match = re.search(r"flags\s*:\s*(.+)", text)
        if model_match:
            cpu_model = model_match.group(1).strip()
        if vendor_match:
            cpu_vendor = vendor_match.group(1).strip()
        if flags_match:
            cpu_flags = flags_match.group(1).split()

    total_mb = available_mb = used_mb = 0
    swap_total_mb = swap_used_mb = 0
    if Path("/proc/meminfo").exists():
        meminfo: Dict[str, int] = {}
        for raw in Path("/proc/meminfo").read_text(encoding="utf-8", errors="replace").splitlines():
            parts = raw.replace(":", "").split()
            if len(parts) >= 2:
                meminfo[parts[0]] = int(parts[1])
        total_mb = meminfo.get("MemTotal", 0) // 1024
        available_mb = meminfo.get("MemAvailable", 0) // 1024
        used_mb = max(total_mb - available_mb, 0)
        swap_total_mb = meminfo.get("SwapTotal", 0) // 1024
        swap_used_mb = max((meminfo.get("SwapTotal", 0) - meminfo.get("SwapFree", 0)) // 1024, 0)

    disk_total, disk_used, disk_free = shutil.disk_usage("/")
    disks = [
        {
            "device": "/",
            "mount_point": "/",
            "filesystem": "",
            "total_gb": round(disk_total / (1024 ** 3), 3),
            "used_gb": round(disk_used / (1024 ** 3), 3),
            "free_gb": round(disk_free / (1024 ** 3), 3),
            "percent_used": round((disk_used / disk_total) * 100.0, 3) if disk_total else 0.0,
            "type": "Unknown",
            "model": "Unknown",
            "is_rotational": True,
        }
    ]

    relevant_env = {
        key: os.environ[key]
        for key in sorted(os.environ)
        if key.startswith("SCRATCHBIRD_") or key.startswith("SB_") or key.startswith("CTEST_")
    }

    return {
        "collection_time": utc_now_iso(),
        "platform": platform_module.platform(),
        "python_version": platform_module.python_version(),
        "cpu": {
            "vendor": cpu_vendor,
            "model": cpu_model,
            "architecture": platform_module.machine(),
            "physical_cores": os.cpu_count() or 0,
            "logical_cores": os.cpu_count() or 0,
            "threads_per_core": 1,
            "base_frequency_mhz": 0.0,
            "max_frequency_mhz": 0.0,
            "cache_l1_kb": 0,
            "cache_l2_kb": 0,
            "cache_l3_kb": 0,
            "flags": cpu_flags,
            "virtualization": "Unknown",
        },
        "gpu": [],
        "memory": {
            "total_mb": total_mb,
            "available_mb": available_mb,
            "used_mb": used_mb,
            "percent_used": round((used_mb / total_mb) * 100.0, 3) if total_mb else 0.0,
            "type": "Unknown",
            "speed_mhz": 0,
            "channels": 0,
            "swap_total_mb": swap_total_mb,
            "swap_used_mb": swap_used_mb,
        },
        "disks": disks,
        "os": {
            "name": platform_module.system(),
            "version": platform_module.version(),
            "codename": "",
            "kernel": platform_module.release(),
            "architecture": platform_module.machine(),
            "distribution": " ".join(platform_module.uname()),
            "desktop_environment": os.environ.get("XDG_CURRENT_DESKTOP", ""),
            "language": os.environ.get("LANG", ""),
            "timezone": time.tzname[0] if time.tzname else "",
        },
        "container": {
            "is_container": Path("/.dockerenv").exists(),
            "container_type": "docker" if Path("/.dockerenv").exists() else "None",
            "is_vm": False,
            "vm_hypervisor": "None",
            "cgroup_limits": {},
        },
        "network": {
            "hostname": hostname,
            "ip_address": ip_address,
            "mac_address": "",
            "interface_name": "",
            "is_wifi": False,
        },
        "environment_variables": relevant_env,
        "benchmark_metadata": {},
    }


def unique_in_order(values: Iterable[str]) -> List[str]:
    seen = set()
    ordered: List[str] = []
    for value in values:
        if value in seen:
            continue
        seen.add(value)
        ordered.append(value)
    return ordered


def sort_suite_runs(artifacts: Sequence[SuiteArtifact]) -> List[SuiteArtifact]:
    order = {name: idx for idx, name in enumerate(SUITE_ORDER)}
    return sorted(artifacts, key=lambda item: (order.get(item.suite, len(order)), item.engine, item.suite))


def build_matrix_summary(run_id: str, output_root: Path, artifacts: Sequence[SuiteArtifact]) -> Dict[str, Any]:
    ordered = sort_suite_runs(artifacts)
    engines = unique_in_order(item.engine for item in ordered)
    suites = unique_in_order(item.suite for item in ordered)
    failed = sum(1 for item in ordered if item.status not in ("passed", "skipped"))
    durations = [item.duration_seconds for item in ordered if item.duration_seconds is not None]
    suite_runs = [
        {
            "engine": item.engine,
            "suite": item.suite,
            "started_at": item.started_at,
            "duration_seconds": round(item.duration_seconds, 6),
            "status": item.status,
            "exit_code": item.exit_code,
            "output_dir": str(item.output_dir.relative_to(output_root)),
        }
        for item in ordered
    ]
    return {
        "run_id": run_id,
        "output_root": str(output_root),
        "started_at_utc": utc_now_iso(),
        "completed_at_utc": utc_now_iso(),
        "duration_seconds": round(sum(durations), 6) if durations else 0.0,
        "engines_requested": engines,
        "suites_requested": suites,
        "result": "passed" if failed == 0 else "failed",
        "failed_suite_runs": failed,
        "total_suite_runs": len(suite_runs),
        "suite_runs": suite_runs,
    }


def write_matrix_runs(output_root: Path, suite_runs: Sequence[Mapping[str, Any]]) -> None:
    path = output_root / ".matrix-runs.tsv"
    with path.open("w", encoding="utf-8") as handle:
        for item in suite_runs:
            handle.write(
                "{engine}|{suite}|{started_at}|{duration_seconds}|{exit_code}|{status}|{output_dir}\n".format(
                    **item
                )
            )


def load_unified_matrix(path: Path) -> Tuple[List[str], Dict[Tuple[str, str], Dict[str, str]]]:
    with path.open(newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle)
        engines = [name for name in reader.fieldnames or [] if name not in ("run_id", "suite", "metric")]
        rows: Dict[Tuple[str, str], Dict[str, str]] = {}
        for row in reader:
            rows[(row["suite"], row["metric"])] = {engine: row.get(engine, "") for engine in engines}
        return engines, rows


def resolve_baseline_summary_path(baseline_root: Path | None) -> tuple[Path | None, Path | None]:
    if baseline_root is None:
        return None, None

    summary_path: Path | None = None
    if baseline_root.is_file() and baseline_root.name == "matrix-summary.json":
        summary_path = baseline_root
        baseline_root = baseline_root.parent
    elif baseline_root.is_dir() and (baseline_root / "matrix-summary.json").exists():
        summary_path = baseline_root / "matrix-summary.json"
    elif baseline_root.is_dir():
        candidates = sorted(baseline_root.glob("*/matrix-summary.json"), key=lambda path: path.parent.name, reverse=True)
        for candidate in candidates:
            try:
                payload = load_json(candidate)
            except Exception:
                continue
            suite_runs = list(payload.get("suite_runs", []))
            total_suite_runs = int(payload.get("total_suite_runs", len(suite_runs)))
            result = str(payload.get("result", "passed" if total_suite_runs > 0 else "unknown"))
            if result == "passed" and total_suite_runs > 0:
                summary_path = candidate
                baseline_root = candidate.parent
                break
        if summary_path is None and candidates:
            summary_path = candidates[0]
            baseline_root = summary_path.parent
    return summary_path, baseline_root


def parse_benchmark_matrix_artifacts(benchmarks_root: Path | None, output_root: Path) -> List[SuiteArtifact]:
    summary_path, matrix_root = resolve_baseline_summary_path(benchmarks_root)
    if summary_path is None or matrix_root is None:
        return []

    summary = load_json(summary_path)
    suite_runs = list(summary.get("suite_runs", []))
    if not suite_runs:
        return []

    import_metadata = {
        "source_run_id": summary.get("run_id", ""),
        "source_summary_json": str(summary_path),
        "source_output_root": str(matrix_root),
        "suite_runs_imported": len(suite_runs),
        "suite_families_imported": sorted({str(item.get("suite", "")) for item in suite_runs if item.get("suite")}),
    }
    write_json(output_root / "benchmark-matrix-import.json", import_metadata)

    artifacts: List[SuiteArtifact] = []
    for item in suite_runs:
        engine = str(item.get("engine", "")).strip()
        suite = str(item.get("suite", "")).strip()
        if not engine or not suite:
            continue
        source_dir = resolve_relative(matrix_root, str(item.get("output_dir", "")).strip())
        if source_dir is None or not source_dir.exists():
            continue
        destination_dir = output_root / engine / suite
        copy_tree_replace(source_dir, destination_dir)
        artifacts.append(
            SuiteArtifact(
                engine=engine,
                suite=suite,
                status=str(item.get("status", "unknown")),
                exit_code=safe_int(item.get("exit_code")) or 0,
                started_at=str(item.get("started_at", summary.get("started_at_utc", ""))),
                duration_seconds=safe_float(item.get("duration_seconds")) or 0.0,
                output_dir=destination_dir,
                summary_path=destination_dir,
            )
        )
    return artifacts


def write_baseline_comparison(
    output_root: Path,
    current_unified: Path,
    baseline_root: Path | None,
) -> None:
    summary_path, baseline_root = resolve_baseline_summary_path(baseline_root)
    if summary_path is None or baseline_root is None:
        return

    baseline_summary = load_json(summary_path)
    baseline_unified = baseline_root / "matrix-comparison-unified.csv"
    if not baseline_unified.exists():
        generate_unified_comparison_csv.write_unified_csv(summary_path, baseline_unified)

    current_engines, current_rows = load_unified_matrix(current_unified)
    baseline_engines, baseline_rows = load_unified_matrix(baseline_unified)
    all_keys = sorted(set(current_rows) | set(baseline_rows))

    csv_path = output_root / "benchmark-baseline-comparison.csv"
    md_path = output_root / "benchmark-baseline-comparison.md"
    fieldnames = ["suite", "metric"] + [f"current.{engine}" for engine in current_engines] + [
        f"baseline.{engine}" for engine in baseline_engines
    ]
    with csv_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        for suite, metric in all_keys:
            row: Dict[str, str] = {"suite": suite, "metric": metric}
            for engine in current_engines:
                row[f"current.{engine}"] = current_rows.get((suite, metric), {}).get(engine, "")
            for engine in baseline_engines:
                row[f"baseline.{engine}"] = baseline_rows.get((suite, metric), {}).get(engine, "")
            writer.writerow(row)

    comparable_rows: List[Tuple[str, str, str, str, str]] = []
    common_keys = sorted(set(current_rows) & set(baseline_rows))
    for suite, metric in common_keys:
        current_values = current_rows.get((suite, metric), {})
        baseline_values = baseline_rows.get((suite, metric), {})
        for engine in sorted(set(current_engines) & set(baseline_engines)):
            current_value = current_values.get(engine, "")
            baseline_value = baseline_values.get(engine, "")
            if current_value == "" and baseline_value == "":
                continue
            comparable_rows.append((suite, metric, engine, current_value, baseline_value))

    baseline_suite_runs = list(baseline_summary.get("suite_runs", []))
    baseline_key_metrics: List[Tuple[str, str, str, str]] = []
    wanted_metrics = {
        "matrix.duration_seconds",
        "matrix.status",
        "summary.passed",
        "summary.failed",
        "summary.errors",
        "summary.total_steps",
        "summary.total_runs",
        "totals.passed",
        "totals.failed",
        "totals.errors",
        "totals.total",
    }
    for suite, metric in sorted(baseline_rows):
        if metric not in wanted_metrics:
            continue
        values = baseline_rows[(suite, metric)]
        for engine in baseline_engines:
            value = values.get(engine, "")
            if value == "":
                continue
            baseline_key_metrics.append((suite, metric, engine, value))

    lines = [
        "# Benchmark Baseline Comparison",
        "",
        f"- Current matrix: `{current_unified}`",
        f"- Baseline matrix: `{baseline_unified}`",
        "",
        "## Common Metrics",
        "",
    ]
    if comparable_rows:
        lines.extend(
            [
                "| Suite | Metric | Engine | Current | Baseline |",
                "|---|---|---|---:|---:|",
            ]
        )
        for suite, metric, engine, current_value, baseline_value in comparable_rows[:80]:
            lines.append(f"| `{suite}` | `{metric}` | `{engine}` | {current_value or ''} | {baseline_value or ''} |")
    else:
        lines.append("No exact `(suite, metric, engine)` overlap exists between the current ScratchBird full-run matrix and the imported benchmark baseline.")
    lines.extend(["", "## Baseline Suite Health", ""])
    if baseline_suite_runs:
        lines.extend(
            [
                "| Engine | Suite | Status | Exit | Duration (s) |",
                "|---|---|---|---:|---:|",
            ]
        )
        for item in baseline_suite_runs[:120]:
            lines.append(
                f"| `{item.get('engine', '')}` | `{item.get('suite', '')}` | {item.get('status', '')} | {item.get('exit_code', '')} | {item.get('duration_seconds', '')} |"
            )
    else:
        lines.append("Baseline summary did not record any suite runs.")
    lines.extend(["", "## Baseline Key Metrics", ""])
    if baseline_key_metrics:
        lines.extend(
            [
                "| Suite | Metric | Engine | Baseline |",
                "|---|---|---|---:|",
            ]
        )
        for suite, metric, engine, value in baseline_key_metrics[:120]:
            lines.append(f"| `{suite}` | `{metric}` | `{engine}` | {value} |")
    else:
        lines.append("No key metric rows were available from the imported benchmark baseline.")
    md_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_history_comparison(
    output_root: Path,
    current_unified: Path,
    history_root: Path,
    history_limit: int,
) -> None:
    current_summary = output_root / "matrix-summary.json"
    history_dirs = [
        item
        for item in sorted(history_root.iterdir(), key=lambda path: path.name, reverse=True)
        if item.is_dir() and item != output_root and (item / "matrix-summary.json").exists()
    ] if history_root.exists() else []
    history_dirs = history_dirs[:history_limit]
    if not history_dirs:
        return

    current_engines, current_rows = load_unified_matrix(current_unified)
    csv_path = output_root / "history-comparison.csv"
    md_path = output_root / "history-comparison.md"
    fieldnames = ["prior_run_id", "suite", "metric", "engine", "current_value", "prior_value", "delta", "delta_pct"]
    md_lines = [
        "# Recent ScratchBird Run Comparison",
        "",
        f"- Current matrix: `{current_summary}`",
        f"- Prior runs considered: `{len(history_dirs)}`",
        "",
        "| Prior Run | Suite | Metric | Engine | Current | Prior | Delta | Delta % |",
        "|---|---|---|---|---:|---:|---:|---:|",
    ]
    with csv_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        for history_dir in history_dirs:
            prior_summary = load_json(history_dir / "matrix-summary.json")
            prior_run_id = str(prior_summary.get("run_id", history_dir.name))
            prior_unified = history_dir / "matrix-comparison-unified.csv"
            if not prior_unified.exists():
                continue
            prior_engines, prior_rows = load_unified_matrix(prior_unified)
            for suite, metric in sorted(set(current_rows) & set(prior_rows)):
                if (suite, metric) not in HISTORY_KEY_METRICS and metric != "matrix.duration_seconds":
                    continue
                current_values = current_rows[(suite, metric)]
                prior_values = prior_rows[(suite, metric)]
                shared_engines = sorted(
                    engine
                    for engine in (set(current_engines) & set(prior_engines))
                    if current_values.get(engine, "") != "" or prior_values.get(engine, "") != ""
                )
                for engine in shared_engines:
                    current_value = current_values.get(engine, "")
                    prior_value = prior_values.get(engine, "")
                    delta = ""
                    delta_pct = ""
                    current_num = safe_float(current_value)
                    prior_num = safe_float(prior_value)
                    if current_num is not None and prior_num is not None:
                        delta_num = round(current_num - prior_num, 6)
                        delta = str(delta_num)
                        if not math.isclose(prior_num, 0.0):
                            delta_pct = str(round((delta_num / prior_num) * 100.0, 6))
                    row = {
                        "prior_run_id": prior_run_id,
                        "suite": suite,
                        "metric": metric,
                        "engine": engine,
                        "current_value": current_value,
                        "prior_value": prior_value,
                        "delta": delta,
                        "delta_pct": delta_pct,
                    }
                    writer.writerow(row)
                    if len(md_lines) < 90:
                        md_lines.append(
                            f"| `{prior_run_id}` | `{suite}` | `{metric}` | `{engine}` | {current_value} | {prior_value} | {delta} | {delta_pct} |"
                        )
    md_path.write_text("\n".join(md_lines) + "\n", encoding="utf-8")


def normalize(args: argparse.Namespace) -> Path:
    repo_root = args.repo_root.resolve()
    run_id = args.run_id or utc_now_stamp()
    output_root = (args.output_root or (repo_root / "tests" / "results" / "full_run_metrics" / run_id)).resolve()
    ensure_dir(output_root)
    clean_output_root(output_root)
    ensure_dir(output_root)

    full_gate_dir = args.full_gate_dir.resolve() if args.full_gate_dir else latest_subdir(repo_root / "tests" / "results" / "full_gate")
    public_beta_dir = args.public_beta_dir.resolve() if args.public_beta_dir else latest_subdir(
        repo_root / "tests" / "conformance" / "public_beta" / "results",
        require="SUMMARY.md",
    )
    compatibility_root = (args.compatibility_root or (repo_root / "tests" / "compatibility")).resolve()
    v3_native_inet_dir = args.v3_native_inet_dir.resolve() if args.v3_native_inet_dir else latest_subdir(
        repo_root / "tests" / "conformance" / "v3_native_inet" / "results" / "ctest",
        require="RUN_MANIFEST.json",
    )
    v3_native_comparative_dir = args.v3_native_comparative_dir.resolve() if args.v3_native_comparative_dir else latest_subdir(
        repo_root / "tests" / "conformance" / "v3_native_comparative_regression" / "results" / "ctest",
        require="RUN_MANIFEST.json",
    )
    verification_root = (args.verification_root or (repo_root / "scripts" / "verification_bundle" / "suite" / "results")).resolve()
    anchor_timestamp = anchor_timestamp_for_run(full_gate_dir)

    artifacts: List[SuiteArtifact] = []
    if full_gate_dir:
        full_gate_artifact = parse_full_gate(full_gate_dir, repo_root, output_root)
        if full_gate_artifact:
            artifacts.append(full_gate_artifact)
    if public_beta_dir:
        public_beta_artifact = parse_public_beta(public_beta_dir, output_root)
        if public_beta_artifact:
            artifacts.append(public_beta_artifact)
    artifacts.extend(parse_compatibility_manifests(compatibility_root, output_root))
    artifacts.extend(parse_emulation_comparison_manifests(compatibility_root, output_root))
    if v3_native_inet_dir:
        v3_artifact = parse_v3_native_inet(v3_native_inet_dir, output_root)
        if v3_artifact:
            artifacts.append(v3_artifact)
    if v3_native_comparative_dir:
        artifacts.extend(parse_native_comparative(v3_native_comparative_dir, output_root))
    artifacts.extend(parse_verification_perf(verification_root, output_root, anchor_timestamp))
    artifacts.extend(parse_optimizer_compare(verification_root, output_root, anchor_timestamp))
    benchmarks_root = args.benchmarks_root.resolve() if args.benchmarks_root else None
    artifacts.extend(parse_benchmark_matrix_artifacts(benchmarks_root, output_root))

    matrix_summary = build_matrix_summary(run_id, output_root, artifacts)
    summary_path = output_root / "matrix-summary.json"
    write_json(summary_path, matrix_summary)
    write_matrix_runs(output_root, matrix_summary["suite_runs"])
    generate_unified_comparison_csv.write_unified_csv(summary_path, output_root / "matrix-comparison-unified.csv")

    if not args.skip_system_info:
        write_json(output_root / "system-info.json", collect_system_info())

    write_baseline_comparison(output_root, output_root / "matrix-comparison-unified.csv", benchmarks_root)

    history_root = (args.history_root or (repo_root / "tests" / "results" / "full_run_metrics")).resolve()
    write_history_comparison(output_root, output_root / "matrix-comparison-unified.csv", history_root, args.history_limit)

    return output_root


def main() -> int:
    args = parse_args()
    output_root = normalize(args)
    print(output_root)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
