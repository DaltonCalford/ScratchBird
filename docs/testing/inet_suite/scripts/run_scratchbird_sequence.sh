#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# ScratchBird-only sequence: native first, then emulated dialects.
unset WIRE_CLIENTS PG_CLIENT MY_CLIENT FB_CLIENT

"$SCRIPT_DIR/run_native_bootstrap.sh"
"$SCRIPT_DIR/run_native_schema.sh"
"$SCRIPT_DIR/run_native_load.sh"
"$SCRIPT_DIR/run_native_reports.sh"

"$SCRIPT_DIR/run_postgresql_create_db.sh"
"$SCRIPT_DIR/run_postgresql_schema.sh"
"$SCRIPT_DIR/run_postgresql_load.sh"

"$SCRIPT_DIR/run_mysql_create_db.sh"
"$SCRIPT_DIR/run_mysql_schema.sh"
"$SCRIPT_DIR/run_mysql_load.sh"

"$SCRIPT_DIR/run_firebird_create_db.sh"
"$SCRIPT_DIR/run_firebird_schema.sh"
"$SCRIPT_DIR/run_firebird_load.sh"

# Concurrency after all datasets exist
"$SCRIPT_DIR/run_parallel_workers.sh"

"$SCRIPT_DIR/run_postgresql_reports.sh"
"$SCRIPT_DIR/run_mysql_reports.sh"
"$SCRIPT_DIR/run_firebird_reports.sh"
