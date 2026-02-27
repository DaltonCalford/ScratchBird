#!/usr/bin/env bash
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUNDLE_BOOTSTRAP="${SCRIPT_DIR}/bootstrap.sh"
INSTALL_SCRIPT="${SCRIPT_DIR}/suite/scripts/bootstrap_install_linux.sh"
REFRESH_SB_SCRIPT="${SCRIPT_DIR}/suite/scripts/refresh_scratchbird_repo.sh"

SB_USER="scratchbird"
SB_GROUP="scratchbird"
WORKSPACE_ROOT="/opt"
TARGET_DIR_NAME="sb_verification"
CLONE_PRESET="core"
CLONE_DEPTH=1

ASSUME_YES=0
SKIP_INSTALL_DEPS=0
REFRESH_PACKAGES=0
REFRESH_SB_REPO=0
REFRESH_ALL_REPOS=0
SKIP_RUNTIME_START=0
SKIP_VERIFY=0
KEEP_RUNTIME_UP=0
ARTIFACT_ZIP=""

RUN_ID="$(date -u +%Y%m%dT%H%M%SZ)"
WORKSPACE_DIR=""
LOG_ROOT=""
SUMMARY_FILE=""
ZIP_FILE=""
ANY_FAIL=0

declare -a STEP_ROWS=()

usage() {
  cat <<USAGE
Usage: $(basename "$0") [options]

Provision a Linux host for ScratchBird beta verification, then run:
  install -> user/group setup -> clone/update -> build+ctest -> runtime up -> verification
Finally packages diagnostics into a zip for email/debug review.

Options:
  --yes                       Non-interactive mode
  --workspace-root <path>     Root path for verification workspace (default: /opt)
  --target-dir <name>         Verification workspace directory name (default: sb_verification)
  --sb-user <name>            Host user to own/build workspace (default: scratchbird)
  --sb-group <name>           Host group to own/build workspace (default: scratchbird)
  --clone-preset <preset>     core|full|scratchbird (default: core)
  --clone-depth <n>           git clone depth (default: 1)
  --skip-install-deps         Skip apt dependency install
  --refresh-packages          Refresh apt packages before install
  --refresh-sb-repo           Fetch/pull ScratchBird repo before build (safe; no forced reset)
  --refresh-all-repos         Pass --update-existing to repository clone bootstrap
  --skip-runtime-start        Skip runtime stack startup
  --skip-verify               Skip verification runners
  --keep-runtime-up           Do not tear runtime stack down at end
  --artifact-zip <path>       Output zip path (default: <workspace>/reports/SB_Dev_Bootstrap_<run_id>.zip)
  --help                      Show help
USAGE
}

log() {
  echo "[SB_Dev_Bootstrap] $*"
}

append_step_row() {
  STEP_ROWS+=("$1|$2|$3|$4|$5|$6")
}

run_step() {
  local step_id="$1"
  local step_desc="$2"
  shift 2
  local step_log="${LOG_ROOT}/${step_id}.log"
  local rc=0
  local status="PASS"
  local start_ts end_ts elapsed

  mkdir -p "$(dirname "${step_log}")"
  start_ts="$(date +%s)"
  log "${step_id}: ${step_desc}"
  if "$@" > >(tee -a "${step_log}") 2> >(tee -a "${step_log}" >&2); then
    rc=0
    status="PASS"
  else
    rc=$?
    status="FAIL"
    ANY_FAIL=1
  fi
  end_ts="$(date +%s)"
  elapsed="$((end_ts - start_ts))"
  append_step_row "${step_id}" "${status}" "${rc}" "${elapsed}" "${step_log}" "${step_desc}"
  return "${rc}"
}

skip_step() {
  local step_id="$1"
  local step_desc="$2"
  local reason="$3"
  local step_log="${LOG_ROOT}/${step_id}.log"
  mkdir -p "$(dirname "${step_log}")"
  printf 'SKIPPED: %s\n' "${reason}" > "${step_log}"
  append_step_row "${step_id}" "SKIP" "0" "0" "${step_log}" "${step_desc} (${reason})"
}

join_quoted() {
  local out=""
  local arg
  for arg in "$@"; do
    out+=" $(printf "%q" "${arg}")"
  done
  printf "%s" "${out# }"
}

run_as_user_cmd() {
  local user="$1"
  shift
  if command -v runuser >/dev/null 2>&1; then
    runuser -u "${user}" -- "$@"
  else
    su - "${user}" -s /bin/bash -c "$(join_quoted "$@")"
  fi
}

ensure_system_user_group() {
  if ! getent group "${SB_GROUP}" >/dev/null 2>&1; then
    groupadd --system "${SB_GROUP}"
    echo "Created group: ${SB_GROUP}"
  else
    echo "Group exists: ${SB_GROUP}"
  fi

  if ! id -u "${SB_USER}" >/dev/null 2>&1; then
    useradd --create-home --home-dir "/home/${SB_USER}" --gid "${SB_GROUP}" --shell /bin/bash "${SB_USER}"
    echo "Created user: ${SB_USER}"
  else
    echo "User exists: ${SB_USER}"
    if ! id -nG "${SB_USER}" | tr ' ' '\n' | grep -Fxq "${SB_GROUP}"; then
      usermod -a -G "${SB_GROUP}" "${SB_USER}"
      echo "Added user ${SB_USER} to group ${SB_GROUP}"
    fi
  fi

  if getent group docker >/dev/null 2>&1; then
    if ! id -nG "${SB_USER}" | tr ' ' '\n' | grep -Fxq docker; then
      usermod -a -G docker "${SB_USER}"
      echo "Added user ${SB_USER} to docker group"
    fi
  fi
}

prepare_workspace() {
  mkdir -p "${WORKSPACE_DIR}"
  chown -R "${SB_USER}:${SB_GROUP}" "${WORKSPACE_DIR}"
}

collect_system_evidence() {
  local out_dir="$1"
  mkdir -p "${out_dir}"

  {
    echo "run_id=${RUN_ID}"
    echo "date_utc=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "script=${BASH_SOURCE[0]}"
    echo "workspace_dir=${WORKSPACE_DIR}"
    echo "sb_user=${SB_USER}"
    echo "sb_group=${SB_GROUP}"
  } > "${out_dir}/run_context.env"

  uname -a > "${out_dir}/uname.txt" 2>&1 || true
  [[ -f /etc/os-release ]] && cat /etc/os-release > "${out_dir}/os-release.txt" || true
  id "${SB_USER}" > "${out_dir}/sb_user_identity.txt" 2>&1 || true
  getent group "${SB_GROUP}" > "${out_dir}/sb_group.txt" 2>&1 || true

  {
    for cmd in git cmake ninja python3 pip3 docker psql mysql isql-fb zip jq; do
      if command -v "${cmd}" >/dev/null 2>&1; then
        printf '%s: ' "${cmd}"
        "${cmd}" --version 2>&1 | head -n 1
      else
        printf '%s: missing\n' "${cmd}"
      fi
    done
  } > "${out_dir}/tool_versions.txt"

  if command -v docker >/dev/null 2>&1; then
    docker ps -a > "${out_dir}/docker_ps_a.txt" 2>&1 || true
    docker info > "${out_dir}/docker_info.txt" 2>&1 || true
  fi

  if [[ -d "${WORKSPACE_DIR}/repos" ]]; then
    local repo
    for repo in ScratchBird ScratchBird-driver ScratchBird-ai ScratchRobin mysql-server postgresql firebird; do
      if [[ -d "${WORKSPACE_DIR}/repos/${repo}/.git" ]]; then
        {
          echo "repo=${repo}"
          git -C "${WORKSPACE_DIR}/repos/${repo}" remote -v
          git -C "${WORKSPACE_DIR}/repos/${repo}" rev-parse HEAD
          git -C "${WORKSPACE_DIR}/repos/${repo}" status --short
          echo
        } >> "${out_dir}/git_repo_state.txt" 2>&1 || true
      fi
    done
  fi
}

write_summary_file() {
  local final_status="$1"
  local out_file="$2"
  mkdir -p "$(dirname "${out_file}")"

  {
    echo "# SB_Dev_Bootstrap Summary"
    echo
    echo "- Run ID: \`${RUN_ID}\`"
    echo "- Final Status: \`${final_status}\`"
    echo "- Workspace: \`${WORKSPACE_DIR}\`"
    echo "- Log Root: \`${LOG_ROOT}\`"
    echo
    echo "## Step Results"
    echo
    echo "| Step | Status | RC | Seconds | Description | Log |"
    echo "| --- | --- | ---: | ---: | --- | --- |"
    local row step status rc seconds log_path desc
    for row in "${STEP_ROWS[@]}"; do
      IFS='|' read -r step status rc seconds log_path desc <<< "${row}"
      printf '| %s | %s | %s | %s | %s | `%s` |\n' \
        "${step}" "${status}" "${rc}" "${seconds}" "${desc}" "${log_path}"
    done
    echo
    echo "## Notes"
    echo
    echo "- If a step failed, attach this summary plus the generated zip artifact in your email."
    echo "- Logs are per-step and include command output and errors."
  } > "${out_file}"
}

package_artifacts() {
  local artifact_root="${WORKSPACE_DIR}/reports/SB_Dev_Bootstrap_${RUN_ID}"
  local system_dir="${artifact_root}/system"
  local final_status="$1"

  mkdir -p "${artifact_root}" "${system_dir}" "${artifact_root}/logs"
  cp -a "${LOG_ROOT}/." "${artifact_root}/logs/" 2>/dev/null || true

  collect_system_evidence "${system_dir}"

  if [[ -d "${WORKSPACE_DIR}/reports" ]]; then
    mkdir -p "${artifact_root}/workspace_reports"
    rsync -a \
      --exclude "SB_Dev_Bootstrap_*" \
      --exclude "bootstrap_logs" \
      "${WORKSPACE_DIR}/reports/" "${artifact_root}/workspace_reports/" || true
  fi

  if [[ -d "${WORKSPACE_DIR}/results" ]]; then
    mkdir -p "${artifact_root}/workspace_results"
    rsync -a "${WORKSPACE_DIR}/results/" "${artifact_root}/workspace_results/" || true
  fi

  if [[ -d "${WORKSPACE_DIR}/configs" ]]; then
    mkdir -p "${artifact_root}/workspace_configs"
    rsync -a "${WORKSPACE_DIR}/configs/" "${artifact_root}/workspace_configs/" || true
  fi

  if [[ -d "${WORKSPACE_DIR}/cases" ]]; then
    mkdir -p "${artifact_root}/workspace_cases"
    rsync -a "${WORKSPACE_DIR}/cases/" "${artifact_root}/workspace_cases/" || true
  fi

  SUMMARY_FILE="${artifact_root}/RUN_SUMMARY.md"
  write_summary_file "${final_status}" "${SUMMARY_FILE}"

  if [[ -z "${ARTIFACT_ZIP}" ]]; then
    ZIP_FILE="${WORKSPACE_DIR}/reports/SB_Dev_Bootstrap_${RUN_ID}.zip"
  else
    if [[ "${ARTIFACT_ZIP}" = /* ]]; then
      ZIP_FILE="${ARTIFACT_ZIP}"
    else
      ZIP_FILE="$(pwd)/${ARTIFACT_ZIP}"
    fi
  fi
  mkdir -p "$(dirname "${ZIP_FILE}")"

  if command -v zip >/dev/null 2>&1; then
    (
      cd "$(dirname "${artifact_root}")"
      zip -r "${ZIP_FILE}" "$(basename "${artifact_root}")" >/dev/null
    )
    sha256sum "${ZIP_FILE}" > "${ZIP_FILE}.sha256"
  else
    echo "zip command not found; artifact directory left unpacked at ${artifact_root}" >&2
    return 1
  fi

  return 0
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --yes)
      ASSUME_YES=1
      shift
      ;;
    --workspace-root)
      WORKSPACE_ROOT="$2"
      shift 2
      ;;
    --target-dir)
      TARGET_DIR_NAME="$2"
      shift 2
      ;;
    --sb-user)
      SB_USER="$2"
      shift 2
      ;;
    --sb-group)
      SB_GROUP="$2"
      shift 2
      ;;
    --clone-preset)
      CLONE_PRESET="$2"
      shift 2
      ;;
    --clone-depth)
      CLONE_DEPTH="$2"
      shift 2
      ;;
    --skip-install-deps)
      SKIP_INSTALL_DEPS=1
      shift
      ;;
    --refresh-packages)
      REFRESH_PACKAGES=1
      shift
      ;;
    --refresh-sb-repo)
      REFRESH_SB_REPO=1
      shift
      ;;
    --refresh-all-repos)
      REFRESH_ALL_REPOS=1
      shift
      ;;
    --skip-runtime-start)
      SKIP_RUNTIME_START=1
      shift
      ;;
    --skip-verify)
      SKIP_VERIFY=1
      shift
      ;;
    --keep-runtime-up)
      KEEP_RUNTIME_UP=1
      shift
      ;;
    --artifact-zip)
      ARTIFACT_ZIP="$2"
      shift 2
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage
      exit 1
      ;;
  esac
done

case "${CLONE_PRESET}" in
  core|full|scratchbird) ;;
  *)
    echo "Invalid --clone-preset: ${CLONE_PRESET}" >&2
    exit 1
    ;;
esac

if [[ "${EUID}" -ne 0 ]]; then
  echo "This script must run as root. Example: sudo ./SB_Dev_Bootstrap.sh --yes" >&2
  exit 1
fi

if [[ ! -x "${BUNDLE_BOOTSTRAP}" ]]; then
  echo "Bundle bootstrap script not found: ${BUNDLE_BOOTSTRAP}" >&2
  exit 1
fi
if [[ ! -x "${INSTALL_SCRIPT}" ]]; then
  echo "Install helper script not found: ${INSTALL_SCRIPT}" >&2
  exit 1
fi
if [[ "${REFRESH_SB_REPO}" -eq 1 && ! -x "${REFRESH_SB_SCRIPT}" ]]; then
  echo "ScratchBird refresh helper script not found: ${REFRESH_SB_SCRIPT}" >&2
  exit 1
fi

mkdir -p "${WORKSPACE_ROOT}"
WORKSPACE_DIR="$(cd "${WORKSPACE_ROOT}" && pwd)/${TARGET_DIR_NAME}"
LOG_ROOT="${WORKSPACE_DIR}/reports/bootstrap_logs/${RUN_ID}"
mkdir -p "${LOG_ROOT}"

if [[ "${ASSUME_YES}" -eq 0 ]]; then
  echo "This will:"
  echo "  1) install/update host packages (unless --skip-install-deps)"
  echo "  2) create/validate user+group: ${SB_USER}:${SB_GROUP}"
  echo "  3) clone/update repos, build ScratchBird, run ctest, start runtime, run verification"
  echo "  4) package logs/results/reports into a zip artifact for support"
  read -r -p "Continue? [y/N] " ans
  case "${ans}" in
    y|Y|yes|YES) ;;
    *)
      echo "Cancelled."
      exit 1
      ;;
  esac
fi

if [[ "${SKIP_INSTALL_DEPS}" -eq 0 ]]; then
  INSTALL_ARGS=(--yes)
  if [[ "${REFRESH_PACKAGES}" -eq 1 ]]; then
    INSTALL_ARGS+=(--refresh)
  fi
  run_step "S01_INSTALL_DEPS" "Install Linux build/test/verification dependencies" \
    "${INSTALL_SCRIPT}" "${INSTALL_ARGS[@]}" || true
else
  skip_step "S01_INSTALL_DEPS" "Install Linux build/test/verification dependencies" "--skip-install-deps set"
fi

run_step "S02_SYSTEM_USER_GROUP" "Ensure scratchbird user/group and docker membership" \
  ensure_system_user_group || true

run_step "S03_WORKSPACE_PREP" "Prepare workspace and ownership" \
  prepare_workspace || true

BOOTSTRAP_ARGS=(
  --workspace-root "${WORKSPACE_ROOT}"
  --target-dir "${TARGET_DIR_NAME}"
  --no-install-deps
  --skip-build
  --skip-verify
  --no-start-runtime
  --clone-preset "${CLONE_PRESET}"
  --clone-depth "${CLONE_DEPTH}"
)
if [[ "${REFRESH_ALL_REPOS}" -eq 1 ]]; then
  BOOTSTRAP_ARGS+=(--update-existing)
fi

if [[ "${ANY_FAIL}" -eq 0 ]]; then
  run_step "S04_WORKSPACE_BOOTSTRAP" "Provision verification workspace and clone/update repositories" \
    run_as_user_cmd "${SB_USER}" "${BUNDLE_BOOTSTRAP}" "${BOOTSTRAP_ARGS[@]}" || true
else
  skip_step "S04_WORKSPACE_BOOTSTRAP" "Provision verification workspace and clone/update repositories" "prior failure"
fi

if [[ "${REFRESH_SB_REPO}" -eq 1 ]]; then
  if [[ "${ANY_FAIL}" -eq 0 ]]; then
    run_step "S05_REFRESH_SB_REPO" "Refresh ScratchBird repository (safe pull)" \
      run_as_user_cmd "${SB_USER}" "${REFRESH_SB_SCRIPT}" "${WORKSPACE_DIR}/repos/ScratchBird" || true
  else
    skip_step "S05_REFRESH_SB_REPO" "Refresh ScratchBird repository (safe pull)" "prior failure"
  fi
else
  skip_step "S05_REFRESH_SB_REPO" "Refresh ScratchBird repository (safe pull)" "--refresh-sb-repo not set"
fi

if [[ "${ANY_FAIL}" -eq 0 ]]; then
  run_step "S06_BUILD_AND_CTEST" "Build ScratchBird and execute full ctest" \
    run_as_user_cmd "${SB_USER}" "${WORKSPACE_DIR}/scripts/bootstrap_build_scratchbird.sh" \
    --repo-root "${WORKSPACE_DIR}/repos" || true
else
  skip_step "S06_BUILD_AND_CTEST" "Build ScratchBird and execute full ctest" "prior failure"
fi

if [[ "${SKIP_RUNTIME_START}" -eq 0 ]]; then
  if [[ "${ANY_FAIL}" -eq 0 ]]; then
    run_step "S07_RUNTIME_UP" "Start ScratchBird static stack and reference engine containers" \
      run_as_user_cmd "${SB_USER}" "${WORKSPACE_DIR}/scripts/bootstrap_runtime_stack.sh" up || true
  else
    skip_step "S07_RUNTIME_UP" "Start ScratchBird static stack and reference engine containers" "prior failure"
  fi
else
  skip_step "S07_RUNTIME_UP" "Start ScratchBird static stack and reference engine containers" "--skip-runtime-start set"
fi

if [[ "${SKIP_VERIFY}" -eq 0 ]]; then
  if [[ "${ANY_FAIL}" -eq 0 ]]; then
    run_step "S08_VERIFY" "Run verification suite (footprint, differential, perf, wire when available)" \
      run_as_user_cmd "${SB_USER}" "${WORKSPACE_DIR}/scripts/run_full_verification.sh" \
      --workspace-root "${WORKSPACE_DIR}" \
      --repo-root "${WORKSPACE_DIR}/repos" || true
  else
    skip_step "S08_VERIFY" "Run verification suite (footprint, differential, perf, wire when available)" "prior failure"
  fi
else
  skip_step "S08_VERIFY" "Run verification suite (footprint, differential, perf, wire when available)" "--skip-verify set"
fi

if [[ "${SKIP_RUNTIME_START}" -eq 0 && "${KEEP_RUNTIME_UP}" -eq 0 ]]; then
  run_step "S09_RUNTIME_DOWN" "Stop runtime stack and containers" \
    run_as_user_cmd "${SB_USER}" "${WORKSPACE_DIR}/scripts/bootstrap_runtime_stack.sh" down || true
else
  skip_step "S09_RUNTIME_DOWN" "Stop runtime stack and containers" "--keep-runtime-up or --skip-runtime-start"
fi

FINAL_STATUS="PASS"
if [[ "${ANY_FAIL}" -ne 0 ]]; then
  FINAL_STATUS="FAIL"
fi

run_step "S10_PACKAGE_ARTIFACTS" "Package logs/reports/results into zip artifact" \
  package_artifacts "${FINAL_STATUS}" || true

if [[ "${ANY_FAIL}" -ne 0 ]]; then
  FINAL_STATUS="FAIL"
fi

log "Run complete: ${FINAL_STATUS}"
if [[ -n "${SUMMARY_FILE}" ]]; then
  log "Summary: ${SUMMARY_FILE}"
fi
if [[ -n "${ZIP_FILE}" ]]; then
  log "Artifact zip: ${ZIP_FILE}"
  if [[ -f "${ZIP_FILE}.sha256" ]]; then
    log "Artifact checksum: ${ZIP_FILE}.sha256"
  fi
fi

if [[ "${FINAL_STATUS}" == "FAIL" ]]; then
  exit 1
fi

exit 0
