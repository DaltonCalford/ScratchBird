#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "$0")/common.sh"

PG_ISQL=${PG_ISQL:-sb_pg_isql}
PG_CLIENT=${PG_CLIENT:-}
PG_PSQL=${PG_PSQL:-psql}
PG_HOST=${PG_HOST:-localhost}
PG_PORT=${PG_PORT:-5432}
PG_ADMIN_USER=${PG_ADMIN_USER:-admin}
PG_ADMIN_PASS=${PG_ADMIN_PASS:-admin_pw}
PG_DB=${PG_DB:-sb_grind_pg}

if [ -z "$PG_CLIENT" ]; then
    if wire_client_enabled "psql"; then
        PG_CLIENT="psql"
    else
        PG_CLIENT="sb"
    fi
fi

if [ "$PG_CLIENT" = "psql" ]; then
    run_with_output "pg_schema" \
        env PGPASSWORD="$PG_ADMIN_PASS" "$PG_PSQL" -h "$PG_HOST" -p "$PG_PORT" -U "$PG_ADMIN_USER" -d "$PG_DB" -q -X \
        -v ON_ERROR_STOP=1 -f "$SQL_DIR/postgresql/10_schema.sql"

    run_with_output "pg_procs" \
        env PGPASSWORD="$PG_ADMIN_PASS" "$PG_PSQL" -h "$PG_HOST" -p "$PG_PORT" -U "$PG_ADMIN_USER" -d "$PG_DB" -q -X \
        -v ON_ERROR_STOP=1 -f "$SQL_DIR/postgresql/20_procs.sql"
else
    run_with_output "pg_schema" \
        env PGPASSWORD="$PG_ADMIN_PASS" "$PG_ISQL" -h "$PG_HOST" -p "$PG_PORT" -U "$PG_ADMIN_USER" -d "$PG_DB" -q \
        -f "$SQL_DIR/postgresql/10_schema.sql"

    run_with_output "pg_procs" \
        env PGPASSWORD="$PG_ADMIN_PASS" "$PG_ISQL" -h "$PG_HOST" -p "$PG_PORT" -U "$PG_ADMIN_USER" -d "$PG_DB" -q \
        -f "$SQL_DIR/postgresql/20_procs.sql"
fi
