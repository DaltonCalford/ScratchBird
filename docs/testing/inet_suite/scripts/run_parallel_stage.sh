#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ $# -lt 1 ]; then
    echo "Usage: $0 <script_name> [runs]" >&2
    echo "Example: $0 run_native_worker.sh 8" >&2
    exit 1
fi

stage_script="$1"
shift || true
runs="${1:-${RUNS:-4}}"

if [[ "$stage_script" != /* ]]; then
    stage_script="$SCRIPT_DIR/$stage_script"
fi

if [ ! -x "$stage_script" ]; then
    echo "Error: script not executable: $stage_script" >&2
    exit 1
fi

pids=()
for i in $(seq 1 "$runs"); do
    if [[ "$stage_script" == *"worker"* ]]; then
        RUN_ID="$i" WORKER_ID="$i" "$stage_script" &
    else
        RUN_ID="$i" "$stage_script" &
    fi
    pids+=("$!")
done

for pid in "${pids[@]}"; do
    wait "$pid"
done
