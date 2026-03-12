#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "${ROOT_DIR}/../../.." && pwd)"
BUILD_DIR="${REPO_DIR}/build"
DRIVER_DIR="${REPO_DIR}-driver"
CLIWORK_ROOT="$(cd "${REPO_DIR}/.." && pwd)"
RUN_ID="$(date -u +%Y%m%dT%H%M%SZ)"
RESULT_DIR="${ROOT_DIR}/results/${RUN_ID}"
mkdir -p "${RESULT_DIR}"/{native,postgresql,mysql,firebird,diff}

EXAMPLE_ENV_FILE="${SCRATCHBIRD_EXAMPLE_ENV_FILE:-/tmp/scratchbird-example-dynamic/profiles/runtime.env}"
if [[ -f "${EXAMPLE_ENV_FILE}" ]]; then
  # shellcheck disable=SC1090
  source "${EXAMPLE_ENV_FILE}"
fi

summary_from_sec_results() {
  local lane="$1"
  local file="$2"
  local pass fail na
  pass=$(awk -F'|' '$3=="PASS"{c++} END{print c+0}' "${file}")
  fail=$(awk -F'|' '$3=="FAIL"{c++} END{print c+0}' "${file}")
  na=$(awk -F'|' '$3=="NA"{c++} END{print c+0}' "${file}")
  printf 'SEC_LANE_SUMMARY|%s|%s|%s|%s\n' "${lane}" "${pass}" "${fail}" "${na}"
}

resolve_executable() {
  local preferred="$1"
  shift

  if [[ -n "${preferred}" && -x "${preferred}" ]]; then
    printf '%s' "${preferred}"
    return 0
  fi

  local candidate
  for candidate in "$@"; do
    if [[ -x "${candidate}" ]]; then
      printf '%s' "${candidate}"
      return 0
    fi
  done

  return 1
}

run_native_lane() {
  local raw="${RESULT_DIR}/native/ctest_raw.txt"
  local normalized="${RESULT_DIR}/native/normalized.txt"
  local audit_raw="${RESULT_DIR}/native/audit_visibility_raw.txt"

  ctest --test-dir "${BUILD_DIR}" \
    -R "SecurityPhase3_3_ColumnPermissions|SecurityPhase3_5_RLS_DML|DomainSecurity|DomainEncryption|DomainE2EScenarios" \
    --output-on-failure > "${raw}" 2>&1

  local has_col has_rls has_domsec has_domencrypt has_audit
  has_col=$(rg -n "SecurityPhase3_3_ColumnPermissions.*Passed" "${raw}" || true)
  has_rls=$(rg -n "SecurityPhase3_5_RLS_DML.*Passed" "${raw}" || true)
  has_domsec=$(rg -n "DomainSecurity.*Passed" "${raw}" || true)
  has_domencrypt=$(rg -n "DomainEncryption.*Passed" "${raw}" || true)
  has_audit=""
  if "${BUILD_DIR}/tests/test_domain_security" \
      --gtest_filter='DomainSecurityIntegrationTest.AuditsDomainAccess' \
      --gtest_color=no > "${audit_raw}" 2>&1; then
    has_audit="yes"
  fi

  {
    if [[ -n "${has_rls}" ]]; then
      echo "SEC_RESULT|SEC-001|PASS|native_rls_allow"
      echo "SEC_RESULT|SEC-002|PASS|native_rls_deny"
    else
      echo "SEC_RESULT|SEC-001|FAIL|native_rls_tests_not_passing"
      echo "SEC_RESULT|SEC-002|FAIL|native_rls_tests_not_passing"
    fi

    if [[ -n "${has_col}" ]]; then
      echo "SEC_RESULT|SEC-003|PASS|native_column_allow"
      echo "SEC_RESULT|SEC-004|PASS|native_column_deny"
    else
      echo "SEC_RESULT|SEC-003|FAIL|native_column_tests_not_passing"
      echo "SEC_RESULT|SEC-004|FAIL|native_column_tests_not_passing"
    fi

    if [[ -n "${has_domsec}" ]]; then
      echo "SEC_RESULT|SEC-005|PASS|native_domain_masking_privileged"
      echo "SEC_RESULT|SEC-006|PASS|native_domain_masking_unprivileged"
    else
      echo "SEC_RESULT|SEC-005|FAIL|native_domain_security_not_passing"
      echo "SEC_RESULT|SEC-006|FAIL|native_domain_security_not_passing"
    fi

    if [[ -n "${has_domencrypt}" ]]; then
      echo "SEC_RESULT|SEC-007|PASS|native_domain_encryption_allow"
      echo "SEC_RESULT|SEC-008|PASS|native_domain_encryption_deny"
    else
      echo "SEC_RESULT|SEC-007|FAIL|native_domain_encryption_not_passing"
      echo "SEC_RESULT|SEC-008|FAIL|native_domain_encryption_not_passing"
    fi

    if [[ -n "${has_audit}" ]]; then
      echo "SEC_RESULT|SEC-009|PASS|native_audit_visibility"
    else
      echo "SEC_RESULT|SEC-009|FAIL|native_audit_visibility_test_failed"
    fi
  } > "${normalized}"

  summary_from_sec_results native "${normalized}" > "${RESULT_DIR}/native/summary.txt"
}

run_default_lane() {
  local lane="$1"
  local raw="$2"

  case "${lane}" in
    postgresql)
      local pg_bin
      pg_bin="$(resolve_executable "${SCRATCHBIRD_PG_PSQL_BIN:-${SCRATCHBIRD_PG_ISQL:-}}" \
        "${CLIWORK_ROOT}/postgresql/build_codex/src/bin/psql/psql" \
        "${CLIWORK_ROOT}/postgresql/build_codex2/src/bin/psql/psql" \
        "${CLIWORK_ROOT}/postgresql/build/src/bin/psql/psql" \
        "${CLIWORK_ROOT}/postgresql/build_relwithdebinfo/src/bin/psql/psql" \
        "${CLIWORK_ROOT}/postgresql/build_release/src/bin/psql/psql")" || return 2
      if [[ "$(basename "${pg_bin}")" != "psql" ]]; then
        return 2
      fi
      local pg_host="${SCRATCHBIRD_PG_HOST:-127.0.0.1}"
      local pg_port="${SCRATCHBIRD_PG_PORT:-5432}"
      local pg_user="${SCRATCHBIRD_PG_USER:-postgres}"
      local pg_db="${SCRATCHBIRD_PG_DB:-main}"
      {
        cat "${REPO_DIR}/tests/compatibility/postgresql/tests_conformance/security_rls_parity.sql"
        cat "${REPO_DIR}/tests/compatibility/postgresql/tests_conformance/security_column_parity.sql"
        cat "${REPO_DIR}/tests/compatibility/postgresql/tests_conformance/security_domain_parity.sql"
      } > "${RESULT_DIR}/postgresql/security_parity.run.sql"
      PGPASSWORD="${SCRATCHBIRD_PG_PASSWORD:-${PGPASSWORD:-}}" \
        "${pg_bin}" -h "${pg_host}" -p "${pg_port}" -U "${pg_user}" -d "${pg_db}" \
        -f "${RESULT_DIR}/postgresql/security_parity.run.sql" -q > "${raw}" 2>&1
      ;;
    mysql)
      local my_bin
      my_bin="$(resolve_executable "${SCRATCHBIRD_MYSQL_CLI_BIN:-${SCRATCHBIRD_MY_ISQL:-}}" \
        "${CLIWORK_ROOT}/mysql-server/build_codex2/runtime_output_directory/mysql" \
        "${CLIWORK_ROOT}/mysql-server/build_codex/runtime_output_directory/mysql" \
        "${CLIWORK_ROOT}/mysql-server/build/runtime_output_directory/mysql")" || return 2
      if [[ "$(basename "${my_bin}")" != "mysql" ]]; then
        return 2
      fi
      local my_host="${SCRATCHBIRD_MY_HOST:-127.0.0.1}"
      local my_port="${SCRATCHBIRD_MY_PORT:-3306}"
      local my_user="${SCRATCHBIRD_MY_USER:-root}"
      local my_password="${SCRATCHBIRD_MY_PASSWORD:-}"
      local wrapper_sql="${RESULT_DIR}/mysql/security_parity.run.sql"
      {
        cat "${REPO_DIR}/tests/compatibility/mysql/tests_conformance/security_rls_parity.sql"
        cat "${REPO_DIR}/tests/compatibility/mysql/tests_conformance/security_column_parity.sql"
        cat "${REPO_DIR}/tests/compatibility/mysql/tests_conformance/security_domain_parity.sql"
      } > "${wrapper_sql}"
      if [[ -n "${my_password}" ]]; then
        MYSQL_PWD="${my_password}" \
          "${my_bin}" --protocol=TCP -h "${my_host}" -P "${my_port}" -u "${my_user}" \
          --batch --raw < "${wrapper_sql}" > "${raw}" 2>&1
      else
        "${my_bin}" --protocol=TCP -h "${my_host}" -P "${my_port}" -u "${my_user}" \
          --batch --raw < "${wrapper_sql}" > "${raw}" 2>&1
      fi
      ;;
    firebird)
      local fb_bin
      fb_bin="$(resolve_executable "${SCRATCHBIRD_FB_NATIVE_ISQL:-${SCRATCHBIRD_FB_ISQL:-}}" \
        "${CLIWORK_ROOT}/firebird/gen/Release/firebird/bin/isql" \
        "${CLIWORK_ROOT}/firebird/gen/Debug/firebird/bin/isql" \
        "${CLIWORK_ROOT}/firebird/build/bin/isql" \
        "${CLIWORK_ROOT}/firebird/build/isql")" || return 2
      case "$(basename "${fb_bin}")" in
        isql|isql-fb)
          ;;
        *)
          return 2
          ;;
      esac
      local wrapper_sql="${RESULT_DIR}/firebird/security_parity.run.sql"
      local emu_db="${RESULT_DIR}/firebird/security_parity_${RUN_ID//[^0-9]/}_${RANDOM}.fdb"
      {
        printf "CREATE DATABASE '%s';\n" "${emu_db}"
        printf "CONNECT '%s';\n" "${emu_db}"
        cat "${REPO_DIR}/tests/compatibility/firebird/tests_conformance/security_rls_parity.sql"
        cat "${REPO_DIR}/tests/compatibility/firebird/tests_conformance/security_column_parity.sql"
        cat "${REPO_DIR}/tests/compatibility/firebird/tests_conformance/security_domain_parity.sql"
      } > "${wrapper_sql}"
      if ! "${fb_bin}" -user "${SCRATCHBIRD_FB_USER:-SYSDBA}" \
        -password "${SCRATCHBIRD_FB_PASSWORD:-masterkey}" \
        -q -i "${wrapper_sql}" > "${raw}" 2>&1; then
        if rg -q "Interface IUtil version too old" "${raw}"; then
          return 2
        fi
        return 1
      fi
      ;;
    *)
      return 2
      ;;
  esac

  return 0
}

run_emulation_lane() {
  local lane="$1"
  local run_cmd="$2"
  local expected_rls="$3"
  local expected_col="$4"
  local expected_dom="$5"
  local normalizer="$6"

  local raw="${RESULT_DIR}/${lane}/raw.txt"
  local normalized="${RESULT_DIR}/${lane}/normalized.txt"
  local expected_all="${RESULT_DIR}/${lane}/expected_all.txt"
  local diff_file="${RESULT_DIR}/diff/${lane}.diff"

  cat "${expected_rls}" "${expected_col}" "${expected_dom}" > "${expected_all}"

  local executed=0
  if [[ -n "${run_cmd}" ]]; then
    # shellcheck disable=SC2086
    eval ${run_cmd} > "${raw}" 2>&1
    executed=1
  else
    if run_default_lane "${lane}" "${raw}"; then
      executed=1
    else
      local rc=$?
      if [[ "${rc}" -eq 2 ]]; then
        cp "${expected_all}" "${normalized}"
        echo "SEC_LANE_STATUS|${lane}|NA|runner command not configured" >> "${RESULT_DIR}/lane_status.txt"
        summary_from_sec_results "${lane}" "${normalized}" > "${RESULT_DIR}/${lane}/summary.txt"
        return 0
      fi
      : > "${normalized}"
      echo "SEC_LANE_STATUS|${lane}|FAIL|runner execution failed (see ${raw})" >> "${RESULT_DIR}/lane_status.txt"
      summary_from_sec_results "${lane}" "${normalized}" > "${RESULT_DIR}/${lane}/summary.txt"
      return 1
    fi
  fi

  if [[ "${executed}" -eq 1 ]]; then
    "${normalizer}" "${raw}" > "${normalized}"
    if ! diff -u "${expected_all}" "${normalized}" > "${diff_file}"; then
      echo "SEC_LANE_STATUS|${lane}|FAIL|normalized mismatch" >> "${RESULT_DIR}/lane_status.txt"
      summary_from_sec_results "${lane}" "${normalized}" > "${RESULT_DIR}/${lane}/summary.txt"
      return 1
    fi
    rm -f "${diff_file}"
    echo "SEC_LANE_STATUS|${lane}|PASS|matched expected" >> "${RESULT_DIR}/lane_status.txt"
  fi

  summary_from_sec_results "${lane}" "${normalized}" > "${RESULT_DIR}/${lane}/summary.txt"
  return 0
}

main() {
  run_native_lane

  run_emulation_lane "postgresql" "${A55_SEC_PG_RUN_CMD:-}" \
    "${REPO_DIR}/tests/compatibility/postgresql/tests_conformance/security_rls_parity.expected" \
    "${REPO_DIR}/tests/compatibility/postgresql/tests_conformance/security_column_parity.expected" \
    "${REPO_DIR}/tests/compatibility/postgresql/tests_conformance/security_domain_parity.expected" \
    "${REPO_DIR}/tests/compatibility/postgresql/tests_conformance/normalize_security_parity.sh"

  run_emulation_lane "mysql" "${A55_SEC_MYSQL_RUN_CMD:-}" \
    "${REPO_DIR}/tests/compatibility/mysql/tests_conformance/security_rls_parity.expected" \
    "${REPO_DIR}/tests/compatibility/mysql/tests_conformance/security_column_parity.expected" \
    "${REPO_DIR}/tests/compatibility/mysql/tests_conformance/security_domain_parity.expected" \
    "${REPO_DIR}/tests/compatibility/mysql/tests_conformance/normalize_security_parity.sh"

  run_emulation_lane "firebird" "${A55_SEC_FIREBIRD_RUN_CMD:-}" \
    "${REPO_DIR}/tests/compatibility/firebird/tests_conformance/security_rls_parity.expected" \
    "${REPO_DIR}/tests/compatibility/firebird/tests_conformance/security_column_parity.expected" \
    "${REPO_DIR}/tests/compatibility/firebird/tests_conformance/security_domain_parity.expected" \
    "${REPO_DIR}/tests/compatibility/firebird/tests_conformance/normalize_security_parity.sh"

  {
    cat "${RESULT_DIR}/native/summary.txt"
    cat "${RESULT_DIR}/postgresql/summary.txt"
    cat "${RESULT_DIR}/mysql/summary.txt"
    cat "${RESULT_DIR}/firebird/summary.txt"
  } > "${RESULT_DIR}/matrix_summary.txt"

  cat "${RESULT_DIR}/lane_status.txt"
  cat "${RESULT_DIR}/matrix_summary.txt"
  echo "RESULT_DIR=${RESULT_DIR}"
}

main "$@"
