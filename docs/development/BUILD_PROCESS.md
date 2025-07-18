# ScratchBird v0.5.0 - Proper Build Process Documentation

## Overview

This document describes the correct build process for ScratchBird v0.5.0 that ensures:
- Clean compilation from `src/` directory
- Proper use of `temp/` directory for build artifacts
- Clean root directory with no build artifacts
- Proper output to `release/v0.5.0/` directories

## Build Script Chain

The ScratchBird build process follows this sequence:

### 1. configure_platform.sh
**Purpose**: Configure build system for target platform(s)
```bash
./configure_platform.sh linux    # Configure for Linux
./configure_platform.sh windows  # Configure for Windows cross-compilation
```

**What it does**:
- Detects system capabilities
- Sets up platform-specific build configurations
- Updates makefiles with correct platform settings

### 2. setup_dev_environment.sh
**Purpose**: Verify dependencies and create build configuration
```bash
./setup_dev_environment.sh
```

**What it does**:
- Checks for GCC, Make, MinGW (for Windows builds)
- Verifies system libraries (libcds, re2, readline)
- Creates `build_config.env` with detected settings
- Shows summary of available build targets

### 3. sb_build_all.sh
**Purpose**: Main build script - compiles all utilities cleanly
```bash
./sb_build_all.sh [options]

Options:
  -c, --clean         Clean build directories before building
  -l, --linux-only    Build only Linux utilities
  -w, --windows-only  Build only Windows utilities
  -v, --verbose       Verbose output
  -j, --jobs N        Number of parallel jobs
```

**What it does**:
- **CLEANS ROOT DIRECTORY** of any build artifacts
- Uses `temp/` directory for all compilation
- Builds from `src/` directory using existing build system
- Copies final binaries to `release/v0.5.0/linux-x86_64/bin/`
- **CLEANS ROOT DIRECTORY AGAIN** after build
- Verifies all expected utilities are built

### 4. create_linux_installer.sh
**Purpose**: Package Linux release into installable format
```bash
./create_linux_installer.sh
```

**What it does**:
- Creates tarball from `release/v0.5.0/linux-x86_64/`
- Includes installation script
- Places package in `releases/download/v0.5.0/`

### 5. create_windows_installer.sh
**Purpose**: Package Windows release into installable format
```bash
./create_windows_installer.sh
```

**What it does**:
- Creates ZIP archive from `release/v0.5.0/windows-x64/`
- Includes Windows installation script
- Places package in `releases/download/v0.5.0/`

### 6. complete_release.sh
**Purpose**: Final release verification and cleanup
```bash
./complete_release.sh
```

**What it does**:
- Verifies all release packages are complete
- Runs final cleanup of root directory
- Creates checksums and release notes
- Ensures release is ready for distribution

## Directory Structure

### Source Code (Read-Only During Build)
```
src/                          # Source code - never modified during build
├── common/                   # Common ScratchBird code
├── jrd/                      # Database engine
├── dsql/                     # SQL parser
├── isql/                     # Interactive SQL utility
├── utilities/                # Database utilities
└── include/                  # Header files
```

### Build Process (Temporary)
```
temp/                         # Build artifacts - cleaned between builds
├── Release/                  # Release build artifacts
│   └── scratchbird/         # Built binaries and libraries
└── Debug/                   # Debug build artifacts (if needed)
```

### Release Output (Final)
```
release/v0.5.0/
├── linux-x86_64/           # Linux release
│   ├── bin/                 # Compiled utilities
│   ├── lib/                 # Libraries
│   ├── include/             # Development headers
│   ├── doc/                 # Documentation
│   ├── examples/            # Usage examples
│   └── tests/               # Test suite
└── windows-x64/             # Windows release (cross-compiled)
    ├── bin/                 # Compiled utilities (.exe)
    ├── lib/                 # Libraries (.dll)
    └── ...                  # Same structure as Linux
```

### Distribution Packages
```
releases/download/v0.5.0/
├── scratchbird-v0.5.0-linux-x86_64.tar.gz     # Linux package
├── scratchbird-v0.5.0-windows-x64.zip         # Windows package
├── CHECKSUMS.md5                               # MD5 checksums
├── CHECKSUMS.sha256                            # SHA256 checksums
└── RELEASE_NOTES.md                            # Release notes
```

## Key Principles

### 1. Clean Root Directory
- **NO** build artifacts should ever remain in root directory
- **NO** .cpp, .exe, .dll, .so files in root
- **NO** temporary utilities (sb_*, debug_gpre*, etc.) in root
- Root should only contain source, documentation, and build scripts

### 2. Proper Temp Directory Usage
- All compilation happens in `temp/` directory
- `temp/` is cleaned between builds with `-c/--clean` option
- Existing build system uses `gen/` which maps to `temp/`

### 3. Source Directory Protection
- `src/` directory is never modified during build process
- All changes to source are done via proper development process
- Build scripts only read from `src/`, never write

### 4. Release Directory Structure
- `release/v0.5.0/` contains final, clean binaries
- Organized by platform: `linux-x86_64/`, `windows-x64/`
- Each platform has complete directory structure
- Includes all documentation, examples, and tests

## Common Issues and Solutions

### Issue: Build artifacts in root directory
**Solution**: Run `sb_build_all.sh` which automatically cleans root directory

### Issue: Missing utilities after build
**Cause**: Build system not finding source files or dependencies
**Solution**: 
1. Run `setup_dev_environment.sh` to verify dependencies
2. Use `sb_build_all.sh -v` for verbose output to see errors
3. Check that `src/` directory structure is intact

### Issue: Compilation errors during build
**Cause**: Missing dependencies or incorrect platform configuration
**Solution**:
1. Run `setup_dev_environment.sh` to check dependencies
2. Run `configure_platform.sh linux` to reconfigure
3. Use `sb_build_all.sh -c -v` for clean verbose build

### Issue: Cross-compilation for Windows fails
**Cause**: MinGW not installed or not configured
**Solution**:
1. Install MinGW: `sudo apt-get install mingw-w64`
2. Run `setup_dev_environment.sh` to verify
3. Run `configure_platform.sh windows`

## Build Verification

After running the build process, verify:

1. **Root directory is clean**:
   ```bash
   ls -la | grep -E '\.(cpp|exe|dll|so)$'  # Should return nothing
   ```

2. **Release directory has utilities**:
   ```bash
   ls -la release/v0.5.0/linux-x86_64/bin/
   # Should show: sb_isql, sb_gbak, sb_gfix, sb_gsec, sb_gstat
   ```

3. **Utilities are working**:
   ```bash
   release/v0.5.0/linux-x86_64/bin/sb_isql -z
   # Should show: sb_isql version SB-T0.5.0.1 ScratchBird 0.5 f90eae0
   ```

## Summary

The ScratchBird build process is designed to:
- Keep the root directory clean and professional
- Use proper temporary directories for compilation
- Produce clean, distributable release packages
- Maintain clear separation between source, build, and release

Always use the provided build scripts in order, and never manually copy build artifacts to the root directory.