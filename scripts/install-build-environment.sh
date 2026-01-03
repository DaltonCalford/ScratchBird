#!/bin/bash
# ScratchBird Complete Build Environment Installation
# For Ubuntu/Debian-based systems
# Version: 1.0

set -e

echo "========================================================================"
echo "ScratchBird Beta - Complete Build Environment Installation"
echo "========================================================================"
echo ""
echo "This will install packages for:"
echo "  - Native Linux builds (x86_64, ARM64)"
echo "  - Cross-compilation to Windows (MinGW-w64)"
echo "  - Cross-compilation to macOS (OSXCross - SDK separate)"
echo "  - Docker multi-architecture builds"
echo "  - Package creation (AppImage, DEB, RPM)"
echo "  - Development and debugging tools"
echo ""
echo "Estimated download: ~3 GB"
echo "Estimated disk space: ~15 GB"
echo "Installation time: ~20-30 minutes"
echo ""
read -p "Continue? (y/n) " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Installation cancelled."
    exit 1
fi

echo ""
echo "Starting installation..."
echo ""

# Update package lists
echo ">>> Updating package lists..."
sudo apt update

# SECTION 1: Core Build Tools
echo ">>> Installing core build tools..."
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

# SECTION 2: Compilers and Toolchains
echo ">>> Installing compilers (GCC, Clang)..."
sudo apt install -y \
    gcc-11 g++-11 \
    gcc-12 g++-12 \
    clang-14 clang++-14 \
    clang-15 clang++-15 \
    llvm-14 llvm-15 \
    lld-14 lld-15

# SECTION 3: Build Systems
echo ">>> Installing build systems (CMake, Ninja)..."
sudo apt install -y \
    cmake \
    ninja-build \
    make \
    ccache

# SECTION 4: Cross-Compilation - Windows
echo ">>> Installing MinGW-w64 for Windows cross-compilation..."
sudo apt install -y \
    mingw-w64 \
    mingw-w64-tools \
    gcc-mingw-w64-x86-64 \
    g++-mingw-w64-x86-64 \
    binutils-mingw-w64-x86-64 \
    wine64

# SECTION 5: Cross-Compilation - macOS (dependencies)
echo ">>> Installing OSXCross dependencies..."
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

# SECTION 6: Core Dependencies
echo ">>> Installing core dependencies..."
sudo apt install -y \
    libspdlog-dev \
    libgtest-dev \
    libgmock-dev \
    libssl-dev \
    liblz4-dev \
    zlib1g-dev \
    libc6-dev

# SECTION 7: Optional Dependencies
echo ">>> Installing optional dependencies..."
sudo apt install -y \
    libgeos-dev \
    libproj-dev \
    libxml2-dev

# SECTION 8: Packaging Tools - DEB
echo ">>> Installing DEB packaging tools..."
sudo apt install -y \
    debhelper \
    devscripts \
    dh-make \
    fakeroot \
    lintian \
    dpkg-dev \
    pbuilder

# SECTION 9: Packaging Tools - RPM
echo ">>> Installing RPM tools..."
sudo apt install -y \
    rpm \
    alien

# SECTION 10: Packaging Tools - AppImage
echo ">>> Installing AppImage tools..."
sudo apt install -y \
    fuse \
    libfuse2 \
    desktop-file-utils \
    patchelf \
    zsync

echo ">>> Downloading linuxdeploy and appimagetool..."
cd /tmp
if [ ! -f /usr/local/bin/linuxdeploy ]; then
    wget -q https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
    chmod +x linuxdeploy-x86_64.AppImage
    sudo mv linuxdeploy-x86_64.AppImage /usr/local/bin/linuxdeploy
fi

if [ ! -f /usr/local/bin/appimagetool ]; then
    wget -q https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage
    chmod +x appimagetool-x86_64.AppImage
    sudo mv appimagetool-x86_64.AppImage /usr/local/bin/appimagetool
fi

# SECTION 11: Docker
echo ">>> Installing Docker..."
if ! command -v docker &> /dev/null; then
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

    # Install
    sudo apt update
    sudo apt install -y \
        docker-ce \
        docker-ce-cli \
        containerd.io \
        docker-buildx-plugin \
        docker-compose-plugin

    # Add user to docker group
    sudo usermod -aG docker $USER
    echo "ℹ  Added $USER to docker group - logout/login required"
else
    echo "✓ Docker already installed"
fi

# SECTION 12: Podman
echo ">>> Installing Podman..."
sudo apt install -y \
    podman \
    buildah \
    skopeo

# SECTION 13: Development Tools
echo ">>> Installing development tools..."
sudo apt install -y \
    clang-format-15 \
    clang-tidy-15 \
    cppcheck \
    valgrind \
    gdb \
    lldb-15 \
    strace

# SECTION 14: Profiling Tools
echo ">>> Installing profiling tools..."
sudo apt install -y \
    linux-tools-generic \
    linux-tools-common

# SECTION 15: Documentation Tools
echo ">>> Installing documentation tools..."
sudo apt install -y \
    doxygen \
    graphviz \
    pandoc

# SECTION 16: Compression Tools
echo ">>> Installing compression tools..."
sudo apt install -y \
    p7zip-full \
    zip \
    unzip

# SECTION 17: Network Tools
echo ">>> Installing network tools..."
sudo apt install -y \
    rsync \
    sshfs

# SECTION 18: Version Control
echo ">>> Installing version control tools..."
sudo apt install -y \
    git \
    git-lfs

# SECTION 19: CI/CD Tools
echo ">>> Installing CI/CD tools..."
sudo apt install -y jq

# Install GitHub CLI
if ! command -v gh &> /dev/null; then
    curl -fsSL https://cli.github.com/packages/githubcli-archive-keyring.gpg | \
        sudo dd of=/usr/share/keyrings/githubcli-archive-keyring.gpg
    echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/githubcli-archive-keyring.gpg] https://cli.github.com/packages stable main" | \
        sudo tee /etc/apt/sources.list.d/github-cli.list > /dev/null
    sudo apt update
    sudo apt install -y gh
else
    echo "✓ GitHub CLI already installed"
fi

# SECTION 20: Security Tools
echo ">>> Installing security tools..."
sudo apt install -y \
    gnupg \
    gnupg2 \
    ca-certificates

# SECTION 21: Python
echo ">>> Installing Python tools..."
sudo apt install -y \
    python3 \
    python3-pip \
    python3-venv \
    python3-dev

# Install Python packages
echo ">>> Installing Python build packages..."
pip3 install --user \
    conan \
    gcovr \
    cmake-format

echo ""
echo "========================================================================"
echo "Installation Complete!"
echo "========================================================================"
echo ""
echo "Summary:"
echo "--------"
echo "✓ Core build tools (gcc, clang, cmake, ninja)"
echo "✓ Cross-compilation (MinGW-w64 for Windows)"
echo "✓ OSXCross dependencies (SDK required separately)"
echo "✓ Container tools (Docker, Podman)"
echo "✓ Packaging tools (DEB, AppImage, RPM)"
echo "✓ Development tools (gdb, valgrind, clang-tidy)"
echo "✓ CI/CD tools (GitHub CLI, jq)"
echo ""
echo "Next Steps:"
echo "-----------"
echo "1. LOGOUT AND LOGIN (required for docker group)"
echo ""
echo "2. Verify installation:"
echo "   ./verify-build-environment.sh"
echo ""
echo "3. Optional - Install OSXCross (for macOS builds):"
echo "   See: docs/specifications/beta_requirements/builds/11_LINUX_TO_MACOS.md"
echo ""
echo "4. Optional - Install MXE (for Windows dependency management):"
echo "   git clone https://github.com/mxe/mxe.git /opt/mxe"
echo "   cd /opt/mxe"
echo "   make MXE_TARGETS='x86_64-w64-mingw32.static' spdlog gtest openssl lz4 zlib"
echo ""
echo "5. Test build ScratchBird:"
echo "   cd build"
echo "   cmake -G Ninja -DCMAKE_BUILD_TYPE=Release .."
echo "   ninja"
echo ""
echo "Disk space used: $(df -h / | tail -1 | awk '{print $3 " / " $4 " available"}')"
echo ""
echo "Installation log: /tmp/scratchbird-install-$(date +%Y%m%d-%H%M%S).log"
echo ""
