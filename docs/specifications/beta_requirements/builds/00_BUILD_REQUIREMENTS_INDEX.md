# ScratchBird Build Requirements - Index

**Version:** 1.0
**Last Updated:** 2026-01-02
**Purpose:** Comprehensive cross-platform build requirements for Beta release

---

## Overview

This directory contains detailed build requirements for compiling ScratchBird on various platforms and for various target platforms. Each document provides a complete list of required tools, libraries, and packages needed to successfully build the project.

---

## Document Structure

### Native Builds (Building on the same platform as the target)

| Document | Platform | Target | Description |
|----------|----------|--------|-------------|
| **01_LINUX_NATIVE.md** | Linux | Linux | Building on Linux for Linux |
| **02_WINDOWS_NATIVE.md** | Windows | Windows | Building on Windows for Windows |
| **03_MACOS_NATIVE.md** | macOS | macOS | Building on macOS for macOS |

### Cross-Compilation Builds (Building on one platform for another)

| Document | Platform | Target | Description |
|----------|----------|--------|-------------|
| **10_LINUX_TO_WINDOWS.md** | Linux | Windows | Cross-compiling Windows binaries on Linux |
| **11_LINUX_TO_MACOS.md** | Linux | macOS | Cross-compiling macOS binaries on Linux |
| **12_WINDOWS_TO_LINUX.md** | Windows | Linux | Cross-compiling Linux binaries on Windows (WSL2) |

### Package Formats

| Document | Format | Description |
|----------|--------|-------------|
| **20_APPIMAGE.md** | AppImage | Linux universal binary package requirements |
| **21_FLATPAK.md** | Flatpak | Linux containerized app requirements |
| **22_SNAP.md** | Snap | Ubuntu/Linux Snap package requirements |
| **23_DEB.md** | .deb | Debian/Ubuntu package requirements |
| **24_RPM.md** | .rpm | Red Hat/Fedora/SUSE package requirements |
| **25_MSI.md** | .msi | Windows installer package requirements |
| **26_DMG.md** | .dmg | macOS disk image requirements |
| **27_BREW.md** | Homebrew | macOS Homebrew formula requirements |

### Container/Virtualization

| Document | Technology | Description |
|----------|------------|-------------|
| **30_DOCKER.md** | Docker | Docker container build requirements |
| **31_PODMAN.md** | Podman | Podman container build requirements |
| **32_LXC.md** | LXC/LXD | Linux container requirements |

### CI/CD Integration

| Document | Platform | Description |
|----------|----------|-------------|
| **40_GITHUB_ACTIONS.md** | GitHub Actions | CI/CD pipeline requirements |
| **41_GITLAB_CI.md** | GitLab CI | CI/CD pipeline requirements |
| **42_JENKINS.md** | Jenkins | CI/CD pipeline requirements |

---

## Quick Reference

### Minimum Requirements (All Platforms)

- **C++ Compiler:** C++17 or C++20 support
- **CMake:** 3.20 or later
- **Build System:** Make, Ninja, or MSBuild
- **Git:** 2.30 or later

### Core Dependencies (All Platforms)

- **spdlog:** 1.x (logging framework)
- **GoogleTest:** 1.14+ (testing framework)
- **OpenSSL or BoringSSL:** TLS and encryption
- **LZ4:** Compression library
- **pthread:** Threading (Linux/macOS, included in MSVC on Windows)

### Optional Dependencies

- **GEOS:** Spatial geometry (optional)
- **PROJ:** Geographic projections (optional)
- **libxml2:** XML support (optional)

---

## Platform-Specific Notes

### Linux
- Recommended distributions: Ubuntu 22.04+, Debian 12+, Fedora 38+, RHEL 9+
- Package managers: apt, dnf, pacman, zypper
- Multiple toolchain options: GCC 11+, Clang 14+

### Windows
- Visual Studio 2019 or 2022 required
- MSVC v142 or v143 toolset
- vcpkg recommended for dependency management
- Windows SDK 10.0.19041.0 or later

### macOS
- Xcode 13+ or Xcode Command Line Tools
- macOS 11 (Big Sur) or later
- Homebrew for dependency management
- Apple Clang 13+ or LLVM Clang 14+

---

## Cross-Compilation Notes

### Linux → Windows
- MinGW-w64 toolchain required
- Wine for testing (optional but recommended)
- CMake toolchain file for cross-compilation

### Linux → macOS
- OSXCross toolchain required
- macOS SDK (requires Apple Developer Account)
- Challenging due to Apple licensing restrictions

### Windows → Linux
- WSL2 (Windows Subsystem for Linux) recommended
- Alternatively: Docker Desktop for Windows
- MSYS2/MinGW alternative (less recommended)

---

## Package Format Notes

### AppImage
- Most portable Linux format
- No root required, runs on any distribution
- Requires AppImageKit and linuxdeploy

### Flatpak
- Sandboxed application
- Requires Flatpak runtime and SDK
- Flathub distribution platform

### Snap
- Ubuntu-centric but cross-distribution
- Requires snapcraft
- Snapcraft.io distribution platform

### Docker
- Platform-agnostic containerization
- Multi-stage builds recommended
- Alpine Linux base for minimal images

---

## Beta Release Targets

For the Beta release, we aim to provide:

### Tier 1 Support (Native builds, full testing)
- ✅ Linux (Ubuntu 22.04 LTS, Debian 12)
- ✅ Linux (Fedora 38+, RHEL 9+)
- ✅ Windows 10/11 (x64)
- ✅ macOS 12+ (x64 and ARM64)

### Tier 2 Support (Builds provided, limited testing)
- ○ AppImage (universal Linux)
- ○ Docker (multi-platform)
- ○ Flatpak
- ○ Homebrew (macOS)

### Tier 3 Support (Community builds, no official support)
- △ Snap
- △ Arch Linux (AUR)
- △ FreeBSD (ports)

---

## Usage

1. **Identify your build scenario**: Native build, cross-compilation, or package creation
2. **Locate the relevant document**: See table above
3. **Follow the requirements**: Install all listed tools and dependencies
4. **Verify installation**: Each document includes verification commands
5. **Proceed with build**: Use the CMake commands provided

---

## Contribution Guidelines

When adding new platform support:
1. Create a new document following the template format
2. List ALL required packages with exact version requirements
3. Include package manager installation commands
4. Provide verification steps
5. Document any platform-specific quirks or issues
6. Test the build process completely before submitting

---

## Document Template Structure

Each build requirements document should follow this structure:

1. **Overview**: Platform and target description
2. **System Requirements**: OS version, architecture, disk space, RAM
3. **Build Tools**: Compilers, CMake, build systems
4. **Core Dependencies**: Required libraries
5. **Optional Dependencies**: Optional features
6. **Package Manager Commands**: Complete installation commands
7. **Verification Steps**: How to verify everything is installed
8. **CMake Configuration**: Platform-specific CMake flags
9. **Build Commands**: Complete build process
10. **Testing**: How to run tests
11. **Troubleshooting**: Common issues and solutions

---

## Maintenance

These documents should be updated:
- When minimum versions change
- When new dependencies are added
- When platform-specific issues are discovered
- Before each Beta release milestone
- When CI/CD configurations change

**Maintainer:** Build Infrastructure Team

**Review Cycle:** Quarterly or before each release

---

**Last Updated:** 2026-01-02
**Version:** 1.0
**Status:** Beta Preparation
