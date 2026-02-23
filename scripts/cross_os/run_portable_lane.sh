#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
lane="portable"
test_preset="linux-gcc-debug-test"
build_preset=""
skip_build=0
linux_only_name_regex="UnixSocketTest\\.|TSAN_"

usage() {
  cat <<'USAGE'
Usage: run_portable_lane.sh [options]

Options:
  --lane <portable|windows_portable|linux_only|full_linux|performance_sample>
  --test-preset <preset>     CTest preset name (default: linux-gcc-debug-test)
  --build-preset <preset>    Optional CMake build preset to run before ctest
  --skip-build               Skip CMake build step
  --help                     Show this message
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --lane)
      lane="$2"
      shift 2
      ;;
    --test-preset)
      test_preset="$2"
      shift 2
      ;;
    --build-preset)
      build_preset="$2"
      shift 2
      ;;
    --skip-build)
      skip_build=1
      shift
      ;;
    --help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown argument: $1" >&2
      usage
      exit 2
      ;;
  esac
done

if [[ "${skip_build}" -eq 0 && -n "${build_preset}" ]]; then
  cmake --build --preset "${build_preset}" --parallel
fi

case "${lane}" in
  portable)
    ctest --preset "${test_preset}" -L "smoke|unit|integration" -E "quarantine|${linux_only_name_regex}" -LE "linux_only|disabled" --output-on-failure
    ;;
  windows_portable)
    ctest --preset "${test_preset}" -L "smoke|unit|integration" -E "quarantine|${linux_only_name_regex}" -LE "linux_only|disabled|stress|performance|tsan" --output-on-failure
    ;;
  linux_only)
    ctest --preset "${test_preset}" -R "${linux_only_name_regex}" --output-on-failure
    ;;
  full_linux)
    ctest --preset "${test_preset}" -E "quarantine" --output-on-failure
    ;;
  performance_sample)
    ctest --preset "${test_preset}" -L "performance" --output-on-failure
    ;;
  *)
    echo "Unsupported lane: ${lane}" >&2
    usage
    exit 2
    ;;
esac

echo "Lane completed: ${lane} (preset=${test_preset})"
