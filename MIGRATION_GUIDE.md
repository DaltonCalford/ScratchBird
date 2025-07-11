# ScratchBird Migration Guide

## Overview

This guide details the complete migration process from Firebird 6.0.0.929 to ScratchBird v0.5, including SQL Dialect 4 enhancements, hierarchical schemas, and tool renaming.

## Migration Strategy

### Phase 1: Project Analysis ✅ Complete

**Source Analysis:**
- **Total Project Size**: ~410MB (including build artifacts)
- **Core Source Code**: 31MB (1,289 files)
- **External Dependencies**: 165MB (6,612 files)
- **Essential Components**: ~200MB (minimum viable product)

**Key Findings:**
- 45MB of generated files that can be excluded
- 122MB of temporary/build artifacts to exclude
- Core functionality requires ~200MB for complete migration

### Phase 2: Directory Structure ✅ Complete

**ScratchBird Directory Layout:**
```
ScratchBird/
├── builds/                    # Build system configurations
│   ├── cmake/                 # Modern CMake build system
│   ├── posix/                 # POSIX/Linux build files
│   └── install/               # Installation scripts
├── src/                       # Core source code (31MB)
│   ├── jrd/                   # Database engine with hierarchical schemas
│   ├── dsql/                  # SQL parser with Dialect 4 support
│   ├── common/                # Shared utilities and libraries
│   ├── auth/                  # Authentication systems
│   └── [tools]/               # Renamed tools (sb_*)
├── extern/                    # External dependencies (165MB)
│   ├── boost/                 # C++ Boost libraries
│   ├── decNumber/             # Decimal arithmetic
│   ├── icu/                   # Unicode support
│   └── [others]/              # Additional dependencies
├── doc/                       # Documentation
├── examples/                  # Code examples
└── tests/                     # Test suites
```

### Phase 3: Tool Renaming Strategy ✅ Complete

**Firebird → ScratchBird Tool Mapping:**

| Original | ScratchBird | Purpose |
|----------|-------------|---------|
| `isql` | `sb_isql` | Interactive SQL shell |
| `gbak` | `sb_gbak` | Database backup/restore |
| `gfix` | `sb_gfix` | Database repair utility |
| `gsec` | `sb_gsec` | User management |
| `gstat` | `sb_gstat` | Database statistics |
| `nbackup` | `sb_nbackup` | Online backup |
| `fbsvcmgr` | `sb_svcmgr` | Service manager |
| `fbtracemgr` | `sb_tracemgr` | Trace manager |

## Migration Scripts

### 1. Primary Migration Script

**File**: `migrate_to_scratchbird.sh`
**Purpose**: Complete project migration with exclusions

**Key Features:**
- Copies essential source code and dependencies
- Excludes build artifacts and temporary files
- Renames Firebird references to ScratchBird
- Creates ScratchBird-specific documentation
- Generates installation scripts

**Usage:**
```bash
chmod +x migrate_to_scratchbird.sh
./migrate_to_scratchbird.sh
```

**Exclusions Applied:**
- `*.o`, `*.a`, `*.so`, `*.exe` (compiled binaries)
- `CMakeCache.txt`, `CMakeFiles/` (CMake artifacts)
- `build_test/`, `temp/`, `gen/` (build directories)
- `*.log`, `*.bak`, `*.tmp` (log and backup files)

### 2. Build System Adaptation Script

**File**: `adapt_build_system.sh`
**Purpose**: Adapts build system for ScratchBird branding

**Key Features:**
- Updates CMakeLists.txt files for tool renaming
- Modifies Makefiles for ScratchBird branding
- Updates source code version strings
- Creates ScratchBird version header
- Generates build verification script

**Usage:**
```bash
chmod +x adapt_build_system.sh
./adapt_build_system.sh
```

## File Analysis and Exclusions

### Essential Files (MIGRATE)

**Core Database Engine:**
- `src/jrd/` - Database engine core (318 files)
- `src/dsql/` - SQL parser with Dialect 4 (63 files)
- `src/common/` - Common utilities (240 files)
- `src/yvalve/` - Client interface (28 files)
- `src/remote/` - Network protocol (43 files)

**Tools and Utilities:**
- `src/isql/` - Interactive SQL (21 files) → `sb_isql`
- `src/burp/` - Backup/restore (18 files) → `sb_gbak`
- `src/alice/` - Repair tool (9 files) → `sb_gfix`
- `src/utilities/` - Database utilities → `sb_*` tools

**Build System:**
- `builds/cmake/` - Modern build system
- `builds/posix/` - POSIX build configuration
- `CMakeLists.txt`, `configure`, `Makefile.in`

**External Dependencies:**
- `extern/boost/` - C++ Boost (3,683 files)
- `extern/decNumber/` - Decimal arithmetic (31 files)
- `extern/icu/` - Unicode support (23 files)
- `extern/editline/` - Command line editing (152 files)
- `extern/libtomcrypt/` - Cryptography (936 files)
- `extern/btyacc/` - Parser generator (47 files)
- `extern/cloop/` - Code generator (36 files)

### Legacy/Excludable Files (EXCLUDE)

**Build Artifacts:**
- `build_test/` - Test artifacts (812K)
- `temp/` - Temporary files (122M)
- `gen/` - Generated files (45M)
- All compiled binaries (`*.o`, `*.a`, `*.so`, `*.exe`)
- CMake cache files (`CMakeCache.txt`, `CMakeFiles/`)

**Optional Components:**
- `extern/libcds/` - Lock-free structures (1,247 files) - Evaluate
- `extern/re2/` - Regular expressions (167 files) - Evaluate
- `doc/` - Documentation (1.3M) - Optional for minimal build
- `examples/` - Code examples (1.1M) - Optional

**Platform-Specific:**
- `builds/win32/` - Windows build system (Linux-only deployment)
- `src/iscguard/` - Windows guardian process

## SQL Dialect 4 Features

ScratchBird v0.5 includes comprehensive SQL Dialect 4 implementation:

### 1. FROM-less SELECT Statements
```sql
-- Traditional Firebird (still supported)
SELECT GEN_UUID() FROM RDB$DATABASE;

-- ScratchBird Dialect 4 (new)
SELECT GEN_UUID();
SELECT CURRENT_TIMESTAMP;
SELECT 'Hello World';
```

### 2. Multi-row INSERT VALUES
```sql
-- Traditional (still supported)
INSERT INTO employees(id, name) VALUES (1, 'Alice');
INSERT INTO employees(id, name) VALUES (2, 'Bob');

-- ScratchBird Dialect 4 (new)
INSERT INTO employees(id, name) VALUES (1, 'Alice'), (2, 'Bob'), (3, 'Carol');
```

### 3. Comprehensive SYNONYM Support
```sql
-- Create synonyms for schema-aware development
CREATE SYNONYM emp_data FOR hr.employees;
CREATE SYNONYM fin_reports FOR finance.accounting.reports;
CREATE SYNONYM current_schema FOR finance.accounting;

-- Use synonyms in queries
SELECT * FROM emp_data WHERE department = 'IT';
SELECT * FROM fin_reports.monthly_summary;
```

## Hierarchical Schema System

### 8-Level Deep Schema Nesting
```sql
-- Create hierarchical schema structure
CREATE SCHEMA company;
CREATE SCHEMA company.division;
CREATE SCHEMA company.division.department;
CREATE SCHEMA company.division.department.team;

-- Use qualified names
CREATE TABLE company.division.department.employees (
    id INTEGER,
    name VARCHAR(100)
);

-- Query with full path
SELECT * FROM company.division.department.employees;
```

### Schema Navigation Commands (sb_isql)
```sql
-- Set current working schema
SET SCHEMA company.division;

-- Set home schema for relative navigation
SET HOME SCHEMA company;

-- Show current schema context
SHOW SCHEMA;
SHOW HOME SCHEMA;
```

## Database Links with Schema Resolution

### 5 Schema Resolution Modes
```sql
-- Fixed schema mapping
CREATE DATABASE LINK finance_link 
  TO 'server2:finance_db' 
  SCHEMA_MODE FIXED 
  REMOTE_SCHEMA 'accounting.reports';

-- Hierarchical mapping
CREATE DATABASE LINK hr_link 
  TO 'hr_server:hr_db'
  SCHEMA_MODE HIERARCHICAL
  LOCAL_SCHEMA 'hr'
  REMOTE_SCHEMA 'human_resources';

-- Context-aware resolution
CREATE DATABASE LINK user_link
  TO 'user_server:user_db'
  SCHEMA_MODE CONTEXT_AWARE
  REMOTE_SCHEMA 'CURRENT';
```

## Testing the Migration

### Build Verification
```bash
# Run build verification
./verify_build.sh

# Expected output:
# ✅ CMake found
# ✅ Make found  
# ✅ GCC found
# ✅ Required libraries found
```

### Build Process
```bash
# Create build directory
mkdir -p build && cd build

# Configure with CMake
cmake ..

# Build ScratchBird
make -j$(nproc)

# Test basic functionality
./src/isql/sb_isql -? 
./src/burp/sb_gbak -?
```

### Installation
```bash
# Install ScratchBird system-wide
sudo make install

# Or use custom installation script
./builds/install/install_scratchbird.sh
```

### Feature Testing
```bash
# Start interactive shell
sb_isql

# Test SQL Dialect 4 features
SELECT GEN_UUID();
SELECT CURRENT_TIMESTAMP;

# Test hierarchical schemas
CREATE SCHEMA test;
CREATE SCHEMA test.sub;
CREATE TABLE test.sub.data(id INT);

# Test synonyms  
CREATE SYNONYM test_data FOR test.sub.data;
SELECT * FROM test_data;
```

## Post-Migration Checklist

### ✅ Core Functionality
- [ ] Database engine starts successfully
- [ ] sb_isql connects and executes queries
- [ ] Basic SQL operations work (SELECT, INSERT, UPDATE, DELETE)
- [ ] Transaction management functions correctly

### ✅ SQL Dialect 4 Features  
- [ ] FROM-less SELECT statements execute
- [ ] Multi-row INSERT VALUES works
- [ ] SYNONYM DDL operations succeed
- [ ] Schema-aware synonym resolution functions

### ✅ Hierarchical Schemas
- [ ] Nested schema creation works
- [ ] Qualified name resolution functions
- [ ] Schema navigation commands work in sb_isql
- [ ] Deep hierarchy queries execute

### ✅ Database Links
- [ ] Schema-aware link creation succeeds
- [ ] Remote schema resolution works
- [ ] Cross-database queries execute
- [ ] Context-aware schema references function

### ✅ Tools and Utilities
- [ ] All sb_* tools execute without errors
- [ ] sb_gbak backup/restore operations work
- [ ] sb_gfix database validation succeeds
- [ ] sb_gsec user management functions

### ✅ Build System
- [ ] CMake configuration succeeds
- [ ] Make builds without errors
- [ ] Installation scripts work correctly
- [ ] System-wide tool access functions

## Troubleshooting

### Common Issues

**Build Errors:**
- Missing dependencies: Run `./verify_build.sh` to check requirements
- CMake errors: Ensure CMake 3.10+ is installed
- Linker errors: Check that all external libraries are present

**Runtime Errors:**
- Library not found: Check LD_LIBRARY_PATH includes ScratchBird lib directory
- Permission denied: Ensure proper file permissions on executables
- Database connection errors: Verify database files exist and are accessible

**SQL Dialect 4 Issues:**
- FROM-less SELECT errors: Ensure connection uses SQL Dialect 4
- Synonym resolution errors: Check schema path and synonym definitions
- Hierarchical schema errors: Verify schema hierarchy is properly created

### Support Resources

- **Build Issues**: Check `verify_build.sh` output for missing dependencies
- **SQL Dialect 4**: Review `doc/README.sql_dialect_4.md`
- **Hierarchical Schemas**: See `doc/README.hierarchical_schemas.md`
- **Database Links**: Consult database link documentation

## Migration Summary

**Total Effort**: 4 phases covering analysis, structure, scripts, and testing
**Size Reduction**: ~410MB → ~200MB (51% reduction through exclusions)
**Tool Renaming**: 8 core tools renamed with `sb_` prefix
**Feature Additions**: SQL Dialect 4, hierarchical schemas, enhanced database links
**Build System**: Fully adapted CMake and Make configurations
**Documentation**: Complete migration guide and feature documentation

The ScratchBird migration provides a clean, modern database system based on Firebird's proven architecture while adding significant enhancements for contemporary development needs.