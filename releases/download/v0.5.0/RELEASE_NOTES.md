# ScratchBird v0.5.0 - Release Notes

## Overview

ScratchBird v0.5.0 is a production-ready release featuring advanced PostgreSQL-compatible features, hierarchical schemas, and modern C++17 implementation.

## Download Links

### Linux
- **Linux x86_64**: `scratchbird-v0.5.0-linux-x86_64.tar.gz`
- **Linux ARM64**: `scratchbird-v0.5.0-linux-arm64.tar.gz`

### Windows
- **Windows x64**: `scratchbird-v0.5.0-windows-x64.zip`

### macOS
- **macOS x86_64**: `scratchbird-v0.5.0-macos-x86_64.tar.gz`
- **macOS ARM64**: `scratchbird-v0.5.0-macos-arm64.tar.gz`

### FreeBSD
- **FreeBSD x86_64**: `scratchbird-v0.5.0-freebsd-x86_64.tar.gz`

## Key Features

### ✅ Production-Ready Features
- **🌳 Hierarchical Schema System**: 8-level deep schema nesting exceeding PostgreSQL capabilities
- **📊 PostgreSQL-Compatible Data Types**: Network types, unsigned integers, range types, UUID IDENTITY
- **🔧 Modern GPRE-Free Architecture**: 96.3% code reduction with C++17 standards
- **🔤 PascalCase Object Identifiers**: SQL Server-style case-insensitive mode
- **🎯 SQL Dialect 4 Enhancements**: FROM-less SELECT, multi-row INSERT, comprehensive SYNONYM support
- **🔗 Schema-Aware Database Links**: 5 resolution modes for distributed scenarios
- **🛠️ Complete ScratchBird Branding**: Zero Firebird references, sb_ tool prefixes

### 🚀 Performance & Architecture
- **MVCC Transaction System**: Proven Firebird multi-generational architecture
- **Advanced Query Optimization**: Schema-aware query planning and execution
- **High-Performance Caching**: Multi-level cache hierarchy with lock-free structures
- **Cross-Platform Compatibility**: Native compilation for each platform
- **Enterprise Security**: Multi-plugin authentication with schema-level permissions

## Installation

### Quick Installation
```bash
# Linux
tar -xzf scratchbird-v0.5.0-linux-x86_64.tar.gz
cd scratchbird-v0.5.0-linux-x86_64
sudo ./install.sh

# Windows (as administrator)
# Extract ZIP and run install.bat

# macOS
tar -xzf scratchbird-v0.5.0-macos-x86_64.tar.gz
cd scratchbird-v0.5.0-macos-x86_64
./install.sh
```

### Verification
```bash
sb_isql -z
# Expected: sb_isql version SB-T0.5.0.1 ScratchBird 0.5 f90eae0
```

## Database Utilities

All packages include 12 comprehensive database utilities:

### Core Tools
- **scratchbird** - Main database server with hierarchical schema support
- **sb_isql** - Interactive SQL utility with schema awareness
- **sb_gbak** - Advanced backup/restore with schema preservation
- **sb_gfix** - Database maintenance and validation
- **sb_gsec** - Security management and user administration
- **sb_gstat** - Database statistics and performance analysis

### Advanced Tools
- **sb_guard** - Process monitor for server reliability
- **sb_svcmgr** - Service manager interface for administrative tasks
- **sb_tracemgr** - Database tracing and performance monitoring
- **sb_nbackup** - Incremental backup with schema support
- **sb_gssplit** - File splitting utility for large database files
- **sb_lock_print** - Lock analysis and transaction monitoring

## Technical Specifications

- **Build Version**: SB-T0.5.0.1 ScratchBird 0.5 f90eae0
- **Base Architecture**: Firebird 6.0.0.929 (f90eae0)
- **Language**: C++17 (GPRE-free)
- **Default Port**: 4050 (conflict-free with Firebird)
- **Service Name**: ScratchBird/sb_fdb
- **Maximum Schema Depth**: 8 levels
- **Maximum Schema Path**: 511 characters

## Verification

### Package Integrity
- **MD5 Checksums**: `CHECKSUMS.md5`
- **SHA256 Checksums**: `CHECKSUMS.sha256`


### System Requirements
- **Linux**: glibc 2.17+ (CentOS 7+, Ubuntu 16.04+)
- **Windows**: Windows 10/Server 2016+
- **macOS**: macOS 10.15+
- **FreeBSD**: FreeBSD 12.0+

## Support

- **Documentation**: Complete documentation included in each package
- **Examples**: Sample configurations and schemas
- **Issues**: Report at https://github.com/dcalford/ScratchBird/issues
- **Build from Source**: Use `./sb_build_all` and `./build_release`

## License

ScratchBird is released under the Initial Developer's Public License (IDPL), maintaining compatibility with the original Firebird licensing.

---

**Package Build Date**: Thu 17 Jul 2025 09:59:22 AM EDT
**Build System**: ScratchBird Release Builder v0.5.0
