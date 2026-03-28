#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
import pathlib
import sys
import unittest


SCRIPT_PATH = (
    pathlib.Path(__file__).resolve().parents[1]
    / "verification_bundle"
    / "suite"
    / "scripts"
    / "db_adapters.py"
)
SCRIPT_DIR = SCRIPT_PATH.parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))
SPEC = importlib.util.spec_from_file_location("db_adapters", SCRIPT_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class ExtractSqlstateTest(unittest.TestCase):
    def test_extracts_explicit_sqlstate(self) -> None:
        self.assertEqual("42000", MODULE._extract_sqlstate("Statement failed, SQLSTATE = 42000"))

    def test_infers_syntax_class_from_native_message(self) -> None:
        self.assertEqual(
            "42000",
            MODULE._extract_sqlstate("Error: V3 SELECT GROUP BY projection not in grouping set: ID"),
        )

    def test_infers_connection_class_from_connectivity_message(self) -> None:
        self.assertEqual(
            "08006",
            MODULE._extract_sqlstate("Unable to complete network request to host \"127.0.0.1\"."),
        )


if __name__ == "__main__":
    unittest.main()
