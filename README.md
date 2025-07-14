# ScratchBird
An experimental, refactored fork of FirebirdSQL—built to scratch an itch.

[![Build Status](https://img.shields.io/badge/build-in%20progress-orange)](https://github.com/dcalford/ScratchBird) [![License](https://img.shields.io/badge/license-IDPL-blue)](LICENSE) [![Version](https://img.shields.io/badge/version-0.5--alpha-red)](CHANGELOG.md)

## What is ScratchBird?

ScratchBird is a fork of the powerful and mature FirebirdSQL database. This project involves a significant refactoring of the original codebase with the goal of cleaning up internals, modernizing components, and implementing new features. It's a passion project driven by curiosity and the desire to experiment.

**Important Note on Contributions**: ScratchBird is a divergent fork and is not intended to be merged back into the main FirebirdSQL codebase. The project's experimental nature means that core components are changing daily, and the architecture has drifted significantly from the original.

While we are grateful for the Firebird foundation, the goal of ScratchBird is to be a separate, exploratory project. We encourage contributions within the context of ScratchBird's unique direction.

## The "Itch to Scratch" Philosophy 🤔

The name ScratchBird was chosen for two main reasons:

**To Scratch an Itch**: This fork was started because I have a personal 'itch' to explore database internals, implement modern features, and refactor a large, established codebase. It's a journey of learning and experimentation.

**To Avoid Confusion**: To respect the official FirebirdSQL project and its community, a distinct name and branding are essential. ScratchBird is not Firebird. By using a different name, we ensure that users and testers don't confuse this experimental version with the stable, production-ready releases of Firebird.

## Key Features & Goals 🚀

### 📊 Advanced Datatype System (v0.5)
- **Range Types**: INT4RANGE, INT8RANGE, NUMRANGE, TSRANGE, TSTZRANGE, DATERANGE with PostgreSQL-compatible operators
- **Case-Insensitive Text**: CITEXT datatype with automatic lowercase indexing
- **Advanced Arrays**: Multi-dimensional arrays with slicing operations and PostgreSQL-style operators
- **Full-Text Search**: TSVECTOR and TSQUERY with text processing, ranking, and highlighting
- **Network Types**: INET, CIDR, MACADDR datatypes (PostgreSQL-compatible) *(planned)*
- **Unsigned Integers**: USMALLINT, UINTEGER, UBIGINT, UINT128 datatypes *(planned)*
- **Enhanced VARCHAR**: 128KB UTF-8 support (vs 32KB limit) *(planned)*

### 🌳 Hierarchical Schema System (COMPLETED)
- **8-level deep schema nesting**: `finance.accounting.reports.table`
- **PostgreSQL-style qualified names**: Exceeds PostgreSQL's flat schema capabilities
- **Schema-aware database links**: 5 resolution modes for distributed database scenarios
- **Administrative functions**: Complete management and validation system

### 🔤 PascalCase Object Identifiers (COMPLETED)
- **Database-level case sensitivity control**: SQL Server-style identifier behavior
- **CREATE DATABASE syntax**: `PASCAL CASE IDENTIFIERS` clause for case-insensitive mode
- **Runtime detection**: `CURRENT_PASCAL_CASE_MODE` system context variable
- **Flexible identifier matching**: Choose between case-sensitive (default) or case-insensitive modes

### 🎯 SQL Dialect 4 Enhancements (COMPLETED)
- **FROM-less SELECT statements**: `SELECT GEN_UUID();`
- **Multi-row INSERT VALUES**: `INSERT INTO table VALUES (1,2,3),(4,5,6);`
- **Comprehensive SYNONYM support**: Schema-aware object aliasing

### 🔧 Modernized Tooling (COMPLETED)
- **Prefixed tools**: All command-line tools renamed with `sb_` prefix to prevent conflicts
- **Enhanced ISQL**: Schema commands, hierarchical navigation
- **Updated GBAK**: Schema-aware backup and restore
- **Complete tool suite**: `sb_isql`, `sb_gbak`, `sb_gfix`, `sb_gsec`, `sb_gstat`, etc.

### 🔐 Enhanced Security Features (COMPLETED)
- **Trusted authentication**: GSSAPI/Kerberos integration
- **Advanced authentication plugins**: Multi-factor authentication support
- **Schema-level security**: Granular access control for hierarchical schemas

## Getting Started (Build Instructions)

### Prerequisites
```bash
# Ubuntu/Debian
sudo apt-get install build-essential cmake libedit-dev libicu-dev

# CentOS/RHEL/Fedora
sudo yum install gcc-c++ cmake libedit-devel libicu-devel
```

### Build Process
```bash
# Clone the repository
git clone https://github.com/dcalford/ScratchBird.git
cd ScratchBird

# Configure the build
./configure --enable-superserver --with-system-editline

# Compile (parallel build recommended)
make TARGET=Release -j$(nproc)

# Test the build
./verify_build.sh
```

### Quick Usage Examples
```bash
# Start interactive shell
./gen/Release/scratchbird/bin/sb_isql

# Create database with PascalCase identifiers (case-insensitive)
CREATE DATABASE "myapp.fdb" PASCAL CASE IDENTIFIERS;

# Create hierarchical schema (SQL Dialect 4)
CREATE SCHEMA finance;
CREATE SCHEMA finance.accounting;
CREATE SCHEMA finance.accounting.reports;

# Use new datatypes
CREATE TABLE finance.accounting.reports.monthly_data (
    id INTEGER,
    date_range DATERANGE,
    description CITEXT,
    tags INT4RANGE,
    search_data TSVECTOR
);

# PascalCase mode: these are all equivalent when enabled
CREATE TABLE MyTable (ID INTEGER, ColumnName VARCHAR(50));
INSERT INTO mytable (id, columnname) VALUES (1, 'test');
SELECT ID, COLUMNNAME FROM MYTABLE;

# Check PascalCase mode status
SELECT CURRENT_PASCAL_CASE_MODE FROM RDB$DATABASE;

# FROM-less SELECT statements
SELECT GEN_UUID();
SELECT CURRENT_TIMESTAMP;

# Multi-row INSERT
INSERT INTO mytable(id, name) VALUES (1,'Alice'), (2,'Bob'), (3,'Carol');
```

## Current Development Status

### ✅ Completed Features
- **Core Infrastructure**: Hierarchical schemas, SQL Dialect 4, tool modernization
- **Range Types**: Complete PostgreSQL-compatible range system with 6 range types
- **CITEXT**: Case-insensitive text with indexing support
- **Advanced Arrays**: Multi-dimensional arrays with slicing operations
- **Full-Text Search**: TSVECTOR/TSQUERY with text processing capabilities
- **Database Links**: Schema-aware distributed database connectivity
- **PascalCase Identifiers**: SQL Server-style case-insensitive object identifier support

### 🔄 In Progress (v0.5)
- **Enhanced VARCHAR**: 128KB UTF-8 support implementation (core changes complete)
- **Compilation Testing**: Core new datatypes and identifier system compiling successfully
- **Integration Testing**: Validating datatype infrastructure and PascalCase integration
- **String API Compatibility**: Resolving ScratchBird::string vs std::string issues

### 📋 Planned Features
- **Unsigned Integer Types**: USMALLINT, UINTEGER, UBIGINT, UINT128
- **Network Address Types**: Complete INET, CIDR, MACADDR implementation
- **Type Conversion System**: Complete cvt.cpp integration for all new types
- **Performance Optimizations**: Enhanced indexing for case-insensitive identifiers

## Relationship to FirebirdSQL

ScratchBird would not be possible without the decades of incredible work done by the FirebirdSQL team. This project is a direct fork of their work and gratefully retains the original Mozilla Public License (MPL) and InterBase Public License (IDPL).

**Key Acknowledgments**:
- Based on Firebird 6.0.0.929 (f90eae0)
- Preserves all original licensing and attribution
- Maintains compatibility with Firebird's proven architecture
- Extends rather than replaces core functionality

## Development & Methodology

This project leverages modern AI assistants, including Anthropic's Claude, to accelerate development. Their role is primarily focused on performing systematic work, such as:

- **Code Generation**: Creating comprehensive datatype implementations following established patterns
- **Compilation Issues**: Resolving build errors and template specialization challenges
- **Research & Documentation**: Analyzing PostgreSQL specifications for compatibility features
- **Testing & Validation**: Identifying edge cases and integration issues

The core architectural decisions, feature design, and project direction for ScratchBird are human-led. AI is used as a sophisticated tool to handle systematic implementation tasks, allowing development to focus on high-level database engineering challenges.

**For full transparency**: AI tools are an integral part of this project's development workflow. We respect all viewpoints on this topic, but this is a core aspect of our methodology.

## Documentation

- [Advanced Datatype System](doc/README.advanced_datatypes.md) *(planned)*
- [Hierarchical Schemas Guide](doc/README.hierarchical_schemas.md)
- [SQL Dialect 4 Reference](doc/README.sql_dialect_4.md)
- [PascalCase Identifiers Guide](doc/README.pascalcase_identifiers.md) *(planned)*
- [Build Instructions](doc/README.build.posix.html)
- [Migration Guide](MIGRATION_GUIDE.md)

## PascalCase Object Identifiers Technical Details

### Overview
ScratchBird v0.5 introduces SQL Server-style case-insensitive object identifier support, controllable at the database level. This feature allows developers to choose between traditional case-sensitive Firebird behavior or case-insensitive SQL Server-like behavior.

### Usage
```sql
-- Create database with case-insensitive identifiers
CREATE DATABASE "application.fdb" PASCAL CASE IDENTIFIERS;

-- Now these are all equivalent:
CREATE TABLE MyTable (ColumnName VARCHAR(50), ID INTEGER);
INSERT INTO mytable (columnname, id) VALUES ('test', 1);
SELECT COLUMNNAME, id FROM MYTABLE;

-- Runtime detection
SELECT CURRENT_PASCAL_CASE_MODE FROM RDB$DATABASE;  -- Returns TRUE
```

### Implementation Architecture

**Database Header Flag**
- Stored as `hdr_pascal_case_identifiers = 0x80` in database header (ODS level)
- Runtime flag: `DBB_pascal_case_identifiers = 0x200L` in Database class
- Persistent across database sessions and restarts

**Identifier Resolution Engine**
- Modified `MetaName::compare()` method with conditional logic
- Case-sensitive mode (default): Uses fast `memcmp()` comparison
- Case-insensitive mode: Uses `strncasecmp()` for SQL Server behavior
- Consistent across all database objects: tables, columns, procedures, functions

**System Integration**
- `CURRENT_PASCAL_CASE_MODE` context variable for runtime detection
- Integrated with Dictionary-based identifier caching system
- Minimal performance overhead with conditional branching

## Contributing

We welcome contributions that align with ScratchBird's experimental direction! Please note:

- **Focus Areas**: Advanced datatypes, PostgreSQL compatibility, performance optimizations
- **Code Style**: Follow established ScratchBird patterns (see examples in `/src/common/`)
- **Testing**: All new features should include comprehensive test cases
- **Documentation**: Update relevant documentation for user-facing changes

## ⚠️ Disclaimer

**Please be aware that ScratchBird is an experimental project and is not recommended for production environments.** It is intended for learning, testing, and development purposes. Use at your own risk.

**Key Considerations**:
- **Alpha Software**: Expect breaking changes and instability
- **No Production Support**: This is a research and learning project
- **Experimental Features**: New datatypes and features are still under development
- **Backup Strategy**: Always maintain backups when testing database features

## License

ScratchBird is released under the Initial Developer's Public License (IDPL), maintaining compatibility with the original Firebird licensing.