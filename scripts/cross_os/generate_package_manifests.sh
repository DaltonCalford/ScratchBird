#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
linux_build_dir="${repo_root}/build/linux-gcc-debug"
windows_build_dir="${repo_root}/build/linux-mingw-windows-x64"
out_dir="${repo_root}/artifacts/cross_os/p6s3w2/package_manifests"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --linux-build-dir)
      linux_build_dir="$2"
      shift 2
      ;;
    --windows-build-dir)
      windows_build_dir="$2"
      shift 2
      ;;
    --out-dir)
      out_dir="$2"
      shift 2
      ;;
    *)
      echo "Unknown argument: $1" >&2
      exit 2
      ;;
  esac
done

mkdir -p "${out_dir}"

relpath() {
  local path="$1"
  realpath --relative-to="${repo_root}" "${path}"
}

write_or_placeholder() {
  local file="$1"
  shift
  if [[ "$#" -eq 0 ]]; then
    printf "NONE\n" > "${file}"
    return
  fi
  printf "%s\n" "$@" | sort -u > "${file}"
}

collect_linux_runtime=()
collect_linux_qa=()
collect_windows_runtime=()
collect_windows_qa=()

if [[ -d "${linux_build_dir}" ]]; then
  while IFS= read -r -d '' f; do
    rel="$(relpath "${f}")"
    case "${rel}" in
      */tests/*|*/CMakeFiles/*|*/_deps/*)
        ;;
      */src/*|*/bin/*|*/scratchbird_server|*/sb_listener|*/sb_manager)
        collect_linux_runtime+=("${rel}")
        ;;
      *)
        ;;
    esac
  done < <(find "${linux_build_dir}" -type f -perm -u+x -print0)

  while IFS= read -r -d '' f; do
    collect_linux_qa+=("$(relpath "${f}")")
  done < <(find "${linux_build_dir}/tests" -type f -perm -u+x -print0 2>/dev/null || true)
fi

if [[ -d "${windows_build_dir}" ]]; then
  while IFS= read -r -d '' f; do
    rel="$(relpath "${f}")"
    case "${rel}" in
      */tests/*|*/CMakeFiles/*|*/_deps/*)
        ;;
      *)
        collect_windows_runtime+=("${rel}")
        ;;
    esac
  done < <(find "${windows_build_dir}" -type f \( -name "*.exe" -o -name "*.dll" \) -print0)

  while IFS= read -r -d '' f; do
    collect_windows_qa+=("$(relpath "${f}")")
  done < <(find "${windows_build_dir}/tests" -type f \( -name "*.exe" -o -name "*.dll" \) -print0 2>/dev/null || true)
fi

write_or_placeholder "${out_dir}/linux-runtime-manifest.txt" "${collect_linux_runtime[@]:-}"
write_or_placeholder "${out_dir}/linux-qa-manifest.txt" "${collect_linux_qa[@]:-}"
write_or_placeholder "${out_dir}/windows-runtime-manifest.txt" "${collect_windows_runtime[@]:-}"
write_or_placeholder "${out_dir}/windows-qa-manifest.txt" "${collect_windows_qa[@]:-}"

cat > "${out_dir}/README.md" <<EOF
# Cross-OS Package Manifests
Last-Modified: $(date +%Y-%m-%d)

- linux-runtime-manifest.txt: runtime binaries for Linux package.
- linux-qa-manifest.txt: Linux QA/test binaries.
- windows-runtime-manifest.txt: runtime binaries for Windows package.
- windows-qa-manifest.txt: Windows QA/test binaries.
EOF

echo "Wrote manifests in ${out_dir}"
