#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PG_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../../../.." && pwd)"

if [[ "${SCRATCHBIRD_PG_COMPAT_RUN:-0}" != "1" ]]; then
  echo "PostgreSQL compatibility tests skipped (set SCRATCHBIRD_PG_COMPAT_RUN=1 to run)."
  exit 0
fi

DEFAULT_ISQL="${ROOT_DIR}/build/src/sb_pg_isql"
if [[ ! -x "$DEFAULT_ISQL" ]]; then
  ALT_ISQL="${ROOT_DIR}/build/src/cli/sb_pg_isql"
  if [[ -x "$ALT_ISQL" ]]; then
    DEFAULT_ISQL="$ALT_ISQL"
  fi
fi
ISQL_BIN="${SCRATCHBIRD_PG_ISQL:-$DEFAULT_ISQL}"
LIST_FILE="${SCRATCHBIRD_PG_CTEST_LIST:-$PG_DIR/config/ctest_list.txt}"
CONVERTED_DIR="${PG_DIR}/converted"

if [[ ! -x "$ISQL_BIN" ]]; then
  echo "Error: sb_pg_isql not found or not executable: $ISQL_BIN" >&2
  exit 1
fi

if [[ ! -f "$LIST_FILE" ]]; then
  echo "Error: PostgreSQL CTest list not found: $LIST_FILE" >&2
  exit 1
fi

if [[ ! -d "$CONVERTED_DIR" ]]; then
  echo "Error: PostgreSQL converted tests missing: $CONVERTED_DIR" >&2
  exit 1
fi

HOST="${SCRATCHBIRD_PG_HOST:-localhost}"
PORT="${SCRATCHBIRD_PG_PORT:-5432}"
USER="${SCRATCHBIRD_PG_USER:-postgres}"
DBNAME="${SCRATCHBIRD_PG_DB:-default}"
PASSWORD="${SCRATCHBIRD_PG_PASSWORD:-${PGPASSWORD:-}}"

RUN_ID="$(date +%Y%m%d_%H%M%S)"
RESULTS_DIR="${PG_DIR}/results/ctest/${RUN_ID}"
mkdir -p "$RESULTS_DIR"

failures=()

while IFS= read -r rel_path; do
  if [[ -z "$rel_path" || "$rel_path" == \#* ]]; then
    continue
  fi

  if [[ "$rel_path" != *.sql ]]; then
    rel_path="${rel_path}.sql"
  fi
  if [[ "$rel_path" != */* ]]; then
    rel_path="core/${rel_path}"
  fi

  test_file="${CONVERTED_DIR}/${rel_path}"
  if [[ ! -f "$test_file" ]]; then
    failures+=("$rel_path (missing)")
    continue
  fi

  safe_name="${rel_path//\//_}"
  out_file="${RESULTS_DIR}/${safe_name}.out"

  if ! PGPASSWORD="$PASSWORD" "$ISQL_BIN" -h "$HOST" -p "$PORT" -U "$USER" -d "$DBNAME" \
       -f "$test_file" -o "$out_file" -q; then
    failures+=("$rel_path (see ${out_file})")
  fi
done < "$LIST_FILE"

if [[ ${#failures[@]} -ne 0 ]]; then
  echo "PostgreSQL compatibility failures:" >&2
  for item in "${failures[@]}"; do
    echo "  - ${item}" >&2
  done
  exit 1
fi

echo "PostgreSQL compatibility tests passed. Logs: ${RESULTS_DIR}"
