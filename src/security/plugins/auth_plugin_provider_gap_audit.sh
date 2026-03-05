#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

declare -A PLUGIN_FILES=(
  [ident]="${SCRIPT_DIR}/ident/ident_plugin.cpp"
  [radius]="${SCRIPT_DIR}/radius/radius_plugin.cpp"
  [pam]="${SCRIPT_DIR}/pam/pam_plugin.cpp"
  [ldap]="${SCRIPT_DIR}/ldap/ldap_plugin.cpp"
  [kerberos]="${SCRIPT_DIR}/kerberos/kerberos_plugin.cpp"
)

count_pattern() {
  local pattern="$1"
  local file="$2"
  local count
  count=$(rg -n -F -- "$pattern" "$file" 2>/dev/null | wc -l | tr -d ' ')
  echo "${count}"
}

echo "plugin,file,lines,synthetic_token_refs,policy_test_toggle_refs,total_gap_markers"
for plugin in ident radius pam ldap kerberos; do
  file="${PLUGIN_FILES[$plugin]}"
  if [[ ! -f "${file}" ]]; then
    echo "${plugin},${file},0,0,0,0"
    continue
  fi

  lines=$(wc -l < "${file}" | tr -d ' ')
  timeout_refs=$(count_pattern "__timeout__" "${file}")
  deny_refs=$(count_pattern "__deny__" "${file}")
  reject_refs=$(count_pattern "__reject__" "${file}")
  simulate_refs=$(count_pattern "simulate" "${file}")
  test_toggle_refs=$(count_pattern "allow_test_directives" "${file}")

  synthetic_refs=$((timeout_refs + deny_refs + reject_refs + simulate_refs))
  total_refs=$((synthetic_refs + test_toggle_refs))

  echo "${plugin},${file},${lines},${synthetic_refs},${test_toggle_refs},${total_refs}"
done
