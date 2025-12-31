#!/bin/bash
# ScratchBird Native Test Runner
# Executes SQL test scripts and compares output against expected results

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
TESTS_DIR="${ROOT_DIR}/tests"
EXPECTED_DIR="${ROOT_DIR}/expected"
RESULTS_DIR="${ROOT_DIR}/results"
BUILD_DIR="${ROOT_DIR}/../../build"

# Test binary paths
SB_ISQL="${BUILD_DIR}/sb_isql"
SB_SERVER="${BUILD_DIR}/sb_server"

# Test database
TEST_DB="/tmp/scratchbird_test.db"

# Statistics
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0
TESTS_SKIPPED=0

# Arrays to track results
declare -a FAILED_TESTS
declare -a PASSED_TESTS

# Function: Print usage
usage() {
    cat <<EOF
Usage: $0 [OPTIONS]

Run ScratchBird native SQL tests

OPTIONS:
    -c, --category CATEGORY    Run only tests in specified category (basic, advanced, mga, security, performance)
    -t, --test TEST_FILE       Run specific test file
    -v, --verbose              Verbose output
    -k, --keep-db              Keep test database after completion
    -u, --update-expected      Update expected output files with actual results
    -h, --help                 Show this help message

EXAMPLES:
    $0                          # Run all tests
    $0 -c basic                 # Run only basic tests
    $0 -t basic/001_datatypes   # Run specific test
    $0 -v                       # Run with verbose output
    $0 -u                       # Update expected outputs
EOF
}

# Function: Print colored message
print_msg() {
    local color=$1
    shift
    echo -e "${color}$*${NC}"
}

# Function: Check prerequisites
check_prerequisites() {
    print_msg "${BLUE}" "Checking prerequisites..."

    if [[ ! -x "${SB_ISQL}" ]]; then
        print_msg "${RED}" "ERROR: sb_isql not found at ${SB_ISQL}"
        print_msg "${YELLOW}" "Please build ScratchBird first: cd build && make"
        exit 1
    fi

    print_msg "${GREEN}" "✓ Prerequisites OK"
    echo
}

# Function: Setup test environment
setup_environment() {
    print_msg "${BLUE}" "Setting up test environment..."

    # Create results directory
    mkdir -p "${RESULTS_DIR}"

    # Clean up old test database
    if [[ -f "${TEST_DB}" ]]; then
        rm -f "${TEST_DB}"
    fi

    print_msg "${GREEN}" "✓ Environment ready"
    echo
}

# Function: Run a single test
run_test() {
    local test_file=$1
    local category=$(dirname "${test_file}")
    local test_name=$(basename "${test_file}" .sql)
    local full_name="${category}/${test_name}"

    local test_path="${TESTS_DIR}/${test_file}"
    local expected_path="${EXPECTED_DIR}/${category}/${test_name}.expected"
    local result_path="${RESULTS_DIR}/${category}_${test_name}.result"
    local diff_path="${RESULTS_DIR}/${category}_${test_name}.diff"

    # Create results subdirectory
    mkdir -p "$(dirname "${result_path}")"

    ((TESTS_RUN++))

    if [[ "${VERBOSE}" == "true" ]]; then
        print_msg "${BLUE}" "Running: ${full_name}"
    else
        printf "%-60s" "  ${full_name}"
    fi

    # Run test through sb_isql
    if timeout 60 "${SB_ISQL}" -f "${test_path}" > "${result_path}" 2>&1; then
        test_exit_code=0
    else
        test_exit_code=$?
    fi

    # Check if test ran successfully
    if [[ ${test_exit_code} -ne 0 ]]; then
        if [[ "${VERBOSE}" != "true" ]]; then
            print_msg "${RED}" "[FAIL]"
        fi
        print_msg "${RED}" "  ERROR: Test execution failed (exit code: ${test_exit_code})"
        ((TESTS_FAILED++))
        FAILED_TESTS+=("${full_name}")
        return 1
    fi

    # If updating expected outputs, copy result to expected
    if [[ "${UPDATE_EXPECTED}" == "true" ]]; then
        mkdir -p "$(dirname "${expected_path}")"
        cp "${result_path}" "${expected_path}"
        print_msg "${YELLOW}" "[UPDATED]"
        ((TESTS_PASSED++))
        PASSED_TESTS+=("${full_name}")
        return 0
    fi

    # Compare with expected output if it exists
    if [[ ! -f "${expected_path}" ]]; then
        print_msg "${YELLOW}" "[SKIP - No expected output]"
        ((TESTS_SKIPPED++))
        return 0
    fi

    # Diff results
    if diff -u "${expected_path}" "${result_path}" > "${diff_path}" 2>&1; then
        if [[ "${VERBOSE}" != "true" ]]; then
            print_msg "${GREEN}" "[PASS]"
        else
            print_msg "${GREEN}" "  ✓ PASSED"
        fi
        ((TESTS_PASSED++))
        PASSED_TESTS+=("${full_name}")
        rm -f "${diff_path}"
    else
        if [[ "${VERBOSE}" != "true" ]]; then
            print_msg "${RED}" "[FAIL]"
        fi
        print_msg "${RED}" "  ✗ Output mismatch"
        ((TESTS_FAILED++))
        FAILED_TESTS+=("${full_name}")

        if [[ "${VERBOSE}" == "true" ]]; then
            print_msg "${YELLOW}" "  Diff saved to: ${diff_path}"
            echo "  First 20 lines of diff:"
            head -20 "${diff_path}" | sed 's/^/    /'
        fi
    fi
}

# Function: Find and run tests
run_tests() {
    local category="${1:-}"
    local specific_test="${2:-}"

    print_msg "${BLUE}" "Running tests..."
    echo

    if [[ -n "${specific_test}" ]]; then
        # Run specific test
        if [[ ! -f "${TESTS_DIR}/${specific_test}.sql" ]]; then
            print_msg "${RED}" "ERROR: Test file not found: ${specific_test}.sql"
            exit 1
        fi
        run_test "${specific_test}.sql"
    elif [[ -n "${category}" ]]; then
        # Run tests in category
        if [[ ! -d "${TESTS_DIR}/${category}" ]]; then
            print_msg "${RED}" "ERROR: Category not found: ${category}"
            exit 1
        fi

        while IFS= read -r -d '' test_file; do
            local relative_path="${test_file#${TESTS_DIR}/}"
            run_test "${relative_path}"
        done < <(find "${TESTS_DIR}/${category}" -name "*.sql" -type f -print0 | sort -z)
    else
        # Run all tests
        for cat_dir in basic advanced mga security performance; do
            if [[ -d "${TESTS_DIR}/${cat_dir}" ]]; then
                print_msg "${BLUE}" "\nCategory: ${cat_dir}"
                print_msg "${BLUE}" "=========================================="

                while IFS= read -r -d '' test_file; do
                    local relative_path="${test_file#${TESTS_DIR}/}"
                    run_test "${relative_path}"
                done < <(find "${TESTS_DIR}/${cat_dir}" -name "*.sql" -type f -print0 | sort -z)
            fi
        done
    fi

    echo
}

# Function: Print summary
print_summary() {
    echo
    print_msg "${BLUE}" "========================================"
    print_msg "${BLUE}" "Test Summary"
    print_msg "${BLUE}" "========================================"
    echo "Total tests run:    ${TESTS_RUN}"
    print_msg "${GREEN}" "Passed:             ${TESTS_PASSED}"
    print_msg "${RED}" "Failed:             ${TESTS_FAILED}"
    print_msg "${YELLOW}" "Skipped:            ${TESTS_SKIPPED}"

    if [[ ${TESTS_FAILED} -gt 0 ]]; then
        echo
        print_msg "${RED}" "Failed tests:"
        for test in "${FAILED_TESTS[@]}"; do
            echo "  - ${test}"
        done
    fi

    echo
    if [[ ${TESTS_FAILED} -eq 0 && ${TESTS_RUN} -gt 0 ]]; then
        print_msg "${GREEN}" "✓ All tests passed!"
        exit 0
    elif [[ ${TESTS_RUN} -eq 0 ]]; then
        print_msg "${YELLOW}" "⚠ No tests were run"
        exit 0
    else
        print_msg "${RED}" "✗ Some tests failed"
        exit 1
    fi
}

# Function: Cleanup
cleanup() {
    if [[ "${KEEP_DB}" != "true" ]]; then
        if [[ -f "${TEST_DB}" ]]; then
            rm -f "${TEST_DB}"
        fi
    fi
}

# Main script
main() {
    # Parse arguments
    CATEGORY=""
    SPECIFIC_TEST=""
    VERBOSE="false"
    KEEP_DB="false"
    UPDATE_EXPECTED="false"

    while [[ $# -gt 0 ]]; do
        case $1 in
            -c|--category)
                CATEGORY="$2"
                shift 2
                ;;
            -t|--test)
                SPECIFIC_TEST="$2"
                shift 2
                ;;
            -v|--verbose)
                VERBOSE="true"
                shift
                ;;
            -k|--keep-db)
                KEEP_DB="true"
                shift
                ;;
            -u|--update-expected)
                UPDATE_EXPECTED="true"
                shift
                ;;
            -h|--help)
                usage
                exit 0
                ;;
            *)
                print_msg "${RED}" "ERROR: Unknown option: $1"
                usage
                exit 1
                ;;
        esac
    done

    # Set trap for cleanup
    trap cleanup EXIT

    # Run tests
    print_msg "${BLUE}" "========================================"
    print_msg "${BLUE}" "ScratchBird Native Test Suite"
    print_msg "${BLUE}" "========================================"
    echo

    check_prerequisites
    setup_environment
    run_tests "${CATEGORY}" "${SPECIFIC_TEST}"
    print_summary
}

main "$@"
