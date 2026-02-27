#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROTOCOL_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

OUTPUT_PATH="${1:-${PROTOCOL_ROOT}/GOLDEN_TRACE_INDEX.csv}"
mkdir -p "$(dirname "${OUTPUT_PATH}")"

{
  echo "path,bytes,sha256"
  while IFS= read -r trace_file; do
    rel_path="${trace_file#${PROTOCOL_ROOT}/}"
    byte_count="$(wc -c < "${trace_file}" | tr -d ' ')"
    sha256="$(sha256sum "${trace_file}" | awk '{print $1}')"
    echo "${rel_path},${byte_count},${sha256}"
  done < <(find "${PROTOCOL_ROOT}/fixtures" "${PROTOCOL_ROOT}/golden" -type f -name '*.trace' 2>/dev/null | sort)
} > "${OUTPUT_PATH}"

echo "Wrote ${OUTPUT_PATH}"
