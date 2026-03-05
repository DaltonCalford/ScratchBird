#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

declare -A SWITCH_PATTERNS=(
  ["trust_reject"]="auth.trust_reject.trust_enabled"
  ["peer"]="auth.peer.accept_ipc"
  ["password_compat"]="auth.password_compat.default_credential_ref"
  ["token_authkey"]="auth.token_authkey.expected_issuer"
  ["certificate_mtls"]="auth.certificate_mtls.required_san_prefix"
  ["jwt_oidc"]="auth.jwt_oidc.jwt_expected_issuer"
  ["oauth_validator"]="auth.oauth_validator.expected_issuer"
  ["proxy_assertion"]="auth.proxy_assertion.expected_proxy_id"
  ["workload_identity"]="auth.workload_identity.oidc_trust_bundle"
  ["ident"]="trusted_cidrs"
  ["radius"]="shared_secret_ref"
  ["pam"]="service_name"
  ["ldap"]="allowed_ldap_endpoints"
  ["kerberos"]="allowed_kdc_endpoints"
  ["scram"]="auth.scram.default_credential_ref"
  ["webauthn"]="auth.webauthn.allowed_origin"
  ["factor_chain"]="auth.factor_chain.2fa.sequence"
)

plugins=(
  trust_reject
  peer
  password_compat
  token_authkey
  certificate_mtls
  jwt_oidc
  oauth_validator
  proxy_assertion
  workload_identity
  ident
  radius
  pam
  ldap
  kerberos
  scram
  webauthn
  factor_chain
)

failures=0

for plugin in "${plugins[@]}"; do
  plugin_src="${ROOT_DIR}/${plugin}/${plugin}_plugin.cpp"
  if [[ ! -f "${plugin_src}" ]]; then
    echo "FAIL: missing plugin source for ${plugin}: ${plugin_src}"
    failures=$((failures + 1))
    continue
  fi

  switch_pattern="${SWITCH_PATTERNS[${plugin}]}"
  if ! rg -q --fixed-strings "${switch_pattern}" "${plugin_src}" "${ROOT_DIR}/${plugin}/${plugin}_plugin_config.cpp" 2>/dev/null; then
    echo "FAIL: rollout switch pattern not found for ${plugin}: ${switch_pattern}"
    failures=$((failures + 1))
  fi

  if ! rg -q --fixed-strings "allow_count" "${plugin_src}"; then
    echo "FAIL: health counter allow_count missing for ${plugin}"
    failures=$((failures + 1))
  fi
  if ! rg -q --fixed-strings "deny_count" "${plugin_src}"; then
    echo "FAIL: health counter deny_count missing for ${plugin}"
    failures=$((failures + 1))
  fi
  if ! rg -q --fixed-strings "continue_count" "${plugin_src}"; then
    echo "FAIL: health counter continue_count missing for ${plugin}"
    failures=$((failures + 1))
  fi
  if ! rg -q --fixed-strings "error_count" "${plugin_src}"; then
    echo "FAIL: health counter error_count missing for ${plugin}"
    failures=$((failures + 1))
  fi
done

if [[ ${failures} -ne 0 ]]; then
  echo "verify_rollout_switch_matrix: FAIL (${failures} issue(s))"
  exit 1
fi

echo "verify_rollout_switch_matrix: PASS"
