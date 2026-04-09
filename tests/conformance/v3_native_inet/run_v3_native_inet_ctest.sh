#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
DRIVER_DIR="${REPO_DIR}-driver"

DEFAULT_ISQL="${REPO_DIR}/build/src/sb_isql"
if [[ ! -x "${DEFAULT_ISQL}" ]]; then
  ALT_ISQL="${REPO_DIR}/build/src/cli/sb_isql"
  if [[ -x "${ALT_ISQL}" ]]; then
    DEFAULT_ISQL="${ALT_ISQL}"
  else
    for DRIVER_ISQL in \
      "${DRIVER_DIR}/build_cli/tracks/p3/drivers/cli/sb_isql" \
      "${DRIVER_DIR}/build_cli/tracks/alpha/drivers/cli/sb_isql" \
      "${DRIVER_DIR}/build/tracks/p3/drivers/cli/sb_isql" \
      "${DRIVER_DIR}/build/tracks/alpha/drivers/cli/sb_isql"
    do
      if [[ -x "${DRIVER_ISQL}" ]]; then
        DEFAULT_ISQL="${DRIVER_ISQL}"
        break
      fi
    done
  fi
fi

ISQL_BIN="${SCRATCHBIRD_NATIVE_ISQL:-${SCRATCHBIRD_SB_ISQL:-${DEFAULT_ISQL}}}"
LIST_FILE="${SCRATCHBIRD_V3_INET_CTEST_LIST:-${SCRIPT_DIR}/config/v3_native_inet_list.txt}"
RUN_ID="$(date +%Y%m%d_%H%M%S)"
RESULTS_DIR="${SCRIPT_DIR}/results/ctest/${RUN_ID}"
mkdir -p "${RESULTS_DIR}"

DEFAULT_ENV_FILE="/tmp/scratchbird-example-dynamic/profiles/runtime.env"
ENV_FILE="${SCRATCHBIRD_EXAMPLE_ENV_FILE:-${DEFAULT_ENV_FILE}}"
if [[ -f "${ENV_FILE}" ]]; then
  # shellcheck disable=SC1090
  source "${ENV_FILE}"
fi

HOST="${SCRATCHBIRD_NATIVE_HOST:-127.0.0.1}"
PORT="${SCRATCHBIRD_NATIVE_PORT:-16092}"
DBNAME="${SCRATCHBIRD_NATIVE_DB:-main}"
DBUSER="${SCRATCHBIRD_NATIVE_USER:-SysArch}"
DBPASS="${SCRATCHBIRD_NATIVE_PASSWORD:-replaceme}"

json_escape() {
  local value="$1"
  value="${value//\\/\\\\}"
  value="${value//\"/\\\"}"
  printf '%s' "$value"
}

count_list_entries() {
  local list_file="$1"
  grep -Ev '^[[:space:]]*(#|$)' "$list_file" | wc -l | tr -d '[:space:]'
}

write_run_manifest() {
  local run_status="${1:-running}"
  local failure_count="${2:-0}"
  local listed_tests
  listed_tests="$(count_list_entries "$LIST_FILE")"
  cat > "${RESULTS_DIR}/RUN_MANIFEST.json" <<JSON
{
  "run_id": "$(json_escape "$RUN_ID")",
  "engine": "scratchbird_native",
  "protocol_surface": "sbwp_native",
  "parser_core": "v3",
  "parser_mode": "native_core",
  "execution_mode": "inet_tcp_listener",
  "isql_binary": "$(json_escape "$ISQL_BIN")",
  "ctest_list_file": "$(json_escape "$LIST_FILE")",
  "listed_tests": ${listed_tests},
  "status": "$(json_escape "$run_status")",
  "failure_count": ${failure_count},
  "results_dir": "$(json_escape "$RESULTS_DIR")",
  "host": "$(json_escape "$HOST")",
  "port": "$(json_escape "$PORT")",
  "database": "$(json_escape "$DBNAME")",
  "username": "$(json_escape "$DBUSER")",
  "timestamp_utc": "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
}
JSON

  cat > "${RESULTS_DIR}/PARSER_BOUNDARY.txt" <<'TXT'
parser_core=v3
parser_mode=native_core
protocol_surface=sbwp_native
transport=inet_tcp
statement_path=v3_core_parser_via_listener
TXT
}

run_pass_case() {
  local sql_rel="$1"
  local expected_rel="$2"
  local case_name
  case_name="$(basename "${sql_rel}" .sql)"
  local checks_file="${SCRIPT_DIR}/checks/${case_name}.checks"

  local sql_file="${SCRIPT_DIR}/${sql_rel}"
  local expected_file="${SCRIPT_DIR}/${expected_rel}"
  local out_file="${RESULTS_DIR}/${case_name}.out"
  local err_file="${RESULTS_DIR}/${case_name}.err"

  [[ -f "${sql_file}" ]] || { echo "missing SQL: ${sql_file}" >&2; return 1; }
  [[ -f "${expected_file}" ]] || { echo "missing expected: ${expected_file}" >&2; return 1; }

  set +e
  "${ISQL_BIN}" "${DBNAME}" \
    --mode=local-ipc \
    --ipc-method=tcp \
    --sslmode=disable \
    -H "${HOST}" \
    -p "${PORT}" \
    -U "${DBUSER}" \
    -P "${DBPASS}" \
    -b \
    -f "${sql_file}" \
    -o "${out_file}" \
    -q < /dev/null > /dev/null 2> "${err_file}"
  local rc=$?
  set -e

  if [[ ${rc} -ne 0 ]] || rg -q "Error:" "${err_file}"; then
    echo "Execution error in pass case: ${sql_rel} (see ${err_file})" >&2
    return 1
  fi

  local pattern
  while IFS= read -r pattern || [[ -n "${pattern}" ]]; do
    [[ -z "${pattern}" || "${pattern}" == \#* ]] && continue
    if ! rg -Fq -- "${pattern}" "${out_file}" && ! rg -Fq -- "${pattern}" "${err_file}"; then
      echo "Expected output pattern not found for ${sql_rel}: ${pattern}" >&2
      return 1
    fi
  done < "${expected_file}"

  if [[ -f "${checks_file}" ]]; then
    local check_line
    local check_idx=0
    while IFS= read -r -u 9 check_line || [[ -n "${check_line}" ]]; do
      [[ -z "${check_line}" || "${check_line}" == \#* ]] && continue

      if [[ "${check_line}" != *"|||"* ]]; then
        echo "Invalid check line in ${checks_file}: ${check_line}" >&2
        return 1
      fi

      local check_sql="${check_line%%|||*}"
      local check_pattern="${check_line#*|||}"
      if [[ -z "${check_sql}" || -z "${check_pattern}" ]]; then
        echo "Invalid check line in ${checks_file}: ${check_line}" >&2
        return 1
      fi

      check_idx=$((check_idx + 1))
      local check_out="${RESULTS_DIR}/${case_name}.check${check_idx}.out"
      local check_err="${RESULTS_DIR}/${case_name}.check${check_idx}.err"

      set +e
      "${ISQL_BIN}" "${DBNAME}" \
        --mode=local-ipc \
        --ipc-method=tcp \
        --sslmode=disable \
        -H "${HOST}" \
        -p "${PORT}" \
        -U "${DBUSER}" \
        -P "${DBPASS}" \
        -c "${check_sql}" \
        -o "${check_out}" \
        -q < /dev/null > /dev/null 2> "${check_err}"
      local check_rc=$?
      set -e

      if [[ ${check_rc} -ne 0 ]] || rg -q "Error:" "${check_err}"; then
        echo "Post-check execution error in ${case_name}: ${check_sql}" >&2
        return 1
      fi
      if ! rg -Fq -- "${check_pattern}" "${check_out}" && ! rg -Fq -- "${check_pattern}" "${check_err}"; then
        echo "Post-check pattern not found for ${case_name}: ${check_pattern}" >&2
        return 1
      fi
    done 9< "${checks_file}"
  fi

  printf 'CASE|%s|PASS\n' "${case_name}" >> "${RESULTS_DIR}/case_status.txt"
  return 0
}

run_fail_case() {
  local sql_rel="$1"
  local expected_rel="$2"
  local case_name
  case_name="$(basename "${sql_rel}" .sql)"

  local sql_file="${SCRIPT_DIR}/${sql_rel}"
  local expected_file="${SCRIPT_DIR}/${expected_rel}"
  local out_file="${RESULTS_DIR}/${case_name}.out"
  local err_file="${RESULTS_DIR}/${case_name}.err"

  [[ -f "${sql_file}" ]] || { echo "missing SQL: ${sql_file}" >&2; return 1; }
  [[ -f "${expected_file}" ]] || { echo "missing expected: ${expected_file}" >&2; return 1; }

  set +e
  "${ISQL_BIN}" "${DBNAME}" \
    --mode=local-ipc \
    --ipc-method=tcp \
    --sslmode=disable \
    -H "${HOST}" \
    -p "${PORT}" \
    -U "${DBUSER}" \
    -P "${DBPASS}" \
    -b \
    -f "${sql_file}" \
    -o "${out_file}" \
    -q < /dev/null > /dev/null 2> "${err_file}"
  set -e

  if ! rg -q "Error:" "${err_file}"; then
    echo "Expected parser/runtime error but none captured: ${sql_rel}" >&2
    return 1
  fi

  local pattern
  while IFS= read -r pattern || [[ -n "${pattern}" ]]; do
    [[ -z "${pattern}" || "${pattern}" == \#* ]] && continue
    if ! rg -Fq -- "${pattern}" "${err_file}" && ! rg -Fq -- "${pattern}" "${out_file}"; then
      echo "Expected error pattern not found for ${sql_rel}: ${pattern}" >&2
      return 1
    fi
  done < "${expected_file}"

  printf 'CASE|%s|PASS(expected_failure)\n' "${case_name}" >> "${RESULTS_DIR}/case_status.txt"
  return 0
}

write_run_manifest "initialized" 0

if [[ ! -x "${ISQL_BIN}" ]]; then
  echo "SKIP: sb_isql not found or not executable: ${ISQL_BIN}" >&2
  write_run_manifest "skipped" 0
  exit 77
fi

if [[ ! -f "${LIST_FILE}" ]]; then
  echo "Error: list file not found: ${LIST_FILE}" >&2
  write_run_manifest "failed" 1
  exit 1
fi

PRECHECK_SQL="${RESULTS_DIR}/precheck.sql"
PRECHECK_OUT="${RESULTS_DIR}/precheck.out"
PRECHECK_ERR="${RESULTS_DIR}/precheck.err"
cat > "${PRECHECK_SQL}" <<'SQL'
SELECT 1;
SHOW server_version;
SQL

set +e
"${ISQL_BIN}" "${DBNAME}" \
    --mode=local-ipc \
    --ipc-method=tcp \
    --sslmode=disable \
    -H "${HOST}" \
    -p "${PORT}" \
    -U "${DBUSER}" \
    -P "${DBPASS}" \
    -f "${PRECHECK_SQL}" \
    -o "${PRECHECK_OUT}" \
    -q > /dev/null 2> "${PRECHECK_ERR}"
precheck_rc=$?
set -e

if [[ ${precheck_rc} -ne 0 ]] || rg -q "Error:" "${PRECHECK_ERR}"; then
  echo "SKIP: native inet endpoint not reachable with configured profile" >&2
  cat "${PRECHECK_OUT}" >&2 || true
  cat "${PRECHECK_ERR}" >&2 || true
  write_run_manifest "skipped" 0
  exit 77
fi

failures=0
while IFS='|' read -r -u 8 mode sql_rel expected_rel || [[ -n "${mode}${sql_rel}${expected_rel}" ]]; do
  [[ -z "${mode}" || "${mode}" == \#* ]] && continue

  case "${mode}" in
    pass)
      if ! run_pass_case "${sql_rel}" "${expected_rel}"; then
        failures=$((failures + 1))
      fi
      ;;
    fail)
      if ! run_fail_case "${sql_rel}" "${expected_rel}"; then
        failures=$((failures + 1))
      fi
      ;;
    *)
      echo "Unknown mode in list: ${mode}" >&2
      failures=$((failures + 1))
      ;;
  esac
done 8< "${LIST_FILE}"

if [[ ${failures} -ne 0 ]]; then
  write_run_manifest "failed" "${failures}"
  echo "v3 native inet parser suite failed cases: ${failures}" >&2
  exit 1
fi

write_run_manifest "passed" 0
echo "v3 native inet parser suite passed. Results: ${RESULTS_DIR}"
