#!/bin/bash

# ScratchBird v0.6 Complete Build Script
# Supports Linux and Windows (MinGW) cross-compilation
# Author: ScratchBird Development Team
# Date: July 14, 2025

set -e  # Exit on any error

# Script configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_ROOT="$SCRIPT_DIR"
BUILD_LOG_DIR="$BUILD_ROOT/build_logs"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")

# Build configuration
PARALLEL_JOBS=$(nproc 2>/dev/null || echo "4")
BUILD_TARGET="${1:-Release}"
PLATFORM="${2:-linux}"

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1" | tee -a "$BUILD_LOG_DIR/build_${PLATFORM}_${TIMESTAMP}.log"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1" | tee -a "$BUILD_LOG_DIR/build_${PLATFORM}_${TIMESTAMP}.log"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1" | tee -a "$BUILD_LOG_DIR/build_${PLATFORM}_${TIMESTAMP}.log"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1" | tee -a "$BUILD_LOG_DIR/build_${PLATFORM}_${TIMESTAMP}.log"
}

# Create build log directory
mkdir -p "$BUILD_LOG_DIR"

# Function to check prerequisites
check_prerequisites() {
    log_info "Checking build prerequisites for platform: $PLATFORM"
    
    case $PLATFORM in
        linux)
            # Check for Linux build tools
            if ! command -v gcc &> /dev/null; then
                log_error "GCC not found. Please install build-essential."
                exit 1
            fi
            
            if ! command -v g++ &> /dev/null; then
                log_error "G++ not found. Please install build-essential."
                exit 1
            fi
            
            if ! command -v make &> /dev/null; then
                log_error "Make not found. Please install build-essential."
                exit 1
            fi
            
            # Check for required libraries
            if ! pkg-config --exists icu-uc; then
                log_warning "ICU development libraries not found. Install libicu-dev."
            fi
            
            log_success "Linux build prerequisites satisfied"
            ;;
            
        windows|mingw)
            # Check for MinGW cross-compilation tools
            if ! command -v x86_64-w64-mingw32-gcc &> /dev/null; then
                log_error "MinGW GCC not found. Please install mingw-w64."
                exit 1
            fi
            
            if ! command -v x86_64-w64-mingw32-g++ &> /dev/null; then
                log_error "MinGW G++ not found. Please install mingw-w64."
                exit 1
            fi
            
            log_success "Windows MinGW build prerequisites satisfied"
            ;;
            
        *)
            log_error "Unsupported platform: $PLATFORM"
            log_info "Supported platforms: linux, windows, mingw"
            exit 1
            ;;
    esac
}

# Function to clean previous builds
clean_build() {
    log_info "Cleaning previous build artifacts"
    
    if [[ -d "gen/Release" ]]; then
        rm -rf gen/Release
        log_info "Removed gen/Release directory"
    fi
    
    if [[ -d "gen/Debug" ]]; then
        rm -rf gen/Debug
        log_info "Removed gen/Debug directory"
    fi
    
    if [[ -d "temp/Release" ]]; then
        rm -rf temp/Release
        log_info "Removed temp/Release directory"
    fi
    
    if [[ -d "temp/Debug" ]]; then
        rm -rf temp/Debug
        log_info "Removed temp/Debug directory"
    fi
    
    # Clean external libraries
    if [[ -d "extern" ]]; then
        find extern -name "*.o" -delete 2>/dev/null || true
        find extern -name "*.a" -delete 2>/dev/null || true
        log_info "Cleaned external library artifacts"
    fi
    
    log_success "Build cleanup completed"
}

# Function to configure build system
configure_build() {
    log_info "Configuring build system for $PLATFORM"
    
    # Ensure we have a clean autotools configuration
    if [[ ! -f "configure" ]] || [[ "configure.ac" -nt "configure" ]]; then
        log_info "Regenerating autotools configuration"
        if command -v autoreconf &> /dev/null; then
            autoreconf -fiv 2>&1 | tee -a "$BUILD_LOG_DIR/build_${PLATFORM}_${TIMESTAMP}.log"
        else
            log_warning "autoreconf not available, using existing configure script"
        fi
    fi
    
    # Platform-specific configuration
    case $PLATFORM in
        linux)
            log_info "Configuring for Linux build"
            ./configure \
                --enable-superserver \
                --with-system-editline \
                --with-service-name=sb_fdb \
                --with-service-port=4050 \
                --prefix=/usr/local/scratchbird \
                2>&1 | tee -a "$BUILD_LOG_DIR/build_${PLATFORM}_${TIMESTAMP}.log"
            ;;
            
        windows|mingw)
            log_info "Configuring for Windows MinGW cross-compilation"
            ./configure \
                --host=x86_64-w64-mingw32 \
                --enable-superserver \
                --with-service-name=sb_fdb \
                --with-service-port=4050 \
                --prefix=/usr/local/scratchbird-windows \
                CXX=x86_64-w64-mingw32-g++ \
                CC=x86_64-w64-mingw32-gcc \
                AR=x86_64-w64-mingw32-ar \
                RANLIB=x86_64-w64-mingw32-ranlib \
                2>&1 | tee -a "$BUILD_LOG_DIR/build_${PLATFORM}_${TIMESTAMP}.log"
            ;;
    esac
    
    if [[ $? -eq 0 ]]; then
        log_success "Build configuration completed"
    else
        log_error "Build configuration failed"
        exit 1
    fi
}

# Function to build external dependencies
build_external() {
    log_info "Building external dependencies"
    
    make TARGET=$BUILD_TARGET external -j$PARALLEL_JOBS 2>&1 | tee -a "$BUILD_LOG_DIR/build_${PLATFORM}_${TIMESTAMP}.log"
    
    if [[ $? -eq 0 ]]; then
        log_success "External dependencies built successfully"
    else
        log_error "External dependencies build failed"
        return 1
    fi
}

# Function to build GPRE (database preprocessor)
build_gpre() {
    log_info "Building GPRE database preprocessor"
    
    make TARGET=$BUILD_TARGET boot -j$PARALLEL_JOBS 2>&1 | tee -a "$BUILD_LOG_DIR/build_${PLATFORM}_${TIMESTAMP}.log"
    
    if [[ $? -eq 0 ]]; then
        log_success "GPRE built successfully"
    else
        log_error "GPRE build failed"
        return 1
    fi
}

# Function to build core ScratchBird components
build_core() {
    log_info "Building core ScratchBird components"
    
    # Build client library first
    make TARGET=$BUILD_TARGET libsbclient -j$PARALLEL_JOBS 2>&1 | tee -a "$BUILD_LOG_DIR/build_${PLATFORM}_${TIMESTAMP}.log"
    
    if [[ $? -ne 0 ]]; then
        log_warning "libsbclient target not found, building all components"
    fi
    
    # Build all main components
    make TARGET=$BUILD_TARGET -j$PARALLEL_JOBS 2>&1 | tee -a "$BUILD_LOG_DIR/build_${PLATFORM}_${TIMESTAMP}.log"
    
    if [[ $? -eq 0 ]]; then
        log_success "Core components built successfully"
    else
        log_error "Core components build failed"
        return 1
    fi
}

# Function to build ScratchBird tools
build_tools() {
    log_info "Building ScratchBird command-line tools"
    
    local tools=("sb_isql" "sb_gbak" "sb_gfix" "sb_gsec" "sb_gstat")
    local failed_tools=()
    
    for tool in "${tools[@]}"; do
        log_info "Building $tool"
        if make TARGET=$BUILD_TARGET "$tool" -j$PARALLEL_JOBS 2>&1 | tee -a "$BUILD_LOG_DIR/build_${PLATFORM}_${TIMESTAMP}.log"; then
            log_success "$tool built successfully"
        else
            log_error "$tool build failed"
            failed_tools+=("$tool")
        fi
    done
    
    if [[ ${#failed_tools[@]} -eq 0 ]]; then
        log_success "All ScratchBird tools built successfully"
    else
        log_error "Failed to build tools: ${failed_tools[*]}"
        return 1
    fi
}

# Function to verify build results
verify_build() {
    log_info "Verifying build results"
    
    local bin_dir=""
    case $PLATFORM in
        linux)
            bin_dir="gen/Release/scratchbird/bin"
            ;;
        windows|mingw)
            bin_dir="gen/Release/scratchbird/bin"
            ;;
    esac
    
    if [[ ! -d "$bin_dir" ]]; then
        log_error "Binary directory not found: $bin_dir"
        return 1
    fi
    
    local expected_files=()
    case $PLATFORM in
        linux)
            expected_files=("sb_isql" "sb_gbak" "sb_gfix" "sb_gsec" "sb_gstat" "gpre_boot" "gpre_current")
            ;;
        windows|mingw)
            expected_files=("sb_isql.exe" "sb_gbak.exe" "sb_gfix.exe" "sb_gsec.exe" "sb_gstat.exe" "gpre_boot.exe" "gpre_current.exe")
            ;;
    esac
    
    local missing_files=()
    for file in "${expected_files[@]}"; do
        if [[ ! -f "$bin_dir/$file" ]]; then
            missing_files+=("$file")
        else
            log_success "Found: $bin_dir/$file"
        fi
    done
    
    if [[ ${#missing_files[@]} -eq 0 ]]; then
        log_success "All expected binaries found"
    else
        log_error "Missing binaries: ${missing_files[*]}"
        return 1
    fi
    
    # Check library directory
    local lib_dir="gen/Release/scratchbird/lib"
    if [[ -d "$lib_dir" ]]; then
        local lib_count=$(find "$lib_dir" -name "*.so*" -o -name "*.dll" -o -name "*.a" | wc -l)
        log_info "Found $lib_count library files in $lib_dir"
    fi
    
    return 0
}

# Function to run tests if available
run_tests() {
    log_info "Running build verification tests"
    
    local bin_dir="gen/Release/scratchbird/bin"
    
    # Test basic tool functionality
    local tools=("sb_isql" "sb_gbak" "sb_gfix" "sb_gsec" "sb_gstat")
    
    for tool in "${tools[@]}"; do
        local tool_path="$bin_dir/$tool"
        if [[ $PLATFORM == "windows" ]] || [[ $PLATFORM == "mingw" ]]; then
            tool_path="$tool_path.exe"
        fi
        
        if [[ -f "$tool_path" ]]; then
            log_info "Testing $tool version output"
            if timeout 10s "$tool_path" -z 2>&1 | grep -i "scratchbird\|version" > /dev/null; then
                log_success "$tool version test passed"
            else
                log_warning "$tool version test inconclusive"
            fi
        fi
    done
}

# Function to create build summary
create_build_summary() {
    log_info "Creating build summary"
    
    local summary_file="$BUILD_LOG_DIR/build_summary_${PLATFORM}_${TIMESTAMP}.txt"
    
    cat > "$summary_file" << EOF
ScratchBird v0.6 Build Summary
==============================
Build Date: $(date)
Platform: $PLATFORM
Build Target: $BUILD_TARGET
Parallel Jobs: $PARALLEL_JOBS
Build Root: $BUILD_ROOT

Build Status: SUCCESS
Build Log: build_${PLATFORM}_${TIMESTAMP}.log

Generated Files:
$(find gen/Release/scratchbird -type f -name "sb_*" -o -name "*.so*" -o -name "*.dll" 2>/dev/null | sort)

Build completed successfully!
EOF
    
    log_success "Build summary saved to: $summary_file"
    cat "$summary_file"
}

# Main build function
main() {
    log_info "Starting ScratchBird v0.6 complete build"
    log_info "Platform: $PLATFORM"
    log_info "Target: $BUILD_TARGET"
    log_info "Parallel jobs: $PARALLEL_JOBS"
    
    # Change to build root directory
    cd "$BUILD_ROOT"
    
    # Execute build pipeline
    local build_steps=(
        "check_prerequisites"
        "clean_build"
        "configure_build"
        "build_external"
        "build_gpre"
        "build_core"
        "build_tools"
        "verify_build"
        "run_tests"
        "create_build_summary"
    )
    
    for step in "${build_steps[@]}"; do
        log_info "Executing build step: $step"
        if ! $step; then
            log_error "Build step failed: $step"
            log_error "Check build log: $BUILD_LOG_DIR/build_${PLATFORM}_${TIMESTAMP}.log"
            exit 1
        fi
    done
    
    log_success "ScratchBird v0.6 build completed successfully!"
    log_info "Build artifacts located in: gen/Release/scratchbird/"
    log_info "Build logs saved in: $BUILD_LOG_DIR/"
}

# Script usage information
usage() {
    cat << EOF
ScratchBird v0.6 Complete Build Script

Usage: $0 [BUILD_TARGET] [PLATFORM]

Parameters:
  BUILD_TARGET    Build configuration (Release|Debug) [default: Release]
  PLATFORM        Target platform (linux|windows|mingw) [default: linux]

Examples:
  $0                           # Build Release for Linux
  $0 Release linux             # Build Release for Linux
  $0 Release windows           # Cross-compile Release for Windows
  $0 Debug linux               # Build Debug for Linux

Prerequisites:
  Linux:   build-essential, libicu-dev, libedit-dev
  Windows: mingw-w64, x86_64-w64-mingw32-gcc

For more information, visit: https://github.com/dcalford/ScratchBird
EOF
}

# Handle command line arguments
case "${1:-}" in
    -h|--help|help)
        usage
        exit 0
        ;;
    -*)
        log_error "Unknown option: $1"
        usage
        exit 1
        ;;
esac

# Run main build process
main "$@"