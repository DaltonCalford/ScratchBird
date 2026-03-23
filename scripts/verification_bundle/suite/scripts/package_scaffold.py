#!/usr/bin/env python3
from __future__ import annotations

import json
import subprocess
from pathlib import Path
from typing import Dict, Mapping

from db_adapters import resolve_candidate_binary


def discover_engine_package_scaffold(
    engine: Mapping[str, object], repo_root: Path
) -> Dict[str, object]:
    engine_id = str(engine.get("id", "unknown"))
    cfg = engine.get("package_scaffold", {}) or {}
    row: Dict[str, object] = {
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

    parser_candidates = list(cfg.get("parser_candidates", []) or [])
    if not parser_candidates:
        row["status"] = "missing_config"
        row["message"] = "package_scaffold.parser_candidates is not configured"
        return row

    try:
        parser_binary = resolve_candidate_binary(parser_candidates, repo_root)
    except RuntimeError as exc:
        row["status"] = "missing_binary"
        row["message"] = str(exc)
        return row

    row["parser_binary"] = parser_binary

    try:
        proc = subprocess.run(
            [parser_binary, "--print-package-scaffold"],
            text=True,
            capture_output=True,
            timeout=30,
            check=False,
        )
    except Exception as exc:  # pragma: no cover - defensive for harness discovery
        row["status"] = "discovery_error"
        row["message"] = str(exc)
        return row

    if proc.returncode != 0:
        row["status"] = "discovery_error"
        row["message"] = (proc.stderr or proc.stdout or "parser scaffold probe failed").strip()
        return row

    try:
        payload = json.loads(proc.stdout)
    except json.JSONDecodeError as exc:
        row["status"] = "invalid_payload"
        row["message"] = f"invalid scaffold payload: {exc}"
        return row

    row["profile_id"] = str(payload.get("profile_id", ""))
    row["parser_package"] = str(payload.get("parser_package", ""))
    row["compiler_udr_package"] = str(payload.get("compiler_udr_package", ""))
    row["emulation_udr_package"] = str(payload.get("emulation_udr_package", ""))
    row["bundle_contract_id"] = str(payload.get("bundle_contract_id", ""))

    mismatches = []
    for expected_key, actual_key in (
        ("profile_id", "profile_id"),
        ("parser_package", "parser_package"),
        ("compiler_udr_package", "compiler_udr_package"),
        ("emulation_udr_package", "emulation_udr_package"),
        ("bundle_contract_id", "bundle_contract_id"),
    ):
        expected_value = str(cfg.get(expected_key, "") or "").strip()
        if not expected_value:
            continue
        actual_value = str(row.get(actual_key, "") or "").strip()
        if actual_value != expected_value:
            mismatches.append(f"{expected_key} expected={expected_value} actual={actual_value}")

    if mismatches:
        row["status"] = "mismatch"
        row["message"] = "; ".join(mismatches)
        return row

    row["status"] = "ok"
    row["message"] = "installed parser scaffold matches expected package bundle"
    return row
