#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

BUILD_DIR="${BUILD_DIR:-${REPO_ROOT}/build}"
RELEASE_DIR="${RELEASE_DIR:-${REPO_ROOT}/release}"
BETA_DIR="${BETA_DIR:-${RELEASE_DIR}/beta}"
STAMP="${STAMP:-$(date -u +%Y%m%d)}"

BETA_BIN_DIR="${BETA_DIR}/bin"
BETA_TESTS_DIR="${BETA_DIR}/tests"
RUNTIME_PKG_DIR="${BETA_DIR}/packages/runtime-only"
RUNTIME_PKG_BIN_DIR="${RUNTIME_PKG_DIR}/bin"
QA_PKG_DIR="${BETA_DIR}/packages/qa"
QA_PKG_BIN_DIR="${QA_PKG_DIR}/bin"
QA_PKG_TESTS_DIR="${QA_PKG_DIR}/tests"

TARBALL_NAME="scratchbird-beta-${STAMP}-full.tar.gz"
TARBALL_PATH="${RELEASE_DIR}/${TARBALL_NAME}"
TARBALL_SHA_PATH="${TARBALL_PATH}.sha256"

runtime_bins=(
    "scratchbird"
    "sb_server"
    "sb_listener_native"
    "sb_listener_pg"
    "sb_listener_mysql"
    "sb_listener_fb"
    "sb_parser_native"
    "sb_parser_pg"
    "sb_parser_mysql"
    "sb_parser_fb"
    "sb_charset_loader"
    "sb_timezone_loader"
)

runtime_src_for() {
    local bin="${1}"
    case "${bin}" in
        sb_timezone_loader)
            printf "%s" "${BUILD_DIR}/tools/${bin}"
            ;;
        *)
            printf "%s" "${BUILD_DIR}/src/${bin}"
            ;;
    esac
}

require_file() {
    local path="${1}"
    if [[ ! -f "${path}" ]]; then
        echo "error: required file not found: ${path}" >&2
        exit 2
    fi
}

prepare_dir() {
    local dir="${1}"
    mkdir -p "${dir}"
    find "${dir}" -mindepth 1 -maxdepth 1 -type f -delete
}

prepare_dir "${BETA_BIN_DIR}"
prepare_dir "${BETA_TESTS_DIR}"
prepare_dir "${RUNTIME_PKG_BIN_DIR}"
prepare_dir "${QA_PKG_BIN_DIR}"
prepare_dir "${QA_PKG_TESTS_DIR}"

for bin in "${runtime_bins[@]}"; do
    src="$(runtime_src_for "${bin}")"
    require_file "${src}"
    install -m 0755 "${src}" "${BETA_BIN_DIR}/${bin}"
    install -m 0755 "${src}" "${RUNTIME_PKG_BIN_DIR}/${bin}"
    install -m 0755 "${src}" "${QA_PKG_BIN_DIR}/${bin}"
done

mapfile -t test_bins < <(find "${BUILD_DIR}/tests" -maxdepth 1 -type f -executable -printf '%f\n' | LC_ALL=C sort)
if [[ "${#test_bins[@]}" -eq 0 ]]; then
    echo "error: no executable test binaries found under ${BUILD_DIR}/tests" >&2
    exit 2
fi

for bin in "${test_bins[@]}"; do
    src="${BUILD_DIR}/tests/${bin}"
    install -m 0755 "${src}" "${BETA_TESTS_DIR}/${bin}"
    install -m 0755 "${src}" "${QA_PKG_TESTS_DIR}/${bin}"
done

(
    cd "${BETA_DIR}"
    sha256sum bin/* > SHA256SUMS
    sha256sum tests/* > SHA256SUMS.tests
)

(
    cd "${RUNTIME_PKG_DIR}"
    sha256sum bin/* > SHA256SUMS
)

(
    cd "${QA_PKG_DIR}"
    sha256sum bin/* tests/* > SHA256SUMS
)

tar -C "${RELEASE_DIR}" -czf "${TARBALL_PATH}" "beta"
(
    cd "${RELEASE_DIR}"
    sha256sum "${TARBALL_NAME}" > "$(basename "${TARBALL_SHA_PATH}")"
)

runtime_count="${#runtime_bins[@]}"
test_count="${#test_bins[@]}"
tarball_size_bytes="$(stat -c '%s' "${TARBALL_PATH}")"

cat > "${RELEASE_DIR}/BETA_RELEASE_MANIFEST_${STAMP}.md" <<EOF
# Beta Release Packaging Manifest
Last modified: $(date -u +%Y-%m-%d)

## Inputs

- Build directory: \`build/\`
- Beta staging directory: \`release/beta/\`

## Outputs

- Runtime binaries staged: \`${runtime_count}\` under \`release/beta/bin/\`
- Test binaries staged: \`${test_count}\` under \`release/beta/tests/\`
- Runtime-only package: \`release/beta/packages/runtime-only/\`
- QA package: \`release/beta/packages/qa/\`
- Full beta tarball: \`release/${TARBALL_NAME}\`
- Tarball SHA-256: \`release/$(basename "${TARBALL_SHA_PATH}")\`

## Checksums

- \`release/beta/SHA256SUMS\`
- \`release/beta/SHA256SUMS.tests\`
- \`release/beta/packages/runtime-only/SHA256SUMS\`
- \`release/beta/packages/qa/SHA256SUMS\`

## Tarball Size

- Bytes: \`${tarball_size_bytes}\`
EOF

echo "runtime binaries: ${runtime_count}"
echo "test binaries: ${test_count}"
echo "tarball: ${TARBALL_PATH}"
echo "tarball sha: ${TARBALL_SHA_PATH}"
echo "manifest: ${RELEASE_DIR}/BETA_RELEASE_MANIFEST_${STAMP}.md"

