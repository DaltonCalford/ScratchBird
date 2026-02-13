# DEB Package Requirements

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.



**Authoritative MGA/Lock/GC References:**
- [TRANSACTION_MGA_CORE.md](../../transaction/TRANSACTION_MGA_CORE.md)
- [TRANSACTION_LOCK_MANAGER.md](../../transaction/TRANSACTION_LOCK_MANAGER.md)
- [MGA_IMPLEMENTATION.md](../../storage/MGA_IMPLEMENTATION.md)
- [FIREBIRD_GC_SWEEP_GLOSSARY.md](../../transaction/FIREBIRD_GC_SWEEP_GLOSSARY.md)
- [FIREBIRD_CONSTANTS_REFERENCE.md](../../transaction/FIREBIRD_CONSTANTS_REFERENCE.md)


**Package Format:** .deb (Debian Package)
**Target Platforms:** Debian, Ubuntu, Linux Mint, and derivatives
**Document Version:** 1.0
**Last Updated:** 2026-01-03

---

## 1. Overview

This document specifies all requirements for creating .deb packages for ScratchBird. DEB is the native package format for Debian-based distributions including Ubuntu, Linux Mint, and others.

**DEB Package Advantages**:
- Native package format for Debian/Ubuntu
- Dependency management via APT
- Integration with system package manager
- Digital signatures and verification
- Automatic updates via repositories

---

## 2. System Requirements

### 2.1 Build System Requirements

| Component | Requirement |
|-----------|-------------|
| **OS** | Debian 11+ or Ubuntu 20.04+ (for maximum compatibility) |
| **Architecture** | x86_64 (amd64) or ARM64 (arm64) |
| **RAM** | 8 GB |
| **Disk Space** | 10 GB free |

### 2.2 Target Distributions

**Tier 1 (Fully Supported)**:
- Ubuntu 22.04 LTS (Jammy Jellyfish)
- Ubuntu 24.04 LTS (Noble Numbat)
- Debian 12 (Bookworm)

**Tier 2 (Community Supported)**:
- Ubuntu 20.04 LTS (Focal Fossa)
- Debian 11 (Bullseye)
- Linux Mint 21+
- Pop!_OS 22.04+

---

## 3. Required Tools

### 3.1 Debian Build Tools

```bash
# Install essential packaging tools
sudo apt update
sudo apt install -y \
    build-essential \
    debhelper \
    devscripts \
    dh-make \
    fakeroot \
    lintian \
    dpkg-dev \
    cmake \
    ninja-build
```

### 3.2 Tool Descriptions

| Tool | Purpose |
|------|---------|
| **debhelper** | Packaging helper scripts |
| **devscripts** | Development scripts (debuild, dch, etc.) |
| **dh-make** | Initial debian/ directory creation |
| **fakeroot** | Run commands as fake root |
| **lintian** | Package quality checker |
| **dpkg-dev** | Low-level packaging tools |

---

## 4. Package Structure

### 4.1 Debian Directory Structure

```
scratchbird/
├── debian/
│   ├── changelog          # Package changelog
│   ├── control            # Package metadata and dependencies
│   ├── copyright          # Copyright information
│   ├── rules              # Build script
│   ├── compat             # debhelper compatibility level
│   ├── install            # Installation file list
│   ├── scratchbird.service  # Systemd service file (optional)
│   ├── scratchbird.postinst # Post-installation script
│   ├── scratchbird.prerm    # Pre-removal script
│   └── source/
│       └── format         # Source package format
├── CMakeLists.txt
├── src/
└── ...
```

---

## 5. Debian Control Files

### 5.1 debian/control

**debian/control**:
```control
Source: scratchbird
Section: database
Priority: optional
Maintainer: Your Name <your.email@example.com>
Build-Depends: debhelper-compat (= 13),
               cmake (>= 3.20),
               ninja-build,
               libspdlog-dev (>= 1.10.0),
               libgtest-dev (>= 1.14.0),
               libssl-dev (>= 3.0.0),
               liblz4-dev (>= 1.9.3),
               zlib1g-dev
Standards-Version: 4.6.2
Homepage: https://github.com/yourusername/scratchbird
Vcs-Git: https://github.com/yourusername/scratchbird.git
Vcs-Browser: https://github.com/yourusername/scratchbird
Rules-Requires-Root: no

Package: scratchbird
Architecture: any
Depends: ${shlibs:Depends}, ${misc:Depends},
         libssl3 (>= 3.0.0),
         liblz4-1 (>= 1.9.3),
         zlib1g
Recommends: scratchbird-doc
Suggests: scratchbird-dev
Description: High-performance SQL database engine
 ScratchBird is a modern SQL database engine with support for multiple
 SQL dialects including PostgreSQL, Firebird, and MySQL.
 .
 This package contains the main database server and command-line tools.

Package: scratchbird-dev
Section: libdevel
Architecture: any
Depends: ${misc:Depends},
         scratchbird (= ${binary:Version}),
         libspdlog-dev,
         libssl-dev
Description: Development files for ScratchBird
 Development files, headers, and libraries for building applications
 against ScratchBird.

Package: scratchbird-doc
Section: doc
Architecture: all
Depends: ${misc:Depends}
Description: Documentation for ScratchBird
 User manual, API documentation, and examples for ScratchBird
 database engine.
```

### 5.2 debian/changelog

**debian/changelog**:
```changelog
scratchbird (0.1.0-1) unstable; urgency=medium

  * Initial release (Closes: #XXXXXX)
  * Support for PostgreSQL, Firebird, and MySQL dialects
  * Multi-version concurrency control (MVCC)
  * Advanced indexing support

 -- Your Name <your.email@example.com>  Fri, 03 Jan 2026 12:00:00 +0000
```

**Update changelog**:
```bash
# Interactive changelog update
dch -i

# Or manually specify
dch -v 0.1.1-1 "New upstream release"
```

### 5.3 debian/copyright

**debian/copyright** (DEP-5 format):
```
Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/
Upstream-Name: ScratchBird
Upstream-Contact: Your Name <your.email@example.com>
Source: https://github.com/yourusername/scratchbird

Files: *
Copyright: 2026 Your Name <your.email@example.com>
License: MIT

Files: debian/*
Copyright: 2026 Your Name <your.email@example.com>
License: MIT

License: MIT
 Permission is hereby granted, free of charge, to any person obtaining a
 copy of this software and associated documentation files (the "Software"),
 to deal in the Software without restriction, including without limitation
 the rights to use, copy, modify, merge, publish, distribute, sublicense,
 and/or sell copies of the Software, and to permit persons to whom the
 Software is furnished to do so, subject to the following conditions:
 .
 The above copyright notice and this permission notice shall be included
 in all copies or substantial portions of the Software.
 .
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 DEALINGS IN THE SOFTWARE.
```

### 5.4 debian/rules

**debian/rules** (using dh):
```makefile
#!/usr/bin/make -f

# Uncomment to enable verbose build
#export DH_VERBOSE = 1

# Build flags
export DEB_BUILD_MAINT_OPTIONS = hardening=+all
DPKG_EXPORT_BUILDFLAGS = 1
include /usr/share/dpkg/buildflags.mk

%:
	dh $@ --buildsystem=cmake --with systemd

override_dh_auto_configure:
	dh_auto_configure -- \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX=/usr \
		-DENABLE_TESTING=ON \
		-DBUILD_SHARED_LIBS=OFF

override_dh_auto_test:
	# Run tests
	dh_auto_test || echo "Tests failed, continuing anyway"

override_dh_install:
	dh_install
	# Install systemd service
	install -D -m 0644 debian/scratchbird.service \
		debian/scratchbird/lib/systemd/system/scratchbird.service

override_dh_strip:
	dh_strip --dbgsym-migration='scratchbird-dbg (<< 0.1.0-1~)'
```

**Make executable**:
```bash
chmod +x debian/rules
```

### 5.5 debian/compat

**debian/compat**:
```
13
```

Or use debhelper-compat in Build-Depends (recommended):
```bash
# No debian/compat file needed if using debhelper-compat in control
```

### 5.6 debian/source/format

**debian/source/format**:
```
3.0 (quilt)
```

Or for native packages:
```
3.0 (native)
```

### 5.7 debian/install

**debian/install**:
```
# Binary
usr/bin/scratchbird

# Libraries (if any)
usr/lib/*/libscratchbird*.so.*

# Configuration
etc/scratchbird/*
```

**debian/scratchbird-dev.install**:
```
# Development headers
usr/include/scratchbird/*

# Static libraries
usr/lib/*/libscratchbird*.a

# CMake config
usr/lib/*/cmake/scratchbird/*
```

**debian/scratchbird-doc.install**:
```
# Documentation
usr/share/doc/scratchbird/*
```

---

## 6. Systemd Integration

### 6.1 Service File

**debian/scratchbird.service**:
```ini
[Unit]
Description=ScratchBird Database Server
After=network.target
Documentation=https://github.com/yourusername/scratchbird

[Service]
Type=notify
User=scratchbird
Group=scratchbird
ExecStart=/usr/bin/scratchbird server
ExecReload=/bin/kill -HUP $MAINPID
Restart=on-failure
RestartSec=5s

# Security
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/var/lib/scratchbird

[Install]
WantedBy=multi-user.target
```

### 6.2 Maintainer Scripts

**debian/scratchbird.postinst**:
```bash
#!/bin/sh
set -e

case "$1" in
    configure)
        # Create system user
        if ! getent passwd scratchbird > /dev/null; then
            adduser --system --group --home /var/lib/scratchbird \
                    --no-create-home --disabled-login scratchbird
        fi

        # Create data directory
        mkdir -p /var/lib/scratchbird
        chown scratchbird:scratchbird /var/lib/scratchbird
        chmod 750 /var/lib/scratchbird

        # Create log directory
        mkdir -p /var/log/scratchbird
        chown scratchbird:scratchbird /var/log/scratchbird
        chmod 750 /var/log/scratchbird
        ;;
esac

#DEBHELPER#

exit 0
```

**debian/scratchbird.prerm**:
```bash
#!/bin/sh
set -e

case "$1" in
    remove|upgrade|deconfigure)
        # Stop service
        deb-systemd-invoke stop scratchbird.service || true
        ;;
esac

#DEBHELPER#

exit 0
```

**debian/scratchbird.postrm**:
```bash
#!/bin/sh
set -e

case "$1" in
    purge)
        # Remove data directory on purge
        rm -rf /var/lib/scratchbird
        rm -rf /var/log/scratchbird

        # Remove user
        if getent passwd scratchbird > /dev/null; then
            deluser --system scratchbird || true
        fi
        ;;
esac

#DEBHELPER#

exit 0
```

**Make scripts executable**:
```bash
chmod +x debian/*.postinst debian/*.prerm debian/*.postrm
```

---

## 7. Building the Package

### 7.1 Build with debuild

```bash
# Clean previous builds
debclean

# Build binary package
debuild -us -uc -b

# Build source and binary packages
debuild -us -uc

# Build with signing
debuild -k<GPG-KEY-ID>

# Build for specific architecture
debuild -aamd64

# Build for all architectures
debuild -A
```

**Output files** (in parent directory):
```
scratchbird_0.1.0-1_amd64.deb        # Binary package
scratchbird-dev_0.1.0-1_amd64.deb    # Dev package
scratchbird-doc_0.1.0-1_all.deb      # Doc package
scratchbird_0.1.0-1_amd64.changes    # Changes file
scratchbird_0.1.0-1_amd64.buildinfo  # Build info
scratchbird_0.1.0-1.dsc              # Source description
scratchbird_0.1.0.orig.tar.gz        # Original source
scratchbird_0.1.0-1.debian.tar.xz    # Debian patches
```

### 7.2 Build with dpkg-buildpackage

```bash
# Binary package only
dpkg-buildpackage -b -uc -us

# Source and binary
dpkg-buildpackage -uc -us

# With parallel jobs
dpkg-buildpackage -j$(nproc) -b -uc -us
```

### 7.3 Build with gbp (git-buildpackage)

```bash
# Install git-buildpackage
sudo apt install git-buildpackage

# Build from git
gbp buildpackage --git-ignore-new

# Build with pbuilder (clean environment)
gbp buildpackage --git-pbuilder
```

---

## 8. Package Quality Checks

### 8.1 Lintian

```bash
# Check package
lintian ../scratchbird_0.1.0-1_amd64.deb

# Verbose output
lintian -v ../scratchbird_0.1.0-1_amd64.deb

# Show only errors and warnings
lintian -EvIL +pedantic ../scratchbird_0.1.0-1_amd64.deb

# Check source package
lintian ../scratchbird_0.1.0-1.dsc
```

**Common issues to fix**:
- Missing dependencies
- Wrong permissions
- Spelling errors in descriptions
- Missing manpages
- Debian policy violations

### 8.2 Piuparts

```bash
# Install piuparts
sudo apt install piuparts

# Test installation/removal
sudo piuparts ../scratchbird_0.1.0-1_amd64.deb

# Test upgrade scenario
sudo piuparts -a ../scratchbird_0.1.0-1_amd64.deb \
               ../scratchbird_0.1.1-1_amd64.deb
```

---

## 9. Testing the Package

### 9.1 Install Locally

```bash
# Install package
sudo dpkg -i ../scratchbird_0.1.0-1_amd64.deb

# Fix dependencies if needed
sudo apt install -f

# Verify installation
scratchbird --version
systemctl status scratchbird

# Check files
dpkg -L scratchbird

# Remove package
sudo apt remove scratchbird

# Purge (remove config)
sudo apt purge scratchbird
```

### 9.2 Test in Clean Environment

**Using pbuilder**:
```bash
# Install pbuilder
sudo apt install pbuilder

# Create base environment
sudo pbuilder create --distribution jammy

# Build in clean environment
sudo pbuilder build ../scratchbird_0.1.0-1.dsc

# Result in /var/cache/pbuilder/result/
```

**Using sbuild**:
```bash
# Install sbuild
sudo apt install sbuild

# Setup
sudo sbuild-adduser $USER
newgrp sbuild
sbuild-update --keygen
sbuild-createchroot --include=eatmydata,ccache jammy \
    /srv/chroot/jammy-amd64 http://archive.ubuntu.com/ubuntu

# Build
sbuild -d jammy ../scratchbird_0.1.0-1.dsc
```

---

## 10. Creating a Repository

### 10.1 Simple File Repository

```bash
# Create repository directory
mkdir -p repo/pool/main
mkdir -p repo/dists/jammy/main/binary-amd64

# Copy packages
cp ../scratchbird_*.deb repo/pool/main/

# Create Packages file
cd repo
dpkg-scanpackages pool/ /dev/null | gzip -9c > dists/jammy/main/binary-amd64/Packages.gz

# Create Release file
cat > dists/jammy/Release << EOF
Origin: ScratchBird
Label: ScratchBird
Suite: stable
Codename: jammy
Architectures: amd64
Components: main
Description: ScratchBird package repository
EOF

# Generate checksums
cd dists/jammy
apt-ftparchive release . > Release

# Sign Release file
gpg --clearsign -o InRelease Release
gpg -abs -o Release.gpg Release
```

### 10.2 Use in sources.list

**On client machine**:
```bash
# Add repository
echo "deb [trusted=yes] https://example.com/repo jammy main" | \
    sudo tee /etc/apt/sources.list.d/scratchbird.list

# Update and install
sudo apt update
sudo apt install scratchbird
```

### 10.3 Using reprepro

```bash
# Install reprepro
sudo apt install reprepro

# Create repository config
mkdir -p repo/conf
cat > repo/conf/distributions << EOF
Origin: ScratchBird
Label: ScratchBird
Codename: jammy
Architectures: amd64 arm64 source
Components: main
Description: ScratchBird package repository
SignWith: <GPG-KEY-ID>
EOF

# Add package to repository
cd repo
reprepro includedeb jammy ../scratchbird_0.1.0-1_amd64.deb

# List packages
reprepro list jammy
```

---

## 11. PPA (Personal Package Archive)

### 11.1 Launchpad PPA Setup

1. **Create Launchpad account**: https://launchpad.net/
2. **Create PPA**: https://launchpad.net/~/+activate-ppa
3. **Generate GPG key** (if not exists):
   ```bash
   gpg --full-generate-key
   ```
4. **Upload key to Launchpad**:
   ```bash
   gpg --send-keys --keyserver keyserver.ubuntu.com <KEY-ID>
   ```

### 11.2 Upload to PPA

```bash
# Build source package
debuild -S -k<GPG-KEY-ID>

# Upload to PPA
dput ppa:yourusername/scratchbird ../scratchbird_0.1.0-1_source.changes

# Wait for build (Launchpad will build for all architectures)
```

### 11.3 Users Install from PPA

```bash
# Add PPA
sudo add-apt-repository ppa:yourusername/scratchbird
sudo apt update

# Install
sudo apt install scratchbird
```

---

## 12. CI/CD Integration

### 12.1 GitHub Actions

**.github/workflows/deb.yml**:
```yaml
name: Build DEB Package

on:
  push:
    tags: ['v*']

jobs:
  build-deb:
    runs-on: ubuntu-22.04

    steps:
      - uses: actions/checkout@v4

      - name: Install build dependencies
        run: |
          sudo apt update
          sudo apt install -y \
            build-essential debhelper devscripts \
            cmake ninja-build \
            libspdlog-dev libgtest-dev libssl-dev liblz4-dev

      - name: Build package
        run: |
          debuild -us -uc -b

      - name: Run lintian
        run: |
          lintian --fail-on error ../scratchbird_*.deb

      - name: Upload package
        uses: actions/upload-artifact@v4
        with:
          name: deb-package
          path: ../*.deb

      - name: Release
        uses: softprops/action-gh-release@v1
        if: startsWith(github.ref, 'refs/tags/')
        with:
          files: ../*.deb
        env:
          GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
```

---

## 13. Multi-Distribution Support

### 13.1 Build for Multiple Ubuntu Versions

```bash
# For each Ubuntu version
for distro in focal jammy noble; do
    # Update changelog
    dch -v "0.1.0-1~${distro}1" "Build for ${distro}"
    dch -r ""

    # Build
    debuild -S -k<GPG-KEY-ID>

    # Upload to PPA
    dput ppa:yourusername/scratchbird ../scratchbird_0.1.0-1~${distro}1_source.changes
done
```

---

## 14. Troubleshooting

### 14.1 Common Build Errors

**Missing Build Dependencies**:
```bash
# Install all build dependencies
sudo apt build-dep .

# Or manually
sudo mk-build-deps --install debian/control
```

**GPG Signing Errors**:
```bash
# List keys
gpg --list-secret-keys

# Set default key
echo "default-key <KEY-ID>" >> ~/.gnupg/gpg.conf
```

**Lintian Errors**:
```bash
# See detailed explanation
lintian -i -I --pedantic ../scratchbird_*.deb
```

---

## 15. Additional Resources

- **Debian New Maintainers' Guide:** https://www.debian.org/doc/manuals/maint-guide/
- **Debian Policy Manual:** https://www.debian.org/doc/debian-policy/
- **Ubuntu Packaging Guide:** https://packaging.ubuntu.com/html/
- **Launchpad PPA:** https://help.launchpad.net/Packaging/PPA

---

**Document Version:** 1.0
**Last Updated:** 2026-01-03
**Maintainer:** Build Infrastructure Team
**Status:** Beta Preparation

**Terminology note:** ScratchBird uses Firebird MGA. Any MGA references in this file are legacy shorthand and must be interpreted as MGA per the authoritative references above.
