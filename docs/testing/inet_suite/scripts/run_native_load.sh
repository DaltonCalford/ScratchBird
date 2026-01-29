#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "$0")/common.sh"

SB_ISQL=${SB_ISQL:-sb_isql}
SB_HOST=${SB_HOST:-localhost}
SB_PORT=${SB_PORT:-3092}
SB_ADMIN_USER=${SB_ADMIN_USER:-admin}
SB_ADMIN_PASS=${SB_ADMIN_PASS:-admin_pw}
SB_DB=${SB_DB:-sb_grind_native}
ROWS_LARGE=${ROWS_LARGE:-1000000}

load_file="$(mktemp "$RESULTS_DIR/native_load_XXXX.sql")"
cat >"$load_file" <<LOADSQL
CALL sb_seed_customers(${ROWS_LARGE});
CALL sb_seed_orders(${ROWS_LARGE});
CALL sb_seed_order_items(${ROWS_LARGE});
CALL sb_seed_events(${ROWS_LARGE});
CALL sb_seed_metrics(${ROWS_LARGE});
LOADSQL

run_with_output "native_load" \
    "$SB_ISQL" -H "$SB_HOST" -p "$SB_PORT" -U "$SB_ADMIN_USER" -P "$SB_ADMIN_PASS" -b \
    -f "$load_file" \
    "$SB_DB"

rm -f "$load_file"
