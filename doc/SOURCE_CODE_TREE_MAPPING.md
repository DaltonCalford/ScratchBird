# ScratchBird Database Engine - Complete Source Code Tree Mapping

## Overview

This document provides a comprehensive mapping of the ScratchBird database engine source code tree, explaining the purpose and structure of each major directory and its key files. The source code is organized into logical subsystems that implement different aspects of the database engine functionality.

**Source Root**: `/src/`  
**Total Directories**: 15+ major subsystems  
**Total Files**: 1000+ source files  
**Languages**: C++ (primary), C, SQL, Assembly  
**Build System**: CMake  

---

## Directory Structure Overview

```
src/
├── jrd/                    # Database Engine Core (250+ files)
├── dsql/                   # SQL Parser and Compiler (40+ files)
├── common/                 # Shared Libraries and Utilities (50+ files)
├── utilities/              # Database Utilities (Enhanced ScratchBird tools)
├── remote/                 # Network Protocol Layer
├── auth/                   # Authentication System
├── intl/                   # Internationalization Support
├── lock/                   # Lock Manager
├── gpre/                   # GPRE Preprocessor
├── extlib/                 # External Library Support
├── plugins/                # Plugin Architecture
├── yvalve/                 # Y-Valve API Layer
├── iscguard/              # Database Guardian Service
├── include/                # Header Files and API Definitions
├── msgs/                   # Message Definitions
└── dbs/                    # System Database Schemas
```

---

## Core Database Engine (`src/jrd/`)

**Purpose**: Main database engine implementation with storage, indexing, transaction management, and query execution.

### **Key Architecture Components**

#### **Storage and Page Management**
- **`Database.cpp/.h`** - Database file management and attachment handling
- **`pag.cpp/.h`** - Page management and buffer pool operations
- **`cch.cpp/.h`** - Buffer cache management and page I/O
- **`ods.cpp/.h`** - On-Disk Structure (ODS) definitions and format handling
- **`Relation.cpp/.h`** - Table (relation) metadata and structure management

#### **Index Subsystem (ScratchBird Enhanced)**
- **`IndexType.h`** - Base index type definitions and interface
- **`IndexTypeRegistry.cpp/.h`** - Registry for all supported index types
- **`BitmapIndex.cpp/.h`** - Bitmap index implementation for low-cardinality data
- **`BitmapIndexMaintenance.cpp/.h`** - Bitmap index maintenance and optimization
- **`GinIndex.cpp/.h`** - Generalized Inverted Index for full-text search
- **`GinTokenizer.cpp/.h`** - Text tokenization for GIN indexes
- **`GinCompression.cpp/.h`** - Compression algorithms for GIN data
- **`HashIndex.cpp/.h`** - Hash index implementation
- **`PartialHashIndex.cpp/.h`** - Partial hash indexes with WHERE clause filtering
- **`SpatialIndex.cpp/.h`** - R-tree spatial indexes for geometric data
- **`idx.cpp/.h`** - Traditional B-tree index management

#### **Query Execution Engine**
- **`exe.cpp/.h`** - Query execution engine and runtime
- **`evl.cpp`** - Expression evaluation and value computation
- **`RecordSourceNodes.cpp/.h`** - Query plan node implementations
- **`recsrc/`** - Record source implementations (table scans, joins, etc.)
  - **`FullTableScan.cpp`** - Sequential table scanning
  - **`IndexTableScan.cpp`** - Index-based table access
  - **`HashTableScan.cpp/.h`** - Hash index table scanning
  - **`BitmapTableScan.cpp/.h`** - Bitmap index table scanning
  - **`GinTableScan.cpp/.h`** - GIN index table scanning
  - **`HashJoin.cpp`** - Hash join implementation
  - **`MergeJoin.cpp`** - Sort-merge join implementation
  - **`NestedLoopJoin.cpp`** - Nested loop join implementation

#### **Transaction Management**
- **`tra.cpp/.h`** - Transaction management and MVCC implementation
- **`lck.cpp/.h`** - Lock management and deadlock detection
- **`vio.cpp`** - Version-based I/O and record versioning
- **`Savepoint.cpp/.h`** - Savepoint management for nested transactions

#### **Metadata Management**
- **`met.epp/.h`** - System catalog metadata management
- **`relations.h`** - System table definitions and structure
- **`names.h`** - System names and constants
- **`fields.h`** - Field type definitions and metadata

#### **Hierarchical Schema Support (ScratchBird Enhancement)**
- **`Attachment.cpp/.h`** - Enhanced with hierarchical schema cache and resolution
- **`SchemaPathCache.cpp/.h`** - High-performance schema path parsing and caching
- **`QualifiedName.h`** - Support for 3-level qualified names (schema.subschema.object)

#### **Advanced Features (ScratchBird Enhancements)**
- **`DatabaseLink.cpp/.h`** - Schema-aware database links with 5 resolution modes
- **`Mapping.cpp/.h`** - External authentication mapping system
- **`SpatialDataTypes.cpp/.h`** - Geometric and geographic data type support
- **`WKBParser.cpp/.h`** - Well-Known Binary format parsing for spatial data
- **`WKTParser.cpp/.h`** - Well-Known Text format parsing for spatial data
- **`Function.epp/.h`** - User-defined function management
- **`SystemPackages.cpp/.h`** - Built-in system packages and procedures

#### **Security and Authentication**
- **`UserManagement.cpp/.h`** - User account management and authentication
- **`scl.epp/.h`** - Security class and access control implementation
- **`grant.epp`** - Permission granting and privilege management

#### **Optimization and Statistics**
- **`optimizer/`** - Query optimization subsystem
  - **`Optimizer.cpp/.h`** - Main query optimizer
  - **`BitmapIndexCostModel.cpp/.h`** - Cost modeling for bitmap indexes
  - **`GinIndexCostModel.cpp/.h`** - Cost modeling for GIN indexes
  - **`HashIndexCostModel.cpp/.h`** - Cost modeling for hash indexes
  - **`PartialHashIndexCostModel.cpp/.h`** - Cost modeling for partial hash indexes

#### **Replication System**
- **`replication/`** - Logical replication subsystem
  - **`Manager.cpp/.h`** - Replication management and coordination
  - **`Publisher.cpp/.h`** - Publication of database changes
  - **`Applier.cpp/.h`** - Application of replicated changes
  - **`ChangeLog.cpp/.h`** - Change tracking and log management

#### **Monitoring and Tracing**
- **`Monitoring.cpp/.h`** - Database monitoring and statistics collection
- **`trace/`** - SQL trace and performance monitoring
  - **`TraceManager.cpp/.h`** - Trace session management
  - **`TraceService.cpp/.h`** - Trace service implementation

### **File Organization Summary**
- **Core Files**: ~50 primary engine files
- **Index Files**: ~25 advanced indexing implementations
- **Query Execution**: ~30 record source and execution files
- **Optimizer**: ~10 cost model and optimization files
- **Replication**: ~8 logical replication files
- **Monitoring**: ~10 trace and monitoring files
- **Total**: 250+ files implementing complete database engine

---

## SQL Parser and Compiler (`src/dsql/`)

**Purpose**: SQL parsing, compilation, and DDL/DML statement processing.

### **Core Parser Components**
- **`parse.y`** - Main SQL grammar specification (Yacc/Bison format)
  - Complete DDL grammar for all database objects
  - Enhanced with ScratchBird extensions (hierarchical schemas, advanced indexes)
  - 3-level qualified name support
- **`Parser.cpp/.h`** - SQL parser implementation and error handling
- **`Keywords.cpp/.h`** - SQL keyword definitions and reserved words

### **DDL Node Implementations**
- **`DdlNodes.epp/.h`** - DDL statement node implementations
  - CreateTableNode, AlterTableNode, DropTableNode
  - CreateIndexNode with support for all 6 index types
  - CreateSchemaNode with hierarchical support
  - CreateUserNode, CreateRoleNode for security
- **`DatabaseLinkNodes.cpp/.h`** - Database link DDL nodes (ScratchBird enhancement)
- **`PackageNodes.epp/.h`** - Package DDL nodes for object grouping

### **Expression and Statement Nodes**
- **`ExprNodes.cpp/.h`** - Expression node implementations (arithmetic, logical, etc.)
- **`StmtNodes.cpp/.h`** - Statement node implementations (SELECT, INSERT, etc.)
- **`BoolNodes.cpp/.h`** - Boolean expression handling
- **`AggNodes.cpp/.h`** - Aggregate function implementations
- **`WinNodes.cpp/.h`** - Window function implementations

### **Compilation and Execution**
- **`DsqlCompilerScratch.cpp/.h`** - Compilation context and scratch area
- **`DsqlRequests.cpp/.h`** - Compiled request management
- **`DsqlStatements.cpp/.h`** - Statement execution and lifecycle
- **`DsqlCursor.cpp/.h`** - Cursor management for result sets

### **Metadata Integration**
- **`metd.epp`** - Metadata access for SQL compilation
- **`ddl.cpp`** - DDL execution and metadata updates

### **Utilities**
- **`gen.cpp`** - BLR (Binary Language Representation) generation
- **`NodePrinter.h`** - AST node printing for debugging
- **`Visitors.h`** - Visitor pattern for AST traversal

**Total Files**: ~40 files implementing complete SQL parsing and compilation

---

## Shared Libraries (`src/common/`)

**Purpose**: Shared utilities, data structures, and common functionality used across all subsystems.

### **Core Utilities**
- **`common.h`** - Common definitions and includes
- **`sb_exception.cpp`** - ScratchBird exception handling system
- **`utils.cpp`** - General utility functions
- **`cvt.cpp/.h`** - Data type conversion utilities

### **Advanced Data Types (ScratchBird Enhancements)**
- **`BigInteger.cpp/.h`** - Arbitrary precision integer arithmetic
- **`Int128.cpp/.h`** - 128-bit integer support
- **`UInt128.cpp/.h`** - Unsigned 128-bit integer support
- **`DecFloat.cpp/.h`** - Decimal floating-point arithmetic
- **`GeometricTypes.cpp/.h`** - Geometric and spatial data types
- **`VectorTypes.cpp/.h`** - Vector data types for AI/ML applications
- **`RangeTypes.cpp/.h`** - Range data types for intervals
- **`InetAddr.cpp/.h`** - Network address types (INET, CIDR, MACADDR)

### **Text Processing**
- **`CharSet.cpp/.h`** - Character set handling and conversion
- **`TextType.cpp/.h`** - Text data type implementations
- **`FullTextSearch.cpp/.h`** - Full-text search utilities
- **`SimilarToRegex.cpp/.h`** - SIMILAR TO regex implementation
- **`IntlUtil.cpp/.h`** - Internationalization utilities

### **Container Classes**
- **`classes/`** - Template-based container implementations
  - **`array.h`** - Dynamic array implementation
  - **`vector.h`** - Vector container
  - **`GenericMap.h`** - Generic map/dictionary
  - **`Hash.cpp/.h`** - Hash table implementation
  - **`tree.h`** - Tree data structures
  - **`sparse_bitmap.h`** - Sparse bitmap for efficient storage

### **String Management**
- **`classes/fb_string.h`** - Firebird-compatible string class
- **`classes/sb_string.cpp/.h`** - ScratchBird enhanced string class
- **`DynamicStrings.cpp/.h`** - Dynamic string management
- **`classes/MetaString.cpp/.h`** - Metadata string handling

### **Memory Management**
- **`classes/alloc.cpp/.h`** - Memory allocation and pool management
- **`classes/auto.h`** - Automatic memory management
- **`PerformanceStopWatch.h`** - Performance monitoring utilities

### **Synchronization**
- **`classes/locks.cpp/.h`** - Thread locking mechanisms
- **`classes/rwlock.h`** - Reader-writer locks
- **`classes/SyncObject.cpp/.h`** - Synchronization objects
- **`classes/Synchronize.cpp/.h`** - Thread synchronization utilities

### **Configuration Management**
- **`config/`** - Configuration system
  - **`config.cpp/.h`** - Configuration file parsing
  - **`ConfigCache.cpp/.h`** - Configuration caching
  - **`config_file.cpp/.h`** - Configuration file handling

### **Operating System Abstraction**
- **`os/`** - OS-specific implementations
  - **`posix/`** - Unix/Linux implementations
  - **`win32/`** - Windows implementations
  - **`darwin/`** - macOS implementations

**Total Files**: ~50 files providing comprehensive shared functionality

---

## Database Utilities (`src/utilities/`)

**Purpose**: Enhanced database administration tools with ScratchBird-specific features.

### **Core Utilities (Enhanced)**
- **`sb_gbak.cpp`** - Enhanced backup/restore utility
- **`sb_gfix.cpp`** - Enhanced database maintenance utility
- **`sb_gstat.cpp`** - Enhanced database statistics utility
- **`sb_gsec.cpp`** - Enhanced security management utility
- **`sb_isql.cpp`** - Enhanced interactive SQL utility
- **`sb_nbackup.cpp`** - Enhanced incremental backup utility
- **`sb_svcmgr.cpp`** - Enhanced service manager utility
- **`sb_tracemgr.cpp`** - Enhanced trace manager utility

### **Advanced Features**
- **`sb_compression.cpp/.h`** - Database compression utilities
- **`sb_database_enhanced.cpp/.h`** - Enhanced database operations
- **`sb_engine_integration.cpp/.h`** - Deep engine integration
- **`attachment_manager.cpp/.h`** - Connection management
- **`metadata_cache.h`** - Metadata caching for performance

### **Index Management (ScratchBird Specific)**
- **`GinIndexBackupSupport.cpp/.h`** - GIN index backup/restore support
- **`gin_advanced_config_tool.cpp`** - GIN index configuration utility

### **Archived Legacy Utilities**
- **`archive/`** - Legacy Firebird utilities maintained for compatibility
  - **`nbackup/`** - Original nbackup implementation
  - **`fbsvcmgr/`** - Original service manager
  - **`fbtracemgr/`** - Original trace manager

**Total Files**: ~50 enhanced utility implementations

---

## Network Protocol Layer (`src/remote/`)

**Purpose**: Client-server communication, network protocols, and distributed operations.

### **Core Protocol Implementation**
- **`protocol.cpp/.h`** - Network protocol implementation
- **`remote.cpp/.h`** - Remote database access layer
- **`inet.cpp`** - TCP/IP network handling

### **Client Interface**
- **`client/`** - Client-side networking
  - **`interface.cpp/.h`** - Client interface implementation
  - **`BlrFromMessage.cpp/.h`** - Message format conversion

### **Server Implementation**
- **`server/`** - Server-side networking
  - **`server.cpp`** - Main server implementation
  - **`ReplServer.cpp/.h`** - Replication server (ScratchBird enhancement)

### **Platform-Specific**
- **`os/win32/xnet.cpp/.h`** - Windows named pipe implementation

**Total Files**: ~15 files implementing complete networking layer

---

## Authentication System (`src/auth/`)

**Purpose**: User authentication, security plugins, and credential management.

### **Core Authentication**
- **`AuthDbg.cpp/.h`** - Authentication debugging and logging
- **`SecDbCache.cpp/.h`** - Security database caching

### **Secure Remote Password (SRP)**
- **`SecureRemotePassword/`** - SRP authentication implementation
  - **`srp.cpp/.h`** - Main SRP implementation
  - **`client/SrpClient.cpp/.h`** - Client-side SRP
  - **`server/SrpServer.cpp/.h`** - Server-side SRP
  - **`manage/SrpManagement.cpp`** - SRP management utilities

### **Legacy Security Database**
- **`SecurityDatabase/`** - Legacy authentication support
  - **`LegacyClient.cpp/.h`** - Legacy client authentication
  - **`LegacyServer.cpp/.h`** - Legacy server authentication
  - **`LegacyManagement.epp/.h`** - Legacy user management

### **Trusted Authentication**
- **`trusted/`** - OS-integrated authentication
  - **`AuthGssapi.cpp/.h`** - GSSAPI authentication (Unix/Linux)
  - **`AuthSspi.cpp/.h`** - SSPI authentication (Windows)

**Total Files**: ~15 files implementing comprehensive authentication

---

## Internationalization (`src/intl/`)

**Purpose**: Character sets, collations, and international text processing.

### **Character Set Support**
- **`charsets/`** - Character set definitions (50+ files)
  - Support for ISO-8859, Windows codepages, Unicode, Asian charsets
- **`cs_*.cpp`** - Character set implementations
- **`cv_*.cpp`** - Character set conversion utilities

### **Collation Support**
- **`collations/`** - Collation definitions (100+ files)
  - Language-specific sorting rules
  - Country-specific collations
- **`lc_*.cpp`** - Locale-specific implementations

### **Unicode and Multibyte Support**
- **`cs_unicode_*.cpp`** - Unicode implementations
- **`cv_unicode_*.cpp`** - Unicode conversion utilities
- **`kanji.cpp/.h`** - Japanese character handling

**Total Files**: ~200 files supporting comprehensive internationalization

---

## Plugin Architecture (`src/plugins/`)

**Purpose**: Extensible plugin system for encryption, profiling, and UDR engines.

### **Encryption Plugins**
- **`crypt/arc4/Arc4.cpp/.h`** - ARC4 encryption plugin
- **`crypt/chacha/ChaCha.cpp`** - ChaCha encryption plugin

### **Profiler Plugin**
- **`profiler/Profiler.cpp`** - SQL profiling and performance analysis

### **UDR Engine**
- **`udr_engine/UdrEngine.cpp`** - User-Defined Routine engine

**Total Files**: ~10 plugin implementations

---

## API Layer (`src/yvalve/`)

**Purpose**: Y-Valve API abstraction layer providing unified client interface.

### **Core Y-Valve Implementation**
- **`gds.cpp`** - Main API entry points
- **`alt.cpp`** - Alternative API implementations
- **`why.cpp`** - Y-Valve core functionality

### **Object Management**
- **`array.cpp`** - Array handling
- **`blob.cpp`** - BLOB handling
- **`MasterImplementation.cpp/.h`** - Master interface implementation
- **`PluginManager.cpp/.h`** - Plugin management

### **Distributed Transactions**
- **`DistributedTransaction.cpp/.h`** - Two-phase commit implementation

**Total Files**: ~15 files implementing complete API layer

---

## Preprocessor (`src/gpre/`)

**Purpose**: GPRE (General Purpose REcord Extractor) for embedded SQL preprocessing.

### **Core GPRE Implementation**
- **`gpre.cpp/.h`** - Main preprocessor logic
- **`cmd.cpp`** - Command line processing
- **`cmp.cpp`** - SQL compilation
- **`exp.cpp`** - Expression handling

### **Language Support**
- **`languages/`** - Multiple programming language support
  - **`c_cxx.cpp`** - C/C++ language support
  - **`ada.cpp`** - Ada language support
  - **`cob.cpp`** - COBOL language support
  - **`ftn.cpp`** - Fortran language support
  - **`pas.cpp`** - Pascal language support

### **Metadata Handling**
- **`std/gpre_meta.epp`** - Standard metadata handling
- **`boot/gpre_meta_boot.cpp`** - Bootstrap metadata

**Total Files**: ~25 files implementing complete preprocessor

---

## Supporting Components

### **Lock Manager (`src/lock/`)**
- **`lock.cpp`** - Distributed lock management
- **`print.cpp`** - Lock debugging utilities

### **Guardian Service (`src/iscguard/`)**
- **`iscguard.cpp/.h`** - Database guardian service
- **`cntl_guard.cpp`** - Guardian control logic

### **Message System (`src/msgs/`)**
- **`build_file.cpp`** - Message file building
- **`sqlstates.sql`** - SQL state definitions

### **System Databases (`src/dbs/`)**
- **`security.sql`** - Security database schema
- **`database_links.sql`** - Database links schema (ScratchBird enhancement)

### **Header Files (`src/include/`)**
- **`scratchbird/`** - ScratchBird-specific headers
- **`firebird/`** - Firebird compatibility headers
- **`gen/`** - Generated headers and configuration

### **External Library Support (`src/extlib/`)**
- **`ib_util.cpp`** - InterBase/Firebird utility functions
- **`UdfBackwardCompatibility.cpp`** - UDF backward compatibility

---

## ScratchBird-Specific Enhancements

### **Advanced Index Types**
1. **Partial Hash Indexes** - Hash indexes with WHERE clause filtering
2. **GIN Indexes** - Generalized Inverted Indexes for full-text search
3. **Bitmap Indexes** - Efficient indexes for low-cardinality data
4. **Spatial Indexes** - R-tree indexes for geometric data

### **Hierarchical Schema System**
- 3-level qualified names (schema.subschema.object)
- Schema path caching and optimization
- Nested schema relationships up to 11 levels deep

### **Schema-Aware Database Links**
- 5 resolution modes (NONE, FIXED, CONTEXT_AWARE, HIERARCHICAL, MIRROR)
- Cross-database connectivity with intelligent schema mapping
- Integration with hierarchical schema system

### **Enhanced Data Types**
- Extended numeric types (UINT variants, DECIMAL128)
- Network types (INET, CIDR, MACADDR)
- Geometric types for spatial data
- Range types for interval data
- Vector types for AI/ML applications

### **Authentication Mapping**
- External authentication integration
- Plugin-based authentication system
- Global vs database-specific mappings

### **Logical Replication**
- Database-level publication settings
- Table-level filtering and inclusion/exclusion
- Real-time change replication

---

## Build System and Configuration

### **CMake Build System**
- **`CMakeLists.txt`** files throughout source tree
- Platform-specific build configurations
- Enhanced utility builds with integration testing

### **Configuration Management**
- **`autoconfig.h`** - Automatic configuration detection
- **`scratchbird.conf`** - Main database configuration
- **`sbintl.conf`** - Internationalization configuration

### **Version Management**
- **`sb_version.h`** - ScratchBird version definitions
- **`build_no.h`** - Build number tracking

---

## Development and Testing

### **Test Framework**
- **`src/common/tests/`** - Common utility tests
- **`src/dsql/parse-conflicts.txt`** - Parser conflict analysis
- **`src/utilities/test_*.cpp`** - Utility integration tests

### **Documentation**
- **`src/utilities/*.md`** - Implementation plans and summaries
- **`src/misc/upgrade/`** - Database upgrade procedures
- **`src/intl/moved_files.txt`** - File reorganization tracking

---

## Summary Statistics

| Component | Files | Primary Language | Purpose |
|-----------|-------|------------------|---------|
| **jrd/** | 250+ | C++ | Database engine core |
| **dsql/** | 40+ | C++/Yacc | SQL parser and compiler |
| **common/** | 50+ | C++ | Shared libraries |
| **utilities/** | 50+ | C++ | Enhanced admin tools |
| **remote/** | 15+ | C++ | Network protocol |
| **auth/** | 15+ | C++ | Authentication system |
| **intl/** | 200+ | C++ | Internationalization |
| **plugins/** | 10+ | C++ | Plugin architecture |
| **yvalve/** | 15+ | C++ | API layer |
| **gpre/** | 25+ | C++ | SQL preprocessor |
| **Other** | 50+ | C++/SQL | Supporting components |

**Total**: 1000+ source files implementing a complete enterprise-grade database engine with advanced ScratchBird enhancements including hierarchical schemas, advanced indexing, schema-aware database links, and comprehensive administrative tools.

The source code architecture demonstrates a well-structured, modular design with clear separation of concerns, extensive internationalization support, and significant enhancements over the base Firebird architecture to provide advanced enterprise database capabilities.