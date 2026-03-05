#!/usr/bin/env bash
set -euo pipefail

build_dir="${1:-build}"
enterprise_regex='sb_auth_plugin_(ident|radius|pam|ldap|kerberos)_selftest|sb_auth_plugin_hardening_h1_selftest'
full_regex='sb_auth_plugin_.*(selftest|contract_harness)'

echo "[enterprise-matrix] running targeted enterprise matrix gate"
ctest --test-dir "$build_dir" -R "$enterprise_regex" --output-on-failure

echo "[enterprise-matrix] running full plugin fail-open regression gate"
ctest --test-dir "$build_dir" -R "$full_regex" --output-on-failure

echo "[enterprise-matrix] PASS"
