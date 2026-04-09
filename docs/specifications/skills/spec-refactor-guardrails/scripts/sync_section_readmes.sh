#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASE_DIR="${1:-$(cd "${SCRIPT_DIR}/../../.." && pwd)}"

if [[ ! -d "${BASE_DIR}" ]]; then
  echo "Base directory not found: ${BASE_DIR}" >&2
  exit 1
fi

if [[ ! -f "${BASE_DIR}/README.md" ]]; then
  echo "Expected specifications root README in: ${BASE_DIR}" >&2
  exit 1
fi

for dir in "${BASE_DIR}"/*; do
  [[ -d "${dir}" ]] || continue
  section_name="$(basename "${dir}")"

  if [[ ! "${section_name}" =~ ^[0-9][0-9]_ ]]; then
    continue
  fi

  readme="${dir}/README.md"
  tmp_list="$(mktemp)"

  found_files=0
  while IFS= read -r rel; do
    found_files=1
    if [[ "${rel}" == *.md ]]; then
      printf -- '- [%s](%s)\n' "${rel}" "${rel}" >> "${tmp_list}"
    else
      printf -- '- `%s`\n' "${rel}" >> "${tmp_list}"
    fi
  done < <(find "${dir}" -mindepth 1 -type f ! -name 'README.md' | sed "s|^${dir}/||" | sort)

  if [[ ${found_files} -eq 0 ]]; then
    printf -- '- (no spec files yet)\n' > "${tmp_list}"
  fi

  if [[ ! -f "${readme}" ]]; then
    cat > "${readme}" <<DOC
# ${section_name}

## Purpose
Canonical specification area for ${section_name}.

## Status
Draft - not yet approved as authoritative.

## Links
- Back to root index: [../README.md](../README.md)

## File Index
<!-- AUTO-GENERATED:FILE-LIST:START -->
- (no spec files yet)
<!-- AUTO-GENERATED:FILE-LIST:END -->

## Maintenance
- Update file list with `../skills/spec-refactor-guardrails/scripts/sync_section_readmes.sh`.
DOC
  fi

  if ! rg -q '^<!-- AUTO-GENERATED:FILE-LIST:START -->$' "${readme}" || \
     ! rg -q '^<!-- AUTO-GENERATED:FILE-LIST:END -->$' "${readme}"; then
    {
      printf '\n## File Index\n'
      printf '<!-- AUTO-GENERATED:FILE-LIST:START -->\n'
      cat "${tmp_list}"
      printf '<!-- AUTO-GENERATED:FILE-LIST:END -->\n\n'
      printf '## Maintenance\n'
      printf -- '- Update file list with `../skills/spec-refactor-guardrails/scripts/sync_section_readmes.sh`.\n'
    } >> "${readme}"
  else
    tmp_out="$(mktemp)"
    awk -v list_file="${tmp_list}" '
      BEGIN {
        while ((getline line < list_file) > 0) {
          lines[++n] = line
        }
        close(list_file)
      }
      {
        if ($0 == "<!-- AUTO-GENERATED:FILE-LIST:START -->") {
          print
          for (i = 1; i <= n; i++) {
            print lines[i]
          }
          in_block = 1
          next
        }
        if (in_block && $0 == "<!-- AUTO-GENERATED:FILE-LIST:END -->") {
          print
          in_block = 0
          next
        }
        if (!in_block) {
          print
        }
      }
    ' "${readme}" > "${tmp_out}"
    mv "${tmp_out}" "${readme}"
  fi

  rm -f "${tmp_list}"
done

echo "Section README file indexes synced under: ${BASE_DIR}"
