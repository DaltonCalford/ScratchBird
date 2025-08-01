#!/bin/bash

#
# The contents of this file are subject to the Initial
# Developer's Public License Version 1.0 (the "License");
# you may not use this file except in compliance with the
# License. You may obtain a copy of the License at
# http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
#
# Software distributed under the License is distributed AS IS,
# WITHOUT WARRANTY OF ANY KIND, either express or implied.
# See the License for the specific language governing rights
# and limitations under the License.
#
# The Original Code was created for the ScratchBird Open Source 
# RDBMS project.
#
# Copyright (c) 2025 ScratchBird Project
# and all contributors signed below.
#
# All Rights Reserved.
# Contributor(s): ______________________________________.
#
# 2025.07.23 - ScratchBird GIN Index Implementation - Test Runner Script

set -e  # Exit on any error

# Script configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="${SCRIPT_DIR}/src/jrd"
TEST_LOG_DIR="${SCRIPT_DIR}/test_results"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test configuration
RUN_UNIT_TESTS=1
RUN_FUNCTIONAL_TESTS=1
RUN_PERFORMANCE_TESTS=0
RUN_MEMORY_TESTS=0
RUN_STRESS_TESTS=0
VERBOSE=0
CLEAN_BUILD=1

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_header() {
    echo -e "\n${BLUE}============================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}============================================${NC}\n"
}

# Function to show usage
show_usage() {
    cat << EOF
ScratchBird GIN Index Test Runner

Usage: $0 [OPTIONS]

Options:
    -u, --unit              Run unit tests only
    -f, --functional        Run functional tests only
    -p, --performance       Run performance tests
    -m, --memory           Run memory leak tests (requires valgrind)
    -s, --stress           Run stress tests
    -a, --all              Run all tests (default)
    -v, --verbose          Verbose output
    -c, --clean            Clean build before testing
    --no-clean             Skip clean build
    -h, --help             Show this help message

Examples:
    $0                     # Run unit and functional tests
    $0 -p                  # Run performance tests
    $0 -a -v              # Run all tests with verbose output
    $0 -u -f --no-clean   # Run unit and functional tests without clean build

Test Results:
    Test results are saved to: ${TEST_LOG_DIR}/
    
Requirements:
    - GCC/G++ with C++17 support
    - Make build system
    - ScratchBird source tree
    - Optional: valgrind (for memory tests)
    - Optional: gcov (for coverage analysis)
EOF
}

# Parse command line arguments
parse_arguments() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            -u|--unit)
                RUN_UNIT_TESTS=1
                RUN_FUNCTIONAL_TESTS=0
                RUN_PERFORMANCE_TESTS=0
                RUN_MEMORY_TESTS=0
                RUN_STRESS_TESTS=0
                shift
                ;;
            -f|--functional)
                RUN_UNIT_TESTS=0
                RUN_FUNCTIONAL_TESTS=1
                RUN_PERFORMANCE_TESTS=0
                RUN_MEMORY_TESTS=0
                RUN_STRESS_TESTS=0
                shift
                ;;
            -p|--performance)
                RUN_PERFORMANCE_TESTS=1
                shift
                ;;
            -m|--memory)
                RUN_MEMORY_TESTS=1
                shift
                ;;
            -s|--stress)
                RUN_STRESS_TESTS=1
                shift
                ;;
            -a|--all)
                RUN_UNIT_TESTS=1
                RUN_FUNCTIONAL_TESTS=1
                RUN_PERFORMANCE_TESTS=1
                RUN_MEMORY_TESTS=1
                RUN_STRESS_TESTS=1
                shift
                ;;
            -v|--verbose)
                VERBOSE=1
                shift
                ;;
            -c|--clean)
                CLEAN_BUILD=1
                shift
                ;;
            --no-clean)
                CLEAN_BUILD=0
                shift
                ;;
            -h|--help)
                show_usage
                exit 0
                ;;
            *)
                print_error "Unknown option: $1"
                show_usage
                exit 1
                ;;
        esac
    done
}

# Function to check prerequisites
check_prerequisites() {
    print_status "Checking prerequisites..."
    
    # Check for required tools
    if ! command -v g++ &> /dev/null; then
        print_error "g++ compiler not found. Please install GCC/G++."
        exit 1
    fi
    
    if ! command -v make &> /dev/null; then
        print_error "make not found. Please install make build system."
        exit 1
    fi
    
    # Check for optional tools
    if [[ $RUN_MEMORY_TESTS -eq 1 ]] && ! command -v valgrind &> /dev/null; then
        print_warning "valgrind not found. Memory tests will be skipped."
        RUN_MEMORY_TESTS=0
    fi
    
    if ! command -v gcov &> /dev/null; then
        print_warning "gcov not found. Code coverage analysis not available."
    fi
    
    # Check source directory structure
    if [[ ! -d "$SRC_DIR" ]]; then
        print_error "Source directory not found: $SRC_DIR"
        exit 1
    fi
    
    if [[ ! -f "$SRC_DIR/gin_test.cpp" ]]; then
        print_error "GIN test files not found in: $SRC_DIR"
        exit 1
    fi
    
    print_success "Prerequisites check completed"
}

# Function to setup test environment
setup_test_environment() {
    print_status "Setting up test environment..."
    
    # Create test results directory
    mkdir -p "$TEST_LOG_DIR"
    
    # Set up environment variables
    export GIN_TEST_VERBOSE=$VERBOSE
    export GIN_TEST_LOG_FILE="${TEST_LOG_DIR}/gin_test_${TIMESTAMP}.log"
    export GIN_TEST_PERF_FILE="${TEST_LOG_DIR}/gin_performance_${TIMESTAMP}.log"
    
    print_success "Test environment setup completed"
}

# Function to build tests
build_tests() {
    print_status "Building GIN Index tests..."
    
    cd "$SRC_DIR"
    
    if [[ $CLEAN_BUILD -eq 1 ]]; then
        print_status "Cleaning previous build..."
        make -f gin_test.mk clean || true
    fi
    
    print_status "Compiling test executables..."
    if [[ $VERBOSE -eq 1 ]]; then
        make -f gin_test.mk all
    else
        make -f gin_test.mk all > /dev/null 2>&1
    fi
    
    if [[ $? -eq 0 ]]; then
        print_success "Tests built successfully"
    else
        print_error "Test build failed"
        exit 1
    fi
    
    cd - > /dev/null
}

# Function to run unit tests
run_unit_tests() {
    if [[ $RUN_UNIT_TESTS -eq 1 ]]; then
        print_header "Running GIN Index Unit Tests"
        
        cd "$SRC_DIR"
        
        local test_output="${TEST_LOG_DIR}/unit_tests_${TIMESTAMP}.log"
        
        if [[ $VERBOSE -eq 1 ]]; then
            ./gin_test | tee "$test_output"
            local result=${PIPESTATUS[0]}
        else
            ./gin_test > "$test_output" 2>&1
            local result=$?
        fi
        
        if [[ $result -eq 0 ]]; then
            print_success "Unit tests PASSED"
        else
            print_error "Unit tests FAILED (see $test_output)"
            return 1
        fi
        
        cd - > /dev/null
    fi
    
    return 0
}

# Function to run functional tests
run_functional_tests() {
    if [[ $RUN_FUNCTIONAL_TESTS -eq 1 ]]; then
        print_header "Running GIN Index Functional Tests"
        
        cd "$SRC_DIR"
        
        local test_output="${TEST_LOG_DIR}/functional_tests_${TIMESTAMP}.log"
        
        if [[ $VERBOSE -eq 1 ]]; then
            ./gin_functional_test | tee "$test_output"
            local result=${PIPESTATUS[0]}
        else
            ./gin_functional_test > "$test_output" 2>&1
            local result=$?
        fi
        
        if [[ $result -eq 0 ]]; then
            print_success "Functional tests PASSED"
        else
            print_error "Functional tests FAILED (see $test_output)"
            return 1
        fi
        
        cd - > /dev/null
    fi
    
    return 0
}

# Function to run performance tests
run_performance_tests() {
    if [[ $RUN_PERFORMANCE_TESTS -eq 1 ]]; then
        print_header "Running GIN Index Performance Tests"
        
        cd "$SRC_DIR"
        
        local test_output="${TEST_LOG_DIR}/performance_tests_${TIMESTAMP}.log"
        
        print_status "Running performance benchmarks..."
        
        if [[ $VERBOSE -eq 1 ]]; then
            make -f gin_test.mk test-performance | tee "$test_output"
            local result=${PIPESTATUS[0]}
        else
            make -f gin_test.mk test-performance > "$test_output" 2>&1
            local result=$?
        fi
        
        if [[ $result -eq 0 ]]; then
            print_success "Performance tests completed"
        else
            print_warning "Performance tests completed with warnings (see $test_output)"
        fi
        
        cd - > /dev/null
    fi
}

# Function to run memory tests
run_memory_tests() {
    if [[ $RUN_MEMORY_TESTS -eq 1 ]]; then
        print_header "Running GIN Index Memory Tests"
        
        cd "$SRC_DIR"
        
        local test_output="${TEST_LOG_DIR}/memory_tests_${TIMESTAMP}.log"
        
        print_status "Running memory leak detection with valgrind..."
        
        if [[ $VERBOSE -eq 1 ]]; then
            make -f gin_test.mk test-memory | tee "$test_output"
            local result=${PIPESTATUS[0]}
        else
            make -f gin_test.mk test-memory > "$test_output" 2>&1
            local result=$?
        fi
        
        if [[ $result -eq 0 ]]; then
            print_success "Memory tests PASSED"
        else
            print_error "Memory tests FAILED (see $test_output)"
            return 1
        fi
        
        cd - > /dev/null
    fi
    
    return 0
}

# Function to run stress tests
run_stress_tests() {
    if [[ $RUN_STRESS_TESTS -eq 1 ]]; then
        print_header "Running GIN Index Stress Tests"
        
        cd "$SRC_DIR"
        
        local test_output="${TEST_LOG_DIR}/stress_tests_${TIMESTAMP}.log"
        
        print_status "Running stress tests (this may take a while)..."
        
        if [[ $VERBOSE -eq 1 ]]; then
            make -f gin_test.mk test-stress | tee "$test_output"
            local result=${PIPESTATUS[0]}
        else
            make -f gin_test.mk test-stress > "$test_output" 2>&1
            local result=$?
        fi
        
        if [[ $result -eq 0 ]]; then
            print_success "Stress tests completed"
        else
            print_warning "Stress tests completed with issues (see $test_output)"
        fi
        
        cd - > /dev/null
    fi
}

# Function to generate test report
generate_test_report() {
    print_header "Generating Test Report"
    
    local report_file="${TEST_LOG_DIR}/gin_test_report_${TIMESTAMP}.txt"
    
    cat > "$report_file" << EOF
ScratchBird GIN Index Test Report
=================================

Test Run: $(date)
Timestamp: $TIMESTAMP

Test Configuration:
- Unit Tests: $([ $RUN_UNIT_TESTS -eq 1 ] && echo "YES" || echo "NO")
- Functional Tests: $([ $RUN_FUNCTIONAL_TESTS -eq 1 ] && echo "YES" || echo "NO")
- Performance Tests: $([ $RUN_PERFORMANCE_TESTS -eq 1 ] && echo "YES" || echo "NO")
- Memory Tests: $([ $RUN_MEMORY_TESTS -eq 1 ] && echo "YES" || echo "NO")
- Stress Tests: $([ $RUN_STRESS_TESTS -eq 1 ] && echo "YES" || echo "NO")
- Verbose Mode: $([ $VERBOSE -eq 1 ] && echo "YES" || echo "NO")
- Clean Build: $([ $CLEAN_BUILD -eq 1 ] && echo "YES" || echo "NO")

Environment:
- Operating System: $(uname -s)
- Architecture: $(uname -m)
- Compiler: $(g++ --version | head -n1)
- Make: $(make --version | head -n1)

Test Results Directory: $TEST_LOG_DIR

Individual Test Logs:
EOF
    
    # List all test log files
    find "$TEST_LOG_DIR" -name "*_${TIMESTAMP}.log" -type f | while read logfile; do
        echo "- $(basename "$logfile")" >> "$report_file"
    done
    
    print_success "Test report generated: $report_file"
}

# Function to cleanup
cleanup() {
    print_status "Cleaning up test environment..."
    
    cd "$SRC_DIR"
    if [[ $CLEAN_BUILD -eq 1 ]]; then
        make -f gin_test.mk clean > /dev/null 2>&1 || true
    fi
    cd - > /dev/null
    
    print_success "Cleanup completed"
}

# Main execution function
main() {
    print_header "ScratchBird GIN Index Test Suite"
    
    local start_time=$(date +%s)
    local failed_tests=0
    
    # Setup
    check_prerequisites
    setup_test_environment
    build_tests
    
    # Run tests
    run_unit_tests || ((failed_tests++))
    run_functional_tests || ((failed_tests++))
    run_performance_tests
    run_memory_tests || ((failed_tests++))
    run_stress_tests
    
    # Generate report
    generate_test_report
    
    # Summary
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))
    
    print_header "Test Suite Summary"
    echo "Total execution time: ${duration} seconds"
    echo "Test results directory: $TEST_LOG_DIR"
    
    if [[ $failed_tests -eq 0 ]]; then
        print_success "All critical tests PASSED! 🎉"
        echo "The GIN Index implementation is ready for integration."
    else
        print_error "$failed_tests critical test suite(s) FAILED! ❌"
        echo "Please review the test logs and fix the issues before proceeding."
        return 1
    fi
    
    # Cleanup
    if [[ $CLEAN_BUILD -eq 1 ]]; then
        cleanup
    fi
    
    return 0
}

# Parse arguments and run main function
parse_arguments "$@"
main

exit $?