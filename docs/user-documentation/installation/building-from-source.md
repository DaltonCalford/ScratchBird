# Building from Source
Last modified: 2026-02-22

Compile ScratchBird from source code.

[Back to Installation Index](index.md) | [Back to Documentation Index](../index.md)

---

## When to Build from Source

- You need features not in released packages
- You want to contribute to development
- Your platform lacks prebuilt packages
- You need custom compilation options

---

## Prerequisites

### Minimum Requirements

| Tool | Minimum Version |
|------|-----------------|
| CMake | 3.20 |
| GCC | 9.0 (C++17) |
| Clang | 10.0 (C++17) |
| Make | 4.0 |

### Install Build Dependencies

**Debian/Ubuntu:**
```bash
sudo apt update
sudo apt install \
    build-essential \
    cmake \
    pkg-config \
    git \
    libssl-dev \
    liblz4-dev \
    libgeos-dev \
    libproj-dev \
    libxml2-dev
```

**RHEL/Fedora:**
```bash
sudo dnf install \
    gcc-c++ \
    cmake \
    pkg-config \
    git \
    openssl-devel \
    lz4-devel \
    geos-devel \
    proj-devel \
    libxml2-devel
```

**Arch Linux:**
```bash
sudo pacman -S \
    base-devel \
    cmake \
    git \
    openssl \
    lz4 \
    geos \
    proj \
    libxml2
```

**macOS (Homebrew):**
```bash
brew install \
    cmake \
    openssl@3 \
    lz4 \
    geos \
    proj \
    libxml2
```

---

## Quick Build

```bash
# Clone repository
git clone https://github.com/DaltonCalford/ScratchBird.git
cd ScratchBird

# Create build directory
mkdir build && cd build

# Configure
cmake ..

# Build
make -j$(nproc)

# Run tests
ctest

# Install (optional)
sudo make install
```

## Cross-OS Preset Workflow (Linux + Windows)

ScratchBird 0.1.0 uses CMake presets for reproducible Linux and Windows builds.

```bash
# Linux GCC
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug-build --parallel
ctest --preset linux-gcc-debug-test -E quarantine --output-on-failure

# Linux Clang
cmake --preset linux-clang-debug
cmake --build --preset linux-clang-debug-build --parallel
ctest --preset linux-clang-debug-test -E quarantine --output-on-failure

# Linux -> Windows (MinGW cross compile)
scripts/cross_os/bootstrap_mingw_zlib.sh
scripts/cross_os/bootstrap_mingw_openssl.sh
cmake --preset linux-mingw-windows-x64
cmake --build --preset linux-mingw-windows-x64-build --parallel
```

Portable lane helpers:

```bash
scripts/cross_os/run_portable_lane.sh --lane portable --test-preset linux-gcc-debug-test
scripts/cross_os/run_portable_lane.sh --lane windows_portable --test-preset windows-msvc-debug-test
```

---

## Step-by-Step Build

### 1. Clone Repository

```bash
git clone https://github.com/DaltonCalford/ScratchBird.git
cd ScratchBird
```

For a specific version:
```bash
git checkout v0.9.0-beta0
```

### 2. Create Build Directory

```bash
mkdir build
cd build
```

### 3. Configure with CMake

Basic configuration:
```bash
cmake ..
```

With options:
```bash
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/opt/scratchbird \
    -DWITH_LZ4=ON \
    -DWITH_GEOS=ON
```

### 4. Build

```bash
# Build with all available cores
make -j$(nproc)

# Or specify number of jobs
make -j8
```

### 5. Run Tests

```bash
# Run all tests
ctest

# Run with verbose output
ctest -V

# Run specific test
ctest -R test_name
```

### 6. Install (Optional)

```bash
# Install to CMAKE_INSTALL_PREFIX
sudo make install
```

---

## CMake Options

### Build Type

| Option | Description |
|--------|-------------|
| `-DCMAKE_BUILD_TYPE=Release` | Optimized build (-O3) |
| `-DCMAKE_BUILD_TYPE=Debug` | Debug build with symbols |
| `-DCMAKE_BUILD_TYPE=RelWithDebInfo` | Release with debug info |

### Installation Prefix

```bash
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local
```

### Feature Toggles

| Option | Default | Description |
|--------|---------|-------------|
| `-DWITH_LZ4=ON` | Auto | LZ4 compression |
| `-DWITH_GEOS=ON` | Auto | Spatial functions |
| `-DWITH_PROJ=ON` | Auto | Coordinate systems |
| `-DWITH_LIBXML2=ON` | Auto | XML functions |
| `-DBUILD_TESTING=ON` | ON | Build tests |
| `-DBUILD_DOCS=OFF` | OFF | Build documentation |

### Compiler Selection

```bash
cmake .. \
    -DCMAKE_C_COMPILER=gcc-11 \
    -DCMAKE_CXX_COMPILER=g++-11
```

Or for Clang:
```bash
cmake .. \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++
```

---

## Build Outputs

After successful build:

```
build/
├── src/
│   ├── sb_server          # Server daemon
│   ├── sb_isql            # Interactive SQL shell
│   ├── sb_verify          # Verification tool
│   ├── sb_backup          # Backup utility
│   └── sb_security        # Security tool
├── lib/
│   ├── libscratchbird_core.a
│   ├── libscratchbird_parser.a
│   ├── libscratchbird_sblr.a
│   └── ...
└── tests/
    └── scratchbird_tests  # Test executable
```

---

## Running Without Install

You can run directly from the build directory:

```bash
# Start server
./src/sb_server --config ../etc/scratchbird/sb_server.conf.example --foreground

# Connect
./src/sb_isql -H localhost -P 3092
```

---

## Windows Cross-Compilation

Use the cross preset and bootstrap scripts in-tree:

```bash
scripts/cross_os/bootstrap_mingw_zlib.sh
scripts/cross_os/bootstrap_mingw_openssl.sh
cmake --preset linux-mingw-windows-x64
cmake --build --preset linux-mingw-windows-x64-build --parallel
```

Output binaries are emitted under:

- `build/linux-mingw-windows-x64/src/*.exe`
- `build/linux-mingw-windows-x64/tools/*.exe`

---

## Debug Build

For development and debugging:

```bash
cmake .. \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-g -O0 -fno-omit-frame-pointer"

make -j$(nproc)
```

### With Address Sanitizer

```bash
cmake .. \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-g -O0 -fsanitize=address -fno-omit-frame-pointer"

make -j$(nproc)
```

### With Thread Sanitizer

```bash
cmake .. \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-g -O0 -fsanitize=thread"

make -j$(nproc)
```

---

## Optimized Build

For maximum performance:

```bash
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS="-O3 -march=native -mtune=native"

make -j$(nproc)

# Strip symbols (smaller binaries)
strip src/sb_server src/sb_isql
```

---

## Packaging

Generate packages after building:

```bash
# Configure with CPack
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
make -j$(nproc)

# Generate DEB
cpack -G DEB

# Generate RPM
cpack -G RPM

# Generate tarball
cpack -G TGZ
```

---

## Troubleshooting

### CMake Can't Find Package

```bash
# Specify package location
cmake .. -DLIBXML2_ROOT=/usr/local

# Or add to CMAKE_PREFIX_PATH
cmake .. -DCMAKE_PREFIX_PATH="/usr/local;/opt/libs"
```

### Compiler Version Too Old

```bash
# Check GCC version
g++ --version

# Install newer compiler
sudo apt install g++-11

# Use it
cmake .. -DCMAKE_CXX_COMPILER=g++-11
```

### Missing Headers

```bash
# Check which package provides the header
apt-file search some_header.h

# Install the package
sudo apt install libsome-dev
```

### Link Errors

```bash
# Check library paths
pkg-config --libs libssl

# Add library path
cmake .. -DCMAKE_LIBRARY_PATH=/usr/local/lib
```

### Out of Memory During Build

```bash
# Reduce parallel jobs
make -j2

# Or build one target at a time
make sb_server
make sb_isql
```

---

## Development Workflow

```bash
# Initial setup
git clone https://github.com/DaltonCalford/ScratchBird.git
cd ScratchBird
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Build and test cycle
make -j$(nproc) && ctest

# Rebuild specific target
make sb_server

# Clean build
make clean
# or
rm -rf * && cmake ..
```

---

## Next Steps

After building:

1. [Configure the server](../configuration/sb_server.conf.md)
2. [Create your first database](../getting-started/first-database.md)
3. [Connect with a client](../getting-started/first-connection.md)

For development:
- Read `CONTRIBUTING.md` in the repository
- Follow coding standards in `docs/development/`
- Run tests before submitting changes
