#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "$0")/common.sh"

FB_CLIENT=${FB_CLIENT:-}
FB_ISQL=${FB_ISQL:-isql}
FB_SB_ISQL=${FB_SB_ISQL:-sb_fb_isql}
FB_HOST=${FB_HOST:-localhost}
FB_PORT=${FB_PORT:-3050}
FB_ADMIN_USER=${FB_ADMIN_USER:-SYSDBA}
FB_ADMIN_PASS=${FB_ADMIN_PASS:-masterkey}
FB_DB=${FB_DB:-/var/lib/firebird/data/sb_grind_fb.fdb}
FB_CONN=${FB_CONN:-${FB_HOST}/${FB_PORT}:${FB_DB}}

if [ -z "$FB_CLIENT" ]; then
    if wire_client_enabled "firebird"; then
        FB_CLIENT="firebird"
    else
        FB_CLIENT="sb"
    fi
fi

if [ "$FB_CLIENT" = "firebird" ] || [ "$FB_CLIENT" = "isql" ]; then
    run_with_output_noo "firebird_reports" \
        "$FB_ISQL" -user "$FB_ADMIN_USER" -password "$FB_ADMIN_PASS" "$FB_CONN" \
        -i "$SQL_DIR/firebird/40_reports.sql"
else
    run_with_output_noo "firebird_reports" \
        "$FB_SB_ISQL" "$FB_DB" -f "$SQL_DIR/firebird/40_reports.sql"
fi
