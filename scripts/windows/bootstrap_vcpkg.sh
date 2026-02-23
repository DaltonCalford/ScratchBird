#!/usr/bin/env bash
set -euo pipefail

repo_root="${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)}"
vcpkg_root="${2:-${repo_root}/build/vcpkg}"
baseline="${3:-05442024c3fda64320bd25d2251cc9807b84fb6f}"
triplet="${4:-x64-windows}"

echo "Repo root: ${repo_root}"
echo "Vcpkg root: ${vcpkg_root}"
echo "Baseline : ${baseline}"
echo "Triplet  : ${triplet}"

if [[ ! -d "${vcpkg_root}" ]]; then
    git clone https://github.com/microsoft/vcpkg.git "${vcpkg_root}"
fi

git -C "${vcpkg_root}" fetch --all --tags
git -C "${vcpkg_root}" checkout "${baseline}"
"${vcpkg_root}/bootstrap-vcpkg.sh" -disableMetrics

export VCPKG_ROOT="${vcpkg_root}"
"${vcpkg_root}/vcpkg" install \
    --x-manifest-root="${repo_root}" \
    --triplet "${triplet}" \
    --x-feature=manifests

echo "vcpkg bootstrap complete."
echo "Next: cmake --preset windows-msvc-debug"
