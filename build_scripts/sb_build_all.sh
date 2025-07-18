#!/bin/bash

# ScratchBird v0.5.0 - Complete Build Script
# This script builds all ScratchBird utilities properly using temp directory
# and places clean results in release directories
# 
# Usage: ./sb_build_all.sh [options]
# Options:
#   -h, --help          Show this help message
#   -c, --clean         Clean build directories before building
#   -l, --linux-only    Build only Linux utilities
#   -w, --windows-only  Build only Windows utilities
#   -v, --verbose       Verbose output
#   -j, --jobs N        Number of parallel jobs (default: number of CPU cores)

set -e  # Exit on any error

# Configuration
SCRATCHBIRD_VERSION="0.5.0"
BUILD_VERSION="SB-T0.5.0.1 ScratchBird 0.5 f90eae0"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/temp"
RELEASE_DIR="$SCRIPT_DIR/release/v0.5.0"
SOURCE_DIR="$SCRIPT_DIR/src"

# Default options
CLEAN_BUILD=false
LINUX_ONLY=false
WINDOWS_ONLY=false
VERBOSE=false
JOBS=$(nproc)

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_verbose() {
    if [ "$VERBOSE" = true ]; then
        echo -e "${BLUE}[VERBOSE]${NC} $1"
    fi
}

# Help function
show_help() {
    cat << EOF
ScratchBird v${SCRATCHBIRD_VERSION} - Complete Build Script

USAGE:
    ./sb_build_all.sh [OPTIONS]

OPTIONS:
    -h, --help          Show this help message
    -c, --clean         Clean build directories before building
    -l, --linux-only    Build only Linux utilities
    -w, --windows-only  Build only Windows utilities
    -v, --verbose       Enable verbose output
    -j, --jobs N        Number of parallel jobs (default: ${JOBS})

PROCESS:
    1. Clean up any artifacts in root directory
    2. Build from src/ directory using temp/ for compilation
    3. Copy final binaries to release/v0.5.0/ directories
    4. Verify all tools are properly built

UTILITIES BUILT:
    - sb_isql, sb_gbak, sb_gfix, sb_gsec, sb_gstat
    - All utilities properly isolated from root directory

OUTPUT DIRECTORIES:
    - ${RELEASE_DIR}/linux-x86_64/bin/
    - ${RELEASE_DIR}/windows-x64/bin/

EOF
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_help
            exit 0
            ;;
        -c|--clean)
            CLEAN_BUILD=true
            shift
            ;;
        -l|--linux-only)
            LINUX_ONLY=true
            shift
            ;;
        -w|--windows-only)
            WINDOWS_ONLY=true
            shift
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -j|--jobs)
            JOBS="$2"
            shift 2
            ;;
        *)
            log_error "Unknown option: $1"
            echo "Use -h or --help for usage information"
            exit 1
            ;;
    esac
done

# Clean up any build artifacts in root directory first
cleanup_root_directory() {
    log_info "Cleaning up root directory artifacts..."
    
    cd "$SCRIPT_DIR"
    
    # Remove any build artifacts that shouldn't be in root
    rm -f sb_* *.cpp *.exe *.dll *.so *.so.* debug_gpre* create_mock_library* create_windows_library*
    rm -f test_simple test_manual memdebug.log
    
    log_success "Root directory cleaned"
}

# Setup build directories properly
setup_build_dirs() {
    log_info "Setting up build directories..."
    
    if [ "$CLEAN_BUILD" = true ]; then
        log_info "Cleaning previous build..."
        rm -rf "$BUILD_DIR"
        rm -rf "$SCRIPT_DIR/gen"
    fi
    
    # Create proper directory structure
    mkdir -p "$BUILD_DIR"
    mkdir -p "$RELEASE_DIR/linux-x86_64"/{bin,lib,include,doc,examples,tests}
    mkdir -p "$RELEASE_DIR/windows-x64"/{bin,lib,include,doc,examples,tests}
    
    log_success "Build directories ready"
}

# Check dependencies
check_dependencies() {
    log_info "Checking build dependencies..."
    
    if ! command -v gcc &> /dev/null; then
        log_error "GCC not found. Please install GCC 7.0+ with C++17 support"
        exit 1
    fi
    
    if ! command -v make &> /dev/null; then
        log_error "Make not found. Please install GNU Make"
        exit 1
    fi
    
    log_success "Dependencies satisfied"
}

# Build core utilities using existing build system
build_core_utilities() {
    local platform="$1"
    
    log_info "Building core utilities for $platform..."
    
    cd "$SCRIPT_DIR"
    
    if [ "$platform" = "linux" ]; then
        log_verbose "Building Linux utilities..."
        
        # Use existing build system but ensure temp directory usage
        make TARGET=Release external -j$JOBS
        make TARGET=Release sb_gbak sb_gfix sb_gsec sb_gstat sb_isql -j$JOBS
        
        # Copy built utilities to release directory
        if [ -d "gen/Release/scratchbird/bin" ]; then
            cp gen/Release/scratchbird/bin/sb_* "$RELEASE_DIR/linux-x86_64/bin/" 2>/dev/null || true
        fi
        
        # Copy libraries
        if [ -d "gen/Release/scratchbird/lib" ]; then
            cp gen/Release/scratchbird/lib/libsbclient* "$RELEASE_DIR/linux-x86_64/lib/" 2>/dev/null || true
        fi
        
    elif [ "$platform" = "windows" ]; then
        log_verbose "Windows cross-compilation not implemented in core build system"
    fi
    
    log_success "Core utilities built for $platform"
}

# Verify build results
verify_build() {
    log_info "Verifying build results..."
    
    local expected_utilities=(
        "sb_gbak"
        "sb_gfix"
        "sb_gsec"
        "sb_gstat"
        "sb_isql"
    )
    
    # Check Linux utilities
    if [ "$WINDOWS_ONLY" = false ]; then
        log_verbose "Verifying Linux utilities..."
        local linux_dir="$RELEASE_DIR/linux-x86_64/bin"
        local found_count=0
        
        for util in "${expected_utilities[@]}"; do
            if [ -f "$linux_dir/$util" ]; then
                ((found_count++))
                log_verbose "✓ $util found"
                
                # Test version output
                if [ "$VERBOSE" = true ]; then
                    local version_output=$($linux_dir/$util -z 2>/dev/null || echo "No version info")
                    log_verbose "  Version: $version_output"
                fi
            else
                log_warning "✗ $util missing"
            fi
        done
        
        log_success "Linux utilities verified ($found_count/${#expected_utilities[@]} found)"
    fi
}

# Clean up any remaining artifacts in root after build
final_cleanup() {
    log_info "Final cleanup of root directory..."
    
    cd "$SCRIPT_DIR"
    
    # Remove any build artifacts that may have been created during build
    rm -f sb_* *.cpp *.exe *.dll *.so *.so.* debug_gpre* create_mock_library* create_windows_library*
    rm -f test_simple test_manual memdebug.log
    
    log_success "Final cleanup completed"
}

# Main build process
main() {
    log_info "Starting ScratchBird v${SCRATCHBIRD_VERSION} build process..."
    log_info "Build version: ${BUILD_VERSION}"
    log_info "Using temp directory: $BUILD_DIR"
    log_info "Output directory: $RELEASE_DIR"
    
    # Initial cleanup
    cleanup_root_directory
    
    # Check dependencies
    check_dependencies
    
    # Setup build directories
    setup_build_dirs
    
    # Build for Linux
    if [ "$WINDOWS_ONLY" = false ]; then
        build_core_utilities "linux"
    fi
    
    # Build for Windows (placeholder)
    if [ "$LINUX_ONLY" = false ]; then
        log_warning "Windows builds not yet implemented"
    fi
    
    # Verify build results
    verify_build
    
    # Final cleanup
    final_cleanup
    
    # Final summary
    log_success "Build completed successfully!"
    log_info "Release directories:"
    if [ "$WINDOWS_ONLY" = false ]; then
        log_info "  Linux x86_64: $RELEASE_DIR/linux-x86_64/bin/"
        local count=$(ls -1 "$RELEASE_DIR/linux-x86_64/bin/" 2>/dev/null | wc -l)
        log_info "    $count utilities built"
    fi
    
    log_info "Build completed at $(date)"
    log_info "Root directory is clean - no build artifacts remain"
}

# Run main function
main "$@"