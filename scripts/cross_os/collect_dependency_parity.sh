#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
output_csv="${1:-${repo_root}/artifacts/cross_os/p6s2w1/xos-035-dependency-parity.csv}"

mkdir -p "$(dirname "${output_csv}")"

profiles=(
  "linux-gcc-debug:${repo_root}/build/linux-gcc-debug/CMakeCache.txt"
  "linux-clang-debug:${repo_root}/build/linux-clang-debug/CMakeCache.txt"
  "linux-mingw-windows-x64:${repo_root}/build/linux-mingw-windows-x64/CMakeCache.txt"
  "windows-msvc-debug:${repo_root}/build/windows-msvc-debug/CMakeCache.txt"
)

deps=(
  "OpenSSL:OPENSSL_FOUND|OPENSSL_CRYPTO_LIBRARY:OPENSSL_VERSION|OPENSSL_VERSION_STR:OPENSSL_INCLUDE_DIR|OPENSSL_INCLUDE_DIRS:OPENSSL_CRYPTO_LIBRARY|OPENSSL_SSL_LIBRARY"
  "ZLIB:ZLIB_FOUND|ZLIB_LIBRARY_RELEASE:ZLIB_VERSION_STRING|ZLIB_VERSION:ZLIB_INCLUDE_DIR|ZLIB_INCLUDE_DIRS:ZLIB_LIBRARY|ZLIB_LIBRARY_RELEASE"
  "LZ4:LZ4_FOUND|LZ4_LIBRARIES:LZ4_VERSION:LZ4_INCLUDE_DIRS:LZ4_LIBRARIES"
  "ZSTD:ZSTD_FOUND|ZSTD_LIBRARIES:ZSTD_VERSION|ZSTD_VERSION_STRING:ZSTD_INCLUDE_DIRS:ZSTD_LIBRARIES"
  "GEOS:GEOS_FOUND|GEOS_LIBRARIES:GEOS_VERSION:GEOS_INCLUDE_DIRS:GEOS_LIBRARIES"
  "PROJ:PROJ_FOUND|PROJ_LIBRARIES:PROJ_VERSION:PROJ_INCLUDE_DIRS:PROJ_LIBRARIES"
  "LIBXML2:LIBXML2_FOUND|LIBXML2_LIBRARIES:LIBXML2_VERSION:LIBXML2_INCLUDE_DIRS:LIBXML2_LIBRARIES"
)

cache_get() {
  local cache_file="$1"
  local key="$2"
  if [[ ! -f "${cache_file}" ]]; then
    return 0
  fi
  awk -F= -v k="${key}" '$1 ~ ("^" k ":") {print $2; exit}' "${cache_file}"
}

cache_get_first() {
  local cache_file="$1"
  local keys_csv="$2"
  IFS='|' read -r -a keys <<< "${keys_csv}"
  for key in "${keys[@]}"; do
    val="$(cache_get "${cache_file}" "${key}")"
    if [[ -n "${val}" ]]; then
      echo "${val}"
      return 0
    fi
  done
}

{
  echo "profile,dependency,found,version,include_path,library_path,cache_file"
  for profile_entry in "${profiles[@]}"; do
    profile="${profile_entry%%:*}"
    cache="${profile_entry#*:}"

    if [[ ! -f "${cache}" ]]; then
      for dep in "${deps[@]}"; do
        dep_name="${dep%%:*}"
        echo "${profile},${dep_name},cache_missing,,,,${cache}"
      done
      continue
    fi

    for dep in "${deps[@]}"; do
      IFS=":" read -r dep_name found_key version_key include_key lib_key <<< "${dep}"
      found_val="$(cache_get_first "${cache}" "${found_key}")"
      version_val="$(cache_get_first "${cache}" "${version_key}")"
      include_val="$(cache_get_first "${cache}" "${include_key}")"
      lib_val="$(cache_get_first "${cache}" "${lib_key}")"

      if [[ -z "${found_val}" ]]; then
        if [[ -n "${lib_val}" && "${lib_val}" != *"NOTFOUND"* ]]; then
          found_val="1"
        else
          found_val="0"
        fi
      elif [[ "${found_val}" == "TRUE" ]]; then
        found_val="1"
      elif [[ "${found_val}" == "FALSE" ]]; then
        found_val="0"
      elif [[ "${found_val}" == *"NOTFOUND"* ]]; then
        found_val="0"
      elif [[ "${found_val}" != "0" && "${found_val}" != "1" ]]; then
        # Treat resolved path/token as found for CSV parity reporting.
        found_val="1"
      fi

      # Normalize list separators for CSV readability.
      include_val="${include_val//;/|}"
      lib_val="${lib_val//;/|}"

      echo "${profile},${dep_name},${found_val},${version_val},${include_val},${lib_val},${cache}"
    done
  done
} > "${output_csv}"

echo "Wrote dependency parity CSV: ${output_csv}"
