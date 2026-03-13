#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
import json
import pathlib
import sys
import tempfile
import unittest


SCRIPT_PATH = pathlib.Path(__file__).resolve().parents[1] / "release" / "verify_repro_build.py"
SPEC = importlib.util.spec_from_file_location("verify_repro_build", SCRIPT_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class VerifyReproBuildTest(unittest.TestCase):
    def test_compare_trees_detects_match_and_drift(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            one = root / "one"
            two = root / "two"
            one.mkdir()
            two.mkdir()
            (one / "a.txt").write_text("same\n", encoding="utf-8")
            (two / "a.txt").write_text("same\n", encoding="utf-8")
            (one / "b.txt").write_text("left\n", encoding="utf-8")
            (two / "b.txt").write_text("right\n", encoding="utf-8")

            rows = MODULE.compare_trees(one, two)
            by_path = {row["path"]: row for row in rows}
            self.assertEqual(by_path["a.txt"]["status"], "match")
            self.assertEqual(by_path["b.txt"]["status"], "drift")

    def test_validate_report_requires_zero_drift(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            report_path = pathlib.Path(tmp) / "report.json"
            report_path.write_text(
                json.dumps(
                    {
                        "summary": {
                            "drift_count": 0,
                            "signed_bundle_verification": "pass",
                        }
                    }
                )
                + "\n",
                encoding="utf-8",
            )
            self.assertEqual(MODULE.validate_report(report_path), 0)

            report_path.write_text(
                json.dumps(
                    {
                        "summary": {
                            "drift_count": 1,
                            "signed_bundle_verification": "pass",
                        }
                    }
                )
                + "\n",
                encoding="utf-8",
            )
            self.assertEqual(MODULE.validate_report(report_path), 1)


if __name__ == "__main__":
    unittest.main()
