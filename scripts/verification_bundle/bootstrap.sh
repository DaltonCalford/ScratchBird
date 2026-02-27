#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SUITE_TEMPLATE="${SCRIPT_DIR}/suite"

WORKSPACE_ROOT="${PWD}"
TARGET_DIR_NAME="sb_verification"
INSTALL_DEPS=1
CLONE_REPOS=1
BUILD_SB=1
RUN_SB_TESTS=1
RUN_VERIFY=1
START_RUNTIME=1
CLONE_PRESET="core"
CLONE_DEPTH=1
UPDATE_EXISTING=0
REFRESH_TEMPLATE=0

usage() {
  cat <<USAGE
Usage: $(basename "$0") [options]

Options:
  --workspace-root <path>    Root directory where workspace will be created (default: current directory)
  --target-dir <name>        Workspace directory name under workspace-root (default: sb_verification)
  --no-install-deps          Skip dependency installation
  --no-clone                 Skip repository cloning
  --skip-build               Skip ScratchBird build
  --skip-sb-tests            Skip ScratchBird ctest execution
  --skip-verify              Skip verification execution
  --no-start-runtime         Skip runtime stack startup (ScratchBird static stack + reference containers)
  --clone-preset <preset>    Clone preset: core|full|scratchbird (default: core)
  --clone-depth <n>          Git clone depth (default: 1)
  --update-existing          Run git fetch on existing clones
  --refresh-template         Replace existing cases/configs/scripts/docs/ctest from bundle template
  --help                     Show this help
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --workspace-root)
      WORKSPACE_ROOT="$2"
      shift 2
      ;;
    --target-dir)
      TARGET_DIR_NAME="$2"
      shift 2
      ;;
    --no-install-deps)
      INSTALL_DEPS=0
      shift
      ;;
    --no-clone)
      CLONE_REPOS=0
      shift
      ;;
    --skip-build)
      BUILD_SB=0
      shift
      ;;
    --skip-sb-tests)
      RUN_SB_TESTS=0
      shift
      ;;
    --skip-verify)
      RUN_VERIFY=0
      shift
      ;;
    --no-start-runtime)
      START_RUNTIME=0
      shift
      ;;
    --clone-preset)
      CLONE_PRESET="$2"
      shift 2
      ;;
    --clone-depth)
      CLONE_DEPTH="$2"
      shift 2
      ;;
    --update-existing)
      UPDATE_EXISTING=1
      shift
      ;;
    --refresh-template)
      REFRESH_TEMPLATE=1
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

case "${CLONE_PRESET}" in
  core|full|scratchbird) ;;
  *)
    echo "Invalid --clone-preset: ${CLONE_PRESET}" >&2
    exit 1
    ;;
esac

TARGET_DIR="$(cd "${WORKSPACE_ROOT}" && pwd)/${TARGET_DIR_NAME}"
mkdir -p "${TARGET_DIR}"

copy_template_dir() {
  local name="$1"
  local src="${SUITE_TEMPLATE}/${name}"
  local dst="${TARGET_DIR}/${name}"
  if [[ ! -e "${src}" ]]; then
    return 0
  fi
  if [[ -e "${dst}" && "${REFRESH_TEMPLATE}" -eq 0 ]]; then
    return 0
  fi
  rm -rf "${dst}"
  cp -a "${src}" "${dst}"
}

for d in cases configs ctest docs scripts; do
  copy_template_dir "$d"
done
mkdir -p "${TARGET_DIR}/results" "${TARGET_DIR}/reports"

echo "[verification-bundle] workspace: ${TARGET_DIR}"

cd "${TARGET_DIR}"

if [[ "${INSTALL_DEPS}" -eq 1 ]]; then
  ./scripts/bootstrap_install_linux.sh --yes
fi

./scripts/bootstrap_prepare_workspace.sh
./scripts/bootstrap_python_env.sh

if [[ "${CLONE_REPOS}" -eq 1 ]]; then
  CLONE_ARGS=(
    --config configs/repositories.yaml
    --preset "${CLONE_PRESET}"
    --depth "${CLONE_DEPTH}"
  )
  if [[ "${UPDATE_EXISTING}" -eq 1 ]]; then
    CLONE_ARGS+=(--update-existing)
  fi
  ./.venv/bin/python scripts/bootstrap_clone_repos.py "${CLONE_ARGS[@]}"
fi

if [[ "${BUILD_SB}" -eq 1 ]]; then
  BUILD_ARGS=()
  if [[ "${RUN_SB_TESTS}" -eq 0 ]]; then
    BUILD_ARGS+=(--skip-tests)
  fi
  ./scripts/bootstrap_build_scratchbird.sh "${BUILD_ARGS[@]}"
fi

if [[ "${START_RUNTIME}" -eq 1 ]]; then
  ./scripts/bootstrap_runtime_stack.sh up
fi

if [[ "${RUN_VERIFY}" -eq 1 ]]; then
  ./scripts/run_full_verification.sh
fi

echo
echo "[verification-bundle] done"
echo "Workspace: ${TARGET_DIR}"
echo "Reports:   ${TARGET_DIR}/reports"
echo "Results:   ${TARGET_DIR}/results"
