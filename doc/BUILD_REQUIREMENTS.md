# ScratchBird Build Requirements - Development Environment Setup

**Version**: Alpha 0.6.0  
**Target Platforms**: Windows 10/11, Ubuntu Linux 25.04  
**Documentation Date**: July 27, 2025  
**Status**: ✅ **Test Ready** - Still missing features to be Implemented  

---

## Overview

This document provides comprehensive build requirements and development environment setup instructions for compiling ScratchBird from source code. ScratchBird supports building on Windows and Ubuntu Linux platforms with complete toolchain documentation.

### Supported Build Platforms

- **Windows**: Windows 10 (version 1909+), Windows 11
- **Linux**: Ubuntu 25.04 LTS (Noble Numbat), Ubuntu 24.04 LTS (recommended)
- **Architecture**: x86_64 (64-bit) required on all platforms

### Build System Overview

ScratchBird uses a custom GNU Make-based build system:
- **Primary**: GNU Make with custom build scripts in `build_scripts/`
- **Build Configuration**: Platform-specific files in `builds/posix/`
- **Command Format**: `make TARGET=Release [targets]`
- **Dependencies**: Built-in external libraries in `extern/` directory
- **Output Structure**: `gen/Release/scratchbird/` → `release/alpha0.6.0/[platform]/`

---

## Windows Build Requirements

### System Requirements

**Operating System:**
- Windows 10 version 1909 (November 2019 Update) or newer
- Windows 11 (all versions)
- Windows Server 2019 or Windows Server 2022

**Hardware Requirements:**
- CPU: x86_64 (64-bit) processor with SSE4.1 support
- RAM: Minimum 8GB, recommended 16GB for parallel builds
- Disk Space: 5GB free space for build artifacts
- Network: Internet connection for downloading dependencies

### Core Development Tools

#### MSYS2/MinGW Development Environment

**MSYS2 (Recommended for ScratchBird):**
```
Version: Latest MSYS2 release
Architecture: x86_64
Purpose: Provides POSIX-compatible build environment on Windows
Download: https://www.msys2.org/
```

**Installation Steps:**
1. Download MSYS2 installer from https://www.msys2.org/
2. Install to default location (C:\msys64)
3. Run MSYS2 MINGW64 terminal
4. Update package database:
   ```bash
   pacman -Syu
   ```
5. Install development tools:
   ```bash
   pacman -S mingw-w64-x86_64-gcc
   pacman -S mingw-w64-x86_64-make
   pacman -S mingw-w64-x86_64-pkg-config
   pacman -S git
   ```

#### Alternative: Visual Studio (Limited Support)

**Note**: ScratchBird primarily uses GNU Make. Visual Studio support is limited.

**Visual Studio Build Tools:**
```
Edition: Build Tools for Visual Studio 2022
Components Required:
  - MSVC v143 compiler toolset
  - Windows 10/11 SDK
  - Git for Windows
```

**For developers requiring Visual Studio integration, consider using WSL2 with Ubuntu instead.**

### External Dependencies (Built-in)

**ScratchBird includes external dependencies in `extern/` directory:**
```
extern/
├── libtommath/          # Arbitrary precision math library
├── libtomcrypt/         # Cryptographic functions  
├── decNumber/           # Decimal arithmetic
├── editline/            # Command line editing
├── re2/                 # Regular expressions
├── icu/                 # Unicode support (platform packages)
└── boost/               # C++ utilities (headers only)
```

**No package manager required** - dependencies are built automatically with:
```bash
make TARGET=Release external
```

**System Dependencies (install via package manager):**
- ICU (International Components for Unicode) - system package
- Standard C++ compiler and libraries

### Additional Windows Dependencies

#### Git for Windows
```
Version: 2.40 or newer
Download: https://git-scm.com/download/win
Options:
  - Use Git from the Windows Command Prompt
  - Checkout Windows-style, commit Unix-style line endings
  - Use Windows' default console window
```

#### Python (for build scripts)
```
Version: Python 3.9 or newer
Download: https://www.python.org/downloads/windows/
Options:
  - Add Python to PATH
  - Install pip
  - Install for all users (recommended)
```

#### NASM (for assembly optimizations)
```
Version: 2.15 or newer
Download: https://www.nasm.us/pub/nasm/releasebuilds/
Install to: C:\nasm
Add to PATH: C:\nasm
```

### Windows Environment Variables

**Required Environment Variables:**
```batch
set VCPKG_ROOT=C:\vcpkg
set PATH=%PATH%;C:\nasm;%VCPKG_ROOT%\installed\x64-windows\bin
set CMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
```

**Verification Script:**
```powershell
# Create verification script: check_windows_env.ps1
Write-Host "ScratchBird Windows Build Environment Check"
Write-Host "========================================="

# Check Visual Studio
$vsPath = "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\Common7\IDE\devenv.exe"
if (Test-Path $vsPath) {
    Write-Host "✓ Visual Studio 2022 found"
} else {
    Write-Host "✗ Visual Studio 2022 not found"
}

# Check CMake
try {
    $cmakeVersion = & cmake --version 2>$null
    Write-Host "✓ CMake: $($cmakeVersion[0])"
} catch {
    Write-Host "✗ CMake not found"
}

# Check vcpkg
if ($env:VCPKG_ROOT -and (Test-Path "$env:VCPKG_ROOT\vcpkg.exe")) {
    Write-Host "✓ vcpkg found at $env:VCPKG_ROOT"
} else {
    Write-Host "✗ vcpkg not properly configured"
}

# Check Git
try {
    $gitVersion = & git --version 2>$null
    Write-Host "✓ Git: $gitVersion"
} catch {
    Write-Host "✗ Git not found"
}

# Check Python
try {
    $pythonVersion = & python --version 2>$null
    Write-Host "✓ Python: $pythonVersion"
} catch {
    Write-Host "✗ Python not found"
}

# Check NASM
try {
    $nasmVersion = & nasm -version 2>$null
    Write-Host "✓ NASM: $($nasmVersion[0])"
} catch {
    Write-Host "✗ NASM not found"
}
```

---

## Ubuntu Linux 25.04 Build Requirements

### System Requirements

**Operating System:**
- Ubuntu 25.04 LTS (Noble Numbat) - Latest
- Ubuntu 24.04 LTS (Noble Numbat) - Recommended for stability
- Ubuntu 22.04 LTS (Jammy Jellyfish) - Minimum supported

**Hardware Requirements:**
- CPU: x86_64 (64-bit) processor with SSE4.1 support
- RAM: Minimum 4GB, recommended 8GB for parallel builds
- Disk Space: 3GB free space for build artifacts
- Network: Internet connection for package downloads

### Core Development Tools

#### GCC Compiler Toolchain

**Installation:**
```bash
# Update package lists
sudo apt update

# Install build essentials
sudo apt install -y build-essential

# Install specific GCC version (recommended)
sudo apt install -y gcc-13 g++-13

# Set as default (optional)
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-13 100
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-13 100
```

**Verify Installation:**
```bash
gcc --version    # Should show GCC 13.x or newer
g++ --version    # Should show G++ 13.x or newer
```

#### Alternative: Clang Compiler

**Installation:**
```bash
# Install Clang 17 (latest)
sudo apt install -y clang-17 libc++-17-dev libc++abi-17-dev

# Set as default (if preferred over GCC)
sudo update-alternatives --install /usr/bin/clang clang /usr/bin/clang-17 100
sudo update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-17 100
```

### Build System Tools

#### GNU Make and Build Tools

**Installation:**
```bash
# Essential build tools
sudo apt install -y build-essential

# Verify GNU Make version
make --version  # Should be 4.0 or newer

# Additional build utilities
sudo apt install -y pkg-config autoconf automake libtool
```

**Note**: ScratchBird's build system is specifically designed for GNU Make. Other build systems like Ninja or CMake have limited or no support.

### Version Control

#### Git

**Installation:**
```bash
# Install Git
sudo apt install -y git

# Configure Git (replace with your info)
git config --global user.name "Your Name"
git config --global user.email "your.email@example.com"
git config --global init.defaultBranch main

# Verify installation
git --version
```

### ScratchBird Dependencies

#### Core System Dependencies

**Installation:**
```bash
# ICU (International Components for Unicode) - Required
sudo apt install -y libicu-dev

# Standard development libraries
sudo apt install -y libc6-dev linux-libc-dev

# Threading and system libraries
sudo apt install -y libnuma-dev

# Optional: Package config for library detection
sudo apt install -y pkg-config
```

**Note**: ScratchBird builds most dependencies from source in the `extern/` directory. Only ICU and system libraries need to be installed via package manager.

#### Optional Dependencies

**Installation:**
```bash
# Boost libraries (optional, for enhanced features)
sudo apt install -y libboost-all-dev

# Python development (for UDR support)
sudo apt install -y python3-dev python3-pip

# Java development (for UDR support)
sudo apt install -y default-jdk

# Documentation tools
sudo apt install -y doxygen graphviz

# Debugging tools
sudo apt install -y gdb valgrind

# Performance analysis
sudo apt install -y perf-tools-unstable
```

#### Database Testing Dependencies

**Installation:**
```bash
# For running test suites
sudo apt install -y expect tcl-dev

# Memory leak detection
sudo apt install -y libc6-dbg

# Network testing
sudo apt install -y netcat-openbsd telnet

# Text processing for tests
sudo apt install -y sed gawk grep
```

### Ubuntu Environment Setup

#### Environment Variables

**Add to ~/.bashrc or ~/.profile:**
```bash
# ScratchBird build configuration
export SCRATCHBIRD_ROOT="$HOME/scratchbird"
export SCRATCHBIRD_BUILD="$SCRATCHBIRD_ROOT/builds"

# Compiler preferences
export CC=gcc-13
export CXX=g++-13

# Build optimization
export MAKEFLAGS="-j$(nproc)"

# CMake configuration
export CMAKE_BUILD_TYPE=Release
export CMAKE_INSTALL_PREFIX=/usr/local

# Library paths
export LD_LIBRARY_PATH="/usr/local/lib:$LD_LIBRARY_PATH"
export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH"
```

**Apply changes:**
```bash
source ~/.bashrc
```

#### System Limits Configuration

**For optimal build performance, update system limits:**
```bash
# Create limits configuration
sudo tee /etc/security/limits.d/scratchbird.conf << EOF
# ScratchBird build limits
*               soft    nofile          65536
*               hard    nofile          65536
*               soft    nproc           32768
*               hard    nproc           32768
EOF

# Update sysctl for shared memory
sudo tee -a /etc/sysctl.conf << EOF
# ScratchBird shared memory settings
kernel.shmmax = 268435456
kernel.shmall = 2097152
EOF

# Apply sysctl changes
sudo sysctl -p
```

### Ubuntu Verification Script

**Create verification script:**
```bash
#!/bin/bash
# save as: check_ubuntu_env.sh

echo "ScratchBird Ubuntu Build Environment Check"
echo "=========================================="

# Check GCC
if command -v gcc >/dev/null 2>&1; then
    echo "✓ GCC: $(gcc --version | head -1)"
else
    echo "✗ GCC not found"
fi

# Check G++
if command -v g++ >/dev/null 2>&1; then
    echo "✓ G++: $(g++ --version | head -1)"
else
    echo "✗ G++ not found"
fi

# Check CMake
if command -v cmake >/dev/null 2>&1; then
    echo "✓ CMake: $(cmake --version | head -1)"
else
    echo "✗ CMake not found"
fi

# Check Make
if command -v make >/dev/null 2>&1; then
    echo "✓ Make: $(make --version | head -1)"
else
    echo "✗ Make not found"
fi

# Check Git
if command -v git >/dev/null 2>&1; then
    echo "✓ Git: $(git --version)"
else
    echo "✗ Git not found"
fi

# Check key libraries
echo "Checking libraries..."

for lib in libicu-dev zlib1g-dev libssl-dev libtommath-dev libre2-dev; do
    if dpkg -l | grep -q "^ii  $lib "; then
        echo "✓ $lib installed"
    else
        echo "✗ $lib not installed"
    fi
done

# Check system limits
echo "System limits:"
echo "  Max open files: $(ulimit -n)"
echo "  Max processes: $(ulimit -u)"

# Check available space
echo "Disk space:"
df -h . | tail -1 | awk '{print "  Available: " $4 " (" $5 " used)"}'

# Check memory
echo "Memory:"
free -h | grep "^Mem:" | awk '{print "  Total: " $2 ", Available: " $7}'

# Check CPU cores
echo "CPU cores: $(nproc)"

chmod +x check_ubuntu_env.sh
```

---

## Cross-Platform Dependencies

### Third-Party Libraries

ScratchBird requires several third-party libraries that must be available on all platforms:

#### Required Libraries

**libtommath (Multi-precision integer library):**
- **Version**: 1.2.0 or newer
- **Purpose**: Large integer arithmetic for cryptographic functions
- **Windows**: Install via vcpkg
- **Linux**: Install libtomath-dev package

**libtomcrypt (Cryptographic library):**
- **Version**: 1.18.2 or newer
- **Purpose**: Cryptographic functions and algorithms
- **Windows**: Install via vcpkg
- **Linux**: Install libtomcrypt-dev package

**ICU (International Components for Unicode):**
- **Version**: 70.0 or newer
- **Purpose**: Unicode and localization support
- **Windows**: Install via vcpkg
- **Linux**: Install libicu-dev package

**zlib (Compression library):**
- **Version**: 1.2.11 or newer
- **Purpose**: Data compression and decompression
- **Windows**: Install via vcpkg
- **Linux**: Install zlib1g-dev package

**RE2 (Regular expression library):**
- **Version**: 2022-06-01 or newer
- **Purpose**: Fast, safe regular expression matching
- **Windows**: Install via vcpkg
- **Linux**: Install libre2-dev package

#### Optional Libraries

**OpenSSL:**
- **Version**: 3.0 or newer
- **Purpose**: Enhanced cryptographic functions and SSL/TLS support
- **Windows**: Install via vcpkg
- **Linux**: Install libssl-dev package

**Boost Libraries:**
- **Version**: 1.80 or newer
- **Purpose**: Enhanced C++ utilities and algorithms
- **Windows**: Install boost-system and boost-filesystem via vcpkg
- **Linux**: Install libboost-all-dev package

### Development Dependencies

#### For UDR (User Defined Routines) Support

**Python Development:**
- **Version**: Python 3.9 or newer
- **Windows**: Install from python.org
- **Linux**: Install python3-dev package

**Java Development:**
- **Version**: OpenJDK 11 or newer
- **Windows**: Install OpenJDK from adoptium.net
- **Linux**: Install default-jdk package

#### For Testing and Quality Assurance

**Testing Framework:**
- **Purpose**: Automated testing infrastructure
- **Windows**: Included in build system
- **Linux**: Install expect and tcl-dev packages

**Memory Analysis:**
- **Windows**: Application Verifier (Windows SDK)
- **Linux**: Valgrind and AddressSanitizer

---

## Build Performance Optimization

### Hardware Recommendations

**For Optimal Build Performance:**

**CPU:**
- Intel Core i7/i9 or AMD Ryzen 7/9 series
- Minimum 8 cores/16 threads
- Base clock speed 3.0GHz or higher

**Memory:**
- Minimum: 16GB DDR4
- Recommended: 32GB DDR4 or DDR5
- Fast memory speeds (3200MHz+) beneficial

**Storage:**
- NVMe SSD strongly recommended
- Minimum 500GB available space
- SATA SSD acceptable but slower
- Traditional HDD not recommended

**Network:**
- Broadband internet for dependency downloads
- Consider local package mirrors for faster downloads

### Build Configuration

**Parallel Build Settings:**

**Windows:**
```batch
REM Use all available CPU cores for builds
set CL=/MP
set CMAKE_BUILD_PARALLEL_LEVEL=%NUMBER_OF_PROCESSORS%
```

**Linux:**
```bash
# Use all available CPU cores
export MAKEFLAGS="-j$(nproc)"
export CMAKE_BUILD_PARALLEL_LEVEL=$(nproc)
```

**Memory Usage Optimization:**

**Windows (Visual Studio):**
- Limit parallel builds if memory constrained: `/maxcpucount:4`
- Increase virtual memory if needed
- Close unnecessary applications during build

**Linux:**
- Monitor memory usage: `free -h`
- Limit parallel jobs if memory constrained: `make -j4`
- Enable swap if necessary (not recommended for SSD)

---

## Troubleshooting Common Issues

### Windows Issues

**Visual Studio Integration Problems:**
```powershell
# Re-integrate vcpkg
cd C:\vcpkg
.\vcpkg integrate remove
.\vcpkg integrate install

# Clear Visual Studio cache
del /s /q "%LOCALAPPDATA%\Microsoft\VisualStudio\16.0_*\ComponentModelCache"
del /s /q "%LOCALAPPDATA%\Microsoft\VisualStudio\17.0_*\ComponentModelCache"
```

**CMake Configuration Issues:**
```batch
REM Clear CMake cache
rmdir /s /q build
mkdir build
cd build

REM Reconfigure with verbose output
cmake .. -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake -DCMAKE_VERBOSE_MAKEFILE=ON
```

**Dependency Resolution Problems:**
```powershell
# Update vcpkg
cd C:\vcpkg
git pull
.\bootstrap-vcpkg.bat

# Reinstall packages
.\vcpkg remove --recurse zlib icu libtommath libtomcrypt re2
.\vcpkg install zlib:x64-windows icu:x64-windows libtommath:x64-windows libtomcrypt:x64-windows re2:x64-windows
```

### Linux Issues

**Missing Dependencies:**
```bash
# Update package lists
sudo apt update

# Fix broken packages
sudo apt --fix-broken install

# Reinstall build essentials
sudo apt install --reinstall build-essential
```

**Compiler Version Issues:**
```bash
# Check available compilers
apt list --installed | grep gcc
apt list --installed | grep clang

# Update alternatives
sudo update-alternatives --config gcc
sudo update-alternatives --config g++
```

**Library Linking Problems:**
```bash
# Update library cache
sudo ldconfig

# Check library locations
ldconfig -p | grep -E "(tommath|tomcrypt|icu|ssl)"

# Verify pkg-config
pkg-config --list-all | grep -E "(icu|openssl|zlib)"
```

**Permission Issues:**
```bash
# Fix file permissions
find ~/scratchbird -type f -name "*.sh" -exec chmod +x {} \;

# Fix directory permissions
find ~/scratchbird -type d -exec chmod 755 {} \;

# Check user groups
groups $USER
```

### Cross-Platform Issues

**Character Encoding Problems:**
- Ensure all source files use UTF-8 encoding
- Configure Git for proper line ending handling
- Set locale environment variables consistently

**Path Length Limitations:**
- Windows: Enable long path support in Windows 10/11
- Use shorter build directory names
- Consider using subst on Windows for shorter paths

**Build Reproducibility:**
- Use specific versions of dependencies
- Document exact compiler versions used
- Maintain consistent build environments

---

## Next Steps

After setting up your development environment:

1. **Verify Installation**: Run the provided verification scripts
2. **Clone Repository**: Follow instructions in BUILD_INSTRUCTIONS.md
3. **Configure Build**: Set up platform-specific build configuration
4. **Compile ScratchBird**: Execute the build process
5. **Run Tests**: Validate your build with the test suite

For detailed build instructions, see the companion document: **BUILD_INSTRUCTIONS.md**

---

*This document covers the complete development environment setup for ScratchBird. For step-by-step build instructions, refer to BUILD_INSTRUCTIONS.md*