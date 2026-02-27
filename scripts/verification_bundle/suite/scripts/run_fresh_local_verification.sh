#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
PROJECT_ROOT="$(cd "${WORKSPACE_ROOT}/../../.." && pwd)"
DEFAULT_REPO_ROOT="${WORKSPACE_ROOT}/repos"
REPO_ROOT="${SB_VERIFY_REPO_ROOT:-}"
PYTHON_BIN="${WORKSPACE_ROOT}/.venv/bin/python"

RESET_RUNTIME=1
RUN_REBUILD=0
RUN_SB_TESTS=0
RUN_WIRE=1
CLEAN_PRIOR=1

usage() {
  cat <<USAGE
Usage: $(basename "$0") [options]

Assumes dependencies and repositories are already present.
Performs a clean local verification pass by archiving prior artifacts,
optionally rebuilding ScratchBird, resetting runtime stack, then executing verification.

Options:
  --workspace-root <path>   Verification workspace root (default: script parent)
  --repo-root <path>        Clone root (default: SB_VERIFY_REPO_ROOT, then auto-detect local clone root)
  --python <path>           Python interpreter path (default: <workspace>/.venv/bin/python or python3)
  --skip-runtime-reset      Do not restart runtime stack before verification
  --rebuild                 Rebuild ScratchBird before verification
  --run-sb-tests            Run ctest during rebuild (implies --rebuild)
  --skip-wire               Skip wire capture lane
  --no-clean                Do not archive/clear prior verification artifacts
  --help                    Show help
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --workspace-root)
      WORKSPACE_ROOT="$2"
      shift 2
      ;;
    --repo-root)
      REPO_ROOT="$2"
      shift 2
      ;;
    --python)
      PYTHON_BIN="$2"
      shift 2
      ;;
    --skip-runtime-reset)
      RESET_RUNTIME=0
      shift
      ;;
    --rebuild)
      RUN_REBUILD=1
      shift
      ;;
    --run-sb-tests)
      RUN_REBUILD=1
      RUN_SB_TESTS=1
      shift
      ;;
    --skip-wire)
      RUN_WIRE=0
      shift
      ;;
    --no-clean)
      CLEAN_PRIOR=0
      shift
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

detect_repo_root() {
  local candidate
  local cliwork_root
  cliwork_root="$(dirname "${PROJECT_ROOT}")"

  # Prefer verification workspace clone root when present.
  candidate="${DEFAULT_REPO_ROOT}"
  if [[ -d "${candidate}/ScratchBird" ]]; then
    printf '%s\n' "${candidate}"
    return 0
  fi

  # Common local developer layout: ~/CliWork/{ScratchBird,mysql-server,...}
  candidate="${cliwork_root}"
  if [[ -d "${candidate}/ScratchBird" ]]; then
    printf '%s\n' "${candidate}"
    return 0
  fi

  # Fallback to current repository parent.
  candidate="$(dirname "${PROJECT_ROOT}")"
  if [[ -d "${candidate}/ScratchBird" ]]; then
    printf '%s\n' "${candidate}"
    return 0
  fi

  printf '%s\n' "${DEFAULT_REPO_ROOT}"
}

if [[ -z "${REPO_ROOT}" ]]; then
  REPO_ROOT="$(detect_repo_root)"
fi

if [[ ! -d "${REPO_ROOT}/ScratchBird" ]]; then
  echo "Repository root does not contain ScratchBird: ${REPO_ROOT}" >&2
  echo "Set --repo-root <path> or SB_VERIFY_REPO_ROOT to your local clone root." >&2
  exit 1
fi

if [[ ! -x "${PYTHON_BIN}" ]]; then
  PYTHON_BIN="$(command -v python3)"
fi

RUN_ID="$(date -u +%Y%m%dT%H%M%SZ)"
ARCHIVE_ROOT="${WORKSPACE_ROOT}/reports/archive/fresh_local_${RUN_ID}"
SUMMARY_FILE="${WORKSPACE_ROOT}/reports/fresh_local_summary_${RUN_ID}.md"

archive_path() {
  local path="$1"
  local rel
  rel="${path#${WORKSPACE_ROOT}/}"
  if [[ -e "${path}" ]]; then
    mkdir -p "${ARCHIVE_ROOT}/$(dirname "${rel}")"
    mv "${path}" "${ARCHIVE_ROOT}/${rel}"
  fi
}

archive_glob() {
  local pattern="$1"
  local match=0
  local f
  shopt -s nullglob
  for f in ${pattern}; do
    match=1
    archive_path "${f}"
  done
  shopt -u nullglob
  return 0
}

mkdir -p "${WORKSPACE_ROOT}/reports" "${WORKSPACE_ROOT}/results"

if [[ "${CLEAN_PRIOR}" -eq 1 ]]; then
  mkdir -p "${ARCHIVE_ROOT}"
  archive_path "${WORKSPACE_ROOT}/results/diff"
  archive_path "${WORKSPACE_ROOT}/results/perf"
  archive_path "${WORKSPACE_ROOT}/results/wire"
  archive_glob "${WORKSPACE_ROOT}/reports/verification_manifest_*.json"
  archive_glob "${WORKSPACE_ROOT}/reports/verification_index_*.md"
  archive_glob "${WORKSPACE_ROOT}/reports/footprint_*.csv"
  archive_glob "${WORKSPACE_ROOT}/reports/footprint_snapshot.json"
fi

if [[ "${RUN_REBUILD}" -eq 1 ]]; then
  BUILD_ARGS=(--repo-root "${REPO_ROOT}")
  if [[ "${RUN_SB_TESTS}" -eq 0 ]]; then
    BUILD_ARGS+=(--skip-tests)
  fi
  "${WORKSPACE_ROOT}/scripts/bootstrap_build_scratchbird.sh" "${BUILD_ARGS[@]}"
fi

if [[ "${RESET_RUNTIME}" -eq 1 ]]; then
  "${WORKSPACE_ROOT}/scripts/bootstrap_runtime_stack.sh" down || true
  "${WORKSPACE_ROOT}/scripts/bootstrap_runtime_stack.sh" up
fi

VERIFY_ARGS=(
  --workspace-root "${WORKSPACE_ROOT}"
  --repo-root "${REPO_ROOT}"
  --python "${PYTHON_BIN}"
)
if [[ "${RUN_WIRE}" -eq 0 ]]; then
  VERIFY_ARGS+=(--skip-wire)
fi
"${WORKSPACE_ROOT}/scripts/run_full_verification.sh" "${VERIFY_ARGS[@]}"

LATEST_MANIFEST="$(ls -1t "${WORKSPACE_ROOT}"/reports/verification_manifest_*.json 2>/dev/null | head -n1 || true)"
LATEST_INDEX="$(ls -1t "${WORKSPACE_ROOT}"/reports/verification_index_*.md 2>/dev/null | head -n1 || true)"

cat > "${SUMMARY_FILE}" <<MD
# Fresh Local Verification Summary ${RUN_ID}

- Workspace: `${WORKSPACE_ROOT}`
- Repo root: `${REPO_ROOT}`
- Python: `${PYTHON_BIN}`
- Cleaned prior artifacts: `${CLEAN_PRIOR}`
- Runtime reset performed: `${RESET_RUNTIME}`
- Rebuild performed: `${RUN_REBUILD}`
- ctest during rebuild: `${RUN_SB_TESTS}`
- Wire lane executed: `${RUN_WIRE}`

## Artifacts

- Archive root: `${ARCHIVE_ROOT}`
- Latest verification manifest: `${LATEST_MANIFEST}`
- Latest verification index: `${LATEST_INDEX}`
MD

echo "Fresh local verification complete."
echo "Summary: ${SUMMARY_FILE}"
if [[ "${CLEAN_PRIOR}" -eq 1 ]]; then
  echo "Archive: ${ARCHIVE_ROOT}"
fi
