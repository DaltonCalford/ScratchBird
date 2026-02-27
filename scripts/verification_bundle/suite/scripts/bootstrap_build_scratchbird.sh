#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
REPO_ROOT="${SB_VERIFY_REPO_ROOT:-${WORKSPACE_ROOT}/repos}"
SCRATCHBIRD_REPO="${REPO_ROOT}/ScratchBird"
BUILD_TYPE="Release"
RUN_TESTS=1
JOBS="$(nproc)"

usage() {
  cat <<USAGE
Usage: $(basename "$0") [options]

Options:
  --repo-root <path>          Clone root (default: SB_VERIFY_REPO_ROOT or <workspace>/repos)
  --scratchbird-repo <path>   ScratchBird repo path (default: <repo-root>/ScratchBird)
  --build-type <type>         CMake build type (default: Release)
  --jobs <n>                  Build parallelism (default: nproc)
  --skip-tests                Skip ctest
  --help                      Show help
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --repo-root)
      REPO_ROOT="$2"
      shift 2
      ;;
    --scratchbird-repo)
      SCRATCHBIRD_REPO="$2"
      shift 2
      ;;
    --build-type)
      BUILD_TYPE="$2"
      shift 2
      ;;
    --jobs)
      JOBS="$2"
      shift 2
      ;;
    --skip-tests)
      RUN_TESTS=0
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

if [[ ! -d "${SCRATCHBIRD_REPO}" ]]; then
  echo "ScratchBird repository not found: ${SCRATCHBIRD_REPO}" >&2
  exit 1
fi

BUILD_DIR="${SCRATCHBIRD_REPO}/build"

echo "[build] repo: ${SCRATCHBIRD_REPO}"
echo "[build] dir:  ${BUILD_DIR}"

cmake -S "${SCRATCHBIRD_REPO}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
cmake --build "${BUILD_DIR}" -j"${JOBS}"

if [[ "${RUN_TESTS}" -eq 1 ]]; then
  ctest --test-dir "${BUILD_DIR}" --output-on-failure
fi

echo "ScratchBird build bootstrap complete."
