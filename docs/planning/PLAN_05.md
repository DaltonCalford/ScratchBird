# Plan 05 - ScratchBird Native ODBC Driver Implementation

**Plan:** ScratchBird Native ODBC 3.x Driver using libscratchbird.so/dll
**Version:** 1.0
**Date:** 2025-12-26
**Status:** 🔍 ANALYSIS COMPLETE - AWAITING DECISIONS

---

## Executive Summary

Plan 05 implements a **full ODBC 3.x compliant driver** for the ScratchBird native wire protocol. This is NOT about emulated database protocols (PostgreSQL/MySQL/Firebird use their own ODBC drivers for server-side emulation). This driver allows ODBC client applications to connect directly to ScratchBird using the native wire protocol.

**Current State:**
- ✅ **5,627 lines of ODBC code** already implemented across 5 files
- ✅ **75% of core ODBC functions** have skeleton or partial implementations (39/52)
- ✅ **Complete wire protocol specification** exists
- ⚠️ **Critical gaps identified**: Wire protocol client, catalog functions, type conversion
- ⚠️ **Decisions needed**: See "Open Questions" section below

**Scope:**
- ODBC 3.x full specification compliance (NOT a subset)
- Native ScratchBird wire protocol (port 3092, TLS 1.3 mandatory)
- Uses libscratchbird.so/dll only
- Support for all 86 ScratchBird native types
- Full catalog function support (SQLTables, SQLColumns, etc.)

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

### Gap 1: Wire Protocol Client Library ⚠️ NEEDS VERIFICATION

**Status:** UNKNOWN - Need to verify libscratchbird.so/dll capabilities

**Questions:**
1. Does libscratchbird.so/dll include a **client-side wire protocol implementation**?
2. Does it provide C API functions for:
   - Connection establishment (with TLS 1.3)
   - Authentication (SCRAM-SHA-256, etc.)
   - Simple query execution
   - Extended query protocol (PARSE/BIND/EXECUTE)
   - Result set retrieval
   - Transaction management
   - Error handling

**If YES:** Estimate 8-12 hours integration work
**If NO:** Estimate 40-60 hours to implement client library

**Decision Required:** User must confirm libscratchbird capabilities

---

### Gap 2: Catalog Function Implementation 🔴 MISSING

**Status:** All 10 functions are STUBS returning SQL_SUCCESS with no data

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

**Estimated Effort:** 20-30 hours

---

### Gap 3: Type Conversion Infrastructure ⚠️ PARTIAL

**Status:** Basic framework exists, but incomplete for 86 ScratchBird types

**Current State:**
- Some conversions in `convertAndStore()` (line 1810, odbc_handles.cpp) - marked TODO
- No comprehensive mapping table

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

**Estimated Effort:** 15-20 hours

**Decision Required:** How should complex ScratchBird types be exposed via ODBC?

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

## Open Questions Requiring Decisions

### Question 1: libscratchbird.so/dll Capabilities 🔴 CRITICAL

**Question:** Does libscratchbird.so/dll include a client-side wire protocol implementation with C API?

**Why Critical:** This determines whether we need 8 hours (integration) or 60 hours (implement from scratch)

**User Must Provide:**
- Confirmation of libscratchbird capabilities
- API documentation if it exists
- Or confirmation we need to implement client library

---

### Question 2: Complex Type Mapping Strategy

**Question:** How should ScratchBird-specific types be exposed via ODBC?

**Specific Types:**
- **RECORD**: JSON string? SQL_C_CHAR? Custom binary format?
- **VARIANT**: Tagged JSON? SQL_C_BINARY with type prefix?
- **ARRAY**: JSON array? Delimited string? Binary?
- **GEOMETRY**: WKB (Well-Known Binary)? WKT (Well-Known Text)? GeoJSON?
- **JSON/XML**: SQL_C_CHAR with content? SQL_WCHAR for XML?
- **ENUM/SET**: String representation? Integer codes? Both?

**Options:**
- **A:** Use SQL_C_CHAR (string) for all complex types with JSON/text serialization
- **B:** Use SQL_C_BINARY for structured types, SQL_C_CHAR for text-like types
- **C:** Define custom ODBC type codes (SQL_SB_RECORD, etc.) - non-standard but precise

**Recommendation Needed:** User decision on type mapping philosophy

---

### Question 3: Autocommit Mapping Strategy

**Question:** How should ODBC autocommit mode interact with always-in-transaction model?

**Options:**
- **A:** Autocommit ON → Issue COMMIT after every statement execution
  - Pro: Matches ODBC semantics exactly
  - Con: Performance overhead for every statement

- **B:** Autocommit ON → Single transaction, commit only on disconnect
  - Pro: Better performance
  - Con: Violates ODBC semantics, long-lived transactions

- **C:** Autocommit ON → COMMIT after DDL/DML, keep SELECT in transaction
  - Pro: Balances performance and semantics
  - Con: More complex logic

**Recommendation Needed:** User decision on transaction mapping

---

### Question 4: ODBC Conformance Level Target

**Question:** What ODBC conformance level is required?

**Levels:**
- **Core:** Minimal ODBC functionality
- **Level 1:** Core + extended features (scrollable cursors, SQLBrowseConnect, etc.)
- **Level 2:** Level 1 + advanced features (bookmarks, positioned updates, etc.)

**Current Status:** Most Core functions exist, some Level 1 features missing

**Decision:** Should we target Core, Level 1, or Level 2 conformance?

---

### Question 5: Catalog Function Scope

**Question:** Which catalog functions are REQUIRED vs OPTIONAL for Alpha?

**All 10 Functions:**
1. SQLTables ✓ (REQUIRED for most tools)
2. SQLColumns ✓ (REQUIRED for most tools)
3. SQLPrimaryKeys ✓ (REQUIRED for schema tools)
4. SQLForeignKeys ⚠️ (Nice to have)
5. SQLStatistics ⚠️ (Nice to have)
6. SQLSpecialColumns ⚠️ (Rarely used)
7. SQLTablePrivileges ⚠️ (Security tools)
8. SQLColumnPrivileges ⚠️ (Security tools)
9. SQLProcedures ⚠️ (If procedures supported)
10. SQLProcedureColumns ⚠️ (If procedures supported)

**Recommendation:** Implement at minimum: SQLTables, SQLColumns, SQLPrimaryKeys (15 hours)
**Full Set:** All 10 functions (20-30 hours)

**Decision:** User approval on scope

---

### Question 6: SSL/TLS Implementation

**Question:** How is TLS 1.3 mandatory enforcement handled?

**Wire Protocol Spec Says:** TLS 1.3 mandatory on port 3092

**ODBC Driver Needs:**
- Connection string parameter for certificate validation?
- Support for self-signed certificates (testing)?
- Certificate path configuration?
- TLS version negotiation (fallback to 1.2?)?

**Assumption:** libscratchbird.so/dll handles TLS internally
**Verify:** User confirmation this is correct

---

### Question 7: Federation and Multi-Database Support

**Question:** How should ODBC catalog name parameter work with ScratchBird federation?

**ScratchBird Model:**
- Multi-database with federation (Oracle/DB2/SQL Server/ScratchBird engines)
- Emulated databases under emulation tree (emulation.postgres.mydb)
- Cross-database queries supported

**ODBC Catalog Functions:**
- Accept `catalog_name` parameter (database name in ODBC terms)
- NULL means current database

**Questions:**
1. Should catalog functions list ALL databases (federated + emulated)?
2. Should they respect connection's current database only?
3. How to represent emulated databases in catalog results?
4. Should SQLTables distinguish native vs emulated tables?

**Decision:** User input on catalog scope and federation visibility

---

## Dependencies

### Internal Dependencies (ScratchBird)

1. **libscratchbird.so/dll** ⚠️ NEEDS VERIFICATION
   - Must provide client wire protocol implementation
   - OR we must implement client library first

2. **System Catalog Tables** ✅ ASSUMED READY
   - Metadata tables for SQLTables, SQLColumns, etc.
   - Permission tables for privilege catalog functions
   - Procedure tables if SQLProcedures needed

3. **Schema/Database DDL** ⚠️ BLOCKED (Plan 02B)
   - If catalog functions need to query schema metadata
   - May be able to work around with existing catalog

### External Dependencies

1. **OpenSSL/TLS Library** (for TLS 1.3)
2. **ODBC Driver Manager** (unixODBC/iODBC on Linux, Windows built-in)
3. **C++17 Compiler** (for implementation)

---

## Proposed Implementation Plan

### Phase 1: Verify and Integrate Wire Protocol Client (8-60 hours)

**Tasks:**
1. Verify libscratchbird.so/dll capabilities
2. Document client API if exists
3. OR implement client library if needed:
   - Connection management with TLS 1.3
   - Authentication flows
   - Simple query execution
   - Extended query protocol
   - Result set retrieval
   - Transaction commands
4. Create integration layer between ODBC driver and client library

**Estimated:** 8-12 hours (if exists) OR 40-60 hours (if must implement)

**Decision Gate:** CANNOT proceed without knowing libscratchbird status

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

### Phase 4: Catalog Functions (20-30 hours)

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
1. Implement autocommit mode mapping (based on decision)
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

**Total Minimum:** 76-104 hours (if client ready) OR 108-152 hours (if client needed)

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

### Step 1: Verify libscratchbird.so/dll Capabilities 🔴 CRITICAL

**Action Required:** User must provide:
- Documentation for libscratchbird.so/dll
- List of exported functions
- Confirmation of wire protocol client support
- OR confirmation we need to implement client library

**Blocking:** All implementation work

---

### Step 2: Make Architecture Decisions

**Decisions Needed:**
1. Complex type mapping strategy (Question 2)
2. Autocommit mapping strategy (Question 3)
3. ODBC conformance level target (Question 4)
4. Catalog function scope (Question 5)
5. Federation visibility in catalog (Question 7)

**Impact:** Affects design and implementation approach

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
- `/src/odbc/wire_protocol_client.cpp` (NEW) - If libscratchbird insufficient
- `/docs/planning/PLAN_05_IMPLEMENTATION_CHECKLIST.md` (NEW) - After decisions

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
- Plan 02B: Schema/Database DDL (dependency for catalog functions)
- Plan 03: Authentication/Authorization (used by wire protocol)
- Plan 04: Domain DDL (affects type system)

---

**Status:** READY FOR DECISION GATE
**Next Action:** User to answer Open Questions and approve plan
**Blocking Issues:** libscratchbird.so/dll verification (Question 1)

---

**Last Updated:** 2025-12-26
**Document Owner:** Plan 05 Team (Claude Code)
