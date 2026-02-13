# Linux to macOS Cross-Compilation Requirements

**Platform:** Linux
**Target:** macOS (x86_64, ARM64)
**Document Version:** 1.0
**Last Updated:** 2026-01-03

---

## 1. Overview

This document specifies all requirements for cross-compiling ScratchBird for macOS targets from a Linux host system using OSXCross toolchain.

**IMPORTANT LEGAL NOTICE**: Cross-compiling for macOS from Linux requires access to macOS SDK, which is subject to Apple's licensing terms. You must have a valid Apple Developer account and accept Apple's terms of service. This documentation is provided for educational and authorized development purposes only.

---

## 2. System Requirements

### 2.1 Minimum System Requirements

| Component | Requirement |
|-----------|-------------|
| **Host OS** | Linux kernel 5.10+ |
| **Architecture** | x86_64 (AMD64) |
| **RAM** | 16 GB (32 GB recommended) |
| **Disk Space** | 25 GB free (SDK + toolchain + dependencies + builds) |
| **CPU** | 8 cores minimum (cross-compilation is CPU-intensive) |

### 2.2 Supported Host Distributions

**Tier 1 (Fully Supported)**:
- Ubuntu 22.04 LTS or later
- Debian 12 (Bookworm) or later
- Fedora 38 or later

**Tier 2 (Community Supported)**:
- Arch Linux (rolling release)
- openSUSE Tumbleweed

---

## 3. Legal Requirements

### 3.1 Apple Developer Account

- **Required**: Valid Apple Developer account
- **Purpose**: Access to Xcode and macOS SDK
- **Cost**: Free account sufficient for SDK access
- **Registration**: https://developer.apple.com/

### 3.2 macOS SDK Licensing

**Important**: The macOS SDK is proprietary software owned by Apple:
- You must agree to Apple's Software License Agreement
- SDK redistribution is prohibited
- SDK can only be obtained from official Apple sources (Xcode)
- Use is restricted to developing software for Apple platforms

**This documentation assumes**:
- You have legal access to macOS SDK
- You are developing software intended for macOS distribution
- You comply with all applicable Apple licensing terms

---

## 4. OSXCross Toolchain

### 4.1 What is OSXCross?

**OSXCross** is a cross-compilation toolchain for macOS on Linux:
- Wraps Clang/LLVM to target macOS
- Provides macOS system libraries and headers
- Supports both x86_64 and ARM64 (Apple Silicon) targets
- Repository: https://github.com/tpoechtrager/osxcross

### 4.2 Supported macOS SDK Versions

| SDK Version | macOS Version | Architectures | Status |
|-------------|---------------|---------------|--------|
| 14.x | macOS 14 Sonoma | x86_64, ARM64 | ✅ Recommended |
| 13.x | macOS 13 Ventura | x86_64, ARM64 | ✅ Supported |
| 12.x | macOS 12 Monterey | x86_64, ARM64 | ✅ Supported |
| 11.x | macOS 11 Big Sur | x86_64, ARM64 | ○ Legacy |

---

## 5. Host Dependencies

### 5.1 Ubuntu / Debian (apt)

```bash
# Install build tools
sudo apt update
sudo apt install -y \
    clang \
    llvm \
    cmake \
    ninja-build \
    git \
    patch \
    python3 \
    libssl-dev \
    lzma-dev \
    libxml2-dev \
    xz-utils \
    bzip2 \
    cpio \
    zlib1g-dev

# Install additional tools
sudo apt install -y \
    libbz2-dev \
    liblzma-dev \
    libz-dev \
    uuid-dev
```

### 5.2 Fedora / RHEL / Rocky (dnf)

```bash
# Install build tools
sudo dnf install -y \
    clang \
    llvm \
    cmake \
    ninja-build \
    git \
    patch \
    python3 \
    openssl-devel \
    xz-devel \
    libxml2-devel \
    zlib-devel \
    bzip2-devel

# Install additional tools
sudo dnf install -y \
    uuid-devel \
    cpio
```

### 5.3 Arch Linux (pacman)

```bash
# Install build tools
sudo pacman -S --needed \
    clang \
    llvm \
    cmake \
    ninja \
    git \
    patch \
    python \
    openssl \
    xz \
    libxml2 \
    zlib \
    bzip2 \
    cpio \
    util-linux
```

---

## 6. Obtaining macOS SDK

### 6.1 Method 1: Download Xcode (Requires macOS or macOS VM)

**On a macOS machine**:
```bash
# Download Xcode from App Store or developer.apple.com
# Extract SDK from Xcode
xcode-select --install

# Locate SDK
ls /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/

# Package SDK for transfer
cd /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/
tar czf ~/MacOSX14.0.sdk.tar.gz MacOSX14.0.sdk/

# Transfer MacOSX14.0.sdk.tar.gz to Linux machine
```

### 6.2 Method 2: Extract from Xcode.xip

**On Linux (if you have Xcode.xip)**:
```bash
# Install tools to extract XIP
git clone https://github.com/tpoechtrager/osxcross
cd osxcross

# Download Xcode.xip from https://developer.apple.com/download/
# (Requires Apple Developer account login)

# Place Xcode.xip in osxcross/tarballs/
mv ~/Downloads/Xcode_14.0.xip tarballs/

# OSXCross will extract SDK automatically during build
```

### 6.3 Method 3: Use Packaged SDK (Community)

**WARNING**: Only use if you have legal right to use the SDK.

Some community members package SDKs, but redistribution violates Apple's license. Only use if you independently have rights to the SDK.

---

## 7. Building OSXCross

### 7.1 Clone OSXCross Repository

```bash
# Clone OSXCross
git clone https://github.com/tpoechtrager/osxcross.git
cd osxcross
```

### 7.2 Install SDK

```bash
# Copy SDK tarball to tarballs directory
# Format: MacOSX{version}.sdk.tar.* or Xcode.xip
cp ~/MacOSX14.0.sdk.tar.gz tarballs/

# Or if you have Xcode.xip
cp ~/Xcode_14.0.xip tarballs/
```

### 7.3 Build Toolchain

```bash
# Set target architectures
# For both x86_64 and ARM64:
UNATTENDED=1 TARGET_DIR=/opt/osxcross ./build.sh

# For x86_64 only:
UNATTENDED=1 TARGET_DIR=/opt/osxcross ARCH=x86_64 ./build.sh

# For ARM64 only:
UNATTENDED=1 TARGET_DIR=/opt/osxcross ARCH=arm64 ./build.sh
```

**Build time**: 10-30 minutes depending on CPU

### 7.4 Install Toolchain

```bash
# Move to installation directory
sudo mkdir -p /opt/osxcross
sudo mv target/* /opt/osxcross/

# Add to PATH
echo 'export PATH="/opt/osxcross/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

---

## 8. Verify OSXCross Installation

### 8.1 Verify Compilers

```bash
# x86_64 compilers
x86_64-apple-darwin23-clang --version
x86_64-apple-darwin23-clang++ --version

# ARM64 compilers (if built)
arm64-apple-darwin23-clang --version
arm64-apple-darwin23-clang++ --version

# Universal (both architectures)
o64-clang --version  # Compiles for x86_64
oa64-clang --version # Compiles for ARM64
```

### 8.2 Test Compilation

```bash
# Simple test program
cat > test.cpp << 'EOF'
#include <iostream>
int main() {
    std::cout << "Hello from macOS!" << std::endl;
    return 0;
}
EOF

# Compile for x86_64
x86_64-apple-darwin23-clang++ -o test_x64 test.cpp

# Compile for ARM64
arm64-apple-darwin23-clang++ -o test_arm64 test.cpp

# Check architecture
file test_x64
file test_arm64
```

---

## 9. Cross-Compiling Dependencies

### 9.1 Dependency Strategy

**Option 1**: Use osxcross-macports (Recommended)
```bash
# Install osxcross-macports wrapper
cd osxcross
./build_macports.sh

# This provides many pre-built packages
```

**Option 2**: Manual cross-compilation of each dependency

**Option 3**: Use pre-built universal binaries from Homebrew bottles

### 9.2 Manual Cross-Compilation Example (zlib)

```bash
export OSXCROSS_TARGET=darwin23
export OSXCROSS_SDK=/opt/osxcross/SDK/MacOSX14.0.sdk

# Clone zlib
git clone https://github.com/madler/zlib.git
cd zlib

# Configure for x86_64
CC=x86_64-apple-darwin23-clang \
AR=x86_64-apple-darwin23-ar \
RANLIB=x86_64-apple-darwin23-ranlib \
./configure --prefix=/opt/osxcross/x86_64

make -j$(nproc)
sudo make install

# Clean and configure for ARM64
make clean
CC=arm64-apple-darwin23-clang \
AR=arm64-apple-darwin23-ar \
RANLIB=arm64-apple-darwin23-ranlib \
./configure --prefix=/opt/osxcross/arm64

make -j$(nproc)
sudo make install
```

### 9.3 Create Universal Binary

```bash
# Combine x86_64 and ARM64 libraries into universal binary
lipo -create \
    /opt/osxcross/x86_64/lib/libz.a \
    /opt/osxcross/arm64/lib/libz.a \
    -output /opt/osxcross/universal/lib/libz.a
```

---

## 10. CMake Toolchain Files

### 10.1 Toolchain for x86_64

Create `toolchain-macos-x64.cmake`:

```cmake
# toolchain-macos-x64.cmake
set(CMAKE_SYSTEM_NAME Darwin)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# OSXCross paths
set(OSXCROSS_TARGET "darwin23")
set(OSXCROSS_SDK "/opt/osxcross/SDK/MacOSX14.0.sdk")

# Compilers
set(CMAKE_C_COMPILER x86_64-apple-darwin23-clang)
set(CMAKE_CXX_COMPILER x86_64-apple-darwin23-clang++)
set(CMAKE_AR x86_64-apple-darwin23-ar)
set(CMAKE_RANLIB x86_64-apple-darwin23-ranlib)

# Target environment
set(CMAKE_FIND_ROOT_PATH /opt/osxcross/x86_64)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# macOS deployment target
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0")
set(CMAKE_OSX_SYSROOT ${OSXCROSS_SDK})
```

### 10.2 Toolchain for ARM64 (Apple Silicon)

Create `toolchain-macos-arm64.cmake`:

```cmake
# toolchain-macos-arm64.cmake
set(CMAKE_SYSTEM_NAME Darwin)
set(CMAKE_SYSTEM_PROCESSOR arm64)

# OSXCross paths
set(OSXCROSS_TARGET "darwin23")
set(OSXCROSS_SDK "/opt/osxcross/SDK/MacOSX14.0.sdk")

# Compilers
set(CMAKE_C_COMPILER arm64-apple-darwin23-clang)
set(CMAKE_CXX_COMPILER arm64-apple-darwin23-clang++)
set(CMAKE_AR arm64-apple-darwin23-ar)
set(CMAKE_RANLIB arm64-apple-darwin23-ranlib)

# Target environment
set(CMAKE_FIND_ROOT_PATH /opt/osxcross/arm64)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# macOS deployment target
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0")
set(CMAKE_OSX_SYSROOT ${OSXCROSS_SDK})
```

### 10.3 Toolchain for Universal Binary

Create `toolchain-macos-universal.cmake`:

```cmake
# toolchain-macos-universal.cmake
set(CMAKE_SYSTEM_NAME Darwin)

# OSXCross paths
set(OSXCROSS_SDK "/opt/osxcross/SDK/MacOSX14.0.sdk")

# Universal compiler wrapper
set(CMAKE_C_COMPILER o64-clang)
set(CMAKE_CXX_COMPILER o64-clang++)

# Build for both architectures
set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0")
set(CMAKE_OSX_SYSROOT ${OSXCROSS_SDK})

# Target environment
set(CMAKE_FIND_ROOT_PATH /opt/osxcross/universal)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
```

---

## 11. CMake Configuration

### 11.1 Configure for x86_64

```bash
# Clone repository
git clone https://github.com/yourusername/ScratchBird.git
cd ScratchBird

# Create build directory
mkdir build-macos-x64
cd build-macos-x64

# Configure
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain-macos-x64.cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -G Ninja \
      ..
```

### 11.2 Configure for ARM64

```bash
mkdir build-macos-arm64
cd build-macos-arm64

cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain-macos-arm64.cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -G Ninja \
      ..
```

### 11.3 Configure for Universal Binary

```bash
mkdir build-macos-universal
cd build-macos-universal

cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain-macos-universal.cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -G Ninja \
      ..
```

---

## 12. Build Commands

### 12.1 Build x86_64

```bash
cd build-macos-x64
ninja
```

### 12.2 Build ARM64

```bash
cd build-macos-arm64
ninja
```

### 12.3 Build Universal Binary

```bash
cd build-macos-universal
ninja
```

### 12.4 Manually Create Universal Binary

If building separately:

```bash
# Use lipo to create universal binary
lipo -create \
    build-macos-x64/scratchbird \
    build-macos-arm64/scratchbird \
    -output scratchbird-universal

# Verify architectures
lipo -info scratchbird-universal
# Should show: Architectures in the fat file: x86_64 arm64
```

---

## 13. Code Signing (Optional)

### 13.1 Self-Signed Certificate

**Note**: Cross-compiled binaries cannot be notarized by Apple without a Mac. However, you can prepare for signing:

```bash
# Create ad-hoc signature (for testing)
osxcross-codesign scratchbird

# Or use Apple's codesign on macOS later
# codesign --force --sign "Developer ID Application: Your Name" scratchbird
```

---

## 14. Packaging

### 14.1 Create macOS Application Bundle

```bash
# Create app bundle structure
mkdir -p ScratchBird.app/Contents/MacOS
mkdir -p ScratchBird.app/Contents/Resources

# Copy binary
cp scratchbird-universal ScratchBird.app/Contents/MacOS/ScratchBird

# Create Info.plist
cat > ScratchBird.app/Contents/Info.plist << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>ScratchBird</string>
    <key>CFBundleIdentifier</key>
    <string>com.scratchbird.app</string>
    <key>CFBundleName</key>
    <string>ScratchBird</string>
    <key>CFBundleVersion</key>
    <string>0.1.0</string>
    <key>CFBundleShortVersionString</key>
    <string>0.1.0</string>
    <key>LSMinimumSystemVersion</key>
    <string>11.0</string>
    <key>NSHighResolutionCapable</key>
    <true/>
</dict>
</plist>
EOF

# Make executable
chmod +x ScratchBird.app/Contents/MacOS/ScratchBird
```

### 14.2 Create DMG

**Install DMG creation tools**:
```bash
sudo apt install genisoimage  # Ubuntu/Debian
sudo dnf install genisoimage  # Fedora/RHEL
```

**Create DMG**:
```bash
# Create directory for DMG contents
mkdir dmg-contents
cp -r ScratchBird.app dmg-contents/

# Create DMG
genisoimage -V "ScratchBird" \
            -D -R -apple -no-pad \
            -o ScratchBird.dmg \
            dmg-contents/
```

**Alternative: Use dmgbuild on macOS**
```bash
# On macOS (after transferring .app)
pip3 install dmgbuild
dmgbuild -s settings.py "ScratchBird" ScratchBird.dmg
```

---

## 15. Testing

### 15.1 Testing Without macOS

**Limitation**: Cannot natively run macOS binaries on Linux without macOS VM.

```bash
# Verify binary architecture
file scratchbird-universal
# Should show: Mach-O universal binary with 2 architectures

# Use lipo to verify
lipo -info scratchbird-universal
# Should show: x86_64 arm64
```

### 15.2 Testing with macOS VM

**Option 1: QEMU + macOS**
```bash
# Install QEMU
sudo apt install qemu-system-x86

# Use OSX-KVM project
git clone https://github.com/kholia/OSX-KVM.git
cd OSX-KVM
# Follow instructions to create macOS VM
```

**Option 2: Transfer to Physical macOS Machine**
```bash
# Copy binary to macOS
scp scratchbird-universal user@macos-machine:~/

# On macOS, test
./scratchbird-universal --version
```

---

## 16. Troubleshooting

### 16.1 Common Issues

**Issue: Cannot find macOS SDK**
```bash
# Solution: Verify SDK path
ls /opt/osxcross/SDK/

# Re-extract if needed
cd osxcross
rm -rf target
./build.sh
```

**Issue: Compiler not found**
```bash
# Solution: Add OSXCross to PATH
export PATH="/opt/osxcross/bin:$PATH"

# Verify
which x86_64-apple-darwin23-clang
```

**Issue: Missing system libraries**
```bash
# Solution: Use osxcross-macports
cd osxcross
./build_macports.sh
osxcross-macports install <package>
```

**Issue: Linker errors**
```bash
# Solution: Specify correct SDK and deployment target
cmake -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
      -DCMAKE_OSX_SYSROOT=/opt/osxcross/SDK/MacOSX14.0.sdk \
      ..
```

### 16.2 SDK Version Mismatch

```bash
# Check SDK version
ls /opt/osxcross/SDK/

# Update toolchain file to match
# Change darwin23 to match your SDK (darwin21, darwin22, etc.)
```

---

## 17. Continuous Integration

### 17.1 GitHub Actions Example

```yaml
# .github/workflows/cross-compile-macos.yml
jobs:
  cross-compile-macos:
    runs-on: ubuntu-22.04
    steps:
      - uses: actions/checkout@v4

      - name: Install dependencies
        run: |
          sudo apt update
          sudo apt install -y clang llvm cmake ninja-build \
            libssl-dev lzma-dev libxml2-dev

      - name: Cache OSXCross
        uses: actions/cache@v3
        id: osxcross-cache
        with:
          path: /opt/osxcross
          key: osxcross-${{ runner.os }}-14.0

      - name: Build OSXCross
        if: steps.osxcross-cache.outputs.cache-hit != 'true'
        run: |
          git clone https://github.com/tpoechtrager/osxcross
          cd osxcross
          # NOTE: You must provide SDK separately (cannot redistribute)
          # wget <secure-location>/MacOSX14.0.sdk.tar.gz -O tarballs/MacOSX14.0.sdk.tar.gz
          UNATTENDED=1 TARGET_DIR=/tmp/osxcross ./build.sh
          sudo mv /tmp/osxcross/* /opt/osxcross/

      - name: Configure
        run: |
          export PATH="/opt/osxcross/bin:$PATH"
          cmake -DCMAKE_TOOLCHAIN_FILE=toolchain-macos-universal.cmake \
                -DCMAKE_BUILD_TYPE=Release \
                -G Ninja \
                -B build-macos

      - name: Build
        run: |
          export PATH="/opt/osxcross/bin:$PATH"
          ninja -C build-macos

      - name: Create DMG
        run: |
          mkdir -p ScratchBird.app/Contents/MacOS
          cp build-macos/scratchbird ScratchBird.app/Contents/MacOS/
          # Create Info.plist...
          genisoimage -V "ScratchBird" -D -R -apple -no-pad \
            -o ScratchBird.dmg ScratchBird.app

      - name: Upload artifacts
        uses: actions/upload-artifact@v4
        with:
          name: macos-universal
          path: ScratchBird.dmg
```

---

## 18. Limitations

### 18.1 Known Limitations

- **Cannot notarize**: Apple notarization requires Xcode on macOS
- **No App Store distribution**: App Store submissions require macOS and Xcode
- **Limited testing**: Cannot run binaries natively on Linux
- **SDK licensing**: SDK cannot be freely redistributed in CI/CD

### 18.2 Workarounds

- **Notarization**: Use macOS machine for final notarization step
- **Testing**: Use macOS VM or transfer to physical Mac
- **CI/CD**: Cache SDK in private storage (not in public repositories)

---

## 19. Version Matrix

| Host Distribution | Clang Version | SDK Version | Target macOS | Status |
|-------------------|---------------|-------------|--------------|--------|
| Ubuntu 22.04 | 14.0 | 14.0 | 11.0+ | ✅ Tested |
| Debian 12 | 15.0 | 13.0 | 11.0+ | ✅ Tested |
| Fedora 38 | 16.0 | 14.0 | 11.0+ | ✅ Tested |
| Arch Linux | 17.0 | 14.0 | 11.0+ | ○ Community |

---

## 20. Additional Resources

- **OSXCross Repository:** https://github.com/tpoechtrager/osxcross
- **Apple Developer:** https://developer.apple.com/
- **Xcode Downloads:** https://developer.apple.com/download/
- **macOS SDK Documentation:** https://developer.apple.com/documentation/

---

## 21. Legal Disclaimer

This documentation is provided for educational purposes. Users are responsible for:
- Obtaining legal access to macOS SDK
- Complying with Apple's software license agreements
- Ensuring their use case is permitted under Apple's terms
- Not redistributing Apple's proprietary software

The ScratchBird project does not provide, host, or distribute macOS SDK files.

---

**Document Version:** 1.0
**Last Updated:** 2026-01-03
**Maintainer:** Build Infrastructure Team
**Status:** Beta Preparation
