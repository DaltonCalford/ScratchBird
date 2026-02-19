#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

BUILD_DIR="${BUILD_DIR:-${REPO_ROOT}/build}"
OUT_DIR="${OUT_DIR:-${REPO_ROOT}/docs/planning/gates/BETA-GATE-001}"
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 8)}"

RUN_DATE="$(date -u +%Y-%m-%d)"
RUN_STAMP="$(date -u +%Y%m%dT%H%M%SZ)"
LOG_FILE="${OUT_DIR}/BETA_GATE_001_RUN_${RUN_STAMP}.log"
CTEST_LOG="${OUT_DIR}/BETA_GATE_001_CTEST_${RUN_STAMP}.log"
MANIFEST_FILE="${OUT_DIR}/BETA_GATE_001_MANIFEST_${RUN_STAMP}.md"
SUMMARY_ENV="${OUT_DIR}/BETA_GATE_001_SUMMARY_${RUN_STAMP}.env"

mkdir -p "${OUT_DIR}"

log() {
    printf "%s\n" "$*" | tee -a "${LOG_FILE}"
}

run_cmd() {
    log ""
    log "+ $*"
    "$@" 2>&1 | tee -a "${LOG_FILE}"
}

log "# ScratchBird BETA-GATE-001 run"
log "# run_utc=${RUN_STAMP}"
log "# repo_root=${REPO_ROOT}"
log "# build_dir=${BUILD_DIR}"
log "# jobs=${JOBS}"

start_epoch="$(date +%s)"

run_cmd rm -rf "${BUILD_DIR}"
run_cmd cmake -S "${REPO_ROOT}" -B "${BUILD_DIR}"
run_cmd cmake --build "${BUILD_DIR}" -j "${JOBS}"

log ""
log "+ ctest --test-dir ${BUILD_DIR} --output-on-failure"
set +e
ctest --test-dir "${BUILD_DIR}" --output-on-failure 2>&1 | tee "${CTEST_LOG}" | tee -a "${LOG_FILE}"
ctest_status="${PIPESTATUS[0]}"
set -e

end_epoch="$(date +%s)"
wall_seconds="$((end_epoch - start_epoch))"

summary_line="$(rg "tests passed, .*failed out of" "${CTEST_LOG}" | tail -n 1 || true)"
if [[ -z "${summary_line}" ]]; then
    summary_line="(summary line not found)"
fi

passed="0"
failed="0"
total="0"
if [[ "${summary_line}" =~ ([0-9]+)%[[:space:]]+tests[[:space:]]+passed,[[:space:]]+([0-9]+)[[:space:]]+tests[[:space:]]+failed[[:space:]]+out[[:space:]]+of[[:space:]]+([0-9]+) ]]; then
    failed="${BASH_REMATCH[2]}"
    total="${BASH_REMATCH[3]}"
    passed="$((total - failed))"
elif [[ "${summary_line}" =~ ([0-9]+)[[:space:]]+tests[[:space:]]+passed,[[:space:]]+([0-9]+)[[:space:]]+tests[[:space:]]+failed[[:space:]]+out[[:space:]]+of[[:space:]]+([0-9]+) ]]; then
    passed="${BASH_REMATCH[1]}"
    failed="${BASH_REMATCH[2]}"
    total="${BASH_REMATCH[3]}"
fi

test_time_real_sec="$(rg -o "Total Test time \\(real\\) = [0-9.]+ sec" "${CTEST_LOG}" | tail -n 1 | awk '{print $(NF-1)}' || true)"
if [[ -z "${test_time_real_sec}" ]]; then
    test_time_real_sec="unknown"
fi

cat > "${SUMMARY_ENV}" <<EOF
RUN_UTC=${RUN_STAMP}
RUN_DATE=${RUN_DATE}
BUILD_DIR=${BUILD_DIR}
CTEST_STATUS=${ctest_status}
CTEST_PASSED=${passed}
CTEST_FAILED=${failed}
CTEST_TOTAL=${total}
CTEST_SUMMARY_LINE=${summary_line}
CTEST_REAL_TIME_SEC=${test_time_real_sec}
WALL_SECONDS=${wall_seconds}
LOG_FILE=${LOG_FILE}
CTEST_LOG=${CTEST_LOG}
MANIFEST_FILE=${MANIFEST_FILE}
EOF

cat > "${MANIFEST_FILE}" <<EOF
# BETA-GATE-001 Clean Build + Full Suite Manifest
Last modified: ${RUN_DATE}

## Run Metadata

- Run timestamp (UTC): \`${RUN_STAMP}\`
- Repository root: \`.\`
- Build directory: \`build/\`
- Jobs: \`${JOBS}\`
- Wall clock seconds: \`${wall_seconds}\`

## Commands Executed

1. \`rm -rf build\`
2. \`cmake -S . -B build\`
3. \`cmake --build build -j ${JOBS}\`
4. \`ctest --test-dir build --output-on-failure\`

## ctest Result

- Exit code: \`${ctest_status}\`
- Summary: \`${summary_line}\`
- Passed: \`${passed}\`
- Failed: \`${failed}\`
- Total: \`${total}\`
- Real test time (sec): \`${test_time_real_sec}\`

## Evidence Files

- Run log: \`${LOG_FILE#${REPO_ROOT}/}\`
- ctest log: \`${CTEST_LOG#${REPO_ROOT}/}\`
- Summary env: \`${SUMMARY_ENV#${REPO_ROOT}/}\`
EOF

log ""
log "wrote ${MANIFEST_FILE}"
log "wrote ${SUMMARY_ENV}"

if [[ "${ctest_status}" -ne 0 ]]; then
    exit "${ctest_status}"
fi
