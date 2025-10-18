#!/bin/bash
# ScratchBird Database - Local Sanitizer Testing Script
# This script runs various sanitizers and static analysis tools locally
# before committing code to catch issues early.

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${BUILD_DIR:-${PROJECT_ROOT}/build}"

echo -e "${BLUE}=== ScratchBird Sanitizer Test Suite ===${NC}"
echo "Project root: $PROJECT_ROOT"
echo "Build directory: $BUILD_DIR"
echo ""

# Function to print section headers
print_section() {
    echo ""
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""
}

# Function to run a command and capture status
run_test() {
    local test_name="$1"
    shift
    echo -e "${YELLOW}Running: $test_name${NC}"
    if "$@"; then
        echo -e "${GREEN}✓ $test_name passed${NC}"
        return 0
    else
        echo -e "${RED}✗ $test_name failed${NC}"
        return 1
    fi
}

# Parse command line arguments
RUN_TSAN=false
RUN_ASAN=false
RUN_HELGRIND=false
RUN_VALGRIND=false
RUN_CLANG_TIDY=false
RUN_ALL=false

if [ $# -eq 0 ]; then
    RUN_ALL=true
fi

while [[ $# -gt 0 ]]; do
    case $1 in
        --tsan)
            RUN_TSAN=true
            shift
            ;;
        --asan)
            RUN_ASAN=true
            shift
            ;;
        --helgrind)
            RUN_HELGRIND=true
            shift
            ;;
        --valgrind|--leak)
            RUN_VALGRIND=true
            shift
            ;;
        --clang-tidy|--tidy)
            RUN_CLANG_TIDY=true
            shift
            ;;
        --all)
            RUN_ALL=true
            shift
            ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --tsan          Run ThreadSanitizer"
            echo "  --asan          Run AddressSanitizer + LeakSanitizer + UBSanitizer"
            echo "  --helgrind      Run Helgrind (Valgrind race detector)"
            echo "  --valgrind      Run Valgrind memcheck (memory leak detection)"
            echo "  --clang-tidy    Run Clang-Tidy static analysis"
            echo "  --all           Run all sanitizers (default if no options given)"
            echo "  --help          Show this help message"
            echo ""
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# If --all is set, enable everything
if [ "$RUN_ALL" = true ]; then
    RUN_TSAN=true
    RUN_ASAN=true
    RUN_HELGRIND=true
    RUN_VALGRIND=true
    RUN_CLANG_TIDY=true
fi

# Counter for failures
TOTAL_FAILURES=0

# ==============================================================================
# ThreadSanitizer (TSAN)
# ==============================================================================
if [ "$RUN_TSAN" = true ]; then
    print_section "ThreadSanitizer (TSAN) - Data Race Detection"

    TSAN_BUILD_DIR="${BUILD_DIR}_tsan"
    mkdir -p "$TSAN_BUILD_DIR"

    echo "Configuring build with ThreadSanitizer..."
    cmake -B "$TSAN_BUILD_DIR" -S "$PROJECT_ROOT" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_CXX_FLAGS="-fsanitize=thread -g -O1" \
        -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread" \
        -DCMAKE_SHARED_LINKER_FLAGS="-fsanitize=thread"

    echo "Building with ThreadSanitizer..."
    cmake --build "$TSAN_BUILD_DIR" --config Debug -j$(nproc)

    echo "Running TSAN tests..."
    cd "$TSAN_BUILD_DIR"
    if run_test "TSAN" ctest -R "tsan_|StatisticsAccuracy|PageManagerDestructor" --output-on-failure --timeout 300; then
        echo -e "${GREEN}TSAN: No data races detected${NC}"
    else
        TOTAL_FAILURES=$((TOTAL_FAILURES + 1))
        echo -e "${RED}TSAN: Data races detected or tests failed${NC}"
    fi
    cd "$PROJECT_ROOT"
fi

# ==============================================================================
# AddressSanitizer (ASAN) + LeakSanitizer + UBSanitizer
# ==============================================================================
if [ "$RUN_ASAN" = true ]; then
    print_section "AddressSanitizer (ASAN) - Memory Error Detection"

    ASAN_BUILD_DIR="${BUILD_DIR}_asan"
    mkdir -p "$ASAN_BUILD_DIR"

    echo "Configuring build with AddressSanitizer..."
    cmake -B "$ASAN_BUILD_DIR" -S "$PROJECT_ROOT" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_CXX_FLAGS="-fsanitize=address -fsanitize=undefined -fsanitize=leak -fno-omit-frame-pointer -g -O1" \
        -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address -fsanitize=undefined -fsanitize=leak" \
        -DCMAKE_SHARED_LINKER_FLAGS="-fsanitize=address -fsanitize=undefined -fsanitize=leak"

    echo "Building with AddressSanitizer..."
    cmake --build "$ASAN_BUILD_DIR" --config Debug -j$(nproc)

    echo "Running ASAN tests..."
    cd "$ASAN_BUILD_DIR"
    export ASAN_OPTIONS="detect_leaks=1:check_initialization_order=1:strict_init_order=1:detect_stack_use_after_return=1:verbosity=0:halt_on_error=0"
    export LSAN_OPTIONS="suppressions=${SCRIPT_DIR}/lsan_suppressions.txt:print_suppressions=0"
    export UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=0"

    if run_test "ASAN" ctest --output-on-failure --timeout 300; then
        echo -e "${GREEN}ASAN: No memory errors detected${NC}"
    else
        TOTAL_FAILURES=$((TOTAL_FAILURES + 1))
        echo -e "${RED}ASAN: Memory errors detected or tests failed${NC}"
    fi
    cd "$PROJECT_ROOT"
fi

# ==============================================================================
# Helgrind - Race Condition Detection
# ==============================================================================
if [ "$RUN_HELGRIND" = true ]; then
    print_section "Helgrind - Race Condition Detection"

    # Check if valgrind is installed
    if ! command -v valgrind &> /dev/null; then
        echo -e "${YELLOW}Warning: valgrind not found, skipping Helgrind tests${NC}"
        echo "Install with: sudo apt-get install valgrind"
    else
        # Use regular debug build
        if [ ! -d "$BUILD_DIR" ]; then
            echo "Creating debug build..."
            cmake -B "$BUILD_DIR" -S "$PROJECT_ROOT" -DCMAKE_BUILD_TYPE=Debug
            cmake --build "$BUILD_DIR" --config Debug -j$(nproc)
        fi

        cd "$BUILD_DIR"

        # Run Helgrind on dedicated test if it exists
        if [ -f tests/helgrind_races ]; then
            echo "Running Helgrind on dedicated race tests..."
            if valgrind --tool=helgrind \
                --log-file=helgrind-races.log \
                --error-exitcode=1 \
                --suppressions="${SCRIPT_DIR}/helgrind_suppressions.txt" \
                ./tests/helgrind_races; then
                echo -e "${GREEN}Helgrind (dedicated tests): No races detected${NC}"
            else
                TOTAL_FAILURES=$((TOTAL_FAILURES + 1))
                echo -e "${RED}Helgrind (dedicated tests): Races detected${NC}"
                echo "See: ${BUILD_DIR}/helgrind-races.log"
            fi
        fi

        # Run Helgrind on concurrency tests
        echo "Running Helgrind on concurrency tests..."
        for test in ConcurrentPageAccess MultithreadedStress; do
            if ctest -N | grep -q "$test"; then
                timeout 600 valgrind --tool=helgrind \
                    --log-file=helgrind-${test}.log \
                    --error-exitcode=0 \
                    --suppressions="${SCRIPT_DIR}/helgrind_suppressions.txt" \
                    $(ctest -N -R "$test" | grep "Test command:" | sed 's/.*Test command: //') || true

                # Check results
                if grep -q "ERROR SUMMARY: 0 errors" helgrind-${test}.log 2>/dev/null; then
                    echo -e "${GREEN}Helgrind ($test): No races detected${NC}"
                else
                    TOTAL_FAILURES=$((TOTAL_FAILURES + 1))
                    echo -e "${RED}Helgrind ($test): Races may be present${NC}"
                    echo "See: ${BUILD_DIR}/helgrind-${test}.log"
                fi
            fi
        done

        cd "$PROJECT_ROOT"
    fi
fi

# ==============================================================================
# Valgrind Memcheck - Memory Leak Detection
# ==============================================================================
if [ "$RUN_VALGRIND" = true ]; then
    print_section "Valgrind Memcheck - Memory Leak Detection"

    # Check if valgrind is installed
    if ! command -v valgrind &> /dev/null; then
        echo -e "${YELLOW}Warning: valgrind not found, skipping leak detection${NC}"
        echo "Install with: sudo apt-get install valgrind"
    else
        # Use regular debug build
        if [ ! -d "$BUILD_DIR" ]; then
            echo "Creating debug build..."
            cmake -B "$BUILD_DIR" -S "$PROJECT_ROOT" -DCMAKE_BUILD_TYPE=Debug
            cmake --build "$BUILD_DIR" --config Debug -j$(nproc)
        fi

        cd "$BUILD_DIR"

        # Run leak detection on resource-intensive tests
        for test in ExceptionInjection BufferPoolExhaustion PageManagerDestructor; do
            if ctest -N | grep -q "$test"; then
                echo "Running leak detection on $test..."
                timeout 600 valgrind \
                    --tool=memcheck \
                    --leak-check=full \
                    --show-leak-kinds=definite,possible \
                    --track-origins=yes \
                    --log-file=valgrind-${test}.log \
                    --error-exitcode=0 \
                    --suppressions="${SCRIPT_DIR}/valgrind_suppressions.txt" \
                    $(ctest -N -R "$test" | grep "Test command:" | sed 's/.*Test command: //') || true

                # Check for definitely lost bytes
                definitely_lost=$(grep "definitely lost:" valgrind-${test}.log 2>/dev/null | awk '{print $4}' | sed 's/,//g' || echo "0")
                if [ "$definitely_lost" = "0" ]; then
                    echo -e "${GREEN}Leak check ($test): No definite leaks${NC}"
                else
                    TOTAL_FAILURES=$((TOTAL_FAILURES + 1))
                    echo -e "${RED}Leak check ($test): $definitely_lost bytes definitely lost${NC}"
                    echo "See: ${BUILD_DIR}/valgrind-${test}.log"
                fi
            fi
        done

        cd "$PROJECT_ROOT"
    fi
fi

# ==============================================================================
# Clang-Tidy - Static Analysis
# ==============================================================================
if [ "$RUN_CLANG_TIDY" = true ]; then
    print_section "Clang-Tidy - Static Analysis"

    # Check if clang-tidy is installed
    if ! command -v clang-tidy &> /dev/null; then
        echo -e "${YELLOW}Warning: clang-tidy not found, skipping static analysis${NC}"
        echo "Install with: sudo apt-get install clang-tidy"
    else
        # Use clang build for compile_commands.json
        TIDY_BUILD_DIR="${BUILD_DIR}_tidy"
        mkdir -p "$TIDY_BUILD_DIR"

        echo "Configuring build with Clang for compile_commands.json..."
        cmake -B "$TIDY_BUILD_DIR" -S "$PROJECT_ROOT" \
            -DCMAKE_BUILD_TYPE=Debug \
            -DCMAKE_CXX_COMPILER=clang++ \
            -DCMAKE_C_COMPILER=clang \
            -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

        echo "Running Clang-Tidy on source files..."
        cd "$TIDY_BUILD_DIR"

        TIDY_ERRORS=0
        find "$PROJECT_ROOT/src" -name "*.cpp" -type f | while read file; do
            echo "Analyzing: $(basename $file)"
            if ! clang-tidy "$file" -p=. \
                --checks='bugprone-*,clang-analyzer-*,cppcoreguidelines-*,concurrency-*' \
                2>&1 | tee -a clang-tidy.log | grep -q "error:"; then
                echo -e "${GREEN}  ✓ $(basename $file)${NC}"
            else
                echo -e "${RED}  ✗ $(basename $file) has errors${NC}"
                TIDY_ERRORS=$((TIDY_ERRORS + 1))
            fi
        done

        # Check for critical warnings
        if grep -E "(warning:|error:)" clang-tidy.log | grep -qE "(bounds|overflow|use-after|null|leak|race)"; then
            TOTAL_FAILURES=$((TOTAL_FAILURES + 1))
            echo -e "${RED}Clang-Tidy: Critical warnings found${NC}"
            grep -E "(warning:|error:)" clang-tidy.log | grep -E "(bounds|overflow|use-after|null|leak|race)" | head -20
            echo "Full log: ${TIDY_BUILD_DIR}/clang-tidy.log"
        else
            echo -e "${GREEN}Clang-Tidy: No critical issues found${NC}"
        fi

        cd "$PROJECT_ROOT"
    fi
fi

# ==============================================================================
# Summary
# ==============================================================================
print_section "Summary"

if [ $TOTAL_FAILURES -eq 0 ]; then
    echo -e "${GREEN}✓ All sanitizer checks passed!${NC}"
    echo -e "${GREEN}Code is ready for commit.${NC}"
    exit 0
else
    echo -e "${RED}✗ $TOTAL_FAILURES check(s) failed${NC}"
    echo -e "${YELLOW}Please fix the issues before committing.${NC}"
    exit 1
fi
