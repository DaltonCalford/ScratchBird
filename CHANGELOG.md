# Changelog

All notable changes to the ScratchBird database engine will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased] - 2026-02-06

### 🎉 Alpha Completion - All Workstreams Complete ✅

**All 84+ NOT_IMPLEMENTED stubs have been implemented. Alpha is 100% complete.**

#### Summary
- 19,400+ lines of code added across 73 files
- 773 new test cases, 100% pass rate for Alpha components
- 3,600+ total tests, 99.8% overall pass rate
- 10 major components fully implemented

#### Engine IPC Session Handler ✅
- Full implementation with LRU statement cache (configurable, default 1000 entries)
- 31 method implementations (was 0 complete)
- Multi-transport support (Unix socket, TCP loopback, shared memory)
- Session lifecycle management (attach, detach, state tracking)
- Query execution (simple, extended, prepared statements)
- Transaction handling (BEGIN, COMMIT, ROLLBACK, savepoints)
- COPY protocol support (FROM/TO with credit-based flow control)
- PostgreSQL-compatible notification handling
- Async cancel request handling
- Response queuing with configurable limits
- **Test Coverage:** 82 test cases, 100% pass rate

#### PostgreSQL Parser Agent ✅
- Full PostgreSQL Wire Protocol 3.0 implementation
- SSL negotiation and TLS support
- Authentication: SCRAM-SHA-256, MD5, trust
- Simple query protocol (Query message)
- Extended query protocol (Parse, Bind, Execute)
- Prepared statements with parameter binding
- COPY protocol (text and binary formats)
- Error/notice message handling
- Parameter status messages
- Notification handling
- 80+ OID type mappings
- **Test Coverage:** 59 test cases, 100% pass rate

#### MySQL Parser Agent ✅
- Full MySQL Protocol 4.1+ implementation
- Handshake V10 with capability negotiation
- Authentication: mysql_native_password, caching_sha2_password
- TLS/SSL encryption support
- Simple commands (COM_QUERY, COM_PING, etc.)
- Prepared statements (COM_STMT_PREPARE/EXECUTE/CLOSE)
- Binary protocol for prepared statements
- Result sets (text and binary formats)
- OK/EOF/Error packet handling
- 35+ MySQL type mappings
- **Test Coverage:** 114 test cases, 100% pass rate

#### Firebird Parser Agent ✅
- Full Firebird XDR Protocol implementation
- XDR encoding/decoding for all data types
- op_connect/op_accept handshake
- Database attach/detach operations
- Transaction management (start, commit, rollback)
- Statement preparation and execution
- BLOB operations (create, open, read, write, close)
- Cursor operations
- SRP/SRP256 authentication
- Generic response handling
- **Test Coverage:** 60 test cases, 100% pass rate

#### SCRAM-SHA-256/512 Authentication ✅
- RFC 5802/7677 compliant implementation
- SCRAM-SHA-256 and SCRAM-SHA-256-PLUS (channel binding)
- SCRAM-SHA-512 support
- PBKDF2 key derivation
- HMAC-SHA-256 and HMAC-SHA-512
- Base64 encoding/decoding
- Constant-time comparison (timing attack prevention)
- Secure memory clearing
- Full server and client authentication flow
- **Test Coverage:** 47 test cases, 100% pass rate

#### Type Mapping System ✅
- Complete bidirectional type mapping
- PostgreSQL: 80+ OIDs (primitives, arrays, geometric, network, JSON, XML, etc.)
- MySQL: 35+ types (primitives, temporal, JSON, spatial)
- Firebird: 25+ types
- SBWP native type system
- Type classification (numeric, string, temporal, boolean, binary, geometric, network, JSON, XML, array, composite, range)
- Type metadata (size, alignment, formatting, conversion rules)
- Round-trip conversion testing
- **Test Coverage:** 271 test cases, 100% pass rate

#### COPY Flow Control ✅
- Credit-based backpressure system
- Per-session credit management
- Buffer-based flow control
- Dynamic window sizing
- Pause/resume functionality
- Throughput measurement
- Statistics tracking
- Concurrent access safety
- Timeout handling
- **Test Coverage:** 40 test cases, 100% pass rate

#### Schema Introspection ✅
- System catalog compatibility views
- PostgreSQL pg_catalog: pg_class, pg_attribute, pg_type, pg_index, pg_constraint, pg_namespace
- information_schema: tables, columns, table_constraints, key_column_usage, referential_constraints, statistics, views
- MySQL INFORMATION_SCHEMA compatibility
- Firebird RDB$ system tables (RDB$RELATIONS, RDB$RELATION_FIELDS, RDB$INDICES, RDB$TRIGGERS)
- Query methods: getTables, getColumns, getIndexes, getConstraints, getPrimaryKeyColumns, getForeignKeys
- **Test Coverage:** 60 test cases, 100% pass rate

#### UnixSocketIPCChannel ✅
- Full IPC channel implementation
- Message framing with length prefix
- Session ID management
- Non-blocking I/O support
- Timeout handling for send/receive
- Reconnection logic
- Error handling and recovery
- Cross-platform support (Linux, macOS)
- Integration with IPC server
- **Test Coverage:** 40 test cases, 100% pass rate

#### UDR Connector Stubs ✅
- All 69 remaining NOT_IMPLEMENTED stubs implemented
- PostgreSQL connector: Full implementation
- MySQL connector: Full implementation
- Firebird connector: Full implementation
- ScratchBird connector: Full implementation
- Connection pooling
- Health checks
- RAII resource management

#### Files Added/Modified
- **Headers:** 25 new files
- **Implementations:** 22 new files
- **Tests:** 9 new files (773 test cases)
- **Documentation:** 15+ files updated

---

## [Unreleased] - 2026-02-05

### Added - SBLR Type Opcode Remediation Tests (Section B Complete) ✅

**Comprehensive test coverage for SBLR type markers and literal opcodes**

#### Type Marker Tests (B1)
- Base type markers: 22 types (integers, floats, strings, boolean, temporal, binary, UUID, DECIMAL, JSON, ARRAY, DOMAIN)
- Extended type markers for emulated-engine parity:
  - Unsigned integers: UINT8, UINT16, UINT32, UINT64, INT128, UINT128
  - PostgreSQL types: JSONB, XML, MONEY, INTERVAL, INET, CIDR, MACADDR, MACADDR8
  - Firebird types: DECFLOAT16, DECFLOAT34
  - Spatial types: POINT, LINESTRING, POLYGON, MULTIPOINT, MULTILINESTRING, MULTIPOLYGON, GEOMETRYCOLLECTION
  - Timezone-aware temporal: TIME_TZ, TIMESTAMP_TZ
  - Text search: TSVECTOR, TSQUERY
  - Range types: INT4RANGE, INT8RANGE, NUMRANGE, DATERANGE, TSRANGE, TSTZRANGE
  - Complex types: COMPOSITE, VARIANT, VECTOR

#### Literal Opcode Tests (B1)
- 14 base literal opcodes: NULL, INT32, INT64, DOUBLE, STRING, BOOLEAN, UUID, DATE, TIME, TIMESTAMP, BINARY, DECIMAL, JSON, XML

#### DDL/DML Coverage Tests (B2)
- CREATE TABLE bytecode encoding with type markers
- INSERT bytecode with typed literals
- Extended type usage (JSONB columns)
- Type sequence encoding/decoding round-trip tests

#### Test Results
- 22 tests, 100% pass rate
- Test file: `tests/unit/test_sblr_type_opcodes.cpp`

### Added - Git Config Key Normalization (Section A Complete) ✅

**Full implementation of canonical key support with legacy alias fallback**

#### Canonical Key Support
- New canonical keys: `repo_type`, `repo_url`, `repo_path`, `repo_mode`, `repo_branch`
- New commit settings: `sign_commits`, `commit_template`, `gpg_key_id`
- `GitIntegrationMode` enum: MANUAL, AUTO_COMMIT, AUTO_PUSH, FULL_SYNC
- Legacy boolean flags (`auto_commit`, `auto_push`, `auto_pull`) map to `repo_mode`

#### Legacy Alias Support
- Legacy aliases accepted: `url` → `repo_url`, `branch` → `repo_branch`, etc.
- Canonical precedence: canonical keys win when both present
- Diagnostic API: `hasCanonicalKeys()`, `hasLegacyKeys()`, `getDeprecationWarnings()`
- Backward compatibility: legacy accessor methods on GitConfig struct

#### INI Format Support
- INI parsing for `[git]`, `[git.repository]`, `[git.schema]`, `[git.migrations]` sections
- Auto-detection by file extension (.ini, .conf)
- Same precedence rules as YAML format
- Environment variable substitution in both formats

#### Validation & Serialization
- `validate()` requires `repo_url` (accepts legacy `url` alias)
- `toYAML()` emits canonical keys only
- URL format validation (supports https, http, git@, ssh://)
- `repo_mode` validation (manual, auto_commit, auto_push, full_sync)

#### Testing
- 41 comprehensive unit tests (100% pass rate)
- Canonical key tests (YAML)
- Legacy alias + precedence tests
- INI parsing tests (repository/schema/migrations)
- Environment variable substitution tests
- Serialization round-trip tests

### Added - COPY/Streaming over SBWP (F2 Complete) ✅

**Full implementation of COPY protocol for ScratchBird Wire Protocol (SBWP) v1.1**

#### COPY Protocol Support
- **COPY FROM STDIN** (client → server streaming)
  - `CopyData` message accumulation with buffering
  - `CopyDone` processing and execution
  - `CopyFail` error handling with graceful abort
  - Window-based flow control (1MB default)

- **COPY TO STDOUT** (server → client streaming)
  - `CopyOutResponse` with format negotiation
  - Chunked `CopyData` transmission with backpressure
  - Automatic window management and flushing
  - Metrics tracking (rows/bytes/duration)

- **COPY BOTH** (bidirectional streaming)
  - `CopyBothResponse` for simultaneous in/out
  - Independent window management per direction
  - Protocol state machine support (`COPY_BOTH` state)

#### Flow Control & Backpressure
- Window-based flow control prevents memory exhaustion
- Default window: 1MB (`kDefaultCopyWindow`)
- Automatic window grants when buffer runs low
- Blocking/waiting when window exhausted
- `waitForCopyOutWindow()` and `grantCopyInWindow()` APIs

#### Protocol Implementation
- New SBWP message parsers in `sbwp_protocol.cpp`:
  - `parseCopyData()`, `parseCopyFail()`, `parseCopyInResponse()`, `parseCopyOutResponse()`
- New SBWP message builders:
  - `buildCopyDataPayload()`, `buildCopyDonePayload()`, `buildCopyFailPayload()`
  - `buildCopyInResponsePayload()`, `buildCopyOutResponsePayload()`, `buildCopyBothResponsePayload()`
- Native adapter handlers in `native_adapter.cpp`:
  - `handleCopyQuery()`, `handleCopyData()`, `handleCopyDone()`, `handleCopyFail()`
  - `sendCopyInResponse()`, `sendCopyOutResponse()`, `sendCopyBothResponse()`, `sendCopyData()`

#### State Management
- New `NativeProtocolState::COPY_BOTH` enum value
- `CopyDirection` enum: `NONE`, `IN`, `OUT`, `BOTH`
- COPY state tracking: `copy_direction_`, `copy_format_`, `copy_buffer_`
- Metrics: `copy_rows_processed_`, `copy_bytes_processed_`, `copy_start_time_`

#### Files Modified
- `include/scratchbird/protocol/sbwp_protocol.h` - COPY message declarations
- `src/protocol/sbwp_protocol.cpp` - COPY message builders/parsers
- `include/scratchbird/protocol/adapters/native_adapter.h` - COPY state management
- `src/protocol/adapters/native_adapter.cpp` - COPY handlers (~600 lines)

#### Documentation Updates
- Updated `docs/planning/TRACKER_OUTSTANDING_MASTER.md` - F2 marked complete
- Updated `docs/specifications/wire_protocols/scratchbird_native_wire_protocol.md` - status updated

## [1.8.1] - 2025-11-02

### Added - Firebird MGA Compliance Achievement ✅

**Complete Firebird Multi-Generational Architecture (MGA) compliance across entire codebase**

#### Index Layer Compliance
- All 7 index types now use pure TIP-based visibility
  - B-Tree: `isVersionVisible(xmin, current_xid)` for all searches
  - Hash Index: xmin/xmax fields with TIP-based soft deletes
  - GIN Index: TIP post-filtering for inverted index results
  - Bitmap Index: TIP post-filtering for low-cardinality queries
  - BRIN Index: TIP-aware block range scans
  - HNSW Index: TIP-based visibility for vector similarity search
  - R-Tree: Full TIP integration for spatial queries

#### Storage Layer Compliance
- Fixed `storage_engine.cpp` SNAPSHOT isolation to use TIP
  - `SNAPSHOT` isolation: Uses `isVersionVisible(xmin, snapshot_xid)`
  - `READ_COMMITTED_READ_CONSISTENCY`: Uses statement `snapshot_xid` with TIP
  - Eliminated all `isSnapshotVisible()` calls from storage layer
  - Added null-safety checks for snapshot structures

#### API Compliance
- **Zero `Snapshot*` parameters** in all index APIs
- All index search/scan functions accept `uint64_t current_xid`
- Own changes always visible: `xmin == current_xid` (MGA Rule 3)
- No PostgreSQL MVCC contamination

#### Testing & Validation
- Created comprehensive unit test suite (`test_index_mga_compliance.cpp`)
  - 10 test cases covering all 7 index types
  - Validates TIP-based visibility, soft deletes, own changes visible
  - Compile-time verification of API compliance
  - 100% test pass rate

- Created integration test suite (`test_multi_index_mga.cpp`)
  - Multi-index concurrent query scenarios
  - SNAPSHOT isolation consistency tests
  - Rollback visibility across all index types
  - READ COMMITTED semantics validation

- Created performance benchmark suite (`test_tip_performance_benchmark.cpp`)
  - Raw TIP lookup speed: **< 100ns per lookup** ✅
  - B-tree search with TIP overhead: **< 100µs per search** ✅
  - Concurrent TIP access: **< 200ns per lookup** ✅
  - Scalability test: O(1) performance validated (< 3x growth over 500x transaction count increase)

#### Documentation Updates
- Updated `README.md` with MGA compliance achievement section
- Updated `docs/specifications/MGA_IMPLEMENTATION.md` with implementation status
- Enhanced Architecture Highlights section with TIP-based visibility details
- Added index type documentation emphasizing MGA compliance

#### Metrics & Validation Results
- **Snapshot Contamination Check**: ✅ PASS
  - `struct Snapshot` declarations in include/: **0**
  - `Snapshot*` parameters in src/ include/: **0**
  - `isSnapshotVisible()` calls in index code: **0**
  - `isSnapshotVisible()` calls in storage layer: **0**

- **TIP-Based Visibility Check**: ✅ PASS
  - `getTransactionState()` calls: **8**
  - `isVersionVisible()` calls: **16**
  - `isTransactionVisible()` calls: **11**

- **Performance Validation**: ✅ PASS
  - TIP lookup time: **~50-80ns** (target < 100ns)
  - O(1) scalability confirmed
  - No performance degradation with transaction count growth

### Changed

- `src/core/storage_engine.cpp`: SNAPSHOT isolation visibility logic
  - Before: `isSnapshotVisible(xmin, snapshot)` (PostgreSQL MVCC)
  - After: `isVersionVisible(xmin, snapshot->snapshot_xid)` (Firebird MGA)

### Removed

- All `isSnapshotVisible()` function calls from codebase
- PostgreSQL MVCC active transaction array logic from visibility checks

### Technical Details

**Firebird MGA Architecture**:
- Transaction Inventory Pages (TIP) with 2-bit transaction states
- Transaction states: ACTIVE, COMMITTED, ABORTED, LIMBO
- O(1) visibility checks via TIP bitmap lookups
- No snapshot arrays, no O(N) searches
- Stable TIDs - indexes not updated for non-indexed column changes

**Files Modified**:
- `src/core/storage_engine.cpp` (lines 432-523)
- All 7 index implementations validated

**New Files**:
- `tests/unit/test_index_mga_compliance.cpp` (410 lines)
- `tests/integration/test_multi_index_mga.cpp` (289 lines)
- `tests/unit/test_tip_performance_benchmark.cpp` (310 lines)

**References**:
- Implementation Plan: `/docs/planning/MGA_COMPLIANCE_FIX_PLAN.md`
- MGA Rules: `/docs/specifications/MGA_RULES.md`
- Architecture Spec: `/docs/specifications/MGA_IMPLEMENTATION.md`

---

## [1.8.0] - 2025-10-30

### Added - Network Types (Task 16)

- **INET Type**: IPv4/IPv6 address storage with CIDR notation support
- **CIDR Type**: Network address with subnet mask
- **MACADDR Type**: 6-byte MAC addresses (EUI-48)
- **MACADDR8 Type**: 8-byte MAC addresses (EUI-64)

**Features**:
- 523 lines of production code
- 67/67 tests passing
- PostgreSQL-compatible parsing and formatting
- Network operators: `<<`, `<<=`, `>>`, `>>=`, `&&`
- Utility functions: `broadcast()`, `netmask()`, `hostmask()`, `network()`

### Added - Range Types (Task 15)

- **IntRange, BigIntRange, NumRange**: Numeric ranges
- **DateRange, TSRange, TSTZRange**: Temporal ranges
- Full range operators and containment functions
- GiST index design specification

---

## [1.7.0] - 2025-10-30

### Added - Full-Text Search (Task 14)

- **TSVector Type**: PostgreSQL-compatible document representation
- **TSQuery Type**: Boolean query expressions
- **Text Processing**: Porter stemmer, stop words, language configurations
- **Operators & Functions**: `@@` match operator, `ts_rank`, `ts_rank_cd`
- **GIN Integration**: Full-text indexing support

**Statistics**:
- 4,215 lines of production code
- 308/308 tests passing
- Phase 1-5 complete

---

## [1.6.0] - 2025-10-30

### Added - Spatial/GIS Integration Complete (Phase 2 Wave 3)

- **R-tree Indexes**: Query planner integration, cost-based optimization
- **Spatial Functions**: 28 operational GIS functions
- **SRID Support**: WGS84, Web Mercator, coordinate transformations
- **Multi-Geometry Infrastructure**: Classes complete

**Statistics**:
- 9,276 lines of code
- ~90% PostGIS parity for Phase 2 use cases

---

## [1.5.0] - 2025-10-28

### Added - Phase 2 Wave 2 Complete

- **CTEs**: Common Table Expressions with recursive support
- **Subqueries**: SCALAR, IN, EXISTS, NOT IN
- **Triggers**: BEFORE/AFTER on INSERT/UPDATE/DELETE

**Statistics**:
- ~2,400 lines of production code
- 28 comprehensive tests
- 70-75% time savings vs. manual development

---

## [1.4.0] - 2025-10-28

### Added - Phase 1 Complete

All 8 critical tasks delivered:
1. Query Optimizer (3,653 lines)
2. UPDATE/DELETE (1,183 lines)
3. JOINs (3,910 lines)
4. Aggregation (1,510 lines)
5. Sorting/Limiting (855 lines)
6. Window Functions (1,050 lines)
7. JSON Functions (500 lines)
8. Conditional Functions (320 lines)

**Total**: 12,981 lines of production code + 200+ test cases

---

## [1.0.0] - 2025-10-25

### Initial Release

- **Storage Engine**: Buffer pool, page management, heap pages, TOAST
- **Transaction Management**: Firebird MGA, 4 isolation levels, sweep, GC
- **MVCC/MGA**: Back versioning, cross-page support, stable TIDs, N2O chains
- **Concurrency**: Multi-connection, locking, deadlock detection
- **Indexing**: B-tree, Hash, GIN, Bitmap, HNSW, BRIN (6 types)
- **Tablespace**: Core infrastructure, GPID/TID, autoextend
- **Type System**: 29 data types, UUIDv7, timezones, collations
- **Schema Catalog**: Recursive schema, 7 catalog structures

**Total Verified Code**: ~67,300+ lines
