# Complete Build Environment Setup

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Purpose:** Full cross-platform build environment for ScratchBird Beta
**Target:** Linux build machine (Ubuntu/Debian or Fedora/RHEL)
**Document Version:** 1.0
**Last Updated:** 2026-01-03

---

## Overview

This document provides a single comprehensive package list to set up a complete build environment capable of:
- Native Linux builds (x86_64, ARM64)
- Cross-compilation to Windows (MinGW-w64)
- Cross-compilation to macOS (OSXCross - SDK required separately)
- Docker container builds (multi-architecture)
- Package creation (AppImage, DEB, RPM)
- CI/CD integration testing

---

## System Requirements

- **OS:** Ubuntu 22.04+ or Fedora 38+
- **Architecture:** x86_64 (AMD64)
- **RAM:** 32 GB recommended (16 GB minimum)
- **Disk Space:** 100 GB free
- **CPU:** 8+ cores recommended for parallel builds

---

## Ubuntu / Debian Installation

### Complete Installation Script

```bash
#!/bin/bash
set -e

echo "Installing complete ScratchBird build environment for Ubuntu/Debian..."

# Update package lists
sudo apt update

# ============================================================================
# SECTION 1: Core Build Tools
# ============================================================================
sudo apt install -y \
    build-essential \
    autoconf \
    automake \
    libtool \
    pkg-config \
    git \
    wget \
    curl \
    unzip \
    zip \
    tar \
    gzip \
    bzip2 \
    xz-utils \
    patch \
    file

# ============================================================================
# SECTION 2: Compilers and Toolchains
# ============================================================================

# GCC (multiple versions)
sudo apt install -y \
    gcc-11 \
    g++-11 \
    gcc-12 \
    g++-12 \
    gcc-13 \
    g++-13

# Clang/LLVM (multiple versions)
sudo apt install -y \
    clang-14 \
    clang-15 \
    clang-16 \
    clang++-14 \
    clang++-15 \
    clang++-16 \
    llvm-14 \
    llvm-15 \
    llvm-16 \
    lld-14 \
    lld-15 \
    lld-16

# ============================================================================
# SECTION 3: Build Systems
# ============================================================================
sudo apt install -y \
    cmake \
    ninja-build \
    make \
    ccache \
    sccache

# ============================================================================
# SECTION 4: Cross-Compilation - Windows (MinGW-w64)
# ============================================================================
sudo apt install -y \
    mingw-w64 \
    mingw-w64-tools \
    gcc-mingw-w64 \
    g++-mingw-w64 \
    gcc-mingw-w64-x86-64 \
    g++-mingw-w64-x86-64 \
    gcc-mingw-w64-i686 \
    g++-mingw-w64-i686 \
    binutils-mingw-w64 \
    binutils-mingw-w64-x86-64 \
    wine64 \
    wine32

# ============================================================================
# SECTION 5: Cross-Compilation - macOS (OSXCross dependencies)
# ============================================================================
sudo apt install -y \
    clang \
    llvm \
    libssl-dev \
    lzma-dev \
    libxml2-dev \
    uuid-dev \
    libbz2-dev \
    liblzma-dev \
    cpio

# ============================================================================
# SECTION 6: Core Dependencies (Development packages)
# ============================================================================
sudo apt install -y \
    libspdlog-dev \
    libgtest-dev \
    libgmock-dev \
    libssl-dev \
    liblz4-dev \
    zlib1g-dev \
    libc6-dev \
    linux-libc-dev

# ============================================================================
# SECTION 7: Optional Dependencies
# ============================================================================
sudo apt install -y \
    libgeos-dev \
    libproj-dev \
    libxml2-dev

# ============================================================================
# SECTION 8: Packaging Tools - DEB
# ============================================================================
sudo apt install -y \
    debhelper \
    devscripts \
    dh-make \
    fakeroot \
    lintian \
    dpkg-dev \
    pbuilder \
    sbuild \
    ubuntu-dev-tools \
    debian-archive-keyring

# ============================================================================
# SECTION 9: Packaging Tools - RPM
# ============================================================================
sudo apt install -y \
    rpm \
    alien

# Note: For full RPM building, use Fedora/RHEL or mock in pbuilder

# ============================================================================
# SECTION 10: Packaging Tools - AppImage
# ============================================================================
sudo apt install -y \
    fuse \
    libfuse2 \
    libfuse3-3 \
    desktop-file-utils \
    patchelf \
    zsync

# Download AppImage tools
cd /tmp
wget -q https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
wget -q https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage
chmod +x linuxdeploy-x86_64.AppImage appimagetool-x86_64.AppImage
sudo mv linuxdeploy-x86_64.AppImage /usr/local/bin/linuxdeploy
sudo mv appimagetool-x86_64.AppImage /usr/local/bin/appimagetool

# ============================================================================
# SECTION 11: Container Tools - Docker
# ============================================================================

# Install Docker
# Add Docker's official GPG key
sudo install -m 0755 -d /etc/apt/keyrings
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | \
    sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg
sudo chmod a+r /etc/apt/keyrings/docker.gpg

# Add repository
echo \
  "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] https://download.docker.com/linux/ubuntu \
  $(. /etc/os-release && echo "$VERSION_CODENAME") stable" | \
  sudo tee /etc/apt/sources.list.d/docker.list > /dev/null

# Install Docker Engine
sudo apt update
sudo apt install -y \
    docker-ce \
    docker-ce-cli \
    containerd.io \
    docker-buildx-plugin \
    docker-compose-plugin

# Add user to docker group
sudo usermod -aG docker $USER

# ============================================================================
# SECTION 12: Container Tools - Podman
# ============================================================================
sudo apt install -y \
    podman \
    buildah \
    skopeo

# ============================================================================
# SECTION 13: Development Tools
# ============================================================================
sudo apt install -y \
    clang-format-15 \
    clang-tidy-15 \
    cppcheck \
    valgrind \
    gdb \
    lldb-15 \
    strace \
    ltrace

# ============================================================================
# SECTION 14: Profiling and Performance Tools
# ============================================================================
sudo apt install -y \
    perf-tools-unstable \
    linux-tools-generic \
    linux-tools-common \
    gperftools \
    google-perftools

# ============================================================================
# SECTION 15: Documentation Tools
# ============================================================================
sudo apt install -y \
    doxygen \
    graphviz \
    pandoc \
    texlive-latex-base \
    texlive-fonts-recommended

# ============================================================================
# SECTION 16: Compression and Archive Tools
# ============================================================================
sudo apt install -y \
    p7zip-full \
    p7zip-rar \
    rar \
    unrar \
    zip \
    unzip

# ============================================================================
# SECTION 17: Network and Transfer Tools
# ============================================================================
sudo apt install -y \
    rsync \
    sshfs \
    nfs-common \
    cifs-utils

# ============================================================================
# SECTION 18: Version Control
# ============================================================================
sudo apt install -y \
    git \
    git-lfs \
    git-flow \
    subversion \
    mercurial

# ============================================================================
# SECTION 19: CI/CD Tools
# ============================================================================
sudo apt install -y \
    gh \
    jq \
    yq

# Install GitHub CLI if not available
if ! command -v gh &> /dev/null; then
    curl -fsSL https://cli.github.com/packages/githubcli-archive-keyring.gpg | \
        sudo dd of=/usr/share/keyrings/githubcli-archive-keyring.gpg
    echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/githubcli-archive-keyring.gpg] https://cli.github.com/packages stable main" | \
        sudo tee /etc/apt/sources.list.d/github-cli.list > /dev/null
    sudo apt update
    sudo apt install -y gh
fi

# ============================================================================
# SECTION 20: Security and Signing Tools
# ============================================================================
sudo apt install -y \
    gnupg \
    gnupg2 \
    gpgv \
    signing-party \
    ca-certificates

# ============================================================================
# SECTION 21: Python (for build scripts)
# ============================================================================
sudo apt install -y \
    python3 \
    python3-pip \
    python3-venv \
    python3-dev

# Install Python packages
pip3 install --user \
    conan \
    meson \
    gcovr \
    cmake-format

echo ""
echo "============================================================================"
echo "Installation Complete!"
echo "============================================================================"
echo ""
echo "Next steps:"
echo "1. Log out and back in (for docker group membership)"
echo "2. Verify installations with: ./verify-build-environment.sh"
echo "3. For OSXCross: Obtain macOS SDK and build OSXCross"
echo "4. For MXE (MinGW dependencies): Clone and build MXE"
echo ""
echo "Disk space used: $(du -sh /usr | cut -f1)"
echo ""
```

**Save as:** `install-build-environment-ubuntu.sh`

**Run:**
```bash
chmod +x install-build-environment-ubuntu.sh
./install-build-environment-ubuntu.sh
```

---

## Fedora / RHEL / Rocky / AlmaLinux Installation

### Complete Installation Script

```bash
#!/bin/bash
set -e

echo "Installing complete ScratchBird build environment for Fedora/RHEL..."

# ============================================================================
# SECTION 1: Core Build Tools
# ============================================================================
sudo dnf install -y \
    @development-tools \
    autoconf \
    automake \
    libtool \
    pkgconfig \
    git \
    wget \
    curl \
    unzip \
    zip \
    tar \
    gzip \
    bzip2 \
    xz \
    patch \
    file

# ============================================================================
# SECTION 2: Compilers and Toolchains
# ============================================================================

# GCC
sudo dnf install -y \
    gcc \
    gcc-c++ \
    gcc-toolset-12 \
    gcc-toolset-13

# Clang/LLVM
sudo dnf install -y \
    clang \
    clang-tools-extra \
    llvm \
    llvm-devel \
    lld

# ============================================================================
# SECTION 3: Build Systems
# ============================================================================
sudo dnf install -y \
    cmake \
    ninja-build \
    make \
    ccache

# ============================================================================
# SECTION 4: Cross-Compilation - Windows (MinGW-w64)
# ============================================================================
sudo dnf install -y \
    mingw64-gcc \
    mingw64-gcc-c++ \
    mingw64-binutils \
    mingw64-headers \
    mingw64-crt \
    mingw64-winpthreads-static \
    mingw32-gcc \
    mingw32-gcc-c++ \
    wine

# ============================================================================
# SECTION 5: Cross-Compilation - macOS (OSXCross dependencies)
# ============================================================================
sudo dnf install -y \
    clang \
    llvm \
    openssl-devel \
    xz-devel \
    libxml2-devel \
    libuuid-devel \
    bzip2-devel \
    cpio

# ============================================================================
# SECTION 6: Core Dependencies (Development packages)
# ============================================================================
sudo dnf install -y \
    spdlog-devel \
    gtest-devel \
    gmock-devel \
    openssl-devel \
    lz4-devel \
    zlib-devel \
    glibc-devel \
    kernel-headers

# ============================================================================
# SECTION 7: Optional Dependencies
# ============================================================================
sudo dnf install -y \
    geos-devel \
    proj-devel \
    libxml2-devel

# ============================================================================
# SECTION 8: Packaging Tools - RPM
# ============================================================================
sudo dnf install -y \
    rpm-build \
    rpmdevtools \
    rpmlint \
    mock \
    createrepo_c \
    rpm-sign

# Setup RPM build tree
rpmdev-setuptree

# Add user to mock group
sudo usermod -a -G mock $USER

# ============================================================================
# SECTION 9: Packaging Tools - DEB
# ============================================================================
sudo dnf install -y \
    dpkg \
    dpkg-dev

# Note: For full DEB building, use Ubuntu/Debian

# ============================================================================
# SECTION 10: Packaging Tools - AppImage
# ============================================================================
sudo dnf install -y \
    fuse \
    fuse-libs \
    desktop-file-utils \
    patchelf

# Download AppImage tools
cd /tmp
wget -q https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
wget -q https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage
chmod +x linuxdeploy-x86_64.AppImage appimagetool-x86_64.AppImage
sudo mv linuxdeploy-x86_64.AppImage /usr/local/bin/linuxdeploy
sudo mv appimagetool-x86_64.AppImage /usr/local/bin/appimagetool

# ============================================================================
# SECTION 11: Container Tools - Docker
# ============================================================================

# Install Docker
sudo dnf -y install dnf-plugins-core
sudo dnf config-manager --add-repo https://download.docker.com/linux/fedora/docker-ce.repo

sudo dnf install -y \
    docker-ce \
    docker-ce-cli \
    containerd.io \
    docker-buildx-plugin \
    docker-compose-plugin

# Start Docker
sudo systemctl start docker
sudo systemctl enable docker

# Add user to docker group
sudo usermod -aG docker $USER

# ============================================================================
# SECTION 12: Container Tools - Podman
# ============================================================================
sudo dnf install -y \
    podman \
    buildah \
    skopeo

# ============================================================================
# SECTION 13: Development Tools
# ============================================================================
sudo dnf install -y \
    clang-tools-extra \
    cppcheck \
    valgrind \
    gdb \
    lldb \
    strace \
    ltrace

# ============================================================================
# SECTION 14: Profiling and Performance Tools
# ============================================================================
sudo dnf install -y \
    perf \
    gperftools \
    gperftools-devel

# ============================================================================
# SECTION 15: Documentation Tools
# ============================================================================
sudo dnf install -y \
    doxygen \
    graphviz \
    pandoc

# ============================================================================
# SECTION 16: Compression and Archive Tools
# ============================================================================
sudo dnf install -y \
    p7zip \
    p7zip-plugins \
    zip \
    unzip

# ============================================================================
# SECTION 17: Network and Transfer Tools
# ============================================================================
sudo dnf install -y \
    rsync \
    sshfs \
    nfs-utils \
    cifs-utils

# ============================================================================
# SECTION 18: Version Control
# ============================================================================
sudo dnf install -y \
    git \
    git-lfs \
    subversion \
    mercurial

# ============================================================================
# SECTION 19: CI/CD Tools
# ============================================================================
sudo dnf install -y \
    jq

# Install GitHub CLI
sudo dnf config-manager --add-repo https://cli.github.com/packages/rpm/gh-cli.repo
sudo dnf install -y gh

# ============================================================================
# SECTION 20: Security and Signing Tools
# ============================================================================
sudo dnf install -y \
    gnupg2 \
    ca-certificates

# ============================================================================
# SECTION 21: Python (for build scripts)
# ============================================================================
sudo dnf install -y \
    python3 \
    python3-pip \
    python3-devel

# Install Python packages
pip3 install --user \
    conan \
    meson \
    gcovr \
    cmake-format

echo ""
echo "============================================================================"
echo "Installation Complete!"
echo "============================================================================"
echo ""
echo "Next steps:"
echo "1. Log out and back in (for docker/mock group membership)"
echo "2. Verify installations with: ./verify-build-environment.sh"
echo "3. For OSXCross: Obtain macOS SDK and build OSXCross"
echo "4. For MXE (MinGW dependencies): Clone and build MXE"
echo ""
echo "Disk space used: $(du -sh /usr | cut -f1)"
echo ""
```

**Save as:** `install-build-environment-fedora.sh`

**Run:**
```bash
chmod +x install-build-environment-fedora.sh
./install-build-environment-fedora.sh
```

---

## Post-Installation Setup

### 1. Build OSXCross (for macOS cross-compilation)

```bash
# Clone OSXCross
git clone https://github.com/tpoechtrager/osxcross.git /tmp/osxcross
cd /tmp/osxcross

# IMPORTANT: Obtain macOS SDK (requires Apple Developer account)
# Download Xcode.xip or package SDK from macOS
# Place in: /tmp/osxcross/tarballs/MacOSX14.0.sdk.tar.gz

# Build OSXCross
UNATTENDED=1 TARGET_DIR=/opt/osxcross ./build.sh

# Add to PATH
echo 'export PATH="/opt/osxcross/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

### 2. Build MXE (for Windows dependency management)

```bash
# Clone MXE
git clone https://github.com/mxe/mxe.git /opt/mxe
cd /opt/mxe

# Build required packages (this takes 1-2 hours)
make MXE_TARGETS='x86_64-w64-mingw32.static' \
    spdlog gtest openssl lz4 zlib -j$(nproc)

# Add to PATH
echo 'export PATH="/opt/mxe/usr/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

### 3. Setup vcpkg (Alternative Windows dependency manager)

```bash
# Clone vcpkg
git clone https://github.com/Microsoft/vcpkg.git /opt/vcpkg
cd /opt/vcpkg
./bootstrap-vcpkg.sh

# Install packages for Windows
./vcpkg install spdlog:x64-mingw-static \
                gtest:x64-mingw-static \
                openssl:x64-mingw-static \
                lz4:x64-mingw-static \
                zlib:x64-mingw-static

# Add to PATH
echo 'export PATH="/opt/vcpkg:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

### 4. Configure GPG for Package Signing

```bash
# Generate GPG key
gpg --full-generate-key

# Export public key
gpg --export -a 'Your Name' > ~/RPM-GPG-KEY-scratchbird

# Configure RPM signing
echo "%_gpg_name Your Name" >> ~/.rpmmacros
```

### 5. Logout and Login

```bash
# Required for group membership (docker, mock)
# Log out and log back in
```

---

## Verification Script

**verify-build-environment.sh**:
```bash
#!/bin/bash

echo "Verifying ScratchBird build environment..."
echo ""

# Function to check command
check_cmd() {
    if command -v $1 &> /dev/null; then
        echo "✓ $1: $($1 --version 2>&1 | head -n1)"
    else
        echo "✗ $1: NOT FOUND"
    fi
}

# Function to check file
check_file() {
    if [ -f "$1" ]; then
        echo "✓ $1: EXISTS"
    else
        echo "✗ $1: NOT FOUND"
    fi
}

echo "=== Compilers ==="
check_cmd gcc
check_cmd g++
check_cmd clang
check_cmd clang++

echo ""
echo "=== Build Systems ==="
check_cmd cmake
check_cmd ninja
check_cmd make

echo ""
echo "=== Cross-Compilation ==="
check_cmd x86_64-w64-mingw32-gcc
check_cmd x86_64-apple-darwin23-clang || echo "ℹ OSXCross not installed"

echo ""
echo "=== Containers ==="
check_cmd docker
check_cmd podman

echo ""
echo "=== Packaging ==="
check_cmd debuild || echo "ℹ Debian tools not on Fedora"
check_cmd rpmbuild || echo "ℹ RPM tools not on Debian"
check_cmd linuxdeploy
check_cmd appimagetool

echo ""
echo "=== Development Tools ==="
check_cmd clang-format
check_cmd clang-tidy
check_cmd valgrind
check_cmd gdb

echo ""
echo "=== Version Control ==="
check_cmd git
check_cmd gh

echo ""
echo "=== Groups ==="
groups | grep -q docker && echo "✓ docker group" || echo "✗ docker group (logout/login required)"
groups | grep -q mock && echo "✓ mock group" || echo "ℹ mock group not on Debian"

echo ""
echo "=== Disk Space ==="
df -h / | tail -1

echo ""
echo "Verification complete!"
```

**Run verification**:
```bash
chmod +x verify-build-environment.sh
./verify-build-environment.sh
```

---

## Estimated Disk Space Requirements

| Component | Size |
|-----------|------|
| Base tools and compilers | ~5 GB |
| Docker images | ~10 GB |
| MXE (Windows deps) | ~15 GB |
| OSXCross (macOS SDK) | ~5 GB |
| Build artifacts (scratch) | ~20 GB |
| vcpkg cache | ~5 GB |
| **Total** | **~60 GB** |

**Recommended:** 100 GB free space for comfortable development

---

## Quick Reference: Package Counts

### Ubuntu/Debian
- **Total packages:** ~220
- **Installation time:** 20-30 minutes
- **Download size:** ~3 GB

### Fedora/RHEL
- **Total packages:** ~180
- **Installation time:** 15-25 minutes
- **Download size:** ~2.5 GB

---

## Troubleshooting

### Out of disk space

```bash
# Clean package cache
sudo apt clean          # Ubuntu/Debian
sudo dnf clean all      # Fedora/RHEL

# Remove old kernels
sudo apt autoremove     # Ubuntu/Debian
```

### Docker permission denied

```bash
# Ensure you're in docker group
sudo usermod -aG docker $USER

# Log out and back in
# Verify:
groups | grep docker
```

### MinGW not found

```bash
# Verify installation
dpkg -l | grep mingw    # Ubuntu/Debian
rpm -qa | grep mingw    # Fedora/RHEL

# Test compiler
x86_64-w64-mingw32-gcc --version
```

---

**Document Version:** 1.0
**Last Updated:** 2026-01-03
**Status:** Beta Preparation
