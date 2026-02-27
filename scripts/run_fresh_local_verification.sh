#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
CLIWORK_ROOT="$(dirname "${PROJECT_ROOT}")"
SUITE_SCRIPT="${SCRIPT_DIR}/verification_bundle/suite/scripts/run_fresh_local_verification.sh"
WORKSPACE_ROOT="${SCRIPT_DIR}/verification_bundle/suite"
REPO_ROOT="${SB_VERIFY_REPO_ROOT:-${CLIWORK_ROOT}}"

if [[ ! -x "${SUITE_SCRIPT}" ]]; then
  echo "Missing verification runner: ${SUITE_SCRIPT}" >&2
  echo "Ensure scripts/verification_bundle is present." >&2
  exit 1
fi

exec "${SUITE_SCRIPT}" \
  --workspace-root "${WORKSPACE_ROOT}" \
  --repo-root "${REPO_ROOT}" \
  "$@"
