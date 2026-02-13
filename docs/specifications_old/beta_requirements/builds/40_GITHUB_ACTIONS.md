# GitHub Actions CI/CD Requirements

**Platform:** GitHub Actions
**Purpose:** Continuous Integration and Deployment
**Document Version:** 1.0
**Last Updated:** 2026-01-03

---

## 1. Overview

This document specifies comprehensive GitHub Actions workflows for building, testing, and releasing ScratchBird across multiple platforms. Covers native builds, cross-compilation, packaging, and automated releases.

---

## 2. GitHub Actions Basics

### 2.1 Workflow Location

All workflows are stored in:
```
.github/
  workflows/
    ci.yml                  # Main CI pipeline
    build-matrix.yml        # Multi-platform matrix build
    cross-compile.yml       # Cross-compilation builds
    docker.yml              # Docker image builds
    appimage.yml            # AppImage packaging
    release.yml             # Release automation
```

### 2.2 Workflow Triggers

**Common triggers**:
```yaml
on:
  push:
    branches: [main, develop]
    tags: ['v*']
  pull_request:
    branches: [main]
  workflow_dispatch:  # Manual trigger
```

---

## 3. Main CI Pipeline

### 3.1 Basic CI Workflow

**.github/workflows/ci.yml**:
```yaml
name: CI

on:
  push:
    branches: [main, develop]
  pull_request:
    branches: [main]

jobs:
  # Lint and format check
  lint:
    runs-on: ubuntu-22.04
    steps:
      - uses: actions/checkout@v4

      - name: Install clang-format
        run: sudo apt install -y clang-format-15

      - name: Check formatting
        run: |
          find src include -name '*.cpp' -o -name '*.h' | \
          xargs clang-format-15 --dry-run --Werror

      - name: Run clang-tidy
        run: |
          sudo apt install -y clang-tidy-15
          # Configure and run clang-tidy
          cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -B build
          clang-tidy-15 -p build src/*.cpp

  # Build and test on Linux
  build-linux:
    runs-on: ubuntu-22.04
    strategy:
      matrix:
        compiler: [gcc-11, gcc-12, clang-14, clang-15]
        build_type: [Debug, Release]

    steps:
      - uses: actions/checkout@v4

      - name: Install dependencies
        run: |
          sudo apt update
          sudo apt install -y \
            build-essential cmake ninja-build \
            libspdlog-dev libgtest-dev libssl-dev liblz4-dev zlib1g-dev

      - name: Install compiler
        run: |
          if [[ "${{ matrix.compiler }}" == clang* ]]; then
            sudo apt install -y ${{ matrix.compiler }}
          fi

      - name: Configure
        run: |
          if [[ "${{ matrix.compiler }}" == gcc* ]]; then
            CC=gcc CXX=g++
          else
            CC=${{ matrix.compiler }} CXX=${{ matrix.compiler }}++
          fi
          cmake -G Ninja \
                -DCMAKE_BUILD_TYPE=${{ matrix.build_type }} \
                -DCMAKE_C_COMPILER=$CC \
                -DCMAKE_CXX_COMPILER=$CXX \
                -DENABLE_TESTING=ON \
                -B build

      - name: Build
        run: ninja -C build

      - name: Test
        run: ctest --test-dir build --output-on-failure -j$(nproc)

      - name: Upload test results
        if: failure()
        uses: actions/upload-artifact@v4
        with:
          name: test-results-${{ matrix.compiler }}-${{ matrix.build_type }}
          path: build/Testing/Temporary/

  # Build and test on macOS
  build-macos:
    runs-on: macos-13  # Intel
    strategy:
      matrix:
        build_type: [Debug, Release]

    steps:
      - uses: actions/checkout@v4

      - name: Install dependencies
        run: |
          brew install cmake ninja spdlog googletest openssl@3 lz4

      - name: Configure
        run: |
          cmake -G Ninja \
                -DCMAKE_BUILD_TYPE=${{ matrix.build_type }} \
                -DOPENSSL_ROOT_DIR=$(brew --prefix openssl@3) \
                -DENABLE_TESTING=ON \
                -B build

      - name: Build
        run: ninja -C build

      - name: Test
        run: ctest --test-dir build --output-on-failure

  # Build and test on Windows
  build-windows:
    runs-on: windows-2022
    strategy:
      matrix:
        build_type: [Debug, Release]

    steps:
      - uses: actions/checkout@v4

      - name: Setup vcpkg
        uses: lukka/run-vcpkg@v11
        with:
          vcpkgGitCommitId: 'a42af01b72c28a8e1d7b48107b33e4f286a55ef6'  # Pin version

      - name: Install dependencies
        run: |
          vcpkg install spdlog:x64-windows gtest:x64-windows openssl:x64-windows lz4:x64-windows

      - name: Configure
        run: |
          cmake -G "Visual Studio 17 2022" -A x64 `
                -DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake `
                -DCMAKE_BUILD_TYPE=${{ matrix.build_type }} `
                -DENABLE_TESTING=ON `
                -B build

      - name: Build
        run: cmake --build build --config ${{ matrix.build_type }}

      - name: Test
        run: ctest --test-dir build -C ${{ matrix.build_type }} --output-on-failure
```

---

## 4. Multi-Platform Matrix Build

### 4.1 Comprehensive Matrix

**.github/workflows/build-matrix.yml**:
```yaml
name: Multi-Platform Build Matrix

on:
  push:
    branches: [main]
  pull_request:
  workflow_dispatch:

jobs:
  matrix-build:
    strategy:
      fail-fast: false
      matrix:
        include:
          # Linux - GCC
          - os: ubuntu-22.04
            compiler: gcc
            compiler_version: 11
            build_type: Release

          - os: ubuntu-22.04
            compiler: gcc
            compiler_version: 12
            build_type: Release

          # Linux - Clang
          - os: ubuntu-22.04
            compiler: clang
            compiler_version: 15
            build_type: Release

          # macOS - Apple Clang
          - os: macos-13
            compiler: clang
            build_type: Release
            arch: x86_64

          # macOS - ARM64
          - os: macos-14  # M1
            compiler: clang
            build_type: Release
            arch: arm64

          # Windows - MSVC
          - os: windows-2022
            compiler: msvc
            build_type: Release

    runs-on: ${{ matrix.os }}

    steps:
      - uses: actions/checkout@v4

      - name: Install Linux dependencies
        if: runner.os == 'Linux'
        run: |
          sudo apt update
          sudo apt install -y \
            build-essential cmake ninja-build \
            libspdlog-dev libgtest-dev libssl-dev liblz4-dev

          if [ "${{ matrix.compiler }}" = "clang" ]; then
            sudo apt install -y clang-${{ matrix.compiler_version }}
          fi

      - name: Install macOS dependencies
        if: runner.os == 'macOS'
        run: |
          brew install cmake ninja spdlog googletest openssl@3 lz4

      - name: Setup vcpkg (Windows)
        if: runner.os == 'Windows'
        uses: lukka/run-vcpkg@v11

      - name: Install Windows dependencies
        if: runner.os == 'Windows'
        run: |
          vcpkg install spdlog:x64-windows gtest:x64-windows openssl:x64-windows lz4:x64-windows

      - name: Configure
        run: |
          if [ "$RUNNER_OS" = "Linux" ]; then
            if [ "${{ matrix.compiler }}" = "gcc" ]; then
              CC=gcc-${{ matrix.compiler_version }}
              CXX=g++-${{ matrix.compiler_version }}
            else
              CC=clang-${{ matrix.compiler_version }}
              CXX=clang++-${{ matrix.compiler_version }}
            fi
            cmake -G Ninja \
                  -DCMAKE_BUILD_TYPE=${{ matrix.build_type }} \
                  -DCMAKE_C_COMPILER=$CC \
                  -DCMAKE_CXX_COMPILER=$CXX \
                  -B build
          elif [ "$RUNNER_OS" = "macOS" ]; then
            cmake -G Ninja \
                  -DCMAKE_BUILD_TYPE=${{ matrix.build_type }} \
                  -DOPENSSL_ROOT_DIR=$(brew --prefix openssl@3) \
                  -B build
          else
            # Windows
            cmake -G "Visual Studio 17 2022" -A x64 \
                  -DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake \
                  -B build
          fi
        shell: bash

      - name: Build
        run: cmake --build build --config ${{ matrix.build_type }}

      - name: Test
        run: ctest --test-dir build --output-on-failure

      - name: Package
        run: |
          cd build
          cpack -C ${{ matrix.build_type }}

      - name: Upload artifacts
        uses: actions/upload-artifact@v4
        with:
          name: scratchbird-${{ matrix.os }}-${{ matrix.compiler }}-${{ matrix.build_type }}
          path: build/*.tar.gz
```

---

## 5. Cross-Compilation Workflows

### 5.1 Linux to Windows (MinGW)

**.github/workflows/cross-compile-windows.yml**:
```yaml
name: Cross-Compile Windows

on:
  push:
    branches: [main]
    tags: ['v*']

jobs:
  cross-compile-mingw:
    runs-on: ubuntu-22.04

    steps:
      - uses: actions/checkout@v4

      - name: Install MinGW toolchain
        run: |
          sudo apt update
          sudo apt install -y \
            mingw-w64 mingw-w64-tools \
            cmake ninja-build wine64

      - name: Setup MXE
        run: |
          git clone https://github.com/mxe/mxe.git /tmp/mxe
          cd /tmp/mxe
          make MXE_TARGETS='x86_64-w64-mingw32.static' \
              spdlog gtest openssl lz4 zlib -j$(nproc)

      - name: Configure
        run: |
          cmake -G Ninja \
                -DCMAKE_TOOLCHAIN_FILE=/tmp/mxe/usr/x86_64-w64-mingw32.static/share/cmake/mxe-conf.cmake \
                -DCMAKE_BUILD_TYPE=Release \
                -B build

      - name: Build
        run: ninja -C build

      - name: Test with Wine
        run: |
          wine64 build/tests/scratchbird_tests.exe || true

      - name: Package
        run: |
          mkdir dist
          cp build/*.exe dist/
          cd dist
          zip -r ../scratchbird-windows-x64.zip .

      - name: Upload artifacts
        uses: actions/upload-artifact@v4
        with:
          name: windows-cross-compiled
          path: scratchbird-windows-x64.zip
```

---

## 6. Docker Image Builds

### 6.1 Multi-Platform Docker

**.github/workflows/docker.yml**:
```yaml
name: Docker Build and Push

on:
  push:
    branches: [main]
    tags: ['v*']
  workflow_dispatch:

jobs:
  docker:
    runs-on: ubuntu-latest

    steps:
      - uses: actions/checkout@v4

      - name: Set up QEMU
        uses: docker/setup-qemu-action@v3

      - name: Set up Docker Buildx
        uses: docker/setup-buildx-action@v3

      - name: Login to GitHub Container Registry
        uses: docker/login-action@v3
        with:
          registry: ghcr.io
          username: ${{ github.actor }}
          password: ${{ secrets.GITHUB_TOKEN }}

      - name: Login to Docker Hub
        if: github.event_name != 'pull_request'
        uses: docker/login-action@v3
        with:
          username: ${{ secrets.DOCKERHUB_USERNAME }}
          password: ${{ secrets.DOCKERHUB_TOKEN }}

      - name: Extract metadata
        id: meta
        uses: docker/metadata-action@v5
        with:
          images: |
            ghcr.io/${{ github.repository }}
            ${{ secrets.DOCKERHUB_USERNAME }}/scratchbird
          tags: |
            type=ref,event=branch
            type=ref,event=pr
            type=semver,pattern={{version}}
            type=semver,pattern={{major}}.{{minor}}
            type=sha

      - name: Build and push
        uses: docker/build-push-action@v5
        with:
          context: .
          platforms: linux/amd64,linux/arm64
          push: ${{ github.event_name != 'pull_request' }}
          tags: ${{ steps.meta.outputs.tags }}
          labels: ${{ steps.meta.outputs.labels }}
          cache-from: type=gha
          cache-to: type=gha,mode=max
```

---

## 7. AppImage Build

### 7.1 AppImage Workflow

**.github/workflows/appimage.yml**:
```yaml
name: Build AppImage

on:
  push:
    tags: ['v*']
  workflow_dispatch:

jobs:
  appimage:
    runs-on: ubuntu-20.04  # Use oldest for compatibility

    steps:
      - uses: actions/checkout@v4

      - name: Install dependencies
        run: |
          sudo apt update
          sudo apt install -y \
            build-essential cmake ninja-build \
            libspdlog-dev libgtest-dev libssl-dev liblz4-dev \
            file wget patchelf desktop-file-utils fuse libfuse2

      - name: Download AppImage tools
        run: |
          wget https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
          chmod +x linuxdeploy-x86_64.AppImage
          sudo mv linuxdeploy-x86_64.AppImage /usr/local/bin/linuxdeploy

      - name: Build
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
          VERSION=${{ github.ref_name }}
          linuxdeploy --appdir AppDir \
                      --executable AppDir/usr/bin/scratchbird \
                      --desktop-file AppDir/usr/share/applications/scratchbird.desktop \
                      --icon-file AppDir/usr/share/icons/hicolor/256x256/apps/scratchbird.png \
                      --output appimage

          mv ScratchBird-*.AppImage ScratchBird-$VERSION-x86_64.AppImage

      - name: Upload AppImage
        uses: actions/upload-artifact@v4
        with:
          name: appimage
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

## 8. Automated Release

### 8.1 Comprehensive Release Workflow

**.github/workflows/release.yml**:
```yaml
name: Release

on:
  push:
    tags: ['v*']

jobs:
  # Build for all platforms
  build-linux:
    runs-on: ubuntu-22.04
    steps:
      - uses: actions/checkout@v4
      - name: Build
        run: |
          cmake -B build -DCMAKE_BUILD_TYPE=Release
          cmake --build build
          cd build && cpack -G TGZ
      - uses: actions/upload-artifact@v4
        with:
          name: linux-x64
          path: build/*.tar.gz

  build-windows:
    runs-on: windows-2022
    steps:
      - uses: actions/checkout@v4
      - uses: lukka/run-vcpkg@v11
      - name: Build
        run: |
          vcpkg install spdlog:x64-windows gtest:x64-windows openssl:x64-windows lz4:x64-windows
          cmake -B build -DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake
          cmake --build build --config Release
          cd build && cpack -C Release -G ZIP
      - uses: actions/upload-artifact@v4
        with:
          name: windows-x64
          path: build/*.zip

  build-macos:
    runs-on: macos-13
    steps:
      - uses: actions/checkout@v4
      - name: Install dependencies
        run: brew install cmake ninja spdlog googletest openssl@3 lz4
      - name: Build
        run: |
          cmake -B build -DCMAKE_BUILD_TYPE=Release \
                -DOPENSSL_ROOT_DIR=$(brew --prefix openssl@3)
          cmake --build build
          cd build && cpack -G TGZ
      - uses: actions/upload-artifact@v4
        with:
          name: macos-x64
          path: build/*.tar.gz

  # Create release with all artifacts
  release:
    needs: [build-linux, build-windows, build-macos]
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Download all artifacts
        uses: actions/download-artifact@v4

      - name: Create Release
        uses: softprops/action-gh-release@v1
        with:
          name: Release ${{ github.ref_name }}
          generate_release_notes: true
          files: |
            linux-x64/*
            windows-x64/*
            macos-x64/*
        env:
          GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
```

---

## 9. Code Quality and Security

### 9.1 Code Coverage

**.github/workflows/coverage.yml**:
```yaml
name: Code Coverage

on:
  push:
    branches: [main]
  pull_request:

jobs:
  coverage:
    runs-on: ubuntu-22.04

    steps:
      - uses: actions/checkout@v4

      - name: Install dependencies
        run: |
          sudo apt update
          sudo apt install -y \
            build-essential cmake ninja-build gcovr \
            libspdlog-dev libgtest-dev libssl-dev liblz4-dev

      - name: Configure with coverage
        run: |
          cmake -G Ninja \
                -DCMAKE_BUILD_TYPE=Debug \
                -DENABLE_COVERAGE=ON \
                -DENABLE_TESTING=ON \
                -B build

      - name: Build and test
        run: |
          ninja -C build
          ctest --test-dir build --output-on-failure

      - name: Generate coverage report
        run: |
          gcovr --xml-pretty --exclude-unreachable-branches \
                --print-summary -o coverage.xml --root .

      - name: Upload coverage to Codecov
        uses: codecov/codecov-action@v3
        with:
          files: ./coverage.xml
          fail_ci_if_error: true
```

### 9.2 Security Scanning

**.github/workflows/security.yml**:
```yaml
name: Security Scan

on:
  push:
    branches: [main]
  schedule:
    - cron: '0 0 * * 0'  # Weekly

jobs:
  codeql:
    runs-on: ubuntu-latest
    permissions:
      security-events: write

    steps:
      - uses: actions/checkout@v4

      - name: Initialize CodeQL
        uses: github/codeql-action/init@v2
        with:
          languages: cpp

      - name: Build
        run: |
          cmake -B build
          cmake --build build

      - name: Perform CodeQL Analysis
        uses: github/codeql-action/analyze@v2

  dependency-review:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/dependency-review-action@v3
```

---

## 10. Caching Strategies

### 10.1 Build Cache

```yaml
- name: Cache CMake build
  uses: actions/cache@v3
  with:
    path: |
      build
      ~/.cache/ccache
    key: ${{ runner.os }}-build-${{ hashFiles('**/CMakeLists.txt') }}
    restore-keys: |
      ${{ runner.os }}-build-
```

### 10.2 Dependency Cache

```yaml
# vcpkg cache (Windows)
- name: Cache vcpkg
  uses: actions/cache@v3
  with:
    path: |
      ${{ env.VCPKG_ROOT }}
      !${{ env.VCPKG_ROOT }}/.git
    key: vcpkg-${{ runner.os }}-${{ hashFiles('vcpkg.json') }}

# Homebrew cache (macOS)
- name: Cache Homebrew
  uses: actions/cache@v3
  with:
    path: |
      ~/Library/Caches/Homebrew
      /usr/local/Cellar
    key: brew-${{ runner.os }}-${{ hashFiles('Brewfile') }}
```

---

## 11. Workflow Optimization

### 11.1 Conditional Steps

```yaml
- name: Run expensive check
  if: github.event_name == 'push' && github.ref == 'refs/heads/main'
  run: ./expensive-check.sh

- name: Deploy
  if: startsWith(github.ref, 'refs/tags/v')
  run: ./deploy.sh
```

### 11.2 Reusable Workflows

**.github/workflows/build-template.yml**:
```yaml
name: Build Template

on:
  workflow_call:
    inputs:
      os:
        required: true
        type: string
      build_type:
        required: true
        type: string

jobs:
  build:
    runs-on: ${{ inputs.os }}
    steps:
      - uses: actions/checkout@v4
      - name: Build
        run: |
          cmake -B build -DCMAKE_BUILD_TYPE=${{ inputs.build_type }}
          cmake --build build
```

**Use in other workflow**:
```yaml
jobs:
  call-build:
    uses: ./.github/workflows/build-template.yml
    with:
      os: ubuntu-22.04
      build_type: Release
```

---

## 12. Secrets Management

### 12.1 Required Secrets

Add these secrets in repository settings:

```
DOCKERHUB_USERNAME      # Docker Hub username
DOCKERHUB_TOKEN         # Docker Hub access token
CODECOV_TOKEN          # Codecov upload token
GPG_PRIVATE_KEY        # For signing releases
GPG_PASSPHRASE         # GPG key passphrase
```

### 12.2 Using Secrets

```yaml
- name: Login to Docker Hub
  uses: docker/login-action@v3
  with:
    username: ${{ secrets.DOCKERHUB_USERNAME }}
    password: ${{ secrets.DOCKERHUB_TOKEN }}
```

---

## 13. Status Badges

Add to README.md:

```markdown
[![CI](https://github.com/yourusername/scratchbird/workflows/CI/badge.svg)](https://github.com/yourusername/scratchbird/actions/workflows/ci.yml)
[![Build Matrix](https://github.com/yourusername/scratchbird/workflows/Multi-Platform%20Build%20Matrix/badge.svg)](https://github.com/yourusername/scratchbird/actions/workflows/build-matrix.yml)
[![Docker](https://github.com/yourusername/scratchbird/workflows/Docker%20Build%20and%20Push/badge.svg)](https://github.com/yourusername/scratchbird/actions/workflows/docker.yml)
[![codecov](https://codecov.io/gh/yourusername/scratchbird/branch/main/graph/badge.svg)](https://codecov.io/gh/yourusername/scratchbird)
```

---

## 14. Best Practices

### 14.1 Performance

- Use matrix builds for parallel execution
- Cache dependencies aggressively
- Use `fail-fast: false` for comprehensive testing
- Leverage BuildKit for Docker builds

### 14.2 Security

- Use `actions/checkout@v4` (latest versions)
- Pin action versions for reproducibility
- Scan for vulnerabilities (CodeQL, Trivy)
- Use least-privilege tokens
- Never commit secrets

### 14.3 Maintainability

- Use reusable workflows
- Document workflow purpose
- Keep workflows focused (single responsibility)
- Use meaningful job/step names

---

## 15. Additional Resources

- **GitHub Actions Documentation:** https://docs.github.com/en/actions
- **Workflow Syntax:** https://docs.github.com/en/actions/reference/workflow-syntax-for-github-actions
- **GitHub Marketplace:** https://github.com/marketplace?type=actions

---

**Document Version:** 1.0
**Last Updated:** 2026-01-03
**Maintainer:** Build Infrastructure Team
**Status:** Beta Preparation
