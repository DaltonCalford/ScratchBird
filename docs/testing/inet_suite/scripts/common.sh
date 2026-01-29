#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
SQL_DIR="$ROOT_DIR/sql"
RESULTS_DIR="$ROOT_DIR/results"

mkdir -p "$RESULTS_DIR"

wire_client_enabled() {
    local needle="$1"
    local list="${WIRE_CLIENTS:-}"

    if [ -z "$list" ]; then
        return 1
    fi

    list="${list,,}"
    list="${list// /}"
    IFS=',' read -r -a items <<< "$list"
    for item in "${items[@]}"; do
        if [ "$item" = "$needle" ]; then
            return 0
        fi
    done
    return 1
}

timestamp() {
    date +"%Y%m%d_%H%M%S_%N"
}

run_with_output() {
    local label="$1"
    shift
    if [ -n "${RUN_ID:-}" ]; then
        label="${label}_${RUN_ID}"
    fi

    local ts
    ts="$(timestamp)"
    local data_file="$RESULTS_DIR/${label}_${ts}.out"
    local log_file="$RESULTS_DIR/${label}_${ts}.log"
    local err_file="$RESULTS_DIR/${label}_${ts}.err"

    echo "[run] ${label}"
    "$@" -o "$data_file" >"$log_file" 2>"$err_file"
    echo "[out] ${data_file}"
    echo "[log] ${log_file}"
    echo "[err] ${err_file}"
}

run_with_output_noo() {
    local label="$1"
    shift
    if [ -n "${RUN_ID:-}" ]; then
        label="${label}_${RUN_ID}"
    fi

    local ts
    ts="$(timestamp)"
    local data_file="$RESULTS_DIR/${label}_${ts}.out"
    local log_file="$RESULTS_DIR/${label}_${ts}.log"
    local err_file="$RESULTS_DIR/${label}_${ts}.err"

    echo "[run] ${label}"
    {
        echo "command: $*"
    } >"$log_file"
    "$@" >"$data_file" 2>"$err_file"
    echo "[out] ${data_file}"
    echo "[log] ${log_file}"
    echo "[err] ${err_file}"
}

run_with_output_stdin() {
    local label="$1"
    local input_file="$2"
    shift 2
    if [ -n "${RUN_ID:-}" ]; then
        label="${label}_${RUN_ID}"
    fi

    local ts
    ts="$(timestamp)"
    local data_file="$RESULTS_DIR/${label}_${ts}.out"
    local log_file="$RESULTS_DIR/${label}_${ts}.log"
    local err_file="$RESULTS_DIR/${label}_${ts}.err"

    echo "[run] ${label}"
    {
        echo "command: $*"
        echo "input: ${input_file}"
    } >"$log_file"
    "$@" <"$input_file" >"$data_file" 2>"$err_file"
    echo "[out] ${data_file}"
    echo "[log] ${log_file}"
    echo "[err] ${err_file}"
}
