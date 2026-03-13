#!/usr/bin/env python3
"""Run the non-cluster PH7 integrated gameday and validate the result."""

from __future__ import annotations

import argparse
import csv
import json
import pathlib
import re
import subprocess
import sys
from typing import Any, Dict, List, Optional


REQUIRED_AREAS = [
    "release_integrity",
    "security",
    "recovery",
    "forensics",
    "reliability",
]


GTEST_SCENARIOS = [
    {
        "scenario_id": "security_audit_integrity",
        "title": "Security audit tamper and retention drill",
        "area": "security",
        "tests": [
            "CatalogBackedAuditLoggerTest.VerifyIntegrityPassesForPersistedAuditChain",
            "CatalogBackedAuditLoggerTest.VerifyIntegrityDetectsTamperedPersistedAuditRecord",
            "CatalogBackedAuditLoggerTest.ValidateAuditPackageDetectsPayloadTamper",
            "CatalogBackedAuditLoggerTest.LegalHoldBlocksRetentionEligibilityAndAppendsEvidence",
            "OperationalSupportBundleTest.GeneratedSupportBundleRedactsSensitiveFieldsAndKeepsForensicReferences",
        ],
    },
    {
        "scenario_id": "recovery_chain_guardrails",
        "title": "Recovery rehearsal and rollback guardrail drill",
        "area": "recovery",
        "tests": [
            "RestoreValidationRehearsalTest.FullBackupValidationPassesDeterministicChecks",
            "RestoreValidationRehearsalTest.DisasterRecoveryRehearsalAppliesIncrementalChain",
            "RestoreValidationRehearsalTest.ValidationFailsClosedOnCorruptBackup",
            "BackupRollbackCheckpointTest.RehearsalRejectsChainWithoutExplicitCheckpointMarkers",
            "BackupRollbackCheckpointTest.RehearsalRejectsChainThatCrossesFormatBoundary",
        ],
    },
    {
        "scenario_id": "forensic_evidence_flow",
        "title": "Forensic lineage and derivative evidence drill",
        "area": "forensics",
        "tests": [
            "CatalogRuntimeContextExtensionContractTest.LiveTransactionPersistsRetainedLineage",
            "GarbageCollectorTest.SweepPersistsLocalEvidenceManifestBeforePruneHandoff",
            "GarbageCollectorTest.SweepBlocksPruneWhenLocalEvidencePersistenceFails",
            "GarbageCollectorTest.SweepExportsWalAfterLogForCommittedTransactionsOnly",
            "GarbageCollectorTest.SweepPageSpotAuditEmitsDeterministicFindingsWithoutInlineRepair",
            "GarbageCollectorTest.SweepShadowCaptureEmitsLogicalManifestFromRetainedEvidence",
            "GarbageCollectorTest.SweepBlocksPruneWhenShadowCapturePersistenceFails",
        ],
    },
    {
        "scenario_id": "operational_fault_response",
        "title": "Operational burn, fault, and recovery drill",
        "area": "reliability",
        "tests": [
            "WorkloadGovernanceTest.ShowsSloBudgetAutoscaleAndTuningStateThroughSqlSurface",
            "WorkloadGovernanceTest.PersistsBurnEvidenceAndTightensAdmissionWithinBounds",
            "WorkloadGovernanceTest.TriggersScaleOutAndCooldownThenScaleInFromRecordedTelemetry",
            "OperationalSupportBundleTest.ShowsAlertDashboardReadinessAndSupportBundleSafetyThroughSqlSurface",
            "SupportBundleChaosTest.RestartKeepsOperationalEvidenceReadableAndRedacted",
            "DeadlockChaosTest.DeadlockFaultInjectionResolvesWithDeterministicVictim",
            "SweepChaosTest.SweepEvidencePersistenceFailureBlocksPruneDeterministically",
            "IPCChaosTest.TcpConnectionFailureRecoversAfterServerStarts",
            "IPCChaosTest.UnixSocketFailureRecoversAfterServerStarts",
        ],
    },
]


def stable_json(data: Any) -> str:
    return json.dumps(data, indent=2, sort_keys=True) + "\n"


def repo_root_from_script() -> pathlib.Path:
    return pathlib.Path(__file__).resolve().parents[2]


def trim_text(raw: str, limit: int = 4000) -> str:
    if len(raw) <= limit:
        return raw
    return raw[: limit - 13] + "\n...[trimmed]\n"


def build_gtest_filter(test_names: List[str]) -> str:
    return ":".join(test_names)


def parse_gtest_pass_count(stdout: str) -> int:
    match = re.search(r"\[\s+PASSED\s+\]\s+(\d+)\s+tests?\.", stdout)
    if not match:
        return 0
    return int(match.group(1))


def run_command(command: List[str], cwd: pathlib.Path) -> Dict[str, Any]:
    proc = subprocess.run(
        command,
        cwd=cwd,
        capture_output=True,
        text=True,
        check=False,
    )
    return {
        "returncode": proc.returncode,
        "stdout": proc.stdout,
        "stderr": proc.stderr,
    }


def release_validation_scenarios(
    repo_root: pathlib.Path,
    bundle_manifest: pathlib.Path,
    signing_manifest: pathlib.Path,
    triage_dir: pathlib.Path,
    repro_report: pathlib.Path,
    compliance_bundle_dir: pathlib.Path,
) -> List[Dict[str, Any]]:
    release_dir = repo_root / "scripts" / "release"
    return [
        {
            "scenario_id": "release_bundle_signature",
            "title": "Signed release bundle verification",
            "area": "release_integrity",
            "kind": "validation",
            "command": [
                "python3",
                str(release_dir / "sign_release_bundle.py"),
                "verify",
                "--bundle-manifest",
                str(bundle_manifest),
                "--signing-manifest",
                str(signing_manifest),
            ],
            "expected_checks": 1,
        },
        {
            "scenario_id": "release_cve_triage_bundle",
            "title": "CVE triage bundle validation",
            "area": "release_integrity",
            "kind": "validation",
            "command": [
                "python3",
                str(release_dir / "triage_cve_feed.py"),
                "validate",
                "--out-dir",
                str(triage_dir),
            ],
            "expected_checks": 1,
        },
        {
            "scenario_id": "release_reproducibility_bundle",
            "title": "Reproducible build report validation",
            "area": "release_integrity",
            "kind": "validation",
            "command": [
                "python3",
                str(release_dir / "verify_repro_build.py"),
                "validate",
                "--report-path",
                str(repro_report),
            ],
            "expected_checks": 1,
        },
        {
            "scenario_id": "release_compliance_bundle",
            "title": "Compliance bundle validation",
            "area": "release_integrity",
            "kind": "validation",
            "command": [
                "python3",
                str(release_dir / "generate_compliance_bundle.py"),
                "validate",
                "--bundle-dir",
                str(compliance_bundle_dir),
            ],
            "expected_checks": 1,
        },
    ]


def gtest_scenarios(scratchbird_tests: pathlib.Path) -> List[Dict[str, Any]]:
    scenarios: List[Dict[str, Any]] = []
    for item in GTEST_SCENARIOS:
        tests = list(item["tests"])
        scenarios.append(
            {
                "scenario_id": item["scenario_id"],
                "title": item["title"],
                "area": item["area"],
                "kind": "gtest",
                "tests": tests,
                "expected_checks": len(tests),
                "command": [
                    str(scratchbird_tests),
                    "--gtest_color=no",
                    f"--gtest_filter={build_gtest_filter(tests)}",
                ],
            }
        )
    return scenarios


def scenario_result(scenario: Dict[str, Any], proc: Dict[str, Any]) -> Dict[str, Any]:
    if scenario["kind"] == "gtest":
        observed_checks = parse_gtest_pass_count(proc["stdout"])
    else:
        observed_checks = 1 if proc["returncode"] == 0 else 0
    status = "pass" if proc["returncode"] == 0 else "fail"
    return {
        "scenario_id": scenario["scenario_id"],
        "title": scenario["title"],
        "area": scenario["area"],
        "kind": scenario["kind"],
        "status": status,
        "expected_checks": scenario["expected_checks"],
        "observed_checks": observed_checks,
        "command": scenario["command"],
        "tests": scenario.get("tests", []),
        "stdout": trim_text(proc["stdout"]),
        "stderr": trim_text(proc["stderr"]),
    }


def write_scenario_csv(scenarios: List[Dict[str, Any]], path: pathlib.Path) -> None:
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=[
                "scenario_id",
                "area",
                "kind",
                "status",
                "expected_checks",
                "observed_checks",
                "title",
                "command",
            ],
        )
        writer.writeheader()
        for item in scenarios:
            writer.writerow(
                {
                    "scenario_id": item["scenario_id"],
                    "area": item["area"],
                    "kind": item["kind"],
                    "status": item["status"],
                    "expected_checks": item["expected_checks"],
                    "observed_checks": item["observed_checks"],
                    "title": item["title"],
                    "command": " ".join(item["command"]),
                }
            )


def render_markdown(report: Dict[str, Any]) -> str:
    lines = [
        "# Integrated Gameday Results",
        "",
        f"- scenario count: `{report['summary']['scenario_count']}`",
        f"- passed: `{report['summary']['passed_count']}`",
        f"- failed: `{report['summary']['failed_count']}`",
        f"- completed areas: `{', '.join(report['summary']['completed_areas'])}`",
        "",
        "## Scenario Outcomes",
    ]
    for scenario in report["scenarios"]:
        lines.append(
            f"- `{scenario['scenario_id']}` [{scenario['area']}] `{scenario['status']}` "
            f"checks `{scenario['observed_checks']}/{scenario['expected_checks']}`"
        )
    lines.append("")
    return "\n".join(lines)


def generate_report(
    out_dir: pathlib.Path,
    scratchbird_tests: pathlib.Path,
    bundle_manifest: pathlib.Path,
    signing_manifest: pathlib.Path,
    triage_dir: pathlib.Path,
    repro_report: pathlib.Path,
    compliance_bundle_dir: pathlib.Path,
) -> pathlib.Path:
    repo_root = repo_root_from_script()
    scenarios = release_validation_scenarios(
        repo_root,
        bundle_manifest,
        signing_manifest,
        triage_dir,
        repro_report,
        compliance_bundle_dir,
    ) + gtest_scenarios(scratchbird_tests)

    out_dir.mkdir(parents=True, exist_ok=True)
    scenario_results: List[Dict[str, Any]] = []
    for scenario in scenarios:
        proc = run_command(scenario["command"], repo_root)
        scenario_results.append(scenario_result(scenario, proc))

    passed_count = sum(1 for item in scenario_results if item["status"] == "pass")
    failed_count = sum(1 for item in scenario_results if item["status"] != "pass")
    completed_areas = sorted({item["area"] for item in scenario_results if item["status"] == "pass"})
    report = {
        "schema": "scratchbird.release.integrated_gameday_report.v1",
        "repo_root": str(repo_root),
        "inputs": {
            "scratchbird_tests": str(scratchbird_tests),
            "bundle_manifest": str(bundle_manifest),
            "signing_manifest": str(signing_manifest),
            "triage_dir": str(triage_dir),
            "repro_report": str(repro_report),
            "compliance_bundle_dir": str(compliance_bundle_dir),
        },
        "summary": {
            "scenario_count": len(scenario_results),
            "passed_count": passed_count,
            "failed_count": failed_count,
            "required_areas": REQUIRED_AREAS,
            "completed_areas": completed_areas,
        },
        "scenarios": scenario_results,
    }
    report_path = out_dir / "integrated-gameday-report.json"
    report_path.write_text(stable_json(report), encoding="utf-8")
    write_scenario_csv(scenario_results, out_dir / "gameday-scenario-status.csv")
    (out_dir / "gameday-results.md").write_text(render_markdown(report), encoding="utf-8")
    return report_path


def validate_report(report_path: pathlib.Path) -> int:
    report = json.loads(report_path.read_text(encoding="utf-8"))
    summary = report.get("summary", {})
    if int(summary.get("failed_count", 1)) != 0:
        print("validation failed: one or more gameday scenarios failed", file=sys.stderr)
        return 1
    completed_areas = set(summary.get("completed_areas", []))
    missing_areas = [area for area in REQUIRED_AREAS if area not in completed_areas]
    if missing_areas:
        print(
            "validation failed: missing required gameday areas: "
            + ", ".join(missing_areas),
            file=sys.stderr,
        )
        return 1
    if int(summary.get("scenario_count", 0)) == 0:
        print("validation failed: gameday report contains no scenarios", file=sys.stderr)
        return 1
    print(f"validated integrated gameday report: {report_path}")
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    generate = subparsers.add_parser("generate", help="run the integrated gameday and write a report")
    generate.add_argument("--out-dir", type=pathlib.Path, required=True)
    generate.add_argument(
        "--scratchbird-tests",
        type=pathlib.Path,
        default=repo_root_from_script() / "build" / "tests" / "scratchbird_tests",
    )
    generate.add_argument("--bundle-manifest", type=pathlib.Path, required=True)
    generate.add_argument("--signing-manifest", type=pathlib.Path, required=True)
    generate.add_argument("--triage-dir", type=pathlib.Path, required=True)
    generate.add_argument("--repro-report", type=pathlib.Path, required=True)
    generate.add_argument("--compliance-bundle-dir", type=pathlib.Path, required=True)

    validate = subparsers.add_parser("validate", help="validate a generated integrated gameday report")
    validate.add_argument("--report-path", type=pathlib.Path, required=True)
    return parser


def main(argv: Optional[List[str]] = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    if args.command == "generate":
        report_path = generate_report(
            args.out_dir.resolve(),
            args.scratchbird_tests.resolve(),
            args.bundle_manifest.resolve(),
            args.signing_manifest.resolve(),
            args.triage_dir.resolve(),
            args.repro_report.resolve(),
            args.compliance_bundle_dir.resolve(),
        )
        print(f"wrote integrated gameday report: {report_path}")
        return 0
    if args.command == "validate":
        return validate_report(args.report_path.resolve())
    parser.error("unsupported command")
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
