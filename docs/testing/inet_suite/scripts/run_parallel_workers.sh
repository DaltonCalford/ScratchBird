#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DIALECTS=${DIALECTS:-native,postgresql,mysql,firebird}

pids=()
IFS=',' read -r -a dialect_list <<< "$DIALECTS"
for dialect in "${dialect_list[@]}"; do
    case "$dialect" in
        native)
            "$SCRIPT_DIR/run_native_workers.sh" &
            pids+=("$!")
            ;;
        postgresql|pg)
            "$SCRIPT_DIR/run_postgresql_workers.sh" &
            pids+=("$!")
            ;;
        mysql)
            "$SCRIPT_DIR/run_mysql_workers.sh" &
            pids+=("$!")
            ;;
        firebird|fb)
            "$SCRIPT_DIR/run_firebird_workers.sh" &
            pids+=("$!")
            ;;
        *)
            echo "[warn] unknown dialect: $dialect" >&2
            ;;
    esac
done

for pid in "${pids[@]}"; do
    wait "$pid"
done
