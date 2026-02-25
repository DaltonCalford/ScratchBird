#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FB_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../../../.." && pwd)"
DRIVER_DIR="${ROOT_DIR}-driver"

DEFAULT_ISQL="${ROOT_DIR}/build/src/sb_fb_isql"
ISQL_FLAVOR="firebird"
ISQL_MODE="scratchbird"
if [[ ! -x "$DEFAULT_ISQL" ]]; then
  ALT_ISQL="${ROOT_DIR}/build/src/cli/sb_fb_isql"
  if [[ -x "$ALT_ISQL" ]]; then
    DEFAULT_ISQL="$ALT_ISQL"
  else
    DRIVER_FB_ISQL="${DRIVER_DIR}/build/tracks/alpha/drivers/cli/sb_fb_isql"
    DRIVER_GENERIC_ISQL="${DRIVER_DIR}/build/tracks/alpha/drivers/cli/sb_isql"
    SYSTEM_FB_ISQL="$(command -v isql-fb 2>/dev/null || true)"
    if [[ -x "$DRIVER_FB_ISQL" ]]; then
      DEFAULT_ISQL="$DRIVER_FB_ISQL"
    elif [[ -n "$SYSTEM_FB_ISQL" ]]; then
      DEFAULT_ISQL="$SYSTEM_FB_ISQL"
      ISQL_MODE="native_firebird"
    elif [[ -x "$DRIVER_GENERIC_ISQL" ]]; then
      DEFAULT_ISQL="$DRIVER_GENERIC_ISQL"
      ISQL_FLAVOR="generic"
    fi
  fi
fi
ISQL_BIN="${SCRATCHBIRD_FB_ISQL:-$DEFAULT_ISQL}"
if [[ -n "${SCRATCHBIRD_FB_ISQL:-}" ]]; then
  case "$(basename "$ISQL_BIN")" in
    sb_isql) ISQL_FLAVOR="generic" ;;
    isql-fb|isql) ISQL_FLAVOR="firebird"; ISQL_MODE="native_firebird" ;;
    *) ISQL_FLAVOR="firebird"; ISQL_MODE="scratchbird" ;;
  esac
fi
LIST_FILE="${SCRATCHBIRD_FB_CTEST_LIST:-$FB_DIR/config/ctest_list.txt}"
CONVERTED_DIR="${FB_DIR}/converted"

if [[ ! -x "$ISQL_BIN" ]]; then
  echo "SKIP: sb_fb_isql not found or not executable: $ISQL_BIN" >&2
  exit 77
fi

if [[ "$ISQL_FLAVOR" == "generic" ]]; then
  cat >&2 <<'EOF'
SKIP: generic sb_isql fallback is not valid for Firebird emulation compare runs.
The generic client speaks native protocol only and cannot execute Firebird wire-protocol parity.
Provide sb_fb_isql via SCRATCHBIRD_FB_ISQL, build FDW CLI wrappers in ScratchBird-driver, or use native isql-fb.
EOF
  exit 77
fi

if [[ ! -f "$LIST_FILE" ]]; then
  echo "Error: Firebird CTest list not found: $LIST_FILE" >&2
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
    exit 77
  fi
fi

failures=()

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

  if ! "${cmd[@]}" > "$log_file" 2>&1; then
    failures+=("$rel_path (see ${log_file})")
  fi
done < "$LIST_FILE"

if [[ ${#failures[@]} -ne 0 ]]; then
  echo "Firebird compatibility failures:" >&2
  for item in "${failures[@]}"; do
    echo "  - ${item}" >&2
  done
  exit 1
fi

echo "Firebird compatibility tests passed. Logs: ${RESULTS_DIR}"
