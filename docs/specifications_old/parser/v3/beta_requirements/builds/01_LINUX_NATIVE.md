# Linux Native Build Requirements

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Platform:** Linux
**Target:** Linux (x86_64, ARM64)
**Document Version:** 1.0
**Last Updated:** 2026-01-02

---

## 1. Overview

This document specifies all requirements for building ScratchBird natively on Linux systems. Covers major distributions including Ubuntu, Debian, Fedora, RHEL, Arch Linux, and openSUSE.

---

## 2. System Requirements

### 2.1 Minimum System Requirements

| Component | Requirement |
|-----------|-------------|
| **OS** | Linux kernel 5.10+ |
| **Architecture** | x86_64 (AMD64) or ARM64 (aarch64) |
| **RAM** | 8 GB (16 GB recommended for parallel builds) |
| **Disk Space** | 10 GB free (source + build artifacts + dependencies) |
| **CPU** | 4 cores minimum (8+ recommended for faster builds) |

### 2.2 Supported Distributions

**Tier 1 (Fully Supported)**:
- Ubuntu 22.04 LTS (Jammy Jellyfish) or later
- Debian 12 (Bookworm) or later
- Fedora 38 or later
- RHEL 9 or later / Rocky Linux 9+ / AlmaLinux 9+

**Tier 2 (Community Supported)**:
- Arch Linux (rolling release)
- openSUSE Leap 15.5+ / Tumbleweed
- Gentoo Linux
- Linux Mint 21+

---

## 3. Build Tools

### 3.1 Compiler Toolchain

**GCC** (Recommended):
- **Minimum Version:** GCC 11.0
- **Recommended Version:** GCC 12.0 or later
- **C++17/C++20 Support:** Required

**Clang** (Alternative):
- **Minimum Version:** Clang 14.0
- **Recommended Version:** Clang 15.0 or later
- **C++17/C++20 Support:** Required

### 3.2 Build System

**CMake**:
- **Minimum Version:** 3.20
- **Recommended Version:** 3.25 or later

**Build Backend** (choose one):
- **Make:** GNU Make 4.3+
- **Ninja:** 1.10+ (recommended for faster builds)

**Git**:
- **Version:** 2.30 or later
- **Purpose:** Source code management, version control

---

## 4. Core Dependencies

### 4.1 Required Libraries

| Library | Min Version | Purpose |
|---------|-------------|---------|
| **spdlog** | 1.10.0+ | Logging framework |
| **GoogleTest** | 1.14.0+ | Unit testing framework |
| **OpenSSL** | 1.1.1+ or 3.0+ | TLS, encryption, hashing |
| **LZ4** | 1.9.3+ | Fast compression |
| **pthread** | (included in glibc) | POSIX threading |
| **zlib** | 1.2.11+ | General compression |

### 4.2 Development Headers

All libraries require corresponding `-dev` or `-devel` packages:
- `libspdlog-dev` or `spdlog-devel`
- `libgtest-dev` or `gtest-devel`
- `libssl-dev` or `openssl-devel`
- `liblz4-dev` or `lz4-devel`
- `zlib1g-dev` or `zlib-devel`

---

## 5. Optional Dependencies

### 5.1 Spatial Support (Optional)

| Library | Min Version | Purpose |
|---------|-------------|---------|
| **GEOS** | 3.10+ | Geometry operations |
| **PROJ** | 9.0+ | Geographic projections |

### 5.2 XML Support (Optional)

| Library | Min Version | Purpose |
|---------|-------------|---------|
| **libxml2** | 2.10+ | XML parsing and manipulation |

### 5.3 Development Tools (Optional but Recommended)

| Tool | Purpose |
|------|---------|
| **ccache** | Compiler cache for faster rebuilds |
| **clang-format** | Code formatting |
| **clang-tidy** | Static analysis |
| **valgrind** | Memory debugging |
| **gdb** | Debugging |
| **perf** | Performance profiling |

---

## 6. Package Manager Installation Commands

### 6.1 Ubuntu / Debian (apt)

```bash
# Update package list
sudo apt update

# Install build tools
sudo apt install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    pkg-config

# Install compiler (GCC - already in build-essential)
# Or install Clang:
sudo apt install -y clang-15 clang++-15

# Install core dependencies
sudo apt install -y \
    libspdlog-dev \
    libgtest-dev \
    libssl-dev \
    liblz4-dev \
    zlib1g-dev

# Install optional dependencies
sudo apt install -y \
    libgeos-dev \
    libproj-dev \
    libxml2-dev

# Install development tools (optional)
sudo apt install -y \
    ccache \
    clang-format \
    clang-tidy \
    valgrind \
    gdb \
    linux-tools-generic  # perf
```

### 6.2 Fedora / RHEL / Rocky / AlmaLinux (dnf/yum)

```bash
# Install build tools
sudo dnf install -y \
    gcc \
    gcc-c++ \
    cmake \
    ninja-build \
    git \
    pkg-config

# Or install Clang:
sudo dnf install -y clang

# Install core dependencies
sudo dnf install -y \
    spdlog-devel \
    gtest-devel \
    openssl-devel \
    lz4-devel \
    zlib-devel

# Install optional dependencies
sudo dnf install -y \
    geos-devel \
    proj-devel \
    libxml2-devel

# Install development tools (optional)
sudo dnf install -y \
    ccache \
    clang-tools-extra \
    valgrind \
    gdb \
    perf
```

### 6.3 Arch Linux (pacman)

```bash
# Install build tools
sudo pacman -S --needed \
    base-devel \
    cmake \
    ninja \
    git \
    pkg-config

# Install compiler (GCC in base-devel)
# Or install Clang:
sudo pacman -S --needed clang

# Install core dependencies
sudo pacman -S --needed \
    spdlog \
    gtest \
    openssl \
    lz4 \
    zlib

# Install optional dependencies
sudo pacman -S --needed \
    geos \
    proj \
    libxml2

# Install development tools (optional)
sudo pacman -S --needed \
    ccache \
    clang \
    valgrind \
    gdb \
    perf
```

### 6.4 openSUSE (zypper)

```bash
# Install build tools
sudo zypper install -y \
    gcc \
    gcc-c++ \
    cmake \
    ninja \
    git \
    pkg-config

# Or install Clang:
sudo zypper install -y clang

# Install core dependencies
sudo zypper install -y \
    spdlog-devel \
    gtest \
    libopenssl-devel \
    liblz4-devel \
    zlib-devel

# Install optional dependencies
sudo zypper install -y \
    geos-devel \
    proj-devel \
    libxml2-devel

# Install development tools (optional)
sudo zypper install -y \
    ccache \
    clang-tools \
    valgrind \
    gdb \
    perf
```

---

## 7. Verification Steps

After installing all dependencies, verify they are correctly installed:

### 7.1 Verify Compiler

```bash
# GCC
gcc --version    # Should show 11.0 or later
g++ --version    # Should show 11.0 or later

# OR Clang
clang --version    # Should show 14.0 or later
clang++ --version  # Should show 14.0 or later
```

### 7.2 Verify CMake

```bash
cmake --version  # Should show 3.20 or later
```

### 7.3 Verify Build System

```bash
make --version    # GNU Make 4.3+
# OR
ninja --version   # Ninja 1.10+
```

### 7.4 Verify Core Dependencies

```bash
# Check pkg-config can find libraries
pkg-config --modversion spdlog    # Should show 1.10.0+
pkg-config --modversion openssl   # Should show 1.1.1+ or 3.0+
pkg-config --modversion lz4       # Should show 1.9.3+

# Check header files exist
ls /usr/include/spdlog/spdlog.h
ls /usr/include/openssl/ssl.h
ls /usr/include/lz4.h
```

### 7.5 Verify Optional Dependencies (if installed)

```bash
pkg-config --modversion geos      # 3.10+
pkg-config --modversion proj      # 9.0+
pkg-config --modversion libxml-2.0 # 2.10+
```

---

## 8. CMake Configuration

### 8.1 Basic Configuration (GCC)

```bash
# Clone repository
git clone https://github.com/yourusername/ScratchBird.git
cd ScratchBird

# Create build directory
mkdir build
cd build

# Configure with CMake (GCC, Make)
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER=gcc \
      -DCMAKE_CXX_COMPILER=g++ \
      ..

# Or configure with Ninja (faster)
cmake -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER=gcc \
      -DCMAKE_CXX_COMPILER=g++ \
      ..
```

### 8.2 Configuration with Clang

```bash
cmake -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER=clang \
      -DCMAKE_CXX_COMPILER=clang++ \
      ..
```

### 8.3 Debug Build Configuration

```bash
cmake -G Ninja \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_C_COMPILER=gcc \
      -DCMAKE_CXX_COMPILER=g++ \
      ..
```

### 8.4 Configuration with ccache

```bash
cmake -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER_LAUNCHER=ccache \
      -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
      ..
```

### 8.5 Configuration with Optional Features

```bash
cmake -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DENABLE_SPATIAL_SUPPORT=ON \
      -DENABLE_XML_SUPPORT=ON \
      -DENABLE_TESTING=ON \
      ..
```

---

## 9. Build Commands

### 9.1 Build with Make

```bash
# From build directory
make -j$(nproc)  # Parallel build using all CPU cores
```

### 9.2 Build with Ninja

```bash
# From build directory
ninja  # Ninja automatically uses all cores
```

### 9.3 Build Specific Target

```bash
# Build only the core library
ninja scratchbird_core

# Build only tests
ninja tests
```

### 9.4 Clean Build

```bash
# With Make
make clean

# With Ninja
ninja -t clean
```

---

## 10. Testing

### 10.1 Run All Tests

```bash
# From build directory
ctest --output-on-failure
```

### 10.2 Run Tests in Parallel

```bash
ctest -j$(nproc) --output-on-failure
```

### 10.3 Run Specific Test Suite

```bash
ctest -R unit_tests --output-on-failure
```

### 10.4 Run Tests with Verbose Output

```bash
ctest -V
```

---

## 11. Installation

### 11.1 System-Wide Installation (requires sudo)

```bash
# From build directory
sudo ninja install

# Or with Make
sudo make install
```

### 11.2 Custom Installation Prefix

```bash
# Configure with custom prefix
cmake -DCMAKE_INSTALL_PREFIX=/opt/scratchbird ..

# Build and install
ninja
sudo ninja install
```

### 11.3 Local Installation (no sudo required)

```bash
# Configure with local prefix
cmake -DCMAKE_INSTALL_PREFIX=$HOME/.local ..

# Build and install
ninja
ninja install  # No sudo needed
```

---

## 12. Troubleshooting

### 12.1 Common Issues

**Issue: CMake cannot find spdlog**
```bash
# Solution: Install spdlog-dev package
sudo apt install libspdlog-dev  # Ubuntu/Debian
sudo dnf install spdlog-devel   # Fedora/RHEL
```

**Issue: Compiler version too old**
```bash
# Check current version
gcc --version

# Ubuntu: Install newer GCC from toolchain PPA
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt update
sudo apt install gcc-12 g++-12

# Use specific version
cmake -DCMAKE_C_COMPILER=gcc-12 -DCMAKE_CXX_COMPILER=g++-12 ..
```

**Issue: Out of memory during build**
```bash
# Reduce parallel jobs
make -j2  # Use only 2 cores instead of all

# Or with Ninja
ninja -j2
```

**Issue: Missing pthread**
```bash
# pthread is part of glibc, install:
sudo apt install libc6-dev      # Ubuntu/Debian
sudo dnf install glibc-devel    # Fedora/RHEL
```

### 12.2 Library Not Found at Runtime

```bash
# Update library cache
sudo ldconfig

# Or set LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
```

### 12.3 CMake Cache Issues

```bash
# Clear CMake cache and reconfigure
rm -rf build
mkdir build
cd build
cmake ..
```

---

## 13. Performance Optimization

### 13.1 Link-Time Optimization (LTO)

```bash
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON \
      ..
```

### 13.2 Native CPU Optimization

```bash
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_FLAGS="-march=native -mtune=native" \
      ..
```

### 13.3 Compiler-Specific Optimizations

```bash
# GCC optimizations
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_FLAGS="-O3 -march=native -flto" \
      ..

# Clang optimizations
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_FLAGS="-O3 -march=native -flto=thin" \
      ..
```

---

## 14. Development Setup

### 14.1 Install Development Tools

```bash
# Ubuntu/Debian
sudo apt install \
    clang-format \
    clang-tidy \
    cppcheck \
    doxygen \
    graphviz

# Fedora/RHEL
sudo dnf install \
    clang-tools-extra \
    cppcheck \
    doxygen \
    graphviz
```

### 14.2 Configure Development Build

```bash
cmake -G Ninja \
      -DCMAKE_BUILD_TYPE=Debug \
      -DENABLE_TESTING=ON \
      -DENABLE_SANITIZERS=ON \
      -DENABLE_COVERAGE=ON \
      ..
```

### 14.3 Run Static Analysis

```bash
# clang-tidy
ninja clang-tidy

# cppcheck
cppcheck --enable=all --project=compile_commands.json
```

---

## 15. Continuous Integration Setup

### 15.1 GitHub Actions Example

```yaml
# .github/workflows/linux-build.yml
jobs:
  build:
    runs-on: ubuntu-22.04
    steps:
      - uses: actions/checkout@v4
      - name: Install dependencies
        run: |
          sudo apt update
          sudo apt install -y \
            build-essential cmake ninja-build \
            libspdlog-dev libgtest-dev libssl-dev liblz4-dev
      - name: Configure
        run: cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -B build
      - name: Build
        run: ninja -C build
      - name: Test
        run: ctest --test-dir build --output-on-failure
```

---

## 16. Version Matrix

Tested and supported combinations:

| Distribution | Compiler | CMake | Status |
|--------------|----------|-------|--------|
| Ubuntu 22.04 | GCC 11 | 3.22 | ✅ Tested |
| Ubuntu 22.04 | GCC 12 | 3.22 | ✅ Tested |
| Ubuntu 22.04 | Clang 14 | 3.22 | ✅ Tested |
| Debian 12 | GCC 12 | 3.25 | ✅ Tested |
| Fedora 38 | GCC 13 | 3.26 | ✅ Tested |
| Fedora 38 | Clang 16 | 3.26 | ✅ Tested |
| RHEL 9 | GCC 11 | 3.20 | ✅ Tested |
| Arch Linux | GCC 13 | 3.27 | ○ Community |
| openSUSE Leap | GCC 11 | 3.20 | ○ Community |

---

## 17. Additional Resources

- **CMake Documentation:** https://cmake.org/documentation/
- **GCC Documentation:** https://gcc.gnu.org/onlinedocs/
- **Clang Documentation:** https://clang.llvm.org/docs/
- **pkg-config Guide:** https://people.freedesktop.org/~dbn/pkg-config-guide.html

---

**Document Version:** 1.0
**Last Updated:** 2026-01-02
**Maintainer:** Build Infrastructure Team
**Status:** Beta Preparation
