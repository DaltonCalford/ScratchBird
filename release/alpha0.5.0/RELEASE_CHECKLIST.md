# ScratchBird Alpha 0.5.0 Release Checklist

## Current Status: Infrastructure Complete ✅

### ✅ COMPLETED
- [x] Proper release directory structure (`./release/alpha0.5.0/`)
- [x] Git-trackable release (not in .gitignore)
- [x] ScratchBird branding (no Firebird references)
- [x] Configuration files properly renamed
- [x] Release documentation (README.md, VERSION)
- [x] Build system with package-release target
- [x] Directory structure for both platforms

## MISSING COMPONENTS

### 🐧 Linux Distribution
**Core Executables** (❌ Missing):
- [ ] `bin/sb_isql` - Interactive SQL client
- [ ] `bin/sb_gbak` - Backup/restore utility
- [ ] `bin/sb_gfix` - Database maintenance
- [ ] `bin/sb_gsec` - Security management
- [ ] `bin/sb_gstat` - Database statistics
- [ ] `bin/scratchbird` - Database server daemon
- [ ] `bin/gpre` - Preprocessor tool

**Libraries** (❌ Missing):
- [ ] `lib/libsbclient.so` - Client library
- [ ] `lib/libsbclient.so.0.5.0` - Versioned client lib
- [ ] `lib/libsbembed.so` - Embedded database library
- [ ] `lib/libib_util.so` - Utility library

**Linux-Specific Additions Needed**:
- [ ] `lib/libsbclient.so.2` - Compatibility symlink
- [ ] `share/man/man1/sb_*.1` - Manual pages
- [ ] `etc/systemd/system/scratchbird.service` - Service file
- [ ] `etc/bash_completion.d/scratchbird` - Shell completion
- [ ] `bin/sb_svcmgr` - Service manager

### 🪟 Windows Distribution  
**Core Executables** (❌ Missing):
- [ ] `bin/sb_isql.exe` - Interactive SQL client
- [ ] `bin/sb_gbak.exe` - Backup/restore utility
- [ ] `bin/sb_gfix.exe` - Database maintenance
- [ ] `bin/sb_gsec.exe` - Security management
- [ ] `bin/sb_gstat.exe` - Database statistics
- [ ] `bin/scratchbird.exe` - Database server
- [ ] `bin/gpre.exe` - Preprocessor tool

**Libraries** (❌ Missing):
- [ ] `bin/sbclient.dll` - Client library
- [ ] `bin/sbembed.dll` - Embedded database library
- [ ] `bin/ib_util.dll` - Utility library

**Windows-Specific Additions Needed**:
- [ ] `bin/scratchbird_service.exe` - Windows service
- [ ] `bin/sb_regserver.exe` - Registry management
- [ ] `bin/install.bat` - Windows installer script
- [ ] `bin/uninstall.bat` - Windows uninstaller
- [ ] `setup.exe` - Installation wizard
- [ ] `scratchbird.msi` - MSI installer package

### 📚 Documentation & Examples
**Missing Documentation**:
- [ ] `doc/README.txt` - Plain text readme
- [ ] `doc/INSTALL.txt` - Installation instructions
- [ ] `doc/sql-extensions.txt` - ScratchBird SQL features
- [ ] `doc/migration-guide.txt` - Firebird migration
- [ ] `doc/hierarchical-schemas.txt` - Schema documentation
- [ ] `doc/database-links.txt` - Database links guide

**Missing Examples**:
- [ ] `examples/hierarchical-schemas.sql` - Schema examples
- [ ] `examples/database-links.sql` - Link examples
- [ ] `examples/uuid-identity.sql` - UUID examples
- [ ] `examples/unsigned-integers.sql` - Type examples

## BUILD REQUIREMENTS

### To Complete Linux Release:
```bash
# Build all core tools
make TARGET=Release sb_isql sb_gbak sb_gfix sb_gsec sb_gstat
make TARGET=Release scratchbird gpre

# Build client libraries  
make TARGET=Release libsbclient libsbembed

# Package complete release
make TARGET=Release package-release
```

### To Complete Windows Release:
```bash
# Cross-compile for Windows (MinGW)
make TARGET=Release CROSS_OUT=Y sb_isql sb_gbak sb_gfix sb_gsec sb_gstat
make TARGET=Release CROSS_OUT=Y scratchbird gpre

# Build Windows libraries
make TARGET=Release CROSS_OUT=Y sbclient.dll sbembed.dll

# Create Windows installer
./create_windows_installer.sh
```

## VERIFICATION COMMANDS

```bash
# Verify Linux release
./complete_release.sh

# Check Windows binaries (if cross-compiled)
file release/alpha0.5.0/bin/*.exe
ldd release/alpha0.5.0/bin/*.dll

# Test core functionality
release/alpha0.5.0/bin/sb_isql -z
release/alpha0.5.0/bin/sb_gbak -z
```

## RELEASE READINESS

**Current Status**: 🟡 **PARTIAL** - Infrastructure Complete
- ✅ Release structure ready for both platforms
- ✅ Configuration and documentation complete  
- ❌ Core executables and libraries missing
- ❌ Platform-specific packaging incomplete

**Next Steps**:
1. Complete core tool compilation
2. Build client libraries
3. Add platform-specific files
4. Create installation packages
5. Verify functionality on both platforms

**Timeline**: Infrastructure ready now, executables pending successful build completion.