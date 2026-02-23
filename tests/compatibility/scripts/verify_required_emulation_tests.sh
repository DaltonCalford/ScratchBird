#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"

echo "[verify] repo root: ${REPO_ROOT}"
echo "[verify] running emulation gate reports from in-tree suites"
"${REPO_ROOT}/scripts/emulation/start_upstream_suite_gates.sh" execute

echo "[verify] running wire-capture parity from in-tree defaults"
"${REPO_ROOT}/scripts/emulation/generate_wire_capture_parity.sh" live

echo "[verify] required evidence generated under:"
echo "  ${REPO_ROOT}/tests/compatibility/results/emulation"
