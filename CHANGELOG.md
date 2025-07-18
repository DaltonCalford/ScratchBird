# ScratchBird Changelog

All notable changes to ScratchBird will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.6.0] - TBD - Development Version

### 🚀 Development in Progress

This is the next major version of ScratchBird currently under development. Features are being added incrementally.

#### Planned Features
- Advanced query optimization enhancements
- Extended PostgreSQL compatibility features
- Performance improvements for hierarchical schemas
- Enhanced database link functionality
- Additional network data type operations
- Improved GPRE-free architecture optimizations

#### Work in Progress
- Schema path cache performance optimizations
- DatabaseLink DDL enhancements
- Extended SQL Dialect 4 features

### 📝 Notes
- This is a development version - not recommended for production use
- Features may change during development
- Use v0.5.0 for production deployments

---

## [0.5.0] - 2025-01-17 - Production Ready Release

### 🎉 Major Features Added

#### Hierarchical Schema System
- **8-Level Deep Schema Nesting**: Exceeds PostgreSQL capabilities with support for schemas like `company.division.department.team.project.environment.component.module`
- **Schema Context Management**: Full support for `SET SCHEMA` and `SET HOME SCHEMA` commands
- **Schema-Qualified Names**: Complete support for 3-level qualified names in all SQL operations
- **Schema Metadata Views**: New `RDB$SCHEMA_HIERARCHY` system view for schema management
- **Performance Optimizations**: Schema path caching and efficient hierarchical resolution

#### PostgreSQL-Compatible Data Types
- **Network Types**: Full implementation of `INET`, `CIDR`, and `MACADDR` with PostgreSQL operators
- **UUID Support**: All UUID versions (1, 4, 6, 7, 8) with `UUID GENERATED ALWAYS AS IDENTITY`
- **Unsigned Integers**: `USMALLINT`, `UINTEGER`, `UBIGINT`, and `UINT128` data types
- **Range Types**: 6 range types (`INT4RANGE`, `INT8RANGE`, `TSRANGE`, etc.) with operators
- **Enhanced VARCHAR**: Support for up to 128KB UTF-8 text (4x larger than Firebird)

#### Schema-Aware Database Links
- **5 Resolution Modes**: NONE, FIXED, CONTEXT_AWARE, HIERARCHICAL, and MIRROR
- **Remote Schema Mapping**: Intelligent mapping between local and remote schema hierarchies
- **Context-Aware Resolution**: Support for CURRENT, HOME, and USER schema references
- **Performance Optimization**: Pre-computed schema depth caching and efficient path resolution
- **Integration**: Seamless integration with Firebird's attachment system

### 🔧 Core System Enhancements

#### Build System & Architecture
- **Complete Linux Compilation**: Successfully built all core tools and utilities
- **Modern C++17**: Updated codebase with contemporary C++ standards
- **Comprehensive Directory Structure**: Organized build system with proper dependency management
- **Cross-Platform Ready**: Foundation for Windows compilation support

#### Database Utilities (All Working)
- **scratchbird**: Main database server with hierarchical schema support
- **sb_isql**: Interactive SQL utility (version SB-T0.5.0.1)
- **sb_gbak**: Backup/restore utility with schema preservation
- **sb_gfix**: Database maintenance and validation tool
- **sb_gsec**: Security management and user administration
- **sb_gstat**: Database statistics and performance analysis
- **sb_guard**: Process monitor for server reliability
- **sb_svcmgr**: Service manager interface
- **sb_tracemgr**: Database tracing and monitoring
- **sb_nbackup**: Incremental backup utility
- **sb_gssplit**: File splitting utility
- **sb_lock_print**: Lock analysis tool

#### Complete ScratchBird Branding
- **Zero Firebird References**: All user-facing components show ScratchBird branding
- **sb_ Tool Prefixes**: All utilities use ScratchBird naming convention
- **Separate Port**: Default port changed to 4050 to prevent conflicts
- **Service Differentiation**: Clear separation from Firebird installations

### 🧪 Testing & Quality Assurance

#### Comprehensive Test Suite
- **8 Test Categories**: Hierarchical schemas, network data types, UDR functionality, performance benchmarks, package functionality, database links, regression tests, and stress tests
- **Performance Benchmarks**: Automated timing and performance categorization (FAST/MEDIUM/SLOW)
- **Operational Speed Testing**: Validates system performance under various load conditions
- **sb_isql Integration**: All tests use sb_isql for consistent execution environment

#### Build Quality
- **Successful Compilation**: Clean build with resolved dependency issues
- **Authentication Constants**: Fixed missing Auth operation constants (ADD_OPER, DEL_OPER, MOD_OPER)
- **Directory Structure**: Proper build directory organization and object file placement
- **Version Consistency**: All tools show consistent SB-T0.5.0.1 version information

### 🔨 Technical Improvements

#### Parser & DDL Extensions
- **3-Level Qualified Names**: Extended parser grammar to support deep schema references
- **DDL Support**: CREATE/ALTER/DROP operations for hierarchical schemas
- **Circular Reference Detection**: Prevents invalid schema hierarchies
- **Schema Validation**: Comprehensive validation of schema paths and depth limits

#### Performance Optimizations
- **Schema Path Caching**: High-performance caching system for schema resolution
- **Depth Limits**: Maximum 8-level depth with 511-character path length
- **Optimized String Operations**: Efficient hierarchical path resolution
- **Memory Management**: Reduced allocations through cached components

#### Security & Authentication
- **Schema-Level Permissions**: Extended security model for hierarchical schemas
- **Database Link Security**: Remote schema validation and access control
- **User Management**: Enhanced user administration with schema awareness

### 📚 Documentation & Examples

#### Comprehensive Documentation
- **Updated README.md**: Complete feature documentation and quick start guide
- **Test Examples**: Extensive SQL examples demonstrating all new features
- **Build Instructions**: Detailed compilation and installation procedures
- **Performance Analysis**: Benchmarking results and optimization recommendations

#### Code Examples
```sql
-- Hierarchical Schema Creation
CREATE SCHEMA company.division.department.team;

-- PostgreSQL-Compatible Network Types  
CREATE TABLE servers (ip INET, network CIDR, mac MACADDR);

-- UUID IDENTITY Columns
CREATE TABLE users (
    id UUID GENERATED ALWAYS AS IDENTITY (GENERATOR GEN_UUID(7)),
    name VARCHAR(100)
);

-- Schema-Aware Database Links
CREATE DATABASE LINK finance_link 
  TO 'server2:finance_db' 
  SCHEMA_MODE HIERARCHICAL
  LOCAL_SCHEMA 'finance'
  REMOTE_SCHEMA 'accounting';
```

### 🏗️ Infrastructure & Deployment

#### Production Readiness
- **Stable Architecture**: Built on proven Firebird 6.0.0.929 foundation
- **Conflict Prevention**: Separate ports and service names prevent Firebird conflicts
- **Enterprise Features**: Complete toolset for production database management
- **Cross-Platform Foundation**: Ready for multi-platform deployment

#### Release Package Structure
```
scratchbird-v0.5.0-linux-x86_64/
├── bin/                    # All ScratchBird utilities
├── lib/                    # Client libraries  
├── include/                # Development headers
├── doc/                    # Documentation
├── examples/               # Usage examples
└── tests/                  # Comprehensive test suite
```

### 🔧 Technical Debt & Fixes

#### Resolved Build Issues
- **Missing Header Files**: Fixed gsec.h include path issues
- **Namespace Constants**: Added missing Auth operation constants
- **Directory Creation**: Automated creation of required build directories
- **Template Compilation**: Resolved C++ template and namespace issues

#### Code Quality
- **Error Handling**: Comprehensive error handling for schema operations
- **Memory Safety**: Proper resource management and cleanup
- **Thread Safety**: Multi-threaded operation support
- **Standards Compliance**: Modern C++17 implementation

### 🚀 What's Next (v0.6.0 Preview)

- **Enhanced Default Schema Architecture**: Enterprise-grade schema organization
- **Advanced Array Types**: Multi-dimensional arrays with PostgreSQL operators  
- **Full-Text Search**: TSVECTOR/TSQUERY implementation
- **Spatial Data Types**: POINT, LINE, POLYGON with indexing
- **Windows Build Support**: Complete cross-platform compilation

---

## [0.4.x] - Development Releases
*Previous development versions focusing on core architecture and PostgreSQL compatibility*

## [0.3.x] - Alpha Releases  
*Early alpha releases with basic PostgreSQL data type implementation*

## [0.2.x] - Proof of Concept
*Initial fork from Firebird with basic ScratchBird branding*

## [0.1.x] - Initial Development
*Project initialization and development environment setup*

---

For more details about upcoming features, see our [Development Roadmap](doc/notes/roadmap.md).

**Download ScratchBird v0.5.0**: [GitHub Releases](https://github.com/dcalford/ScratchBird/releases/tag/v0.5.0)