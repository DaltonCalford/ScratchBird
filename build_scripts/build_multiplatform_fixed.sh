#!/bin/bash
# ScratchBird Multi-Platform Build Script
# Builds for linux64, linux32, windows64, windows32

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
echo "           ScratchBird Multi-Platform Build System"
echo "=================================================================="
echo "Building for: linux64, linux32, windows64, windows32"
echo "Build Log: $BUILD_LOG"
echo "=================================================================="

mkdir -p builds

check_tools() {
    log "Checking cross-compilation toolchains..."
    
    if ! command -v x86_64-w64-mingw32-gcc >/dev/null 2>&1; then
        error "MinGW-w64 toolchain not found. Install with:"
        error "sudo apt install mingw-w64"
        return 1
    fi
    
    if ! dpkg -l gcc-multilib >/dev/null 2>&1; then
        error "Multilib support not found. Install with:"
        error "sudo apt install gcc-multilib g++-multilib"
        return 1
    fi
    
    if ! command -v wine >/dev/null 2>&1; then
        warning "Wine not found. Windows executables cannot be tested."
        warning "Install with: sudo apt install wine"
    fi
    
    success "Cross-compilation toolchain check completed"
    return 0
}

build_platform() {
    local platform=$1
    local arch=$2
    
    log "Building ScratchBird for $platform-$arch..."
    
    case "$platform-$arch" in
        "linux-x64")
            export CC="gcc"
            export CXX="g++"
            cp gen/make.platform.linux gen/make.platform
            RELEASE_DIR="release/alpha0.6.0/linux-x64"
            ;;
        "linux-x86")
            export CC="gcc"
            export CXX="g++"
            sed 's/DAMD64/DI386/g; s/SIZEOF_LONG=8/SIZEOF_LONG=4/g' gen/make.platform.linux > gen/make.platform.linux32
            cp gen/make.platform.linux32 gen/make.platform
            echo "CFLAGS+=-m32" >> gen/make.platform
            echo "CXXFLAGS+=-m32" >> gen/make.platform
            echo "LDFLAGS+=-m32" >> gen/make.platform
            RELEASE_DIR="release/alpha0.6.0/linux-x86"
            ;;
        "windows-x64")
            export CC="x86_64-w64-mingw32-gcc"
            export CXX="x86_64-w64-mingw32-g++"
            cp gen/make.platform.windows gen/make.platform
            RELEASE_DIR="release/alpha0.6.0/windows-x64"
            ;;
        "windows-x86")
            export CC="i686-w64-mingw32-gcc"
            export CXX="i686-w64-mingw32-g++"
            sed 's/DWIN64/DWIN32/g; s/x86_64-w64-mingw32/i686-w64-mingw32/g' gen/make.platform.windows > gen/make.platform.windows32
            cp gen/make.platform.windows32 gen/make.platform
            RELEASE_DIR="release/alpha0.6.0/windows-x86"
            ;;
        *)
            error "Unknown platform-architecture combination: $platform-$arch"
            return 1
            ;;
    esac
    
    log "Cleaning previous build for $platform-$arch..."
    make TARGET=Release clean >/dev/null 2>&1 || warning "Clean had issues"
    
    log "Building external dependencies for $platform-$arch..."
    if ! make TARGET=Release external >>$BUILD_LOG 2>&1; then
        error "External dependencies build failed for $platform-$arch"
        return 1
    fi
    
    log "Building core utilities for $platform-$arch..."
    UTILITIES="sb_isql sb_gbak sb_gfix sb_gstat sb_gsec"
    
    BUILD_SUCCESS=true
    for utility in $UTILITIES; do
        log "Building $utility for $platform-$arch..."
        if make TARGET=Release -j$(nproc) $utility >>$BUILD_LOG 2>&1; then
            success "$utility built successfully for $platform-$arch"
        else
            warning "Failed to build $utility for $platform-$arch"
            BUILD_SUCCESS=false
        fi
    done
    
    if [ "$BUILD_SUCCESS" = false ]; then
        error "Some utilities failed to build for $platform-$arch"
        return 1
    fi
    
    log "Deploying $platform-$arch to $RELEASE_DIR..."
    BUILD_DIR="gen/Release/scratchbird"
    
    if [ -d "$BUILD_DIR" ]; then
        if [ -d "$BUILD_DIR/bin" ]; then
            cp "$BUILD_DIR/bin"/* "$RELEASE_DIR/bin/" 2>/dev/null || warning "Some binaries failed to copy for $platform-$arch"
        fi
        
        if [ -d "$BUILD_DIR/lib" ]; then
            cp "$BUILD_DIR/lib"/* "$RELEASE_DIR/lib/" 2>/dev/null || warning "Some libraries failed to copy for $platform-$arch"
        fi
        
        success "Deployment completed for $platform-$arch"
    else
        warning "Build directory not found for $platform-$arch"
        return 1
    fi
    
    return 0
}

test_executables() {
    local platform=$1
    local arch=$2
    
    log "Testing executables for $platform-$arch..."
    
    RELEASE_DIR="release/alpha0.6.0/$platform-$arch"
    
    case "$platform" in
        "linux")
            if [ -f "$RELEASE_DIR/bin/sb_gbak" ]; then
                if "$RELEASE_DIR/bin/sb_gbak" -z >/dev/null 2>&1; then
                    success "sb_gbak functional for $platform-$arch"
                else
                    warning "sb_gbak test failed for $platform-$arch"
                fi
            fi
            ;;
        "windows")
            if command -v wine >/dev/null 2>&1; then
                if [ -f "$RELEASE_DIR/bin/sb_gbak.exe" ]; then
                    if wine "$RELEASE_DIR/bin/sb_gbak.exe" -z >/dev/null 2>&1; then
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

main() {
    if ! check_tools; then
        error "Cross-compilation toolchain check failed"
        error "Please install required packages and re-run"
        exit 1
    fi
    
    BUILD_RESULTS=""
    
    # Build Linux x64
    log "Starting build for linux-x64..."
    if build_platform "linux" "x64"; then
        success "Build completed successfully for linux-x64"
        BUILD_RESULTS="$BUILD_RESULTS✅ linux-x64: SUCCESS\n"
        test_executables "linux" "x64"
    else
        error "Build failed for linux-x64"
        BUILD_RESULTS="$BUILD_RESULTS❌ linux-x64: FAILED\n"
    fi
    echo ""
    
    # Build Linux x86
    log "Starting build for linux-x86..."
    if build_platform "linux" "x86"; then
        success "Build completed successfully for linux-x86"
        BUILD_RESULTS="$BUILD_RESULTS✅ linux-x86: SUCCESS\n"
        test_executables "linux" "x86"
    else
        error "Build failed for linux-x86"
        BUILD_RESULTS="$BUILD_RESULTS❌ linux-x86: FAILED\n"
    fi
    echo ""
    
    # Build Windows x64
    log "Starting build for windows-x64..."
    if build_platform "windows" "x64"; then
        success "Build completed successfully for windows-x64"
        BUILD_RESULTS="$BUILD_RESULTS✅ windows-x64: SUCCESS\n"
        test_executables "windows" "x64"
    else
        error "Build failed for windows-x64"
        BUILD_RESULTS="$BUILD_RESULTS❌ windows-x64: FAILED\n"
    fi
    echo ""
    
    # Build Windows x86
    log "Starting build for windows-x86..."
    if build_platform "windows" "x86"; then
        success "Build completed successfully for windows-x86"
        BUILD_RESULTS="$BUILD_RESULTS✅ windows-x86: SUCCESS\n"
        test_executables "windows" "x86"
    else
        error "Build failed for windows-x86"
        BUILD_RESULTS="$BUILD_RESULTS❌ windows-x86: FAILED\n"
    fi
    echo ""
    
    echo "=================================================================="
    echo "                    MULTI-PLATFORM BUILD SUMMARY"
    echo "=================================================================="
    echo -e "$BUILD_RESULTS"
    echo ""
    echo "📝 Build log: $BUILD_LOG"
    echo "📁 Release location: release/alpha0.6.0/"
    echo "=================================================================="
}

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