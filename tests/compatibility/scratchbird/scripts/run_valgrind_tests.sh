#!/bin/bash
# ScratchBird Valgrind Memory Leak Detection Tests
# Runs tests under valgrind to detect memory leaks, invalid memory access, etc.

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
RESULTS_DIR="${ROOT_DIR}/results/valgrind"
BUILD_DIR="${ROOT_DIR}/../../build"

# Test binary paths
SB_ISQL="${BUILD_DIR}/sb_isql"
SB_SERVER="${BUILD_DIR}/sb_server"

# Valgrind configuration
VALGRIND_OPTS=(
    --leak-check=full
    --show-leak-kinds=all
    --track-origins=yes
    --verbose
    --error-exitcode=1
    --suppressions="${ROOT_DIR}/config/valgrind.supp"
)

# Statistics
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0
LEAKS_FOUND=0
ERRORS_FOUND=0

# Arrays to track results
declare -a FAILED_TESTS
declare -a TESTS_WITH_LEAKS

# Function: Print colored message
print_msg() {
    local color=$1
    shift
    echo -e "${color}$*${NC}"
}

# Function: Check prerequisites
check_prerequisites() {
    print_msg "${BLUE}" "Checking prerequisites..."

    # Check valgrind
    if ! command -v valgrind &> /dev/null; then
        print_msg "${RED}" "ERROR: valgrind not found"
        print_msg "${YELLOW}" "Please install valgrind: sudo apt-get install valgrind"
        exit 1
    fi

    # Check debug build
    if [[ ! -x "${SB_ISQL}" ]]; then
        print_msg "${RED}" "ERROR: sb_isql not found at ${SB_ISQL}"
        print_msg "${YELLOW}" "Please build ScratchBird first: cd build && cmake -DCMAKE_BUILD_TYPE=Debug .. && make"
        exit 1
    fi

    # Check if built with debug symbols
    if ! file "${SB_ISQL}" | grep -q "not stripped"; then
        print_msg "${YELLOW}" "WARNING: Binary may be stripped of debug symbols"
        print_msg "${YELLOW}" "For best results, rebuild with: cmake -DCMAKE_BUILD_TYPE=Debug"
    fi

    print_msg "${GREEN}" "✓ Prerequisites OK"
    echo
}

# Function: Create suppressions file
create_suppressions() {
    local supp_file="${ROOT_DIR}/config/valgrind.supp"
    mkdir -p "$(dirname "${supp_file}")"

    if [[ ! -f "${supp_file}" ]]; then
        cat > "${supp_file}" <<'EOF'
# ScratchBird Valgrind Suppressions
# Add suppressions for known false positives here

# Example: Suppress known library leaks
{
   glibc_dlopen_leak
   Memcheck:Leak
   ...
   fun:_dl_open
   fun:dlopen_doit
}

{
   glibc_pthread_create
   Memcheck:Leak
   ...
   fun:pthread_create*
}

# Suppress C++ std::string allocations that are intentionally static
{
   cpp_static_string
   Memcheck:Leak
   ...
   fun:_ZNSsC1ERKSs
}
EOF
        print_msg "${YELLOW}" "Created default suppressions file: ${supp_file}"
    fi
}

# Function: Run test under valgrind
run_valgrind_test() {
    local test_file=$1
    local category=$(dirname "${test_file}")
    local test_name=$(basename "${test_file}" .sql)
    local full_name="${category}/${test_name}"

    local test_path="${TESTS_DIR}/${test_file}"
    local result_path="${RESULTS_DIR}/${category}_${test_name}.valgrind.log"

    # Create results subdirectory
    mkdir -p "$(dirname "${result_path}")"

    ((TESTS_RUN++))

    printf "%-60s" "  ${full_name}"

    # Run test through valgrind
    local valgrind_exit=0
    if timeout 300 valgrind "${VALGRIND_OPTS[@]}" \
        "${SB_ISQL}" -f "${test_path}" \
        > /dev/null 2> "${result_path}"; then
        valgrind_exit=0
    else
        valgrind_exit=$?
    fi

    # Parse valgrind output
    local definitely_lost=$(grep "definitely lost:" "${result_path}" | awk '{print $4}' | sed 's/,//g')
    local indirectly_lost=$(grep "indirectly lost:" "${result_path}" | awk '{print $4}' | sed 's/,//g')
    local possibly_lost=$(grep "possibly lost:" "${result_path}" | awk '{print $4}' | sed 's/,//g')
    local error_summary=$(grep "ERROR SUMMARY:" "${result_path}" | awk '{print $4}')

    definitely_lost=${definitely_lost:-0}
    indirectly_lost=${indirectly_lost:-0}
    possibly_lost=${possibly_lost:-0}
    error_summary=${error_summary:-0}

    # Check for errors and leaks
    local has_errors=false
    local has_leaks=false

    if [[ ${error_summary} -gt 0 ]]; then
        has_errors=true
        ((ERRORS_FOUND++))
    fi

    if [[ ${definitely_lost} -gt 0 ]] || [[ ${indirectly_lost} -gt 0 ]]; then
        has_leaks=true
        ((LEAKS_FOUND++))
        TESTS_WITH_LEAKS+=("${full_name}: ${definitely_lost} bytes definitely lost, ${indirectly_lost} bytes indirectly lost")
    fi

    # Print result
    if [[ ${has_errors} == true ]] || [[ ${has_leaks} == true ]]; then
        print_msg "${RED}" "[FAIL]"
        ((TESTS_FAILED++))
        FAILED_TESTS+=("${full_name}")

        if [[ ${has_errors} == true ]]; then
            print_msg "${RED}" "    ✗ ${error_summary} errors detected"
        fi
        if [[ ${has_leaks} == true ]]; then
            print_msg "${RED}" "    ✗ Memory leaks: ${definitely_lost} bytes definitely lost, ${indirectly_lost} bytes indirectly lost"
        fi
        print_msg "${YELLOW}" "    Log: ${result_path}"
    else
        print_msg "${GREEN}" "[PASS]"
        ((TESTS_PASSED++))

        # Show possibly lost as warning
        if [[ ${possibly_lost} -gt 0 ]]; then
            print_msg "${YELLOW}" "    ⚠ ${possibly_lost} bytes possibly lost (may be false positive)"
        fi
    fi
}

# Function: Run all tests
run_all_tests() {
    print_msg "${BLUE}" "Running valgrind memory tests..."
    echo

    # Run basic tests only (full suite would be too slow)
    if [[ -d "${TESTS_DIR}/basic" ]]; then
        print_msg "${BLUE}" "\nCategory: basic"
        print_msg "${BLUE}" "=========================================="

        while IFS= read -r -d '' test_file; do
            local relative_path="${test_file#${TESTS_DIR}/}"
            run_valgrind_test "${relative_path}"
        done < <(find "${TESTS_DIR}/basic" -name "*.sql" -type f -print0 | sort -z | head -z -3)
    fi

    echo
}

# Function: Run stress test
run_stress_test() {
    print_msg "${BLUE}" "Running stress test under valgrind..."

    local stress_result="${RESULTS_DIR}/stress_test.valgrind.log"
    mkdir -p "${RESULTS_DIR}"

    # Create temporary stress test
    local stress_sql="/tmp/stress_test.sql"
    cat > "${stress_sql}" <<'EOF'
CREATE DATABASE stress_test;
\c stress_test;

CREATE TABLE stress_data (
    id INTEGER PRIMARY KEY,
    data VARCHAR(100)
);

-- Insert 1000 rows
INSERT INTO stress_data SELECT generate_series(1, 1000), 'Data_' || generate_series(1, 1000);

-- Update all rows
UPDATE stress_data SET data = 'Updated_' || id;

-- Delete half the rows
DELETE FROM stress_data WHERE id % 2 = 0;

-- Re-insert deleted rows
INSERT INTO stress_data SELECT generate_series(2, 1000, 2), 'ReInserted_' || generate_series(2, 1000, 2);

-- Create and drop multiple indexes
CREATE INDEX idx1 ON stress_data(id);
CREATE INDEX idx2 ON stress_data(data);
DROP INDEX idx1;
DROP INDEX idx2;

-- Multiple transactions
BEGIN TRANSACTION;
UPDATE stress_data SET data = 'TX1_' || id WHERE id < 100;
COMMIT;

BEGIN TRANSACTION;
DELETE FROM stress_data WHERE id < 50;
ROLLBACK;

-- Cleanup
DROP TABLE stress_data;
DROP DATABASE stress_test;
EOF

    printf "%-60s" "  Stress Test (1000 rows)"

    # Run under valgrind
    if timeout 600 valgrind "${VALGRIND_OPTS[@]}" \
        "${SB_ISQL}" -f "${stress_sql}" \
        > /dev/null 2> "${stress_result}"; then
        local stress_lost=$(grep "definitely lost:" "${stress_result}" | awk '{print $4}' | sed 's/,//g')
        stress_lost=${stress_lost:-0}

        if [[ ${stress_lost} -eq 0 ]]; then
            print_msg "${GREEN}" "[PASS]"
        else
            print_msg "${RED}" "[FAIL]"
            print_msg "${RED}" "    ✗ ${stress_lost} bytes definitely lost"
        fi
    else
        print_msg "${RED}" "[TIMEOUT]"
    fi

    rm -f "${stress_sql}"
    echo
}

# Function: Print summary
print_summary() {
    echo
    print_msg "${BLUE}" "========================================"
    print_msg "${BLUE}" "Valgrind Test Summary"
    print_msg "${BLUE}" "========================================"
    echo "Total tests run:    ${TESTS_RUN}"
    print_msg "${GREEN}" "Passed:             ${TESTS_PASSED}"
    print_msg "${RED}" "Failed:             ${TESTS_FAILED}"
    echo "Tests with leaks:   ${LEAKS_FOUND}"
    echo "Tests with errors:  ${ERRORS_FOUND}"

    if [[ ${#TESTS_WITH_LEAKS[@]} -gt 0 ]]; then
        echo
        print_msg "${RED}" "Tests with memory leaks:"
        for test in "${TESTS_WITH_LEAKS[@]}"; do
            echo "  - ${test}"
        done
    fi

    if [[ ${TESTS_FAILED} -gt 0 ]]; then
        echo
        print_msg "${RED}" "Failed tests:"
        for test in "${FAILED_TESTS[@]}"; do
            echo "  - ${test}"
        done
    fi

    echo
    print_msg "${BLUE}" "Detailed logs: ${RESULTS_DIR}/"

    echo
    if [[ ${TESTS_FAILED} -eq 0 && ${TESTS_RUN} -gt 0 ]]; then
        print_msg "${GREEN}" "✓ No memory leaks or errors detected!"
        exit 0
    else
        print_msg "${RED}" "✗ Memory issues detected"
        exit 1
    fi
}

# Main script
main() {
    print_msg "${BLUE}" "========================================"
    print_msg "${BLUE}" "ScratchBird Valgrind Memory Tests"
    print_msg "${BLUE}" "========================================"
    echo

    check_prerequisites
    create_suppressions
    mkdir -p "${RESULTS_DIR}"

    run_all_tests
    run_stress_test
    print_summary
}

main "$@"
