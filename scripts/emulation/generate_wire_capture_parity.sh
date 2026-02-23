#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
ARTIFACT_ROOT="${SCRATCHBIRD_EMU_ARTIFACT_ROOT:-${REPO_ROOT}/tests/compatibility/results/emulation}"
MODE="${1:-live}"
WORKSPACE_ROOT="${WORKSPACE_ROOT:-${REPO_ROOT}}"
NATIVE_MYSQL="${NATIVE_MYSQL_ENDPOINT:-127.0.0.1:3306}"
NATIVE_POSTGRESQL="${NATIVE_POSTGRESQL_ENDPOINT:-127.0.0.1:5432}"
NATIVE_FIREBIRD="${NATIVE_FIREBIRD_ENDPOINT:-127.0.0.1:3050}"
EMULATED_MYSQL="${EMULATED_MYSQL_ENDPOINT:-127.0.0.1:13306}"
EMULATED_POSTGRESQL="${EMULATED_POSTGRESQL_ENDPOINT:-127.0.0.1:15432}"
EMULATED_FIREBIRD="${EMULATED_FIREBIRD_ENDPOINT:-127.0.0.1:13050}"
TIMEOUT_SEC="${WIRE_CAPTURE_TIMEOUT_SEC:-5}"

echo "[wire-capture] writing artifacts to ${ARTIFACT_ROOT}"
echo "[wire-capture] mode: ${MODE}"
python3 "${REPO_ROOT}/scripts/emulation/generate_wire_capture_parity.py" \
  --artifact-root "${ARTIFACT_ROOT}" \
  --mode "${MODE}" \
  --workspace-root "${WORKSPACE_ROOT}" \
  --native-mysql "${NATIVE_MYSQL}" \
  --native-postgresql "${NATIVE_POSTGRESQL}" \
  --native-firebird "${NATIVE_FIREBIRD}" \
  --emulated-mysql "${EMULATED_MYSQL}" \
  --emulated-postgresql "${EMULATED_POSTGRESQL}" \
  --emulated-firebird "${EMULATED_FIREBIRD}" \
  --timeout-sec "${TIMEOUT_SEC}" \
  --start-emulated-server

echo "[wire-capture] generated:"
echo "  ${ARTIFACT_ROOT}/firebird/p5s1w2/fb-emu-010-wire-captures/"
echo "  ${ARTIFACT_ROOT}/mysql/p5s1w2/my-emu-010-wire-captures/"
echo "  ${ARTIFACT_ROOT}/postgresql/p5s1w2/pg-emu-010-wire-captures/"
echo "  ${ARTIFACT_ROOT}/live-capture-availability-2026-02-22.md"
