#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "Usage: $0 <scratchbird_repo_root>" >&2
    exit 2
fi

repo_root="$1"
scanner="${repo_root}/tools/compliance/vnext_scope_scan.sh"
if [[ ! -x "$scanner" ]]; then
    echo "Scope scanner not executable: $scanner" >&2
    exit 2
fi

tmp_root="$(mktemp -d)"
cleanup() {
    rm -rf "$tmp_root"
}
trap cleanup EXIT

test_repo="${tmp_root}/scope_scan_repo"
mkdir -p "$test_repo"
git -C "$test_repo" init -q
git -C "$test_repo" config user.email "scope.scan@test.local"
git -C "$test_repo" config user.name "scope-scan-contract"

mkdir -p \
    "$test_repo/src/core" \
    "$test_repo/src/parser/mysql" \
    "$test_repo/src/parser/postgresql" \
    "$test_repo/src/udr" \
    "$test_repo/include/scratchbird/udr"

cat > "$test_repo/src/core/allowed.cpp" <<'EOF'
// allowed baseline file
EOF
cat > "$test_repo/src/parser/mysql/mysql_parser.cpp" <<'EOF'
// forbidden parser file
EOF
cat > "$test_repo/src/parser/postgresql/pg_parser_ddl.cpp" <<'EOF'
// forbidden parser file
EOF
cat > "$test_repo/src/udr/mysql_udr.cpp" <<'EOF'
// forbidden production udr file
EOF
cat > "$test_repo/include/scratchbird/udr/mysql_udr.hpp" <<'EOF'
// forbidden production udr header
EOF

git -C "$test_repo" add .
git -C "$test_repo" commit -q -m "baseline"

# Case 1: allowed-only change passes.
echo "// changed allowed file" >> "$test_repo/src/core/allowed.cpp"
"$scanner" --repo "$test_repo" > "${tmp_root}/allowed_report.txt"

git -C "$test_repo" checkout -q -- .

# Case 2: forbidden change fails with exit code 3.
echo "// changed forbidden parser file" >> "$test_repo/src/parser/mysql/mysql_parser.cpp"
set +e
"$scanner" --repo "$test_repo" > "${tmp_root}/forbidden_report.txt"
rc=$?
set -e
if [[ $rc -ne 3 ]]; then
    echo "Expected scanner to fail with exit code 3 for forbidden changes, got: $rc" >&2
    cat "${tmp_root}/forbidden_report.txt" >&2
    exit 1
fi
grep -q "src/parser/mysql/mysql_parser.cpp" "${tmp_root}/forbidden_report.txt" || {
    echo "Forbidden report missing changed parser path" >&2
    cat "${tmp_root}/forbidden_report.txt" >&2
    exit 1
}

# Case 3: allowlist suppresses known baseline violation.
cat > "${tmp_root}/allowlist.txt" <<'EOF'
src/parser/mysql/mysql_parser.cpp
EOF
"$scanner" --repo "$test_repo" --allowlist "${tmp_root}/allowlist.txt" \
    > "${tmp_root}/allowlisted_report.txt"
grep -q "## Violations" "${tmp_root}/allowlisted_report.txt" || {
    echo "Allowlisted report missing violations section" >&2
    cat "${tmp_root}/allowlisted_report.txt" >&2
    exit 1
}
grep -q "(none)" "${tmp_root}/allowlisted_report.txt" || {
    echo "Allowlisted report should contain no violations" >&2
    cat "${tmp_root}/allowlisted_report.txt" >&2
    exit 1
}

# Case 4: show-all includes changed file section.
"$scanner" --repo "$test_repo" --allowlist "${tmp_root}/allowlist.txt" --show-all \
    > "${tmp_root}/show_all_report.txt"
grep -q "## Changed Files" "${tmp_root}/show_all_report.txt" || {
    echo "Show-all report missing changed files section" >&2
    cat "${tmp_root}/show_all_report.txt" >&2
    exit 1
}

echo "vnext scope scan contract: PASS"
