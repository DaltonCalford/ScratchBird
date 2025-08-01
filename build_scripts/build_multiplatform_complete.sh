#!/bin/bash
# ScratchBird Multi-Platform Build Script v2.0
# Builds for linux64, linux32, windows64, windows32
# Now includes comprehensive directory structure creation

set -e

PROJECT_ROOT="/home/dcalford/Documents/claude/GitHubRepo/ScratchBird"
cd "$PROJECT_ROOT"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log() {
    echo -e "${BLUE}[$(date '+%Y-%m-%d %H:%M:%S')]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1" >&2
}

success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

BUILD_TIMESTAMP=$(date '+%Y%m%d_%H%M%S')
BUILD_LOG="builds/multiplatform_build_${BUILD_TIMESTAMP}.log"

echo "=================================================================="
echo "           ScratchBird Multi-Platform Build System v2.0"
echo "=================================================================="
echo "Building for: linux64, linux32, windows64, windows32"
echo "Build Log: $BUILD_LOG"
echo "=================================================================="

mkdir -p builds

# Comprehensive directory creation function
create_build_directories() {
    local target=$1
    local platform=$2
    
    log "Creating build directories for $target ($platform)..."
    
    # Create main directories
    mkdir -p "temp/$target"
    mkdir -p "gen/$target/scratchbird/bin"
    mkdir -p "gen/$target/scratchbird/lib"
    mkdir -p "gen/$target/scratchbird/etc"
    mkdir -p "gen/$target/scratchbird/include"
    mkdir -p "gen/$target/scratchbird/plugins"
    
    # Create all source-based directories in temp build area
    while IFS= read -r -d '' dir; do
        # Convert src/ path to temp/TARGET/ path
        target_dir="${dir/src\//temp/$target/}"
        mkdir -p "$target_dir"
    done < <(find src -type d -print0)
    
    # Create additional build-specific directories that may be needed
    mkdir -p "temp/$target/plugins/crypt/arc4"
    mkdir -p "temp/$target/plugins/crypt/chacha"
    mkdir -p "temp/$target/common/config"
    mkdir -p "temp/$target/common/sha2"
    mkdir -p "temp/$target/yvalve/config/os/posix"
    mkdir -p "temp/$target/remote/client"
    mkdir -p "temp/$target/auth/SecureRemotePassword/client"
    mkdir -p "temp/$target/auth/SecurityDatabase"
    
    # Create gen directories that might be needed during build
    mkdir -p "gen/$target/cloop"
    mkdir -p "gen/$target/include"
    
    success "Created build directories for $target"
}

check_tools() {
    log "Checking cross-compilation toolchains..."
    
    if ! command -v x86_64-w64-mingw32-gcc >/dev/null 2>&1; then
        error "MinGW-w64 toolchain not found. Install with:"
        error "sudo apt install mingw-w64"
        return 1
    fi
    
    if ! command -v i686-w64-mingw32-gcc >/dev/null 2>&1; then
        error "MinGW-w64 32-bit toolchain not found. Install with:"
        error "sudo apt install mingw-w64"
        return 1
    fi
    
    if ! dpkg --print-architecture | grep -q x86_64; then
        error "This script requires a 64-bit Linux system for cross-compilation"
        return 1
    fi
    
    # Check for multilib support for 32-bit Linux builds
    if ! dpkg --print-foreign-architectures | grep -q i386; then
        warning "32-bit architecture support not enabled. Run:"
        warning "sudo dpkg --add-architecture i386"
        warning "sudo apt update"
        warning "sudo apt install gcc-multilib g++-multilib"
    fi
    
    success "All required toolchains found"
}

setup_platform_config() {
    local platform=$1
    local target=$2
    
    log "Setting up platform configuration for $platform ($target)..."
    
    case $platform in
        "linux-x64")
            export CC="gcc"
            export CXX="g++"
            export AR="ar"
            export TARGET_DIR="$target"
            export CFLAGS="-DLINUX -DAMD64 -DSIZEOF_LONG=8 -DFB_ALIGNMENT=8 -DFB_DOUBLE_ALIGN=8 -fPIC"
            export CXXFLAGS="-DLINUX -DAMD64 -DSIZEOF_LONG=8 -DFB_ALIGNMENT=8 -DFB_DOUBLE_ALIGN=8 -fPIC"
            ;;
        "linux-x86")
            export CC="gcc -m32"
            export CXX="g++ -m32"
            export AR="ar"
            export TARGET_DIR="$target"
            export CFLAGS="-DLINUX -Di386 -DSIZEOF_LONG=4 -DFB_ALIGNMENT=4 -DFB_DOUBLE_ALIGN=4 -fPIC"
            export CXXFLAGS="-DLINUX -Di386 -DSIZEOF_LONG=4 -DFB_ALIGNMENT=4 -DFB_DOUBLE_ALIGN=4 -fPIC"
            ;;
        "windows-x64")
            export CC="x86_64-w64-mingw32-gcc"
            export CXX="x86_64-w64-mingw32-g++"
            export AR="x86_64-w64-mingw32-ar"
            export TARGET_DIR="$target"
            export CFLAGS="-DWIN32 -DAMD64 -DSIZEOF_LONG=4 -DFB_ALIGNMENT=8 -DFB_DOUBLE_ALIGN=8"
            export CXXFLAGS="-DWIN32 -DAMD64 -DSIZEOF_LONG=4 -DFB_ALIGNMENT=8 -DFB_DOUBLE_ALIGN=8"
            ;;
        "windows-x86")
            export CC="i686-w64-mingw32-gcc"
            export CXX="i686-w64-mingw32-g++"
            export AR="i686-w64-mingw32-ar"
            export TARGET_DIR="$target"
            export CFLAGS="-DWIN32 -Di386 -DSIZEOF_LONG=4 -DFB_ALIGNMENT=4 -DFB_DOUBLE_ALIGN=4"
            export CXXFLAGS="-DWIN32 -Di386 -DSIZEOF_LONG=4 -DFB_ALIGNMENT=4 -DFB_DOUBLE_ALIGN=4"
            ;;
        *)
            error "Unknown platform: $platform"
            return 1
            ;;
    esac
    
    success "Platform configuration set for $platform"
}

build_platform() {
    local platform=$1
    local target="Release"
    local full_target="${target}-${platform}"
    
    log "Building ScratchBird for $platform..."
    
    # Create all necessary directories first
    create_build_directories "$full_target" "$platform"
    
    # Setup platform-specific configuration
    setup_platform_config "$platform" "$full_target"
    
    # Copy correct platform configuration
    case $platform in
        "linux-x64"|"linux-x86")
            cp "gen/make.platform.linux" "gen/make.platform" 2>/dev/null || true
            ;;
        "windows-x64"|"windows-x86")
            if [ -f "gen/make.platform.windows" ]; then
                cp "gen/make.platform.windows" "gen/make.platform"
            else
                warning "Windows platform configuration not found, using Linux config as base"
                cp "gen/make.platform.linux" "gen/make.platform"
            fi
            ;;
    esac
    
    # Build external dependencies first
    log "Building external dependencies for $platform..."
    if ! make TARGET="$full_target" external 2>&1; then
        error "External dependencies build failed for $platform"
        return 1
    fi
    
    # Build main ScratchBird target
    log "Building main ScratchBird target for $platform..."
    if ! make TARGET="$full_target" scratchbird 2>&1; then
        error "ScratchBird build failed for $platform"
        return 1
    fi
    
    # Verify build results
    verify_build "$platform" "$full_target"
    
    success "Successfully built ScratchBird for $platform"
}

verify_build() {
    local platform=$1
    local target=$2
    
    log "Verifying build results for $platform..."
    
    local bin_dir="gen/$target/scratchbird/bin"
    local lib_dir="gen/$target/scratchbird/lib"
    
    if [ ! -d "$bin_dir" ]; then
        error "Build verification failed: $bin_dir not found"
        return 1
    fi
    
    # Count built executables and libraries
    local exe_count=$(find "$bin_dir" -type f -executable | wc -l)
    local lib_count=$(find "$lib_dir" -name "*.so*" -o -name "*.dll*" -o -name "*.a" 2>/dev/null | wc -l)
    
    log "Build verification for $platform:"
    log "  - Executables: $exe_count"
    log "  - Libraries: $lib_count"
    
    if [ "$exe_count" -eq 0 ]; then
        warning "No executables found in $bin_dir"
    fi
    
    if [ "$lib_count" -eq 0 ]; then
        warning "No libraries found in $lib_dir"
    fi
    
    # List built files for debugging
    log "Built files in $bin_dir:"
    ls -la "$bin_dir" || true
    
    log "Built files in $lib_dir:"
    ls -la "$lib_dir" || true
}

test_executables() {
    local platform=$1
    local target=$2
    
    log "Testing executables for $platform..."
    
    local bin_dir="gen/$target/scratchbird/bin"
    
    case $platform in
        "linux-x64"|"linux-x86")
            # Test Linux executables directly
            for exe in "$bin_dir"/*; do
                if [ -x "$exe" ] && [ -f "$exe" ]; then
                    log "Testing $(basename "$exe")..."
                    if "$exe" -? >/dev/null 2>&1 || "$exe" --help >/dev/null 2>&1 || "$exe" -z >/dev/null 2>&1; then
                        success "$(basename "$exe") runs successfully"
                    else
                        warning "$(basename "$exe") may have issues"
                    fi
                fi
            done
            ;;
        "windows-x64"|"windows-x86")
            # Test Windows executables with Wine if available
            if command -v wine >/dev/null 2>&1; then
                log "Testing Windows executables with Wine..."
                for exe in "$bin_dir"/*.exe; do
                    if [ -f "$exe" ]; then
                        log "Testing $(basename "$exe")..."
                        if wine "$exe" /? >/dev/null 2>&1 || wine "$exe" --help >/dev/null 2>&1; then
                            success "$(basename "$exe") runs successfully under Wine"
                        else
                            warning "$(basename "$exe") may have issues under Wine"
                        fi
                    fi
                done
            else
                warning "Wine not available, skipping Windows executable testing"
            fi
            ;;
    esac
}

create_release_package() {
    local platform=$1
    local target=$2
    
    log "Creating release package for $platform..."
    
    local package_dir="builds/scratchbird-${platform}-${BUILD_TIMESTAMP}"
    mkdir -p "$package_dir"
    
    # Copy built files
    cp -r "gen/$target/scratchbird/"* "$package_dir/" 2>/dev/null || true
    
    # Create platform-specific README
    cat > "$package_dir/README.txt" << EOF
ScratchBird v0.5 - $platform Build
Built: $(date)
Build ID: $BUILD_TIMESTAMP

This package contains the ScratchBird database system built for $platform.

Installation:
1. Extract this package to your desired location
2. Add the bin/ directory to your PATH
3. Set SCRATCHBIRD environment variable to point to this directory

For more information, visit: https://scratchbird.org
EOF
    
    # Create archive
    tar -czf "$package_dir.tar.gz" -C "builds" "$(basename "$package_dir")"
    
    success "Release package created: $package_dir.tar.gz"
}

# Main build sequence
main() {
    log "Starting ScratchBird multi-platform build..."
    
    # Check prerequisites
    check_tools
    
    # Define platforms to build
    platforms=("linux-x64" "linux-x86" "windows-x64" "windows-x86")
    
    # Build each platform
    for platform in "${platforms[@]}"; do
        log "======== Building $platform ========"
        
        if build_platform "$platform"; then
            test_executables "$platform" "Release-$platform"
            create_release_package "$platform" "Release-$platform"
            success "✅ $platform build completed successfully"
        else
            error "❌ $platform build failed"
            # Continue with other platforms
        fi
        
        echo ""
    done
    
    # Summary
    echo "=================================================================="
    echo "                    Build Summary"
    echo "=================================================================="
    
    for platform in "${platforms[@]}"; do
        if [ -f "builds/scratchbird-${platform}-${BUILD_TIMESTAMP}.tar.gz" ]; then
            success "✅ $platform: SUCCESS"
        else
            error "❌ $platform: FAILED"
        fi
    done
    
    echo "=================================================================="
    log "Multi-platform build completed. Check builds/ directory for packages."
}

# Execute main function, logging everything
main 2>&1 | tee "$BUILD_LOG"