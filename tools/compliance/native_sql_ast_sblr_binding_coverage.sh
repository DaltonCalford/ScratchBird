#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

SCOPE_TSV="${1:-${REPO_ROOT}/tests/unit/data/native_sql_syn13_registration_scope.tsv}"
REJECT_TSV="${2:-${REPO_ROOT}/tests/unit/data/native_sql_reject_consistency_scope.tsv}"
OUT_CSV="${3:-${REPO_ROOT}/build/native_sql_ast_sblr_binding_report.csv}"
SUMMARY_OUT="${4:-}"
TEST_BIN="${5:-${REPO_ROOT}/build/tests/scratchbird_tests}"

if [[ ! -f "${SCOPE_TSV}" ]]; then
    echo "error: scope file not found: ${SCOPE_TSV}" >&2
    exit 2
fi
if [[ ! -f "${REJECT_TSV}" ]]; then
    echo "error: reject file not found: ${REJECT_TSV}" >&2
    exit 2
fi
if [[ ! -x "${TEST_BIN}" ]]; then
    echo "error: test binary not found or not executable: ${TEST_BIN}" >&2
    exit 2
fi

mkdir -p "$(dirname "${OUT_CSV}")"

SB_AST_SBLR_BINDING_SCOPE="${SCOPE_TSV}" \
SB_AST_SBLR_REJECT_SCOPE="${REJECT_TSV}" \
SB_AST_SBLR_BINDING_REPORT_OUT="${OUT_CSV}" \
timeout 3600s "${TEST_BIN}" \
    --gtest_filter='NativeSqlAstSblrBindingCoverageMeterTest.MandatoryScopeAndRejectMatrixAreDeterministic'

if [[ ! -f "${OUT_CSV}" ]]; then
    echo "error: coverage report not generated: ${OUT_CSV}" >&2
    exit 3
fi

total_count=$(awk -F, 'NR>1 {++n} END {print n+0}' "${OUT_CSV}")
pass_count=$(awk -F, 'NR>1 && $6=="pass" {++n} END {print n+0}' "${OUT_CSV}")
fail_count=$(awk -F, 'NR>1 && $6!="pass" {++n} END {print n+0}' "${OUT_CSV}")
mandatory_count=$(awk -F, 'NR>1 && $5=="mandatory" {++n} END {print n+0}' "${OUT_CSV}")
mandatory_fail_count=$(awk -F, 'NR>1 && $5=="mandatory" && $6!="pass" {++n} END {print n+0}' "${OUT_CSV}")
reject_count=$(awk -F, 'NR>1 && $5=="reject" {++n} END {print n+0}' "${OUT_CSV}")
reject_fail_count=$(awk -F, 'NR>1 && $5=="reject" && $6!="pass" {++n} END {print n+0}' "${OUT_CSV}")

summary_text="TOTAL=${total_count}
PASS=${pass_count}
FAIL=${fail_count}
MANDATORY_ROWS=${mandatory_count}
MANDATORY_FAIL=${mandatory_fail_count}
REJECT_ROWS=${reject_count}
REJECT_FAIL=${reject_fail_count}
OUTPUT_CSV=${OUT_CSV}"

if [[ -n "${SUMMARY_OUT}" ]]; then
    mkdir -p "$(dirname "${SUMMARY_OUT}")"
    printf "%s\n" "${summary_text}" > "${SUMMARY_OUT}"
fi

printf "%s\n" "${summary_text}" >&2

if [[ "${fail_count}" -ne 0 ]]; then
    exit 4
fi

