#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "$0")/common.sh"

MY_ISQL=${MY_ISQL:-sb_my_isql}
MY_CLIENT=${MY_CLIENT:-}
MY_MYSQL=${MY_MYSQL:-mysql}
MY_HOST=${MY_HOST:-localhost}
MY_PORT=${MY_PORT:-3306}
MY_ADMIN_USER=${MY_ADMIN_USER:-admin}
MY_ADMIN_PASS=${MY_ADMIN_PASS:-admin_pw}
MY_ADMIN_DB=${MY_ADMIN_DB:-default}

if [ -z "$MY_CLIENT" ]; then
    if wire_client_enabled "mysql"; then
        MY_CLIENT="mysql"
    else
        MY_CLIENT="sb"
    fi
fi

if [ "$MY_CLIENT" = "mysql" ]; then
    run_with_output_stdin "mysql_create_db" "$SQL_DIR/mysql/00_create_db.sql" \
        "$MY_MYSQL" -h "$MY_HOST" -P "$MY_PORT" -u "$MY_ADMIN_USER" --password="$MY_ADMIN_PASS" -D "$MY_ADMIN_DB"
else
    run_with_output "mysql_create_db" \
        "$MY_ISQL" -h "$MY_HOST" -P "$MY_PORT" -u "$MY_ADMIN_USER" --password="$MY_ADMIN_PASS" -D "$MY_ADMIN_DB" -q \
        -f "$SQL_DIR/mysql/00_create_db.sql"
fi
