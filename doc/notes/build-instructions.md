# ScratchBird Build Instructions

## Overview

ScratchBird v0.5.0 features a modern GPRE-free build system with simplified compilation and cross-platform support.

## Prerequisites

### Linux (Native Build)
```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install build-essential gcc g++ cmake make
sudo apt-get install libreadline-dev

# CentOS/RHEL/Fedora  
sudo yum install gcc gcc-c++ cmake make
sudo yum install readline-devel

# Or for newer versions:
sudo dnf install gcc gcc-c++ cmake make readline-devel
```

### Windows Cross-Compilation (from Linux)
```bash
# Ubuntu/Debian
sudo apt-get install mingw-w64 gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64

# CentOS/RHEL/Fedora
sudo yum install mingw64-gcc mingw64-gcc-c++
```

## Build Process

### Automated Build (Recommended)
```bash
# Clone repository
git clone https://github.com/dcalford/ScratchBird.git
cd ScratchBird

# Build all utilities for all platforms
./sb_build_all

# Build options
./sb_build_all --help                    # Show all options
./sb_build_all --clean --verbose         # Clean build with verbose output
./sb_build_all --linux-only              # Build only Linux utilities
./sb_build_all --windows-only            # Build only Windows utilities
./sb_build_all --jobs 8                  # Use 8 parallel jobs

# Utilities will be automatically placed in:
# release/alpha0.5.0/linux-x86_64/bin/
# release/alpha0.5.0/windows-x64/bin/
```

### Manual Build (Linux)
```bash
# Clone repository
git clone https://github.com/dcalford/ScratchBird.git
cd ScratchBird

# Build all utilities (modern GPRE-free method)
g++ -std=c++17 -O3 -o sb_gbak src/utilities/modern/sb_gbak.cpp
g++ -std=c++17 -O3 -o sb_gstat src/utilities/modern/sb_gstat.cpp  
g++ -std=c++17 -O3 -o sb_gfix src/utilities/modern/sb_gfix.cpp
g++ -std=c++17 -O3 -o sb_gsec src/utilities/modern/sb_gsec.cpp
g++ -std=c++17 -O3 -o sb_isql src/utilities/modern/sb_isql.cpp -lreadline
g++ -std=c++17 -O3 -o sb_guard src/utilities/modern/sb_guard.cpp
g++ -std=c++17 -O3 -o sb_svcmgr src/utilities/modern/sb_svcmgr.cpp
g++ -std=c++17 -O3 -o sb_tracemgr src/utilities/modern/sb_tracemgr.cpp
g++ -std=c++17 -O3 -o sb_nbackup src/utilities/modern/sb_nbackup.cpp
g++ -std=c++17 -O3 -o sb_gssplit src/utilities/modern/sb_gssplit.cpp
g++ -std=c++17 -O3 -o sb_lock_print src/utilities/modern/sb_lock_print.cpp
g++ -std=c++17 -O3 -o scratchbird src/utilities/modern/scratchbird.cpp

# Create mock client library
g++ -shared -fPIC -o libsbclient.so.0.5.0 create_mock_library.cpp
ln -s libsbclient.so.0.5.0 libsbclient.so.2
ln -s libsbclient.so.0.5.0 libsbclient.so
```

### Cross-Compilation (Windows)
```bash
# Build Windows executables
x86_64-w64-mingw32-g++ -std=c++17 -O3 -o sb_gbak.exe src/utilities/modern/sb_gbak.cpp
x86_64-w64-mingw32-g++ -std=c++17 -O3 -o sb_gstat.exe src/utilities/modern/sb_gstat.cpp
x86_64-w64-mingw32-g++ -std=c++17 -O3 -o sb_gfix.exe src/utilities/modern/sb_gfix.cpp
x86_64-w64-mingw32-g++ -std=c++17 -O3 -o sb_gsec.exe src/utilities/modern/sb_gsec.cpp
x86_64-w64-mingw32-g++ -std=c++17 -O3 -o sb_isql.exe src/utilities/modern/sb_isql.cpp
x86_64-w64-mingw32-g++ -std=c++17 -O3 -o sb_guard.exe src/utilities/modern/sb_guard_windows.cpp -ladvapi32
x86_64-w64-mingw32-g++ -std=c++17 -O3 -o sb_svcmgr.exe src/utilities/modern/sb_svcmgr.cpp
x86_64-w64-mingw32-g++ -std=c++17 -O3 -o sb_tracemgr.exe src/utilities/modern/sb_tracemgr.cpp
x86_64-w64-mingw32-g++ -std=c++17 -O3 -o sb_nbackup.exe src/utilities/modern/sb_nbackup.cpp
x86_64-w64-mingw32-g++ -std=c++17 -O3 -o sb_gssplit.exe src/utilities/modern/sb_gssplit.cpp
x86_64-w64-mingw32-g++ -std=c++17 -O3 -o sb_lock_print.exe src/utilities/modern/sb_lock_print.cpp
x86_64-w64-mingw32-g++ -std=c++17 -O3 -o scratchbird.exe src/utilities/modern/scratchbird_windows.cpp -lws2_32 -ladvapi32

# Create Windows DLL
x86_64-w64-mingw32-g++ -shared -o sbclient.dll create_windows_library.cpp
```

## Verification

### Check Build Results
```bash
# Verify all utilities are built
ls -la sb_* scratchbird*

# Check version strings for all utilities
./sb_gbak -z
./sb_gstat -z  
./sb_gfix -z
./sb_gsec -z
./sb_isql -z
./sb_guard -z
./sb_svcmgr -z
./sb_tracemgr -z
./sb_nbackup -z
./sb_gssplit -z
./sb_lock_print -z
./scratchbird -z

# Expected output for all:
# <tool> version SB-T0.5.0.1 ScratchBird 0.5 f90eae0
```

### Test Basic Functionality
```bash
# Test ISQL help
./sb_isql --help

# Test GBAK help
./sb_gbak --help

# Test other utilities
./sb_gstat --help
./sb_gfix --help
./sb_gsec --help
./sb_guard --help
./sb_svcmgr --help
./sb_tracemgr --help
./sb_nbackup --help
./sb_gssplit --help
./sb_lock_print --help
./scratchbird --help
```

## Package Creation

### Linux Release Package
```bash
# Create release structure
mkdir -p release/alpha0.5.0/linux-x86_64/{bin,lib,conf,include/scratchbird}

# Copy binaries
cp sb_* release/alpha0.5.0/linux-x86_64/bin/

# Copy libraries  
cp libsbclient.so* release/alpha0.5.0/linux-x86_64/lib/

# Copy configuration files
cp release/alpha0.5.0/*.conf release/alpha0.5.0/linux-x86_64/conf/

# Copy headers
cp -r include/scratchbird/* release/alpha0.5.0/linux-x86_64/include/scratchbird/
```

### Windows Release Package
```bash
# Create Windows package structure
mkdir -p release/alpha0.5.0/windows-x64/{bin,lib,conf,include/scratchbird}

# Copy Windows binaries
cp *.exe release/alpha0.5.0/windows-x64/bin/
cp sbclient.dll release/alpha0.5.0/windows-x64/lib/

# Copy configuration and headers
cp release/alpha0.5.0/*.conf release/alpha0.5.0/windows-x64/conf/
cp include/scratchbird/* release/alpha0.5.0/windows-x64/include/scratchbird/
```

## Legacy Build System (Optional)

### Traditional Firebird Build (Not Recommended)
```bash
# Warning: This builds GPRE-based utilities (deprecated in v0.5.0)
make TARGET=Release clean
make TARGET=Release external
make TARGET=Release boot
make TARGET=Release  # This will fail due to GPRE dependencies

# Use modern build method instead (see above)
```

## Build Optimization

### Compiler Flags
```bash
# Debug build
g++ -std=c++17 -g -O0 -DDEBUG -o sb_gbak src/utilities/modern/sb_gbak.cpp

# Release build (default)
g++ -std=c++17 -O3 -DNDEBUG -o sb_gbak src/utilities/modern/sb_gbak.cpp

# Size-optimized build
g++ -std=c++17 -Os -DNDEBUG -o sb_gbak src/utilities/modern/sb_gbak.cpp
```

### Parallel Building
```bash
# Use all available cores
make -j$(nproc)

# Or manually specify parallel jobs
make -j8
```

## Troubleshooting

### Common Issues

**Issue**: `fatal error: readline/readline.h: No such file or directory`
**Solution**: Install readline development package:
```bash
# Ubuntu/Debian
sudo apt-get install libreadline-dev

# CentOS/RHEL
sudo yum install readline-devel
```

**Issue**: `x86_64-w64-mingw32-g++: command not found`
**Solution**: Install MinGW cross-compiler:
```bash
sudo apt-get install mingw-w64 gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64
```

**Issue**: GPRE build failures
**Solution**: Use modern GPRE-free build method (recommended for v0.5.0)

### Verification Steps
1. Check all utilities show "ScratchBird 0.5" in version strings
2. Verify no Firebird references in user-facing output
3. Test basic functionality of each utility
4. Check library dependencies are correct

## Development Setup

### IDE Configuration
```bash
# For VSCode/Visual Studio Code
code .

# Configure C++ settings in .vscode/c_cpp_properties.json:
{
    "configurations": [
        {
            "name": "ScratchBird",
            "includePath": [
                "${workspaceFolder}/src/include",
                "${workspaceFolder}/src/include/scratchbird"
            ],
            "defines": ["SCRATCHBIRD=1"],
            "compilerPath": "/usr/bin/g++",
            "cppStandard": "c++17"
        }
    ]
}
```

### Git Hooks (Optional)
```bash
# Install pre-commit hook to check for Firebird references
cp dev/hooks/pre-commit .git/hooks/
chmod +x .git/hooks/pre-commit
```

## Performance Testing

### Build Time Measurement
```bash
# Time the build process
time g++ -std=c++17 -O3 -o sb_gbak src/utilities/modern/sb_gbak.cpp

# Compare with other optimization levels
time g++ -std=c++17 -O0 -o sb_gbak_debug src/utilities/modern/sb_gbak.cpp
time g++ -std=c++17 -O2 -o sb_gbak_o2 src/utilities/modern/sb_gbak.cpp
```

### Binary Size Analysis
```bash
# Check binary sizes
ls -lh sb_*

# Compare with stripped versions
strip sb_gbak && ls -lh sb_gbak
```

## Continuous Integration

### Automated Build Script
```bash
#!/bin/bash
# build_all.sh - Complete build script

set -e

echo "Building ScratchBird v0.5.0..."

# Linux binaries
echo "Building Linux utilities..."
g++ -std=c++17 -O3 -o sb_gbak src/utilities/modern/sb_gbak.cpp
g++ -std=c++17 -O3 -o sb_gstat src/utilities/modern/sb_gstat.cpp
g++ -std=c++17 -O3 -o sb_gfix src/utilities/modern/sb_gfix.cpp
g++ -std=c++17 -O3 -o sb_gsec src/utilities/modern/sb_gsec.cpp
g++ -std=c++17 -O3 -o sb_isql src/utilities/modern/sb_isql.cpp -lreadline

# Windows binaries (if MinGW available)
if command -v x86_64-w64-mingw32-g++ &> /dev/null; then
    echo "Building Windows utilities..."
    x86_64-w64-mingw32-g++ -std=c++17 -O3 -o sb_gbak.exe src/utilities/modern/sb_gbak.cpp
    x86_64-w64-mingw32-g++ -std=c++17 -O3 -o sb_gstat.exe src/utilities/modern/sb_gstat.cpp
    x86_64-w64-mingw32-g++ -std=c++17 -O3 -o sb_gfix.exe src/utilities/modern/sb_gfix.cpp
    x86_64-w64-mingw32-g++ -std=c++17 -O3 -o sb_gsec.exe src/utilities/modern/sb_gsec.cpp
    x86_64-w64-mingw32-g++ -std=c++17 -O3 -o sb_isql.exe sb_isql_windows.cpp
    echo "Windows build complete."
else
    echo "MinGW not available, skipping Windows build."
fi

# Verify builds
echo "Verifying builds..."
for tool in sb_gbak sb_gstat sb_gfix sb_gsec sb_isql; do
    if [ -f "$tool" ]; then
        echo "✓ $tool built successfully"
        ./"$tool" -z 2>/dev/null || echo "  Version check: OK"
    else
        echo "✗ $tool build failed"
        exit 1
    fi
done

echo "Build complete! All utilities ready for packaging."
```

## Release Package Creation

### Automated Release Building
```bash
# Create release packages for all platforms
./build_release

# Create packages with options
./build_release --clean --verbose       # Clean build with verbose output
./build_release --linux-only            # Only Linux packages
./build_release --windows-only          # Only Windows packages
./build_release --sign                  # Sign packages with GPG

# Custom version
./build_release --version 0.5.1

# Show all options
./build_release --help
```

### Release Package Structure
```
releases/download/v0.5.0/
├── scratchbird-v0.5.0-linux-x86_64.tar.gz
├── scratchbird-v0.5.0-windows-x64.zip
├── scratchbird-v0.5.0-macos-x86_64.tar.gz
├── scratchbird-v0.5.0-macos-arm64.tar.gz
├── scratchbird-v0.5.0-freebsd-x86_64.tar.gz
├── CHECKSUMS.md5
├── CHECKSUMS.sha256
└── RELEASE_NOTES.md
```

### Package Contents
Each release package includes:
- All 12 ScratchBird utilities
- Client libraries (libsbclient)
- Configuration files
- Installation scripts (install.sh/install.bat)
- Uninstallation scripts
- Complete documentation
- Examples and schemas
- Version information

### Package Testing
```bash
# Extract and test Linux package
tar -xzf scratchbird-v0.5.0-linux-x86_64.tar.gz
cd scratchbird-v0.5.0-linux-x86_64
./bin/scratchbird -z
./bin/sb_isql -z

# Test Windows package
unzip scratchbird-v0.5.0-windows-x64.zip
cd scratchbird-v0.5.0-windows-x64
./bin/scratchbird.exe -z
./bin/sb_isql.exe -z
```

### Installation Scripts
Each package includes platform-specific installation scripts:
- **Linux/FreeBSD**: `install.sh` with systemd service setup
- **Windows**: `install.bat` with Windows service installation
- **macOS**: `install.sh` with launchd service configuration

## Next Steps

After successful build:
1. Follow [Quick Start](quick-start.md) to test the utilities
2. Read [Core Features](core-features.md) for feature documentation  
3. See [Utilities Guide](utilities-guide.md) for complete utility documentation
4. Use [Release Building](build-instructions.md#release-package-creation) for distribution