#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "Usage: $0 [--repo <scratchbird_repo_root>]" >&2
}

repo_root=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --repo)
            if [[ $# -lt 2 ]]; then
                usage
                exit 2
            fi
            repo_root="$2"
            shift 2
            ;;
        *)
            usage
            exit 2
            ;;
    esac
done

if [[ -z "$repo_root" ]]; then
    script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    repo_root="$(cd "${script_dir}/../.." && pwd)"
else
    repo_root="$(cd "$repo_root" && pwd)"
fi

header_targets=(
    "include/scratchbird/server/daemon.h"
    "include/scratchbird/server/scratchbird_server.h"
    "include/scratchbird/server/service_controller.h"
)

signal_targets=(
    "src/server/scratchbird_server.cpp"
    "src/server/service_controller.cpp"
    "src/network/sb_listener_main.cpp"
)

declare -a violations=()

record_matches() {
    local file="$1"
    local pattern="$2"
    local rule="$3"
    local abs_path="${repo_root}/${file}"

    if [[ ! -f "$abs_path" ]]; then
        return
    fi

    local lines
    lines="$(rg -n --pcre2 "$pattern" "$abs_path" || true)"
    if [[ -z "$lines" ]]; then
        return
    fi

    while IFS= read -r line; do
        [[ -z "$line" ]] && continue
        violations+=("${file}|${rule}|${line}")
    done <<< "$lines"
}

for file in "${header_targets[@]}"; do
    record_matches \
        "$file" \
        '#include <(unistd\.h|signal\.h|sys/[^>]+|pwd\.h|grp\.h|fcntl\.h|poll\.h)>' \
        'forbidden_posix_include_in_portable_header'
    record_matches \
        "$file" \
        '\b(pid_t|uid_t|gid_t|mode_t)\b' \
        'forbidden_posix_type_in_portable_header'
done

for file in "${signal_targets[@]}"; do
    record_matches \
        "$file" \
        '\b(std::signal|::signal)\s*\(' \
        'forbidden_direct_signal_registration'
done

echo "# Portable Runtime Guard Report"
echo
echo "repo_root: ${repo_root}"
echo "header_targets: ${#header_targets[@]}"
echo "signal_targets: ${#signal_targets[@]}"
echo

if [[ ${#violations[@]} -eq 0 ]]; then
    echo "result: PASS"
    exit 0
fi

echo "result: FAIL"
echo "violation_count: ${#violations[@]}"
echo
echo "file|rule|match"
for row in "${violations[@]}"; do
    echo "$row"
done

exit 3
