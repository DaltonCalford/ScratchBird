# macOS Native Build Requirements

**Platform:** macOS
**Target:** macOS (x86_64, ARM64/Apple Silicon)
**Document Version:** 1.0
**Last Updated:** 2026-01-02

---

## 1. Overview

This document specifies all requirements for building ScratchBird natively on macOS systems, supporting both Intel (x86_64) and Apple Silicon (ARM64) architectures.

---

## 2. System Requirements

### 2.1 Minimum System Requirements

| Component | Requirement |
|-----------|-------------|
| **OS** | macOS 11 (Big Sur) or later |
| **Architecture** | x86_64 (Intel) or ARM64 (Apple Silicon M1/M2/M3) |
| **RAM** | 8 GB (16 GB recommended for parallel builds) |
| **Disk Space** | 15 GB free (Xcode + source + build artifacts + dependencies) |
| **CPU** | 4 cores minimum (8+ recommended for faster builds) |

### 2.2 Supported macOS Versions

**Tier 1 (Fully Supported)**:
- macOS 14 Sonoma
- macOS 13 Ventura
- macOS 12 Monterey

**Tier 2 (Community Supported)**:
- macOS 11 Big Sur

---

## 3. Build Tools

### 3.1 Xcode / Command Line Tools

**Option 1: Xcode (Full IDE)**
- **Minimum Version:** Xcode 13.0
- **Recommended Version:** Xcode 15.0 or later
- **Download:** Mac App Store or https://developer.apple.com/xcode/
- **Size:** ~12-15 GB

**Option 2: Xcode Command Line Tools (Minimal)**
- **Install Command:**
  ```bash
  xcode-select --install
  ```
- **Size:** ~1-2 GB
- **Note:** Sufficient for command-line builds without IDE

### 3.2 Compiler Toolchain

**Apple Clang** (Default with Xcode):
- **Minimum Version:** Apple Clang 13.0
- **Recommended Version:** Apple Clang 15.0+
- **C++17/C++20 Support:** Required

**LLVM Clang** (Alternative via Homebrew):
- **Minimum Version:** Clang 14.0
- **Install:** `brew install llvm`

### 3.3 Build System

**CMake**:
- **Minimum Version:** 3.20
- **Recommended Version:** 3.27 or later
- **Install:** `brew install cmake` or download from cmake.org

**Build Backend** (choose one):
- **Make:** GNU Make 4.3+ or BSD Make (included with Xcode)
- **Ninja:** 1.10+ (recommended for faster builds)
  - Install: `brew install ninja`

**Git**:
- **Version:** 2.30 or later (included with Xcode)
- **Purpose:** Source code management

---

## 4. Package Manager

### 4.1 Homebrew (Recommended)

**Homebrew** is the de facto standard package manager for macOS.

**Installation:**
```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

**Verify Installation:**
```bash
brew --version  # Should show Homebrew 4.0+
```

**Update Homebrew:**
```bash
brew update
brew upgrade
```

### 4.2 MacPorts (Alternative)

**MacPorts** is an alternative package manager.

**Installation:** https://www.macports.org/install.php

**Note:** Do not mix Homebrew and MacPorts on the same system.

---

## 5. Core Dependencies

### 5.1 Required Libraries

| Library | Min Version | Purpose |
|---------|-------------|---------|
| **spdlog** | 1.10.0+ | Logging framework |
| **GoogleTest** | 1.14.0+ | Unit testing framework |
| **OpenSSL** | 1.1.1+ or 3.0+ | TLS, encryption, hashing |
| **LZ4** | 1.9.3+ | Fast compression |
| **zlib** | 1.2.11+ | General compression (included with macOS) |

### 5.2 macOS-Specific Notes

- **zlib:** Pre-installed with macOS (system library)
- **OpenSSL:** Apple deprecated system OpenSSL; use Homebrew version
- **Threading:** POSIX threads (pthread) included with system

---

## 6. Optional Dependencies

### 6.1 Spatial Support (Optional)

| Library | Min Version | Purpose |
|---------|-------------|---------|
| **GEOS** | 3.10+ | Geometry operations |
| **PROJ** | 9.0+ | Geographic projections |

### 6.2 XML Support (Optional)

| Library | Min Version | Purpose |
|---------|-------------|---------|
| **libxml2** | 2.10+ | XML parsing (included with macOS) |

---

## 7. Installation Instructions

### 7.1 Install Xcode Command Line Tools

```bash
# Install Command Line Tools
xcode-select --install

# Verify installation
xcode-select -p  # Should show: /Library/Developer/CommandLineTools

# Verify compiler
clang --version  # Should show Apple clang 13.0+
```

**OR Install Full Xcode:**
1. Open Mac App Store
2. Search for "Xcode"
3. Click "Get" / "Install"
4. After installation, open Xcode once to accept license

### 7.2 Install Homebrew (if not already installed)

```bash
# Install Homebrew
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# For Apple Silicon, add Homebrew to PATH
echo 'eval "$(/opt/homebrew/bin/brew shellenv)"' >> ~/.zprofile
eval "$(/opt/homebrew/bin/brew shellenv)"

# Verify
brew --version
```

### 7.3 Install Build Tools

```bash
# Install CMake
brew install cmake

# Install Ninja (optional, for faster builds)
brew install ninja

# Verify
cmake --version  # 3.20+
ninja --version  # 1.10+
```

### 7.4 Install Core Dependencies

```bash
# Install core dependencies
brew install spdlog
brew install googletest
brew install openssl@3
brew install lz4

# Install optional dependencies
brew install geos
brew install proj
# libxml2 is pre-installed on macOS
```

---

## 8. Verification Steps

### 8.1 Verify Compiler

```bash
# Apple Clang (default)
clang --version   # Should show Apple clang 13.0+
clang++ --version # Should show Apple clang 13.0+

# Check C++17 support
echo '#if __cplusplus >= 201703L
int main() { return 0; }
#endif' | clang++ -x c++ -std=c++17 - && echo "C++17 supported"
```

### 8.2 Verify CMake

```bash
cmake --version  # Should show 3.20 or later
```

### 8.3 Verify Build System

```bash
make --version    # GNU Make or BSD Make
ninja --version   # 1.10+ (if installed)
```

### 8.4 Verify Dependencies

```bash
# Check installed packages
brew list spdlog
brew list openssl@3
brew list lz4

# Check library paths
brew --prefix spdlog     # /usr/local/opt/spdlog or /opt/homebrew/opt/spdlog
brew --prefix openssl@3  # /usr/local/opt/openssl@3 or /opt/homebrew/opt/openssl@3
```

### 8.5 Verify Architecture

```bash
# Check system architecture
uname -m  # x86_64 (Intel) or arm64 (Apple Silicon)

# Check compiler target
clang -v  # Shows target triple
```

---

## 9. CMake Configuration

### 9.1 Basic Configuration (Intel x86_64)

```bash
# Clone repository
git clone https://github.com/yourusername/ScratchBird.git
cd ScratchBird

# Create build directory
mkdir build
cd build

# Configure with CMake (Unix Makefiles)
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER=clang \
      -DCMAKE_CXX_COMPILER=clang++ \
      -DOPENSSL_ROOT_DIR=$(brew --prefix openssl@3) \
      ..

# Or configure with Ninja (faster)
cmake -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER=clang \
      -DCMAKE_CXX_COMPILER=clang++ \
      -DOPENSSL_ROOT_DIR=$(brew --prefix openssl@3) \
      ..
```

### 9.2 Configuration for Apple Silicon (ARM64)

```bash
# Same as above, but CMake auto-detects ARM64
cmake -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER=clang \
      -DCMAKE_CXX_COMPILER=clang++ \
      -DOPENSSL_ROOT_DIR=/opt/homebrew/opt/openssl@3 \
      ..
```

### 9.3 Universal Binary (x86_64 + ARM64)

```bash
# Build fat binary supporting both architectures
cmake -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
      -DOPENSSL_ROOT_DIR=$(brew --prefix openssl@3) \
      ..
```

### 9.4 Debug Configuration

```bash
cmake -G Ninja \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_C_COMPILER=clang \
      -DCMAKE_CXX_COMPILER=clang++ \
      -DOPENSSL_ROOT_DIR=$(brew --prefix openssl@3) \
      ..
```

### 9.5 Configuration with Optional Features

```bash
cmake -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DOPENSSL_ROOT_DIR=$(brew --prefix openssl@3) \
      -DENABLE_SPATIAL_SUPPORT=ON \
      -DENABLE_XML_SUPPORT=ON \
      -DENABLE_TESTING=ON \
      ..
```

### 9.6 Specify macOS Deployment Target

```bash
# Minimum macOS version for binary compatibility
cmake -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
      -DOPENSSL_ROOT_DIR=$(brew --prefix openssl@3) \
      ..
```

---

## 10. Build Commands

### 10.1 Build with Make

```bash
# From build directory
make -j$(sysctl -n hw.ncpu)  # Parallel build using all CPU cores
```

### 10.2 Build with Ninja

```bash
# From build directory
ninja  # Ninja automatically uses all cores
```

### 10.3 Build Specific Target

```bash
# Build only the core library
ninja scratchbird_core

# Build only tests
ninja tests
```

### 10.4 Clean Build

```bash
# With Make
make clean

# With Ninja
ninja -t clean
```

---

## 11. Testing

### 11.1 Run All Tests

```bash
# From build directory
ctest --output-on-failure
```

### 11.2 Run Tests in Parallel

```bash
ctest -j$(sysctl -n hw.ncpu) --output-on-failure
```

### 11.3 Run Specific Test Suite

```bash
ctest -R unit_tests --output-on-failure
```

---

## 12. Installation

### 12.1 System-Wide Installation (requires sudo)

```bash
# From build directory
sudo ninja install

# Or with Make
sudo make install
```

### 12.2 Custom Installation Prefix

```bash
# Configure with custom prefix
cmake -DCMAKE_INSTALL_PREFIX=/opt/scratchbird ..

# Build and install
ninja
sudo ninja install
```

### 12.3 Local Installation (Homebrew-style)

```bash
# Configure with local prefix
cmake -DCMAKE_INSTALL_PREFIX=$HOME/.local ..

# Build and install
ninja
ninja install  # No sudo needed
```

---

## 13. Code Signing (Required for macOS Gatekeeper)

### 13.1 Ad-hoc Code Signing (Development)

```bash
# Sign binary for local development
codesign -s - ./build/scratchbird

# Verify signature
codesign -v ./build/scratchbird
```

### 13.2 Developer ID Signing (Distribution)

Requires Apple Developer Account ($99/year)

```bash
# Sign with Developer ID
codesign -s "Developer ID Application: Your Name (TEAM_ID)" \
         --timestamp \
         --options runtime \
         ./build/scratchbird

# Verify
codesign -v -v ./build/scratchbird
spctl -a -v ./build/scratchbird
```

### 13.3 Notarization (Required for macOS Catalina+)

```bash
# Create zip for notarization
zip -r scratchbird.zip ./build/scratchbird

# Submit for notarization
xcrun notarytool submit scratchbird.zip \
      --apple-id your@email.com \
      --team-id TEAM_ID \
      --password APP_SPECIFIC_PASSWORD \
      --wait

# Staple notarization ticket
xcrun stapler staple ./build/scratchbird
```

---

## 14. Package Creation

### 14.1 Create DMG Installer

```bash
# Install create-dmg (Homebrew)
brew install create-dmg

# Create DMG
create-dmg \
  --volname "ScratchBird" \
  --volicon "path/to/icon.icns" \
  --window-pos 200 120 \
  --window-size 800 400 \
  --icon-size 100 \
  --icon "ScratchBird.app" 200 190 \
  --hide-extension "ScratchBird.app" \
  --app-drop-link 600 185 \
  "ScratchBird.dmg" \
  "build/Release/"
```

### 14.2 Create Homebrew Formula

```ruby
# scratchbird.rb (example formula)
class Scratchbird < Formula
  desc "Firebird-style MGA database engine"
  homepage "https://github.com/yourusername/ScratchBird"
  url "https://github.com/yourusername/ScratchBird/archive/v1.0.0.tar.gz"
  sha256 "sha256_hash_here"
  license "MIT"

  depends_on "cmake" => :build
  depends_on "ninja" => :build
  depends_on "spdlog"
  depends_on "googletest"
  depends_on "openssl@3"
  depends_on "lz4"

  def install
    system "cmake", "-G", "Ninja", "-B", "build",
                    "-DCMAKE_BUILD_TYPE=Release",
                    "-DOPENSSL_ROOT_DIR=#{Formula["openssl@3"].opt_prefix}",
                    *std_cmake_args
    system "ninja", "-C", "build"
    system "ninja", "-C", "build", "install"
  end

  test do
    system "#{bin}/scratchbird", "--version"
  end
end
```

---

## 15. Troubleshooting

### 15.1 Common Issues

**Issue: Command Line Tools not found**
```bash
# Solution: Install Xcode Command Line Tools
xcode-select --install

# Reset if already installed
sudo rm -rf /Library/Developer/CommandLineTools
xcode-select --install
```

**Issue: Cannot find OpenSSL**
```bash
# Solution: Explicitly specify OpenSSL path
cmake -DOPENSSL_ROOT_DIR=$(brew --prefix openssl@3) ..

# Or create symlink (not recommended)
brew link openssl@3 --force
```

**Issue: Architecture mismatch (Rosetta 2)**
```bash
# Check if running under Rosetta
sysctl sysctl.proc_translated  # 1 = Rosetta, 0 = native

# For Apple Silicon, use native Homebrew
arch -arm64 brew install <package>

# For Intel binaries on Apple Silicon
arch -x86_64 /usr/local/bin/brew install <package>
```

**Issue: Library not loaded (dylib)**
```bash
# Check library dependencies
otool -L ./build/scratchbird

# Fix library paths
install_name_tool -change old_path new_path ./build/scratchbird

# Or set DYLD_LIBRARY_PATH (development only)
export DYLD_LIBRARY_PATH=$(brew --prefix)/lib:$DYLD_LIBRARY_PATH
```

### 15.2 Homebrew on Apple Silicon

```bash
# Apple Silicon Homebrew paths
/opt/homebrew/bin/brew        # Homebrew executable
/opt/homebrew/opt/             # Installed packages

# Intel Homebrew paths (if using Rosetta)
/usr/local/bin/brew           # Homebrew executable
/usr/local/opt/               # Installed packages

# Check which Homebrew
which brew
brew --prefix
```

### 15.3 Clean CMake Cache

```bash
# Delete build directory
rm -rf build
mkdir build
cd build
cmake ..
```

---

## 16. Performance Optimization

### 16.1 Link-Time Optimization (LTO)

```bash
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON \
      ..
```

### 16.2 Architecture-Specific Optimization

```bash
# Intel x86_64
cmake -DCMAKE_CXX_FLAGS="-march=native -mtune=native" ..

# Apple Silicon ARM64
cmake -DCMAKE_CXX_FLAGS="-mcpu=apple-m1" ..
```

---

## 17. Continuous Integration Setup

### 17.1 GitHub Actions Example

```yaml
# .github/workflows/macos-build.yml
jobs:
  build:
    runs-on: macos-13  # or macos-14 for Apple Silicon
    steps:
      - uses: actions/checkout@v4

      - name: Install dependencies
        run: |
          brew install cmake ninja spdlog googletest openssl@3 lz4

      - name: Configure
        run: |
          cmake -G Ninja -B build \
                -DCMAKE_BUILD_TYPE=Release \
                -DOPENSSL_ROOT_DIR=$(brew --prefix openssl@3)

      - name: Build
        run: ninja -C build

      - name: Test
        run: ctest --test-dir build --output-on-failure
```

---

## 18. Version Matrix

Tested and supported combinations:

| macOS Version | Xcode | Arch | CMake | Status |
|---------------|-------|------|-------|--------|
| macOS 14 Sonoma | Xcode 15 | ARM64 | 3.27 | ✅ Tested |
| macOS 14 Sonoma | Xcode 15 | x86_64 | 3.27 | ✅ Tested |
| macOS 13 Ventura | Xcode 14 | ARM64 | 3.25 | ✅ Tested |
| macOS 13 Ventura | Xcode 14 | x86_64 | 3.25 | ✅ Tested |
| macOS 12 Monterey | Xcode 13 | x86_64 | 3.22 | ✅ Tested |
| macOS 11 Big Sur | Xcode 13 | x86_64 | 3.20 | ○ Community |

---

## 19. Additional Resources

- **Xcode Documentation:** https://developer.apple.com/documentation/xcode
- **Homebrew Documentation:** https://docs.brew.sh/
- **CMake macOS Guide:** https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html#cross-compiling-for-ios-tvos-or-watchos
- **Code Signing Guide:** https://developer.apple.com/support/code-signing/

---

**Document Version:** 1.0
**Last Updated:** 2026-01-02
**Maintainer:** Build Infrastructure Team
**Status:** Beta Preparation
