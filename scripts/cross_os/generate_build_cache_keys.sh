#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
output_md="${1:-${repo_root}/artifacts/cross_os/p6s2w1/xos-036-cache-keys.md}"

mkdir -p "$(dirname "${output_md}")"

hash_file() {
  local file="$1"
  if [[ -f "${file}" ]]; then
    sha256sum "${file}" | awk '{print $1}'
  else
    echo "missing"
  fi
}

lock_inputs=(
  "${repo_root}/vcpkg.json"
  "${repo_root}/vcpkg-configuration.json"
  "${repo_root}/CMakePresets.json"
  "${repo_root}/CMakeLists.txt"
  "${repo_root}/src/CMakeLists.txt"
  "${repo_root}/cmake/toolchains/mingw-w64-x86_64.cmake"
)

tmp_lock_material="$(mktemp)"
trap 'rm -f "${tmp_lock_material}"' EXIT

for f in "${lock_inputs[@]}"; do
  printf "%s %s\n" "$(hash_file "${f}")" "${f#${repo_root}/}" >> "${tmp_lock_material}"
done

dependency_lock_hash="$(sha256sum "${tmp_lock_material}" | awk '{print $1}')"

profiles=(
  "linux-gcc-debug"
  "linux-clang-debug"
  "linux-mingw-windows-x64"
  "windows-msvc-debug"
)

{
  echo "# XOS-036 Build Cache Keys"
  echo
  echo "dependency_lock_hash: \`${dependency_lock_hash}\`"
  echo
  echo "| profile | cache_key |"
  echo "|---|---|"
  for profile in "${profiles[@]}"; do
    key_material="${profile}:${dependency_lock_hash}"
    profile_hash="$(printf "%s" "${key_material}" | sha256sum | awk '{print $1}')"
    cache_key="scratchbird-${profile}-${profile_hash:0:16}"
    echo "| \`${profile}\` | \`${cache_key}\` |"
  done
  echo
  echo "## Lock inputs"
  for f in "${lock_inputs[@]}"; do
    echo "- \`${f#${repo_root}/}\`: \`$(hash_file "${f}")\`"
  done
} > "${output_md}"

echo "Wrote cache key document: ${output_md}"
