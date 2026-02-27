#!/usr/bin/env bash
set -euo pipefail
INPUT="${1:?input file required}"
rg "SEC_RESULT\|" "${INPUT}" | sed -E 's/^.*(SEC_RESULT\|.*)$/\1/' | sed -E 's/[[:space:]]+\|?$//'
