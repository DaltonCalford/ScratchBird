#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

REGISTRY_JSON="${1:-${REPO_ROOT}/docs/planning/native_sql/NATIVE_GAP_FEATURE_REGISTRY.json}"
SCOPE_TSV="${2:-${REPO_ROOT}/tests/unit/data/native_sql_syn13_registration_scope.tsv}"
BINDING_REPORT_CSV="${3:-${REPO_ROOT}/docs/planning/native_sql/gates/NSQL-GATE-04/AST_SBLR_BINDING_REPORT.csv}"
OUT_DIR="${4:-${REPO_ROOT}/docs/planning/native_sql/gates/NSQL-GATE-05}"

SYN_OUT="${OUT_DIR}/SYN13_COVERAGE_REPORT.csv"
MATRIX_OUT="${OUT_DIR}/NATIVE_CAPABILITY_MATRIX.csv"
ENGINE_OUT="${OUT_DIR}/ENGINE_SURFACE_PACK_COVERAGE.csv"
SYN_SUMMARY="${OUT_DIR}/SYN13_COVERAGE_SUMMARY.env"
MATRIX_SUMMARY="${OUT_DIR}/CAPABILITY_MATRIX_SUMMARY.env"
UNMAPPED_IDS_OUT="${OUT_DIR}/unmapped_ids.txt"

if [[ ! -f "${REGISTRY_JSON}" ]]; then
    echo "error: registry file not found: ${REGISTRY_JSON}" >&2
    exit 2
fi
if [[ ! -f "${SCOPE_TSV}" ]]; then
    echo "error: scope file not found: ${SCOPE_TSV}" >&2
    exit 2
fi
if [[ ! -f "${BINDING_REPORT_CSV}" ]]; then
    echo "error: binding report not found: ${BINDING_REPORT_CSV}" >&2
    exit 2
fi

mkdir -p "${OUT_DIR}"

SYN13_FORCE_ALL_MANDATORY=1 \
SYN13_ALLOW_MANDATORY_MISS=1 \
"${SCRIPT_DIR}/native_sql_syn13_coverage.sh" \
    "${REGISTRY_JSON}" \
    "${SCOPE_TSV}" \
    "${SYN_OUT}" \
    "${SYN_SUMMARY}"

NATIVE_SQL_ALLOW_OPEN_MANDATORY=1 \
"${SCRIPT_DIR}/native_sql_capability_matrix_freeze.sh" \
    "${REGISTRY_JSON}" \
    "${SYN_OUT}" \
    "${BINDING_REPORT_CSV}" \
    "${MATRIX_OUT}" \
    "${ENGINE_OUT}" \
    "${MATRIX_SUMMARY}"

awk -F, '
    NR == 1 { next }
    $10 == "\"unmapped\"" {
        gsub(/"/, "", $1);
        print $1;
    }
' "${MATRIX_OUT}" | LC_ALL=C sort > "${UNMAPPED_IDS_OUT}"

printf "SCOPE_PROMOTION_COMPLETE=1\nOUTPUT_DIR=%s\n" "${OUT_DIR}" >&2
