# ScratchBird Build Instructions

## Overview

ScratchBird v0.5.0 features a modern GPRE-free build system with simplified compilation and cross-platform support.

## Directory Structure

### Enhanced Utilities Location
All ScratchBird Enhanced Utilities are located in:
```
src/utilities/
├── sb_gbak.cpp           # Enhanced backup/restore utility
├── sb_gstat.cpp          # Enhanced database statistics utility  
├── sb_gfix.cpp           # Enhanced database maintenance utility
├── sb_gsec.cpp           # Enhanced security management utility
├── sb_isql.cpp           # Enhanced interactive SQL utility
├── sb_guard.cpp          # Enhanced database guardian service
├── sb_svcmgr.cpp         # Enhanced service management utility
├── sb_tracemgr.cpp       # Enhanced trace analysis utility
├── sb_nbackup.cpp        # Enhanced incremental backup utility
├── sb_gssplit.cpp        # Enhanced file splitting utility
├── sb_lock_print.cpp     # Enhanced lock monitoring utility
├── scratchbird.cpp       # Master utility executable
└── archive/              # Original Firebird utilities (archived)
```

### Archived Original Utilities
Original Firebird utilities have been moved to:
```
src/utilities/archive/
├── fbsvcmgr/            # Original Firebird service manager
├── fbtracemgr/          # Original Firebird trace manager
├── guard/               # Original Firebird guardian
├── nbackup/             # Original Firebird nbackup
├── ntrace/              # Original Firebird trace plugin
├── fbcpl/               # Original Firebird control panel
├── ibmgr/               # Original InterBase manager
├── install/             # Original installation utilities
└── *.cpp                # Other original utility source files
```

**Note**: The archive directory preserves all original Firebird utility source code for reference and compatibility verification. The enhanced utilities provide 100% backward compatibility while adding modern features.

### Enhanced Utilities Features
Each enhanced utility includes:
- **100% Command-Line Compatibility**: All original Firebird switches supported
- **Modern Implementation**: C++17 with performance optimizations
- **Enhanced Capabilities**: Advanced features beyond original functionality
- **Cross-Platform Support**: Linux and Windows builds from single source
- **Standalone Compilation**: No GPRE dependencies required
- **Enterprise Features**: Advanced security, monitoring, and analytics

#### Complete Utility Suite (11 Enhanced Utilities):
1. **sb_gbak** - Advanced backup/restore with compression and parallel processing
2. **sb_gstat** - Enhanced database analysis with web interface and advanced statistics
3. **sb_gfix** - Multi-level validation and repair with backup integration
4. **sb_gsec** - Enterprise security with MFA, RBAC, and compliance frameworks
5. **sb_isql** - Enhanced SQL interface with hierarchical schema support
6. **sb_guard** - Multi-database monitoring with predictive analytics
7. **sb_svcmgr** - Advanced service management with queue optimization
8. **sb_tracemgr** - Performance and security analysis with predictive capabilities
9. **sb_nbackup** - 9-level incremental backup with encryption and validation
10. **sb_gssplit** - File operations with compression and integrity checking
11. **sb_lock_print** - Real-time lock monitoring with deadlock analysis

## Prerequisites

### Required Client Library
**CRITICAL**: sb_isql requires the Firebird client library for database connectivity.

**Location**: The build process expects the client library at:
```
release/alpha0.6.0/lib/libfbclient.so
```

**If missing**: 
- Build the complete ScratchBird project first to generate client libraries
- Or copy from existing Firebird installation: `cp /opt/firebird/lib/libfbclient.so* release/alpha0.6.0/lib/`
- Or install system Firebird development package: `sudo apt-get install libfirebird-dev`

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

# IMPORTANT: Check for required client library first
if [ ! -f "release/alpha0.6.0/lib/libfbclient.so" ]; then
    echo "ERROR: Firebird client library not found at release/alpha0.6.0/lib/libfbclient.so"
    echo "This library is required for sb_isql database connectivity."
    echo "Please ensure the ScratchBird release structure is complete."
    exit 1
fi

# Verify target directory exists
mkdir -p gen/Release/scratchbird/bin

# Build standalone utilities first (no dependencies)
echo "Building standalone utilities..."
g++ -std=c++17 -O3 -o sb_gbak src/utilities/sb_gbak.cpp
g++ -std=c++17 -O3 -o sb_gstat src/utilities/sb_gstat.cpp  
g++ -std=c++17 -O3 -o sb_gfix src/utilities/sb_gfix.cpp
g++ -std=c++17 -O3 -o sb_gsec src/utilities/sb_gsec.cpp
g++ -std=c++17 -O3 -o sb_guard src/utilities/sb_guard.cpp
g++ -std=c++17 -O3 -o sb_svcmgr src/utilities/sb_svcmgr.cpp
g++ -std=c++17 -O3 -o sb_tracemgr src/utilities/sb_tracemgr.cpp
g++ -std=c++17 -O3 -o sb_nbackup src/utilities/sb_nbackup.cpp
g++ -std=c++17 -O3 -o sb_gssplit src/utilities/sb_gssplit.cpp
g++ -std=c++17 -O3 -o sb_lock_print src/utilities/sb_lock_print.cpp
g++ -std=c++17 -O3 -o scratchbird src/utilities/scratchbird.cpp

# Build sb_isql with database connectivity (requires client library)
echo "Building sb_isql with database support..."
g++ -std=c++17 -O3 -o sb_isql src/utilities/sb_isql.cpp src/utilities/sb_database.cpp \
    -I./src/include -L./release/alpha0.6.0/lib -lfbclient -lreadline

# Verify all utilities were built successfully
echo "Verifying build results..."
for tool in sb_gbak sb_gstat sb_gfix sb_gsec sb_isql sb_guard sb_svcmgr sb_tracemgr sb_nbackup sb_gssplit sb_lock_print scratchbird; do
    if [ ! -f "$tool" ]; then
        echo "ERROR: Failed to build $tool"
        exit 1
    fi
    echo "✓ $tool built successfully"
done

# Move utilities to final location
echo "Installing utilities to gen/Release/scratchbird/bin/..."
mv sb_* scratchbird gen/Release/scratchbird/bin/

# Verify installation
echo "Verifying installation..."
cd gen/Release/scratchbird
for tool in bin/sb_*; do
    if [ -x "$tool" ]; then
        echo "✓ $(basename $tool) installed and executable"
    else
        echo "ERROR: $(basename $tool) not properly installed"
        exit 1
    fi
done

echo "✅ All utilities built and installed successfully!"
echo "Location: gen/Release/scratchbird/bin/"
echo "Test with: ./bin/sb_isql -z"
```

### Cross-Compilation (Windows)
```bash
# Build all Windows executables (11 utilities total)
x86_64-w64-mingw32-g++ -std=c++17 -O3 -o sb_gbak.exe src/utilities/sb_gbak.cpp
x86_64-w64-mingw32-g++ -std=c++17 -O3 -o sb_gstat.exe src/utilities/sb_gstat.cpp
x86_64-w64-mingw32-g++ -std=c++17 -O3 -o sb_gfix.exe src/utilities/sb_gfix.cpp
x86_64-w64-mingw32-g++ -std=c++17 -O3 -o sb_gsec.exe src/utilities/sb_gsec.cpp
x86_64-w64-mingw32-g++ -std=c++17 -O3 -o sb_isql.exe src/utilities/sb_isql.cpp
x86_64-w64-mingw32-g++ -std=c++17 -O3 -o sb_guard.exe src/utilities/sb_guard_windows.cpp -ladvapi32
x86_64-w64-mingw32-g++ -std=c++17 -O3 -o sb_svcmgr.exe src/utilities/sb_svcmgr.cpp
x86_64-w64-mingw32-g++ -std=c++17 -O3 -o sb_tracemgr.exe src/utilities/sb_tracemgr.cpp
x86_64-w64-mingw32-g++ -std=c++17 -O3 -o sb_nbackup.exe src/utilities/sb_nbackup.cpp
x86_64-w64-mingw32-g++ -std=c++17 -O3 -o sb_gssplit.exe src/utilities/sb_gssplit.cpp
x86_64-w64-mingw32-g++ -std=c++17 -O3 -o sb_lock_print.exe src/utilities/sb_lock_print.cpp
x86_64-w64-mingw32-g++ -std=c++17 -O3 -o scratchbird.exe src/utilities/scratchbird_windows.cpp -lws2_32 -ladvapi32

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
g++ -std=c++17 -g -O0 -DDEBUG -o sb_gbak src/utilities/sb_gbak.cpp

# Release build (default)
g++ -std=c++17 -O3 -DNDEBUG -o sb_gbak src/utilities/sb_gbak.cpp

# Size-optimized build
g++ -std=c++17 -Os -DNDEBUG -o sb_gbak src/utilities/sb_gbak.cpp
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

**Issue**: `undefined reference to isc_attach_database` and other Firebird API functions when building sb_isql
**Root Cause**: Missing or incompatible Firebird client library
**Solution**: Ensure proper client library is available:
```bash
# Check if client library exists
ls -la release/alpha0.6.0/lib/libfbclient.so*

# If missing, you may need to:
# 1. Build the full ScratchBird project first to generate client libraries
# 2. Or copy from a working Firebird installation:
#    cp /opt/firebird/lib/libfbclient.so* release/alpha0.6.0/lib/
# 3. Or use system Firebird library (if available):
#    sudo apt-get install libfirebird-dev
#    # Then modify build command to use -lfbclient without -L path
```

**Issue**: `isc_info_read_only not declared` in sb_database.cpp
**Root Cause**: API compatibility issue with newer Firebird versions
**Solution**: Already fixed in current codebase (changed to `isc_info_db_read_only`)

**Issue**: `deprecated isc_interprete function` warnings
**Root Cause**: Using older deprecated Firebird API functions
**Impact**: Warnings only - build still succeeds and functions correctly
**Solution**: Warnings can be safely ignored for now

### Verification Steps
1. Check all utilities show "ScratchBird 0.5" in version strings
2. Verify no Firebird references in user-facing output
3. Test basic functionality of each utility
4. Check library dependencies are correct
5. **Test sb_isql database connectivity**:
   ```bash
   cd gen/Release/scratchbird
   
   # Test version (should work regardless of database availability)
   ./bin/sb_isql -z
   
   # Test help (should show all database connection options)
   ./bin/sb_isql -?
   
   # Test interactive mode (should start properly and show SQL prompt)
   echo "QUIT" | ./bin/sb_isql
   
   # Test database connection (only if you have a test database)
   # ./bin/sb_isql -user SYSDBA -password masterkey /path/to/test.fdb
   ```

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
time g++ -std=c++17 -O3 -o sb_gbak src/utilities/sb_gbak.cpp

# Compare with other optimization levels
time g++ -std=c++17 -O0 -o sb_gbak_debug src/utilities/sb_gbak.cpp
time g++ -std=c++17 -O2 -o sb_gbak_o2 src/utilities/sb_gbak.cpp
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
# build_all.sh - Complete build script with error checking

set -e

echo "Building ScratchBird v0.5.0..."

# Check for required client library
echo "Checking for required client library..."
if [ ! -f "release/alpha0.6.0/lib/libfbclient.so" ]; then
    echo "ERROR: Firebird client library not found at release/alpha0.6.0/lib/libfbclient.so"
    echo "This library is required for sb_isql database connectivity."
    echo "Please ensure the ScratchBird release structure is complete."
    exit 1
fi
echo "✓ Client library found"

# Create target directory
mkdir -p gen/Release/scratchbird/bin

# Linux binaries (11 enhanced utilities)
echo "Building standalone utilities..."
g++ -std=c++17 -O3 -o sb_gbak src/utilities/sb_gbak.cpp
g++ -std=c++17 -O3 -o sb_gstat src/utilities/sb_gstat.cpp
g++ -std=c++17 -O3 -o sb_gfix src/utilities/sb_gfix.cpp
g++ -std=c++17 -O3 -o sb_gsec src/utilities/sb_gsec.cpp
g++ -std=c++17 -O3 -o sb_guard src/utilities/sb_guard.cpp
g++ -std=c++17 -O3 -o sb_svcmgr src/utilities/sb_svcmgr.cpp
g++ -std=c++17 -O3 -o sb_tracemgr src/utilities/sb_tracemgr.cpp
g++ -std=c++17 -O3 -o sb_nbackup src/utilities/sb_nbackup.cpp
g++ -std=c++17 -O3 -o sb_gssplit src/utilities/sb_gssplit.cpp
g++ -std=c++17 -O3 -o sb_lock_print src/utilities/sb_lock_print.cpp
g++ -std=c++17 -O3 -o scratchbird src/utilities/scratchbird.cpp

echo "Building sb_isql with database support..."
g++ -std=c++17 -O3 -o sb_isql src/utilities/sb_isql.cpp src/utilities/sb_database.cpp \
    -I./src/include -L./release/alpha0.6.0/lib -lfbclient -lreadline

# Windows binaries (if MinGW available)
if command -v x86_64-w64-mingw32-g++ &> /dev/null; then
    echo "Building Windows utilities..."
    x86_64-w64-mingw32-g++ -std=c++17 -O3 -o sb_gbak.exe src/utilities/sb_gbak.cpp
    x86_64-w64-mingw32-g++ -std=c++17 -O3 -o sb_gstat.exe src/utilities/sb_gstat.cpp
    x86_64-w64-mingw32-g++ -std=c++17 -O3 -o sb_gfix.exe src/utilities/sb_gfix.cpp
    x86_64-w64-mingw32-g++ -std=c++17 -O3 -o sb_gsec.exe src/utilities/sb_gsec.cpp
    x86_64-w64-mingw32-g++ -std=c++17 -O3 -o sb_isql.exe src/utilities/sb_isql.cpp
    x86_64-w64-mingw32-g++ -std=c++17 -O3 -o sb_guard.exe src/utilities/sb_guard_windows.cpp -ladvapi32
    x86_64-w64-mingw32-g++ -std=c++17 -O3 -o sb_svcmgr.exe src/utilities/sb_svcmgr.cpp
    x86_64-w64-mingw32-g++ -std=c++17 -O3 -o sb_tracemgr.exe src/utilities/sb_tracemgr.cpp
    x86_64-w64-mingw32-g++ -std=c++17 -O3 -o sb_nbackup.exe src/utilities/sb_nbackup.cpp
    x86_64-w64-mingw32-g++ -std=c++17 -O3 -o sb_gssplit.exe src/utilities/sb_gssplit.cpp
    x86_64-w64-mingw32-g++ -std=c++17 -O3 -o sb_lock_print.exe src/utilities/sb_lock_print.cpp
    x86_64-w64-mingw32-g++ -std=c++17 -O3 -o scratchbird.exe src/utilities/scratchbird_windows.cpp -lws2_32 -ladvapi32
    echo "Windows build complete."
else
    echo "MinGW not available, skipping Windows build."
fi

# Verify builds (all 11 enhanced utilities)
echo "Verifying builds..."
for tool in sb_gbak sb_gstat sb_gfix sb_gsec sb_isql sb_guard sb_svcmgr sb_tracemgr sb_nbackup sb_gssplit sb_lock_print scratchbird; do
    if [ -f "$tool" ]; then
        echo "✓ $tool built successfully"
        if ./"$tool" -z 2>/dev/null | grep -q "ScratchBird 0.5"; then
            echo "  Version check: PASS"
        else
            echo "  Version check: WARN (unexpected version string)"
        fi
    else
        echo "✗ $tool build failed"
        exit 1
    fi
done

# Move utilities to final location
echo "Installing utilities to gen/Release/scratchbird/bin/..."
mv sb_* scratchbird gen/Release/scratchbird/bin/

echo "✅ Build complete! All utilities installed and verified."
echo "Location: gen/Release/scratchbird/bin/"
echo "Test database connectivity: cd gen/Release/scratchbird && ./bin/sb_isql -z"
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
- **All 11 Enhanced ScratchBird Utilities**:
  - sb_gbak, sb_gstat, sb_gfix, sb_gsec, sb_isql
  - sb_guard, sb_svcmgr, sb_tracemgr, sb_nbackup
  - sb_gssplit, sb_lock_print
- **Master Utility**: scratchbird (unified interface)
- **Client Libraries**: libsbclient with version compatibility
- **Configuration Files**: scratchbird.conf and utility-specific configs
- **Installation Scripts**: install.sh/install.bat with service setup
- **Uninstallation Scripts**: Complete removal and cleanup
- **Complete Documentation**: User guides, admin manuals, API reference
- **Examples and Schemas**: Sample configurations and database schemas
- **Version Information**: Complete version tracking and compatibility matrix
- **Archive Reference**: Original Firebird utility source for compatibility verification

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