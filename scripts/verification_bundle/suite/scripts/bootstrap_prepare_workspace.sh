#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
REPO_ROOT="${SB_VERIFY_REPO_ROOT:-${WORKSPACE_ROOT}/repos}"

mkdir -p "${REPO_ROOT}"
mkdir -p "${WORKSPACE_ROOT}/results"
mkdir -p "${WORKSPACE_ROOT}/reports"

echo "Prepared verification workspace:"
echo "  workspace: ${WORKSPACE_ROOT}"
echo "  repos:     ${REPO_ROOT}"
echo "  results:   ${WORKSPACE_ROOT}/results"
echo "  reports:   ${WORKSPACE_ROOT}/reports"

