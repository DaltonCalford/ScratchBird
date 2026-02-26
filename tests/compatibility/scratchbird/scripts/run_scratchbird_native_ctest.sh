#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SB_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../../../.." && pwd)"
DRIVER_DIR="${ROOT_DIR}-driver"

DEFAULT_ISQL="${ROOT_DIR}/build/src/sb_isql"
if [[ ! -x "${DEFAULT_ISQL}" ]]; then
  ALT_ISQL="${ROOT_DIR}/build/src/cli/sb_isql"
  if [[ -x "${ALT_ISQL}" ]]; then
    DEFAULT_ISQL="${ALT_ISQL}"
  else
    DRIVER_ISQL="${DRIVER_DIR}/build/tracks/alpha/drivers/cli/sb_isql"
    if [[ -x "${DRIVER_ISQL}" ]]; then
      DEFAULT_ISQL="${DRIVER_ISQL}"
    fi
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
DBUSER="${SCRATCHBIRD_NATIVE_USER:-sb_admin}"
DBPASS="${SCRATCHBIRD_NATIVE_PASSWORD:-SbAdmin_Compat1!}"

if [[ ! -x "${ISQL_BIN}" ]]; then
  echo "SKIP: sb_isql not found or not executable: ${ISQL_BIN}" >&2
  exit 77
fi

if [[ ! -f "${LIST_FILE}" ]]; then
  echo "Error: ScratchBird native CTest list not found: ${LIST_FILE}" >&2
  exit 1
fi

PRECHECK_SQL="${RESULTS_DIR}/precheck.sql"
PRECHECK_OUT="${RESULTS_DIR}/precheck.out"
cat > "${PRECHECK_SQL}" <<'SQL'
SELECT 1;
SHOW server_version;
SQL

if ! "${ISQL_BIN}" "${DBNAME}" \
    --mode=local-ipc \
    --ipc-method=tcp \
    -H "${HOST}" \
    -p "${PORT}" \
    -U "${DBUSER}" \
    -P "${DBPASS}" \
    -f "${PRECHECK_SQL}" \
    -o "${PRECHECK_OUT}" \
    -q; then
  echo "SKIP: ScratchBird native compatibility endpoint not reachable with configured profile." >&2
  cat "${PRECHECK_OUT}" >&2 || true
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
      -H "${HOST}" \
      -p "${PORT}" \
      -U "${DBUSER}" \
      -P "${DBPASS}" \
      -b \
      -f "${test_file}" \
      -o "${out_file}" \
      -q; then
    failures+=("${rel_path} (see ${out_file})")
  fi
done < "${LIST_FILE}"

if [[ ${#failures[@]} -ne 0 ]]; then
  echo "ScratchBird native compatibility failures:" >&2
  for item in "${failures[@]}"; do
    echo "  - ${item}" >&2
  done
  exit 1
fi

echo "ScratchBird native compatibility tests passed. Logs: ${RESULTS_DIR}"
