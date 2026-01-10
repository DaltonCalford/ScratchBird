#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"

run_ctest() {
    local label="$1"
    shift
    local start=$SECONDS
    "$@"
    local elapsed=$((SECONDS - start))
    echo "Completed ${label} in ${elapsed}s"
}

if [[ ! -d "${BUILD_DIR}" ]]; then
    echo "Build directory not found: ${BUILD_DIR}" >&2
    echo "Run CMake configure/generate first (e.g., mkdir build && cmake ..)." >&2
    exit 1
fi

cmake --build "${BUILD_DIR}" --target scratchbird_test_binaries

cd "${BUILD_DIR}"

case "${1:-all}" in
    smoke)
        echo "Running smoke tests..."
        run_ctest "smoke" ctest -L smoke --output-on-failure
        ;;
    unit)
        echo "Running unit tests..."
        run_ctest "unit" ctest -L unit --output-on-failure --timeout 10
        ;;
    integration)
        echo "Running integration tests..."
        run_ctest "integration" ctest -L integration --output-on-failure --timeout 60
        ;;
    stress)
        echo "Running stress tests..."
        run_ctest "stress" ctest -L stress --output-on-failure --timeout 600
        ;;
    performance)
        echo "Running performance tests..."
        run_ctest "performance" ctest -L performance --output-on-failure --timeout 300
        ;;
    quarantine)
        echo "Running quarantine tests (known issues)..."
        run_ctest "quarantine" ctest -L quarantine --output-on-failure --timeout 60 || true
        ;;
    quick)
        echo "Running quick test suite (smoke + unit)..."
        run_ctest "quick" ctest -L "smoke|unit" --output-on-failure --timeout 10
        ;;
    ci)
        echo "Running CI test suite (smoke + unit + integration)..."
        run_ctest "ci" ctest -L "smoke|unit|integration" -E "quarantine" --output-on-failure --timeout 60
        ;;
    all)
        echo "Running all tests (excluding quarantine)..."
        run_ctest "all" ctest -E "quarantine" --output-on-failure --timeout 300
        ;;
    *)
        echo "Usage: $0 {smoke|unit|integration|stress|performance|quarantine|quick|ci|all}" >&2
        echo "" >&2
        echo "  smoke       - Fast sanity tests (< 1s each, ~30s total)" >&2
        echo "  unit        - Unit tests (< 5s each, ~5min total)" >&2
        echo "  integration - Integration tests (< 30s each, ~20min total)" >&2
        echo "  stress      - Stress tests (> 30s each, ~1hr total)" >&2
        echo "  performance - Performance/benchmark tests (on-demand)" >&2
        echo "  quarantine  - Known-problematic tests (manual investigation)" >&2
        echo "  quick       - smoke + unit (fast feedback, ~5min)" >&2
        echo "  ci          - CI suite: smoke + unit + integration (~25min)" >&2
        echo "  all         - Everything except quarantine (~1.5hrs)" >&2
        echo "" >&2
        echo "Note: socket-based integration tests require SCRATCHBIRD_TEST_NETWORK=1." >&2
        exit 1
        ;;
 esac
