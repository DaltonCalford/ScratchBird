#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SB_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../../../.." && pwd)"
DRIVER_DIR="${ROOT_DIR}-driver"

# `sb_isql` belongs to the native ScratchBird/V3 lane. Emulated PostgreSQL,
# MySQL, and Firebird verification must use the original engine tools instead.
DEFAULT_ISQL="${ROOT_DIR}/build/src/sb_isql"
if [[ ! -x "${DEFAULT_ISQL}" ]]; then
  ALT_ISQL="${ROOT_DIR}/build/src/cli/sb_isql"
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
LIST_FILE="${SCRATCHBIRD_NATIVE_CTEST_LIST:-${SB_DIR}/config/example_ctest_list.txt}"
RUN_ID="$(date +%Y%m%d_%H%M%S)"
RESULTS_DIR="${SB_DIR}/results/ctest/${RUN_ID}"
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
  cat > "${RESULTS_DIR}/RUN_MANIFEST.json" <<EOF
{
  "run_id": "$(json_escape "$RUN_ID")",
  "engine": "scratchbird_native",
  "protocol_surface": "sbwp_native",
  "parser_core": "v3",
  "parser_mode": "native_core",
  "execution_mode": "native_sql_ctest",
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
EOF
  cat > "${RESULTS_DIR}/PARSER_BOUNDARY.txt" <<'EOF'
parser_core=v3
parser_mode=native_core
protocol_surface=sbwp_native
statement_path=v3_core_parser_direct
EOF
}

write_run_manifest "initialized" 0

if [[ ! -x "${ISQL_BIN}" ]]; then
  echo "SKIP: sb_isql not found or not executable: ${ISQL_BIN}" >&2
  write_run_manifest "skipped" 0
  exit 77
fi

if [[ ! -f "${LIST_FILE}" ]]; then
  echo "Error: ScratchBird native CTest list not found: ${LIST_FILE}" >&2
  write_run_manifest "failed" 1
  exit 1
fi

PRECHECK_SQL="${RESULTS_DIR}/precheck.sql"
PRECHECK_OUT="${RESULTS_DIR}/precheck.out"
cat > "${PRECHECK_SQL}" <<'SQL'
SELECT 1;
SQL

if ! "${ISQL_BIN}" "${DBNAME}" \
    --mode=local-ipc \
    --ipc-method=tcp \
    --sslmode=disable \
    -H "${HOST}" \
    -p "${PORT}" \
    -U "${DBUSER}" \
    -P "${DBPASS}" \
    -f "${PRECHECK_SQL}" \
    -o "${PRECHECK_OUT}" \
    -q; then
  echo "SKIP: ScratchBird native compatibility endpoint not reachable with configured profile." >&2
  cat "${PRECHECK_OUT}" >&2 || true
  write_run_manifest "skipped" 0
  exit 77
fi

failures=()
while IFS= read -r rel_path; do
  [[ -z "${rel_path}" || "${rel_path}" == \#* ]] && continue

  test_file="${SB_DIR}/${rel_path}"
  if [[ ! -f "${test_file}" ]]; then
    failures+=("${rel_path} (missing)")
    continue
  fi

  safe_name="${rel_path//\//_}"
  out_file="${RESULTS_DIR}/${safe_name}.out"

  if ! "${ISQL_BIN}" "${DBNAME}" \
      --mode=local-ipc \
      --ipc-method=tcp \
      --sslmode=disable \
      -H "${HOST}" \
      -p "${PORT}" \
      -U "${DBUSER}" \
      -P "${DBPASS}" \
      -b \
      -f "${test_file}" \
      -o "${out_file}" \
      -q; then
    failures+=("${rel_path} (see ${out_file})")
    continue
  fi

  if rg -q "Error:|Stopping due to error" "${out_file}"; then
    failures+=("${rel_path} (reported execution errors in ${out_file})")
  fi
done < "${LIST_FILE}"

if [[ ${#failures[@]} -ne 0 ]]; then
  echo "ScratchBird native compatibility failures:" >&2
  for item in "${failures[@]}"; do
    echo "  - ${item}" >&2
  done
  write_run_manifest "failed" "${#failures[@]}"
  exit 1
fi

write_run_manifest "passed" 0
echo "ScratchBird native compatibility tests passed. Logs: ${RESULTS_DIR}"
