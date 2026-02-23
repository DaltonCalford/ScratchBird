#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
mode="local"
profiles_csv="linux-gcc-debug,linux-clang-debug"
configure_only=0
skip_tests=0
dry_run=0

usage() {
  cat <<'USAGE'
Usage: smoke_profiles.sh [options]

Options:
  --mode <local|ci>         Execution mode (default: local)
  --profiles <csv>          Configure preset names (default: linux-gcc-debug,linux-clang-debug)
  --configure-only          Run configure only
  --skip-tests              Skip test smoke stage
  --dry-run                 Print commands without executing
  --help                    Show this message
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --mode)
      mode="$2"
      shift 2
      ;;
    --profiles)
      profiles_csv="$2"
      shift 2
      ;;
    --configure-only)
      configure_only=1
      shift
      ;;
    --skip-tests)
      skip_tests=1
      shift
      ;;
    --dry-run)
      dry_run=1
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

run_cmd() {
  local cmd="$1"
  echo "[smoke] ${cmd}"
  if [[ "${dry_run}" -eq 0 ]]; then
    eval "${cmd}"
  fi
}

IFS=',' read -r -a profiles <<< "${profiles_csv}"

for preset in "${profiles[@]}"; do
  run_cmd "cd \"${repo_root}\" && cmake --preset ${preset}"

  if [[ "${configure_only}" -eq 1 ]]; then
    continue
  fi

  run_cmd "cd \"${repo_root}\" && cmake --build --preset ${preset}-build -j4"

  if [[ "${skip_tests}" -eq 0 ]]; then
    if [[ "${mode}" == "ci" ]]; then
      run_cmd "cd \"${repo_root}\" && ctest --preset ${preset}-test -R 'PortableRuntimeGuard|SignalControlTest' --output-on-failure"
    else
      run_cmd "cd \"${repo_root}\" && ctest --preset ${preset}-test -R 'PortableRuntimeGuard' --output-on-failure"
    fi
  fi
done

echo "Smoke profile run complete."
