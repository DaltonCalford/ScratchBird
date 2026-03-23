#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FB_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../../../.." && pwd)"
CLIWORK_ROOT="$(cd "${ROOT_DIR}/.." && pwd)"

DEFAULT_ISQL=""
for candidate in \
  "${CLIWORK_ROOT}/firebird/gen/Release/firebird/bin/isql" \
  "${CLIWORK_ROOT}/firebird/gen/Debug/firebird/bin/isql" \
  "${CLIWORK_ROOT}/firebird/build/bin/isql" \
  "${CLIWORK_ROOT}/firebird/build/isql" \
  "${FB_DIR}/repos/firebird/gen/Release/firebird/bin/isql"; do
  if [[ -x "$candidate" ]]; then
    DEFAULT_ISQL="$candidate"
    break
  fi
done
ISQL_MODE="native_firebird"
# Emulated Firebird verification must use donor `isql`/`isql-fb`. `sb_isql` is
# native ScratchBird/V3 only and is intentionally not accepted for this lane.
ISQL_BIN="${SCRATCHBIRD_FB_NATIVE_ISQL:-${SCRATCHBIRD_FB_ISQL:-$DEFAULT_ISQL}}"
LIST_MODE="${SCRATCHBIRD_FB_CTEST_LIST_MODE:-${SCRATCHBIRD_COMPAT_CTEST_LIST_MODE:-curated}}"
case "$LIST_MODE" in
  curated)
    DEFAULT_LIST_FILE="$FB_DIR/config/ctest_list.txt"
    ;;
  expanded)
    DEFAULT_LIST_FILE="$FB_DIR/config/ctest_list_expanded.txt"
    ;;
  full)
    DEFAULT_LIST_FILE="$FB_DIR/config/ctest_list_full.txt"
    ;;
  *)
    echo "Error: invalid Firebird list mode '$LIST_MODE' (expected curated|expanded|full)." >&2
    exit 2
    ;;
esac
LIST_FILE="${SCRATCHBIRD_FB_CTEST_LIST:-$DEFAULT_LIST_FILE}"
CONVERTED_DIR="${FB_DIR}/converted"

if [[ ! -x "$ISQL_BIN" ]]; then
  echo "SKIP: cloned Firebird isql not found or not executable: $ISQL_BIN" >&2
  exit 77
fi

case "$(basename "$ISQL_BIN")" in
  isql|isql-fb)
    ;;
  *)
    cat >&2 <<EOF
SKIP: ScratchBird Firebird wrapper clients are deprecated for compatibility compare runs.
Provide the cloned donor isql via SCRATCHBIRD_FB_NATIVE_ISQL. sb_isql is native/V3 only.
Resolved path: ${ISQL_BIN}
EOF
    exit 77
    ;;
esac

if [[ ! -f "$LIST_FILE" ]]; then
  echo "Error: Firebird CTest list not found: $LIST_FILE (mode=$LIST_MODE)" >&2
  echo "Hint: run tests/compatibility/scripts/generate_ctest_lists.py to generate expanded/full lists." >&2
  exit 1
fi

if [[ ! -d "$CONVERTED_DIR" ]]; then
  echo "Error: Firebird converted tests missing: $CONVERTED_DIR" >&2
  exit 1
fi

RUN_ID="$(date +%Y%m%d_%H%M%S)"
RESULTS_DIR="${FB_DIR}/results/ctest/${RUN_ID}"
WORK_DIR="${RESULTS_DIR}/work"
mkdir -p "$RESULTS_DIR" "$WORK_DIR"
FB_USER="${SCRATCHBIRD_FB_USER:-SYSDBA}"
FB_PASSWORD="${SCRATCHBIRD_FB_PASSWORD:-masterkey}"
PER_TEST_TIMEOUT_SEC="${SCRATCHBIRD_FB_PER_TEST_TIMEOUT_SEC:-120}"
TIMEOUT_BIN="$(command -v timeout 2>/dev/null || true)"

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
  "engine": "firebird",
  "protocol_surface": "firebird_remote",
  "parser_core": "v3",
  "parser_mode": "emulation_surface_only",
  "execution_mode": "$([[ "$ISQL_MODE" == "native_firebird" ]] && echo "native_firebird_client" || echo "scratchbird_fb_emulation_client")",
  "isql_binary": "$(json_escape "$ISQL_BIN")",
  "ctest_list_mode": "$(json_escape "$LIST_MODE")",
  "ctest_list_file": "$(json_escape "$LIST_FILE")",
  "listed_tests": ${listed_tests},
  "status": "$(json_escape "$run_status")",
  "failure_count": ${failure_count},
  "results_dir": "$(json_escape "$RESULTS_DIR")",
  "isql_mode": "$(json_escape "$ISQL_MODE")",
  "username": "$(json_escape "$FB_USER")",
  "timestamp_utc": "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
}
EOF
  cat > "${RESULTS_DIR}/PARSER_BOUNDARY.txt" <<'EOF'
parser_core=v3
parser_mode=emulation_surface_only
protocol_surface=firebird_remote
statement_path=v3_core_parser_then_emulation_adapter
EOF
}

write_run_manifest "initialized" 0

if [[ "$ISQL_MODE" == "native_firebird" ]]; then
  PRECHECK_FILE="${WORK_DIR}/precheck.sql"
  PRECHECK_LOG="${RESULTS_DIR}/precheck.log"
  cat > "$PRECHECK_FILE" <<'EOF'
SHOW VERSION;
EOF
  if ! "$ISQL_BIN" -user "$FB_USER" -password "$FB_PASSWORD" -q -i "$PRECHECK_FILE" \
       > "$PRECHECK_LOG" 2>&1; then
    echo "SKIP: Firebird endpoint is not reachable with current isql-fb credentials/settings." >&2
    cat "$PRECHECK_LOG" >&2
    write_run_manifest "skipped" 0
    exit 77
  fi
fi

failures=()

is_sql_compatible_test() {
  local file_path="$1"
  local test_type
  test_type="$(awk -F: '
    BEGIN { type="" }
    /^[[:space:]]*--[[:space:]]*Test Type:[[:space:]]*/ {
      type=$2
      gsub(/^[[:space:]]+|[[:space:]]+$/, "", type)
      print tolower(type)
      exit
    }
  ' "$file_path" || true)"

  if [[ -z "$test_type" ]]; then
    return 0
  fi
  [[ "$test_type" == *sql* ]]
}

while IFS= read -r rel_path; do
  if [[ -z "$rel_path" || "$rel_path" == \#* ]]; then
    continue
  fi

  test_file="${CONVERTED_DIR}/${rel_path}"
  if [[ ! -f "$test_file" ]]; then
    failures+=("$rel_path (missing)")
    continue
  fi

  base_name="$(basename "$rel_path")"
  safe_name="${rel_path//\//_}"
  db_file="${WORK_DIR}/${safe_name}.sbdb"
  log_file="${RESULTS_DIR}/${safe_name}.log"
  emulated_path="${WORK_DIR}/${safe_name}.fdb"
  run_file="${WORK_DIR}/${safe_name}.run.sql"
  if ! is_sql_compatible_test "$test_file"; then
    {
      echo "SKIP: non-SQL Firebird test type in converted lane"
      echo "source=${test_file}"
    } > "$log_file"
    continue
  fi

  cat > "$run_file" <<EOF
-- === PRELUDE SCRIPT ===
CREATE DATABASE '${emulated_path}';
CONNECT '${emulated_path}';
EOF
  awk 'BEGIN{emit=0} /^-- ===/ {emit=1} emit {print}' "$test_file" >> "$run_file"

  if [[ "$ISQL_MODE" == "native_firebird" ]]; then
    cmd=("$ISQL_BIN" -user "$FB_USER" -password "$FB_PASSWORD" -q -i "$run_file")
  else
    cmd=("$ISQL_BIN" "$db_file" -q -f "$run_file")
  fi

  rc=0
  if [[ -n "$TIMEOUT_BIN" && "$PER_TEST_TIMEOUT_SEC" -gt 0 ]]; then
    if ! "$TIMEOUT_BIN" --signal=TERM "${PER_TEST_TIMEOUT_SEC}s" "${cmd[@]}" > "$log_file" 2>&1; then
      rc=$?
    fi
  else
    if ! "${cmd[@]}" > "$log_file" 2>&1; then
      rc=$?
    fi
  fi

  if [[ "$rc" -ne 0 ]]; then
    if [[ "$rc" -eq 124 || "$rc" -eq 137 ]]; then
      echo "TIMEOUT: test exceeded ${PER_TEST_TIMEOUT_SEC}s" >> "$log_file"
    fi
    failures+=("$rel_path (see ${log_file})")
  fi
done < "$LIST_FILE"

if [[ ${#failures[@]} -ne 0 ]]; then
  echo "Firebird compatibility failures:" >&2
  for item in "${failures[@]}"; do
    echo "  - ${item}" >&2
  done
  write_run_manifest "failed" "${#failures[@]}"
  exit 1
fi

write_run_manifest "passed" 0
echo "Firebird compatibility tests passed. Logs: ${RESULTS_DIR}"
