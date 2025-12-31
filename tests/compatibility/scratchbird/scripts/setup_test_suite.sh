#!/bin/bash
# ScratchBird Test Suite Setup Script
# Initializes the test environment and validates prerequisites

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
BUILD_DIR="${ROOT_DIR}/../../build"

# Function: Print colored message
print_msg() {
    local color=$1
    shift
    echo -e "${color}$*${NC}"
}

# Function: Check prerequisites
check_prerequisites() {
    local all_ok=true

    print_msg "${BLUE}" "Checking prerequisites..."
    echo

    # Check CMake
    if command -v cmake &> /dev/null; then
        print_msg "${GREEN}" "✓ CMake found: $(cmake --version | head -1)"
    else
        print_msg "${RED}" "✗ CMake not found"
        all_ok=false
    fi

    # Check Make
    if command -v make &> /dev/null; then
        print_msg "${GREEN}" "✓ Make found: $(make --version | head -1)"
    else
        print_msg "${RED}" "✗ Make not found"
        all_ok=false
    fi

    # Check C++ compiler
    if command -v g++ &> /dev/null; then
        print_msg "${GREEN}" "✓ C++ Compiler found: $(g++ --version | head -1)"
    elif command -v clang++ &> /dev/null; then
        print_msg "${GREEN}" "✓ C++ Compiler found: $(clang++ --version | head -1)"
    else
        print_msg "${RED}" "✗ C++ Compiler not found"
        all_ok=false
    fi

    # Check Valgrind (optional)
    if command -v valgrind &> /dev/null; then
        print_msg "${GREEN}" "✓ Valgrind found: $(valgrind --version)"
    else
        print_msg "${YELLOW}" "⚠ Valgrind not found (optional, needed for memory tests)"
        print_msg "${YELLOW}" "  Install with: sudo apt-get install valgrind"
    fi

    # Check Python (for potential script extensions)
    if command -v python3 &> /dev/null; then
        print_msg "${GREEN}" "✓ Python found: $(python3 --version)"
    else
        print_msg "${YELLOW}" "⚠ Python3 not found (optional, may be needed for some scripts)"
    fi

    echo

    if [[ ${all_ok} == false ]]; then
        print_msg "${RED}" "Missing required prerequisites!"
        return 1
    fi

    print_msg "${GREEN}" "All required prerequisites found!"
    return 0
}

# Function: Create directory structure
create_directories() {
    print_msg "${BLUE}" "Creating directory structure..."

    local dirs=(
        "${ROOT_DIR}/results"
        "${ROOT_DIR}/results/valgrind"
        "${ROOT_DIR}/results/performance"
        "${ROOT_DIR}/config"
    )

    for dir in "${dirs[@]}"; do
        if [[ ! -d "${dir}" ]]; then
            mkdir -p "${dir}"
            print_msg "${GREEN}" "  Created: ${dir}"
        else
            print_msg "${YELLOW}" "  Exists: ${dir}"
        fi
    done

    echo
}

# Function: Build ScratchBird
build_scratchbird() {
    print_msg "${BLUE}" "Building ScratchBird..."
    echo

    # Create build directory
    if [[ ! -d "${BUILD_DIR}" ]]; then
        print_msg "${YELLOW}" "Creating build directory..."
        mkdir -p "${BUILD_DIR}"
    fi

    cd "${BUILD_DIR}"

    # Configure with CMake
    print_msg "${YELLOW}" "Configuring with CMake (Debug build)..."
    if cmake -DCMAKE_BUILD_TYPE=Debug .. > /dev/null 2>&1; then
        print_msg "${GREEN}" "✓ CMake configuration successful"
    else
        print_msg "${RED}" "✗ CMake configuration failed"
        return 1
    fi

    # Build
    print_msg "${YELLOW}" "Building (this may take a few minutes)..."
    local cpu_count=$(nproc 2>/dev/null || echo 4)

    if make -j${cpu_count} > /dev/null 2>&1; then
        print_msg "${GREEN}" "✓ Build successful"
    else
        print_msg "${RED}" "✗ Build failed"
        print_msg "${YELLOW}" "  Try running manually: cd ${BUILD_DIR} && cmake -DCMAKE_BUILD_TYPE=Debug .. && make"
        return 1
    fi

    cd - > /dev/null
    echo
}

# Function: Verify binaries
verify_binaries() {
    print_msg "${BLUE}" "Verifying built binaries..."
    echo

    local binaries=(
        "${BUILD_DIR}/sb_isql"
        "${BUILD_DIR}/sb_server"
    )

    local all_ok=true

    for binary in "${binaries[@]}"; do
        if [[ -x "${binary}" ]]; then
            print_msg "${GREEN}" "  ✓ Found: $(basename ${binary})"
        else
            print_msg "${RED}" "  ✗ Missing: $(basename ${binary})"
            all_ok=false
        fi
    done

    echo

    if [[ ${all_ok} == false ]]; then
        print_msg "${RED}" "Some binaries are missing!"
        return 1
    fi

    print_msg "${GREEN}" "All binaries verified!"
    return 0
}

# Function: Run quick smoke test
run_smoke_test() {
    print_msg "${BLUE}" "Running quick smoke test..."
    echo

    local test_db="/tmp/scratchbird_smoke_test.db"
    rm -f "${test_db}"

    # Create simple test
    cat <<'EOF' | "${BUILD_DIR}/sb_isql" > /dev/null 2>&1
CREATE DATABASE smoke_test;
\c smoke_test;
CREATE TABLE test (id INTEGER PRIMARY KEY, name VARCHAR(50));
INSERT INTO test VALUES (1, 'Smoke Test');
SELECT * FROM test;
DROP TABLE test;
DROP DATABASE smoke_test;
EOF

    if [[ $? -eq 0 ]]; then
        print_msg "${GREEN}" "✓ Smoke test passed!"
    else
        print_msg "${YELLOW}" "⚠ Smoke test had issues (this may be expected if features not fully implemented)"
    fi

    rm -f "${test_db}"
    echo
}

# Function: Print summary
print_summary() {
    print_msg "${BLUE}" "=========================================="
    print_msg "${BLUE}" "Setup Complete!"
    print_msg "${BLUE}" "=========================================="
    echo
    print_msg "${GREEN}" "ScratchBird test suite is ready to use."
    echo
    print_msg "${CYAN}" "Available test scripts:"
    echo "  ${SCRIPT_DIR}/run_tests.sh              - Run functional tests"
    echo "  ${SCRIPT_DIR}/run_valgrind_tests.sh     - Run memory leak tests"
    echo "  ${SCRIPT_DIR}/run_performance_tests.sh  - Run performance benchmarks"
    echo "  ${SCRIPT_DIR}/run_all_tests.sh          - Run all tests"
    echo
    print_msg "${CYAN}" "Quick start:"
    echo "  cd ${ROOT_DIR}"
    echo "  ./scripts/run_tests.sh                  # Run all functional tests"
    echo "  ./scripts/run_tests.sh -c basic         # Run basic tests only"
    echo "  ./scripts/run_tests.sh -v               # Verbose output"
    echo
    print_msg "${CYAN}" "Documentation:"
    echo "  ${ROOT_DIR}/README.md"
    echo
    print_msg "${GREEN}" "Happy testing! 🚀"
}

# Main script
main() {
    print_msg "${BLUE}" "=========================================="
    print_msg "${BLUE}" "ScratchBird Test Suite Setup"
    print_msg "${BLUE}" "=========================================="
    echo

    # Run setup steps
    check_prerequisites || exit 1
    create_directories
    build_scratchbird || exit 1
    verify_binaries || exit 1
    run_smoke_test

    # Print summary
    print_summary
}

main "$@"
