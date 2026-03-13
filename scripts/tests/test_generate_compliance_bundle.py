#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
import json
import pathlib
import sys
import tempfile
import unittest


SCRIPT_PATH = pathlib.Path(__file__).resolve().parents[1] / "release" / "generate_compliance_bundle.py"
SPEC = importlib.util.spec_from_file_location("generate_compliance_bundle", SCRIPT_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class GenerateComplianceBundleTest(unittest.TestCase):
    def test_component_notice_rows_use_root_fallback(self) -> None:
        inventories = [
            {
                "repo_id": "ScratchBird",
                "components": [
                    {
                        "component_name": "node",
                        "manifest_path": "tracks/node/package.json",
                        "manifest_type": "npm",
                        "dependencies": [],
                    }
                ],
            }
        ]
        legal_files_by_repo = {
            "ScratchBird": [
                {
                    "repo_name": "ScratchBird",
                    "repo_slug": "scratchbird",
                    "source_path": "LICENSE",
                    "sha256": "abc",
                    "scope": "repo-root",
                }
            ]
        }
        rows = MODULE.component_notice_rows(inventories, legal_files_by_repo)
        self.assertEqual(rows[0]["coverage_mode"], "repo-root-fallback")
        self.assertEqual(rows[0]["legal_files"], "LICENSE")

    def test_generate_and_validate_bundle(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            sb = root / "ScratchBird"
            sbd = root / "ScratchBird-driver"
            sb.mkdir()
            sbd.mkdir()
            (sb / "LICENSE").write_text("sb\n", encoding="utf-8")
            (sb / "resources").mkdir()
            (sb / "resources" / "timezones").mkdir()
            (sb / "resources" / "timezones" / "LICENSE").write_text("tz\n", encoding="utf-8")
            (sbd / "LICENSE").write_text("driver\n", encoding="utf-8")
            (sbd / "tracks").mkdir()
            (sbd / "tracks" / "alpha").mkdir(parents=True, exist_ok=True)
            (sbd / "tracks" / "alpha" / "drivers").mkdir(parents=True, exist_ok=True)
            (sbd / "tracks" / "alpha" / "drivers" / "odbc").mkdir(parents=True)
            (sbd / "tracks" / "alpha" / "drivers" / "odbc" / "LICENSE").write_text("odbc\n", encoding="utf-8")

            bundle_dir = root / "release"
            bundle_dir.mkdir()
            inventory = {
                "repo_id": "ScratchBird-driver",
                "components": [
                    {
                        "component_name": "odbc",
                        "manifest_path": "tracks/alpha/drivers/odbc/Cargo.toml",
                        "manifest_type": "cargo",
                        "dependencies": [{"name": "bytes", "version": "1"}],
                    }
                ],
            }
            inv_path = bundle_dir / "scratchbird-driver-dependency-inventory.json"
            inv_path.write_text(json.dumps(inventory, indent=2, sort_keys=True) + "\n", encoding="utf-8")
            bundle_manifest = {
                "bundle_outputs": [
                    {"path": inv_path.name, "sha256": "ignored"}
                ]
            }
            bundle_manifest_path = bundle_dir / "release-sbom-manifest.json"
            bundle_manifest_path.write_text(json.dumps(bundle_manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
            signing_path = bundle_dir / "release-bundle-signing-manifest.json"
            signing_path.write_text('{"ok":true}\n', encoding="utf-8")
            triage_path = bundle_dir / "cve-triage-report.json"
            triage_path.write_text('{"ok":true}\n', encoding="utf-8")

            original = MODULE.default_repo_roots
            MODULE.default_repo_roots = lambda _: {"ScratchBird": sb, "ScratchBird-driver": sbd}
            try:
                out_dir = root / "compliance"
                MODULE.generate_bundle(out_dir, bundle_manifest_path, signing_path, triage_path, MODULE.default_repo_roots(SCRIPT_PATH))
                self.assertEqual(MODULE.validate_bundle(out_dir), 0)
                matrix = (out_dir / "dependency-notice-matrix.csv").read_text(encoding="utf-8")
                self.assertIn("tracks/alpha/drivers/odbc/LICENSE", matrix)
            finally:
                MODULE.default_repo_roots = original


if __name__ == "__main__":
    unittest.main()
