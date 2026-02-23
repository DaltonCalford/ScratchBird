#!/usr/bin/env bash
set -euo pipefail

repo_root="${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)}"
prefix="${2:-${repo_root}/build/deps/mingw-openssl}"
work_dir="${repo_root}/build/deps/src"
openssl_version="${OPENSSL_VERSION:-3.3.2}"
archive="openssl-${openssl_version}.tar.gz"
source_dir="${work_dir}/openssl-${openssl_version}"

mkdir -p "${work_dir}"
mkdir -p "${prefix}"

if [[ ! -f "${work_dir}/${archive}" ]]; then
    major_minor="${openssl_version%.*}"
    urls=(
        "https://www.openssl.org/source/${archive}"
        "https://www.openssl.org/source/old/${major_minor}/${archive}"
    )
    downloaded=0
    for url in "${urls[@]}"; do
        echo "[bootstrap_mingw_openssl] downloading ${url}"
        if curl -fsSL "${url}" -o "${work_dir}/${archive}"; then
            downloaded=1
            break
        fi
    done
    if [[ "${downloaded}" -ne 1 ]]; then
        echo "[bootstrap_mingw_openssl] failed to download ${archive}" >&2
        exit 1
    fi
fi

if [[ ! -d "${source_dir}" ]]; then
    echo "[bootstrap_mingw_openssl] extracting ${archive}"
    tar -xzf "${work_dir}/${archive}" -C "${work_dir}"
fi

if [[ -f "${prefix}/lib/libcrypto.a" && -f "${prefix}/lib/libssl.a" ]]; then
    echo "[bootstrap_mingw_openssl] existing install detected at ${prefix}"
    echo "[bootstrap_mingw_openssl] export SCRATCHBIRD_MINGW_OPENSSL_ROOT=${prefix}"
    exit 0
fi

pushd "${source_dir}" >/dev/null

echo "[bootstrap_mingw_openssl] configuring cross build"
perl Configure mingw64 \
    no-shared \
    no-tests \
    --cross-compile-prefix=x86_64-w64-mingw32- \
    --prefix="${prefix}"

echo "[bootstrap_mingw_openssl] building and installing"
make -j"$(nproc)"
make install_sw

popd >/dev/null

echo "[bootstrap_mingw_openssl] installed to ${prefix}"
echo "[bootstrap_mingw_openssl] export SCRATCHBIRD_MINGW_OPENSSL_ROOT=${prefix}"
