# Linux to Windows Cross-Compilation Requirements

**Platform:** Linux
**Target:** Windows (x86_64)
**Document Version:** 1.0
**Last Updated:** 2026-01-03

---

## 1. Overview

This document specifies all requirements for cross-compiling ScratchBird for Windows targets from a Linux host system using MinGW-w64 toolchain.

---

## 2. System Requirements

### 2.1 Minimum System Requirements

| Component | Requirement |
|-----------|-------------|
| **Host OS** | Linux kernel 5.10+ |
| **Architecture** | x86_64 (AMD64) |
| **RAM** | 16 GB (32 GB recommended for parallel builds) |
| **Disk Space** | 15 GB free (toolchain + dependencies + build artifacts) |
| **CPU** | 4 cores minimum (8+ recommended) |

### 2.2 Supported Host Distributions

**Tier 1 (Fully Supported)**:
- Ubuntu 22.04 LTS or later
- Debian 12 (Bookworm) or later
- Fedora 38 or later

**Tier 2 (Community Supported)**:
- Arch Linux (rolling release)
- openSUSE Leap 15.5+ / Tumbleweed

---

## 3. Cross-Compilation Toolchain

### 3.1 MinGW-w64

**MinGW-w64** (Minimalist GNU for Windows):
- **Minimum Version:** GCC 11.0 (MinGW-w64 runtime 10.0+)
- **Recommended Version:** GCC 12.0 or later
- **Target:** x86_64-w64-mingw32
- **C++17/C++20 Support:** Required

**Components**:
- GCC cross-compiler for Windows
- Binutils (ld, as, ar, etc.)
- MinGW-w64 runtime libraries
- Windows API headers

### 3.2 Build System

**CMake**:
- **Minimum Version:** 3.20
- **Recommended Version:** 3.25 or later
- **Toolchain File:** Required for cross-compilation

**Build Backend**:
- **Make:** GNU Make 4.3+
- **Ninja:** 1.10+ (recommended)

**Git**:
- **Version:** 2.30 or later

---

## 4. Core Dependencies (Windows Target)

### 4.1 Required Libraries (Cross-Compiled)

All dependencies must be compiled for Windows target (x86_64-w64-mingw32):

| Library | Min Version | Purpose |
|---------|-------------|---------|
| **spdlog** | 1.10.0+ | Logging framework |
| **GoogleTest** | 1.14.0+ | Unit testing framework |
| **OpenSSL** | 1.1.1+ or 3.0+ | TLS, encryption, hashing |
| **LZ4** | 1.9.3+ | Fast compression |
| **zlib** | 1.2.11+ | General compression |

### 4.2 Threading

- **pthread-w32** or **winpthreads**: POSIX threads implementation for Windows
- Typically included with MinGW-w64 toolchain

---

## 5. Optional Dependencies (Windows Target)

### 5.1 Spatial Support (Optional)

| Library | Min Version | Purpose |
|---------|-------------|---------|
| **GEOS** | 3.10+ | Geometry operations |
| **PROJ** | 9.0+ | Geographic projections |

### 5.2 XML Support (Optional)

| Library | Min Version | Purpose |
|---------|-------------|---------|
| **libxml2** | 2.10+ | XML parsing and manipulation |

---

## 6. Package Manager Installation

### 6.1 Ubuntu / Debian (apt)

```bash
# Install MinGW-w64 toolchain
sudo apt update
sudo apt install -y \
    mingw-w64 \
    mingw-w64-tools \
    gcc-mingw-w64-x86-64 \
    g++-mingw-w64-x86-64 \
    binutils-mingw-w64-x86-64

# Install build tools
sudo apt install -y \
    cmake \
    ninja-build \
    git \
    pkg-config \
    wine64

# Install development tools
sudo apt install -y \
    autoconf \
    automake \
    libtool
```

### 6.2 Fedora / RHEL / Rocky / AlmaLinux (dnf/yum)

```bash
# Install MinGW-w64 toolchain
sudo dnf install -y \
    mingw64-gcc \
    mingw64-gcc-c++ \
    mingw64-binutils \
    mingw64-headers \
    mingw64-crt \
    mingw64-winpthreads-static

# Install build tools
sudo dnf install -y \
    cmake \
    ninja-build \
    git \
    wine

# Install development tools
sudo dnf install -y \
    autoconf \
    automake \
    libtool
```

### 6.3 Arch Linux (pacman)

```bash
# Install MinGW-w64 toolchain
sudo pacman -S --needed \
    mingw-w64-gcc \
    mingw-w64-binutils \
    mingw-w64-headers \
    mingw-w64-crt \
    mingw-w64-winpthreads

# Install build tools
sudo pacman -S --needed \
    cmake \
    ninja \
    git \
    wine

# Install development tools
sudo pacman -S --needed \
    autoconf \
    automake \
    libtool
```

---

## 7. Dependency Management

### 7.1 Option 1: MXE (M Cross Environment) - Recommended

**MXE** provides pre-built cross-compiled libraries for MinGW:

```bash
# Clone MXE repository
git clone https://github.com/mxe/mxe.git /opt/mxe
cd /opt/mxe

# Build required packages (this takes a while)
make MXE_TARGETS='x86_64-w64-mingw32.static' \
    spdlog \
    gtest \
    openssl \
    lz4 \
    zlib

# Add to PATH
export PATH=/opt/mxe/usr/bin:$PATH
```

### 7.2 Option 2: Manual Cross-Compilation

Cross-compile each dependency manually:

**Example: Cross-compiling zlib**:
```bash
# Set cross-compilation environment
export CC=x86_64-w64-mingw32-gcc
export CXX=x86_64-w64-mingw32-g++
export AR=x86_64-w64-mingw32-ar
export RANLIB=x86_64-w64-mingw32-ranlib

# Clone and build zlib
git clone https://github.com/madler/zlib.git
cd zlib
mkdir build && cd build

cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain-mingw64.cmake \
      -DCMAKE_INSTALL_PREFIX=/opt/mingw64 \
      ..
make -j$(nproc)
sudo make install
```

### 7.3 Option 3: vcpkg with MinGW

```bash
# Clone vcpkg
git clone https://github.com/Microsoft/vcpkg.git /opt/vcpkg
cd /opt/vcpkg
./bootstrap-vcpkg.sh

# Install dependencies for MinGW target
./vcpkg install spdlog:x64-mingw-static
./vcpkg install gtest:x64-mingw-static
./vcpkg install openssl:x64-mingw-static
./vcpkg install lz4:x64-mingw-static
./vcpkg install zlib:x64-mingw-static
```

---

## 8. CMake Toolchain File

### 8.1 Create Toolchain File

Create `toolchain-mingw64.cmake`:

```cmake
# toolchain-mingw64.cmake
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Specify the cross compiler
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

# Where is the target environment
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)

# Search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# Search for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Set compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -static-libgcc")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++")
set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "${CMAKE_SHARED_LIBRARY_LINK_C_FLAGS} -static-libgcc -s")
set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "${CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS} -static-libgcc -static-libstdc++ -s")

# Threading
set(CMAKE_THREAD_LIBS_INIT "-lpthread")
set(CMAKE_HAVE_THREADS_LIBRARY 1)
set(CMAKE_USE_WIN32_THREADS_INIT 0)
set(CMAKE_USE_PTHREADS_INIT 1)
set(THREADS_PREFER_PTHREAD_FLAG ON)
```

### 8.2 Toolchain File for MXE

If using MXE:

```cmake
# toolchain-mxe.cmake
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# MXE paths
set(MXE_PREFIX /opt/mxe/usr/x86_64-w64-mingw32.static)

# Specify compilers
set(CMAKE_C_COMPILER /opt/mxe/usr/bin/x86_64-w64-mingw32.static-gcc)
set(CMAKE_CXX_COMPILER /opt/mxe/usr/bin/x86_64-w64-mingw32.static-g++)
set(CMAKE_RC_COMPILER /opt/mxe/usr/bin/x86_64-w64-mingw32.static-windres)

# Target environment
set(CMAKE_FIND_ROOT_PATH ${MXE_PREFIX})

# Search mode
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
```

---

## 9. Verification Steps

### 9.1 Verify MinGW Toolchain

```bash
# Check cross-compiler
x86_64-w64-mingw32-gcc --version    # Should show GCC 11.0+
x86_64-w64-mingw32-g++ --version    # Should show GCC 11.0+

# Check binutils
x86_64-w64-mingw32-ld --version
x86_64-w64-mingw32-ar --version
```

### 9.2 Verify CMake

```bash
cmake --version  # Should show 3.20 or later
```

### 9.3 Verify Wine (Optional, for Testing)

```bash
wine64 --version  # Should show Wine 6.0+
```

### 9.4 Test Cross-Compilation

```bash
# Simple test program
cat > test.cpp << 'EOF'
#include <iostream>
int main() {
    std::cout << "Hello from Windows!" << std::endl;
    return 0;
}
EOF

# Cross-compile
x86_64-w64-mingw32-g++ -o test.exe test.cpp

# Run with Wine
wine64 test.exe  # Should print "Hello from Windows!"
```

---

## 10. CMake Configuration

### 10.1 Basic Configuration

```bash
# Clone repository
git clone https://github.com/yourusername/ScratchBird.git
cd ScratchBird

# Create build directory
mkdir build-mingw64
cd build-mingw64

# Configure with toolchain file
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain-mingw64.cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -G Ninja \
      ..
```

### 10.2 Configuration with MXE

```bash
cmake -DCMAKE_TOOLCHAIN_FILE=/opt/mxe/usr/x86_64-w64-mingw32.static/share/cmake/mxe-conf.cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -G Ninja \
      ..
```

### 10.3 Configuration with vcpkg

```bash
cmake -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake \
      -DVCPKG_TARGET_TRIPLET=x64-mingw-static \
      -DCMAKE_BUILD_TYPE=Release \
      -G Ninja \
      ..
```

### 10.4 Static Linking Configuration

```bash
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain-mingw64.cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=OFF \
      -DCMAKE_EXE_LINKER_FLAGS="-static -static-libgcc -static-libstdc++" \
      -G Ninja \
      ..
```

---

## 11. Build Commands

### 11.1 Build with Ninja

```bash
# From build directory
ninja
```

### 11.2 Build with Make

```bash
# If using Make generator
make -j$(nproc)
```

### 11.3 Build Specific Target

```bash
ninja scratchbird_core
ninja tests
```

---

## 12. Testing

### 12.1 Test on Linux with Wine

```bash
# Run executable with Wine
wine64 ./build-mingw64/scratchbird.exe --version

# Run tests with Wine
wine64 ./build-mingw64/tests/scratchbird_tests.exe
```

### 12.2 Test with CTest and Wine

```bash
# Configure CTest to use Wine
export WINEPATH="Z:/path/to/build-mingw64"
ctest --output-on-failure
```

### 12.3 Copy to Windows for Native Testing

```bash
# Package binaries
mkdir dist
cp build-mingw64/*.exe dist/
cp build-mingw64/*.dll dist/ 2>/dev/null || true

# Create archive
tar czf scratchbird-windows-x64.tar.gz dist/

# Transfer to Windows machine and test natively
```

---

## 13. Troubleshooting

### 13.1 Common Issues

**Issue: Cannot find MinGW compiler**
```bash
# Solution: Install MinGW-w64 package
sudo apt install mingw-w64        # Ubuntu/Debian
sudo dnf install mingw64-gcc      # Fedora/RHEL
```

**Issue: Missing Windows headers**
```bash
# Solution: Install MinGW headers
sudo apt install mingw-w64-common mingw-w64-x86-64-dev
```

**Issue: pthread linkage errors**
```bash
# Solution: Link against winpthreads
# Add to CMakeLists.txt:
# target_link_libraries(your_target PRIVATE pthread)
```

**Issue: DLL dependencies**
```bash
# Check DLL dependencies
x86_64-w64-mingw32-objdump -p scratchbird.exe | grep "DLL Name"

# Copy missing DLLs from MinGW
cp /usr/x86_64-w64-mingw32/lib/*.dll ./dist/

# Or use static linking (see section 10.4)
```

### 13.2 Wine Configuration Issues

```bash
# Initialize Wine prefix
WINEARCH=win64 WINEPREFIX=~/.wine64 winecfg

# Set Wine to Windows 10
WINEPREFIX=~/.wine64 winecfg
# Select "Windows 10" in Applications tab
```

### 13.3 Linker Errors

```bash
# Issue: undefined reference to WinMain
# Solution: Ensure you have a proper main() function, not WinMain
# For console applications, use main()

# Issue: Multiple definitions
# Solution: Use static linking or hidden visibility
# Add to CMakeLists.txt:
# set(CMAKE_CXX_VISIBILITY_PRESET hidden)
# set(CMAKE_VISIBILITY_INLINES_HIDDEN YES)
```

---

## 14. Packaging

### 14.1 Create Standalone Package

```bash
# Create distribution directory
mkdir -p dist/bin dist/lib

# Copy executables
cp build-mingw64/*.exe dist/bin/

# Copy required DLLs (if not statically linked)
# From MinGW runtime
cp /usr/x86_64-w64-mingw32/lib/libgcc_s_seh-1.dll dist/bin/ || true
cp /usr/x86_64-w64-mingw32/lib/libstdc++-6.dll dist/bin/ || true
cp /usr/x86_64-w64-mingw32/lib/libwinpthread-1.dll dist/bin/ || true

# From dependencies (OpenSSL, etc.)
# Note: Not needed if statically linked

# Create README
cat > dist/README.txt << 'EOF'
ScratchBird for Windows (x64)
Cross-compiled on Linux

Run: bin\scratchbird.exe --help
EOF

# Create ZIP archive
cd dist
zip -r ../scratchbird-windows-x64.zip .
```

### 14.2 Create Installer (NSIS)

**Install NSIS on Linux**:
```bash
sudo apt install nsis  # Ubuntu/Debian
sudo dnf install nsis  # Fedora/RHEL
```

**Create installer script** (`installer.nsi`):
```nsis
!define APPNAME "ScratchBird"
!define COMPANYNAME "ScratchBird Project"
!define DESCRIPTION "SQL Database Engine"
!define VERSIONMAJOR 0
!define VERSIONMINOR 1
!define VERSIONBUILD 0

RequestExecutionLevel admin
InstallDir "$PROGRAMFILES64\${APPNAME}"
Name "${APPNAME}"
OutFile "ScratchBird-Setup-x64.exe"

Section "Main Application"
    SetOutPath "$INSTDIR"
    File "dist\bin\scratchbird.exe"
    File /r "dist\bin\*.dll"
SectionEnd

Section "Uninstaller"
    WriteUninstaller "$INSTDIR\Uninstall.exe"
SectionEnd
```

**Build installer**:
```bash
makensis installer.nsi
```

---

## 15. Performance Optimization

### 15.1 Link-Time Optimization (LTO)

```bash
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain-mingw64.cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON \
      ..
```

### 15.2 Aggressive Optimization

```bash
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain-mingw64.cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_FLAGS="-O3 -march=x86-64 -mtune=generic -flto" \
      ..
```

### 15.3 Strip Binaries

```bash
# Strip debug symbols
x86_64-w64-mingw32-strip scratchbird.exe

# Strip all symbols
x86_64-w64-mingw32-strip --strip-all scratchbird.exe
```

---

## 16. Continuous Integration

### 16.1 GitHub Actions Example

```yaml
# .github/workflows/cross-compile-windows.yml
jobs:
  cross-compile:
    runs-on: ubuntu-22.04
    steps:
      - uses: actions/checkout@v4

      - name: Install MinGW toolchain
        run: |
          sudo apt update
          sudo apt install -y mingw-w64 cmake ninja-build wine64

      - name: Install MXE dependencies
        run: |
          git clone https://github.com/mxe/mxe.git /tmp/mxe
          cd /tmp/mxe
          make MXE_TARGETS='x86_64-w64-mingw32.static' spdlog gtest openssl lz4 zlib

      - name: Configure
        run: |
          cmake -DCMAKE_TOOLCHAIN_FILE=/tmp/mxe/usr/x86_64-w64-mingw32.static/share/cmake/mxe-conf.cmake \
                -DCMAKE_BUILD_TYPE=Release \
                -G Ninja \
                -B build-mingw64

      - name: Build
        run: ninja -C build-mingw64

      - name: Test with Wine
        run: |
          wine64 build-mingw64/tests/scratchbird_tests.exe

      - name: Package
        run: |
          mkdir dist
          cp build-mingw64/*.exe dist/
          cd dist
          zip -r ../scratchbird-windows-x64.zip .

      - name: Upload artifacts
        uses: actions/upload-artifact@v4
        with:
          name: windows-x64-build
          path: scratchbird-windows-x64.zip
```

---

## 17. Alternative Cross-Compilation Methods

### 17.1 Using Docker

```dockerfile
# Dockerfile.mingw64
FROM ubuntu:22.04

RUN apt update && apt install -y \
    mingw-w64 cmake ninja-build git wine64

WORKDIR /src
COPY . .

RUN mkdir build && cd build && \
    cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain-mingw64.cmake \
          -DCMAKE_BUILD_TYPE=Release \
          -G Ninja .. && \
    ninja
```

```bash
# Build in Docker
docker build -f Dockerfile.mingw64 -t scratchbird-mingw64 .
docker run --rm -v $(pwd)/dist:/dist scratchbird-mingw64 \
    cp /src/build/*.exe /dist/
```

### 17.2 Using Podman

```bash
podman build -f Dockerfile.mingw64 -t scratchbird-mingw64 .
podman run --rm -v $(pwd)/dist:/dist scratchbird-mingw64 \
    cp /src/build/*.exe /dist/
```

---

## 18. Version Matrix

Tested and supported combinations:

| Host Distribution | MinGW Version | CMake | Wine | Status |
|-------------------|---------------|-------|------|--------|
| Ubuntu 22.04 | GCC 11.3 (MinGW) | 3.22 | 7.0 | ✅ Tested |
| Debian 12 | GCC 12.2 (MinGW) | 3.25 | 8.0 | ✅ Tested |
| Fedora 38 | GCC 12.2 (MinGW) | 3.26 | 8.0 | ✅ Tested |
| Arch Linux | GCC 13.1 (MinGW) | 3.27 | 8.0 | ○ Community |

---

## 19. Additional Resources

- **MinGW-w64 Documentation:** https://www.mingw-w64.org/
- **MXE Documentation:** https://mxe.cc/
- **CMake Cross-Compiling:** https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html#cross-compiling
- **Wine Documentation:** https://www.winehq.org/documentation

---

**Document Version:** 1.0
**Last Updated:** 2026-01-03
**Maintainer:** Build Infrastructure Team
**Status:** Beta Preparation
