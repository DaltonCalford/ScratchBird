#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "$0")/common.sh"

MY_ISQL=${MY_ISQL:-sb_my_isql}
MY_CLIENT=${MY_CLIENT:-}
MY_MYSQL=${MY_MYSQL:-mysql}
MY_HOST=${MY_HOST:-localhost}
MY_PORT=${MY_PORT:-3306}
MY_USER=${MY_USER:-sb_runner}
MY_PASS=${MY_PASS:-sb_runner_pw}
MY_DB=${MY_DB:-sb_grind_mysql}
ROWS_LARGE=${ROWS_LARGE:-1000000}

if [ -z "$MY_CLIENT" ]; then
    if wire_client_enabled "mysql"; then
        MY_CLIENT="mysql"
    else
        MY_CLIENT="sb"
    fi
fi

load_file="$(mktemp "$RESULTS_DIR/mysql_load_XXXX.sql")"
cat >"$load_file" <<LOADSQL
CALL sb_seed_customers(${ROWS_LARGE});
CALL sb_seed_orders(${ROWS_LARGE});
CALL sb_seed_order_items(${ROWS_LARGE});
CALL sb_seed_events(${ROWS_LARGE});
CALL sb_seed_metrics(${ROWS_LARGE});
LOADSQL

if [ "$MY_CLIENT" = "mysql" ]; then
    run_with_output_stdin "mysql_load" "$load_file" \
        "$MY_MYSQL" -h "$MY_HOST" -P "$MY_PORT" -u "$MY_USER" --password="$MY_PASS" -D "$MY_DB"
else
    run_with_output "mysql_load" \
        "$MY_ISQL" -h "$MY_HOST" -P "$MY_PORT" -u "$MY_USER" --password="$MY_PASS" -D "$MY_DB" -q \
        -f "$load_file"
fi

rm -f "$load_file"
