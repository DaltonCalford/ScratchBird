#!/usr/bin/env python3
"""Re-run manifest generators and verify deterministic output."""

from __future__ import annotations

import argparse
import csv
import json
import pathlib
import subprocess
import sys
import tempfile
from typing import Any, Dict, List, Optional


def stable_json(data: Any) -> str:
    return json.dumps(data, indent=2, sort_keys=True) + "\n"


def sha256_file(path: pathlib.Path) -> str:
    import hashlib

    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def repo_root_from_script() -> pathlib.Path:
    return pathlib.Path(__file__).resolve().parents[2]


def run_command(command: List[str], cwd: pathlib.Path) -> None:
    subprocess.run(command, cwd=cwd, check=True)


def collect_relative_files(root: pathlib.Path) -> List[str]:
    return sorted(
        path.relative_to(root).as_posix()
        for path in root.rglob("*")
        if path.is_file()
    )


def compare_trees(pass_one: pathlib.Path, pass_two: pathlib.Path) -> List[Dict[str, str]]:
    files = sorted(set(collect_relative_files(pass_one)) | set(collect_relative_files(pass_two)))
    results: List[Dict[str, str]] = []
    for rel in files:
        first = pass_one / rel
        second = pass_two / rel
        first_hash = sha256_file(first) if first.exists() else "missing"
        second_hash = sha256_file(second) if second.exists() else "missing"
        results.append(
            {
                "path": rel,
                "pass_one_sha256": first_hash,
                "pass_two_sha256": second_hash,
                "status": "match" if first_hash == second_hash else "drift",
            }
        )
    return results


def write_drift_csv(rows: List[Dict[str, str]], path: pathlib.Path) -> None:
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=["path", "pass_one_sha256", "pass_two_sha256", "status"],
        )
        writer.writeheader()
        writer.writerows(rows)


def verify_signed_bundle(
    repo_root: pathlib.Path,
    bundle_manifest: Optional[pathlib.Path],
    signing_manifest: Optional[pathlib.Path],
) -> Dict[str, Any]:
    if bundle_manifest is None or signing_manifest is None:
        return {"status": "skipped"}
    command = [
        "python3",
        str(repo_root / "scripts/release/sign_release_bundle.py"),
        "verify",
        "--bundle-manifest",
        str(bundle_manifest),
        "--signing-manifest",
        str(signing_manifest),
    ]
    proc = subprocess.run(command, cwd=repo_root, capture_output=True, text=True, check=False)
    return {
        "status": "pass" if proc.returncode == 0 else "fail",
        "stdout": proc.stdout.strip(),
        "stderr": proc.stderr.strip(),
    }


def generate_report(
    repo_root: pathlib.Path,
    out_dir: pathlib.Path,
    linux_build_dir: pathlib.Path,
    windows_build_dir: pathlib.Path,
    bundle_manifest: Optional[pathlib.Path],
    signing_manifest: Optional[pathlib.Path],
) -> pathlib.Path:
    out_dir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as temp_dir:
        temp_root = pathlib.Path(temp_dir)
        pass_one = temp_root / "pass_one"
        pass_two = temp_root / "pass_two"
        pass_one.mkdir()
        pass_two.mkdir()

        for target_dir in (pass_one, pass_two):
            run_command(
                [
                    "python3",
                    str(repo_root / "scripts/cross_os/generate_build_manifest.py"),
                    str(target_dir / "build-manifest.json"),
                ],
                repo_root,
            )
            run_command(
                [
                    "bash",
                    str(repo_root / "scripts/cross_os/generate_build_cache_keys.sh"),
                    str(target_dir / "cache-keys.md"),
                ],
                repo_root,
            )
            run_command(
                [
                    "bash",
                    str(repo_root / "scripts/cross_os/generate_package_manifests.sh"),
                    "--linux-build-dir",
                    str(linux_build_dir),
                    "--windows-build-dir",
                    str(windows_build_dir),
                    "--out-dir",
                    str(target_dir / "package-manifests"),
                ],
                repo_root,
            )
            run_command(
                [
                    "python3",
                    str(repo_root / "scripts/release/generate_sbom_inventory.py"),
                    "generate",
                    "--repo",
                    f"ScratchBird={repo_root}",
                    "--repo",
                    f"ScratchBird-driver={repo_root.parent / 'ScratchBird-driver'}",
                    "--out-dir",
                    str(target_dir / "sbom"),
                ],
                repo_root,
            )

        drift_rows = compare_trees(pass_one, pass_two)
        signed_bundle_check = verify_signed_bundle(repo_root, bundle_manifest, signing_manifest)
        report = {
            "schema": "scratchbird.release.repro_build_report.v1",
            "repo_root": str(repo_root),
            "inputs": {
                "linux_build_dir": str(linux_build_dir),
                "windows_build_dir": str(windows_build_dir),
                "bundle_manifest": str(bundle_manifest) if bundle_manifest else "",
                "signing_manifest": str(signing_manifest) if signing_manifest else "",
            },
            "summary": {
                "file_count": len(drift_rows),
                "drift_count": sum(1 for row in drift_rows if row["status"] != "match"),
                "signed_bundle_verification": signed_bundle_check["status"],
            },
            "drift_rows": drift_rows,
            "signed_bundle_verification": signed_bundle_check,
        }
        report_path = out_dir / "repro-build-report.json"
        report_path.write_text(stable_json(report), encoding="utf-8")
        write_drift_csv(drift_rows, out_dir / "build-drift-audit.csv")
        verification_results = out_dir / "manifest-verification-results.md"
        verification_results.write_text(
            "\n".join(
                [
                    "# Manifest Verification Results",
                    "",
                    f"- compared files: `{report['summary']['file_count']}`",
                    f"- drift count: `{report['summary']['drift_count']}`",
                    f"- signed bundle verification: `{signed_bundle_check['status']}`",
                    "",
                    "## Drift Summary",
                    *[
                        f"- `{row['path']}`: `{row['status']}`"
                        for row in drift_rows
                    ],
                ]
            )
            + "\n",
            encoding="utf-8",
        )
        return report_path


def validate_report(report_path: pathlib.Path) -> int:
    report = json.loads(report_path.read_text(encoding="utf-8"))
    drift_count = int(report["summary"]["drift_count"])
    signed_bundle_status = report["summary"]["signed_bundle_verification"]
    if drift_count != 0:
        print(f"validation failed: detected {drift_count} manifest drifts", file=sys.stderr)
        return 1
    if signed_bundle_status == "fail":
        print("validation failed: signed bundle verification failed", file=sys.stderr)
        return 1
    print(f"validated reproducibility report: {report_path}")
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    generate = subparsers.add_parser("generate", help="re-run generators and compare outputs")
    generate.add_argument("--out-dir", type=pathlib.Path, required=True)
    generate.add_argument("--linux-build-dir", type=pathlib.Path, default=repo_root_from_script() / "build")
    generate.add_argument("--windows-build-dir", type=pathlib.Path, default=repo_root_from_script() / "build/windows")
    generate.add_argument("--bundle-manifest", type=pathlib.Path)
    generate.add_argument("--signing-manifest", type=pathlib.Path)

    validate = subparsers.add_parser("validate", help="validate a reproducibility report")
    validate.add_argument("--report-path", type=pathlib.Path, required=True)
    return parser


def main(argv: Optional[List[str]] = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    repo_root = repo_root_from_script()
    if args.command == "generate":
        report_path = generate_report(
            repo_root,
            args.out_dir.resolve(),
            args.linux_build_dir.resolve(),
            args.windows_build_dir.resolve(),
            args.bundle_manifest.resolve() if args.bundle_manifest else None,
            args.signing_manifest.resolve() if args.signing_manifest else None,
        )
        print(f"wrote reproducibility report: {report_path}")
        return 0
    if args.command == "validate":
        return validate_report(args.report_path.resolve())
    parser.error("unsupported command")
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
