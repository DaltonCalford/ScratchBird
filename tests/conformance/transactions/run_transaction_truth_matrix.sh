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

row_summary() {
  local lane="$1"
  local normalized_file="$2"
  local pass_count fail_count na_count
  pass_count=$(awk -F'|' '$3=="PASS"{c++} END{print c+0}' "${normalized_file}")
  fail_count=$(awk -F'|' '$3=="FAIL"{c++} END{print c+0}' "${normalized_file}")
  na_count=$(awk -F'|' '$3=="NA"{c++} END{print c+0}' "${normalized_file}")
  printf 'LANE_SUMMARY|%s|%s|%s|%s\n' "${lane}" "${pass_count}" "${fail_count}" "${na_count}"
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
  local raw="${RESULT_DIR}/native/raw.txt"
  local normalized="${RESULT_DIR}/native/normalized.txt"

  "${BUILD_DIR}/tests/test_transaction_truth_native" --gtest_color=no > "${raw}" 2>&1
  rg '^ROW_RESULT\|' "${raw}" > "${normalized}"
  row_summary native "${normalized}" > "${RESULT_DIR}/native/summary.txt"
}

run_default_lane() {
  local lane="$1"
  local sql_file="$2"
  local raw="$3"

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
      local pg_db="${SCRATCHBIRD_PG_DB:-${PGDATABASE:-main}}"
      PGPASSWORD="${SCRATCHBIRD_PG_PASSWORD:-${PGPASSWORD:-}}" \
        "${pg_bin}" -h "${pg_host}" -p "${pg_port}" -U "${pg_user}" -d "${pg_db}" \
        -f "${sql_file}" -q > "${raw}" 2>&1
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
      if [[ -n "${my_password}" ]]; then
        MYSQL_PWD="${my_password}" \
          "${my_bin}" --protocol=TCP -h "${my_host}" -P "${my_port}" -u "${my_user}" \
          --batch --raw < "${sql_file}" > "${raw}" 2>&1
      else
        "${my_bin}" --protocol=TCP -h "${my_host}" -P "${my_port}" -u "${my_user}" \
          --batch --raw < "${sql_file}" > "${raw}" 2>&1
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
      local wrapper_sql="${RESULT_DIR}/firebird/transaction_truth.run.sql"
      local emu_db="${RESULT_DIR}/firebird/transaction_truth_${RUN_ID//[^0-9]/}_${RANDOM}.fdb"
      {
        printf "CREATE DATABASE '%s';\n" "${emu_db}"
        printf "CONNECT '%s';\n" "${emu_db}"
        cat "${sql_file}"
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
  local sql_file="$2"
  local expected_file="$3"
  local normalize_script="$4"
  local run_cmd="$5"

  local raw="${RESULT_DIR}/${lane}/raw.txt"
  local normalized="${RESULT_DIR}/${lane}/normalized.txt"
  local diff_file="${RESULT_DIR}/diff/${lane}.diff"

  local executed=0
  if [[ -n "${run_cmd}" ]]; then
    # shellcheck disable=SC2086
    eval ${run_cmd} > "${raw}" 2>&1
    executed=1
  else
    if run_default_lane "${lane}" "${sql_file}" "${raw}"; then
      executed=1
    else
      local rc=$?
      if [[ "${rc}" -eq 2 ]]; then
        cp "${expected_file}" "${normalized}"
        sed -i -E 's/\|PASS\|/|NA|/' "${normalized}"
        row_summary "${lane}" "${normalized}" > "${RESULT_DIR}/${lane}/summary.txt"
        echo "LANE_STATUS|${lane}|NA|runner command not configured" >> "${RESULT_DIR}/lane_status.txt"
        return 0
      fi
      : > "${normalized}"
      row_summary "${lane}" "${normalized}" > "${RESULT_DIR}/${lane}/summary.txt"
      echo "LANE_STATUS|${lane}|FAIL|runner execution failed (see ${raw})" >> "${RESULT_DIR}/lane_status.txt"
      return 1
    fi
  fi

  if [[ "${executed}" -eq 1 ]]; then
    "${normalize_script}" "${raw}" > "${normalized}"
    if ! diff -u "${expected_file}" "${normalized}" > "${diff_file}"; then
      echo "LANE_STATUS|${lane}|FAIL|normalized output mismatch" >> "${RESULT_DIR}/lane_status.txt"
      row_summary "${lane}" "${normalized}" > "${RESULT_DIR}/${lane}/summary.txt"
      return 1
    fi
    rm -f "${diff_file}"
    echo "LANE_STATUS|${lane}|PASS|matched expected output" >> "${RESULT_DIR}/lane_status.txt"
  fi

  row_summary "${lane}" "${normalized}" > "${RESULT_DIR}/${lane}/summary.txt"
  return 0
}

main() {
  run_native_lane

  run_emulation_lane "postgresql" \
    "${REPO_DIR}/tests/compatibility/postgresql/tests_conformance/transaction_truth.sql" \
    "${REPO_DIR}/tests/compatibility/postgresql/tests_conformance/transaction_truth.expected" \
    "${REPO_DIR}/tests/compatibility/postgresql/tests_conformance/normalize_transaction_truth.sh" \
    "${A55_PG_RUN_CMD:-}"

  run_emulation_lane "mysql" \
    "${REPO_DIR}/tests/compatibility/mysql/tests_conformance/transaction_truth.sql" \
    "${REPO_DIR}/tests/compatibility/mysql/tests_conformance/transaction_truth.expected" \
    "${REPO_DIR}/tests/compatibility/mysql/tests_conformance/normalize_transaction_truth.sh" \
    "${A55_MYSQL_RUN_CMD:-}"

  run_emulation_lane "firebird" \
    "${REPO_DIR}/tests/compatibility/firebird/tests_conformance/transaction_truth.sql" \
    "${REPO_DIR}/tests/compatibility/firebird/tests_conformance/transaction_truth.expected" \
    "${REPO_DIR}/tests/compatibility/firebird/tests_conformance/normalize_transaction_truth.sh" \
    "${A55_FIREBIRD_RUN_CMD:-}"

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
