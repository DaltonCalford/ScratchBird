#!/usr/bin/env python3

from __future__ import annotations

import contextlib
import importlib.util
import json
import io
import pathlib
import sys
import tempfile
import unittest


SCRIPT_PATH = pathlib.Path(__file__).resolve().parents[1] / "release" / "run_integrated_gameday.py"
SPEC = importlib.util.spec_from_file_location("run_integrated_gameday", SCRIPT_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class RunIntegratedGamedayTest(unittest.TestCase):
    def test_build_gtest_filter_preserves_order(self) -> None:
        tests = ["Suite.One", "Suite.Two", "Suite.Three"]
        self.assertEqual(MODULE.build_gtest_filter(tests), "Suite.One:Suite.Two:Suite.Three")

    def test_validate_report_requires_required_area_coverage(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            report_path = pathlib.Path(tmp) / "report.json"
            report_path.write_text(
                json.dumps(
                    {
                        "summary": {
                            "scenario_count": 1,
                            "failed_count": 0,
                            "completed_areas": MODULE.REQUIRED_AREAS[:-1],
                        }
                    }
                )
                + "\n",
                encoding="utf-8",
            )
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(MODULE.validate_report(report_path), 1)

            report_path.write_text(
                json.dumps(
                    {
                        "summary": {
                            "scenario_count": 2,
                            "failed_count": 0,
                            "completed_areas": MODULE.REQUIRED_AREAS,
                        }
                    }
                )
                + "\n",
                encoding="utf-8",
            )
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(MODULE.validate_report(report_path), 0)

    def test_gtest_scenarios_expose_expected_check_counts(self) -> None:
        scenarios = MODULE.gtest_scenarios(pathlib.Path("/tmp/fake_scratchbird_tests"))
        by_id = {scenario["scenario_id"]: scenario for scenario in scenarios}
        self.assertEqual(by_id["security_audit_integrity"]["expected_checks"], 5)
        self.assertEqual(by_id["operational_fault_response"]["expected_checks"], 9)


if __name__ == "__main__":
    unittest.main()
