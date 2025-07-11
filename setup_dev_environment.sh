#!/bin/bash

# ScratchBird Development Environment Setup
# Detects system configuration and sets up build environment
# Version: 0.5 - Portable for all developers

set -e

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Get script directory (works regardless of where script is called from)
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

# Function to detect OS and distribution
detect_os() {
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        OS="linux"
        if command -v lsb_release &> /dev/null; then
            DISTRO=$(lsb_release -si)
            DISTRO_VERSION=$(lsb_release -sr)
        elif [[ -f /etc/os-release ]]; then
            DISTRO=$(grep -oP '^ID=\K.*' /etc/os-release | tr -d '"')
            DISTRO_VERSION=$(grep -oP '^VERSION_ID=\K.*' /etc/os-release | tr -d '"')
        else
            DISTRO="unknown"
            DISTRO_VERSION="unknown"
        fi
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        OS="macos"
        DISTRO="macos"
        DISTRO_VERSION=$(sw_vers -productVersion)
    else
        OS="unknown"
        DISTRO="unknown"
        DISTRO_VERSION="unknown"
    fi
    
    print_status "Detected OS: $OS ($DISTRO $DISTRO_VERSION)"
}

# Function to detect architecture
detect_arch() {
    ARCH=$(uname -m)
    case $ARCH in
        x86_64)
            ARCH="x64"
            ;;
        i386|i686)
            ARCH="x86"
            ;;
        aarch64|arm64)
            ARCH="arm64"
            ;;
        armv7l)
            ARCH="armv7"
            ;;
        *)
            print_warning "Unknown architecture: $ARCH, assuming x64"
            ARCH="x64"
            ;;
    esac
    
    print_status "Detected architecture: $ARCH"
}

# Function to check for required tools
check_build_tools() {
    print_status "Checking build tools..."
    
    local missing_tools=()
    
    # Essential build tools
    if ! command -v gcc &> /dev/null; then
        missing_tools+=("gcc")
    fi
    
    if ! command -v g++ &> /dev/null; then
        missing_tools+=("g++")
    fi
    
    if ! command -v make &> /dev/null; then
        missing_tools+=("make")
    fi
    
    if ! command -v cmake &> /dev/null; then
        missing_tools+=("cmake")
    fi
    
    if ! command -v autoconf &> /dev/null; then
        missing_tools+=("autoconf")
    fi
    
    if ! command -v automake &> /dev/null; then
        missing_tools+=("automake")
    fi
    
    if ! command -v libtool &> /dev/null && ! command -v libtoolize &> /dev/null; then
        missing_tools+=("libtool")
    fi
    
    if [[ ${#missing_tools[@]} -gt 0 ]]; then
        print_error "Missing required build tools: ${missing_tools[*]}"
        print_info "Please install them using your package manager:"
        
        case $DISTRO in
            ubuntu|debian)
                echo "  sudo apt-get install build-essential cmake autoconf automake libtool"
                ;;
            fedora|rhel|centos)
                echo "  sudo dnf install gcc gcc-c++ make cmake autoconf automake libtool"
                ;;
            arch|manjaro)
                echo "  sudo pacman -S base-devel cmake autoconf automake libtool"
                ;;
            *)
                echo "  Install gcc, g++, make, cmake, autoconf, automake, libtool"
                ;;
        esac
        return 1
    fi
    
    print_success "All required build tools found"
}

# Function to check for cross-compilation tools
check_cross_tools() {
    print_status "Checking cross-compilation tools..."
    
    if command -v x86_64-w64-mingw32-gcc &> /dev/null; then
        print_success "MinGW cross-compiler found"
        MINGW_AVAILABLE=true
    else
        print_warning "MinGW cross-compiler not found"
        print_info "To build for Windows, install MinGW:"
        
        case $DISTRO in
            ubuntu|debian)
                echo "  sudo apt-get install mingw-w64"
                ;;
            fedora|rhel|centos)
                echo "  sudo dnf install mingw64-gcc mingw64-gcc-c++"
                ;;
            arch|manjaro)
                echo "  sudo pacman -S mingw-w64-gcc"
                ;;
            *)
                echo "  Install mingw-w64 package"
                ;;
        esac
        MINGW_AVAILABLE=false
    fi
}

# Function to check for system libraries
check_system_libraries() {
    print_status "Checking system libraries..."
    
    local missing_libs=()
    
    # Check for libcds
    if [[ -f /usr/lib/x86_64-linux-gnu/libcds-s.a ]] || \
       [[ -f /usr/lib64/libcds-s.a ]] || \
       [[ -f /usr/lib/libcds-s.a ]] || \
       [[ -f /usr/local/lib/libcds-s.a ]]; then
        print_success "libcds found"
    else
        missing_libs+=("libcds-dev")
    fi
    
    # Check for RE2
    if [[ -f /usr/lib/x86_64-linux-gnu/libre2.so ]] || \
       [[ -f /usr/lib64/libre2.so ]] || \
       [[ -f /usr/lib/libre2.so ]] || \
       [[ -f /usr/local/lib/libre2.so ]]; then
        print_success "RE2 library found"
    else
        missing_libs+=("libre2-dev")
    fi
    
    # Check for readline/editline
    if [[ -f /usr/lib/x86_64-linux-gnu/libreadline.so ]] || \
       [[ -f /usr/lib64/libreadline.so ]] || \
       [[ -f /usr/lib/libreadline.so ]] || \
       [[ -f /usr/local/lib/libreadline.so ]]; then
        print_success "readline library found"
    else
        missing_libs+=("libreadline-dev")
    fi
    
    if [[ ${#missing_libs[@]} -gt 0 ]]; then
        print_error "Missing required libraries: ${missing_libs[*]}"
        print_info "Please install them using your package manager:"
        
        case $DISTRO in
            ubuntu|debian)
                echo "  sudo apt-get install ${missing_libs[*]}"
                ;;
            fedora|rhel|centos)
                # Convert debian package names to RPM equivalents
                local rpm_libs=()
                for lib in "${missing_libs[@]}"; do
                    case $lib in
                        libcds-dev) rpm_libs+=("libcds-devel") ;;
                        libre2-dev) rpm_libs+=("re2-devel") ;;
                        libreadline-dev) rpm_libs+=("readline-devel") ;;
                        *) rpm_libs+=("$lib") ;;
                    esac
                done
                echo "  sudo dnf install ${rpm_libs[*]}"
                ;;
            arch|manjaro)
                # Convert debian package names to Arch equivalents
                local arch_libs=()
                for lib in "${missing_libs[@]}"; do
                    case $lib in
                        libcds-dev) arch_libs+=("libcds") ;;
                        libre2-dev) arch_libs+=("re2") ;;
                        libreadline-dev) arch_libs+=("readline") ;;
                        *) arch_libs+=("$lib") ;;
                    esac
                done
                echo "  sudo pacman -S ${arch_libs[*]}"
                ;;
            *)
                echo "  Install development packages for: libcds, re2, readline"
                ;;
        esac
        return 1
    fi
    
    print_success "All required libraries found"
}

# Function to create build configuration
create_build_config() {
    print_status "Creating build configuration..."
    
    local config_file="$PROJECT_ROOT/build_config.env"
    
    cat > "$config_file" << EOF
# ScratchBird Build Configuration
# Generated by setup_dev_environment.sh
# This file is specific to your development environment

# System information
OS=$OS
DISTRO=$DISTRO
DISTRO_VERSION=$DISTRO_VERSION
ARCH=$ARCH

# Build paths (relative to project root)
PROJECT_ROOT=$PROJECT_ROOT
BUILD_TYPE=Release
PARALLEL_JOBS=$(nproc)

# Cross-compilation support
MINGW_AVAILABLE=$MINGW_AVAILABLE

# Platform targets
LINUX_TARGET=linux-$ARCH
WINDOWS_TARGET=windows-$ARCH

# Compiler settings
CC=gcc
CXX=g++
AR=ar
RANLIB=ranlib

# MinGW settings (if available)
MINGW_PREFIX=x86_64-w64-mingw32
MINGW_CC=\${MINGW_PREFIX}-gcc
MINGW_CXX=\${MINGW_PREFIX}-g++
MINGW_AR=\${MINGW_PREFIX}-ar
MINGW_RANLIB=\${MINGW_PREFIX}-ranlib

# Library paths (auto-detected)
LIBCDS_PATH=$(find /usr/lib* /usr/local/lib* -name "libcds-s.a" 2>/dev/null | head -n1)
RE2_PATH=$(find /usr/lib* /usr/local/lib* -name "libre2.so*" 2>/dev/null | head -n1)
READLINE_PATH=$(find /usr/lib* /usr/local/lib* -name "libreadline.so*" 2>/dev/null | head -n1)

# Include paths
LIBCDS_INC=$(find /usr/include* /usr/local/include* -name "cds" -type d 2>/dev/null | head -n1)
RE2_INC=$(find /usr/include* /usr/local/include* -name "re2" -type d 2>/dev/null | head -n1)

# Export all variables
export OS DISTRO DISTRO_VERSION ARCH
export PROJECT_ROOT BUILD_TYPE PARALLEL_JOBS
export MINGW_AVAILABLE LINUX_TARGET WINDOWS_TARGET
export CC CXX AR RANLIB
export MINGW_PREFIX MINGW_CC MINGW_CXX MINGW_AR MINGW_RANLIB
export LIBCDS_PATH RE2_PATH READLINE_PATH
export LIBCDS_INC RE2_INC
EOF
    
    print_success "Build configuration created: $config_file"
}

# Function to show setup summary
show_setup_summary() {
    print_status "Setup Summary:"
    echo "  Operating System: $OS ($DISTRO $DISTRO_VERSION)"
    echo "  Architecture: $ARCH"
    echo "  Project Root: $PROJECT_ROOT"
    echo "  Cross-compilation: $([ "$MINGW_AVAILABLE" = true ] && echo "Available" || echo "Not available")"
    echo ""
    print_status "Available build commands:"
    echo "  ./build_scratchbird.sh build-linux    - Build for Linux"
    if [ "$MINGW_AVAILABLE" = true ]; then
        echo "  ./build_scratchbird.sh build-windows  - Build for Windows"
        echo "  ./build_scratchbird.sh build-all      - Build for all platforms"
    fi
    echo "  ./build_scratchbird.sh clean           - Clean build artifacts"
    echo "  ./build_scratchbird.sh tools           - Show renamed tools"
    echo ""
    print_status "ScratchBird v0.5 features:"
    echo "  - FROM-less SELECT: SELECT GEN_UUID();"
    echo "  - Multi-row INSERT: INSERT INTO t VALUES (1,2),(3,4);"
    echo "  - Hierarchical schemas: schema1.schema2.table"
    echo "  - Database links with schema resolution"
    echo "  - All tools renamed with sb_ prefix"
}

# Main setup function
main() {
    print_status "ScratchBird Development Environment Setup"
    print_status "========================================="
    
    detect_os
    detect_arch
    
    if ! check_build_tools; then
        print_error "Missing build tools. Please install them and run this script again."
        exit 1
    fi
    
    check_cross_tools
    
    if ! check_system_libraries; then
        print_error "Missing system libraries. Please install them and run this script again."
        exit 1
    fi
    
    create_build_config
    show_setup_summary
    
    print_success "Development environment setup completed!"
    print_info "You can now build ScratchBird using the build scripts."
}

# Run main function
main "$@"