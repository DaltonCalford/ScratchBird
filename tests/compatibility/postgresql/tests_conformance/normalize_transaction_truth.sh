#!/usr/bin/env bash
set -euo pipefail
INPUT="${1:?input file required}"
rg "ROW_RESULT\|" "${INPUT}" | sed -E 's/^.*(ROW_RESULT\|.*)$/\1/' | sed -E 's/[[:space:]]+\|?$//'
