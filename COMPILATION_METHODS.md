# ScratchBird Compilation Methods & Procedures

**Generated**: July 15, 2025  
**Purpose**: Document successful compilation methods to avoid repeating discovery steps  
**Target**: ScratchBird v0.6.0 Development Build

---

## 📋 Prerequisites & Environment Setup

### **Required System Dependencies**
```bash
# Core build tools
sudo apt-get update
sudo apt-get install -y build-essential cmake git
sudo apt-get install -y gcc g++ make autoconf automake libtool
sudo apt-get install -y libncurses5-dev libreadline-dev
sudo apt-get install -y zlib1g-dev libssl-dev

# Additional libraries
sudo apt-get install -y libedit-dev libeditline-dev
sudo apt-get install -y libicu-dev
```

### **Build Environment Configuration**
```bash
export ROOT=/home/dcalford/Documents/claude/GitHubRepo/ScratchBird
export TARGET=Release
export SCRATCHBIRD_ROOT=${ROOT}/gen/Release/scratchbird
export LD_LIBRARY_PATH=${SCRATCHBIRD_ROOT}/lib:$LD_LIBRARY_PATH
export PATH=${SCRATCHBIRD_ROOT}/bin:$PATH
```

---

## 🔧 Critical Build System Fixes

### **1. autoconfig.auto Configuration**
**File**: `src/include/gen/autoconfig.auto`  
**Status**: CRITICAL - Must be configured before any build

**Required Definitions**:
```c
// System size definitions
#define SIZEOF_LONG 8
#define SIZEOF_SIZE_T 8
#define SIZEOF_VOID_P 8

// String function definitions
#define HAVE_STRCASECMP 1
#define HAVE_STRNCASECMP 1
#define HAVE_VSNPRINTF 1
#define HAVE_VA_COPY 1

// Time and timezone definitions
#define FB_TZDATADIR "/usr/share/zoneinfo"
#define HAVE_FTIME 1
#define HAVE_SYS_TIMEB_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_GETTIMEOFDAY 1

// System configuration
#define CASE_SENSITIVITY 1
#define SHRLIB_EXT "so"

// ScratchBird paths
#define FB_BINDIR "/usr/local/scratchbird/bin"
#define FB_SBINDIR "/usr/local/scratchbird/bin"
#define FB_CONFDIR "/usr/local/scratchbird/etc"
#define FB_LIBDIR "/usr/local/scratchbird/lib"
#define FB_INCDIR "/usr/local/scratchbird/include"
#define FB_DOCDIR "/usr/local/scratchbird/doc"
#define FB_UDFDIR "/usr/local/scratchbird/UDF"
#define FB_SAMPLEDIR "/usr/local/scratchbird/examples"
#define FB_SAMPLEDBDIR "/usr/local/scratchbird/examples/empbuild"
#define FB_HELPDIR "/usr/local/scratchbird/help"
#define FB_INTLDIR "/usr/local/scratchbird/intl"
#define FB_MISCDIR "/usr/local/scratchbird/misc"
#define FB_SECDBDIR "/usr/local/scratchbird/etc"
#define FB_MSGDIR "/usr/local/scratchbird/etc/msg"
#define FB_LOGDIR "/usr/local/scratchbird/log"
#define FB_GUARDDIR "/usr/local/scratchbird/bin"
#define FB_PLUGDIR "/usr/local/scratchbird/plugins"
#define FB_TZDIR "/usr/local/scratchbird/tzdata"
```

**Implementation**: This file must be manually created/updated before any build attempt.

### **2. Build System Path Configuration**
**File**: `gen/make.defaults`  
**Status**: CRITICAL - Must use scratchbird paths

**Required Updates**:
```makefile
FB_BUILD=$(GEN_ROOT)/$(TARGET)/scratchbird
SCRATCHBIRD=$(GEN_ROOT)/Native/scratchbird
FB_DAEMON = $(BIN)/scratchbird$(EXEC_EXT)
RE2_BUILD_FLG=N  # Disable re2 to avoid compatibility issues
```

### **3. External Dependencies Fix**
**File**: `extern/libcds/CMakeLists.txt`  
**Status**: REQUIRED - Must exist for build

**Content**:
```cmake
cmake_minimum_required(VERSION 3.10)
project(libcds)

# Stub library for compatibility
add_library(cds-s STATIC empty.cpp)
add_library(cds-s_d STATIC empty.cpp)

# Set output directory
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
```

**Empty Source**: `extern/libcds/empty.cpp`
```cpp
// Empty source file for libcds stub
```

---

## 🏗️ Successful Build Sequence

### **Phase 1: Bootstrap & External Dependencies**
```bash
cd ${ROOT}
make TARGET=Release external
```

**Expected Output**: All external dependencies built successfully
**Common Issues**: 
- Missing cmake for libcds → Create stub CMakeLists.txt
- Missing autoconfig.auto → Configure manually

### **Phase 2: Core Libraries**
```bash
make TARGET=Release yvalve
make TARGET=Release rest
```

**Expected Output**: Client library (libfbclient.so.0.6.0) created
**File Size**: ~28MB indicates successful build
**Test**: `ldd gen/Release/scratchbird/lib/libfbclient.so.0.6.0` should show no missing dependencies

### **Phase 3: GPRE Preprocessor**
```bash
make TARGET=Release boot
```

**Expected Output**: 
- `gen/Release/scratchbird/bin/gpre_boot` (~12MB)
- `gen/Release/scratchbird/bin/gpre_current` (symlink)

**Test**: `gpre_boot -z` should show version information
**Version**: Should display ScratchBird branding

### **Phase 4: Utilities (NON-GPRE)**
```bash
make TARGET=Release utilities
```

**Expected Output**: Successfully builds utilities that don't require GPRE processing
**Built Tools**:
- `scratchbird` (server daemon) - 16MB
- `fbguard` (guardian) - 8.7MB  
- `fbsvcmgr` (service manager) - 9.2MB
- `fbtracemgr` (trace manager) - 9.1MB
- `nbackup` (backup utility) - 9.4MB
- `gsplit` (split utility) - 8.7MB
- `fb_lock_print` (lock utility) - 8.3MB

**Common Issues**:
- `errno` redefinition in guard.cpp → Use `extern int errno;`
- Missing system headers → Update autoconfig.auto

### **Phase 5: Client Tools (GPRE-DEPENDENT) - CURRENTLY BLOCKED**
```bash
# These commands currently HANG due to GPRE preprocessing issues
make TARGET=Release sb_isql    # Hangs on extract.epp
make TARGET=Release sb_gbak    # Hangs on GPRE processing
make TARGET=Release sb_gfix    # Hangs on GPRE processing
make TARGET=Release sb_gsec    # Hangs on GPRE processing
make TARGET=Release sb_gstat   # Hangs on GPRE processing
```

**Status**: BLOCKED - GPRE hangs on specific .epp files
**Root Cause**: Unknown - GPRE preprocessing issue

---

## 🔍 Debugging Methods

### **GPRE Debugging**
```bash
# Test GPRE functionality
timeout 30 gpre_current -z  # Should show version
timeout 30 gpre_current -n -z -gds_cxx -ids test.epp test.cpp

# Common hanging files:
# - src/jrd/Function.epp
# - src/isql/extract.epp
# - Multiple client tool .epp files
```

### **Build Verification**
```bash
# Check executable sizes (indicates successful build)
ls -lh gen/Release/scratchbird/bin/
ls -lh gen/Release/scratchbird/lib/

# Test built tools
gen/Release/scratchbird/bin/scratchbird -z
gen/Release/scratchbird/bin/gpre_boot -z
```

### **Library Verification**
```bash
# Check library dependencies
ldd gen/Release/scratchbird/lib/libfbclient.so.0.6.0
ldd gen/Release/scratchbird/bin/scratchbird

# Verify ScratchBird branding
strings gen/Release/scratchbird/bin/scratchbird | grep -i scratchbird
```

---

## 📦 Release Packaging

### **Manual Release Creation**
```bash
# Create release directory
mkdir -p release/alpha0.6.0/{bin,lib,include,conf,doc}

# Copy built components
cp gen/Release/scratchbird/bin/* release/alpha0.6.0/bin/
cp gen/Release/scratchbird/lib/* release/alpha0.6.0/lib/
cp -r gen/Release/scratchbird/include/* release/alpha0.6.0/include/
cp gen/Release/scratchbird/*.conf release/alpha0.6.0/conf/

# Copy documentation
cp RELEASE_BLOCKERS.md TECHNICAL_DEBT.md release/alpha0.6.0/doc/
```

### **Release Verification**
```bash
# Check package size
du -sh release/alpha0.6.0/

# Count components
ls release/alpha0.6.0/bin/ | wc -l
ls release/alpha0.6.0/lib/ | wc -l

# Test key components
SCRATCHBIRD=release/alpha0.6.0 release/alpha0.6.0/bin/scratchbird -z
```

---

## ⚠️ Known Issues & Workarounds

### **GPRE Preprocessing Hang**
**Issue**: GPRE hangs on specific .epp files
**Workaround**: None currently available
**Impact**: Cannot build client tools
**Status**: CRITICAL BLOCKER

### **Tool Naming Inconsistency**
**Issue**: Some tools still use Firebird naming (fb_config, fbguard, etc.)
**Workaround**: Manual renaming in build system
**Impact**: Inconsistent user experience
**Status**: MAJOR ISSUE

### **Database Connectivity Requirements**
**Issue**: Some utilities require database connections for basic operations
**Workaround**: Test with proper database setup
**Impact**: Limited testing capability
**Status**: MODERATE ISSUE

---

## 🎯 Build Success Criteria

### **Successful Build Indicators**
1. **Client Library**: libfbclient.so.0.6.0 (~28MB)
2. **Server Daemon**: scratchbird (~16MB)
3. **GPRE Tool**: gpre_boot (~12MB)
4. **Utilities**: 5+ utilities built successfully
5. **No Missing Dependencies**: All tools run without library errors

### **Build Failure Indicators**
1. **GPRE Hang**: Indefinite hang on .epp processing
2. **Missing Libraries**: Utilities fail to start due to missing dependencies
3. **Compilation Errors**: C++ compilation failures
4. **Linker Errors**: Unresolved symbol errors

### **Quality Checks**
```bash
# Version consistency
grep -r "ScratchBird" release/alpha0.6.0/bin/ | wc -l

# No Firebird references in user-facing tools
grep -r "Firebird" release/alpha0.6.0/bin/ | grep -v "\.sh" | wc -l

# Library completeness
ldd release/alpha0.6.0/lib/libfbclient.so.0.6.0 | grep "not found" | wc -l
```

---

## 📚 Additional Resources

### **Build Logs**
- Keep build logs for debugging: `make TARGET=Release utilities 2>&1 | tee build.log`
- Monitor disk space: Builds can consume 2-3GB of space
- Check temp directory: `du -sh temp/Release/`

### **Configuration Files**
- `gen/make.defaults` - Main build configuration
- `gen/Makefile` - Build targets and rules
- `src/include/gen/autoconfig.auto` - System configuration

### **Critical Paths**
- Source: `src/`
- Build output: `gen/Release/scratchbird/`
- Temporary files: `temp/Release/`
- Release package: `release/alpha0.6.0/`

---

**Document Version**: 1.0  
**Last Updated**: July 15, 2025  
**Next Review**: After GPRE issues resolved  
**Status**: 🟡 ACTIVE DEVELOPMENT GUIDE