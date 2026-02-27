#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TARGET="${SCRIPT_DIR}/verification_bundle/SB_Dev_Bootstrap.sh"

if [[ ! -x "${TARGET}" ]]; then
  echo "Missing bootstrap script: ${TARGET}" >&2
  exit 1
fi

exec "${TARGET}" "$@"
