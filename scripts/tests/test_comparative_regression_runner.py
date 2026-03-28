#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
import pathlib
import sys
import tempfile
import unittest


SCRIPT_PATH = (
    pathlib.Path(__file__).resolve().parents[1]
    / "verification_bundle"
    / "suite"
    / "scripts"
    / "comparative_regression_runner.py"
)
SCRIPT_DIR = SCRIPT_PATH.parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))
SPEC = importlib.util.spec_from_file_location("comparative_regression_runner", SCRIPT_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class ComparativeRegressionRunnerContractTest(unittest.TestCase):
    def test_validate_corpus_contract_rejects_runtime_translation(self) -> None:
        with self.assertRaisesRegex(RuntimeError, "must prohibit runtime translation"):
            MODULE.validate_corpus_contract(
                {
                    "translation_contract": {
                        "id": "vncr-frozen-static-corpus-v1",
                        "required_translation_mode": "static_native_v3",
                        "runtime_translation_allowed": True,
                        "runtime_substitution_only": ["__VNCR_NS__"],
                    }
                }
            )

    def _write(self, root: pathlib.Path, relative: str, text: str) -> None:
        path = root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")

    def _base_case(self) -> dict[str, object]:
        return {
            "id": "firebird_delete_all_rows",
            "translation_mode": "static_native_v3",
            "provenance": {
                "donor_source_path": "tests/compatibility/firebird/converted/functional/dml/delete_01.sql",
                "donor_converted_path": "tests/compatibility/firebird/converted/functional/dml/delete_01.sql",
                "donor_curated_list": "tests/compatibility/firebird/config/ctest_list.txt",
                "donor_statement_scope": "DELETE FROM tb; SELECT * FROM tb;",
            },
            "surfaces": {
                "native": {
                    "exec": "cases/comparative/sql/native/firebird_delete_all_rows.exec.sql",
                },
                "firebird": {
                    "exec": "cases/comparative/sql/firebird/firebird_delete_all_rows.exec.sql",
                },
            },
        }

    def test_validate_case_contract_accepts_static_sql_artifacts(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            self._write(
                root,
                "cases/comparative/sql/native/firebird_delete_all_rows.exec.sql",
                "select '__VNCR_NS__';\n",
            )
            self._write(
                root,
                "cases/comparative/sql/firebird/firebird_delete_all_rows.exec.sql",
                "select '__VNCR_NS__';\n",
            )
            rows = MODULE.validate_case_contract(self._base_case(), root, ["firebird"])
            self.assertEqual(2, len(rows))
            self.assertEqual("native", rows[0]["role"])
            self.assertEqual("donor", rows[1]["role"])

    def test_validate_case_contract_rejects_non_static_translation_mode(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            self._write(
                root,
                "cases/comparative/sql/native/firebird_delete_all_rows.exec.sql",
                "select 1;\n",
            )
            self._write(
                root,
                "cases/comparative/sql/firebird/firebird_delete_all_rows.exec.sql",
                "select 1;\n",
            )
            case = self._base_case()
            case["translation_mode"] = "runtime_rewrite"
            with self.assertRaisesRegex(RuntimeError, "translation_mode must be static_native_v3"):
                MODULE.validate_case_contract(case, root, ["firebird"])

    def test_validate_case_contract_rejects_unapproved_runtime_tokens(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            self._write(
                root,
                "cases/comparative/sql/native/firebird_delete_all_rows.exec.sql",
                "select '__VNCR_OTHER__';\n",
            )
            self._write(
                root,
                "cases/comparative/sql/firebird/firebird_delete_all_rows.exec.sql",
                "select 1;\n",
            )
            with self.assertRaisesRegex(RuntimeError, "unsupported runtime template tokens"):
                MODULE.validate_case_contract(self._base_case(), root, ["firebird"])


if __name__ == "__main__":
    unittest.main()
