#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "$0")/common.sh"

SB_ISQL=${SB_ISQL:-sb_isql}
SB_HOST=${SB_HOST:-localhost}
SB_PORT=${SB_PORT:-3092}
SB_ADMIN_USER=${SB_ADMIN_USER:-admin}
SB_ADMIN_PASS=${SB_ADMIN_PASS:-admin_pw}
SB_DB=${SB_DB:-sb_grind_native}

run_with_output "native_schema" \
    "$SB_ISQL" -H "$SB_HOST" -p "$SB_PORT" -U "$SB_ADMIN_USER" -P "$SB_ADMIN_PASS" -b \
    -f "$SQL_DIR/native/10_schema.sql" \
    "$SB_DB"

run_with_output "native_procs" \
    "$SB_ISQL" -H "$SB_HOST" -p "$SB_PORT" -U "$SB_ADMIN_USER" -P "$SB_ADMIN_PASS" -b \
    -f "$SQL_DIR/native/20_procs.sql" \
    "$SB_DB"
