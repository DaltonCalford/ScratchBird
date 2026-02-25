#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MY_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../../../.." && pwd)"
DRIVER_DIR="${ROOT_DIR}-driver"

DEFAULT_ISQL="${ROOT_DIR}/build/src/sb_my_isql"
ISQL_FLAVOR="mysql"
if [[ ! -x "$DEFAULT_ISQL" ]]; then
  ALT_ISQL="${ROOT_DIR}/build/src/cli/sb_my_isql"
  if [[ -x "$ALT_ISQL" ]]; then
    DEFAULT_ISQL="$ALT_ISQL"
  else
    DRIVER_MY_ISQL="${DRIVER_DIR}/build/tracks/alpha/drivers/cli/sb_my_isql"
    DRIVER_GENERIC_ISQL="${DRIVER_DIR}/build/tracks/alpha/drivers/cli/sb_isql"
    if [[ -x "$DRIVER_MY_ISQL" ]]; then
      DEFAULT_ISQL="$DRIVER_MY_ISQL"
    elif [[ -x "$DRIVER_GENERIC_ISQL" ]]; then
      DEFAULT_ISQL="$DRIVER_GENERIC_ISQL"
      ISQL_FLAVOR="generic"
    fi
  fi
fi
ISQL_BIN="${SCRATCHBIRD_MY_ISQL:-$DEFAULT_ISQL}"
if [[ -n "${SCRATCHBIRD_MY_ISQL:-}" ]]; then
  case "$(basename "$ISQL_BIN")" in
    sb_isql) ISQL_FLAVOR="generic" ;;
    *) ISQL_FLAVOR="mysql" ;;
  esac
fi
LIST_FILE="${SCRATCHBIRD_MY_CTEST_LIST:-$MY_DIR/config/ctest_list.txt}"
CONVERTED_DIR="${MY_DIR}/converted"

if [[ ! -x "$ISQL_BIN" ]]; then
  echo "SKIP: sb_my_isql not found or not executable: $ISQL_BIN" >&2
  exit 77
fi

if [[ "$ISQL_FLAVOR" == "generic" ]]; then
  cat >&2 <<'EOF'
SKIP: generic sb_isql fallback is not valid for MySQL emulation compare runs.
The generic client speaks native protocol only and cannot execute MySQL wire-protocol parity.
Provide sb_my_isql via SCRATCHBIRD_MY_ISQL, or build FDW CLI wrappers in ScratchBird-driver.
EOF
  exit 77
fi

if [[ ! -f "$LIST_FILE" ]]; then
  echo "Error: MySQL CTest list not found: $LIST_FILE" >&2
  exit 1
fi

if [[ ! -d "$CONVERTED_DIR" ]]; then
  echo "Error: MySQL converted tests missing: $CONVERTED_DIR" >&2
  exit 1
fi

HOST="${SCRATCHBIRD_MY_HOST:-localhost}"
PORT="${SCRATCHBIRD_MY_PORT:-3306}"
USER="${SCRATCHBIRD_MY_USER:-root}"
PASSWORD="${SCRATCHBIRD_MY_PASSWORD:-}"
DB_ROOT="${SCRATCHBIRD_MY_DB:-compat_mysql}"
PER_TEST_DB="${SCRATCHBIRD_MY_DB_PER_TEST:-0}"

RUN_ID="$(date +%Y%m%d_%H%M%S)"
RESULTS_DIR="${MY_DIR}/results/ctest/${RUN_ID}"
WORK_DIR="${RESULTS_DIR}/work"
mkdir -p "$RESULTS_DIR" "$WORK_DIR"

PRECHECK_FILE="${WORK_DIR}/precheck.sql"
cat > "$PRECHECK_FILE" <<'EOF'
SELECT 1;
EOF

precheck_cmd=("$ISQL_BIN" -h "$HOST" -P "$PORT" -u "$USER" -f "$PRECHECK_FILE" -q)
if [[ -n "$PASSWORD" ]]; then
  precheck_cmd+=("-p${PASSWORD}")
fi
if ! precheck_output="$("${precheck_cmd[@]}" 2>&1)"; then
  echo "SKIP: MySQL compatibility endpoint is not reachable with current client/auth settings." >&2
  echo "$precheck_output" >&2
  exit 77
fi

failures=()

while IFS= read -r rel_path; do
  if [[ -z "$rel_path" || "$rel_path" == \#* ]]; then
    continue
  fi

  if [[ "$rel_path" != *.sql ]]; then
    rel_path="${rel_path}.sql"
  fi

  test_file="${CONVERTED_DIR}/${rel_path}"
  if [[ ! -f "$test_file" ]]; then
    failures+=("$rel_path (missing)")
    continue
  fi

  safe_name="${rel_path//\//_}"
  db_name="$DB_ROOT"
  if [[ "$PER_TEST_DB" == "1" ]]; then
    db_name="${DB_ROOT}_${safe_name}"
  fi

  run_file="${WORK_DIR}/${safe_name}.run.sql"
  log_file="${RESULTS_DIR}/${safe_name}.log"

  cat > "$run_file" <<EOF
CREATE DATABASE IF NOT EXISTS \`${db_name}\`;
USE \`${db_name}\`;
EOF
  cat "$test_file" >> "$run_file"

  cmd=("$ISQL_BIN" -h "$HOST" -P "$PORT" -u "$USER" -f "$run_file" -q)
  if [[ -n "$PASSWORD" ]]; then
    cmd+=("-p${PASSWORD}")
  fi

  if ! "${cmd[@]}" > "$log_file" 2>&1; then
    failures+=("$rel_path (see ${log_file})")
  fi
done < "$LIST_FILE"

if [[ ${#failures[@]} -ne 0 ]]; then
  echo "MySQL compatibility failures:" >&2
  for item in "${failures[@]}"; do
    echo "  - ${item}" >&2
  done
  exit 1
fi

echo "MySQL compatibility tests passed. Logs: ${RESULTS_DIR}"
