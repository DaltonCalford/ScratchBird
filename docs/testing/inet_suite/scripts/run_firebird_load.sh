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
ROWS_LARGE=${ROWS_LARGE:-1000000}

if [ -z "$FB_CLIENT" ]; then
    if wire_client_enabled "firebird"; then
        FB_CLIENT="firebird"
    else
        FB_CLIENT="sb"
    fi
fi

load_file="$(mktemp "$RESULTS_DIR/firebird_load_XXXX.sql")"
cat >"$load_file" <<LOADSQL
EXECUTE PROCEDURE sb_seed_customers(${ROWS_LARGE});
EXECUTE PROCEDURE sb_seed_orders(${ROWS_LARGE});
EXECUTE PROCEDURE sb_seed_order_items(${ROWS_LARGE});
EXECUTE PROCEDURE sb_seed_events(${ROWS_LARGE});
LOADSQL

if [ "$FB_CLIENT" = "firebird" ] || [ "$FB_CLIENT" = "isql" ]; then
    run_with_output_noo "firebird_load" \
        "$FB_ISQL" -user "$FB_ADMIN_USER" -password "$FB_ADMIN_PASS" "$FB_CONN" \
        -i "$load_file"
else
    run_with_output_noo "firebird_load" \
        "$FB_SB_ISQL" "$FB_DB" -f "$load_file"
fi

rm -f "$load_file"
