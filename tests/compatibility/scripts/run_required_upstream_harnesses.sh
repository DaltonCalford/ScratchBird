#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
ARTIFACT_ROOT="${SCRATCHBIRD_EMU_ARTIFACT_ROOT:-${REPO_ROOT}/tests/compatibility/results/emulation}"
MODE="${1:-plan}"
RUN_ID="$(date -u +%Y%m%dT%H%M%SZ)"
SUMMARY_FILE="${ARTIFACT_ROOT}/required-upstream-harnesses-${MODE}-${RUN_ID}.md"

MYSQL_UPSTREAM_CMD="SCRATCHBIRD_MY_USE_UPSTREAM=1 SCRATCHBIRD_MY_COMPAT_RUN=1 bash \"${REPO_ROOT}/tests/compatibility/mysql/scripts/run_mysql_ctest.sh\""
POSTGRESQL_UPSTREAM_CMD="SCRATCHBIRD_PG_USE_UPSTREAM=1 SCRATCHBIRD_PG_COMPAT_RUN=1 bash \"${REPO_ROOT}/tests/compatibility/postgresql/scripts/run_postgresql_ctest.sh\""
FIREBIRD_UPSTREAM_CMD="bash \"${REPO_ROOT}/scripts/emulation/start_upstream_suite_gates.sh\" execute"

mkdir -p "${ARTIFACT_ROOT}"

{
  echo "Last updated: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo
  echo "# Required Upstream Harness Launcher"
  echo
  echo "- Mode: \`${MODE}\`"
  echo "- Repo root: \`${REPO_ROOT}\`"
  echo
  echo "## Command templates"
  echo '```bash'
  echo "${MYSQL_UPSTREAM_CMD}"
  echo "${POSTGRESQL_UPSTREAM_CMD}"
  echo "${FIREBIRD_UPSTREAM_CMD}"
  echo '```'
  echo
} > "${SUMMARY_FILE}"

run_and_record() {
  local label="$1"
  local command="$2"

  echo "## ${label}" >> "${SUMMARY_FILE}"
  echo >> "${SUMMARY_FILE}"
  echo "- Command: \`${command}\`" >> "${SUMMARY_FILE}"

  set +e
  bash -lc "${command}"
  local rc=$?
  set -e

  echo "- Exit code: \`${rc}\`" >> "${SUMMARY_FILE}"
  echo >> "${SUMMARY_FILE}"
  return "${rc}"
}

case "${MODE}" in
  plan)
    echo "[upstream-harness] plan mode: command templates only"
    echo "[upstream-harness] summary: ${SUMMARY_FILE}"
    ;;
  execute)
    echo "[upstream-harness] execute mode: running MySQL/PostgreSQL/Firebird upstream harness commands"
    mysql_rc=0
    postgresql_rc=0
    firebird_rc=0

    run_and_record "MySQL upstream MTR lane" "${MYSQL_UPSTREAM_CMD}" || mysql_rc=$?
    run_and_record "PostgreSQL upstream pg_regress lane" "${POSTGRESQL_UPSTREAM_CMD}" || postgresql_rc=$?
    run_and_record "Firebird upstream firebird-qa lane" "${FIREBIRD_UPSTREAM_CMD}" || firebird_rc=$?

    if [[ "${mysql_rc}" -ne 0 || "${postgresql_rc}" -ne 0 || "${firebird_rc}" -ne 0 ]]; then
      echo "[upstream-harness] one or more lanes failed"
      echo "[upstream-harness] summary: ${SUMMARY_FILE}"
      exit 1
    fi

    echo "[upstream-harness] all lanes completed successfully"
    echo "[upstream-harness] summary: ${SUMMARY_FILE}"
    ;;
  *)
    echo "Error: unknown mode '${MODE}'. Use 'plan' or 'execute'." >&2
    exit 2
    ;;
esac

