#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage:
  vnext_scope_scan.sh [--repo <path>] [--output <file>] [--allowlist <file>] [--show-all]

Scans changed files in a git repository for out-of-scope paths for the
ENGINE_SBLR_VNEXT_WORKTREE constraints:
  - no listener implementation work
  - no emulated parser implementation work
  - no production UDR implementation work

The scan reads changed files from: git status --porcelain
EOF
}

repo="."
output=""
allowlist=""
show_all=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --repo)
            repo="$2"
            shift 2
            ;;
        --output)
            output="$2"
            shift 2
            ;;
        --allowlist)
            allowlist="$2"
            shift 2
            ;;
        --show-all)
            show_all=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown argument: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

if ! git -C "$repo" rev-parse --git-dir >/dev/null 2>&1; then
    echo "Not a git repository: $repo" >&2
    exit 2
fi

tmp="$(mktemp)"
cleanup() { rm -f "$tmp"; }
trap cleanup EXIT

git -C "$repo" status --porcelain \
    | awk '{print $2}' \
    | sed 's#^\./##' \
    | sort -u > "$tmp"

forbidden_re='^(src/network/sb_listener_main\.cpp|src/parser/firebird/|src/parser/mysql/|src/parser/postgresql/|include/scratchbird/parser/firebird/|include/scratchbird/parser/mysql/|include/scratchbird/parser/postgresql/|src/udr/(firebird|mysql|postgresql|mariadb|redis|mongodb|mongo|cassandra|neo4j|milvus|opensearch|clickhouse|duckdb|influxdb)_udr\.cpp|include/scratchbird/udr/(firebird|mysql|postgresql|mariadb|redis|mongodb|mongo|cassandra|neo4j|milvus|opensearch|clickhouse|duckdb|influxdb)_udr\.h(pp)?)'

violations="$(grep -E "$forbidden_re" "$tmp" || true)"

if [[ -n "$allowlist" && -f "$allowlist" && -n "$violations" ]]; then
    violations="$(printf '%s\n' "$violations" | grep -v -F -x -f "$allowlist" || true)"
fi

render() {
    echo "# vNext Scope Scan"
    echo
    echo "repo: $repo"
    echo "scan_source: git status --porcelain"
    echo "timestamp_utc: $(date -u +"%Y-%m-%dT%H:%M:%SZ")"
    echo
    if [[ $show_all -eq 1 ]]; then
        echo "## Changed Files"
        cat "$tmp"
        echo
    fi
    echo "## Violations"
    if [[ -n "$violations" ]]; then
        printf '%s\n' "$violations"
    else
        echo "(none)"
    fi
}

if [[ -n "$output" ]]; then
    mkdir -p "$(dirname "$output")"
    render > "$output"
else
    render
fi

if [[ -n "$violations" ]]; then
    exit 3
fi

