#!/usr/bin/env python3

from __future__ import annotations

import csv
import importlib.util
import json
import pathlib
import sys
import tempfile
import unittest


SCRIPT_PATH = pathlib.Path(__file__).resolve().parents[1] / "full_run_metrics" / "normalize_full_run.py"
SPEC = importlib.util.spec_from_file_location("normalize_full_run", SCRIPT_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class NormalizeFullRunTest(unittest.TestCase):
    def test_normalize_writes_suite_summaries_and_comparisons(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            repo = root / "ScratchBird"
            repo.mkdir()
            (repo / "tests" / "results" / "full_gate" / "run-a").mkdir(parents=True)
            (repo / "tests" / "conformance" / "public_beta" / "results" / "gate-a").mkdir(parents=True)
            (repo / "tests" / "conformance" / "v3_native_inet" / "results" / "ctest" / "v3-a").mkdir(parents=True)
            (repo / "tests" / "conformance" / "v3_native_comparative_regression" / "results" / "ctest" / "vncr-a").mkdir(parents=True)
            (repo / "tests" / "compatibility" / "firebird" / "results" / "ctest" / "firebird-a").mkdir(parents=True)
            (repo / "tests" / "compatibility" / "mysql" / "results" / "ctest" / "mysql-a").mkdir(parents=True)
            (repo / "tests" / "compatibility" / "postgresql" / "results" / "ctest" / "postgresql-a").mkdir(parents=True)
            (repo / "tests" / "compatibility" / "scratchbird" / "results" / "ctest" / "scratchbird-a").mkdir(parents=True)
            (repo / "scripts" / "verification_bundle" / "suite" / "results" / "perf" / "perf-a").mkdir(parents=True)
            (repo / "scripts" / "verification_bundle" / "suite" / "results" / "optimizer_donor_compare" / "opt-a").mkdir(parents=True)
            history_root = repo / "tests" / "results" / "full_run_metrics"
            history_prior = history_root / "prior-run"
            history_prior.mkdir(parents=True)

            full_gate = repo / "tests" / "results" / "full_gate" / "run-a"
            (full_gate / "RUN_STATUS.txt").write_text(
                "run_dir=tests/results/full_gate/run-a\n"
                "configure_log=tests/results/full_gate/run-a/configure.log\n"
                "build_log=tests/results/full_gate/run-a/build.log\n"
                "ctest_log=tests/results/full_gate/run-a/ctest.log\n"
                "configure_exit=0\n"
                "build_exit=0\n"
                "ctest_exit=0\n",
                encoding="utf-8",
            )
            (full_gate / "configure.log").write_text("configured\n", encoding="utf-8")
            (full_gate / "build.log").write_text("built\n", encoding="utf-8")
            (full_gate / "ctest.log").write_text(
                "100% tests passed, 0 tests failed out of 10\n"
                "unit                     =   1.00 sec*proc (10 tests)\n"
                "\nTotal Test time (real) = 15.50 sec\n"
                "\nThe following tests did not run:\n\t11 - Skipped.Test\n",
                encoding="utf-8",
            )

            public_beta = repo / "tests" / "conformance" / "public_beta" / "results" / "gate-a"
            (public_beta / "category_summary.txt").write_text(
                "CATEGORY_SUMMARY|wire_protocol|2|0\nCATEGORY_SUMMARY|transaction_semantics|3|1\n",
                encoding="utf-8",
            )
            (public_beta / "step_results.txt").write_text(
                "STEP_RESULT|wire_protocol|pg|PASS|logs/pg.log\n"
                "STEP_RESULT|wire_protocol|mysql|PASS|logs/mysql.log\n"
                "STEP_RESULT|transaction_semantics|txn1|PASS|logs/txn1.log\n"
                "STEP_RESULT|transaction_semantics|txn2|PASS|logs/txn2.log\n"
                "STEP_RESULT|transaction_semantics|txn3|FAIL|logs/txn3.log\n",
                encoding="utf-8",
            )
            (public_beta / "SUMMARY.md").write_text("# summary\n", encoding="utf-8")

            v3_dir = repo / "tests" / "conformance" / "v3_native_inet" / "results" / "ctest" / "v3-a"
            (v3_dir / "RUN_MANIFEST.json").write_text(
                json.dumps({"status": "passed", "listed_tests": 2, "failure_count": 0, "timestamp_utc": "2026-03-28T00:00:00Z"})
                + "\n",
                encoding="utf-8",
            )
            (v3_dir / "case_status.txt").write_text("CASE|one|PASS\nCASE|two|PASS\n", encoding="utf-8")

            v3_comparative_dir = repo / "tests" / "conformance" / "v3_native_comparative_regression" / "results" / "ctest" / "vncr-a"
            v3_comparative_artifact_dir = v3_comparative_dir / "vncr-a-inner"
            (v3_comparative_dir / "RUN_MANIFEST.json").write_text(
                json.dumps(
                    {
                        "status": "passed",
                        "listed_tests": 3,
                        "failure_count": 0,
                        "comparison_suite_family": "native-comparative-regression",
                        "comparison_contract_id": "native-v3-comparative-regression-v1",
                        "timestamp_utc": "2026-03-28T00:00:00Z",
                    }
                )
                + "\n",
                encoding="utf-8",
            )
            v3_comparative_artifact_dir.mkdir(parents=True)
            (v3_comparative_artifact_dir / "comparative_summary.json").write_text(
                json.dumps(
                    {
                        "suite_family": "native-comparative-regression",
                        "contract_id": "native-v3-comparative-regression-v1",
                        "translation_contract": {
                            "translation_contract_id": "vncr-frozen-static-corpus-v1",
                            "required_translation_mode": "static_native_v3",
                            "runtime_translation_allowed": False,
                            "runtime_substitution_only": ["__VNCR_NS__"],
                            "corpus_fingerprint_sha256": "abc123",
                        },
                        "total_cases": 2,
                        "total_pairs": 3,
                        "passed_pairs": 3,
                        "failed_pairs": 0,
                    }
                )
                + "\n",
                encoding="utf-8",
            )
            with (v3_comparative_artifact_dir / "comparative_engine_runs.csv").open("w", newline="", encoding="utf-8") as handle:
                writer = csv.DictWriter(
                    handle,
                    fieldnames=[
                        "case_id",
                        "engine_id",
                        "dialect_family",
                        "role",
                        "expectation",
                        "behavior_class",
                        "translation_mode",
                        "exec_status",
                        "sqlstate",
                        "assert_count",
                        "exec_elapsed_ms",
                        "donor_source_path",
                        "donor_converted_path",
                    ],
                )
                writer.writeheader()
                writer.writerow(
                    {
                        "case_id": "firebird_delete_all_rows",
                        "engine_id": "scratchbird_native",
                        "dialect_family": "firebird",
                        "role": "native",
                        "expectation": "must_match",
                        "behavior_class": "dml_delete_all_rows",
                        "translation_mode": "static_native_v3",
                        "exec_status": "ok",
                        "sqlstate": "00000",
                        "assert_count": 1,
                        "exec_elapsed_ms": 5,
                        "donor_source_path": "fb.sql",
                        "donor_converted_path": "fb.sql",
                    }
                )
                writer.writerow(
                    {
                        "case_id": "firebird_delete_all_rows",
                        "engine_id": "ref_firebird",
                        "dialect_family": "firebird",
                        "role": "donor",
                        "expectation": "must_match",
                        "behavior_class": "dml_delete_all_rows",
                        "translation_mode": "static_native_v3",
                        "exec_status": "ok",
                        "sqlstate": "00000",
                        "assert_count": 1,
                        "exec_elapsed_ms": 7,
                        "donor_source_path": "fb.sql",
                        "donor_converted_path": "fb.sql",
                    }
                )
                writer.writerow(
                    {
                        "case_id": "mysql_alias_wildcard_error",
                        "engine_id": "scratchbird_native",
                        "dialect_family": "mysql",
                        "role": "native",
                        "expectation": "must_fail_same_class",
                        "behavior_class": "wildcard_alias_rejection",
                        "translation_mode": "static_native_v3",
                        "exec_status": "error",
                        "sqlstate": "42000",
                        "assert_count": 0,
                        "exec_elapsed_ms": 4,
                        "donor_source_path": "my.sql",
                        "donor_converted_path": "my.sql",
                    }
                )
                writer.writerow(
                    {
                        "case_id": "mysql_alias_wildcard_error",
                        "engine_id": "ref_mysql",
                        "dialect_family": "mysql",
                        "role": "donor",
                        "expectation": "must_fail_same_class",
                        "behavior_class": "wildcard_alias_rejection",
                        "translation_mode": "static_native_v3",
                        "exec_status": "error",
                        "sqlstate": "42000",
                        "assert_count": 0,
                        "exec_elapsed_ms": 6,
                        "donor_source_path": "my.sql",
                        "donor_converted_path": "my.sql",
                    }
                )
            with (v3_comparative_artifact_dir / "comparative_pairwise_scores.csv").open("w", newline="", encoding="utf-8") as handle:
                writer = csv.DictWriter(
                    handle,
                    fieldnames=[
                        "case_id",
                        "dialect_family",
                        "donor_engine_id",
                        "native_engine_id",
                        "expectation",
                        "behavior_class",
                        "translation_mode",
                        "result",
                        "reason",
                        "native_exec_status",
                        "donor_exec_status",
                        "native_sqlstate",
                        "donor_sqlstate",
                        "native_exec_elapsed_ms",
                        "donor_exec_elapsed_ms",
                        "latency_ratio",
                        "donor_source_path",
                        "donor_converted_path",
                        "donor_curated_list",
                        "donor_statement_scope",
                    ],
                )
                writer.writeheader()
                writer.writerow(
                    {
                        "case_id": "firebird_delete_all_rows",
                        "dialect_family": "firebird",
                        "donor_engine_id": "ref_firebird",
                        "native_engine_id": "scratchbird_native",
                        "expectation": "must_match",
                        "behavior_class": "dml_delete_all_rows",
                        "translation_mode": "static_native_v3",
                        "result": "pass",
                        "reason": "",
                        "native_exec_status": "ok",
                        "donor_exec_status": "ok",
                        "native_sqlstate": "00000",
                        "donor_sqlstate": "00000",
                        "native_exec_elapsed_ms": 5,
                        "donor_exec_elapsed_ms": 7,
                        "latency_ratio": 0.714286,
                        "donor_source_path": "fb.sql",
                        "donor_converted_path": "fb.sql",
                        "donor_curated_list": "fb.list",
                        "donor_statement_scope": "delete all rows",
                    }
                )
                writer.writerow(
                    {
                        "case_id": "mysql_alias_wildcard_error",
                        "dialect_family": "mysql",
                        "donor_engine_id": "ref_mysql",
                        "native_engine_id": "scratchbird_native",
                        "expectation": "must_fail_same_class",
                        "behavior_class": "wildcard_alias_rejection",
                        "translation_mode": "static_native_v3",
                        "result": "pass",
                        "reason": "",
                        "native_exec_status": "error",
                        "donor_exec_status": "error",
                        "native_sqlstate": "42000",
                        "donor_sqlstate": "42000",
                        "native_exec_elapsed_ms": 4,
                        "donor_exec_elapsed_ms": 6,
                        "latency_ratio": 0.666667,
                        "donor_source_path": "my.sql",
                        "donor_converted_path": "my.sql",
                        "donor_curated_list": "my.list",
                        "donor_statement_scope": "wildcard alias error",
                    }
                )

            firebird_original_dir = repo / "tests" / "compatibility" / "firebird" / "results" / "ctest" / "firebird-original-a"
            firebird_emulation_dir = repo / "tests" / "compatibility" / "firebird" / "results" / "ctest" / "firebird-emulation-a"
            mysql_original_dir = repo / "tests" / "compatibility" / "mysql" / "results" / "ctest" / "mysql-original-a"
            mysql_upstream_incompatible_dir = repo / "tests" / "compatibility" / "mysql" / "results" / "ctest" / "mysql-upstream-z"
            mysql_emulation_dir = repo / "tests" / "compatibility" / "mysql" / "results" / "ctest" / "mysql-emulation-z"
            postgresql_original_dir = repo / "tests" / "compatibility" / "postgresql" / "results" / "ctest" / "postgresql-original-a"
            postgresql_upstream_incompatible_dir = repo / "tests" / "compatibility" / "postgresql" / "results" / "ctest" / "postgresql-upstream-z"
            postgresql_emulation_dir = repo / "tests" / "compatibility" / "postgresql" / "results" / "ctest" / "postgresql-emulation-z"
            scratchbird_regression_dir = repo / "tests" / "compatibility" / "scratchbird" / "results" / "ctest" / "scratchbird-a"

            compat_manifests = {
                firebird_original_dir: {
                    "engine": "firebird",
                    "protocol_surface": "firebird_remote",
                    "parser_core": "v3",
                    "parser_mode": "emulation_surface_only",
                    "comparison_suite_family": "emulation-comparison",
                    "comparison_contract_id": "compatibility-emulation-compare-v1",
                    "comparison_harness": "compatibility_converted_sql_ctest",
                    "comparison_target_role": "original",
                    "comparison_target_id": "upstream-firebird",
                    "execution_mode": "native_firebird_client",
                    "ctest_list_mode": "curated",
                    "ctest_list_file": str(repo / "tests" / "compatibility" / "firebird" / "config" / "ctest_list.txt"),
                    "listed_tests": 12,
                    "status": "passed",
                    "failure_count": 0,
                    "timestamp_utc": "2026-03-28T00:00:01Z",
                },
                firebird_emulation_dir: {
                    "engine": "firebird",
                    "protocol_surface": "firebird_remote",
                    "parser_core": "v3",
                    "parser_mode": "emulation_surface_only",
                    "comparison_suite_family": "emulation-comparison",
                    "comparison_contract_id": "compatibility-emulation-compare-v1",
                    "comparison_harness": "compatibility_converted_sql_ctest",
                    "comparison_target_role": "emulation",
                    "comparison_target_id": "scratchbird-firebird",
                    "execution_mode": "scratchbird_fb_emulation_client",
                    "ctest_list_mode": "curated",
                    "ctest_list_file": str(repo / "tests" / "compatibility" / "firebird" / "config" / "ctest_list.txt"),
                    "listed_tests": 12,
                    "status": "passed",
                    "failure_count": 0,
                    "timestamp_utc": "2026-03-28T00:00:02Z",
                },
                mysql_original_dir: {
                    "engine": "mysql",
                    "protocol_surface": "mysql_8x",
                    "parser_core": "v3",
                    "parser_mode": "emulation_surface_only",
                    "comparison_suite_family": "emulation-comparison",
                    "comparison_contract_id": "compatibility-emulation-compare-v1",
                    "comparison_harness": "compatibility_converted_sql_ctest",
                    "comparison_target_role": "original",
                    "comparison_target_id": "upstream-mysql",
                    "execution_mode": "converted_sql_ctest",
                    "ctest_list_mode": "curated",
                    "ctest_list_file": str(repo / "tests" / "compatibility" / "mysql" / "config" / "ctest_list.txt"),
                    "listed_tests": 4,
                    "status": "passed",
                    "failure_count": 0,
                    "require_sb_emulation_marker": "0",
                    "timestamp_utc": "2026-03-28T00:00:03Z",
                },
                mysql_upstream_incompatible_dir: {
                    "engine": "mysql",
                    "protocol_surface": "mysql_8x",
                    "parser_core": "v3",
                    "parser_mode": "emulation_surface_only",
                    "comparison_suite_family": "emulation-comparison",
                    "comparison_contract_id": "compatibility-emulation-compare-v1",
                    "comparison_harness": "upstream_mysql_test_run",
                    "comparison_target_role": "original",
                    "comparison_target_id": "upstream-mysql",
                    "execution_mode": "upstream_mysql_test_run",
                    "ctest_list_mode": "curated",
                    "ctest_list_file": str(repo / "tests" / "compatibility" / "mysql" / "config" / "ctest_list.txt"),
                    "listed_tests": 4,
                    "status": "failed",
                    "failure_count": 1,
                    "require_sb_emulation_marker": "0",
                    "timestamp_utc": "2026-03-28T00:00:05Z",
                },
                mysql_emulation_dir: {
                    "engine": "mysql",
                    "protocol_surface": "mysql_8x",
                    "parser_core": "v3",
                    "parser_mode": "emulation_surface_only",
                    "comparison_suite_family": "emulation-comparison",
                    "comparison_contract_id": "compatibility-emulation-compare-v1",
                    "comparison_harness": "compatibility_converted_sql_ctest",
                    "comparison_target_role": "emulation",
                    "comparison_target_id": "scratchbird-mysql",
                    "execution_mode": "converted_sql_ctest",
                    "ctest_list_mode": "curated",
                    "ctest_list_file": str(repo / "tests" / "compatibility" / "mysql" / "config" / "ctest_list.txt"),
                    "listed_tests": 4,
                    "status": "passed",
                    "failure_count": 0,
                    "require_sb_emulation_marker": "1",
                    "timestamp_utc": "2026-03-28T00:00:06Z",
                },
                postgresql_original_dir: {
                    "engine": "postgresql",
                    "protocol_surface": "postgresql_v3",
                    "parser_core": "v3",
                    "parser_mode": "emulation_surface_only",
                    "comparison_suite_family": "emulation-comparison",
                    "comparison_contract_id": "compatibility-emulation-compare-v1",
                    "comparison_harness": "compatibility_converted_sql_ctest",
                    "comparison_target_role": "original",
                    "comparison_target_id": "upstream-postgresql",
                    "execution_mode": "converted_sql_ctest",
                    "ctest_list_mode": "curated",
                    "ctest_list_file": str(repo / "tests" / "compatibility" / "postgresql" / "config" / "ctest_list.txt"),
                    "listed_tests": 5,
                    "status": "passed",
                    "failure_count": 0,
                    "require_sb_emulation_marker": "0",
                    "timestamp_utc": "2026-03-28T00:00:07Z",
                },
                postgresql_upstream_incompatible_dir: {
                    "engine": "postgresql",
                    "protocol_surface": "postgresql_v3",
                    "parser_core": "v3",
                    "parser_mode": "emulation_surface_only",
                    "comparison_suite_family": "emulation-comparison",
                    "comparison_contract_id": "compatibility-emulation-compare-v1",
                    "comparison_harness": "upstream_pg_regress",
                    "comparison_target_role": "original",
                    "comparison_target_id": "upstream-postgresql",
                    "execution_mode": "upstream_pg_regress",
                    "ctest_list_mode": "curated",
                    "ctest_list_file": str(repo / "tests" / "compatibility" / "postgresql" / "config" / "ctest_list.txt"),
                    "listed_tests": 5,
                    "status": "failed",
                    "failure_count": 1,
                    "require_sb_emulation_marker": "0",
                    "timestamp_utc": "2026-03-28T00:00:09Z",
                },
                postgresql_emulation_dir: {
                    "engine": "postgresql",
                    "protocol_surface": "postgresql_v3",
                    "parser_core": "v3",
                    "parser_mode": "emulation_surface_only",
                    "comparison_suite_family": "emulation-comparison",
                    "comparison_contract_id": "compatibility-emulation-compare-v1",
                    "comparison_harness": "compatibility_converted_sql_ctest",
                    "comparison_target_role": "emulation",
                    "comparison_target_id": "scratchbird-postgresql",
                    "execution_mode": "converted_sql_ctest",
                    "ctest_list_mode": "curated",
                    "ctest_list_file": str(repo / "tests" / "compatibility" / "postgresql" / "config" / "ctest_list.txt"),
                    "listed_tests": 5,
                    "status": "passed",
                    "failure_count": 0,
                    "require_sb_emulation_marker": "1",
                    "timestamp_utc": "2026-03-28T00:00:10Z",
                },
                scratchbird_regression_dir: {
                    "engine": "scratchbird",
                    "status": "passed",
                    "listed_tests": 100,
                    "failure_count": 0,
                    "timestamp_utc": "2026-03-28T00:00:00Z",
                },
            }
            for manifest_dir, payload in compat_manifests.items():
                manifest_dir.mkdir(parents=True, exist_ok=True)
                (manifest_dir / "RUN_MANIFEST.json").write_text(json.dumps(payload) + "\n", encoding="utf-8")

            perf_dir = repo / "scripts" / "verification_bundle" / "suite" / "results" / "perf" / "perf-a"
            (perf_dir / "perf_summary.csv").write_text("total_runs,passed,failed\n2,2,0\n", encoding="utf-8")
            with (perf_dir / "perf_all.csv").open("w", newline="", encoding="utf-8") as handle:
                writer = csv.DictWriter(
                    handle,
                    fieldnames=[
                        "workload_id",
                        "engine_id",
                        "lane",
                        "concurrency",
                        "operations_per_worker",
                        "successful_workers",
                        "failed_workers",
                        "wall_ms",
                        "result",
                        "reason",
                        "throughput_tps",
                        "latency_p50_ms",
                        "latency_p95_ms",
                        "latency_p99_ms",
                        "latency_avg_ms",
                        "error_rate",
                    ],
                )
                writer.writeheader()
                writer.writerow(
                    {
                        "workload_id": "wl1",
                        "engine_id": "scratchbird",
                        "lane": "native",
                        "concurrency": 1,
                        "operations_per_worker": 1,
                        "successful_workers": 1,
                        "failed_workers": 0,
                        "wall_ms": 10,
                        "result": "pass",
                        "reason": "",
                        "throughput_tps": 100,
                        "latency_p50_ms": 1,
                        "latency_p95_ms": 2,
                        "latency_p99_ms": 3,
                        "latency_avg_ms": 1.5,
                        "error_rate": 0,
                    }
                )

            opt_dir = repo / "scripts" / "verification_bundle" / "suite" / "results" / "optimizer_donor_compare" / "opt-a"
            (opt_dir / "optimizer_summary.json").write_text(json.dumps({"total_cases": 1}) + "\n", encoding="utf-8")
            with (opt_dir / "optimizer_engine_runs.csv").open("w", newline="", encoding="utf-8") as handle:
                writer = csv.DictWriter(
                    handle,
                    fieldnames=[
                        "case_id",
                        "engine_id",
                        "lane",
                        "surface_key",
                        "setup_status",
                        "plan_status",
                        "exec_status",
                        "teardown_status",
                        "plan_line_count",
                        "assert_line_count",
                        "metric_line_count",
                        "exec_elapsed_ms",
                    ],
                )
                writer.writeheader()
                writer.writerow(
                    {
                        "case_id": "c1",
                        "engine_id": "scratchbird_native",
                        "lane": "native",
                        "surface_key": "native",
                        "setup_status": "ok",
                        "plan_status": "ok",
                        "exec_status": "ok",
                        "teardown_status": "ok",
                        "plan_line_count": 1,
                        "assert_line_count": 1,
                        "metric_line_count": 0,
                        "exec_elapsed_ms": 5,
                    }
                )

            baseline_root = root / "ScratchBird-Benchmarks" / "results"
            baseline = baseline_root / "baseline-a"
            (baseline / "firebird" / "stress").mkdir(parents=True)
            (baseline / "firebird" / "stress" / "stress_firebird_20260327_000000.json").write_text(
                json.dumps({"summary": {"total_tests": 15, "passed": 15, "failed": 0, "errors": 0, "total_duration_ms": 1250.0}})
                + "\n",
                encoding="utf-8",
            )
            (baseline / "matrix-summary.json").write_text(
                json.dumps(
                    {
                        "run_id": "baseline-a",
                        "output_root": str(baseline),
                        "engines_requested": ["firebird"],
                        "suites_requested": ["stress"],
                        "suite_runs": [
                            {
                                "engine": "firebird",
                                "suite": "stress",
                                "started_at": "2026-03-27T00:00:00Z",
                                "duration_seconds": 100,
                                "status": "passed",
                                "exit_code": 0,
                                "output_dir": "firebird/stress",
                            }
                        ],
                    }
                )
                + "\n",
                encoding="utf-8",
            )
            empty_latest = baseline_root / "zz-empty-latest"
            empty_latest.mkdir(parents=True)
            (empty_latest / "matrix-summary.json").write_text(
                json.dumps(
                    {
                        "run_id": "empty-latest",
                        "output_root": str(empty_latest),
                        "engines_requested": ["firebird"],
                        "suites_requested": ["regression"],
                        "suite_runs": [],
                        "total_suite_runs": 0,
                        "result": "passed",
                    }
                )
                + "\n",
                encoding="utf-8",
            )

            (history_prior / "matrix-summary.json").write_text(
                json.dumps(
                    {
                        "run_id": "prior-run",
                        "output_root": str(history_prior),
                        "engines_requested": ["scratchbird"],
                        "suites_requested": ["full-gate"],
                        "suite_runs": [
                            {
                                "engine": "scratchbird",
                                "suite": "full-gate",
                                "started_at": "2026-03-27T00:00:00Z",
                                "duration_seconds": 20,
                                "status": "passed",
                                "exit_code": 0,
                                "output_dir": "scratchbird/full-gate",
                            }
                        ],
                    }
                )
                + "\n",
                encoding="utf-8",
            )
            (history_prior / "scratchbird" / "full-gate").mkdir(parents=True)
            (history_prior / "scratchbird" / "full-gate" / "full-gate-scratchbird-summary.json").write_text(
                json.dumps({"summary": {"total_time_sec": 20, "failed": 0}}) + "\n",
                encoding="utf-8",
            )
            MODULE.generate_unified_comparison_csv.write_unified_csv(history_prior / "matrix-summary.json", history_prior / "matrix-comparison-unified.csv")

            args = MODULE.parse_args.__wrapped__ if hasattr(MODULE.parse_args, "__wrapped__") else None
            output_root = MODULE.normalize(
                MODULE.argparse.Namespace(
                    repo_root=repo,
                    run_id="normalized-a",
                    output_root=None,
                    full_gate_dir=full_gate,
                    public_beta_dir=public_beta,
                    compatibility_root=repo / "tests" / "compatibility",
                    v3_native_inet_dir=v3_dir,
                    v3_native_comparative_dir=v3_comparative_dir,
                    verification_root=repo / "scripts" / "verification_bundle" / "suite" / "results",
                    benchmarks_root=baseline_root,
                    history_root=history_root,
                    history_limit=5,
                    skip_system_info=False,
                )
            )

            self.assertTrue((output_root / "matrix-summary.json").exists())
            self.assertTrue((output_root / "matrix-comparison-unified.csv").exists())
            self.assertTrue((output_root / "benchmark-baseline-comparison.csv").exists())
            self.assertTrue((output_root / "history-comparison.csv").exists())
            self.assertTrue((output_root / "benchmark-matrix-import.json").exists())
            self.assertTrue((output_root / "scratchbird" / "public-beta" / "public-beta-scratchbird-summary.json").exists())
            self.assertTrue((output_root / "firebird" / "regression" / "regression-firebird-summary.json").exists())
            self.assertTrue((output_root / "firebird" / "stress" / "stress_firebird_20260327_000000.json").exists())
            self.assertTrue((output_root / "upstream-mysql" / "emulation-comparison" / "emulation-comparison-upstream-mysql-summary.json").exists())
            self.assertTrue((output_root / "scratchbird-mysql" / "emulation-comparison" / "emulation-comparison-scratchbird-mysql-summary.json").exists())
            self.assertTrue((output_root / "emulation-comparison-pairwise.json").exists())
            self.assertTrue((output_root / "native-comparative-regression-pairwise.json").exists())
            self.assertTrue((output_root / "scratchbird-native" / "native-comparative-regression" / "native-comparative-regression-scratchbird-native-summary.json").exists())
            self.assertTrue((output_root / "system-info.json").exists())
            baseline_md = (output_root / "benchmark-baseline-comparison.md").read_text(encoding="utf-8")
            self.assertIn("## Baseline Suite Health", baseline_md)
            self.assertIn("`firebird`", baseline_md)
            self.assertIn("## Baseline Key Metrics", baseline_md)
            self.assertNotIn("No exact `(suite, metric, engine)` overlap exists", baseline_md)

            pairwise = json.loads((output_root / "emulation-comparison-pairwise.json").read_text(encoding="utf-8"))
            verdicts = {item["dialect_family"]: item["verdict"] for item in pairwise["pairs"]}
            self.assertEqual(verdicts["firebird"], "comparable")
            self.assertEqual(verdicts["mysql"], "comparable")
            self.assertEqual(verdicts["postgresql"], "comparable")
            native_pairwise = json.loads((output_root / "native-comparative-regression-pairwise.json").read_text(encoding="utf-8"))
            self.assertEqual(
                native_pairwise["translation_contract"]["translation_contract_id"],
                "vncr-frozen-static-corpus-v1",
            )
            native_summary = json.loads(
                (
                    output_root
                    / "scratchbird-native"
                    / "native-comparative-regression"
                    / "native-comparative-regression-scratchbird-native-summary.json"
                ).read_text(encoding="utf-8")
            )
            self.assertEqual(
                native_summary["comparison_contract"]["translation_contract"]["corpus_fingerprint_sha256"],
                "abc123",
            )

            summary = json.loads((output_root / "matrix-summary.json").read_text(encoding="utf-8"))
            with (output_root / "matrix-comparison-unified.csv").open(newline="", encoding="utf-8") as handle:
                unified_rows = list(csv.DictReader(handle))
            by_key = {(row["suite"], row["metric"]): row for row in unified_rows}
            self.assertEqual(by_key[("stress", "matrix.duration_seconds")]["firebird"], "100.0")
            self.assertEqual(by_key[("stress", "summary.passed")]["firebird"], "15")
            self.assertIn("emulation-comparison", summary["suites_requested"])
            self.assertIn("native-comparative-regression", summary["suites_requested"])
            self.assertIn("public-beta", summary["suites_requested"])
            self.assertIn("stress", summary["suites_requested"])
            self.assertIn("scratchbird", summary["engines_requested"])
            self.assertIn("scratchbird-native", summary["engines_requested"])
            self.assertIn("firebird", summary["engines_requested"])
            self.assertIn("upstream-firebird", summary["engines_requested"])
            self.assertIn("scratchbird-postgresql", summary["engines_requested"])
            native_suite_status = {
                (row["engine"], row["suite"]): row["status"]
                for row in summary["suite_runs"]
                if row["suite"] == "native-comparative-regression"
            }
            self.assertEqual(native_suite_status[("scratchbird-native", "native-comparative-regression")], "passed")
            self.assertEqual(native_suite_status[("upstream-firebird", "native-comparative-regression")], "passed")
            self.assertEqual(native_suite_status[("upstream-mysql", "native-comparative-regression")], "passed")

    def test_emulation_comparison_marks_missing_same_contract_baseline(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            compat_root = root / "tests" / "compatibility"
            output_root = root / "out"
            emulation_dir = compat_root / "mysql" / "results" / "ctest" / "mysql-emulation-z"
            upstream_incompatible_dir = compat_root / "mysql" / "results" / "ctest" / "mysql-upstream-z"
            emulation_dir.mkdir(parents=True)
            upstream_incompatible_dir.mkdir(parents=True)
            manifest_base = {
                "engine": "mysql",
                "protocol_surface": "mysql_8x",
                "parser_core": "v3",
                "parser_mode": "emulation_surface_only",
                "comparison_suite_family": "emulation-comparison",
                "comparison_contract_id": "compatibility-emulation-compare-v1",
                "ctest_list_mode": "curated",
                "ctest_list_file": str(root / "tests" / "compatibility" / "mysql" / "config" / "ctest_list.txt"),
                "listed_tests": 4,
            }
            (emulation_dir / "RUN_MANIFEST.json").write_text(
                json.dumps(
                    {
                        **manifest_base,
                        "comparison_harness": "compatibility_converted_sql_ctest",
                        "comparison_target_role": "emulation",
                        "comparison_target_id": "scratchbird-mysql",
                        "execution_mode": "converted_sql_ctest",
                        "status": "passed",
                        "failure_count": 0,
                        "require_sb_emulation_marker": "1",
                        "timestamp_utc": "2026-03-28T01:00:00Z",
                    }
                )
                + "\n",
                encoding="utf-8",
            )
            (upstream_incompatible_dir / "RUN_MANIFEST.json").write_text(
                json.dumps(
                    {
                        **manifest_base,
                        "comparison_harness": "upstream_mysql_test_run",
                        "comparison_target_role": "original",
                        "comparison_target_id": "upstream-mysql",
                        "execution_mode": "upstream_mysql_test_run",
                        "status": "failed",
                        "failure_count": 1,
                        "require_sb_emulation_marker": "0",
                        "timestamp_utc": "2026-03-28T01:00:01Z",
                    }
                )
                + "\n",
                encoding="utf-8",
            )

            artifacts = MODULE.parse_emulation_comparison_manifests(compat_root, output_root)
            self.assertEqual(len(artifacts), 6)
            upstream_summary = json.loads(
                (output_root / "upstream-mysql" / "emulation-comparison" / "emulation-comparison-upstream-mysql-summary.json").read_text(
                    encoding="utf-8"
                )
            )
            self.assertEqual(upstream_summary["summary"]["result"], "missing")
            self.assertEqual(
                upstream_summary["comparison_contract"]["contract_state"],
                "missing_same_contract_source",
            )
            pairwise = json.loads((output_root / "emulation-comparison-pairwise.json").read_text(encoding="utf-8"))
            mysql_pair = next(item for item in pairwise["pairs"] if item["dialect_family"] == "mysql")
            self.assertEqual(mysql_pair["verdict"], "missing-or-ineligible-source")


if __name__ == "__main__":
    unittest.main()
