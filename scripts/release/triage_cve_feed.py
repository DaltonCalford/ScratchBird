#!/usr/bin/env python3
"""Deterministic CVE ingestion, triage, and patch-SLA evaluation."""

from __future__ import annotations

import argparse
import csv
import json
import pathlib
from datetime import datetime, timedelta, timezone
from typing import Any, Dict, List, Optional, Tuple


DEFAULT_POLICY_PATH = pathlib.Path(__file__).with_name("default_cve_triage_policy.json")


def stable_json(data: Any) -> str:
    return json.dumps(data, indent=2, sort_keys=True) + "\n"


def parse_timestamp(raw: str) -> datetime:
    normalized = raw.replace("Z", "+00:00")
    parsed = datetime.fromisoformat(normalized)
    if parsed.tzinfo is None:
        return parsed.replace(tzinfo=timezone.utc)
    return parsed.astimezone(timezone.utc)


def format_timestamp(value: datetime) -> str:
    return value.astimezone(timezone.utc).isoformat().replace("+00:00", "Z")


def load_json(path: pathlib.Path) -> Dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def load_policy(path: Optional[pathlib.Path]) -> Dict[str, Any]:
    return load_json(path or DEFAULT_POLICY_PATH)


def load_bundle_inventories(bundle_manifest_path: pathlib.Path) -> List[Dict[str, Any]]:
    bundle_dir = bundle_manifest_path.parent
    bundle_manifest = load_json(bundle_manifest_path)
    inventories: List[Dict[str, Any]] = []
    for entry in bundle_manifest.get("bundle_outputs", []):
        if entry["path"].endswith("-dependency-inventory.json"):
            inventories.append(load_json(bundle_dir / entry["path"]))
    if not inventories:
        raise ValueError("bundle manifest does not reference any dependency inventories")
    return inventories


def normalize_severity(raw: str) -> str:
    value = (raw or "unknown").strip().lower()
    if value in {"critical", "high", "medium", "low"}:
        return value
    return "unknown"


def iter_dependency_matches(
    inventories: List[Dict[str, Any]],
    package_name: str,
    package_manager: Optional[str],
) -> List[Tuple[Dict[str, Any], Dict[str, Any], Dict[str, Any]]]:
    matches: List[Tuple[Dict[str, Any], Dict[str, Any], Dict[str, Any]]] = []
    expected_manager = package_manager.strip().lower() if package_manager else None
    for inventory in inventories:
        for component in inventory.get("components", []):
            if expected_manager and component.get("manifest_type", "").lower() != expected_manager:
                continue
            for dependency in component.get("dependencies", []):
                if dependency.get("name") == package_name:
                    matches.append((inventory, component, dependency))
    return matches


def evaluate_advisories(
    advisories_path: pathlib.Path,
    bundle_manifest_path: pathlib.Path,
    policy_path: Optional[pathlib.Path],
    now: datetime,
) -> Dict[str, Any]:
    advisories_doc = load_json(advisories_path)
    inventories = load_bundle_inventories(bundle_manifest_path)
    policy = load_policy(policy_path)
    severity_defaults = policy["severity_defaults"]

    triage_records: List[Dict[str, Any]] = []
    remediation_queue: List[Dict[str, Any]] = []

    for advisory in advisories_doc.get("advisories", []):
        severity = normalize_severity(advisory.get("severity", "unknown"))
        severity_policy = severity_defaults[severity]
        published_at = parse_timestamp(advisory["published_at"])
        due_at = published_at + timedelta(days=int(severity_policy["sla_days"]))
        known_exploited = bool(advisory.get("known_exploited", False))
        matches = iter_dependency_matches(
            inventories,
            advisory["package_name"],
            advisory.get("package_manager"),
        )
        for inventory, component, dependency in matches:
            status = advisory.get("status", "open").strip().lower()
            overdue = now > due_at
            if status == "resolved":
                patch_state = "resolved"
                action = "NONE"
            elif overdue:
                patch_state = "overdue"
                action = severity_policy["breach_action"]
            elif known_exploited:
                patch_state = "known_exploited"
                action = "SECOPS_REMEDIATE"
            else:
                patch_state = "open"
                action = severity_policy["open_action"]

            days_open = max(0, int((now - published_at).total_seconds() // 86400))
            days_remaining = int((due_at - now).total_seconds() // 86400)
            triage_record = {
                "action": action,
                "advisory_id": advisory["advisory_id"],
                "component_name": component["component_name"],
                "cvss_score": advisory.get("cvss_score"),
                "days_open": days_open,
                "days_remaining": days_remaining,
                "dependency_name": dependency["name"],
                "dependency_version": dependency["version"],
                "due_at": format_timestamp(due_at),
                "fixed_version": advisory.get("fixed_version", ""),
                "known_exploited": known_exploited,
                "manifest_path": component["manifest_path"],
                "package_manager": component["manifest_type"],
                "patch_state": patch_state,
                "published_at": format_timestamp(published_at),
                "references": advisory.get("references", []),
                "repo_id": inventory["repo_id"],
                "severity": severity,
                "sla_days": int(severity_policy["sla_days"]),
                "status": status,
            }
            triage_records.append(triage_record)
            if action == "SECOPS_REMEDIATE":
                remediation_queue.append(
                    {
                        "action": action,
                        "advisory_id": advisory["advisory_id"],
                        "dependency_name": dependency["name"],
                        "due_at": triage_record["due_at"],
                        "manifest_path": component["manifest_path"],
                        "package_manager": component["manifest_type"],
                        "patch_state": patch_state,
                        "repo_id": inventory["repo_id"],
                        "severity": severity,
                    }
                )

    triage_records.sort(
        key=lambda item: (
            item["patch_state"],
            item["severity"],
            item["repo_id"],
            item["manifest_path"],
            item["dependency_name"],
            item["advisory_id"],
        )
    )
    remediation_queue.sort(
        key=lambda item: (
            item["severity"],
            item["repo_id"],
            item["manifest_path"],
            item["dependency_name"],
            item["advisory_id"],
        )
    )

    summary = {
        "advisory_count": len(advisories_doc.get("advisories", [])),
        "match_count": len(triage_records),
        "remediation_count": len(remediation_queue),
        "overdue_count": sum(1 for item in triage_records if item["patch_state"] == "overdue"),
        "known_exploited_count": sum(1 for item in triage_records if item["patch_state"] == "known_exploited"),
    }
    return {
        "schema": "scratchbird.release.cve_triage_report.v1",
        "bundle_manifest_path": str(bundle_manifest_path),
        "advisories_path": str(advisories_path),
        "policy_path": str((policy_path or DEFAULT_POLICY_PATH).resolve()),
        "generated_at": format_timestamp(now),
        "summary": summary,
        "triage_records": triage_records,
        "remediation_queue": remediation_queue,
    }


def write_triage_outputs(report: Dict[str, Any], out_dir: pathlib.Path) -> pathlib.Path:
    out_dir.mkdir(parents=True, exist_ok=True)
    report_path = out_dir / "cve-triage-report.json"
    queue_path = out_dir / "patch-remediation-queue.json"
    csv_path = out_dir / "patch-sla-status.csv"

    report_path.write_text(stable_json(report), encoding="utf-8")
    queue_path.write_text(stable_json({"remediation_queue": report["remediation_queue"]}), encoding="utf-8")

    with csv_path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=[
                "advisory_id",
                "repo_id",
                "manifest_path",
                "package_manager",
                "dependency_name",
                "severity",
                "patch_state",
                "action",
                "published_at",
                "due_at",
                "days_open",
                "days_remaining",
                "fixed_version",
            ],
        )
        writer.writeheader()
        for record in report["triage_records"]:
            writer.writerow({field: record.get(field, "") for field in writer.fieldnames})
    return report_path


def validate_outputs(out_dir: pathlib.Path) -> int:
    report_path = out_dir / "cve-triage-report.json"
    queue_path = out_dir / "patch-remediation-queue.json"
    csv_path = out_dir / "patch-sla-status.csv"
    try:
        report = load_json(report_path)
        queue = load_json(queue_path)
        if not report.get("triage_records"):
            raise ValueError("triage report has no matched records")
        if "remediation_queue" not in queue:
            raise ValueError("queue file missing remediation_queue")
        with csv_path.open("r", encoding="utf-8", newline="") as handle:
            rows = list(csv.DictReader(handle))
        if len(rows) != len(report["triage_records"]):
            raise ValueError("CSV row count does not match triage records")
    except Exception as exc:
        print(f"validation failed: {exc}", file=sys.stderr)
        return 1
    print(f"validated CVE triage bundle: {report_path}")
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    triage = subparsers.add_parser("triage", help="ingest advisories and compute patch-SLA output")
    triage.add_argument("--bundle-manifest", type=pathlib.Path, required=True)
    triage.add_argument("--advisories", type=pathlib.Path, required=True)
    triage.add_argument("--policy", type=pathlib.Path)
    triage.add_argument("--out-dir", type=pathlib.Path, required=True)
    triage.add_argument("--now", required=True, help="UTC timestamp used for deterministic SLA evaluation")

    validate = subparsers.add_parser("validate", help="validate triage outputs")
    validate.add_argument("--out-dir", type=pathlib.Path, required=True)
    return parser


def main(argv: Optional[List[str]] = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    if args.command == "triage":
        report = evaluate_advisories(
            args.advisories.resolve(),
            args.bundle_manifest.resolve(),
            args.policy.resolve() if args.policy else None,
            parse_timestamp(args.now),
        )
        report_path = write_triage_outputs(report, args.out_dir.resolve())
        print(f"wrote CVE triage bundle: {report_path}")
        return 0
    if args.command == "validate":
        return validate_outputs(args.out_dir.resolve())
    parser.error("unsupported command")
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
