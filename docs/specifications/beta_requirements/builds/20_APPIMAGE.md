# AppImage Package Requirements

**Package Format:** AppImage
**Target Platform:** Linux (Universal)
**Document Version:** 1.0
**Last Updated:** 2026-01-03

---

## 1. Overview

This document specifies all requirements for packaging ScratchBird as an AppImage - a universal Linux package format that runs on any distribution without installation.

**AppImage Advantages**:
- **No installation required**: Run from any location
- **Distribution-agnostic**: Works on any modern Linux distribution
- **Self-contained**: All dependencies bundled
- **Portable**: Single file, easy to distribute
- **No root required**: Users don't need admin privileges

---

## 2. System Requirements

### 2.1 Build System Requirements

| Component | Requirement |
|-----------|-------------|
| **OS** | Linux (Ubuntu 20.04+ recommended for compatibility) |
| **Architecture** | x86_64 or ARM64 |
| **RAM** | 8 GB (16 GB recommended) |
| **Disk Space** | 10 GB free |
| **FUSE** | FUSE 2.x or 3.x (for testing AppImage) |

### 2.2 Target System Requirements

**Minimum Linux Compatibility**:
- glibc 2.31+ (Ubuntu 20.04, Debian 11, RHEL 8.4+)
- Linux kernel 4.15+
- FUSE 2.x or 3.x (for mounting AppImage)

**Supported Distributions**:
- Ubuntu 20.04+, Debian 11+
- Fedora 33+, RHEL 8.4+, Rocky Linux 8.4+
- Arch Linux, openSUSE Leap 15.3+
- Linux Mint 20+

---

## 3. AppImage Tools

### 3.1 Required Tools

**linuxdeploy**:
- **Purpose**: Creates AppDir and bundles dependencies
- **Version**: 1.0.0 or later
- **Download**: https://github.com/linuxdeploy/linuxdeploy

**appimagetool**:
- **Purpose**: Converts AppDir to AppImage
- **Version**: Continuous build
- **Download**: https://github.com/AppImage/AppImageKit

### 3.2 Optional Tools

**linuxdeploy-plugin-qt** (if using Qt):
- **Purpose**: Qt-specific bundling
- **Download**: https://github.com/linuxdeploy/linuxdeploy-plugin-qt

**linuxdeploy-plugin-gtk** (if using GTK):
- **Purpose**: GTK-specific bundling

---

## 4. Installation of Tools

### 4.1 Ubuntu / Debian

```bash
# Install FUSE (required for AppImage)
sudo apt update
sudo apt install -y fuse libfuse2

# Install dependencies for building
sudo apt install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    file \
    wget \
    patchelf \
    desktop-file-utils

# Download linuxdeploy
wget https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
chmod +x linuxdeploy-x86_64.AppImage

# Download appimagetool
wget https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage
chmod +x appimagetool-x86_64.AppImage

# Move to PATH
sudo mv linuxdeploy-x86_64.AppImage /usr/local/bin/linuxdeploy
sudo mv appimagetool-x86_64.AppImage /usr/local/bin/appimagetool
```

### 4.2 Fedora / RHEL

```bash
# Install FUSE
sudo dnf install -y fuse fuse-libs

# Install build dependencies
sudo dnf install -y \
    gcc \
    gcc-c++ \
    cmake \
    ninja-build \
    git \
    file \
    wget \
    patchelf \
    desktop-file-utils

# Download and install tools (same as Ubuntu)
# ... (see 4.1)
```

---

## 5. Build ScratchBird for AppImage

### 5.1 Build Configuration

**Important**: Build on oldest supported distribution (Ubuntu 20.04) for maximum compatibility.

```bash
# Clone repository
git clone https://github.com/yourusername/ScratchBird.git
cd ScratchBird

# Create build directory
mkdir build-appimage
cd build-appimage

# Configure for AppImage
cmake -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/usr \
      -DBUILD_SHARED_LIBS=OFF \
      ..

# Note: Use -DBUILD_SHARED_LIBS=OFF for static linking
# This reduces external dependencies
```

### 5.2 Build and Install to AppDir

```bash
# Build
ninja

# Install to temporary AppDir
DESTDIR=AppDir ninja install

# Verify AppDir structure
tree AppDir
# Expected structure:
# AppDir/
#   usr/
#     bin/
#       scratchbird
#     lib/    (if shared libraries)
#     share/
#       applications/
#       icons/
```

---

## 6. Create AppDir Structure

### 6.1 Required Files

**Desktop Entry File**: `scratchbird.desktop`

Create `AppDir/usr/share/applications/scratchbird.desktop`:
```ini
[Desktop Entry]
Type=Application
Name=ScratchBird
Comment=SQL Database Engine
Exec=scratchbird
Icon=scratchbird
Categories=Development;Database;
Terminal=false
```

**Icon File**: `scratchbird.png`

Create or copy icon:
```bash
# Copy icon (256x256 recommended)
cp ../resources/scratchbird.png AppDir/usr/share/icons/hicolor/256x256/apps/scratchbird.png

# Or create SVG icon
cp ../resources/scratchbird.svg AppDir/usr/share/icons/hicolor/scalable/apps/scratchbird.svg
```

**AppRun Script** (optional, for custom startup):

Create `AppDir/AppRun`:
```bash
#!/bin/bash
# AppRun script for ScratchBird

SELF=$(readlink -f "$0")
HERE=${SELF%/*}

# Set library path
export LD_LIBRARY_PATH="${HERE}/usr/lib:${LD_LIBRARY_PATH}"

# Execute ScratchBird
exec "${HERE}/usr/bin/scratchbird" "$@"
```

```bash
chmod +x AppDir/AppRun
```

---

## 7. Bundle Dependencies

### 7.1 Using linuxdeploy

**Automatic Bundling**:
```bash
# Bundle all dependencies automatically
linuxdeploy --appdir AppDir \
            --executable AppDir/usr/bin/scratchbird \
            --desktop-file AppDir/usr/share/applications/scratchbird.desktop \
            --icon-file AppDir/usr/share/icons/hicolor/256x256/apps/scratchbird.png \
            --output appimage

# This creates ScratchBird-x86_64.AppImage
```

### 7.2 Manual Bundling (if needed)

**Copy specific libraries**:
```bash
# Create lib directory
mkdir -p AppDir/usr/lib

# Copy required libraries (example)
ldd AppDir/usr/bin/scratchbird | grep "=> /" | awk '{print $3}' | while read lib; do
    cp "$lib" AppDir/usr/lib/
done

# Use patchelf to set RPATH
patchelf --set-rpath '$ORIGIN/../lib' AppDir/usr/bin/scratchbird
```

### 7.3 Exclude System Libraries

**Create excludelist**:

Some libraries should NOT be bundled (provided by all systems):
```bash
# linuxdeploy automatically excludes these:
# - libc.so.6
# - libpthread.so.0
# - libdl.so.2
# - libm.so.6
# - librt.so.1
# - libX11.so.6 (graphics libraries)
# - libGL.so.1 (OpenGL)

# Custom excludelist (if needed)
cat > excludelist << EOF
libc.so.6
libpthread.so.0
libdl.so.2
libm.so.6
EOF

linuxdeploy --appdir AppDir \
            --executable AppDir/usr/bin/scratchbird \
            --library-filter excludelist \
            --output appimage
```

---

## 8. Create AppImage

### 8.1 Using linuxdeploy (Recommended)

```bash
# All-in-one command
linuxdeploy --appdir AppDir \
            --executable AppDir/usr/bin/scratchbird \
            --desktop-file AppDir/usr/share/applications/scratchbird.desktop \
            --icon-file AppDir/usr/share/icons/hicolor/256x256/apps/scratchbird.png \
            --output appimage

# Output: ScratchBird-x86_64.AppImage
```

### 8.2 Using appimagetool Directly

```bash
# Create AppImage from AppDir
appimagetool AppDir ScratchBird-x86_64.AppImage

# With custom compression (gzip, xz)
appimagetool --comp gzip AppDir ScratchBird-x86_64.AppImage

# Without compression (faster startup, larger file)
appimagetool --no-appstream AppDir ScratchBird-x86_64.AppImage
```

### 8.3 Sign AppImage (Optional)

```bash
# Generate GPG key (if you don't have one)
gpg --full-generate-key

# Sign AppImage
gpg --detach-sign --armor ScratchBird-x86_64.AppImage

# This creates ScratchBird-x86_64.AppImage.asc

# Verify signature
gpg --verify ScratchBird-x86_64.AppImage.asc ScratchBird-x86_64.AppImage
```

---

## 9. Versioning and Naming

### 9.1 Naming Convention

**Recommended format**:
```
ScratchBird-{version}-{arch}.AppImage
```

**Examples**:
```bash
# Version + architecture
ScratchBird-0.1.0-x86_64.AppImage
ScratchBird-0.1.0-aarch64.AppImage

# Version + git hash
ScratchBird-0.1.0-git.abc1234-x86_64.AppImage

# Latest (for continuous builds)
ScratchBird-latest-x86_64.AppImage
```

### 9.2 Embedding Version Information

**In desktop file**:
```ini
[Desktop Entry]
Version=0.1.0
X-AppImage-Version=0.1.0-git.abc1234
```

**Using environment variable**:
```bash
# Set version during build
export VERSION=0.1.0

linuxdeploy --appdir AppDir \
            --executable AppDir/usr/bin/scratchbird \
            --output appimage
```

---

## 10. Testing AppImage

### 10.1 Basic Testing

```bash
# Make executable
chmod +x ScratchBird-x86_64.AppImage

# Run AppImage
./ScratchBird-x86_64.AppImage --version

# Extract AppImage for inspection
./ScratchBird-x86_64.AppImage --appimage-extract

# Inspect extracted contents
tree squashfs-root/
```

### 10.2 Test on Different Distributions

**Using Docker**:
```bash
# Test on Ubuntu 22.04
docker run -it --rm \
    -v $(pwd)/ScratchBird-x86_64.AppImage:/ScratchBird.AppImage \
    ubuntu:22.04 \
    /ScratchBird.AppImage --version

# Test on Fedora 38
docker run -it --rm \
    -v $(pwd)/ScratchBird-x86_64.AppImage:/ScratchBird.AppImage \
    fedora:38 \
    /ScratchBird.AppImage --version

# Test on Debian 12
docker run -it --rm \
    -v $(pwd)/ScratchBird-x86_64.AppImage:/ScratchBird.AppImage \
    debian:12 \
    /ScratchBird.AppImage --version
```

### 10.3 Verify Dependencies

```bash
# Check bundled libraries
./ScratchBird-x86_64.AppImage --appimage-extract
ldd squashfs-root/usr/bin/scratchbird

# All paths should be relative to $ORIGIN or system libraries
```

---

## 11. AppImage Updates

### 11.1 AppImageUpdate Support

**Embed update information**:

Create `AppDir/.DirIcon` and `AppDir/ScratchBird.desktop`:
```ini
[Desktop Entry]
X-AppImage-Update-Information=gh-releases-zsync|yourusername|scratchbird|latest|ScratchBird-*-x86_64.AppImage.zsync
```

**Generate zsync file**:
```bash
# Install zsync
sudo apt install zsync

# Create zsync file
zsyncmake ScratchBird-0.1.0-x86_64.AppImage

# This creates ScratchBird-0.1.0-x86_64.AppImage.zsync
# Upload both .AppImage and .zsync to GitHub releases
```

### 11.2 Using AppImageUpdate

**End users can update**:
```bash
# Install AppImageUpdate
wget https://github.com/AppImage/AppImageUpdate/releases/download/continuous/AppImageUpdate-x86_64.AppImage
chmod +x AppImageUpdate-x86_64.AppImage

# Update ScratchBird
./AppImageUpdate-x86_64.AppImage ScratchBird-x86_64.AppImage
```

---

## 12. Distribution

### 12.1 GitHub Releases

```bash
# Create release with gh CLI
gh release create v0.1.0 \
    ScratchBird-0.1.0-x86_64.AppImage \
    ScratchBird-0.1.0-x86_64.AppImage.zsync \
    --title "ScratchBird v0.1.0" \
    --notes "Release notes here"
```

### 12.2 AppImageHub

**Submit to AppImageHub**:
1. Fork https://github.com/AppImage/appimage.github.io
2. Add `database/ScratchBird.md`:
   ```yaml
   ---
   layout: app

   permalink: /ScratchBird/
   description: SQL Database Engine

   screenshots:
     - https://example.com/screenshot.png

   authors:
     - name: YourName
       url: https://github.com/yourusername

   links:
     - type: GitHub
       url: https://github.com/yourusername/ScratchBird
     - type: Download
       url: https://github.com/yourusername/ScratchBird/releases

   desktop:
     Desktop Entry:
       Name: ScratchBird
       Comment: SQL Database Engine
       Exec: scratchbird
       Icon: scratchbird
       Categories: Development;Database;
       Type: Application
   ---
   ```
3. Submit pull request

### 12.3 Direct Download

**Host on your website**:
```html
<a href="https://example.com/downloads/ScratchBird-latest-x86_64.AppImage">
  Download ScratchBird AppImage
</a>
```

---

## 13. Continuous Integration

### 13.1 GitHub Actions

**.github/workflows/appimage.yml**:
```yaml
name: Build AppImage

on:
  push:
    tags: ['v*']
  workflow_dispatch:

jobs:
  build-appimage:
    runs-on: ubuntu-20.04  # Use oldest supported for compatibility

    steps:
      - uses: actions/checkout@v4

      - name: Install dependencies
        run: |
          sudo apt update
          sudo apt install -y \
            build-essential cmake ninja-build git \
            libspdlog-dev libgtest-dev libssl-dev liblz4-dev \
            file wget patchelf desktop-file-utils

      - name: Download AppImage tools
        run: |
          wget https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
          chmod +x linuxdeploy-x86_64.AppImage
          sudo mv linuxdeploy-x86_64.AppImage /usr/local/bin/linuxdeploy

      - name: Build ScratchBird
        run: |
          cmake -G Ninja \
                -DCMAKE_BUILD_TYPE=Release \
                -DCMAKE_INSTALL_PREFIX=/usr \
                -DBUILD_SHARED_LIBS=OFF \
                -B build
          ninja -C build
          DESTDIR=AppDir ninja -C build install

      - name: Create AppImage
        run: |
          linuxdeploy --appdir AppDir \
                      --executable AppDir/usr/bin/scratchbird \
                      --desktop-file AppDir/usr/share/applications/scratchbird.desktop \
                      --icon-file AppDir/usr/share/icons/hicolor/256x256/apps/scratchbird.png \
                      --output appimage

          # Rename with version
          mv ScratchBird-*.AppImage ScratchBird-${{ github.ref_name }}-x86_64.AppImage

      - name: Upload AppImage
        uses: actions/upload-artifact@v4
        with:
          name: appimage-x86_64
          path: ScratchBird-*.AppImage

      - name: Release
        uses: softprops/action-gh-release@v1
        if: startsWith(github.ref, 'refs/tags/')
        with:
          files: ScratchBird-*.AppImage
        env:
          GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
```

---

## 14. Troubleshooting

### 14.1 AppImage Won't Run

```bash
# Check FUSE is installed
fusermount --version

# Install FUSE if missing
sudo apt install fuse libfuse2  # Ubuntu/Debian
sudo dnf install fuse fuse-libs  # Fedora

# Extract and run manually
./ScratchBird.AppImage --appimage-extract
./squashfs-root/AppRun
```

### 14.2 Missing Libraries

```bash
# Extract AppImage
./ScratchBird.AppImage --appimage-extract

# Check dependencies
ldd squashfs-root/usr/bin/scratchbird

# If library missing, bundle it:
cp /path/to/missing.so squashfs-root/usr/lib/
# Recreate AppImage
```

### 14.3 Wrong Architecture

```bash
# Check AppImage architecture
file ScratchBird.AppImage
# Should match your system (x86-64 or aarch64)

# Build for correct architecture
uname -m  # Check your architecture
# Build on matching system
```

---

## 15. Best Practices

### 15.1 Compatibility

- **Build on oldest supported distribution** (Ubuntu 20.04)
- **Use static linking** where possible
- **Test on multiple distributions** before release

### 15.2 Size Optimization

```bash
# Strip binaries
strip AppDir/usr/bin/scratchbird

# Use compression
appimagetool --comp xz AppDir

# Remove unnecessary files
rm -rf AppDir/usr/share/doc
rm -rf AppDir/usr/share/man
```

### 15.3 User Experience

- Provide clear `--help` output
- Include version information (`--version`)
- Use standard exit codes
- Provide meaningful error messages

---

## 16. Additional Resources

- **AppImage Documentation:** https://docs.appimage.org/
- **linuxdeploy:** https://github.com/linuxdeploy/linuxdeploy
- **AppImageKit:** https://github.com/AppImage/AppImageKit
- **AppImageHub:** https://www.appimagehub.com/

---

**Document Version:** 1.0
**Last Updated:** 2026-01-03
**Maintainer:** Build Infrastructure Team
**Status:** Beta Preparation
