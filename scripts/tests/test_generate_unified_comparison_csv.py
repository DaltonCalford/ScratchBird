#!/usr/bin/env python3

from __future__ import annotations

import csv
import importlib.util
import json
import pathlib
import sys
import tempfile
import unittest


SCRIPT_PATH = pathlib.Path(__file__).resolve().parents[1] / "full_run_metrics" / "generate_unified_comparison_csv.py"
SPEC = importlib.util.spec_from_file_location("generate_unified_comparison_csv", SCRIPT_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class GenerateUnifiedComparisonCsvTest(unittest.TestCase):
    def test_write_unified_csv_flattens_summary_and_totals(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            (root / "scratchbird" / "full-gate").mkdir(parents=True)
            (root / "firebird" / "regression").mkdir(parents=True)

            (root / "scratchbird" / "full-gate" / "full-gate-scratchbird-summary.json").write_text(
                json.dumps(
                    {
                        "summary": {"failed": 0, "passed": 4000, "total_time_sec": 321.5},
                        "results": {"labels": [{"label": "unit", "tests": 10}]},
                    }
                )
                + "\n",
                encoding="utf-8",
            )
            (root / "firebird" / "regression" / "regression-firebird-summary.json").write_text(
                json.dumps({"totals": {"total": 120, "passed": 118, "failed": 2, "errors": 0, "skipped": 0}})
                + "\n",
                encoding="utf-8",
            )
            summary_path = root / "matrix-summary.json"
            summary_path.write_text(
                json.dumps(
                    {
                        "run_id": "run-1",
                        "output_root": str(root),
                        "engines_requested": ["scratchbird", "firebird"],
                        "suites_requested": ["full-gate", "regression"],
                        "suite_runs": [
                            {
                                "engine": "scratchbird",
                                "suite": "full-gate",
                                "started_at": "2026-03-28T00:00:00Z",
                                "duration_seconds": 321.5,
                                "status": "passed",
                                "exit_code": 0,
                                "output_dir": "scratchbird/full-gate",
                            },
                            {
                                "engine": "firebird",
                                "suite": "regression",
                                "started_at": "2026-03-28T00:01:00Z",
                                "duration_seconds": 10,
                                "status": "failed",
                                "exit_code": 1,
                                "output_dir": "firebird/regression",
                            },
                        ],
                    }
                )
                + "\n",
                encoding="utf-8",
            )

            output_csv = MODULE.write_unified_csv(summary_path)
            with output_csv.open(newline="", encoding="utf-8") as handle:
                rows = list(csv.DictReader(handle))

            by_key = {(row["suite"], row["metric"]): row for row in rows}
            self.assertEqual(by_key[("full-gate", "summary.total_time_sec")]["scratchbird"], "321.5")
            self.assertEqual(by_key[("regression", "totals.failed")]["firebird"], "2")
            self.assertEqual(by_key[("full-gate", "matrix.status")]["scratchbird"], "passed")


if __name__ == "__main__":
    unittest.main()
