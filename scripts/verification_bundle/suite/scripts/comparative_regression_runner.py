#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import os
import re
import time
from pathlib import Path
from typing import Dict, List, Mapping, Tuple

from common_io import ensure_dir, load_structured, write_csv, write_json
from db_adapters import compare_assertions, resolve_client_binary, resolve_sql_path, run_sql_file


CONTRACT_ID = "native-v3-comparative-regression-v1"
SUITE_FAMILY = "native-comparative-regression"
STATIC_TRANSLATION_MODE = "static_native_v3"
TRANSLATION_CONTRACT_ID = "vncr-frozen-static-corpus-v1"
ALLOWED_TEMPLATE_TOKENS = ("__VNCR_NS__",)
TEMPLATE_TOKEN_RE = re.compile(r"__VNCR_[A-Z0-9_]+__")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Static donor-vs-native comparative regression runner.")
    parser.add_argument("--engines", required=True, help="Engine config JSON/YAML path.")
    parser.add_argument("--corpus", required=True, help="Comparative corpus JSON/YAML path.")
    parser.add_argument("--out-dir", required=True, help="Result output directory.")
    parser.add_argument(
        "--workspace-root",
        default=None,
        help="Verification workspace root (default: parent of scripts dir).",
    )
    parser.add_argument(
        "--repo-root",
        default=None,
        help="Repository clone root. Defaults to SB_VERIFY_REPO_ROOT or <workspace-root>/repos.",
    )
    return parser.parse_args()


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


def normalize_lines(lines: List[str], order_sensitive: bool) -> List[str]:
    cleaned = [line.strip() for line in lines if line.strip()]
    return cleaned if order_sensitive else sorted(cleaned)


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def ensure_static_sql_path(sql_ref: str, workspace_root: Path, expected_root: str, case_id: str, role: str, stage: str) -> Dict[str, str]:
    path = resolve_sql_path(sql_ref, workspace_root)
    if not path.exists():
        raise RuntimeError(f"{case_id} {role} {stage}: missing SQL artifact: {sql_ref}")
    expected_prefix = (workspace_root / expected_root).resolve()
    try:
        relative_path = path.relative_to(expected_prefix)
    except ValueError as exc:
        raise RuntimeError(
            f"{case_id} {role} {stage}: SQL artifact must remain under {expected_root}: {path}"
        ) from exc

    text = path.read_text(encoding="utf-8", errors="ignore")
    tokens = sorted({token for token in ALLOWED_TEMPLATE_TOKENS if token in text})
    residual = text
    for token in ALLOWED_TEMPLATE_TOKENS:
        residual = residual.replace(token, "")
    unexpected = sorted(set(TEMPLATE_TOKEN_RE.findall(residual)))
    if "__VNCR_" in residual and not unexpected:
        unexpected = ["unresolved __VNCR_ token fragment"]
    if unexpected:
        raise RuntimeError(
            f"{case_id} {role} {stage}: unsupported runtime template tokens {unexpected} in {path}"
        )
    return {
        "sql_ref": sql_ref,
        "resolved_path": str(path),
        "relative_path": str(relative_path),
        "sha256": file_sha256(path),
        "template_tokens": ",".join(tokens),
    }


def validate_case_contract(
    case: Mapping[str, object],
    workspace_root: Path,
    donor_lanes: List[str],
) -> List[Dict[str, str]]:
    case_id = str(case["id"])
    translation_mode = str(case.get("translation_mode", "")).strip()
    if translation_mode != STATIC_TRANSLATION_MODE:
        raise RuntimeError(
            f"{case_id}: translation_mode must be {STATIC_TRANSLATION_MODE}, got {translation_mode or '<empty>'}"
        )

    provenance = case.get("provenance", {}) or {}
    required_provenance = (
        "donor_source_path",
        "donor_converted_path",
        "donor_curated_list",
        "donor_statement_scope",
    )
    for field in required_provenance:
        if not str(provenance.get(field, "")).strip():
            raise RuntimeError(f"{case_id}: missing provenance field {field}")

    surfaces = case.get("surfaces", {}) or {}
    native_surface = surfaces.get("native")
    if not native_surface:
        raise RuntimeError(f"{case_id}: missing native surface")

    inventory_rows: List[Dict[str, str]] = []
    for stage in ("setup", "exec", "teardown"):
        sql_ref = native_surface.get(stage) if isinstance(native_surface, Mapping) else None
        if not sql_ref:
            continue
        info = ensure_static_sql_path(
            str(sql_ref),
            workspace_root,
            "cases/comparative/sql/native",
            case_id,
            "native",
            stage,
        )
        inventory_rows.append(
            {
                "case_id": case_id,
                "dialect_family": "native",
                "role": "native",
                "stage": stage,
                "translation_mode": translation_mode,
                "sql_ref": info["sql_ref"],
                "resolved_path": info["resolved_path"],
                "relative_path": info["relative_path"],
                "sha256": info["sha256"],
                "template_tokens": info["template_tokens"],
                "donor_source_path": str(provenance.get("donor_source_path", "")),
                "donor_converted_path": str(provenance.get("donor_converted_path", "")),
            }
        )

    for donor_lane in donor_lanes:
        donor_surface = surfaces.get(donor_lane)
        if not donor_surface:
            continue
        for stage in ("setup", "exec", "teardown"):
            sql_ref = donor_surface.get(stage) if isinstance(donor_surface, Mapping) else None
            if not sql_ref:
                continue
            info = ensure_static_sql_path(
                str(sql_ref),
                workspace_root,
                f"cases/comparative/sql/{donor_lane}",
                case_id,
                donor_lane,
                stage,
            )
            inventory_rows.append(
                {
                    "case_id": case_id,
                    "dialect_family": donor_lane,
                    "role": "donor",
                    "stage": stage,
                    "translation_mode": translation_mode,
                    "sql_ref": info["sql_ref"],
                    "resolved_path": info["resolved_path"],
                    "relative_path": info["relative_path"],
                    "sha256": info["sha256"],
                    "template_tokens": info["template_tokens"],
                    "donor_source_path": str(provenance.get("donor_source_path", "")),
                    "donor_converted_path": str(provenance.get("donor_converted_path", "")),
                }
            )
    return inventory_rows


def validate_corpus_contract(corpus_cfg: Mapping[str, object]) -> None:
    contract = corpus_cfg.get("translation_contract", {}) or {}
    contract_id = str(contract.get("id", "")).strip()
    if contract_id != TRANSLATION_CONTRACT_ID:
        raise RuntimeError(
            f"comparative corpus must declare translation_contract.id={TRANSLATION_CONTRACT_ID}"
        )
    if bool(contract.get("runtime_translation_allowed", True)):
        raise RuntimeError("comparative corpus must prohibit runtime translation")
    runtime_tokens = tuple(contract.get("runtime_substitution_only", []) or [])
    if runtime_tokens != ALLOWED_TEMPLATE_TOKENS:
        raise RuntimeError(
            "comparative corpus runtime_substitution_only must exactly match the allowed template token set"
        )
    required_mode = str(contract.get("required_translation_mode", "")).strip()
    if required_mode != STATIC_TRANSLATION_MODE:
        raise RuntimeError(
            f"comparative corpus must require translation_mode={STATIC_TRANSLATION_MODE}"
        )


def corpus_fingerprint(inventory_rows: List[Mapping[str, str]]) -> str:
    digest = hashlib.sha256()
    for row in sorted(
        inventory_rows,
        key=lambda item: (
            item.get("case_id", ""),
            item.get("dialect_family", ""),
            item.get("role", ""),
            item.get("stage", ""),
            item.get("relative_path", ""),
        ),
    ):
        digest.update(
            (
                f"{row.get('case_id','')}|{row.get('dialect_family','')}|{row.get('role','')}|"
                f"{row.get('stage','')}|{row.get('relative_path','')}|{row.get('sha256','')}\n"
            ).encode("utf-8")
        )
    return digest.hexdigest()


def render_sql(sql_ref: str | None, workspace_root: Path, stage_dir: Path, namespace: str, case_id: str, engine_id: str, stage: str) -> Path | None:
    if not sql_ref:
        return None
    source_path = resolve_sql_path(sql_ref, workspace_root)
    text = source_path.read_text(encoding="utf-8", errors="ignore")
    text = text.replace("__VNCR_NS__", namespace)
    out_path = stage_dir / f"{case_id}.{engine_id}.{stage}.sql"
    ensure_dir(out_path.parent)
    out_path.write_text(text, encoding="utf-8")
    return out_path


def run_stage(
    engine: Mapping[str, object],
    binary: str,
    sql_ref: str | None,
    workspace_root: Path,
    stage_dir: Path,
    namespace: str,
    case_id: str,
    engine_id: str,
    stage: str,
    timeout_seconds: int,
) -> Dict[str, object]:
    sql_path = render_sql(sql_ref, workspace_root, stage_dir, namespace, case_id, engine_id, stage)
    if sql_path is None:
        return {
            "status": "skipped",
            "returncode": 0,
            "sqlstate": "00000",
            "message": "",
            "stdout": "",
            "stderr": "",
            "elapsed_ms": 0,
            "assert_lines": [],
            "stdout_path": "",
            "stderr_path": "",
        }
    return run_sql_file(engine, binary, sql_path, stage_dir / "raw", stage, timeout_seconds).__dict__


def run_case_on_engine(
    engine: Mapping[str, object],
    binary: str,
    surface: Mapping[str, object],
    workspace_root: Path,
    run_dir: Path,
    namespace: str,
    case_id: str,
    timeout_seconds: int,
) -> Dict[str, object]:
    engine_id = str(engine.get("id", "unknown"))
    return {
        "setup": run_stage(engine, binary, surface.get("setup"), workspace_root, run_dir, namespace, case_id, engine_id, "setup", timeout_seconds),
        "exec": run_stage(engine, binary, surface.get("exec"), workspace_root, run_dir, namespace, case_id, engine_id, "exec", timeout_seconds),
        "teardown": run_stage(engine, binary, surface.get("teardown"), workspace_root, run_dir, namespace, case_id, engine_id, "teardown", timeout_seconds),
    }


def stage_ok(stage: Mapping[str, object]) -> bool:
    return str(stage.get("status", "error")) in ("ok", "skipped")


def failure_class(sqlstate: str) -> str:
    value = (sqlstate or "").strip()
    return value[:2] if len(value) >= 2 else value


def latency_ratio_ms(native_ms: int, donor_ms: int) -> float:
    if donor_ms <= 0:
        return 1.0
    return float(max(1, native_ms)) / float(max(1, donor_ms))


def evaluate_case(
    expectation: str,
    order_sensitive: bool,
    native_run: Mapping[str, object],
    donor_run: Mapping[str, object],
) -> Tuple[bool, str]:
    native_setup = native_run["setup"]
    donor_setup = donor_run["setup"]
    native_exec = native_run["exec"]
    donor_exec = donor_run["exec"]
    native_teardown = native_run["teardown"]
    donor_teardown = donor_run["teardown"]

    if not stage_ok(native_setup) or not stage_ok(donor_setup):
        return False, (
            f"setup failed native={native_setup.get('status')} donor={donor_setup.get('status')}"
        )

    if expectation == "must_match":
        if str(native_exec.get("status")) != "ok" or str(donor_exec.get("status")) != "ok":
            return False, (
                f"expected both ok native={native_exec.get('status')} donor={donor_exec.get('status')}"
            )
        ok, reason = compare_assertions(
            normalize_lines(list(native_exec.get("assert_lines", []) or []), order_sensitive),
            normalize_lines(list(donor_exec.get("assert_lines", []) or []), order_sensitive),
            order_sensitive=True,
        )
        if not ok:
            return False, reason
    elif expectation == "must_fail_same_class":
        if str(native_exec.get("status")) == "ok" or str(donor_exec.get("status")) == "ok":
            return False, (
                f"expected both fail native={native_exec.get('status')} donor={donor_exec.get('status')}"
            )
        native_class = failure_class(str(native_exec.get("sqlstate", "")))
        donor_class = failure_class(str(donor_exec.get("sqlstate", "")))
        if native_class != donor_class:
            return False, (
                f"sqlstate class mismatch native={native_exec.get('sqlstate')} donor={donor_exec.get('sqlstate')}"
            )
    else:
        return False, f"unsupported expectation={expectation}"

    if not stage_ok(native_teardown) or not stage_ok(donor_teardown):
        return False, (
            f"teardown failed native={native_teardown.get('status')} donor={donor_teardown.get('status')}"
        )
    return True, ""


def write_pairwise_markdown(path: Path, rows: List[Mapping[str, object]]) -> None:
    lines = [
        "# Native Comparative Regression Pairwise Verdicts",
        "",
        "| Case | Dialect | Expectation | Result | Native Exec ms | Donor Exec ms | Ratio |",
        "|---|---|---|---|---|---|---|",
    ]
    for row in rows:
        lines.append(
            f"| `{row['case_id']}` | `{row['dialect_family']}` | `{row['expectation']}` | `{row['result']}` | "
            f"`{row['native_exec_elapsed_ms']}` | `{row['donor_exec_elapsed_ms']}` | `{row['latency_ratio']}` |"
        )
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


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
            corpus_cfg.get("defaults", {}).get("timeout_seconds", 180),
        )
    )
    validate_corpus_contract(corpus_cfg)
    enabled = [engine for engine in engines_cfg.get("engines", []) if engine.get("enabled", True)]
    native_engines = [engine for engine in enabled if str(engine.get("family")) == "scratchbird" and str(engine.get("mode")) == "native"]
    if len(native_engines) != 1:
        raise RuntimeError("comparative regression requires exactly one enabled ScratchBird native engine")
    native_engine = native_engines[0]
    donors_by_lane = {
        lane_for_engine(engine): engine
        for engine in enabled
        if str(engine.get("family")) == "reference"
    }

    run_id = time.strftime("%Y%m%d_%H%M%S")
    run_dir = out_dir / run_id
    ensure_dir(run_dir / "raw")

    native_binary = resolve_client_binary(native_engine, repo_root)
    engine_rows: List[Dict[str, object]] = []
    pair_rows: List[Dict[str, object]] = []
    translation_inventory: List[Dict[str, str]] = []
    summary = {
        "suite_family": SUITE_FAMILY,
        "contract_id": CONTRACT_ID,
        "total_cases": 0,
        "total_pairs": 0,
        "passed_pairs": 0,
        "failed_pairs": 0,
        "positive_pairs": 0,
        "negative_pairs": 0,
        "pair_pass_rate": 0.0,
    }

    cases = [case for case in corpus_cfg.get("cases", []) if case.get("enabled", True)]
    donor_lanes = sorted(set(donors_by_lane.keys()))
    for case in cases:
        translation_inventory.extend(validate_case_contract(case, workspace_root, donor_lanes))

    for case in cases:
        if not case.get("enabled", True):
            continue
        summary["total_cases"] += 1

        case_id = str(case["id"])
        surfaces = case.get("surfaces", {}) or {}
        expectation = str(case.get("expectation", "must_match"))
        order_sensitive = bool((case.get("normalization", {}) or {}).get("order_sensitive", False))
        behavior_class = str(case.get("behavior_class", ""))
        translation_mode = str(case.get("translation_mode", "static_translation"))
        provenance = case.get("provenance", {}) or {}

        native_surface = surfaces.get("native")
        if not native_surface:
            continue

        for donor_lane, donor_engine in donors_by_lane.items():
            donor_surface = surfaces.get(donor_lane)
            if not donor_surface:
                continue

            summary["total_pairs"] += 1
            if expectation == "must_fail_same_class":
                summary["negative_pairs"] += 1
            else:
                summary["positive_pairs"] += 1

            namespace = f"vncr_{abs(hash((case_id, donor_lane, run_id))) & 0xFFFFFF:06x}"
            case_dir = run_dir / case_id / donor_lane
            ensure_dir(case_dir)

            donor_binary = resolve_client_binary(donor_engine, repo_root)
            native_run = run_case_on_engine(
                native_engine,
                native_binary,
                native_surface,
                workspace_root,
                case_dir / "native",
                namespace,
                case_id,
                timeout_seconds,
            )
            donor_run = run_case_on_engine(
                donor_engine,
                donor_binary,
                donor_surface,
                workspace_root,
                case_dir / "donor",
                namespace,
                case_id,
                timeout_seconds,
            )

            ok, reason = evaluate_case(expectation, order_sensitive, native_run, donor_run)
            if ok:
                summary["passed_pairs"] += 1
            else:
                summary["failed_pairs"] += 1

            native_exec = native_run["exec"]
            donor_exec = donor_run["exec"]
            latency_ratio = round(
                latency_ratio_ms(
                    int(native_exec.get("elapsed_ms", 0)),
                    int(donor_exec.get("elapsed_ms", 0)),
                ),
                6,
            )

            engine_rows.append(
                {
                    "case_id": case_id,
                    "engine_id": str(native_engine["id"]),
                    "dialect_family": donor_lane,
                    "role": "native",
                    "expectation": expectation,
                    "behavior_class": behavior_class,
                    "translation_mode": translation_mode,
                    "exec_status": native_exec.get("status", ""),
                    "sqlstate": native_exec.get("sqlstate", ""),
                    "assert_count": len(native_exec.get("assert_lines", []) or []),
                    "exec_elapsed_ms": native_exec.get("elapsed_ms", 0),
                    "donor_source_path": provenance.get("donor_source_path", ""),
                    "donor_converted_path": provenance.get("donor_converted_path", ""),
                }
            )
            engine_rows.append(
                {
                    "case_id": case_id,
                    "engine_id": str(donor_engine["id"]),
                    "dialect_family": donor_lane,
                    "role": "donor",
                    "expectation": expectation,
                    "behavior_class": behavior_class,
                    "translation_mode": translation_mode,
                    "exec_status": donor_exec.get("status", ""),
                    "sqlstate": donor_exec.get("sqlstate", ""),
                    "assert_count": len(donor_exec.get("assert_lines", []) or []),
                    "exec_elapsed_ms": donor_exec.get("elapsed_ms", 0),
                    "donor_source_path": provenance.get("donor_source_path", ""),
                    "donor_converted_path": provenance.get("donor_converted_path", ""),
                }
            )

            pair_rows.append(
                {
                    "case_id": case_id,
                    "dialect_family": donor_lane,
                    "donor_engine_id": str(donor_engine["id"]),
                    "native_engine_id": str(native_engine["id"]),
                    "expectation": expectation,
                    "behavior_class": behavior_class,
                    "translation_mode": translation_mode,
                    "result": "pass" if ok else "fail",
                    "reason": reason,
                    "native_exec_status": native_exec.get("status", ""),
                    "donor_exec_status": donor_exec.get("status", ""),
                    "native_sqlstate": native_exec.get("sqlstate", ""),
                    "donor_sqlstate": donor_exec.get("sqlstate", ""),
                    "native_exec_elapsed_ms": native_exec.get("elapsed_ms", 0),
                    "donor_exec_elapsed_ms": donor_exec.get("elapsed_ms", 0),
                    "latency_ratio": latency_ratio,
                    "donor_source_path": provenance.get("donor_source_path", ""),
                    "donor_converted_path": provenance.get("donor_converted_path", ""),
                    "donor_curated_list": provenance.get("donor_curated_list", ""),
                    "donor_statement_scope": provenance.get("donor_statement_scope", ""),
                }
            )

            write_json(case_dir / "native_run.json", native_run)
            write_json(case_dir / "donor_run.json", donor_run)

    summary["pair_pass_rate"] = round(
        (summary["passed_pairs"] / summary["total_pairs"]) if summary["total_pairs"] else 0.0,
        6,
    )
    summary["translation_contract"] = {
        "translation_contract_id": TRANSLATION_CONTRACT_ID,
        "required_translation_mode": STATIC_TRANSLATION_MODE,
        "runtime_translation_allowed": False,
        "runtime_substitution_only": list(ALLOWED_TEMPLATE_TOKENS),
        "translation_inventory_csv": "comparative_translation_inventory.csv",
        "validated_cases": len(cases),
        "validated_sql_artifacts": len(translation_inventory),
        "native_sql_artifacts": sum(1 for row in translation_inventory if row.get("role") == "native"),
        "donor_sql_artifacts": sum(1 for row in translation_inventory if row.get("role") == "donor"),
        "corpus_fingerprint_sha256": corpus_fingerprint(translation_inventory),
    }

    write_csv(run_dir / "comparative_engine_runs.csv", engine_rows)
    write_csv(run_dir / "comparative_pairwise_scores.csv", pair_rows)
    write_csv(run_dir / "comparative_translation_inventory.csv", translation_inventory)
    write_json(run_dir / "comparative_summary.json", summary)
    write_json(
        run_dir / "comparative_pairwise.json",
        {
            "suite_family": SUITE_FAMILY,
            "contract_id": CONTRACT_ID,
            "translation_contract": summary["translation_contract"],
            "pairs": pair_rows,
        },
    )
    write_pairwise_markdown(run_dir / "comparative_pairwise.md", pair_rows)
    print(f"comparative regression run complete: {run_dir}")


if __name__ == "__main__":
    main()
