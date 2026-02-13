# Windows to Linux Cross-Compilation Requirements

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Platform:** Windows
**Target:** Linux (x86_64)
**Document Version:** 1.0
**Last Updated:** 2026-01-03

---

## 1. Overview

This document specifies all requirements for building ScratchBird for Linux targets from a Windows host system. The recommended approach is Windows Subsystem for Linux 2 (WSL2), which provides a genuine Linux kernel and environment.

---

## 2. System Requirements

### 2.1 Minimum System Requirements

| Component | Requirement |
|-----------|-------------|
| **OS** | Windows 10 version 2004+ (Build 19041+) or Windows 11 |
| **Architecture** | x64 (64-bit) |
| **RAM** | 16 GB (WSL2 can use up to 50% of total RAM) |
| **Disk Space** | 20 GB free (WSL2 + build artifacts + dependencies) |
| **CPU** | 4 cores minimum (8+ recommended) |
| **Virtualization** | Intel VT-x or AMD-V enabled in BIOS |

### 2.2 Windows Version Support

**Tier 1 (Fully Supported)**:
- Windows 11 (all versions)
- Windows 10 version 21H2 or later

**Tier 2 (Limited Support)**:
- Windows 10 version 2004-21H1

---

## 3. Build Approaches

### 3.1 Recommended: WSL2 (Windows Subsystem for Linux 2)

**Advantages**:
- Full Linux kernel and environment
- Native Linux binary compatibility
- Direct filesystem integration with Windows
- Best performance and compatibility
- Official Microsoft support

**Use Case**: Primary method for Linux development on Windows

### 3.2 Alternative: Docker Desktop for Windows

**Advantages**:
- Containerized Linux environment
- Reproducible builds
- Multi-distribution support

**Use Case**: CI/CD pipelines, containerized deployments

### 3.3 Alternative: MSYS2/Cygwin (Not Recommended)

**Limitations**:
- Not true Linux binaries (requires runtime layer)
- Poor compatibility with native Linux
- Not suitable for production builds

**Use Case**: Quick prototyping only

---

## 4. WSL2 Installation

### 4.1 Check Windows Version

```powershell
# Check Windows build
winver

# Should show Version 2004 (Build 19041) or later
```

### 4.2 Enable WSL2

**Method 1: Automatic Installation (Windows 11 / Windows 10 22H2+)**

```powershell
# Open PowerShell as Administrator
wsl --install

# This installs WSL2 and Ubuntu by default
# Reboot when prompted
```

**Method 2: Manual Installation (Older Windows 10)**

```powershell
# 1. Enable WSL feature
dism.exe /online /enable-feature /featurename:Microsoft-Windows-Subsystem-Linux /all /norestart

# 2. Enable Virtual Machine Platform
dism.exe /online /enable-feature /featurename:VirtualMachinePlatform /all /norestart

# 3. Reboot
Restart-Computer

# 4. Download and install WSL2 kernel update
# https://aka.ms/wsl2kernel

# 5. Set WSL2 as default
wsl --set-default-version 2
```

### 4.3 Install Linux Distribution

```powershell
# List available distributions
wsl --list --online

# Install Ubuntu 22.04 (recommended)
wsl --install -d Ubuntu-22.04

# Or install Debian
wsl --install -d Debian

# Verify installation
wsl --list --verbose
# Should show VERSION 2 for your distribution
```

### 4.4 First Launch and Setup

```powershell
# Launch WSL
wsl

# On first launch, create a UNIX username and password
# This user has sudo privileges
```

---

## 5. WSL2 Configuration

### 5.1 Configure WSL2 Resources

Create `%USERPROFILE%\.wslconfig`:

```ini
[wsl2]
# Limit memory to 8GB (adjust based on your system)
memory=8GB

# Limit CPU cores to 4 (adjust based on your system)
processors=4

# Swap size
swap=4GB

# Disable page reporting
pageReporting=false

# Networking mode
networkingMode=mirrored
```

**Apply configuration**:
```powershell
# Shutdown WSL to apply changes
wsl --shutdown

# Restart WSL
wsl
```

### 5.2 Update WSL2 Kernel

```powershell
# Update WSL2
wsl --update

# Check version
wsl --version
```

---

## 6. Install Build Tools in WSL2

### 6.1 Ubuntu / Debian in WSL2

```bash
# Update package list
sudo apt update && sudo apt upgrade -y

# Install build tools
sudo apt install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    pkg-config

# Install compiler (GCC - included in build-essential)
# Or install Clang
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

# Install development tools
sudo apt install -y \
    ccache \
    clang-format \
    clang-tidy \
    valgrind \
    gdb
```

### 6.2 Fedora in WSL2

```bash
# Update system
sudo dnf update -y

# Install build tools
sudo dnf install -y \
    gcc \
    gcc-c++ \
    cmake \
    ninja-build \
    git \
    pkg-config

# Install core dependencies
sudo dnf install -y \
    spdlog-devel \
    gtest-devel \
    openssl-devel \
    lz4-devel \
    zlib-devel

# Install development tools
sudo dnf install -y \
    ccache \
    clang-tools-extra \
    valgrind \
    gdb
```

---

## 7. Filesystem Integration

### 7.1 Accessing Windows Files from WSL2

```bash
# Windows drives are mounted under /mnt/
ls /mnt/c/Users/YourUsername/

# Navigate to Windows user directory
cd /mnt/c/Users/YourUsername/

# Best practice: Clone repository in WSL2 filesystem for better performance
mkdir ~/projects
cd ~/projects
git clone https://github.com/yourusername/ScratchBird.git
```

### 7.2 Accessing WSL2 Files from Windows

**In Windows File Explorer**:
```
\\wsl$\Ubuntu-22.04\home\username\projects\ScratchBird
```

**Or via PowerShell**:
```powershell
# Open WSL home directory in Explorer
explorer.exe \\wsl$\Ubuntu-22.04\home\username\
```

### 7.3 Performance Considerations

**CRITICAL**: For best performance:
- Store source code in WSL2 filesystem (`/home/username/...`)
- Avoid using `/mnt/c/...` for active development
- Cross-filesystem access is ~10x slower

```bash
# FAST: Files in WSL2 filesystem
~/projects/ScratchBird/  # ✅ Good

# SLOW: Files in Windows filesystem
/mnt/c/Users/You/ScratchBird/  # ❌ Slow
```

---

## 8. CMake Configuration in WSL2

### 8.1 Basic Configuration

```bash
# Navigate to project (in WSL2 filesystem)
cd ~/projects/ScratchBird

# Create build directory
mkdir build
cd build

# Configure with CMake
cmake -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER=gcc \
      -DCMAKE_CXX_COMPILER=g++ \
      ..
```

### 8.2 With ccache

```bash
cmake -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER_LAUNCHER=ccache \
      -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
      ..
```

---

## 9. Build Commands in WSL2

### 9.1 Build with Ninja

```bash
# From build directory
ninja

# Parallel build (explicit)
ninja -j$(nproc)
```

### 9.2 Build with Make

```bash
# If using Make generator
make -j$(nproc)
```

---

## 10. Testing in WSL2

### 10.1 Run Tests

```bash
# From build directory
ctest --output-on-failure

# Parallel testing
ctest -j$(nproc) --output-on-failure
```

### 10.2 Run Executable

```bash
# Run built executable
./scratchbird --version

# Run with debugging
gdb ./scratchbird
```

---

## 11. Docker Desktop for Windows

### 11.1 Install Docker Desktop

1. Download Docker Desktop from https://www.docker.com/products/docker-desktop/
2. Install Docker Desktop
3. Enable WSL2 backend in Docker Desktop settings
4. Restart Docker Desktop

### 11.2 Verify Installation

```powershell
# Check Docker version
docker --version

# Run test container
docker run hello-world
```

### 11.3 Build in Docker Container

**Create Dockerfile**:
```dockerfile
# Dockerfile.linux-build
FROM ubuntu:22.04

# Install build dependencies
RUN apt update && apt install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    libspdlog-dev \
    libgtest-dev \
    libssl-dev \
    liblz4-dev \
    zlib1g-dev

WORKDIR /src
COPY . .

# Build
RUN mkdir build && cd build && \
    cmake -G Ninja -DCMAKE_BUILD_TYPE=Release .. && \
    ninja

# Output directory
RUN mkdir /output && cp build/scratchbird /output/
```

**Build using Docker**:
```powershell
# Build Docker image
docker build -f Dockerfile.linux-build -t scratchbird-linux .

# Extract binary
docker create --name temp scratchbird-linux
docker cp temp:/output/scratchbird ./scratchbird-linux
docker rm temp
```

---

## 12. IDE Integration

### 12.1 Visual Studio Code with WSL2

**Install VS Code Extension**:
1. Install "Remote - WSL" extension in VS Code
2. Open VS Code
3. Press F1 → "WSL: Connect to WSL"

**From WSL2**:
```bash
# Open project in VS Code from WSL2
cd ~/projects/ScratchBird
code .
```

**VS Code will**:
- Connect to WSL2
- Use WSL2 compilers and tools
- Provide IntelliSense with WSL2 headers
- Debug with WSL2 GDB

### 12.2 CLion with WSL2

**Configure CLion Toolchain**:
1. File → Settings → Build, Execution, Deployment → Toolchains
2. Click "+" → WSL
3. Select your WSL distribution
4. CLion will auto-detect compilers

---

## 13. Troubleshooting

### 13.1 WSL2 Not Starting

```powershell
# Check WSL status
wsl --status

# Restart WSL
wsl --shutdown
wsl

# Update WSL
wsl --update
```

### 13.2 Slow Filesystem Performance

```bash
# Problem: Using /mnt/c/ for source code
cd /mnt/c/Users/You/ScratchBird  # ❌ Slow

# Solution: Move to WSL2 filesystem
cd ~
cp -r /mnt/c/Users/You/ScratchBird ~/projects/
cd ~/projects/ScratchBird  # ✅ Fast
```

### 13.3 Network Connectivity Issues

```bash
# Check DNS resolution
cat /etc/resolv.conf

# If broken, regenerate
sudo rm /etc/resolv.conf
sudo bash -c 'echo "nameserver 8.8.8.8" > /etc/resolv.conf'
```

### 13.4 Out of Memory

Edit `%USERPROFILE%\.wslconfig`:
```ini
[wsl2]
memory=16GB  # Increase memory allocation
```

```powershell
# Apply changes
wsl --shutdown
wsl
```

### 13.5 Missing Dependencies

```bash
# Update package lists
sudo apt update

# Fix broken packages
sudo apt --fix-broken install

# Reinstall package
sudo apt install --reinstall libspdlog-dev
```

---

## 14. Distribution and Deployment

### 14.1 Copy Binary to Windows

```bash
# From WSL2
cp ~/projects/ScratchBird/build/scratchbird /mnt/c/Users/YourUsername/Desktop/
```

### 14.2 Create Linux Package in WSL2

```bash
# Install packaging tools
sudo apt install -y rpm dpkg-dev

# Create DEB package
cd build
cpack -G DEB

# Create RPM package
cpack -G RPM

# Copy packages to Windows
cp *.deb /mnt/c/Users/YourUsername/Desktop/
cp *.rpm /mnt/c/Users/YourUsername/Desktop/
```

---

## 15. Continuous Integration

### 15.1 GitHub Actions with WSL2

```yaml
# .github/workflows/windows-wsl2-build.yml
jobs:
  build-wsl2:
    runs-on: windows-2022
    steps:
      - uses: actions/checkout@v4

      - name: Setup WSL
        uses: Vampire/setup-wsl@v2
        with:
          distribution: Ubuntu-22.04

      - name: Install dependencies
        shell: wsl-bash {0}
        run: |
          sudo apt update
          sudo apt install -y build-essential cmake ninja-build \
            libspdlog-dev libgtest-dev libssl-dev liblz4-dev

      - name: Configure
        shell: wsl-bash {0}
        run: |
          cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -B build

      - name: Build
        shell: wsl-bash {0}
        run: ninja -C build

      - name: Test
        shell: wsl-bash {0}
        run: ctest --test-dir build --output-on-failure

      - name: Upload artifacts
        uses: actions/upload-artifact@v4
        with:
          name: linux-x64-build
          path: build/scratchbird
```

---

## 16. MSYS2 Alternative (Not Recommended)

### 16.1 Install MSYS2

```powershell
# Download from https://www.msys2.org/
# Run installer

# Update MSYS2
pacman -Syu
```

### 16.2 Install Build Tools

```bash
# In MSYS2 terminal
pacman -S --needed \
    mingw-w64-x86_64-gcc \
    mingw-w64-x86_64-cmake \
    mingw-w64-x86_64-ninja \
    git
```

**Note**: MSYS2 produces Windows binaries with POSIX compatibility layer, NOT native Linux binaries. Use WSL2 for true Linux builds.

---

## 17. Comparison Matrix

| Method | Native Linux Binary | Performance | Complexity | CI/CD Support |
|--------|---------------------|-------------|------------|---------------|
| **WSL2** | ✅ Yes | ⭐⭐⭐⭐⭐ | Low | ✅ Excellent |
| **Docker Desktop** | ✅ Yes | ⭐⭐⭐⭐ | Medium | ✅ Excellent |
| **MSYS2** | ❌ No (Windows+layer) | ⭐⭐ | Low | ⚠️ Limited |

---

## 18. Version Matrix

| Windows Version | WSL2 Version | Linux Distribution | Status |
|-----------------|--------------|-------------------|--------|
| Windows 11 22H2 | 2.0.14+ | Ubuntu 22.04 | ✅ Tested |
| Windows 10 21H2 | 2.0.14+ | Ubuntu 22.04 | ✅ Tested |
| Windows 10 21H2 | 2.0.14+ | Debian 12 | ✅ Tested |
| Windows 11 | 2.0.14+ | Fedora 38 | ○ Community |

---

## 19. Best Practices

### 19.1 Performance

1. **Always** use WSL2 filesystem for source code
2. Use `ccache` to speed up rebuilds
3. Allocate sufficient RAM to WSL2 (`.wslconfig`)
4. Use Ninja instead of Make for faster builds

### 19.2 Development Workflow

1. Install VS Code with Remote-WSL extension
2. Clone repository in WSL2 home directory
3. Open project in VS Code from WSL2
4. Build and test entirely in WSL2
5. Only copy final binaries to Windows if needed

### 19.3 Backup

```bash
# Export WSL2 distribution (backup)
wsl --export Ubuntu-22.04 D:\Backups\ubuntu-backup.tar

# Import WSL2 distribution (restore)
wsl --import Ubuntu-Restored D:\WSL\Ubuntu D:\Backups\ubuntu-backup.tar
```

---

## 20. Additional Resources

- **WSL Documentation:** https://docs.microsoft.com/en-us/windows/wsl/
- **WSL GitHub:** https://github.com/microsoft/WSL
- **Docker Desktop:** https://docs.docker.com/desktop/windows/
- **VS Code Remote-WSL:** https://code.visualstudio.com/docs/remote/wsl

---

**Document Version:** 1.0
**Last Updated:** 2026-01-03
**Maintainer:** Build Infrastructure Team
**Status:** Beta Preparation
