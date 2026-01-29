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

if [ -z "$MY_CLIENT" ]; then
    if wire_client_enabled "mysql"; then
        MY_CLIENT="mysql"
    else
        MY_CLIENT="sb"
    fi
fi

if [ "$MY_CLIENT" = "mysql" ]; then
    run_with_output_stdin "mysql_reports" "$SQL_DIR/mysql/40_reports.sql" \
        "$MY_MYSQL" -h "$MY_HOST" -P "$MY_PORT" -u "$MY_USER" --password="$MY_PASS" -D "$MY_DB"
else
    run_with_output "mysql_reports" \
        "$MY_ISQL" -h "$MY_HOST" -P "$MY_PORT" -u "$MY_USER" --password="$MY_PASS" -D "$MY_DB" -q \
        -f "$SQL_DIR/mysql/40_reports.sql"
fi
