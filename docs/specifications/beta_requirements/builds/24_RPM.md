# RPM Package Requirements

**Package Format:** .rpm (Red Hat Package Manager)
**Target Platforms:** Fedora, RHEL, Rocky Linux, AlmaLinux, openSUSE
**Document Version:** 1.0
**Last Updated:** 2026-01-03

---

## 1. Overview

This document specifies all requirements for creating RPM packages for ScratchBird. RPM is the native package format for Red Hat-based distributions including Fedora, RHEL, Rocky Linux, AlmaLinux, and openSUSE.

**RPM Package Advantages**:
- Native format for Red Hat ecosystem
- Dependency management via DNF/YUM
- Digital signatures and GPG verification
- System integration
- Enterprise distribution support

---

## 2. System Requirements

### 2.1 Build System Requirements

| Component | Requirement |
|-----------|-------------|
| **OS** | Fedora 38+, RHEL 9+, Rocky Linux 9+ |
| **Architecture** | x86_64 or aarch64 |
| **RAM** | 8 GB |
| **Disk Space** | 10 GB free |

### 2.2 Target Distributions

**Tier 1 (Fully Supported)**:
- Fedora 38, 39, 40
- RHEL 9.x
- Rocky Linux 9.x
- AlmaLinux 9.x

**Tier 2 (Community Supported)**:
- openSUSE Leap 15.5+
- openSUSE Tumbleweed
- CentOS Stream 9

---

## 3. Required Tools

### 3.1 Fedora / RHEL / Rocky / AlmaLinux

```bash
# Install RPM build tools
sudo dnf install -y \
    rpm-build \
    rpmdevtools \
    rpmlint \
    mock \
    cmake \
    ninja-build \
    gcc \
    gcc-c++ \
    make

# Install build dependencies
sudo dnf install -y \
    spdlog-devel \
    gtest-devel \
    openssl-devel \
    lz4-devel \
    zlib-devel

# Create RPM build environment
rpmdev-setuptree
```

### 3.2 openSUSE

```bash
# Install build tools
sudo zypper install -y \
    rpm-build \
    rpmdevtools \
    rpmlint \
    cmake \
    ninja \
    gcc \
    gcc-c++

# Install build dependencies
sudo zypper install -y \
    spdlog-devel \
    gtest \
    libopenssl-devel \
    liblz4-devel \
    zlib-devel

# Create build environment
rpmdev-setuptree
```

---

## 4. RPM Build Tree

### 4.1 Directory Structure

```
~/rpmbuild/
├── BUILD/       # Build directory (temporary)
├── BUILDROOT/   # Installation root (temporary)
├── RPMS/        # Binary RPMs
│   ├── x86_64/
│   └── noarch/
├── SOURCES/     # Source tarballs and patches
├── SPECS/       # RPM spec files
└── SRPMS/       # Source RPMs
```

**Created by**:
```bash
rpmdev-setuptree
```

---

## 5. Creating Source Tarball

### 5.1 Prepare Source

```bash
# Clone repository
git clone https://github.com/yourusername/ScratchBird.git
cd ScratchBird

# Get version
VERSION=$(git describe --tags --abbrev=0 | sed 's/^v//')

# Create tarball
git archive --format=tar.gz --prefix=scratchbird-${VERSION}/ \
    -o ~/rpmbuild/SOURCES/scratchbird-${VERSION}.tar.gz HEAD

# Or from exported directory
cd ..
tar czf ~/rpmbuild/SOURCES/scratchbird-${VERSION}.tar.gz \
    --transform "s,^ScratchBird,scratchbird-${VERSION}," \
    ScratchBird/
```

---

## 6. RPM Spec File

### 6.1 Complete Spec File

**~/rpmbuild/SPECS/scratchbird.spec**:
```spec
Name:           scratchbird
Version:        0.1.0
Release:        1%{?dist}
Summary:        High-performance SQL database engine

License:        MIT
URL:            https://github.com/yourusername/scratchbird
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake >= 3.20
BuildRequires:  ninja-build
BuildRequires:  gcc
BuildRequires:  gcc-c++
BuildRequires:  spdlog-devel >= 1.10.0
BuildRequires:  gtest-devel >= 1.14.0
BuildRequires:  openssl-devel >= 3.0.0
BuildRequires:  lz4-devel >= 1.9.3
BuildRequires:  zlib-devel
BuildRequires:  systemd-rpm-macros

Requires:       openssl-libs >= 3.0.0
Requires:       lz4-libs >= 1.9.3
Requires:       zlib

%description
ScratchBird is a modern SQL database engine with support for multiple
SQL dialects including PostgreSQL, Firebird, and MySQL.

This package contains the main database server and command-line tools.

%package devel
Summary:        Development files for ScratchBird
Requires:       %{name}%{?_isa} = %{version}-%{release}
Requires:       spdlog-devel
Requires:       openssl-devel

%description devel
Development files, headers, and libraries for building applications
against ScratchBird.

%package doc
Summary:        Documentation for ScratchBird
BuildArch:      noarch

%description doc
User manual, API documentation, and examples for ScratchBird
database engine.

%prep
%autosetup -n %{name}-%{version}

%build
%cmake \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_TESTING=ON \
    -DBUILD_SHARED_LIBS=OFF
%cmake_build

%install
%cmake_install

# Install systemd service
install -D -m 0644 %{_builddir}/%{name}-%{version}/packaging/scratchbird.service \
    %{buildroot}%{_unitdir}/scratchbird.service

# Create directories
install -d -m 0750 %{buildroot}%{_sharedstatedir}/scratchbird
install -d -m 0750 %{buildroot}%{_localstatedir}/log/scratchbird

%check
%ctest

%pre
# Create system user
getent group scratchbird >/dev/null || groupadd -r scratchbird
getent passwd scratchbird >/dev/null || \
    useradd -r -g scratchbird -d %{_sharedstatedir}/scratchbird \
    -s /sbin/nologin -c "ScratchBird Database Server" scratchbird
exit 0

%post
%systemd_post scratchbird.service

%preun
%systemd_preun scratchbird.service

%postun
%systemd_postun_with_restart scratchbird.service

%files
%license LICENSE
%doc README.md CHANGELOG.md
%{_bindir}/scratchbird
%{_unitdir}/scratchbird.service
%dir %attr(0750,scratchbird,scratchbird) %{_sharedstatedir}/scratchbird
%dir %attr(0750,scratchbird,scratchbird) %{_localstatedir}/log/scratchbird
%config(noreplace) %{_sysconfdir}/scratchbird/scratchbird.conf

%files devel
%{_includedir}/scratchbird/
%{_libdir}/libscratchbird*.a
%{_libdir}/cmake/scratchbird/

%files doc
%doc docs/*

%changelog
* Fri Jan 03 2026 Your Name <your.email@example.com> - 0.1.0-1
- Initial package
- Support for PostgreSQL, Firebird, MySQL dialects
- Multi-version concurrency control (MVCC)
- Advanced indexing support
```

### 6.2 Spec File Sections Explained

**Header**:
```spec
Name:           scratchbird        # Package name
Version:        0.1.0             # Upstream version
Release:        1%{?dist}         # Package release (1.fc38, 1.el9, etc.)
Summary:        Short description # One-line summary
```

**Metadata**:
```spec
License:        MIT               # SPDX license identifier
URL:            https://...       # Project homepage
Source0:        %{name}-%{version}.tar.gz  # Source tarball
```

**Dependencies**:
```spec
BuildRequires:  cmake >= 3.20    # Build-time dependencies
Requires:       openssl-libs     # Runtime dependencies
```

**Macros**:
```spec
%{_bindir}              # /usr/bin
%{_libdir}              # /usr/lib64 (on x86_64)
%{_includedir}          # /usr/include
%{_unitdir}             # /usr/lib/systemd/system
%{_sharedstatedir}      # /var/lib
%{_localstatedir}       # /var
%{_sysconfdir}          # /etc
```

---

## 7. Systemd Service File

### 7.1 Service File

**packaging/scratchbird.service**:
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

---

## 8. Building RPM

### 8.1 Build Binary RPM

```bash
# Build from spec file
cd ~/rpmbuild/SPECS
rpmbuild -ba scratchbird.spec

# Build only binary RPM (skip source RPM)
rpmbuild -bb scratchbird.spec

# Build only source RPM
rpmbuild -bs scratchbird.spec

# Build with specific target
rpmbuild -bb --target x86_64 scratchbird.spec
```

**Output**:
```
~/rpmbuild/RPMS/x86_64/scratchbird-0.1.0-1.fc38.x86_64.rpm
~/rpmbuild/RPMS/x86_64/scratchbird-devel-0.1.0-1.fc38.x86_64.rpm
~/rpmbuild/RPMS/noarch/scratchbird-doc-0.1.0-1.fc38.noarch.rpm
~/rpmbuild/SRPMS/scratchbird-0.1.0-1.fc38.src.rpm
```

### 8.2 Build with Mock (Clean Environment)

```bash
# Install mock
sudo dnf install -y mock
sudo usermod -a -G mock $USER
newgrp mock

# List available configurations
mock --list-chroots

# Build for Fedora 38
mock -r fedora-38-x86_64 ~/rpmbuild/SRPMS/scratchbird-0.1.0-1.fc38.src.rpm

# Build for RHEL 9
mock -r rhel-9-x86_64 ~/rpmbuild/SRPMS/scratchbird-0.1.0-1.el9.src.rpm

# Results in /var/lib/mock/<chroot>/result/
```

### 8.3 Parallel Builds

```bash
# Use all CPU cores
rpmbuild -ba --define "_smp_mflags -j$(nproc)" scratchbird.spec

# Or set in spec file
%global _smp_mflags -j%{?_smp_build_ncpus}
```

---

## 9. Package Quality Checks

### 9.1 RPMLint

```bash
# Check spec file
rpmlint ~/rpmbuild/SPECS/scratchbird.spec

# Check source RPM
rpmlint ~/rpmbuild/SRPMS/scratchbird-0.1.0-1.fc38.src.rpm

# Check binary RPM
rpmlint ~/rpmbuild/RPMS/x86_64/scratchbird-0.1.0-1.fc38.x86_64.rpm

# Detailed output
rpmlint -i ~/rpmbuild/RPMS/x86_64/scratchbird-0.1.0-1.fc38.x86_64.rpm
```

**Common warnings to fix**:
- Missing systemd scriptlets
- Incorrect file permissions
- Missing documentation
- Unversioned dependencies
- Hardcoded library paths

### 9.2 Check Dependencies

```bash
# List package dependencies
rpm -qpR ~/rpmbuild/RPMS/x86_64/scratchbird-0.1.0-1.fc38.x86_64.rpm

# List provided capabilities
rpm -qpP ~/rpmbuild/RPMS/x86_64/scratchbird-0.1.0-1.fc38.x86_64.rpm

# List files in package
rpm -qpl ~/rpmbuild/RPMS/x86_64/scratchbird-0.1.0-1.fc38.x86_64.rpm
```

---

## 10. Testing the Package

### 10.1 Install Locally

```bash
# Install RPM
sudo dnf install ~/rpmbuild/RPMS/x86_64/scratchbird-0.1.0-1.fc38.x86_64.rpm

# Or with rpm directly
sudo rpm -ivh ~/rpmbuild/RPMS/x86_64/scratchbird-0.1.0-1.fc38.x86_64.rpm

# Verify installation
scratchbird --version
systemctl status scratchbird

# List installed files
rpm -ql scratchbird

# Remove package
sudo dnf remove scratchbird
```

### 10.2 Test Installation Scripts

```bash
# Check pre-install script
rpm -q --scripts -p scratchbird-0.1.0-1.fc38.x86_64.rpm

# Verbose installation
sudo rpm -ivh --verbose scratchbird-0.1.0-1.fc38.x86_64.rpm
```

---

## 11. Creating a Repository

### 11.1 Simple File Repository

```bash
# Create repository directory
mkdir -p ~/repo/fedora/38/x86_64

# Copy RPMs
cp ~/rpmbuild/RPMS/x86_64/*.rpm ~/repo/fedora/38/x86_64/

# Create repository metadata
createrepo ~/repo/fedora/38/x86_64/

# Update repository
createrepo --update ~/repo/fedora/38/x86_64/
```

### 11.2 Use Repository

**Create repo file** `/etc/yum.repos.d/scratchbird.repo`:
```ini
[scratchbird]
name=ScratchBird Repository
baseurl=https://example.com/repo/fedora/$releasever/$basearch/
enabled=1
gpgcheck=1
gpgkey=https://example.com/repo/RPM-GPG-KEY-scratchbird
```

**Install from repository**:
```bash
sudo dnf install scratchbird
```

### 11.3 Sign Repository

**Create GPG key**:
```bash
# Generate key
gpg --full-generate-key

# Export public key
gpg --export -a 'Your Name' > RPM-GPG-KEY-scratchbird

# Copy to repository
cp RPM-GPG-KEY-scratchbird ~/repo/
```

**Sign RPMs**:
```bash
# Import key to RPM database
rpm --import RPM-GPG-KEY-scratchbird

# Sign RPM
rpm --addsign ~/rpmbuild/RPMS/x86_64/scratchbird-0.1.0-1.fc38.x86_64.rpm

# Or configure automatic signing in ~/.rpmmacros
echo "%_gpg_name Your Name" >> ~/.rpmmacros
```

**Sign repository metadata**:
```bash
# Sign repomd.xml
gpg --detach-sign --armor \
    ~/repo/fedora/38/x86_64/repodata/repomd.xml
```

---

## 12. COPR (Fedora Build Service)

### 12.1 Setup COPR

1. **Create account**: https://copr.fedorainfracloud.org/
2. **Create new project**: https://copr.fedorainfracloud.org/coprs/add/
3. **Configure API token**:
   ```bash
   # Download from: https://copr.fedorainfracloud.org/api/
   mkdir -p ~/.config
   # Save to ~/.config/copr
   ```

### 12.2 Build on COPR

**Method 1: Upload SRPM**:
```bash
# Build source RPM
rpmbuild -bs ~/rpmbuild/SPECS/scratchbird.spec

# Upload to COPR
copr-cli build yourusername/scratchbird \
    ~/rpmbuild/SRPMS/scratchbird-0.1.0-1.fc38.src.rpm
```

**Method 2: Build from Git**:
```bash
# Create COPR project with webhook
# COPR will automatically build on git push
```

### 12.3 Users Install from COPR

```bash
# Enable COPR repository
sudo dnf copr enable yourusername/scratchbird

# Install package
sudo dnf install scratchbird
```

---

## 13. Multi-Distribution Support

### 13.1 Build for Multiple Distributions

**Build script** `build-all.sh`:
```bash
#!/bin/bash

VERSION="0.1.0"
PACKAGE="scratchbird"

# Distributions to build for
DISTS="fedora-38-x86_64 fedora-39-x86_64 epel-9-x86_64"

# Build SRPM
rpmbuild -bs ~/rpmbuild/SPECS/${PACKAGE}.spec

# Build for each distribution
for dist in $DISTS; do
    echo "Building for $dist..."
    mock -r $dist ~/rpmbuild/SRPMS/${PACKAGE}-${VERSION}-1.*.src.rpm

    # Copy results
    mkdir -p ~/repo/$dist
    cp /var/lib/mock/$dist/result/*.rpm ~/repo/$dist/

    # Create repo metadata
    createrepo ~/repo/$dist/
done
```

### 13.2 Conditional Builds

```spec
# Different dependencies for Fedora vs RHEL
%if 0%{?fedora}
BuildRequires: spdlog-devel >= 1.11.0
%endif

%if 0%{?rhel} >= 9
BuildRequires: spdlog-devel >= 1.10.0
%endif

# Different systemd macros
%if 0%{?fedora} || 0%{?rhel} >= 9
%systemd_post scratchbird.service
%else
/bin/systemctl daemon-reload
%endif
```

---

## 14. CI/CD Integration

### 14.1 GitHub Actions

**.github/workflows/rpm.yml**:
```yaml
name: Build RPM Package

on:
  push:
    tags: ['v*']

jobs:
  build-rpm:
    runs-on: ubuntu-latest
    container: fedora:38

    steps:
      - uses: actions/checkout@v4

      - name: Install build tools
        run: |
          dnf install -y \
            rpm-build rpmdevtools rpmlint \
            cmake ninja-build gcc gcc-c++ \
            spdlog-devel gtest-devel openssl-devel lz4-devel

      - name: Setup RPM build tree
        run: rpmdev-setuptree

      - name: Create source tarball
        run: |
          VERSION=$(echo ${{ github.ref_name }} | sed 's/^v//')
          git archive --format=tar.gz --prefix=scratchbird-${VERSION}/ \
              -o ~/rpmbuild/SOURCES/scratchbird-${VERSION}.tar.gz HEAD

      - name: Copy spec file
        run: cp packaging/scratchbird.spec ~/rpmbuild/SPECS/

      - name: Build RPM
        run: rpmbuild -ba ~/rpmbuild/SPECS/scratchbird.spec

      - name: Run rpmlint
        run: rpmlint ~/rpmbuild/RPMS/x86_64/*.rpm || true

      - name: Upload RPMs
        uses: actions/upload-artifact@v4
        with:
          name: rpm-packages
          path: |
            ~/rpmbuild/RPMS/**/*.rpm
            ~/rpmbuild/SRPMS/*.rpm

      - name: Release
        uses: softprops/action-gh-release@v1
        if: startsWith(github.ref, 'refs/tags/')
        with:
          files: |
            /github/home/rpmbuild/RPMS/**/*.rpm
            /github/home/rpmbuild/SRPMS/*.rpm
        env:
          GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
```

---

## 15. Troubleshooting

### 15.1 Common Build Errors

**Missing Build Dependencies**:
```bash
# Install dependencies from spec
sudo dnf builddep ~/rpmbuild/SPECS/scratchbird.spec
```

**Unpackaged Files**:
```
error: Installed (but unpackaged) file(s) found:
   /usr/bin/extra-tool
```
**Solution**: Add to `%files` section or remove during `%install`

**File Conflicts**:
```bash
# Check for conflicts
rpm -qp --conflicts scratchbird-0.1.0-1.fc38.x86_64.rpm
```

### 15.2 Mock Build Issues

```bash
# Clean mock cache
mock --scrub all

# Rebuild chroot
mock -r fedora-38-x86_64 --rebuild

# Verbose build
mock -r fedora-38-x86_64 --verbose scratchbird-0.1.0-1.fc38.src.rpm
```

---

## 16. Best Practices

### 16.1 Packaging Guidelines

- Follow Fedora Packaging Guidelines: https://docs.fedoraproject.org/en-US/packaging-guidelines/
- Use standard RPM macros
- Include systemd integration
- Provide both binary and -devel packages
- Sign all packages with GPG

### 16.2 Naming Conventions

```
package-name-version-release.distribution.architecture.rpm

Examples:
scratchbird-0.1.0-1.fc38.x86_64.rpm
scratchbird-0.1.0-1.el9.aarch64.rpm
scratchbird-doc-0.1.0-1.fc38.noarch.rpm
```

---

## 17. Additional Resources

- **RPM Packaging Guide:** https://rpm-packaging-guide.github.io/
- **Fedora Packaging Guidelines:** https://docs.fedoraproject.org/en-US/packaging-guidelines/
- **COPR Documentation:** https://docs.pagure.org/copr.copr/
- **Mock Documentation:** https://github.com/rpm-software-management/mock/wiki

---

**Document Version:** 1.0
**Last Updated:** 2026-01-03
**Maintainer:** Build Infrastructure Team
**Status:** Beta Preparation
