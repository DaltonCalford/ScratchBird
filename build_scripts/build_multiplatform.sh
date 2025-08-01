#!/bin/bash
# ScratchBird Multi-Platform Build Script
# Builds for linux64, linux32, windows64, windows32

set -e

PROJECT_ROOT="/home/dcalford/Documents/claude/GitHubRepo/ScratchBird"
cd "$PROJECT_ROOT"

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Logging function
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
echo "           ScratchBird Multi-Platform Build System"
echo "=================================================================="
echo "Building for: linux64, linux32, windows64, windows32"
echo "Build Log: $BUILD_LOG"
echo "=================================================================="

# Create build log directory
mkdir -p builds

# Function to log both to console and file
log_to_file() {
    echo "$@" | tee -a "$BUILD_LOG"
}

# Redirect all output to both console and log file
{

# Check for required cross-compilation tools
check_tools() {
    log "Checking cross-compilation toolchains..."
    
    # Check for MinGW-w64 (Windows cross-compilation)
    if ! command -v x86_64-w64-mingw32-gcc &> /dev/null; then
        error "MinGW-w64 toolchain not found. Install with:"
        error "sudo apt install mingw-w64"
        return 1
    fi
    
    # Check for multilib (32-bit compilation)
    if ! dpkg -l gcc-multilib &> /dev/null; then
        error "Multilib support not found. Install with:"
        error "sudo apt install gcc-multilib g++-multilib"
        return 1
    fi
    
    # Check for Wine (Windows testing)
    if ! command -v wine &> /dev/null; then
        warning "Wine not found. Windows executables cannot be tested."
        warning "Install with: sudo apt install wine"
    fi
    
    success "Cross-compilation toolchain check completed"
    return 0
}

# Build for specific platform
build_platform() {
    local platform=$1
    local arch=$2
    local compiler_prefix=$3
    
    log "Building ScratchBird for $platform-$arch..."
    
    # Set platform-specific variables
    case "$platform-$arch" in
        "linux-x64")
            export CC="gcc"
            export CXX="g++"
            export CFLAGS="-DLINUX -DAMD64 -DSIZEOF_LONG=8"
            export CXXFLAGS="-DLINUX -DAMD64 -DSIZEOF_LONG=8"
            RELEASE_DIR="release/alpha0.6.0/linux-x64"
            ;;
        "linux-x86")
            export CC="gcc"
            export CXX="g++"
            export CFLAGS="-m32 -DLINUX -DI386 -DSIZEOF_LONG=4"
            export CXXFLAGS="-m32 -DLINUX -DI386 -DSIZEOF_LONG=4"
            export LDFLAGS="-m32"
            RELEASE_DIR="release/alpha0.6.0/linux-x86"
            ;;
        "windows-x64")
            export CC="x86_64-w64-mingw32-gcc"
            export CXX="x86_64-w64-mingw32-g++"
            export CFLAGS="-DWIN32 -DWIN64 -DSIZEOF_LONG=4"
            export CXXFLAGS="-DWIN32 -DWIN64 -DSIZEOF_LONG=4"
            RELEASE_DIR="release/alpha0.6.0/windows-x64"
            ;;
        "windows-x86")
            export CC="i686-w64-mingw32-gcc"
            export CXX="i686-w64-mingw32-g++"
            export CFLAGS="-DWIN32 -DI386 -DSIZEOF_LONG=4"
            export CXXFLAGS="-DWIN32 -DI386 -DSIZEOF_LONG=4"
            RELEASE_DIR="release/alpha0.6.0/windows-x86"
            ;;
        *)
            error "Unknown platform-architecture combination: $platform-$arch"
            return 1
            ;;
    esac
    
    # Clean previous build
    log "Cleaning previous build for $platform-$arch..."
    make TARGET=Release clean || warning "Clean had issues"
    
    # Copy platform-specific configuration
    case "$platform" in
        "linux")
            cp gen/make.platform.linux gen/make.platform
            ;;
        "windows")
            cp gen/make.platform.windows gen/make.platform
            ;;
    esac
    
    # Build external dependencies
    log "Building external dependencies for $platform-$arch..."
    if ! make TARGET=Release external; then
        error "External dependencies build failed for $platform-$arch"
        return 1
    fi
    
    # Build core utilities
    log "Building core utilities for $platform-$arch..."
    UTILITIES=(
        "sb_isql"
        "sb_gbak" 
        "sb_gfix"
        "sb_gstat"
        "sb_gsec"
    )
    
    for utility in "${UTILITIES[@]}"; do
        log "Building $utility for $platform-$arch..."
        if ! make TARGET=Release -j$(nproc) $utility; then
            warning "Failed to build $utility for $platform-$arch"
            continue
        fi
        success "$utility built successfully for $platform-$arch"
    done
    
    # Deploy to release directory
    log "Deploying $platform-$arch to $RELEASE_DIR..."
    BUILD_DIR="gen/Release/scratchbird"
    
    if [ -d "$BUILD_DIR" ]; then
        # Deploy binaries
        if [ -d "$BUILD_DIR/bin" ]; then
            cp -v "$BUILD_DIR/bin"/* "$RELEASE_DIR/bin/" 2>/dev/null || warning "Some binaries failed to copy for $platform-$arch"
        fi
        
        # Deploy libraries
        if [ -d "$BUILD_DIR/lib" ]; then
            cp -v "$BUILD_DIR/lib"/* "$RELEASE_DIR/lib/" 2>/dev/null || warning "Some libraries failed to copy for $platform-$arch"
        fi
        
        success "Deployment completed for $platform-$arch"
    else
        warning "Build directory not found for $platform-$arch"
        return 1
    fi
    
    return 0
}

# Test built executables
test_executables() {
    local platform=$1
    local arch=$2
    
    log "Testing executables for $platform-$arch..."
    
    RELEASE_DIR="release/alpha0.6.0/$platform-$arch"
    
    case "$platform" in
        "linux")
            # Test Linux executables directly
            if [ -f "$RELEASE_DIR/bin/sb_gbak" ]; then
                if "$RELEASE_DIR/bin/sb_gbak" -z &>/dev/null; then
                    success "sb_gbak functional for $platform-$arch"
                else
                    warning "sb_gbak test failed for $platform-$arch"
                fi
            fi
            ;;
        "windows")
            # Test Windows executables with Wine (if available)
            if command -v wine &> /dev/null; then
                if [ -f "$RELEASE_DIR/bin/sb_gbak.exe" ]; then
                    if wine "$RELEASE_DIR/bin/sb_gbak.exe" -z &>/dev/null; then
                        success "sb_gbak.exe functional for $platform-$arch (via Wine)"
                    else
                        warning "sb_gbak.exe test failed for $platform-$arch"
                    fi
                fi
            else
                warning "Cannot test Windows executables - Wine not available"
            fi
            ;;
    esac
}

# Main build process
main() {
    # Check tools first
    if ! check_tools; then
        error "Cross-compilation toolchain check failed"
        error "Please install required packages and re-run"
        exit 1
    fi
    
    # Platforms to build
    PLATFORMS=(
        "linux x64"
        "linux x86" 
        "windows x64"
        "windows x86"
    )
    
    BUILD_RESULTS=()
    
    for platform_arch in "${PLATFORMS[@]}"; do
        platform=$(echo $platform_arch | cut -d' ' -f1)
        arch=$(echo $platform_arch | cut -d' ' -f2)
        
        log "Starting build for $platform-$arch..."
        
        if build_platform "$platform" "$arch"; then
            success "Build completed successfully for $platform-$arch"
            BUILD_RESULTS+=("✅ $platform-$arch: SUCCESS")
            
            # Test the built executables
            test_executables "$platform" "$arch"
        else
            error "Build failed for $platform-$arch"
            BUILD_RESULTS+=("❌ $platform-$arch: FAILED")
        fi
        
        echo ""
    done
    
    # Final summary
    echo "=================================================================="
    echo "                    MULTI-PLATFORM BUILD SUMMARY"
    echo "=================================================================="
    
    for result in "${BUILD_RESULTS[@]}"; do
        echo "$result"
    done
    
    echo ""
    echo "📝 Build log: $BUILD_LOG"
    echo "📁 Release location: release/alpha0.6.0/"
    echo "=================================================================="
}

# Handle command line arguments
case "${1:-}" in
    "--help"|"-h")
        echo "ScratchBird Multi-Platform Build Script"
        echo "Usage: $0 [options]"
        echo ""
        echo "Options:"
        echo "  --help, -h     Show this help message"
        echo "  --check-tools  Only check cross-compilation tools"
        echo "  --linux-only   Build only Linux platforms"
        echo "  --windows-only Build only Windows platforms"
        echo ""
        echo "Builds for: linux64, linux32, windows64, windows32"
        exit 0
        ;;
    "--check-tools")
        check_tools
        exit $?
        ;;
    "--linux-only")
        log "Building Linux platforms only..."
        build_platform "linux" "x64"
        build_platform "linux" "x86"
        exit 0
        ;;
    "--windows-only")
        log "Building Windows platforms only..."
        build_platform "windows" "x64"
        build_platform "windows" "x86"
        exit 0
        ;;
    "")
        main
        ;;
    *)
        error "Unknown option: $1"
        echo "Use --help for usage information"
        exit 1
        ;;
esac

} 2>&1 | tee -a "$BUILD_LOG"