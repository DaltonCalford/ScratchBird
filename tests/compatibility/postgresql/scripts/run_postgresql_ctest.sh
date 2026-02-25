#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PG_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../../../.." && pwd)"
DRIVER_DIR="${ROOT_DIR}-driver"

DEFAULT_ISQL="${ROOT_DIR}/build/src/sb_pg_isql"
ISQL_FLAVOR="postgresql"
if [[ ! -x "$DEFAULT_ISQL" ]]; then
  ALT_ISQL="${ROOT_DIR}/build/src/cli/sb_pg_isql"
  if [[ -x "$ALT_ISQL" ]]; then
    DEFAULT_ISQL="$ALT_ISQL"
  else
    DRIVER_PG_ISQL="${DRIVER_DIR}/build/tracks/alpha/drivers/cli/sb_pg_isql"
    DRIVER_GENERIC_ISQL="${DRIVER_DIR}/build/tracks/alpha/drivers/cli/sb_isql"
    if [[ -x "$DRIVER_PG_ISQL" ]]; then
      DEFAULT_ISQL="$DRIVER_PG_ISQL"
    elif [[ -x "$DRIVER_GENERIC_ISQL" ]]; then
      DEFAULT_ISQL="$DRIVER_GENERIC_ISQL"
      ISQL_FLAVOR="generic"
    fi
  fi
fi
ISQL_BIN="${SCRATCHBIRD_PG_ISQL:-$DEFAULT_ISQL}"
if [[ -n "${SCRATCHBIRD_PG_ISQL:-}" ]]; then
  case "$(basename "$ISQL_BIN")" in
    sb_isql) ISQL_FLAVOR="generic" ;;
    *) ISQL_FLAVOR="postgresql" ;;
  esac
fi
LIST_FILE="${SCRATCHBIRD_PG_CTEST_LIST:-$PG_DIR/config/ctest_list.txt}"
CONVERTED_DIR="${PG_DIR}/converted"

if [[ ! -x "$ISQL_BIN" ]]; then
  echo "SKIP: sb_pg_isql not found or not executable: $ISQL_BIN" >&2
  exit 77
fi

if [[ "$ISQL_FLAVOR" == "generic" ]]; then
  cat >&2 <<'EOF'
SKIP: generic sb_isql fallback is not valid for PostgreSQL emulation compare runs.
The generic client speaks native protocol only and cannot execute PostgreSQL wire-protocol parity.
Provide sb_pg_isql via SCRATCHBIRD_PG_ISQL, or build FDW CLI wrappers in ScratchBird-driver.
EOF
  exit 77
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
DBNAME="${SCRATCHBIRD_PG_DB:-postgres}"
PASSWORD="${SCRATCHBIRD_PG_PASSWORD:-${PGPASSWORD:-}}"

RUN_ID="$(date +%Y%m%d_%H%M%S)"
RESULTS_DIR="${PG_DIR}/results/ctest/${RUN_ID}"
mkdir -p "$RESULTS_DIR"

PRECHECK_FILE="${RESULTS_DIR}/precheck.sql"
PRECHECK_OUT="${RESULTS_DIR}/precheck.out"
cat > "$PRECHECK_FILE" <<'EOF'
SELECT 1;
EOF
if ! PGPASSWORD="$PASSWORD" "$ISQL_BIN" -h "$HOST" -p "$PORT" -U "$USER" -d "$DBNAME" \
     -f "$PRECHECK_FILE" -o "$PRECHECK_OUT" -q 2>> "$PRECHECK_OUT"; then
  echo "SKIP: PostgreSQL compatibility endpoint is not reachable with current client/auth settings." >&2
  cat "$PRECHECK_OUT" >&2
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
       -f "$test_file" -o "$out_file" -q 2>> "$out_file"; then
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
