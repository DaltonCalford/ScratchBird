#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "$0")/common.sh"

PG_ISQL=${PG_ISQL:-sb_pg_isql}
PG_CLIENT=${PG_CLIENT:-}
PG_PSQL=${PG_PSQL:-psql}
PG_HOST=${PG_HOST:-localhost}
PG_PORT=${PG_PORT:-5432}
PG_USER=${PG_USER:-sb_runner}
PG_PASS=${PG_PASS:-sb_runner_pw}
PG_DB=${PG_DB:-sb_grind_pg}
ROWS_LARGE=${ROWS_LARGE:-1000000}

if [ -z "$PG_CLIENT" ]; then
    if wire_client_enabled "psql"; then
        PG_CLIENT="psql"
    else
        PG_CLIENT="sb"
    fi
fi

load_file="$(mktemp "$RESULTS_DIR/pg_load_XXXX.sql")"
cat >"$load_file" <<LOADSQL
CALL sb_seed_customers(${ROWS_LARGE});
CALL sb_seed_orders(${ROWS_LARGE});
CALL sb_seed_order_items(${ROWS_LARGE});
CALL sb_seed_events(${ROWS_LARGE});
CALL sb_seed_metrics(${ROWS_LARGE});
LOADSQL

if [ "$PG_CLIENT" = "psql" ]; then
    run_with_output "pg_load" \
        env PGPASSWORD="$PG_PASS" "$PG_PSQL" -h "$PG_HOST" -p "$PG_PORT" -U "$PG_USER" -d "$PG_DB" -q -X \
        -v ON_ERROR_STOP=1 -f "$load_file"
else
    run_with_output "pg_load" \
        env PGPASSWORD="$PG_PASS" "$PG_ISQL" -h "$PG_HOST" -p "$PG_PORT" -U "$PG_USER" -d "$PG_DB" -q \
        -f "$load_file"
fi

rm -f "$load_file"
