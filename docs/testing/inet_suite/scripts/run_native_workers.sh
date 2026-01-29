#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKERS=${WORKERS:-4}

pids=()
for i in $(seq 1 "$WORKERS"); do
    WORKER_ID="$i" "$SCRIPT_DIR/run_native_worker.sh" &
    pids+=("$!")
done

for pid in "${pids[@]}"; do
    wait "$pid"
done
