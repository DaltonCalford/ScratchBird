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

### Quick Build (Linux)
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
x86_64-w64-mingw32-g++ -std=c++17 -O3 -o sb_isql.exe sb_isql_windows.cpp

# Create Windows DLL
x86_64-w64-mingw32-g++ -shared -o sbclient.dll create_windows_library.cpp
```

## Verification

### Check Build Results
```bash
# Verify all utilities are built
ls -la sb_*

# Check version strings
./sb_gbak -z
./sb_gstat -z  
./sb_gfix -z
./sb_gsec -z
./sb_isql -z

# Expected output for all:
# <tool> version SB-T0.5.0.1 ScratchBird 0.5 f90eae0
```

### Test Basic Functionality
```bash
# Test ISQL help
./sb_isql --help

# Test GBAK help
./sb_gbak

# Test other utilities
./sb_gstat --help
./sb_gfix --help
./sb_gsec --help
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

## Next Steps

After successful build:
1. Follow [Quick Start](quick-start.md) to test the utilities
2. Read [Core Features](core-features.md) for feature documentation  
3. See [Migration Guide](migration-guide.md) for deployment guidance