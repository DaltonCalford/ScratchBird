# ScratchBird Build Environment Setup

This guide covers everything you need to set up a development environment for ScratchBird, from installing dependencies to building and testing the project.

---

## Table of Contents

1. [Quick Start](#quick-start)
2. [System Requirements](#system-requirements)
3. [Required Dependencies](#required-dependencies)
4. [Optional Dependencies](#optional-dependencies)
5. [Installation by Platform](#installation-by-platform)
6. [Getting the Source Code](#getting-the-source-code)
7. [Building ScratchBird](#building-scratchbird)
8. [Running Tests](#running-tests)
9. [Development Tools](#development-tools)
10. [Troubleshooting](#troubleshooting)

---

## Quick Start

For experienced developers who want to get started immediately:

```bash
# Ubuntu/Debian
sudo apt-get update && sudo apt-get install -y \
    cmake build-essential git python3 pkg-config \
    liblz4-dev libgeos-dev libproj-dev libxml2-dev libssl-dev

# Clone the repository
git clone https://github.com/DaltonCalford/ScratchBird.git
cd ScratchBird

# Build
mkdir build && cd build
cmake .. && make -j$(nproc)

# Test
ctest --output-on-failure
```

For detailed instructions, continue reading below.

---

## System Requirements

### Minimum Requirements
- **OS**: Linux, macOS, or WSL2 on Windows
- **CPU**: x86_64 or ARM64
- **RAM**: 4GB (8GB+ recommended for compilation)
- **Disk Space**: 2GB for source + build artifacts

### Supported Platforms
- **Linux**: Ubuntu 20.04+, Debian 11+, Fedora 35+, RHEL 8+
- **macOS**: 11.0+ (Big Sur or newer)
- **Windows**: WSL2 with Ubuntu 20.04+

---

## Required Dependencies

These dependencies are **mandatory** for building ScratchBird:

### 1. Build System
- **CMake** >= 3.20
  - Purpose: Build configuration and generation
  - Website: https://cmake.org/

### 2. C++ Compiler
- **GCC** >= 7.0 **OR** **Clang** >= 5.0
  - Must support C++17 standard
  - Purpose: Compiling C++ source code

### 3. Build Tools
- **Make** or **Ninja**
  - Purpose: Executing build commands
  - Typically included with compiler toolchain

### 4. Version Control
- **Git**
  - Purpose: Cloning repository and version control
  - Website: https://git-scm.com/

### 5. Python
- **Python 3** (3.6 or newer)
  - Purpose: Build automation scripts
  - Scripts: `convert_to_googletest.py`, `fix_integration_tests.py`, etc.

### 6. Package Configuration
- **pkg-config**
  - Purpose: Helps CMake find optional libraries
  - Required for detecting GEOS, PROJ, LZ4, etc.

### Auto-Fetched Dependencies
These are automatically downloaded by CMake during configuration:
- **GoogleTest** v1.14.0 (testing framework)
- **nlohmann/json** v3.11.3 (JSON parsing library)

---

## Optional Dependencies

These dependencies are **optional** but highly recommended for full functionality:

### Compression
- **LZ4** (`liblz4-dev`)
  - Purpose: Fast compression/decompression for data pages
  - Impact if missing: Compression features disabled
  - CMake output: "LZ4 compression support: ENABLED/DISABLED"

### Spatial/Geographic
- **GEOS** (`libgeos-dev`)
  - Purpose: Spatial geometry operations (ST_Contains, ST_Intersects, etc.)
  - Impact if missing: Spatial functions limited to basic operations
  - CMake output: "GEOS spatial library: ENABLED/DISABLED"

- **PROJ** (`libproj-dev`)
  - Purpose: Coordinate system transformations and projections
  - Impact if missing: Geographic coordinate operations unavailable
  - CMake output: "PROJ coordinate system library: ENABLED/DISABLED"

### XML Processing
- **libxml2** (`libxml2-dev`)
  - Purpose: Full XML/XPath support for XML data type
  - Impact if missing: Falls back to basic string-based XML implementation
  - CMake output: "libxml2 found: Full XML/XPath support enabled"

### Security
- **OpenSSL** (`libssl-dev`)
  - Purpose: Secure random number generation for password salts
  - Impact if missing: Uses `std::random_device` (less secure)
  - CMake output: "OpenSSL found: Using secure random for password salts"

- **libcrypt** (`libcrypt-dev`)
  - Purpose: BCrypt password hashing for user authentication
  - Impact if missing: Uses fallback password hashing (insecure)
  - CMake output: "libcrypt found: Password hashing enabled"
  - Note: Often included in base system libraries

---

## Installation by Platform

### Ubuntu / Debian

```bash
# Update package lists
sudo apt-get update

# Install required dependencies
sudo apt-get install -y \
    cmake \
    build-essential \
    git \
    python3 \
    pkg-config

# Install optional dependencies (recommended)
sudo apt-get install -y \
    liblz4-dev \
    libgeos-dev \
    libproj-dev \
    libxml2-dev \
    libssl-dev

# Install development tools (optional)
sudo apt-get install -y \
    clang-format \
    clang-tidy \
    valgrind
```

### Fedora / RHEL / CentOS

```bash
# Install required dependencies
sudo dnf install -y \
    cmake \
    gcc-c++ \
    git \
    python3 \
    pkg-config

# Install optional dependencies (recommended)
sudo dnf install -y \
    lz4-devel \
    geos-devel \
    proj-devel \
    libxml2-devel \
    openssl-devel

# Install development tools (optional)
sudo dnf install -y \
    clang-tools-extra \
    valgrind
```

### macOS

```bash
# Install Homebrew if not already installed
# /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install required dependencies
brew install \
    cmake \
    git \
    python3 \
    pkg-config

# Install optional dependencies (recommended)
brew install \
    lz4 \
    geos \
    proj \
    libxml2 \
    openssl

# Install development tools (optional)
brew install \
    clang-format \
    valgrind
```

### Windows (WSL2)

1. Install WSL2 with Ubuntu 20.04+:
   ```powershell
   # In PowerShell (Administrator)
   wsl --install -d Ubuntu-20.04
   ```

2. Follow the Ubuntu installation instructions above inside WSL2

---

## Getting the Source Code

### Clone the Repository

```bash
# Clone via HTTPS
git clone https://github.com/DaltonCalford/ScratchBird.git

# OR clone via SSH (if you have SSH keys configured)
git clone git@github.com:DaltonCalford/ScratchBird.git

# Navigate to project directory
cd ScratchBird
```

### Verify Clone Success

```bash
# Check directory structure
ls -la

# You should see:
# - src/          (source code)
# - include/      (headers)
# - tests/        (test suite)
# - docs/         (documentation)
# - CMakeLists.txt
# - README.md
# - MGA_RULES.md
```

### Update Existing Clone

```bash
# Pull latest changes from main branch
git pull origin main
```

---

## Building ScratchBird

### Standard Build

```bash
# Create build directory (out-of-source build)
mkdir build
cd build

# Configure with CMake
cmake ..

# Build with all available CPU cores
make -j$(nproc)

# On macOS, use:
# make -j$(sysctl -n hw.ncpu)
```

### Build Configuration Options

#### Debug Build (with symbols, no optimization)
```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
```

#### Release Build (optimized, no debug symbols)
```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

#### Using Ninja (faster builds)
```bash
cmake -G Ninja ..
ninja -j$(nproc)
```

### Understanding CMake Output

When you run `cmake ..`, you'll see messages indicating which features are enabled:

```
-- ScratchBird Database Engine - Planning Phase
-- nlohmann/json library: ENABLED (FetchContent)
-- LZ4 compression support: ENABLED
-- GEOS spatial library: ENABLED (3.10.2)
-- PROJ coordinate system library: ENABLED (8.2.0)
-- libxml2 found: Full XML/XPath support enabled
-- OpenSSL found: Using secure random for password salts
-- libcrypt found: Password hashing enabled
-- clang-format found. Use 'make format' to format the code.
```

If a dependency is missing, you'll see:
```
-- LZ4 compression support: DISABLED (install liblz4-dev to enable)
-- GEOS spatial library: DISABLED (install libgeos-dev to enable)
```

---

## Running Tests

ScratchBird has an extensive test suite with 2,011+ test cases.

### Run All Tests

```bash
# From the build directory
ctest --output-on-failure

# Or with verbose output
ctest -V
```

### Run Specific Test Categories

```bash
# Run only unit tests
ctest -L unit

# Run only integration tests
ctest -L integration

# Run stress tests
ctest -L stress

# Run thread safety tests (TSan)
ctest -L tsan

# Run specific test by name
ctest -R BTree

# Run tests matching a pattern
ctest -R ".*Index.*"
```

### Run Individual Test Executables

```bash
# Main test suite
./scratchbird_tests

# Specific test executables
./test_range_types
./test_network_types
./tsan_buffer_pool_race
./test_columnstore_rle
```

### Thread Sanitizer Tests

```bash
# Run with ThreadSanitizer (detects race conditions)
ctest -L tsan

# Manually run TSan tests
./tsan_buffer_pool_race
./tsan_transaction_cache_race
./tsan_lock_ordering
```

### Valgrind/Helgrind Tests

```bash
# Run Helgrind for lock ordering validation (slow)
valgrind --tool=helgrind ./helgrind_races
```

---

## Development Tools

### Code Formatting

```bash
# Format all source files with clang-format
make format
```

### Code Linting

```bash
# clang-tidy is currently disabled in CMakeLists.txt
# due to incompatible .clang-tidy configuration
# Will be re-enabled after configuration update
```

### Verify MGA Compliance

```bash
# Run MGA architecture verification script
./scripts/verify_mga_compliance.sh
```

### Clean Build

```bash
# From build directory
make clean

# Or remove build directory entirely
cd ..
rm -rf build
mkdir build && cd build
cmake .. && make -j$(nproc)
```

---

## Troubleshooting

### CMake Cannot Find Dependencies

**Problem**: CMake reports dependencies as "DISABLED" even though they're installed.

**Solution**:
```bash
# Install pkg-config if missing
sudo apt-get install pkg-config

# Update package database
sudo ldconfig

# Re-run cmake with clean cache
rm -rf CMakeCache.txt CMakeFiles/
cmake ..
```

### Compilation Errors

**Problem**: "error: 'xyz' was not declared in this scope"

**Solution**:
1. Ensure you're using C++17 compatible compiler (GCC 7+ or Clang 5+)
2. Check compiler version:
   ```bash
   g++ --version
   clang++ --version
   ```
3. Update compiler if needed:
   ```bash
   sudo apt-get install gcc-11 g++-11
   export CC=gcc-11
   export CXX=g++-11
   ```

### Out of Memory During Build

**Problem**: Build fails with "virtual memory exhausted"

**Solution**:
```bash
# Reduce parallel jobs
make -j2  # Use only 2 cores instead of all cores

# Or build serially
make
```

### Test Failures

**Problem**: Tests fail with "database file not found" or similar errors

**Solution**:
```bash
# Tests create temporary databases in current directory
# Ensure you have write permissions
chmod +w .

# Run tests from build directory
cd build
ctest --output-on-failure
```

### Git Authentication Issues

**Problem**: Cannot push changes to GitHub

**Solution**:
```bash
# Configure SSH keys (recommended)
ssh-keygen -t ed25519 -C "your_email@example.com"
cat ~/.ssh/id_ed25519.pub  # Add to GitHub SSH keys

# Or use SSH instead of HTTPS
git remote set-url origin git@github.com:DaltonCalford/ScratchBird.git

# Accept GitHub's SSH host key
ssh-keyscan github.com >> ~/.ssh/known_hosts
```

### Missing GoogleTest or nlohmann/json

**Problem**: CMake cannot fetch GoogleTest or nlohmann/json

**Solution**:
```bash
# Ensure internet connectivity for FetchContent
curl -I https://github.com

# Check CMake version (must be >= 3.20)
cmake --version

# If behind proxy, configure git:
git config --global http.proxy http://proxy:port
```

---

## Project Structure

```
ScratchBird/
├── build/                  # Build artifacts (created by you)
├── src/
│   ├── core/              # Core engine (79 files)
│   ├── parser/            # SQL parser (6 files)
│   ├── sblr/              # Bytecode engine (5 files)
│   ├── spatial/           # Spatial operations
│   ├── geo/               # Geographic operations
│   └── optimizer/         # Query optimizer
├── include/
│   └── scratchbird/       # Public headers
├── tests/
│   ├── unit/              # Unit tests (100+ files)
│   ├── integration/       # Integration tests (50+ files)
│   ├── stress/            # Stress tests
│   ├── tsan/              # ThreadSanitizer tests
│   └── helgrind/          # Helgrind tests
├── docs/
│   ├── specifications/    # SQL dialect specs
│   ├── planning/          # Implementation plans
│   └── audit/             # Audit reports
├── scripts/               # Build automation scripts
├── CMakeLists.txt         # Main CMake configuration
├── README.md              # Project overview
├── BUILD_ENVIRONMENT.md   # This file
├── MGA_RULES.md           # Architecture rules (MANDATORY reading)
├── PROJECT_CONTEXT.md     # Current work status
└── OFFICIAL_ROADMAP.md    # Complete project scope
```

---

## Additional Resources

### Documentation
- **[README.md](README.md)** - Project overview and quick start
- **[MGA_RULES.md](MGA_RULES.md)** - Firebird MGA architecture rules (MANDATORY)
- **[PROJECT_CONTEXT.md](PROJECT_CONTEXT.md)** - Current development status
- **[OFFICIAL_ROADMAP.md](OFFICIAL_ROADMAP.md)** - Complete project roadmap

### External Resources
- **CMake Documentation**: https://cmake.org/documentation/
- **GoogleTest Guide**: https://google.github.io/googletest/
- **C++ Reference**: https://en.cppreference.com/
- **GEOS**: https://libgeos.org/
- **PROJ**: https://proj.org/

### Getting Help
- **GitHub Issues**: https://github.com/DaltonCalford/ScratchBird/issues
- **Project Repository**: https://github.com/DaltonCalford/ScratchBird

---

## Next Steps

After building successfully:

1. **Read the mandatory documentation**:
   - [MGA_RULES.md](MGA_RULES.md) - Understand the Firebird MGA architecture
   - [PROJECT_CONTEXT.md](PROJECT_CONTEXT.md) - See what's currently being worked on

2. **Run the test suite**:
   ```bash
   ctest --output-on-failure
   ```

3. **Explore the codebase**:
   - Start with `src/core/` for engine internals
   - Check `include/scratchbird/core/` for public APIs
   - Look at `tests/unit/` for usage examples

4. **Review the roadmap**:
   - See [OFFICIAL_ROADMAP.md](OFFICIAL_ROADMAP.md) for the complete project vision

---

**Last Updated**: November 23, 2025
**Project Status**: Alpha 1 (70% complete)
