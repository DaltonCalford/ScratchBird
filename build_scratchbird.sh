#!/bin/bash

# ScratchBird Cross-Platform Build Script
# Supports Linux native and Windows cross-compilation
# Version: 0.5

set -e

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration - Make paths relative to script location
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"

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

# Load build configuration if available
if [ -f "$PROJECT_ROOT/build_config.env" ]; then
    source "$PROJECT_ROOT/build_config.env"
    print_status "Loaded build configuration"
else
    print_warning "No build configuration found. Run ./setup_dev_environment.sh first"
    # Default fallback configuration
    BUILD_TYPE="Release"
    PARALLEL_JOBS=$(nproc)
    LINUX_TARGET="linux-x64"
    WINDOWS_TARGET="windows-x64"
    MINGW_PREFIX="x86_64-w64-mingw32"
    MINGW_CC="${MINGW_PREFIX}-gcc"
    MINGW_CXX="${MINGW_PREFIX}-g++"
    MINGW_AR="${MINGW_PREFIX}-ar"
    MINGW_RANLIB="${MINGW_PREFIX}-ranlib"
    MINGW_STRIP="${MINGW_PREFIX}-strip"
fi

# Function to clean build artifacts
clean_build() {
    print_status "Cleaning build artifacts..."
    cd "$PROJECT_ROOT"
    
    # Clean make artifacts
    if [ -d "temp" ]; then
        rm -rf temp/*
    fi
    
    if [ -d "gen" ]; then
        cd gen
        make clean 2>/dev/null || true
        cd ..
    fi
    
    # Clean cmake artifacts
    if [ -d "build" ]; then
        rm -rf build/*
    fi
    
    print_success "Build artifacts cleaned"
}

# Function to configure Linux build
configure_linux() {
    print_status "Configuring ScratchBird for Linux ($LINUX_TARGET)..."
    
    cd "$PROJECT_ROOT"
    
    # Set environment variables for Linux build
    export CC=gcc
    export CXX=g++
    export AR=ar
    export RANLIB=ranlib
    export STRIP=strip
    
    # Configure with autotools
    if [ -f "configure" ]; then
        print_status "Running configure for Linux..."
        ./configure \
            --enable-binreloc \
            --with-system-re2 \
            --with-system-editline \
            --enable-shared \
            --enable-static \
            --prefix=/usr/local/scratchbird \
            --with-fbbin=/usr/local/scratchbird/bin \
            --with-fbsbin=/usr/local/scratchbird/bin \
            --with-fblib=/usr/local/scratchbird/lib \
            --with-fbinclude=/usr/local/scratchbird/include \
            --with-fbdoc=/usr/local/scratchbird/doc \
            --with-fbudf=/usr/local/scratchbird/UDF \
            --with-fbsample=/usr/local/scratchbird/examples \
            --with-fbsample-db=/usr/local/scratchbird/examples/empbuild \
            --with-fbhelp=/usr/local/scratchbird/help \
            --with-fbintl=/usr/local/scratchbird/intl \
            --with-fbmisc=/usr/local/scratchbird/misc \
            --with-fbsecure-db=/usr/local/scratchbird \
            --with-fbmsg=/usr/local/scratchbird \
            --with-fblog=/usr/local/scratchbird \
            --with-fbglock=/usr/local/scratchbird \
            --with-fbplugins=/usr/local/scratchbird/plugins
    else
        print_warning "configure script not found, using direct make approach"
    fi
    
    print_success "Linux configuration completed"
}

# Function to configure Windows cross-compilation
configure_windows() {
    print_status "Configuring ScratchBird for Windows cross-compilation ($WINDOWS_TARGET)..."
    
    cd "$PROJECT_ROOT"
    
    # Set environment variables for Windows cross-compilation
    export CC="$MINGW_CC"
    export CXX="$MINGW_CXX"
    export AR="$MINGW_AR"
    export RANLIB="$MINGW_RANLIB"
    export STRIP="$MINGW_STRIP"
    
    # Configure with cross-compilation settings
    if [ -f "configure" ]; then
        print_status "Running configure for Windows cross-compilation..."
        ./configure \
            --host=x86_64-w64-mingw32 \
            --build=x86_64-linux-gnu \
            --enable-binreloc \
            --disable-shared \
            --enable-static \
            --prefix=/usr/local/scratchbird-windows \
            --with-fbbin=/usr/local/scratchbird-windows/bin \
            --with-fbsbin=/usr/local/scratchbird-windows/bin \
            --with-fblib=/usr/local/scratchbird-windows/lib \
            --with-fbinclude=/usr/local/scratchbird-windows/include \
            --with-fbdoc=/usr/local/scratchbird-windows/doc \
            --with-fbudf=/usr/local/scratchbird-windows/UDF \
            --with-fbsample=/usr/local/scratchbird-windows/examples \
            --with-fbsample-db=/usr/local/scratchbird-windows/examples/empbuild \
            --with-fbhelp=/usr/local/scratchbird-windows/help \
            --with-fbintl=/usr/local/scratchbird-windows/intl \
            --with-fbmisc=/usr/local/scratchbird-windows/misc \
            --with-fbsecure-db=/usr/local/scratchbird-windows \
            --with-fbmsg=/usr/local/scratchbird-windows \
            --with-fblog=/usr/local/scratchbird-windows \
            --with-fbglock=/usr/local/scratchbird-windows \
            --with-fbplugins=/usr/local/scratchbird-windows/plugins
    else
        print_warning "configure script not found, using direct make approach"
    fi
    
    print_success "Windows cross-compilation configuration completed"
}

# Function to build for Linux
build_linux() {
    print_status "Building ScratchBird for Linux..."
    
    cd "$PROJECT_ROOT"
    
    # Set Linux environment
    export CC=gcc
    export CXX=g++
    export AR=ar
    export RANLIB=ranlib
    
    # Build using make
    print_status "Starting Linux build with $PARALLEL_JOBS parallel jobs..."
    make -j"$PARALLEL_JOBS" TARGET="$BUILD_TYPE" 2>&1 | tee build_linux.log
    
    if [ $? -eq 0 ]; then
        print_success "Linux build completed successfully"
    else
        print_error "Linux build failed - check build_linux.log for details"
        return 1
    fi
}

# Function to build for Windows
build_windows() {
    print_status "Building ScratchBird for Windows using MinGW..."
    
    cd "$PROJECT_ROOT"
    
    # Set Windows cross-compilation environment
    export CC="$MINGW_CC"
    export CXX="$MINGW_CXX"
    export AR="$MINGW_AR"
    export RANLIB="$MINGW_RANLIB"
    
    # Build using make with cross-compilation
    print_status "Starting Windows cross-compilation with $PARALLEL_JOBS parallel jobs..."
    make -j"$PARALLEL_JOBS" TARGET="$BUILD_TYPE" PLATFORM=WIN32 2>&1 | tee build_windows.log
    
    if [ $? -eq 0 ]; then
        print_success "Windows build completed successfully"
    else
        print_error "Windows build failed - check build_windows.log for details"
        return 1
    fi
}

# Function to create deployment packages
create_packages() {
    print_status "Creating deployment packages..."
    
    cd "$PROJECT_ROOT"
    
    # Create Linux package
    if [ -d "gen/Release/firebird" ]; then
        print_status "Creating Linux package..."
        mkdir -p packages/linux-x64
        cp -r gen/Release/firebird/* packages/linux-x64/
        
        # Create tarball
        cd packages
        tar -czf "scratchbird-v0.5-linux-x64.tar.gz" linux-x64/
        cd ..
        
        print_success "Linux package created: packages/scratchbird-v0.5-linux-x64.tar.gz"
    fi
    
    # Create Windows package
    if [ -d "gen/Release/firebird" ]; then
        print_status "Creating Windows package..."
        mkdir -p packages/windows-x64
        cp -r gen/Release/firebird/* packages/windows-x64/
        
        # Strip Windows binaries
        find packages/windows-x64 -name "*.exe" -exec "$MINGW_STRIP" {} \;
        find packages/windows-x64 -name "*.dll" -exec "$MINGW_STRIP" {} \;
        
        # Create zip archive
        cd packages
        zip -r "scratchbird-v0.5-windows-x64.zip" windows-x64/
        cd ..
        
        print_success "Windows package created: packages/scratchbird-v0.5-windows-x64.zip"
    fi
}

# Function to show tool information
show_tools() {
    print_status "ScratchBird v0.5 - Renamed Tools:"
    echo "  sb_isql    - Interactive SQL shell (replaces isql)"
    echo "  sb_gbak    - Backup/restore utility (replaces gbak)"
    echo "  sb_gfix    - Database repair utility (replaces gfix)"
    echo "  sb_gsec    - Security manager (replaces gsec)"
    echo "  sb_gstat   - Database statistics (replaces gstat)"
    echo "  sb_nbackup - Physical backup utility (replaces nbackup)"
    echo "  sb_fbsvcmgr - Service manager (replaces fbsvcmgr)"
    echo "  sb_fbtracemgr - Trace manager (replaces fbtracemgr)"
    echo ""
    print_status "SQL Dialect 4 Features:"
    echo "  - FROM-less SELECT statements: SELECT GEN_UUID();"
    echo "  - Multi-row INSERT VALUES: INSERT INTO t VALUES (1,2),(3,4);"
    echo "  - Comprehensive SYNONYM support with schema awareness"
    echo "  - Hierarchical schemas (8 levels deep): schema1.schema2.table"
    echo "  - Database links with schema resolution modes"
}

# Main script logic
main() {
    print_status "ScratchBird v0.5 Cross-Platform Build System"
    print_status "============================================="
    
    case "$1" in
        "clean")
            clean_build
            ;;
        "configure-linux")
            configure_linux
            ;;
        "configure-windows")
            configure_windows
            ;;
        "build-linux")
            configure_linux
            build_linux
            ;;
        "build-windows")
            configure_windows
            build_windows
            ;;
        "build-all")
            clean_build
            print_status "Building for all platforms..."
            
            # Build Linux
            configure_linux
            build_linux
            
            # Build Windows
            configure_windows
            build_windows
            
            create_packages
            ;;
        "package")
            create_packages
            ;;
        "tools")
            show_tools
            ;;
        *)
            echo "Usage: $0 {clean|configure-linux|configure-windows|build-linux|build-windows|build-all|package|tools}"
            echo ""
            echo "Commands:"
            echo "  clean             - Clean all build artifacts"
            echo "  configure-linux   - Configure for Linux native build"
            echo "  configure-windows - Configure for Windows cross-compilation"
            echo "  build-linux       - Build ScratchBird for Linux"
            echo "  build-windows     - Build ScratchBird for Windows"
            echo "  build-all         - Build for both Linux and Windows"
            echo "  package           - Create deployment packages"
            echo "  tools             - Show renamed tools and features"
            echo ""
            show_tools
            exit 1
            ;;
    esac
}

# Run main function with all arguments
main "$@"