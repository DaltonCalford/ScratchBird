#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "$0")/common.sh"

FB_CLIENT=${FB_CLIENT:-}
FB_ISQL=${FB_ISQL:-isql}
FB_SB_ISQL=${FB_SB_ISQL:-sb_fb_isql}
FB_HOST=${FB_HOST:-localhost}
FB_PORT=${FB_PORT:-3050}
FB_USER=${FB_USER:-SYSDBA}
FB_PASS=${FB_PASS:-masterkey}
FB_DB=${FB_DB:-/var/lib/firebird/data/sb_grind_fb.fdb}
FB_CONN=${FB_CONN:-${FB_HOST}/${FB_PORT}:${FB_DB}}
WORKER_ID=${WORKER_ID:-1}

if [ -z "$FB_CLIENT" ]; then
    if wire_client_enabled "firebird"; then
        FB_CLIENT="firebird"
    else
        FB_CLIENT="sb"
    fi
fi

if [ "$FB_CLIENT" = "firebird" ] || [ "$FB_CLIENT" = "isql" ]; then
    run_with_output_noo "firebird_worker_${WORKER_ID}" \
        "$FB_ISQL" -user "$FB_USER" -password "$FB_PASS" "$FB_CONN" \
        -i "$SQL_DIR/firebird/50_worker.sql"
else
    run_with_output_noo "firebird_worker_${WORKER_ID}" \
        "$FB_SB_ISQL" "$FB_DB" -f "$SQL_DIR/firebird/50_worker.sql"
fi
