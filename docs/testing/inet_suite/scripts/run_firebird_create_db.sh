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
FB_DB=${FB_DB:-/var/lib/scratchbird/sb_grind_fb.sbdb}
FB_CONN=${FB_CONN:-${FB_HOST}/${FB_PORT}:${FB_DB}}

if [ -z "$FB_CLIENT" ]; then
    if wire_client_enabled "firebird"; then
        FB_CLIENT="firebird"
    else
        FB_CLIENT="sb"
    fi
fi

create_file="$(mktemp "$RESULTS_DIR/firebird_create_XXXX.sql")"
if [ "$FB_CLIENT" = "firebird" ] || [ "$FB_CLIENT" = "isql" ]; then
    FB_TARGET="$FB_CONN"
else
    FB_TARGET="$FB_DB"
fi
sed \
    -e "s|{{FB_TARGET}}|${FB_TARGET}|g" \
    -e "s|{{FB_USER}}|${FB_ADMIN_USER}|g" \
    -e "s|{{FB_PASS}}|${FB_ADMIN_PASS}|g" \
    "$SQL_DIR/firebird/00_create_db.sql" >"$create_file"

if [ "$FB_CLIENT" = "firebird" ] || [ "$FB_CLIENT" = "isql" ]; then
    run_with_output_noo "firebird_create_db" \
        "$FB_ISQL" -user "$FB_ADMIN_USER" -password "$FB_ADMIN_PASS" \
        -i "$create_file"
else
    run_with_output_noo "firebird_create_db" \
        "$FB_SB_ISQL" "$FB_DB" -f "$create_file"
fi

rm -f "$create_file"
