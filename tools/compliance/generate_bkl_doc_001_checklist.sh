#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

LANGUAGE_GUIDE_DIR="${1:-${REPO_ROOT}/docs/user-documentation/language-guide}"
BACKLOG_DOC="${2:-${REPO_ROOT}/docs/planning/BETA_0_2_0_SPEC_BACKLOG_2026-02-19.md}"
CYCLE_SCOPE_TSV="${3:-${REPO_ROOT}/docs/planning/BETA_0_2_0_DOC_CYCLE_SCOPE.tsv}"

if [[ ! -d "${LANGUAGE_GUIDE_DIR}" ]]; then
    echo "error: language guide directory not found: ${LANGUAGE_GUIDE_DIR}" >&2
    exit 2
fi
if [[ ! -f "${BACKLOG_DOC}" ]]; then
    echo "error: backlog document not found: ${BACKLOG_DOC}" >&2
    exit 2
fi

declare -A CYCLE_TOUCHED_DIRS

normalize_dir() {
    local dir="${1:-}"
    dir="${dir#./}"
    dir="${dir%/}/"
    printf "%s" "${dir}"
}

to_repo_path() {
    local value="${1:-}"
    if [[ "${value}" == "${REPO_ROOT}/"* ]]; then
        printf "%s" "${value#${REPO_ROOT}/}"
    else
        printf "%s" "${value}"
    fi
}

if [[ -f "${CYCLE_SCOPE_TSV}" ]]; then
    while IFS=$'\t' read -r dir _; do
        [[ -z "${dir}" ]] && continue
        [[ "${dir}" == \#* ]] && continue
        dir="$(normalize_dir "${dir}")"
        CYCLE_TOUCHED_DIRS["${dir}"]=1
    done < "${CYCLE_SCOPE_TSV}"
fi

tmp_block="$(mktemp)"
tmp_out="$(mktemp)"
cleanup() {
    rm -f "${tmp_block}" "${tmp_out}"
}
trap cleanup EXIT

join_files() {
    local dir="${1}"
    local first=1
    local out=""
    while IFS= read -r file; do
        if [[ -z "${file}" ]]; then
            continue
        fi
        if [[ ${first} -eq 1 ]]; then
            out="${file}"
            first=0
        else
            out="${out}, ${file}"
        fi
    done < <(find "${dir}" -maxdepth 1 -type f -name '*.md' -printf '%f\n' | LC_ALL=C sort | rg -v '^README\.md$' || true)

    if [[ -z "${out}" ]]; then
        printf "%s" "-"
    else
        printf "%s" "${out}"
    fi
}

is_cycle_touched() {
    local dir="${1:-}"
    local probe
    probe="$(normalize_dir "${dir}")"

    while true; do
        if [[ -n "${CYCLE_TOUCHED_DIRS[${probe}]:-}" ]]; then
            printf "%s" "YES"
            return
        fi
        if [[ "${probe}" == "docs/user-documentation/language-guide/" ]]; then
            break
        fi
        probe="${probe%/}"
        probe="${probe%/*}/"
    done

    printf "%s" "NO"
}

{
    echo "### BKL-DOC-001 (OPEN)"
    echo
    echo "- Title: full language-reference refresh for all changed v3 surface"
    echo "- Scope:"
    echo "  - \`docs/user-documentation/language-guide/\` full tree"
    echo "  - all command/object docs touched by parser/emitter/executor changes"
    echo "- Per-directory command/doc closure checklist (all folders under language-guide):"
    echo
    echo "<!-- AUTO-GENERATED:BKL-DOC-001-CHECKLIST:START -->"
    echo "Regeneration command:"
    echo "\`tools/compliance/generate_bkl_doc_001_checklist.sh\`"
    echo "Cycle scope source:"
    echo "\`$(to_repo_path "${CYCLE_SCOPE_TSV}")\`"
    echo "Cycle scope semantics: if a parent directory is listed in the scope file, all descendants are marked \`YES\`."
    echo
    echo "| Done | Parser/Emitter/Executor touched in this cycle | Directory | Command/doc files to close | README |"
    echo "| --- | --- | --- | --- | --- |"
    while IFS= read -r dir; do
        rel="${dir#${LANGUAGE_GUIDE_DIR}}"
        if [[ -z "${rel}" ]]; then
            display_dir="docs/user-documentation/language-guide/"
        else
            display_dir="docs/user-documentation/language-guide${rel}/"
        fi
        cycle_touch="$(is_cycle_touched "${display_dir}")"
        files="$(join_files "${dir}")"
        if [[ -f "${dir}/README.md" ]]; then
            readme="README.md"
        else
            readme="-"
        fi
        printf "| [ ] | \`%s\` | \`%s\` | \`%s\` | \`%s\` |\n" "${cycle_touch}" "${display_dir}" "${files}" "${readme}"
    done < <(find "${LANGUAGE_GUIDE_DIR}" -type d | LC_ALL=C sort)
    echo "<!-- AUTO-GENERATED:BKL-DOC-001-CHECKLIST:END -->"
    echo
    echo "- Acceptance:"
    echo "  - per-directory checklist rows are updated as command/doc closure progresses"
    echo "  - no changed SQL object/command is missing a corresponding language-guide update"
    echo "  - each updated file contains current \`Last modified\` metadata"
} > "${tmp_block}"

awk -v block_file="${tmp_block}" '
BEGIN {
    while ((getline line < block_file) > 0) {
        block = block line ORS
    }
    close(block_file)
}
$0 ~ /^### BKL-DOC-001 \(OPEN\)$/ {
    if (length(block) == 0) {
        print "error: generated block is empty" > "/dev/stderr"
        exit 3
    }
    print block
    in_old = 1
    next
}
in_old && $0 ~ /^### BKL-DOC-002 \(OPEN\)$/ {
    in_old = 0
    print
    next
}
!in_old {
    print
}
' "${BACKLOG_DOC}" > "${tmp_out}"

mv "${tmp_out}" "${BACKLOG_DOC}"

echo "updated ${BACKLOG_DOC}" >&2
