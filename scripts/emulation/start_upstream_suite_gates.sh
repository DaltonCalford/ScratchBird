#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
WORKSPACE_ROOT="${WORKSPACE_ROOT:-${REPO_ROOT}}"
ARTIFACT_ROOT="${SCRATCHBIRD_EMU_ARTIFACT_ROOT:-${REPO_ROOT}/tests/compatibility/results/emulation}"
MODE="${1:-dry-run}"
TIMEOUT_SEC="${2:-${GATE_COMMAND_TIMEOUT_SEC:-300}}"

echo "[upstream-gates] workspace root: ${WORKSPACE_ROOT}"
echo "[upstream-gates] artifact root: ${ARTIFACT_ROOT}"
echo "[upstream-gates] mode: ${MODE}"
echo "[upstream-gates] command timeout: ${TIMEOUT_SEC}s"

python3 "${REPO_ROOT}/scripts/emulation/start_upstream_suite_gates.py" \
  --workspace-root "${WORKSPACE_ROOT}" \
  --artifact-root "${ARTIFACT_ROOT}" \
  --mode "${MODE}" \
  --command-timeout-sec "${TIMEOUT_SEC}"

echo "[upstream-gates] generated:"
echo "  ${ARTIFACT_ROOT}/firebird/p5s2w2/fb-emu-040-firebird-qa.md"
echo "  ${ARTIFACT_ROOT}/firebird/p5s2w2/fb-emu-041-firebird-qa-report.md"
echo "  ${ARTIFACT_ROOT}/firebird/p5s2w2/fb-emu-041-fbtest-tcs.md"
echo "  ${ARTIFACT_ROOT}/mysql/p5s2w2/my-emu-040-mtr-gate.md"
echo "  ${ARTIFACT_ROOT}/mysql/p5s2w2/my-emu-041-mtr-report.md"
echo "  ${ARTIFACT_ROOT}/postgresql/p5s2w2/pg-emu-040-gate-integration.md"
echo "  ${ARTIFACT_ROOT}/postgresql/p5s2w2/pg-emu-041-regression-report.md"
