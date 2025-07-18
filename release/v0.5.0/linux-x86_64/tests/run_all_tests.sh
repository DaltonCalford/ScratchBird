#!/bin/bash

#
# ScratchBird v0.5.0 - Master Test Runner Script
#
# This script runs all ScratchBird tests in sequence, providing comprehensive
# validation of the entire product lifecycle including:
# - Hierarchical schema functionality (8-level deep)
# - PostgreSQL-compatible data types (INET, CIDR, MACADDR, UUID, etc.)
# - UDR (User Defined Routines) functionality
# - Package functionality
# - Database links
# - Performance benchmarks
# - Regression tests
# - Stress tests
#
# Usage: ./run_all_tests.sh [options]
#   -v, --verbose    Enable verbose output
#   -q, --quiet      Suppress non-essential output
#   -h, --help       Show this help message
#
# Output: All results are written to test_results.txt with timing information
#
# Copyright (c) 2025 ScratchBird Development Team
# All Rights Reserved.
#

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTPUT_FILE="$SCRIPT_DIR/test_results.txt"
SB_ISQL_PATH="../gen/Release/scratchbird/bin/sb_isql"
TEST_DB_DIR="$SCRIPT_DIR/test_databases"
VERBOSE=false
QUIET=false

# ANSI color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Test categories and their scripts
declare -A TEST_CATEGORIES=(
    ["hierarchical_schema"]="test_hierarchical_schemas.sh"
    ["network_datatypes"]="test_network_datatypes.sh"
    ["udr_functionality"]="test_udr_functionality.sh"
    ["package_functionality"]="test_package_functionality.sh"
    ["database_links"]="test_database_links.sh"
    ["performance_benchmarks"]="test_performance_benchmarks.sh"
    ["regression_tests"]="test_regression_tests.sh"
    ["stress_tests"]="test_stress_tests.sh"
)

# Function to print colored output
print_colored() {
    local color=$1
    local message=$2
    if [[ $QUIET != true ]]; then
        echo -e "${color}${message}${NC}"
    fi
}

# Function to log with timestamp
log_with_timestamp() {
    local message=$1
    local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    echo "[$timestamp] $message" >> "$OUTPUT_FILE"
    if [[ $VERBOSE == true ]]; then
        print_colored $BLUE "[$timestamp] $message"
    fi
}

# Function to show usage
show_usage() {
    echo "ScratchBird v0.5.0 - Master Test Runner"
    echo "======================================"
    echo ""
    echo "Usage: $0 [options]"
    echo ""
    echo "Options:"
    echo "  -v, --verbose    Enable verbose output"
    echo "  -q, --quiet      Suppress non-essential output"
    echo "  -h, --help       Show this help message"
    echo ""
    echo "Test Categories:"
    echo "  1. Hierarchical Schema Tests"
    echo "  2. Network Data Type Tests"
    echo "  3. UDR Functionality Tests"
    echo "  4. Package Functionality Tests"
    echo "  5. Database Link Tests"
    echo "  6. Performance Benchmark Tests"
    echo "  7. Regression Tests"
    echo "  8. Stress Tests"
    echo ""
    echo "Output: test_results.txt"
    echo ""
}

# Function to check prerequisites
check_prerequisites() {
    print_colored $YELLOW "Checking prerequisites..."
    
    # Check if sb_isql exists
    if [[ ! -f "$SB_ISQL_PATH" ]]; then
        print_colored $RED "ERROR: sb_isql not found at $SB_ISQL_PATH"
        print_colored $RED "Please build ScratchBird first: make TARGET=Release"
        exit 1
    fi
    
    # Check sb_isql version
    local version_output=$($SB_ISQL_PATH -z 2>&1)
    if [[ $? -ne 0 ]]; then
        print_colored $RED "ERROR: sb_isql is not working properly"
        exit 1
    fi
    
    print_colored $GREEN "sb_isql version: $version_output"
    
    # Create test database directory
    mkdir -p "$TEST_DB_DIR"
    
    # Make all test scripts executable
    chmod +x "$SCRIPT_DIR"/test_*.sh 2>/dev/null || true
    
    print_colored $GREEN "Prerequisites check passed"
}

# Function to run a single test category
run_test_category() {
    local category=$1
    local script_name=$2
    local script_path="$SCRIPT_DIR/$script_name"
    
    print_colored $PURPLE "Running $category tests..."
    
    if [[ ! -f "$script_path" ]]; then
        print_colored $RED "WARNING: Test script not found: $script_path"
        log_with_timestamp "SKIPPED: $category - Script not found"
        return 1
    fi
    
    # Record start time
    local start_time=$(date +%s.%N)
    
    # Log test start
    log_with_timestamp "STARTING: $category"
    
    # Run the test script
    cd "$SCRIPT_DIR"
    if [[ $VERBOSE == true ]]; then
        bash "$script_path" 2>&1 | tee -a "$OUTPUT_FILE"
    else
        bash "$script_path" >> "$OUTPUT_FILE" 2>&1
    fi
    
    local exit_code=$?
    
    # Calculate elapsed time
    local end_time=$(date +%s.%N)
    local elapsed_time=$(echo "$end_time - $start_time" | bc -l)
    
    # Log results
    if [[ $exit_code -eq 0 ]]; then
        print_colored $GREEN "✓ $category tests completed in ${elapsed_time}s"
        log_with_timestamp "COMPLETED: $category - Elapsed time: ${elapsed_time}s"
    else
        print_colored $RED "✗ $category tests failed in ${elapsed_time}s"
        log_with_timestamp "FAILED: $category - Exit code: $exit_code - Elapsed time: ${elapsed_time}s"
    fi
    
    return $exit_code
}

# Function to generate test summary
generate_summary() {
    local total_tests=$1
    local passed_tests=$2
    local failed_tests=$3
    local skipped_tests=$4
    local total_time=$5
    
    print_colored $CYAN "Test Summary"
    print_colored $CYAN "============"
    echo ""
    
    # Write summary to output file
    {
        echo ""
        echo "========================================="
        echo "ScratchBird v0.5.0 Test Summary"
        echo "========================================="
        echo "Total test categories: $total_tests"
        echo "Passed: $passed_tests"
        echo "Failed: $failed_tests"
        echo "Skipped: $skipped_tests"
        echo "Total execution time: ${total_time}s"
        echo "Test completed at: $(date)"
        echo "========================================="
    } >> "$OUTPUT_FILE"
    
    # Print summary to console
    print_colored $BLUE "Total test categories: $total_tests"
    print_colored $GREEN "Passed: $passed_tests"
    print_colored $RED "Failed: $failed_tests"
    print_colored $YELLOW "Skipped: $skipped_tests"
    print_colored $CYAN "Total execution time: ${total_time}s"
    
    # Return appropriate exit code
    if [[ $failed_tests -gt 0 ]]; then
        return 1
    else
        return 0
    fi
}

# Main function
main() {
    # Parse command line arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            -v|--verbose)
                VERBOSE=true
                shift
                ;;
            -q|--quiet)
                QUIET=true
                shift
                ;;
            -h|--help)
                show_usage
                exit 0
                ;;
            *)
                print_colored $RED "Unknown option: $1"
                show_usage
                exit 1
                ;;
        esac
    done
    
    # Initialize output file
    {
        echo "========================================="
        echo "ScratchBird v0.5.0 - Test Suite Results"
        echo "========================================="
        echo "Test started at: $(date)"
        echo "Test directory: $SCRIPT_DIR"
        echo "Output file: $OUTPUT_FILE"
        echo "sb_isql path: $SB_ISQL_PATH"
        echo "========================================="
        echo ""
    } > "$OUTPUT_FILE"
    
    # Print banner
    print_colored $CYAN "ScratchBird v0.5.0 - Test Suite"
    print_colored $CYAN "================================"
    echo ""
    
    # Check prerequisites
    check_prerequisites
    
    # Record overall start time
    local overall_start_time=$(date +%s.%N)
    
    # Initialize counters
    local total_tests=0
    local passed_tests=0
    local failed_tests=0
    local skipped_tests=0
    
    # Run all test categories
    for category in "${!TEST_CATEGORIES[@]}"; do
        script_name="${TEST_CATEGORIES[$category]}"
        ((total_tests++))
        
        if run_test_category "$category" "$script_name"; then
            ((passed_tests++))
        else
            if [[ -f "$SCRIPT_DIR/$script_name" ]]; then
                ((failed_tests++))
            else
                ((skipped_tests++))
            fi
        fi
        
        echo "" # Add spacing between test categories
    done
    
    # Calculate total execution time
    local overall_end_time=$(date +%s.%N)
    local total_time=$(echo "$overall_end_time - $overall_start_time" | bc -l)
    
    # Generate summary
    generate_summary $total_tests $passed_tests $failed_tests $skipped_tests $total_time
    local summary_exit_code=$?
    
    # Final message
    if [[ $summary_exit_code -eq 0 ]]; then
        print_colored $GREEN "All tests completed successfully!"
        print_colored $GREEN "Results saved to: $OUTPUT_FILE"
    else
        print_colored $RED "Some tests failed. Check $OUTPUT_FILE for details."
    fi
    
    exit $summary_exit_code
}

# Run main function
main "$@"