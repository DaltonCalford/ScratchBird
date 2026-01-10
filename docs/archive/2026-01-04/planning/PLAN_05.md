# Plan 05 - ScratchBird Native ODBC Driver Implementation

**Plan:** ScratchBird Native ODBC 3.x Driver using libscratchbird.so/dll
**Version:** 1.0
**Date:** 2025-12-26
**Status:** ✅ IMPLEMENTATION COMPLETE (ALPHA) - EXTERNAL TESTING PENDING

---

## Executive Summary

Plan 05 implements a **full ODBC 3.x compliant driver** for the ScratchBird native wire protocol. This is NOT about emulated database protocols (PostgreSQL/MySQL/Firebird use their own ODBC drivers for server-side emulation). This driver allows ODBC client applications to connect directly to ScratchBird using the native wire protocol.

**Current State:**
- ✅ **ODBC driver implementation complete for Alpha** (core/basics + full catalog)
- ✅ **Network/TLS client path implemented** (native wire protocol over network listener)
- ✅ **Type conversion, catalog, fetch/bind, and autocommit semantics implemented**
- ⚠️ **External validation pending** (unixODBC isql + BI tools)

**Scope:**
- ODBC 3.x full specification compliance (NOT a subset)
- Native ScratchBird wire protocol over the **network listener** (parser bridge required; engine never direct)
- Uses **libscratchbird** client APIs (network-only or full engine library, but ODBC uses network path)
- Support for all 86 ScratchBird native types
- Full catalog function support (SQLTables, SQLColumns, etc.)

**Alpha Decisions (Confirmed):**
- ODBC uses **libscratchbird** via **network listener → parser → engine** (no direct engine access; embedded mode not exposed).
- Autocommit semantics: ScratchBird is always in a transaction; **autocommit commits after every statement** and immediately starts a new transaction.
- ODBC conformance level target for Alpha: **Core/Basic** only.
- Driver scope: **ScratchBird platform only** (no emulation drivers).
- Complex type mapping: **Hybrid** (text for JSON/XML/ARRAY/RECORD, binary for GEOMETRY/VECTOR).
- Catalog scope: **All 10 catalog functions**.
- Federation visibility: **Current database only** (Alpha).

---

## Status Update (2026-01-XX)

**Implementation Status:**
- ✅ libscratchbird network client + ODBC bridge implemented (TLS 1.3 supported)
- ✅ Catalog functions (all 10) implemented with tests
- ✅ Type conversion implemented (hybrid strategy) with tests
- ✅ Result binding/fetch implemented with tests
- ✅ Autocommit + isolation mapping implemented with tests
- ✅ Core conformance cleanup complete (unsupported functions return HYC00)

**Remaining (Testing/Validation Only):**
- unixODBC `isql` sanity tests
- Manual BI tool checks (Excel/PowerBI/Tableau)

---

## What Exists (As of 2025-12-26)

### Wire Protocol Specification ✅

**File:** `/docs/specifications/wire_protocols/scratchbird_native_wire_protocol.md` (1,946 lines)

**Key Features:**
- **Message Format:** 40-byte header + variable payload
- **Port:** 3092 (default)
- **Security:** TLS 1.3 mandatory
- **Authentication:** SCRAM-SHA-256, Certificate, LDAP, Kerberos, etc.
- **Protocols:**
  - Simple Query (QUERY message)
  - Extended Query (PARSE/BIND/EXECUTE)
- **Type System:** All 86 ScratchBird types defined with serialization
- **Attachment Multiplexing:** Multiple logical attachments per TCP connection
- **Transaction Support:** Always-in-transaction model with txn_id in header

**Repo Reality Check (Implementation Today):**
- **Implemented protocol:** `include/scratchbird/protocol/wire_protocol.h` uses a **12-byte header** with magic **"SBDB"**, version **1.0**.
- **Transport:** **Network listener** (TCP) with optional TLS 1.3 via libscratchbird network client.
- **Client library:** libscratchbird network client implements connect/auth/query/txn over the native wire protocol.

### ODBC Implementation (Partial) ⚠️

**Total Code:** 5,627 lines across 5 files

#### `/src/odbc/odbc_driver.h` (672 lines)
- ODBC API entry point declarations
- Handle validation macros
- Error handling infrastructure

#### `/src/odbc/odbc_driver.cpp` (1,117 lines)
- **Entry Points:** SQLAllocHandle, SQLFreeHandle, SQLGetDiagRec, SQLGetDiagField
- Handle lifecycle management
- Diagnostic record retrieval

#### `/src/odbc/odbc_handles.h` (833 lines)
- **OdbcHandle** base class with diagnostics
- **OdbcEnvironment** (ODBC version, threading model)
- **OdbcConnection** (DSN, connection state, transaction management)
- **OdbcStatement** (SQL execution, result sets, cursors)
- **OdbcDescriptor** (ARD, APD, IRD, IPD)

#### `/src/odbc/odbc_handles.cpp` (2,184 lines)
**Implemented Functions (56+):**
- Environment: SQLSetEnvAttr, SQLGetEnvAttr
- Connection: SQLConnect, SQLDriverConnect, SQLDisconnect, SQLEndTran, SQLGetConnectAttr, SQLSetConnectAttr
- Statement: SQLPrepare, SQLExecute, SQLExecDirect, SQLFetch, SQLFetchScroll, SQLGetData, SQLBindCol, SQLBindParameter
- Descriptor: SQLSetDescField, SQLGetDescField, SQLSetDescRec, SQLGetDescRec
- Attributes: SQLSetStmtAttr, SQLGetStmtAttr
- **STUBS:** All 10 catalog functions (SQLTables, SQLColumns, SQLPrimaryKeys, SQLForeignKeys, SQLStatistics, SQLSpecialColumns, SQLTablePrivileges, SQLColumnPrivileges, SQLProcedures, SQLProcedureColumns)

#### `/src/odbc/odbc_types.h` (821 lines)
- ODBC type definitions and constants
- SQL type mappings
- Handle type enumerations

---

## Critical Gaps Identified

### Gap 1: Wire Protocol Client Library ⚠️

**Status:** ✅ RESOLVED (network/TLS client implemented)

**Requirement (Alpha):**
ODBC connects via **network listener → parser → engine** using libscratchbird client APIs.

**Implementation:**
Network client path implemented with TLS 1.3 support (see `src/client/network_client.cpp` and ODBC bridge).

---

### Gap 2: Catalog Function Implementation 🔴 MISSING

**Status:** ✅ RESOLVED (all 10 functions implemented with tests)

**Functions Needed:**
1. **SQLTables** - List tables/views/system tables (line 1874 in odbc_handles.cpp)
2. **SQLColumns** - Describe table columns (line 1883)
3. **SQLPrimaryKeys** - List primary key columns (line 1892)
4. **SQLForeignKeys** - List foreign key relationships (line 1901)
5. **SQLStatistics** - List indexes and statistics (line 1910)
6. **SQLSpecialColumns** - ROWID or optimal columns (line 1919)
7. **SQLTablePrivileges** - List table permissions (line 1928)
8. **SQLColumnPrivileges** - List column permissions (line 1937)
9. **SQLProcedures** - List stored procedures (line 1946)
10. **SQLProcedureColumns** - Describe procedure parameters (line 1955)

**Implementation Requirements:**
- Query ScratchBird system catalog tables
- Format results as ODBC-compliant result sets
- Handle catalog name, schema name, table name patterns
- Support wildcard matching ('%', '_')
- Map ScratchBird metadata to ODBC expected columns

**Implementation:** Catalog functions implemented in `src/odbc/odbc_handles.cpp` with unit coverage.

---

### Gap 3: Type Conversion Infrastructure ⚠️ PARTIAL

**Status:** ✅ RESOLVED (hybrid mapping + conversion tests)

**Current State:**
- Type mapping and conversion implemented in ODBC bridge with tests.

**Requirements:**
- **ODBC SQL Types → ScratchBird Types** (for parameter binding)
- **ScratchBird Types → ODBC C Types** (for result retrieval)
- Handle all 86 ScratchBird types:
  - Primitives (INT8, INT16, INT32, INT64, UINT8, etc.)
  - Decimals (DECIMAL64, DECIMAL128, DECIMAL256)
  - Temporal (DATE, TIME, TIMESTAMP, INTERVAL)
  - Binary (BINARY, VARBINARY, BLOB)
  - Text (CHAR, VARCHAR, TEXT with encodings)
  - Complex (RECORD, ARRAY, ENUM, SET, VARIANT, JSON, XML, GEOMETRY)
  - Large Objects (CLOB, NCLOB)

**Challenges:**
- ODBC doesn't have native support for many ScratchBird types
- May need to use SQL_C_BINARY or SQL_C_CHAR for complex types
- Need clear mapping strategy for:
  - RECORD → SQL_C_CHAR (JSON representation?)
  - VARIANT → SQL_C_CHAR (tagged union representation?)
  - GEOMETRY → SQL_BINARY or WKB format?
  - INTERVAL types → Map to SQL_INTERVAL_* types

**Decision:** Hybrid mapping (text for JSON/XML/ARRAY/RECORD/VARIANT, binary for GEOMETRY/VECTOR).

---

### Alpha Type Mapping (ODBC-Supported Types)

ODBC types map to ScratchBird native types as follows (Alpha baseline):

**Numeric**
- `SQL_TINYINT` → `INT8` (or `UINT8` when column is unsigned)
- `SQL_SMALLINT` → `INT16` (or `UINT16`)
- `SQL_INTEGER` → `INT32` (or `UINT32`)
- `SQL_BIGINT` → `INT64` (or `UINT64`)
- `SQL_BIT` → `BOOLEAN`
- `SQL_REAL` → `FLOAT32`
- `SQL_FLOAT`, `SQL_DOUBLE` → `FLOAT64`
- `SQL_DECIMAL`, `SQL_NUMERIC` → `DECIMAL64/128/256` (by precision/scale)

**Text / Unicode**
- `SQL_CHAR` → `CHAR`
- `SQL_VARCHAR` → `VARCHAR`
- `SQL_LONGVARCHAR` → `TEXT`
- `SQL_WCHAR` → `NCHAR` (or UTF-8 `TEXT` with conversion)
- `SQL_WVARCHAR` → `NVARCHAR` (or UTF-8 `TEXT` with conversion)
- `SQL_WLONGVARCHAR` → `NTEXT` (or UTF-8 `TEXT` with conversion)

**Binary**
- `SQL_BINARY` → `BINARY`
- `SQL_VARBINARY` → `VARBINARY`
- `SQL_LONGVARBINARY` → `BLOB` / `BYTEA`

**Temporal**
- `SQL_DATE` / `SQL_TYPE_DATE` → `DATE`
- `SQL_TIME` / `SQL_TYPE_TIME` → `TIME`
- `SQL_TIMESTAMP` / `SQL_TYPE_TIMESTAMP` → `TIMESTAMP`
- `SQL_INTERVAL_*` → `INTERVAL` (fallback to text if not supported)

**Other**
- `SQL_GUID` → `UUID`

**ScratchBird Types without direct ODBC equivalents (Alpha fallback)**
- `JSON`, `XML`, `INET`, `CIDR`, `MACADDR`, `TSVECTOR`, `TSQUERY`, `RANGE` → `SQL_LONGVARCHAR` (text)
- `GEOMETRY` → `SQL_LONGVARBINARY` (WKB), optional text fallback
- `ARRAY`, `COMPOSITE`, `RECORD`, `VARIANT`, `SET`, `ENUM`, `VECTOR` → `SQL_LONGVARCHAR` (JSON/text) or `SQL_LONGVARBINARY` (binary) based on chosen complex-type policy

---

### Gap 4: Result Set Binding 🔴 INCOMPLETE

**Status:** Function stubs exist but core logic marked TODO

**Functions:**
- `bindResultData()` (line 1759, odbc_handles.cpp) - Contains TODO comment
- `convertAndStore()` (line 1810) - Contains TODO comment

**Requirements:**
- Fetch result set from wire protocol
- Convert each column to bound ODBC C type
- Handle NULL indicators
- Support all fetch orientations (NEXT, PRIOR, FIRST, LAST, ABSOLUTE, RELATIVE)
- Implement scrollable cursors if required

**Estimated Effort:** 10-15 hours

---

### Gap 5: Transaction Semantics Mapping ⚠️ NEEDS DESIGN

**Status:** Basic SQLEndTran exists, but semantics unclear

**ScratchBird Model:**
- **Always-in-transaction** (no autocommit off state)
- Transaction ID in every message header
- Immediate new transaction after COMMIT/ROLLBACK
- Savepoint support
- MGA (Multi-Generational Architecture) for MVCC

**ODBC Model:**
- Autocommit mode (default ON)
- Explicit transaction mode (autocommit OFF)
- SQL_ATTR_AUTOCOMMIT connection attribute

**Mapping Strategy Needed:**
- **Option A:** Autocommit ON → COMMIT after every statement
- **Option B:** Autocommit ON → Use single long-lived transaction, COMMIT on disconnect
- **Option C:** Hybrid based on statement type (DDL auto-commits, DML batched)

**Estimated Effort:** 8-12 hours

**Decision Required:** How should ODBC autocommit map to always-in-transaction model?

---

## ODBC 3.x Compliance Matrix

| Category | Required Functions | Implemented | Status | Notes |
|----------|-------------------|-------------|---------|-------|
| **Environment** | 4 | 4 | ✅ 100% | SQLAllocHandle, SQLFreeHandle, SQLSetEnvAttr, SQLGetEnvAttr |
| **Connection** | 11 | 10 | ⚠️ 91% | Missing SQLBrowseConnect |
| **Statement** | 18 | 16 | ⚠️ 89% | Missing SQLBulkOperations, SQLSetPos |
| **Descriptor** | 7 | 7 | ✅ 100% | All implemented |
| **Diagnostic** | 2 | 2 | ✅ 100% | SQLGetDiagRec, SQLGetDiagField |
| **Catalog** | 10 | 0 | 🔴 0% | All are stubs |
| **Type Conversion** | N/A | Partial | ⚠️ 30% | Basic types only |
| **Result Binding** | N/A | Partial | ⚠️ 40% | Core logic TODO |
| **Transaction** | 1 | 1 | ⚠️ 70% | Semantics need clarification |

**Overall Compliance:** ~65% (skeleton exists, critical gaps remain)

---

## Decisions (Resolved for Alpha)

- **libscratchbird usage:** ODBC connects via **network listener → parser → engine** only; embedded mode not exposed.
- **Complex types:** Hybrid mapping (text for JSON/XML/ARRAY/RECORD, binary for GEOMETRY/VECTOR).
- **Catalog scope:** All 10 catalog functions implemented.
- **Federation visibility:** Current database only.

---

## Alpha Defaults (Confirmed)

- **Complex types:** Hybrid mapping (text for JSON/XML/ARRAY/RECORD, binary for GEOMETRY/VECTOR)
- **Catalog scope:** Full 10 catalog functions
- **Federation visibility:** Current database only

---

## Dependencies

### Internal Dependencies (ScratchBird)

1. **scratchbird_client** ✅ EXISTS (IPC, protocol v1.0)
   - Local IPC transport only (Unix socket / named pipe / TCP localhost)
   - No TLS; no v1.1 header format
2. **Network protocol alignment** ⚠️ OPEN
   - Native protocol adapter exists server-side
   - Client-side implementation not present

3. **System Catalog Tables** ✅ ASSUMED READY
   - Metadata tables for SQLTables, SQLColumns, etc.
   - Permission tables for privilege catalog functions
   - Procedure tables if SQLProcedures needed

4. **Schema/Database DDL** ✅ AVAILABLE (Plan 02B core complete)
   - Alignment/testing still in progress; see `docs/archive/2026-01-04/planning/PLAN_02B_SCHEMA_DATABASE_DDL.md`

### External Dependencies

1. **OpenSSL/TLS Library** (for TLS 1.3)
2. **ODBC Driver Manager** (unixODBC/iODBC on Linux, Windows built-in)
3. **C++17 Compiler** (for implementation)

---

## Proposed Implementation Plan

### Phase 1: Integrate libscratchbird Network Client (40-60 hours)

**Tasks:**
1. Implement/complete **libscratchbird network client** aligned with native wire protocol spec
2. Ensure connection path is **ODBC → libscratchbird → network listener → parser → engine**
3. Build ODBC client bridge layer around libscratchbird APIs
4. Confirm TLS 1.3 parameters flow from ODBC connection string to libscratchbird

**Estimated:** 40-60 hours

---

### Phase 2: Type Conversion Infrastructure (15-20 hours)

**Tasks:**
1. Create comprehensive type mapping tables:
   - ODBC SQL types → ScratchBird types (for binding)
   - ScratchBird types → ODBC C types (for retrieval)
2. Implement `convertAndStore()` for all 86 types
3. Handle complex types (RECORD, VARIANT, ARRAY, GEOMETRY, JSON, XML)
4. Implement NULL handling
5. Add length/indicator buffer support
6. Test all type conversions

**Deliverables:**
- `/src/odbc/type_conversion.cpp` (new file)
- `/src/odbc/type_conversion.h` (new file)
- Comprehensive type mapping documentation

---

### Phase 3: Result Set Binding and Fetching (10-15 hours)

**Tasks:**
1. Implement `bindResultData()` (line 1759)
2. Implement result set iteration
3. Support SQLFetch (NEXT orientation)
4. Support SQLFetchScroll (all orientations)
5. Implement SQLGetData for unbound columns
6. Handle scrollable cursor if needed
7. Memory management for result buffers

**Deliverables:**
- Complete implementation of fetch functions
- Test with large result sets

---

### Phase 4: Catalog Functions (Full 10) (20-30 hours)

**Tasks:**
1. **SQLTables** (4 hours)
   - Query system catalog
   - Format result set (TABLE_CAT, TABLE_SCHEM, TABLE_NAME, TABLE_TYPE, REMARKS)
   - Support pattern matching
2. **SQLColumns** (5 hours)
   - Column metadata from catalog
   - Data type mapping to ODBC types
   - Nullable, default values
3. **SQLPrimaryKeys** (3 hours)
   - Primary key columns
   - Key sequence
4. **SQLForeignKeys** (4 hours)
   - Foreign key relationships
   - Update/delete rules
5. **SQLStatistics** (3 hours)
   - Index metadata
   - Cardinality estimates
6. **SQLSpecialColumns** (2 hours)
   - ROWID or optimal row identifier
7. **SQLTablePrivileges** (3 hours)
   - Table-level permissions
8. **SQLColumnPrivileges** (3 hours)
   - Column-level permissions
9. **SQLProcedures** (2 hours)
   - Procedure listing (if supported)
10. **SQLProcedureColumns** (2 hours)
    - Procedure parameter metadata

**Deliverables:**
- All 10 catalog functions fully implemented
- Integration tests with ODBC test tools

---

### Phase 5: Transaction and Concurrency (8-12 hours)

**Tasks:**
1. Implement autocommit mode mapping (commit after every statement when ON)
2. Update SQLEndTran to handle both autocommit ON/OFF
3. Implement SQL_ATTR_AUTOCOMMIT connection attribute
4. Add transaction isolation level support:
   - SQL_ATTR_TXN_ISOLATION
   - Map to ScratchBird isolation levels
5. Test transaction scenarios:
   - Autocommit ON with multiple statements
   - Autocommit OFF with explicit COMMIT/ROLLBACK
   - Nested transactions / savepoints
6. Handle concurrent connections

**Deliverables:**
- Robust transaction handling
- Transaction test suite

---

### Phase 6: Advanced Features (Optional - 25-30 hours)

**Tasks (if Level 1/Level 2 conformance desired):**
1. **SQLBrowseConnect** (6 hours) - Interactive connection building
2. **SQLBulkOperations** (8 hours) - Bulk insert/update/delete
3. **SQLSetPos** (6 hours) - Positioned update/delete
4. **Scrollable Cursors** (5 hours) - If not already complete
5. **Asynchronous Execution** (10 hours) - SQL_ATTR_ASYNC_ENABLE

**Deliverables:**
- Level 1 or Level 2 ODBC conformance

---

### Phase 7: Testing and Compliance (30-40 hours)

**Tasks:**
1. **Unit Tests:**
   - Type conversion edge cases
   - Catalog function results
   - Transaction scenarios
2. **Integration Tests:**
   - ODBC Test Suite (Microsoft ODBC Test)
   - unixODBC isql testing
   - Third-party tools (DBeaver, SQL Workbench/J, Excel, Tableau)
3. **Compliance Testing:**
   - ODBC 3.x conformance test suite
   - Document deviations if any
4. **Performance Testing:**
   - Connection establishment time
   - Query throughput
   - Large result set handling
5. **Security Testing:**
   - TLS 1.3 enforcement
   - Authentication flows
   - SQL injection resistance

**Deliverables:**
- Comprehensive test suite
- Compliance certification documentation
- Performance benchmarks

---

## Total Effort Estimate

### Minimum Viable ODBC Driver (Core Functions Only)
- Phase 1 (client integration): 8-12 hours (if libscratchbird ready) OR 40-60 hours (if not)
- Phase 2 (type conversion): 15-20 hours
- Phase 3 (result binding): 10-15 hours
- Phase 4 (catalog - minimum): 15 hours (SQLTables, SQLColumns, SQLPrimaryKeys only)
- Phase 5 (transactions): 8-12 hours
- Phase 7 (testing): 20-30 hours

**Total Minimum:** 76-104 hours (IPC client ready) OR 108-152 hours (network/TLS client needed)

### Full ODBC 3.x Compliance
- All phases including Phase 6 (advanced features)
- Full catalog function set (all 10)
- Comprehensive testing

**Total Full:** 148-207 hours

---

## Critical Success Factors

1. **libscratchbird.so/dll verification** - Blocks all work
2. **Type mapping decisions** - Affects compatibility with tools
3. **Transaction mapping decisions** - Affects ODBC semantics compliance
4. **Catalog function scope** - Determines tool compatibility

---

## Risks and Mitigation

### Risk 1: libscratchbird.so/dll Insufficient
**Impact:** HIGH - Would require implementing wire protocol client
**Mitigation:** Verify immediately, implement client if needed
**Timeline Impact:** +40-60 hours

### Risk 2: Complex Type Mapping Challenges
**Impact:** MEDIUM - Tools may not handle custom types well
**Mitigation:** Provide fallback to string/binary representations
**Timeline Impact:** +5-10 hours for edge cases

### Risk 3: ODBC Compliance Test Failures
**Impact:** MEDIUM - May require rework
**Mitigation:** Incremental testing during implementation
**Timeline Impact:** +10-20 hours for fixes

### Risk 4: Performance Issues
**Impact:** LOW - Functional but slow
**Mitigation:** Profile and optimize after functional completion
**Timeline Impact:** +15-25 hours for optimization

---

## Next Steps - Immediate Actions Required

### Step 1: Implement/Complete libscratchbird Network Client 🔴 CRITICAL

**Action Required:** Ensure libscratchbird provides network/TLS client APIs used by ODBC, with the mandatory parser bridge.

**Blocking:** ODBC execution path and result fetching

---

### Step 2: Proceed with Implementation (Decisions Locked)

**Scope Locked:** Hybrid type mapping, full 10 catalog functions, current-db-only visibility, core conformance.

---

### Step 3: Review and Approve Plan

**Action Required:** User review of:
- Scope accuracy
- Effort estimates (if useful for planning)
- Phase breakdown
- Success criteria

---

### Step 4: Create Implementation Checklist (Similar to Plan 04)

**Action:** After decisions made, create detailed task breakdown:
- `PLAN_05_IMPLEMENTATION_CHECKLIST.md`
- 80-100 tasks across all phases
- Similar format to Plan 04 checklist

---

## Files and Directories

### Existing Files
- `/docs/specifications/wire_protocols/scratchbird_native_wire_protocol.md` (1,946 lines)
- `/src/odbc/odbc_driver.h` (672 lines)
- `/src/odbc/odbc_driver.cpp` (1,117 lines)
- `/src/odbc/odbc_handles.h` (833 lines)
- `/src/odbc/odbc_handles.cpp` (2,184 lines)
- `/src/odbc/odbc_types.h` (821 lines)

### Files to Create
- `/src/odbc/type_conversion.h` (NEW)
- `/src/odbc/type_conversion.cpp` (NEW)
- `/src/odbc/catalog_queries.cpp` (NEW) - Catalog function SQL builders
- `/src/odbc/odbc_client_bridge.cpp` (NEW) - Adapter around libscratchbird client APIs
- `/src/client/network_client.cpp` (NEW) - libscratchbird network client (native protocol)
- `/include/scratchbird/client/network_client.h` (NEW) - Network client API
- `/docs/archive/2026-01-04/planning/PLAN_05_IMPLEMENTATION_CHECKLIST.md` (NEW) - After decisions

### Documentation to Create
- `/docs/odbc/ODBC_TYPE_MAPPING.md` - Type conversion reference
- `/docs/odbc/ODBC_CATALOG_FUNCTIONS.md` - Catalog function specifications
- `/docs/odbc/ODBC_CONFIGURATION.md` - DSN setup, connection strings

---

## References

### Specifications
- ODBC 3.x Specification (Microsoft/X/Open)
- ScratchBird Native Wire Protocol Specification (internal)
- SQL:2023 Standard (for type mappings)

### Existing ScratchBird Plans
- Plan 02B: Schema/Database DDL (core complete; alignment/testing remaining). See `docs/archive/2026-01-04/planning/PLAN_02B_SCHEMA_DATABASE_DDL.md`.
- Plan 03: Authentication/Authorization (used by wire protocol)
- Plan 04: Domain DDL (affects type system)

---

**Status:** READY FOR IMPLEMENTATION
**Next Action:** Begin Phase 1 (libscratchbird network client integration) and create implementation checklist
**Blocking Issues:** libscratchbird network client capability

---

**Last Updated:** 2025-12-31
**Document Owner:** Plan 05 Team (Claude Code)
