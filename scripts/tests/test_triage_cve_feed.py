#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
import json
import pathlib
import sys
import tempfile
import unittest


SCRIPT_PATH = pathlib.Path(__file__).resolve().parents[1] / "release" / "triage_cve_feed.py"
SPEC = importlib.util.spec_from_file_location("triage_cve_feed", SCRIPT_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class TriageCveFeedTest(unittest.TestCase):
    def make_bundle(self, root: pathlib.Path) -> pathlib.Path:
        bundle_dir = root / "bundle"
        bundle_dir.mkdir()
        inventory = {
            "repo_id": "ScratchBird",
            "components": [
                {
                    "component_name": "scratchbird",
                    "dependencies": [
                        {"name": "openssl", "version": "3.0.13", "scope": "runtime"},
                        {"name": "org.junit.jupiter:junit-jupiter", "version": "5.10.2", "scope": "dev"},
                    ],
                    "manifest_path": "vcpkg.json",
                    "manifest_type": "vcpkg",
                }
            ],
        }
        inventory_path = bundle_dir / "scratchbird-dependency-inventory.json"
        inventory_path.write_text(json.dumps(inventory, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        manifest = {
            "bundle_outputs": [
                {"path": inventory_path.name, "sha256": "ignored"}
            ]
        }
        manifest_path = bundle_dir / "release-sbom-manifest.json"
        manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        return manifest_path

    def test_triage_marks_overdue_and_open(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            manifest_path = self.make_bundle(root)
            advisories = {
                "advisories": [
                    {
                        "advisory_id": "CVE-2026-0001",
                        "package_name": "openssl",
                        "package_manager": "vcpkg",
                        "severity": "critical",
                        "published_at": "2026-03-01T00:00:00Z",
                        "fixed_version": "3.0.14",
                    },
                    {
                        "advisory_id": "CVE-2026-0002",
                        "package_name": "org.junit.jupiter:junit-jupiter",
                        "package_manager": "vcpkg",
                        "severity": "medium",
                        "published_at": "2026-03-25T00:00:00Z",
                        "fixed_version": "5.10.3",
                    },
                ]
            }
            advisories_path = root / "advisories.json"
            advisories_path.write_text(json.dumps(advisories, indent=2, sort_keys=True) + "\n", encoding="utf-8")

            report = MODULE.evaluate_advisories(
                advisories_path,
                manifest_path,
                None,
                MODULE.parse_timestamp("2026-03-28T00:00:00Z"),
            )

            self.assertEqual(report["summary"]["match_count"], 2)
            by_id = {record["advisory_id"]: record for record in report["triage_records"]}
            self.assertEqual(by_id["CVE-2026-0001"]["patch_state"], "overdue")
            self.assertEqual(by_id["CVE-2026-0001"]["action"], "SECOPS_REMEDIATE")
            self.assertEqual(by_id["CVE-2026-0002"]["patch_state"], "open")
            self.assertEqual(by_id["CVE-2026-0002"]["action"], "SECOPS_TRACK")

    def test_known_exploited_forces_remediation(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            manifest_path = self.make_bundle(root)
            advisories_path = root / "advisories.json"
            advisories_path.write_text(
                json.dumps(
                    {
                        "advisories": [
                            {
                                "advisory_id": "CVE-2026-1000",
                                "package_name": "openssl",
                                "package_manager": "vcpkg",
                                "severity": "high",
                                "published_at": "2026-03-20T00:00:00Z",
                                "known_exploited": True,
                            }
                        ]
                    },
                    indent=2,
                    sort_keys=True,
                )
                + "\n",
                encoding="utf-8",
            )

            report = MODULE.evaluate_advisories(
                advisories_path,
                manifest_path,
                None,
                MODULE.parse_timestamp("2026-03-21T00:00:00Z"),
            )

            record = report["triage_records"][0]
            self.assertEqual(record["patch_state"], "known_exploited")
            self.assertEqual(record["action"], "SECOPS_REMEDIATE")
            self.assertEqual(report["summary"]["remediation_count"], 1)

    def test_write_and_validate_outputs(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            manifest_path = self.make_bundle(root)
            advisories_path = root / "advisories.json"
            advisories_path.write_text(
                json.dumps(
                    {
                        "advisories": [
                            {
                                "advisory_id": "CVE-2026-0003",
                                "package_name": "openssl",
                                "package_manager": "vcpkg",
                                "severity": "low",
                                "published_at": "2026-03-20T00:00:00Z",
                            }
                        ]
                    },
                    indent=2,
                    sort_keys=True,
                )
                + "\n",
                encoding="utf-8",
            )
            report = MODULE.evaluate_advisories(
                advisories_path,
                manifest_path,
                None,
                MODULE.parse_timestamp("2026-03-22T00:00:00Z"),
            )
            out_dir = root / "out"
            MODULE.write_triage_outputs(report, out_dir)
            self.assertEqual(MODULE.validate_outputs(out_dir), 0)


if __name__ == "__main__":
    unittest.main()
