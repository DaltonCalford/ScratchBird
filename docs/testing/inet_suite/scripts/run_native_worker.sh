#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "$0")/common.sh"

SB_ISQL=${SB_ISQL:-sb_isql}
SB_HOST=${SB_HOST:-localhost}
SB_PORT=${SB_PORT:-3092}
SB_USER=${SB_USER:-sb_runner}
SB_PASS=${SB_PASS:-sb_runner_pw}
SB_DB=${SB_DB:-sb_grind_native}
WORKER_ID=${WORKER_ID:-1}

run_with_output "native_worker_${WORKER_ID}" \
    "$SB_ISQL" -H "$SB_HOST" -p "$SB_PORT" -U "$SB_USER" -P "$SB_PASS" -b \
    -f "$SQL_DIR/native/50_worker.sql" \
    "$SB_DB"
