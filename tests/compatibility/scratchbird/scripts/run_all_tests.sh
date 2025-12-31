#!/bin/bash
# ScratchBird Master Test Runner
# Runs all test suites: functional, memory (valgrind), and performance

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
RESULTS_DIR="${ROOT_DIR}/results"
REPORT_FILE="${RESULTS_DIR}/master_report_$(date +%Y%m%d_%H%M%S).txt"

# Test results
FUNCTIONAL_EXIT=0
MEMORY_EXIT=0
PERFORMANCE_EXIT=0

# Function: Print colored message
print_msg() {
    local color=$1
    shift
    echo -e "${color}$*${NC}"
}

# Function: Print section header
print_section() {
    local title=$1
    echo
    print_msg "${CYAN}" "========================================================================"
    print_msg "${CYAN}" "  ${title}"
    print_msg "${CYAN}" "========================================================================"
    echo
}

# Function: Run functional tests
run_functional_tests() {
    print_section "PHASE 1: Functional Tests"

    print_msg "${BLUE}" "Running SQL-based functional tests..."
    echo

    if "${SCRIPT_DIR}/run_tests.sh" "$@"; then
        FUNCTIONAL_EXIT=0
        print_msg "${GREEN}" "✓ Functional tests PASSED"
    else
        FUNCTIONAL_EXIT=$?
        print_msg "${RED}" "✗ Functional tests FAILED (exit code: ${FUNCTIONAL_EXIT})"
    fi

    echo
    print_msg "${YELLOW}" "Press Enter to continue to memory tests..."
    read
}

# Function: Run memory tests
run_memory_tests() {
    print_section "PHASE 2: Memory Leak Detection (Valgrind)"

    print_msg "${BLUE}" "Running Valgrind memory tests..."
    print_msg "${YELLOW}" "Note: This may take several minutes..."
    echo

    if "${SCRIPT_DIR}/run_valgrind_tests.sh"; then
        MEMORY_EXIT=0
        print_msg "${GREEN}" "✓ Memory tests PASSED - No leaks detected"
    else
        MEMORY_EXIT=$?
        print_msg "${RED}" "✗ Memory tests FAILED - Leaks or errors detected (exit code: ${MEMORY_EXIT})"
    fi

    echo
    print_msg "${YELLOW}" "Press Enter to continue to performance tests..."
    read
}

# Function: Run performance tests
run_performance_tests() {
    print_section "PHASE 3: Performance Benchmarks"

    print_msg "${BLUE}" "Running performance benchmarks..."
    echo

    if "${SCRIPT_DIR}/run_performance_tests.sh"; then
        PERFORMANCE_EXIT=0
        print_msg "${GREEN}" "✓ Performance tests COMPLETED"
    else
        PERFORMANCE_EXIT=$?
        print_msg "${YELLOW}" "⚠ Performance tests had issues (exit code: ${PERFORMANCE_EXIT})"
    fi
}

# Function: Generate master report
generate_master_report() {
    print_section "Test Report Generation"

    mkdir -p "${RESULTS_DIR}"

    print_msg "${BLUE}" "Generating comprehensive test report..."

    cat > "${REPORT_FILE}" <<EOF
========================================================================
ScratchBird Comprehensive Test Report
========================================================================

Generated: $(date)
Duration: Test suite execution

------------------------------------------------------------------------
System Information
------------------------------------------------------------------------

Hostname: $(hostname)
OS: $(uname -s) $(uname -r)
Kernel: $(uname -v)
CPU: $(lscpu | grep "Model name" | cut -d: -f2 | xargs || echo "Unknown")
Cores: $(nproc)
Memory: $(free -h | grep Mem | awk '{print $2}')
Disk: $(df -h / | tail -1 | awk '{print $2 " total, " $4 " free"}')

------------------------------------------------------------------------
Build Information
------------------------------------------------------------------------

ScratchBird Version: $(cat ${ROOT_DIR}/../../VERSION 2>/dev/null || echo "Unknown")
Build Type: $(cat ${ROOT_DIR}/../../build/CMakeCache.txt 2>/dev/null | grep CMAKE_BUILD_TYPE | cut -d= -f2 || echo "Unknown")
Compiler: $(cat ${ROOT_DIR}/../../build/CMakeCache.txt 2>/dev/null | grep CMAKE_CXX_COMPILER | cut -d= -f2 || echo "Unknown")

------------------------------------------------------------------------
Test Results Summary
------------------------------------------------------------------------

1. Functional Tests:       $([ ${FUNCTIONAL_EXIT} -eq 0 ] && echo "PASSED ✓" || echo "FAILED ✗ (exit: ${FUNCTIONAL_EXIT})")
2. Memory Leak Tests:       $([ ${MEMORY_EXIT} -eq 0 ] && echo "PASSED ✓" || echo "FAILED ✗ (exit: ${MEMORY_EXIT})")
3. Performance Benchmarks:  $([ ${PERFORMANCE_EXIT} -eq 0 ] && echo "COMPLETED ✓" || echo "ISSUES ⚠ (exit: ${PERFORMANCE_EXIT})")

Overall Status: $([ ${FUNCTIONAL_EXIT} -eq 0 ] && [ ${MEMORY_EXIT} -eq 0 ] && echo "PASS ✓" || echo "FAIL ✗")

------------------------------------------------------------------------
Detailed Results
------------------------------------------------------------------------

Functional Test Results:
$([ -f "${RESULTS_DIR}/functional_summary.txt" ] && cat "${RESULTS_DIR}/functional_summary.txt" || echo "No detailed results available")

Memory Test Results:
$([ -d "${RESULTS_DIR}/valgrind" ] && echo "Valgrind logs: ${RESULTS_DIR}/valgrind/" || echo "No valgrind logs available")

Performance Test Results:
$([ -d "${RESULTS_DIR}/performance" ] && ls -1 "${RESULTS_DIR}/performance/"*report*.txt 2>/dev/null | tail -1 | xargs cat || echo "No performance report available")

------------------------------------------------------------------------
Test Files Location
------------------------------------------------------------------------

Test Scripts:       ${ROOT_DIR}/tests/
Expected Outputs:   ${ROOT_DIR}/expected/
Test Results:       ${RESULTS_DIR}/
Valgrind Logs:      ${RESULTS_DIR}/valgrind/
Performance Logs:   ${RESULTS_DIR}/performance/

------------------------------------------------------------------------
Recommendations
------------------------------------------------------------------------

EOF

    # Add recommendations based on results
    if [[ ${FUNCTIONAL_EXIT} -ne 0 ]]; then
        echo "⚠ FUNCTIONAL TESTS FAILED" >> "${REPORT_FILE}"
        echo "  - Review failed test outputs in ${RESULTS_DIR}/" >> "${REPORT_FILE}"
        echo "  - Check diff files for output mismatches" >> "${REPORT_FILE}"
        echo "  - Run individual tests with --verbose for more details" >> "${REPORT_FILE}"
        echo "" >> "${REPORT_FILE}"
    fi

    if [[ ${MEMORY_EXIT} -ne 0 ]]; then
        echo "⚠ MEMORY LEAKS DETECTED" >> "${REPORT_FILE}"
        echo "  - Review valgrind logs in ${RESULTS_DIR}/valgrind/" >> "${REPORT_FILE}"
        echo "  - Identify definitely lost and indirectly lost memory" >> "${REPORT_FILE}"
        echo "  - Add suppressions for false positives to config/valgrind.supp" >> "${REPORT_FILE}"
        echo "" >> "${REPORT_FILE}"
    fi

    if [[ ${FUNCTIONAL_EXIT} -eq 0 ]] && [[ ${MEMORY_EXIT} -eq 0 ]]; then
        echo "✓ ALL TESTS PASSED" >> "${REPORT_FILE}"
        echo "  - No functional test failures" >> "${REPORT_FILE}"
        echo "  - No memory leaks detected" >> "${REPORT_FILE}"
        echo "  - Performance benchmarks completed" >> "${REPORT_FILE}"
        echo "" >> "${REPORT_FILE}"
    fi

    cat >> "${REPORT_FILE}" <<EOF
------------------------------------------------------------------------
Next Steps
------------------------------------------------------------------------

1. Review this report for any issues
2. If tests failed, investigate and fix
3. Re-run tests to verify fixes
4. Commit changes only after all tests pass
5. Track performance trends over time

========================================================================
End of Report
========================================================================
EOF

    print_msg "${GREEN}" "✓ Report generated: ${REPORT_FILE}"
}

# Function: Print final summary
print_final_summary() {
    print_section "Final Summary"

    echo "Test Execution Complete!"
    echo
    echo "Results:"
    echo "--------"
    printf "  Functional Tests:       "
    [ ${FUNCTIONAL_EXIT} -eq 0 ] && print_msg "${GREEN}" "PASSED ✓" || print_msg "${RED}" "FAILED ✗"

    printf "  Memory Tests:           "
    [ ${MEMORY_EXIT} -eq 0 ] && print_msg "${GREEN}" "PASSED ✓" || print_msg "${RED}" "FAILED ✗"

    printf "  Performance Tests:      "
    [ ${PERFORMANCE_EXIT} -eq 0 ] && print_msg "${GREEN}" "COMPLETED ✓" || print_msg "${YELLOW}" "ISSUES ⚠"

    echo
    print_msg "${BLUE}" "Comprehensive report: ${REPORT_FILE}"
    echo

    # Overall status
    if [[ ${FUNCTIONAL_EXIT} -eq 0 ]] && [[ ${MEMORY_EXIT} -eq 0 ]]; then
        print_msg "${GREEN}" "=========================================="
        print_msg "${GREEN}" "         ALL TESTS PASSED! ✓"
        print_msg "${GREEN}" "=========================================="
        exit 0
    else
        print_msg "${RED}" "=========================================="
        print_msg "${RED}" "         SOME TESTS FAILED! ✗"
        print_msg "${RED}" "=========================================="
        exit 1
    fi
}

# Function: Print usage
usage() {
    cat <<EOF
Usage: $0 [OPTIONS]

Run all ScratchBird test suites (functional, memory, performance)

OPTIONS:
    -f, --functional-only      Run only functional tests
    -m, --memory-only          Run only memory tests
    -p, --performance-only     Run only performance tests
    -s, --skip-prompts         Skip interactive prompts between phases
    -h, --help                 Show this help message

EXAMPLES:
    $0                         # Run all tests (interactive)
    $0 -s                      # Run all tests (non-interactive)
    $0 -f                      # Run functional tests only
    $0 -m                      # Run memory tests only
EOF
}

# Main script
main() {
    # Parse arguments
    RUN_FUNCTIONAL=true
    RUN_MEMORY=true
    RUN_PERFORMANCE=true
    SKIP_PROMPTS=false

    while [[ $# -gt 0 ]]; do
        case $1 in
            -f|--functional-only)
                RUN_MEMORY=false
                RUN_PERFORMANCE=false
                shift
                ;;
            -m|--memory-only)
                RUN_FUNCTIONAL=false
                RUN_PERFORMANCE=false
                shift
                ;;
            -p|--performance-only)
                RUN_FUNCTIONAL=false
                RUN_MEMORY=false
                shift
                ;;
            -s|--skip-prompts)
                SKIP_PROMPTS=true
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

    # Override prompts if specific suite selected
    if [[ ${RUN_FUNCTIONAL} == false ]] || [[ ${RUN_MEMORY} == false ]] || [[ ${RUN_PERFORMANCE} == false ]]; then
        SKIP_PROMPTS=true
    fi

    # Print header
    print_msg "${MAGENTA}" "========================================================================"
    print_msg "${MAGENTA}" "                ScratchBird Master Test Suite                          "
    print_msg "${MAGENTA}" "========================================================================"
    print_msg "${CYAN}" ""
    print_msg "${CYAN}" "  This will run all ScratchBird tests:"
    print_msg "${CYAN}" "    1. Functional Tests (SQL execution)"
    print_msg "${CYAN}" "    2. Memory Tests (Valgrind leak detection)"
    print_msg "${CYAN}" "    3. Performance Tests (Benchmarks)"
    print_msg "${CYAN}" ""
    print_msg "${YELLOW}" "  Estimated time: 10-30 minutes depending on system"
    print_msg "${MAGENTA}" "========================================================================"
    echo

    if [[ ${SKIP_PROMPTS} == false ]]; then
        print_msg "${YELLOW}" "Press Enter to begin, or Ctrl+C to cancel..."
        read
    fi

    # Run test suites
    if [[ ${RUN_FUNCTIONAL} == true ]]; then
        if [[ ${SKIP_PROMPTS} == true ]]; then
            run_functional_tests < /dev/null
        else
            run_functional_tests
        fi
    fi

    if [[ ${RUN_MEMORY} == true ]]; then
        if [[ ${SKIP_PROMPTS} == true ]]; then
            run_memory_tests < /dev/null
        else
            run_memory_tests
        fi
    fi

    if [[ ${RUN_PERFORMANCE} == true ]]; then
        run_performance_tests
    fi

    # Generate report and summary
    generate_master_report
    print_final_summary
}

main "$@"
