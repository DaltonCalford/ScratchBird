# ScratchBird Build Instructions - Step-by-Step Compilation Guide

**Version**: Alpha 0.6.0  
**Target Platforms**: Windows 10/11, Ubuntu Linux 25.04  
**Documentation Date**: July 27, 2025  
**Status**: ✅ **Test Ready** - Still missing features to be Implemented  

---

## Overview

This document provides step-by-step instructions for building ScratchBird from source code on Windows and Ubuntu Linux platforms. These instructions assume you have completed the development environment setup described in BUILD_REQUIREMENTS.md.

### Build Products

A successful ScratchBird build produces:
- **libsbclient**: Client library for database connections
- **sb_isql**: Interactive SQL command-line tool
- **sb_gbak**: Backup and restore utility
- **sb_gfix**: Database maintenance tool
- **sb_gstat**: Database statistics utility
- **sb_gsec**: Security management tool
- **gpre**: SQL preprocessor tool

### Build Time Estimates

- **Windows (8-core system)**: 15-25 minutes full build
- **Ubuntu (8-core system)**: 10-20 minutes full build
- **Incremental builds**: 2-5 minutes

---

## Pre-Build Checklist

Before starting the build process, verify your environment:

### Windows Pre-Build Check
```powershell
# Run the verification script from BUILD_REQUIREMENTS.md
.\check_windows_env.ps1

# Verify vcpkg packages
C:\vcpkg\vcpkg list

# Ensure Visual Studio is properly configured
where cl.exe
where cmake.exe
```

### Ubuntu Pre-Build Check
```bash
# Run the verification script from BUILD_REQUIREMENTS.md
./check_ubuntu_env.sh

# Verify compiler versions
gcc --version
g++ --version
cmake --version

# Check installed packages
dpkg -l | grep -E "(libicu-dev|zlib1g-dev|libssl-dev|libtommath-dev|libre2-dev)"
```

---

## Source Code Preparation

### Repository Structure (From STARTING_DEV.md)

**ScratchBird follows a specific directory structure:**
```
ScratchBird/
├── src/                    # Main server source code
│   ├── jrd/                # Database engine core
│   ├── dsql/               # SQL parser and compiler
│   ├── common/             # Shared utilities
│   ├── utilities/          # sb_isql, sb_gbak, sb_gfix, etc.
│   ├── auth/               # Authentication system
│   └── remote/             # Network protocol
├── builds/                 # Build system files (GNU Make based)
│   ├── posix/              # POSIX platform configurations
│   └── cmake/              # Legacy CMake support (limited)
├── build_scripts/          # Build automation scripts
├── gen/                    # Generated build artifacts
├── temp/                   # Temporary build files
├── release/                # Final compiled executables by version/platform
│   └── alpha0.6.0/
│       ├── linux-x64/      # Linux 64-bit builds
│       └── windows-x64/    # Windows 64-bit builds  
├── extern/                 # External dependencies
├── examples/               # Sample code and tutorials
├── tests/                  # All test files and scripts
└── doc/                    # Documentation
```

### Build System Overview

**ScratchBird uses a custom GNU Make-based build system:**
- **Primary Build Tool**: GNU Make (not CMake)
- **Build Commands**: `make TARGET=Release [targets]`
- **Build Configuration**: `builds/posix/make.defaults` and platform-specific files
- **Output Location**: `gen/Release/scratchbird/`
- **Final Release**: `release/alpha0.6.0/[platform]/`

---

## Linux Build Instructions (GNU Make System)

### Method 1: Using Existing Build Scripts

**ScratchBird provides automated build scripts in `build_scripts/`:**

#### Step 1: Use the Complete Build Script

**Run the automated build:**
```bash
# Navigate to ScratchBird directory
cd ScratchBird

# Run complete build script (recommended)
./build_scripts/sb_build_all.sh

# Or with options:
./build_scripts/sb_build_all.sh --clean --verbose

# Check help for all options
./build_scripts/sb_build_all.sh --help
```

**Build script features:**
- Automatically sets up proper directory structure
- Cleans root directory of artifacts
- Uses `gen/Release/scratchbird/` for build output
- Copies final binaries to `release/alpha0.6.0/linux-x64/`
- Verifies all utilities are properly built

#### Step 2: Verify Build Results

**Check build products:**
```bash
# Check final release directory
ls -la release/alpha0.6.0/linux-x64/bin/

# Test utilities
./release/alpha0.6.0/linux-x64/bin/sb_gbak -z
./release/alpha0.6.0/linux-x64/bin/sb_isql -z
./release/alpha0.6.0/linux-x64/bin/sb_gstat -z
```

### Method 2: Manual Build Using GNU Make

#### Step 1: Setup Environment

**Prepare build environment:**
```bash
cd ScratchBird

# Clean any previous build artifacts
make TARGET=Release clean

# Set parallel build jobs
export MAKEFLAGS="-j$(nproc)"
```

#### Step 2: Build External Dependencies

**Build required external libraries:**
```bash
# Build external dependencies first
make TARGET=Release external

# This builds:
# - libtommath (arbitrary precision math)
# - libtomcrypt (cryptographic functions)  
# - decNumber (decimal arithmetic)
# - editline (command line editing)
```

#### Step 3: Build Core Utilities

**Build ScratchBird utilities:**
```bash
# Build all utilities in parallel
make TARGET=Release sb_isql sb_gbak sb_gfix sb_gstat sb_gsec

# Or build individually:
make TARGET=Release sb_isql     # Interactive SQL tool
make TARGET=Release sb_gbak     # Backup/restore utility
make TARGET=Release sb_gfix     # Database maintenance tool  
make TARGET=Release sb_gstat    # Database statistics utility
make TARGET=Release sb_gsec     # Security management tool
```

#### Step 4: Verify Build Output

**Check generated artifacts:**
```bash
# Navigate to build output
cd gen/Release/scratchbird

# Check binaries
ls -la bin/sb_*

# Check libraries  
ls -la lib/libsbclient*

# Test version information
SCRATCHBIRD=. bin/sb_gbak -z
SCRATCHBIRD=. bin/sb_isql -z
```

### Windows Build Instructions

**Current Status**: Windows builds are supported but require specific setup.

#### Windows Build via MSYS2/MinGW

**Setup MSYS2 environment:**
```bash
# In MSYS2 MINGW64 shell
pacman -S mingw-w64-x86_64-gcc
pacman -S mingw-w64-x86_64-make
pacman -S mingw-w64-x86_64-pkg-config

# Navigate to ScratchBird
cd /c/workspace/ScratchBird

# Build using same commands as Linux
make TARGET=Release external
make TARGET=Release sb_isql sb_gbak sb_gfix sb_gstat sb_gsec
```

**Note**: Native Windows builds using Visual Studio are not currently the primary build method for ScratchBird. The project uses a POSIX-compatible build system.

### Windows Troubleshooting

**Common Windows Build Issues:**

**vcpkg Integration Problems:**
```batch
REM Re-integrate vcpkg
cd C:\vcpkg
.\vcpkg integrate remove
.\vcpkg integrate install

REM Clear CMake cache and reconfigure
cd C:\workspace\ScratchBird\gen\Release
del CMakeCache.txt
rmdir /s /q CMakeFiles
cmake -G "Visual Studio 17 2022" -A x64 ..\..
```

**Missing Dependencies:**
```batch
REM Check vcpkg packages
C:\vcpkg\vcpkg list

REM Reinstall missing packages
C:\vcpkg\vcpkg install zlib:x64-windows icu:x64-windows libtommath:x64-windows
```

**Compiler Errors:**
```batch
REM Verify Visual Studio installation
where cl.exe
cl.exe

REM Check Windows SDK
dir "C:\Program Files (x86)\Windows Kits\10\Include"
```

---

## Ubuntu Linux Build Instructions

### Method 1: Traditional Make Build

#### Step 1: Prepare Build Environment

**Setup workspace:**
```bash
# Navigate to source directory
cd ~/workspace/ScratchBird

# Create build directory
mkdir -p gen/Release
cd gen/Release

# Set environment variables
export CC=gcc-13
export CXX=g++-13
export MAKEFLAGS="-j$(nproc)"
```

#### Step 2: Configure Build

**CMake Configuration:**
```bash
# Configure with CMake
cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DCMAKE_C_COMPILER=gcc-13 \
    -DCMAKE_CXX_COMPILER=g++-13 \
    ../..

# Alternative: Debug build
cmake \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DCMAKE_C_COMPILER=gcc-13 \
    -DCMAKE_CXX_COMPILER=g++-13 \
    ../..
```

#### Step 3: Build ScratchBird

**Compilation:**
```bash
# Build using make
make -j$(nproc)

# Alternative: Build using CMake
cmake --build . --parallel $(nproc)

# Monitor progress
watch -n 1 'ps aux | grep -E "(gcc|g\+\+|ld)" | wc -l'
```

#### Step 4: Verify Build

**Test build products:**
```bash
# Navigate to build output
cd scratchbird

# List executables
ls -la bin/

# Test utilities
./bin/sb_gbak -z
./bin/sb_gstat -z
./bin/sb_isql -z

# Check library
ls -la lib/libsbclient*
ldd lib/libsbclient.so
```

### Method 2: Ninja Build (Faster)

#### Step 1: Configure with Ninja

**CMake Configuration:**
```bash
cd ~/workspace/ScratchBird
mkdir -p gen/Release-Ninja
cd gen/Release-Ninja

# Configure with Ninja generator
cmake \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DCMAKE_C_COMPILER=gcc-13 \
    -DCMAKE_CXX_COMPILER=g++-13 \
    ../..
```

#### Step 2: Build with Ninja

**Compilation:**
```bash
# Build with Ninja (automatically uses all cores)
ninja

# Alternative: Limit parallel jobs
ninja -j 4

# Monitor progress
ninja -t browse
```

### Method 3: Traditional Makefile Build

**For compatibility with older build systems:**

#### Step 1: Legacy Build Configuration

```bash
cd ~/workspace/ScratchBird/builds/cmake

# Create traditional makefile
cmake \
    -G "Unix Makefiles" \
    -DCMAKE_BUILD_TYPE=Release \
    ../..

# Build using traditional make
make external
make -j$(nproc) sb_isql sb_gbak sb_gfix sb_gstat sb_gsec
```

### Ubuntu Installation

#### Step 1: Install Locally

**Local Installation:**
```bash
# Install to /usr/local (requires sudo)
sudo cmake --install . --config Release

# Alternative: Install to user directory
cmake --install . --prefix ~/.local

# Update library cache
sudo ldconfig

# Add to PATH (if using user install)
echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

#### Step 2: Create Packages

**DEB Package Creation:**
```bash
# Install packaging tools
sudo apt install -y checkinstall

# Create DEB package
sudo checkinstall \
    --pkgname=scratchbird \
    --pkgversion=0.6.0 \
    --pkgrelease=1 \
    --pkgsource=ScratchBird \
    --pakdir=/tmp \
    --maintainer="your-email@example.com" \
    --requires="libicu70,zlib1g,libssl3" \
    make install

# Install the package
sudo dpkg -i /tmp/scratchbird_0.6.0-1_amd64.deb
```

### Ubuntu Troubleshooting

**Common Ubuntu Build Issues:**

**Missing Dependencies:**
```bash
# Update package lists
sudo apt update

# Install missing development packages
sudo apt install -y build-essential cmake libicu-dev zlib1g-dev libssl-dev

# Check for specific libraries
pkg-config --exists icu-i18n && echo "ICU found" || echo "ICU missing"
pkg-config --exists zlib && echo "zlib found" || echo "zlib missing"
```

**Compiler Issues:**
```bash
# Check compiler installation
which gcc g++
gcc --version
g++ --version

# Update alternatives if needed
sudo update-alternatives --config gcc
sudo update-alternatives --config g++
```

**Library Path Issues:**
```bash
# Update library cache
sudo ldconfig

# Check library paths
ldconfig -p | grep -E "(tommath|tomcrypt|icu)"

# Add custom library paths if needed
echo '/usr/local/lib' | sudo tee /etc/ld.so.conf.d/scratchbird.conf
sudo ldconfig
```

**Permission Issues:**
```bash
# Fix source permissions
chmod +x autogen.sh configure
find . -name "*.sh" -exec chmod +x {} \;

# Clean and retry
make clean
rm -rf CMakeCache.txt CMakeFiles/
cmake ../..
```

---

## Build Verification and Testing

### Functional Testing

#### Basic Functionality Test

**Create test script (test_build.sh for Linux, test_build.bat for Windows):**

**Linux version:**
```bash
#!/bin/bash
# Save as: test_build.sh

echo "ScratchBird Build Verification Test"
echo "=================================="

# Test directory
TEST_DIR="/tmp/scratchbird_test"
mkdir -p "$TEST_DIR"
cd "$TEST_DIR"

# Set library path
export LD_LIBRARY_PATH="~/workspace/ScratchBird/gen/Release/scratchbird/lib:$LD_LIBRARY_PATH"

# Path to binaries
BIN_PATH="~/workspace/ScratchBird/gen/Release/scratchbird/bin"

echo "1. Testing sb_gbak version..."
"$BIN_PATH/sb_gbak" -z
if [ $? -eq 0 ]; then
    echo "✓ sb_gbak working"
else
    echo "✗ sb_gbak failed"
    exit 1
fi

echo "2. Testing sb_isql version..."
"$BIN_PATH/sb_isql" -z
if [ $? -eq 0 ]; then
    echo "✓ sb_isql working"
else
    echo "✗ sb_isql failed"
    exit 1
fi

echo "3. Testing database creation..."
"$BIN_PATH/sb_isql" -user SYSDBA -password masterkey << EOF
CREATE DATABASE 'test.sdb';
CREATE TABLE test_table (id INTEGER, name VARCHAR(50));
INSERT INTO test_table VALUES (1, 'Test Record');
SELECT * FROM test_table;
EXIT;
EOF

if [ $? -eq 0 ]; then
    echo "✓ Database operations working"
else
    echo "✗ Database operations failed"
    exit 1
fi

echo "4. Testing backup and restore..."
"$BIN_PATH/sb_gbak" -b test.sdb test.fbk -user SYSDBA -password masterkey
if [ $? -eq 0 ]; then
    echo "✓ Database backup working"
else
    echo "✗ Database backup failed"
    exit 1
fi

"$BIN_PATH/sb_gbak" -r test.fbk test_restored.sdb -user SYSDBA -password masterkey
if [ $? -eq 0 ]; then
    echo "✓ Database restore working"
else
    echo "✗ Database restore failed"
    exit 1
fi

echo "5. Testing restored database..."
"$BIN_PATH/sb_isql" -user SYSDBA -password masterkey test_restored.sdb << EOF
SELECT * FROM test_table;
EXIT;
EOF

if [ $? -eq 0 ]; then
    echo "✓ Restored database working"
else
    echo "✗ Restored database failed"
    exit 1
fi

# Cleanup
rm -f test.sdb test.fbk test_restored.sdb

echo ""
echo "All tests passed! ScratchBird build is functional."
```

**Windows version:**
```batch
@echo off
REM Save as: test_build.bat

echo ScratchBird Build Verification Test
echo ==================================

REM Test directory
set TEST_DIR=C:\temp\scratchbird_test
mkdir "%TEST_DIR%" 2>nul
cd /d "%TEST_DIR%"

REM Path to binaries
set BIN_PATH=C:\workspace\ScratchBird\gen\Release\scratchbird\bin

echo 1. Testing sb_gbak version...
"%BIN_PATH%\sb_gbak.exe" -z
if %errorlevel% equ 0 (
    echo ✓ sb_gbak working
) else (
    echo ✗ sb_gbak failed
    exit /b 1
)

echo 2. Testing sb_isql version...
"%BIN_PATH%\sb_isql.exe" -z
if %errorlevel% equ 0 (
    echo ✓ sb_isql working
) else (
    echo ✗ sb_isql failed
    exit /b 1
)

echo 3. Testing database creation...
echo CREATE DATABASE 'test.sdb'; > test_commands.sql
echo CREATE TABLE test_table (id INTEGER, name VARCHAR(50)); >> test_commands.sql
echo INSERT INTO test_table VALUES (1, 'Test Record'); >> test_commands.sql
echo SELECT * FROM test_table; >> test_commands.sql
echo EXIT; >> test_commands.sql

"%BIN_PATH%\sb_isql.exe" -user SYSDBA -password masterkey -i test_commands.sql
if %errorlevel% equ 0 (
    echo ✓ Database operations working
) else (
    echo ✗ Database operations failed
    exit /b 1
)

echo 4. Testing backup and restore...
"%BIN_PATH%\sb_gbak.exe" -b test.sdb test.fbk -user SYSDBA -password masterkey
if %errorlevel% equ 0 (
    echo ✓ Database backup working
) else (
    echo ✗ Database backup failed
    exit /b 1
)

"%BIN_PATH%\sb_gbak.exe" -r test.fbk test_restored.sdb -user SYSDBA -password masterkey
if %errorlevel% equ 0 (
    echo ✓ Database restore working
) else (
    echo ✗ Database restore failed
    exit /b 1
)

REM Cleanup
del test.sdb test.fbk test_restored.sdb test_commands.sql 2>nul

echo.
echo All tests passed! ScratchBird build is functional.
```

### Performance Testing

#### Build Performance Benchmarks

**Measure build times:**

**Linux:**
```bash
# Clean build timing
time (make clean && make -j$(nproc))

# Incremental build timing
touch src/jrd/jrd.cpp
time make -j$(nproc)
```

**Windows:**
```batch
REM Clean build timing
powershell "Measure-Command { cmake --build . --config Release --parallel %NUMBER_OF_PROCESSORS% }"
```

#### Runtime Performance Test

**Create performance test script:**
```bash
#!/bin/bash
# Performance test for ScratchBird

echo "ScratchBird Performance Test"
echo "==========================="

BIN_PATH="~/workspace/ScratchBird/gen/Release/scratchbird/bin"
TEST_DB="performance_test.sdb"

# Create test database with data
echo "Creating test database with 100,000 records..."
"$BIN_PATH/sb_isql" -user SYSDBA -password masterkey << EOF
CREATE DATABASE '$TEST_DB';
CREATE TABLE performance_test (
    id INTEGER GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    name VARCHAR(100),
    value DECIMAL(10,2),
    created_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Insert test data (adjust loop for your needs)
-- This would need to be done programmatically for large datasets
EOF

echo "Running performance queries..."
time "$BIN_PATH/sb_isql" -user SYSDBA -password masterkey "$TEST_DB" << EOF
-- Test query performance
SELECT COUNT(*) FROM performance_test;
SELECT * FROM performance_test WHERE id < 1000;
SELECT AVG(value) FROM performance_test;
EXIT;
EOF

echo "Performance test completed."
rm -f "$TEST_DB"
```

---

## Build Customization

### Build Configuration Options

#### CMake Build Options

**Common CMake variables:**
```bash
# Debug vs Release
-DCMAKE_BUILD_TYPE=Debug          # Debug build with symbols
-DCMAKE_BUILD_TYPE=Release        # Optimized release build
-DCMAKE_BUILD_TYPE=RelWithDebInfo # Release with debug info

# Installation paths
-DCMAKE_INSTALL_PREFIX=/usr/local # Installation directory
-DCMAKE_INSTALL_BINDIR=bin        # Binary directory
-DCMAKE_INSTALL_LIBDIR=lib        # Library directory

# Compiler options
-DCMAKE_C_COMPILER=gcc-13         # C compiler
-DCMAKE_CXX_COMPILER=g++-13       # C++ compiler
-DCMAKE_C_FLAGS="-O3 -march=native" # Additional C flags
-DCMAKE_CXX_FLAGS="-O3 -march=native" # Additional C++ flags

# ScratchBird-specific options
-DSCRATCHBIRD_BUILD_TOOLS=ON      # Build command-line tools
-DSCRATCHBIRD_BUILD_TESTS=ON      # Build test suite
-DSCRATCHBIRD_ENABLE_UDR=ON       # Enable UDR support
-DSCRATCHBIRD_ENABLE_PLUGINS=ON   # Enable plugin system
```

#### Feature Toggles

**ScratchBird feature configuration:**
```bash
# Enable/disable major features
cmake \
    -DSCRATCHBIRD_ENABLE_HIERARCHICAL_SCHEMAS=ON \
    -DSCRATCHBIRD_ENABLE_DATABASE_LINKS=ON \
    -DSCRATCHBIRD_ENABLE_SPATIAL_INDEXES=ON \
    -DSCRATCHBIRD_ENABLE_GIN_INDEXES=ON \
    -DSCRATCHBIRD_ENABLE_PARTIAL_HASH_INDEXES=ON \
    -DSCRATCHBIRD_ENABLE_UUID_SUPPORT=ON \
    -DSCRATCHBIRD_ENABLE_JSON_SUPPORT=ON \
    ../..
```

### Optimization Settings

#### Performance Optimizations

**High-performance build configuration:**

**Linux:**
```bash
cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_FLAGS="-O3 -march=native -mtune=native -flto" \
    -DCMAKE_CXX_FLAGS="-O3 -march=native -mtune=native -flto" \
    -DCMAKE_EXE_LINKER_FLAGS="-flto" \
    -DCMAKE_SHARED_LINKER_FLAGS="-flto" \
    ../..
```

**Windows:**
```batch
cmake ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_C_FLAGS="/O2 /GL /arch:AVX2" ^
    -DCMAKE_CXX_FLAGS="/O2 /GL /arch:AVX2" ^
    ..\..
```

#### Debug Build Configuration

**Debug build with sanitizers (Linux):**
```bash
cmake \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_FLAGS="-g -O0 -fsanitize=address -fsanitize=undefined" \
    -DCMAKE_CXX_FLAGS="-g -O0 -fsanitize=address -fsanitize=undefined" \
    -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address -fsanitize=undefined" \
    ../..
```

---

## Continuous Integration Setup

### Automated Build Scripts

#### Linux CI Script

**Create ci_build_linux.sh:**
```bash
#!/bin/bash
set -e

echo "ScratchBird Linux CI Build"
echo "=========================="

# Environment setup
export CC=gcc-13
export CXX=g++-13
export MAKEFLAGS="-j$(nproc)"

# Clean previous builds
rm -rf gen/Release-CI
mkdir -p gen/Release-CI
cd gen/Release-CI

# Configure
cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DSCRATCHBIRD_BUILD_TESTS=ON \
    -DCMAKE_INSTALL_PREFIX=/tmp/scratchbird-install \
    ../..

# Build
cmake --build . --parallel $(nproc)

# Test
ctest --parallel $(nproc) --output-on-failure

# Install
cmake --install .

echo "CI build completed successfully"
```

#### Windows CI Script

**Create ci_build_windows.bat:**
```batch
@echo off
setlocal enabledelayedexpansion

echo ScratchBird Windows CI Build
echo ============================

REM Clean previous builds
rmdir /s /q gen\Release-CI 2>nul
mkdir gen\Release-CI
cd gen\Release-CI

REM Configure
cmake ^
    -G "Visual Studio 17 2022" ^
    -A x64 ^
    -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake ^
    -DVCPKG_TARGET_TRIPLET=x64-windows ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DSCRATCHBIRD_BUILD_TESTS=ON ^
    ..\..

if !errorlevel! neq 0 (
    echo Configuration failed
    exit /b 1
)

REM Build
cmake --build . --config Release --parallel %NUMBER_OF_PROCESSORS%

if !errorlevel! neq 0 (
    echo Build failed
    exit /b 1
)

REM Test
ctest -C Release --parallel %NUMBER_OF_PROCESSORS% --output-on-failure

if !errorlevel! neq 0 (
    echo Tests failed
    exit /b 1
)

echo CI build completed successfully
```

---

## Deployment and Distribution

### Binary Distribution

#### Create Distribution Package

**Linux distribution script:**
```bash
#!/bin/bash
# create_distribution.sh

VERSION="0.6.0"
DIST_NAME="scratchbird-${VERSION}-linux-x64"
DIST_DIR="/tmp/${DIST_NAME}"

echo "Creating ScratchBird distribution package"

# Create distribution directory structure
mkdir -p "${DIST_DIR}"/{bin,lib,include,doc,examples}

# Copy binaries
cp gen/Release/scratchbird/bin/* "${DIST_DIR}/bin/"

# Copy libraries
cp gen/Release/scratchbird/lib/*.so* "${DIST_DIR}/lib/"

# Copy headers
cp -r gen/Release/scratchbird/include/* "${DIST_DIR}/include/"

# Copy documentation
cp doc/*.md "${DIST_DIR}/doc/"

# Create install script
cat > "${DIST_DIR}/install.sh" << 'EOF'
#!/bin/bash
echo "Installing ScratchBird..."

sudo cp bin/* /usr/local/bin/
sudo cp lib/* /usr/local/lib/
sudo cp -r include/* /usr/local/include/

sudo ldconfig

echo "ScratchBird installed successfully"
echo "Run 'sb_isql -z' to verify installation"
EOF

chmod +x "${DIST_DIR}/install.sh"

# Create tarball
cd /tmp
tar -czf "${DIST_NAME}.tar.gz" "${DIST_NAME}"

echo "Distribution package created: ${DIST_NAME}.tar.gz"
```

### Container Deployment

#### Docker Build

**Create Dockerfile:**
```dockerfile
# Multi-stage build for ScratchBird
FROM ubuntu:25.04 as builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libicu-dev \
    zlib1g-dev \
    libssl-dev \
    libtommath-dev \
    libtomcrypt-dev \
    libre2-dev \
    && rm -rf /var/lib/apt/lists/*

# Copy source code
COPY . /src/scratchbird
WORKDIR /src/scratchbird

# Build ScratchBird
RUN mkdir -p gen/Release && cd gen/Release && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr/local \
        ../.. && \
    make -j$(nproc) && \
    make install

# Runtime image
FROM ubuntu:25.04

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libicu70 \
    zlib1g \
    libssl3 \
    libtommath1 \
    libtomcrypt1 \
    libre2-9 \
    && rm -rf /var/lib/apt/lists/*

# Copy ScratchBird binaries from builder
COPY --from=builder /usr/local/bin/sb_* /usr/local/bin/
COPY --from=builder /usr/local/lib/libsbclient* /usr/local/lib/
COPY --from=builder /usr/local/include/scratchbird /usr/local/include/scratchbird

# Update library cache
RUN ldconfig

# Create scratchbird user
RUN useradd -m -s /bin/bash scratchbird

# Set default user
USER scratchbird
WORKDIR /home/scratchbird

# Default command
CMD ["sb_isql", "-z"]
```

**Build Docker image:**
```bash
# Build image
docker build -t scratchbird:0.6.0 .

# Test image
docker run --rm scratchbird:0.6.0 sb_gbak -z
```

---

## Maintenance and Updates

### Updating Dependencies

#### Updating vcpkg Packages (Windows)

```powershell
# Update vcpkg itself
cd C:\vcpkg
git pull
.\bootstrap-vcpkg.bat

# Update all packages
.\vcpkg upgrade --no-dry-run

# Update specific packages
.\vcpkg remove zlib:x64-windows
.\vcpkg install zlib:x64-windows
```

#### Updating Ubuntu Packages

```bash
# Update package lists
sudo apt update

# Upgrade development packages
sudo apt upgrade build-essential cmake libicu-dev zlib1g-dev libssl-dev

# Check for newer compiler versions
apt search gcc-* | grep gcc-[0-9]
```

### Source Code Updates

#### Updating from Repository

```bash
# Fetch latest changes
git fetch origin

# Check for updates
git log HEAD..origin/main --oneline

# Merge updates
git merge origin/main

# Rebuild after updates
cd gen/Release
make clean
cmake ../..
make -j$(nproc)
```

#### Applying Patches

```bash
# Apply patch file
git apply < patch_file.patch

# Or merge from patch branch
git merge feature/patch-branch

# Rebuild after applying patches
cd gen/Release
make -j$(nproc)
```

---

## Final Notes

### Build Success Criteria

A successful ScratchBird build should produce:
- ✅ All utilities compile without errors
- ✅ All utilities pass version check (`-z` option)
- ✅ Client library links properly
- ✅ Basic database operations work
- ✅ Backup and restore functions operate correctly

### Performance Expectations

**Build Performance Targets:**
- **Full clean build**: < 30 minutes on 8-core system
- **Incremental build**: < 5 minutes for typical changes
- **Library linking**: < 2 minutes

**Runtime Performance Targets:**
- **Database creation**: < 1 second
- **Simple queries**: < 10ms response time
- **Backup/restore**: > 10MB/second throughput

### Support and Troubleshooting

For build issues not covered in this guide:
1. Check the BUILD_REQUIREMENTS.md for environment setup
2. Review error messages carefully for missing dependencies
3. Consult the ScratchBird issue tracker
4. Verify your development environment with the provided verification scripts

---

*This guide provides complete build instructions for ScratchBird on supported platforms. For development environment setup, see BUILD_REQUIREMENTS.md*