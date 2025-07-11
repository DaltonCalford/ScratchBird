#!/bin/bash

# ScratchBird Platform Configuration Selector
# Automatically configures build system for target platform

set -e

# Make paths relative to script location
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"
TARGET_PLATFORM="$1"

# Function to print colored output
print_info() {
    echo -e "\033[0;34m[INFO]\033[0m $1"
}

print_success() {
    echo -e "\033[0;32m[SUCCESS]\033[0m $1"
}

print_error() {
    echo -e "\033[0;31m[ERROR]\033[0m $1"
}

# Function to configure for Linux
configure_linux() {
    print_info "Configuring ScratchBird for Linux native build..."
    
    cd "$PROJECT_ROOT"
    
    # Update make.defaults to include Linux platform configuration
    if [ -f "gen/make.defaults" ]; then
        # Add Linux platform include to make.defaults
        if ! grep -q "include make.platform.linux" gen/make.defaults; then
            echo "" >> gen/make.defaults
            echo "# Linux platform configuration" >> gen/make.defaults
            echo "include make.platform.linux" >> gen/make.defaults
        fi
    fi
    
    # Set platform environment
    export SCRATCHBIRD_PLATFORM=linux
    export SCRATCHBIRD_ARCH=x64
    export SCRATCHBIRD_CROSS_COMPILE=no
    
    # Update preprocessor definitions in common headers
    update_platform_defines "linux"
    
    print_success "Linux configuration completed"
}

# Function to configure for Windows
configure_windows() {
    print_info "Configuring ScratchBird for Windows cross-compilation..."
    
    cd "$PROJECT_ROOT"
    
    # Check if MinGW is available
    if ! which x86_64-w64-mingw32-g++ >/dev/null 2>&1; then
        print_error "MinGW cross-compiler not found. Please install mingw-w64"
        print_info "Run: sudo apt-get install mingw-w64"
        exit 1
    fi
    
    # Update make.defaults to include Windows platform configuration
    if [ -f "gen/make.defaults" ]; then
        # Add Windows platform include to make.defaults
        if ! grep -q "include make.platform.windows" gen/make.defaults; then
            echo "" >> gen/make.defaults
            echo "# Windows platform configuration" >> gen/make.defaults
            echo "include make.platform.windows" >> gen/make.defaults
        fi
    fi
    
    # Set platform environment
    export SCRATCHBIRD_PLATFORM=windows
    export SCRATCHBIRD_ARCH=x64
    export SCRATCHBIRD_CROSS_COMPILE=yes
    
    # Update preprocessor definitions in common headers
    update_platform_defines "windows"
    
    print_success "Windows cross-compilation configuration completed"
}

# Function to update platform-specific preprocessor definitions
update_platform_defines() {
    local platform="$1"
    
    print_info "Updating platform-specific preprocessor definitions..."
    
    # Update fb_types.h (sb_types.h) with platform-specific defines
    if [ -f "src/include/sb_types.h" ]; then
        case "$platform" in
            "linux")
                # Add Linux-specific defines
                if ! grep -q "SIZEOF_LONG 8" src/include/sb_types.h; then
                    sed -i '/^#ifndef SB_TYPES_H/a\
#define SIZEOF_LONG 8\
#define FB_ALIGNMENT 8\
#define FB_DOUBLE_ALIGN 8\
#define LINUX 1\
#define AMD64 1' src/include/sb_types.h
                fi
                ;;
            "windows")
                # Add Windows-specific defines
                if ! grep -q "SIZEOF_LONG 4" src/include/sb_types.h; then
                    sed -i '/^#ifndef SB_TYPES_H/a\
#define SIZEOF_LONG 4\
#define FB_ALIGNMENT 8\
#define FB_DOUBLE_ALIGN 8\
#define WIN32 1\
#define WIN64 1\
#define _WIN32 1\
#define _WIN64 1' src/include/sb_types.h
                fi
                ;;
        esac
    fi
    
    # Update common.h with platform-specific defines
    if [ -f "src/common/common.h" ]; then
        case "$platform" in
            "linux")
                # Add Linux-specific defines to common.h
                if ! grep -q "FB_ALIGNMENT 8" src/common/common.h; then
                    sed -i '/^#ifndef COMMON_COMMON_H/a\
#ifndef FB_ALIGNMENT\
#define FB_ALIGNMENT 8\
#endif\
#ifndef FB_DOUBLE_ALIGN\
#define FB_DOUBLE_ALIGN 8\
#endif' src/common/common.h
                fi
                ;;
            "windows")
                # Add Windows-specific defines to common.h
                if ! grep -q "FB_ALIGNMENT 8" src/common/common.h; then
                    sed -i '/^#ifndef COMMON_COMMON_H/a\
#ifndef FB_ALIGNMENT\
#define FB_ALIGNMENT 8\
#endif\
#ifndef FB_DOUBLE_ALIGN\
#define FB_DOUBLE_ALIGN 8\
#endif' src/common/common.h
                fi
                ;;
        esac
    fi
}

# Function to show available platforms
show_platforms() {
    print_info "Available platforms:"
    echo "  linux   - Linux native build (gcc/g++)"
    echo "  windows - Windows cross-compilation (MinGW)"
    echo ""
    print_info "Usage: $0 {linux|windows}"
    print_info "Example: $0 linux"
}

# Main script logic
case "$TARGET_PLATFORM" in
    "linux")
        configure_linux
        ;;
    "windows")
        configure_windows
        ;;
    *)
        show_platforms
        exit 1
        ;;
esac

print_success "Platform configuration completed for: $TARGET_PLATFORM"
print_info "You can now run: ./build_scratchbird.sh build-$TARGET_PLATFORM"