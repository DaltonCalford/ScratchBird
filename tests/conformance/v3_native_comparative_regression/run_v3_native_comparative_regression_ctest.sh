#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
VERIFY_ROOT="${REPO_ROOT}/scripts/verification_bundle/suite"
RUNNER="${VERIFY_ROOT}/scripts/comparative_regression_runner.py"
ENGINES="${VERIFY_ROOT}/configs/native_comparative_regression_engines.json"
CORPUS="${VERIFY_ROOT}/configs/native_comparative_regression_corpus.json"

RUN_ID="$(date +%Y%m%d_%H%M%S)"
RESULTS_DIR="${SCRIPT_DIR}/results/ctest/${RUN_ID}"
mkdir -p "${RESULTS_DIR}"

DEFAULT_ENV_FILE="/tmp/scratchbird-example-dynamic/profiles/runtime.env"
ENV_FILE="${SCRATCHBIRD_EXAMPLE_ENV_FILE:-${DEFAULT_ENV_FILE}}"
if [[ -f "${ENV_FILE}" ]]; then
  set -a
  # shellcheck disable=SC1090
  source "${ENV_FILE}"
  set +a
fi

DEFAULT_SB_ISQL=""
for candidate in \
  "${REPO_ROOT}/../ScratchBird-driver/build/tracks/p3/drivers/cli/sb_isql" \
  "${REPO_ROOT}/build/src/sb_isql" \
  "${REPO_ROOT}/build/src/cli/sb_isql" \
  "${REPO_ROOT}/../ScratchBird-driver/build/tracks/alpha/drivers/cli/sb_isql" \
  "$(command -v sb_isql 2>/dev/null || true)"; do
  if [[ -n "${candidate}" && -x "${candidate}" ]]; then
    DEFAULT_SB_ISQL="${candidate}"
    break
  fi
done

DEFAULT_PSQL=""
for candidate in \
  "${REPO_ROOT}/../postgresql/build_codex/src/bin/psql/psql" \
  "${REPO_ROOT}/../postgresql/build_codex2/src/bin/psql/psql" \
  "${REPO_ROOT}/../postgresql/build/src/bin/psql/psql" \
  "$(command -v psql 2>/dev/null || true)"; do
  if [[ -n "${candidate}" && -x "${candidate}" ]]; then
    DEFAULT_PSQL="${candidate}"
    break
  fi
done

DEFAULT_MYSQL=""
for candidate in \
  "${REPO_ROOT}/../mysql-server/build_codex2/runtime_output_directory/mysql" \
  "${REPO_ROOT}/../mysql-server/build_codex/runtime_output_directory/mysql" \
  "${REPO_ROOT}/../mysql-server/build/runtime_output_directory/mysql" \
  "$(command -v mysql 2>/dev/null || true)"; do
  if [[ -n "${candidate}" && -x "${candidate}" ]]; then
    DEFAULT_MYSQL="${candidate}"
    break
  fi
done

DEFAULT_FB_ISQL=""
for candidate in \
  "${REPO_ROOT}/../firebird/gen/Release/firebird/bin/isql" \
  "${REPO_ROOT}/../firebird/gen/Debug/firebird/bin/isql" \
  "${REPO_ROOT}/../firebird/build/bin/isql" \
  "$(command -v isql-fb 2>/dev/null || true)" \
  "$(command -v isql 2>/dev/null || true)"; do
  if [[ -n "${candidate}" && -x "${candidate}" ]]; then
    DEFAULT_FB_ISQL="${candidate}"
    break
  fi
done

SB_ISQL_BIN="${SCRATCHBIRD_NATIVE_ISQL:-${SCRATCHBIRD_SB_ISQL:-${DEFAULT_SB_ISQL}}}"
PSQL_BIN="${SB_VERIFY_REF_PG_BIN:-${DEFAULT_PSQL}}"
MYSQL_BIN="${SB_VERIFY_REF_MY_BIN:-${DEFAULT_MYSQL}}"
FB_ISQL_BIN="${SB_VERIFY_REF_FB_BIN:-${DEFAULT_FB_ISQL}}"

SB_HOST="${SCRATCHBIRD_NATIVE_HOST:-127.0.0.1}"
SB_PORT="${SCRATCHBIRD_NATIVE_PORT:-16092}"
SB_DB="${SCRATCHBIRD_NATIVE_DB:-main}"
SB_USER="${SCRATCHBIRD_NATIVE_USER:-SysArch}"
SB_PASSWORD="${SCRATCHBIRD_NATIVE_PASSWORD:-replaceme}"

PG_HOST="${SB_VERIFY_REF_PG_HOST:-127.0.0.1}"
PG_PORT="${SB_VERIFY_REF_PG_PORT:-5433}"
PG_DB="${SB_VERIFY_REF_PG_DB:-benchmark}"
PG_USER="${SB_VERIFY_REF_PG_USER:-benchmark}"
PG_PASSWORD="${SB_VERIFY_REF_PG_PASSWORD:-benchmark}"

MY_HOST="${SB_VERIFY_REF_MY_HOST:-127.0.0.1}"
MY_PORT="${SB_VERIFY_REF_MY_PORT:-3306}"
MY_DB="${SB_VERIFY_REF_MY_DB:-benchmark}"
MY_USER="${SB_VERIFY_REF_MY_USER:-benchmark}"
MY_PASSWORD="${SB_VERIFY_REF_MY_PASSWORD:-benchmark}"

FB_HOST="${SB_VERIFY_REF_FB_HOST:-127.0.0.1}"
FB_PORT="${SB_VERIFY_REF_FB_PORT:-3050}"
FB_DB="${SB_VERIFY_REF_FB_DB:-/firebird/data/benchmark.fdb}"
FB_USER="${SB_VERIFY_REF_FB_USER:-benchmark}"
FB_PASSWORD="${SB_VERIFY_REF_FB_PASSWORD:-benchmark}"

json_escape() {
  local value="$1"
  value="${value//\\/\\\\}"
  value="${value//\"/\\\"}"
  printf '%s' "$value"
}

write_manifest() {
  local status="$1"
  local failure_count="$2"
  local total_cases="$3"
  cat > "${RESULTS_DIR}/RUN_MANIFEST.json" <<EOF
{
  "run_id": "$(json_escape "${RUN_ID}")",
  "engine": "scratchbird_native",
  "protocol_surface": "sbwp_native",
  "parser_core": "v3",
  "parser_mode": "native_core",
  "comparison_suite_family": "native-comparative-regression",
  "comparison_contract_id": "native-v3-comparative-regression-v1",
  "comparison_harness": "static_translated_regression_corpus",
  "translation_contract_id": "vncr-frozen-static-corpus-v1",
  "translation_runtime_allowed": false,
  "execution_mode": "static_translated_regression_corpus",
  "engines_config": "$(json_escape "${ENGINES}")",
  "corpus_config": "$(json_escape "${CORPUS}")",
  "listed_tests": ${total_cases},
  "status": "$(json_escape "${status}")",
  "failure_count": ${failure_count},
  "results_dir": "$(json_escape "${RESULTS_DIR}")",
  "timestamp_utc": "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
}
EOF

  cat > "${RESULTS_DIR}/PARSER_BOUNDARY.txt" <<'EOF'
parser_core=v3
parser_mode=native_core
protocol_surface=sbwp_native
statement_path=static_native_v3_comparative_corpus
translation_mode=frozen_on_disk_namespace_substitution_only
EOF
}

skip_suite() {
  local message="$1"
  echo "SKIP: ${message}" >&2
  write_manifest "skipped" 0 0
  exit 77
}

[[ -x "${SB_ISQL_BIN}" ]] || skip_suite "sb_isql not found: ${SB_ISQL_BIN}"
[[ -x "${PSQL_BIN}" ]] || skip_suite "psql not found: ${PSQL_BIN}"
[[ -x "${MYSQL_BIN}" ]] || skip_suite "mysql not found: ${MYSQL_BIN}"
[[ -x "${FB_ISQL_BIN}" ]] || skip_suite "firebird isql not found: ${FB_ISQL_BIN}"

write_manifest "initialized" 0 0

SB_PRECHECK_SQL="${RESULTS_DIR}/sb_precheck.sql"
cat > "${SB_PRECHECK_SQL}" <<'SQL'
SELECT 1;
SQL

set +e
"${SB_ISQL_BIN}" "${SB_DB}" --mode=local-ipc --ipc-method=tcp --sslmode=disable \
  -H "${SB_HOST}" -p "${SB_PORT}" -U "${SB_USER}" -P "${SB_PASSWORD}" \
  -b -f "${SB_PRECHECK_SQL}" -o "${RESULTS_DIR}/sb_precheck.out" -q \
  > /dev/null 2> "${RESULTS_DIR}/sb_precheck.err"
SB_RC=$?
PGPASSWORD="${PG_PASSWORD}" "${PSQL_BIN}" -h "${PG_HOST}" -p "${PG_PORT}" -U "${PG_USER}" -d "${PG_DB}" \
  -Atqc "SELECT 1;" > "${RESULTS_DIR}/pg_precheck.out" 2> "${RESULTS_DIR}/pg_precheck.err"
PG_RC=$?
env MYSQL_PWD="${MY_PASSWORD}" "${MYSQL_BIN}" --protocol=TCP -h "${MY_HOST}" -P "${MY_PORT}" -u "${MY_USER}" "${MY_DB}" \
  -e "SELECT 1;" > "${RESULTS_DIR}/my_precheck.out" 2> "${RESULTS_DIR}/my_precheck.err"
MY_RC=$?
"${FB_ISQL_BIN}" -user "${FB_USER}" -password "${FB_PASSWORD}" "${FB_HOST}/${FB_PORT}:${FB_DB}" -q \
  > "${RESULTS_DIR}/fb_precheck.out" 2> "${RESULTS_DIR}/fb_precheck.err" <<'SQL'
SELECT 1 FROM RDB$DATABASE;
SQL
FB_RC=$?
set -e

if [[ "${SB_RC}" -ne 0 || "${PG_RC}" -ne 0 || "${MY_RC}" -ne 0 || "${FB_RC}" -ne 0 ]]; then
  skip_suite "one or more native/donor endpoints are unavailable"
fi

export SCRATCHBIRD_NATIVE_ISQL="${SB_ISQL_BIN}"
export SB_VERIFY_REF_PG_BIN="${PSQL_BIN}"
export SB_VERIFY_REF_MY_BIN="${MYSQL_BIN}"
export SB_VERIFY_REF_FB_BIN="${FB_ISQL_BIN}"

python3 "${RUNNER}" \
  --engines "${ENGINES}" \
  --corpus "${CORPUS}" \
  --workspace-root "${VERIFY_ROOT}" \
  --repo-root "${REPO_ROOT}/.." \
  --out-dir "${RESULTS_DIR}"

RUNNER_RESULT_DIR="${RESULTS_DIR}"
LATEST_RUNNER_SUBDIR="$(find "${RESULTS_DIR}" -mindepth 1 -maxdepth 1 -type d | sort | tail -n 1 || true)"
if [[ -n "${LATEST_RUNNER_SUBDIR}" ]]; then
  RUNNER_RESULT_DIR="${LATEST_RUNNER_SUBDIR}"
fi

SUMMARY_JSON="${RUNNER_RESULT_DIR}/comparative_summary.json"
PAIRWISE_CSV="${RUNNER_RESULT_DIR}/comparative_pairwise_scores.csv"
TOTAL_CASES=0
FAILURES=0
if [[ -f "${SUMMARY_JSON}" ]]; then
  TOTAL_CASES="$(python3 - <<'PY' "${SUMMARY_JSON}"
import json, sys
obj = json.load(open(sys.argv[1], encoding='utf-8'))
print(int(obj.get('total_pairs', 0)))
PY
)"
  FAILURES="$(python3 - <<'PY' "${SUMMARY_JSON}"
import json, sys
obj = json.load(open(sys.argv[1], encoding='utf-8'))
print(int(obj.get('failed_pairs', 0)))
PY
)"
fi

if [[ -f "${PAIRWISE_CSV}" ]]; then
  python3 - <<'PY' "${PAIRWISE_CSV}" "${RESULTS_DIR}/case_status.txt"
import csv, sys
rows = list(csv.DictReader(open(sys.argv[1], encoding='utf-8')))
with open(sys.argv[2], 'w', encoding='utf-8') as handle:
    for row in rows:
        handle.write(f"CASE|{row['case_id']}|{row['dialect_family']}|{row['result'].upper()}\n")
PY
fi

if [[ "${FAILURES}" -ne 0 ]]; then
  write_manifest "failed" "${FAILURES}" "${TOTAL_CASES}"
  exit 1
fi

write_manifest "passed" 0 "${TOTAL_CASES}"
echo "Native comparative regression passed. Results: ${RESULTS_DIR}"
