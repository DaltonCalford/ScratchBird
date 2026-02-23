#!/usr/bin/env bash
set -euo pipefail

repo_root="${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)}"
prefix="${2:-${repo_root}/build/deps/mingw-zlib}"
work_dir="${repo_root}/build/deps/src"
zlib_version="${ZLIB_VERSION:-1.3.1}"
zlib_archive="zlib-${zlib_version}.tar.gz"
source_dir="${work_dir}/zlib-${zlib_version}"
build_dir="${repo_root}/build/deps/zlib-mingw-build"
toolchain_file="${repo_root}/cmake/toolchains/mingw-w64-x86_64.cmake"

mkdir -p "${work_dir}"
mkdir -p "${prefix}"

if [[ ! -f "${work_dir}/${zlib_archive}" ]]; then
    urls=(
        "https://zlib.net/${zlib_archive}"
        "https://www.zlib.net/${zlib_archive}"
        "https://www.zlib.net/fossils/${zlib_archive}"
    )
    downloaded=0
    for url in "${urls[@]}"; do
        echo "[bootstrap_mingw_zlib] downloading ${url}"
        if curl -fsSL "${url}" -o "${work_dir}/${zlib_archive}"; then
            downloaded=1
            break
        fi
    done
    if [[ "${downloaded}" -ne 1 ]]; then
        echo "[bootstrap_mingw_zlib] failed to download ${zlib_archive}" >&2
        exit 1
    fi
fi

if [[ ! -d "${source_dir}" ]]; then
    echo "[bootstrap_mingw_zlib] extracting ${zlib_archive}"
    tar -xzf "${work_dir}/${zlib_archive}" -C "${work_dir}"
fi

echo "[bootstrap_mingw_zlib] configuring cross build"
cmake -S "${source_dir}" -B "${build_dir}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="${toolchain_file}" \
    -DCMAKE_INSTALL_PREFIX="${prefix}" \
    -DBUILD_SHARED_LIBS=OFF

echo "[bootstrap_mingw_zlib] building and installing"
cmake --build "${build_dir}" -j"$(nproc)"
cmake --install "${build_dir}"

echo "[bootstrap_mingw_zlib] installed to ${prefix}"
echo "[bootstrap_mingw_zlib] export SCRATCHBIRD_MINGW_ZLIB_ROOT=${prefix}"
