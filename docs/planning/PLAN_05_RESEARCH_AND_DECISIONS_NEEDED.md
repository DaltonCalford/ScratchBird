# Plan 05 - Research and Decisions Needed

**Date:** 2025-12-26
**Status:** READY FOR USER INPUT
**Purpose:** Identify all research tasks and decisions required before implementation can begin

---

## Executive Summary

Plan 05 analysis is **COMPLETE**. We have identified:
- ✅ What exists (5,627 lines of ODBC code, 75% of core functions)
- ✅ What's missing (wire protocol client?, catalog functions, type conversion, result binding)
- ✅ What's needed (6 implementation phases, 148-207 hours estimated)

**BLOCKER:** Cannot proceed without answering **7 critical questions** below.

---

## 🔴 CRITICAL RESEARCH REQUIRED

### Research Task 1: libscratchbird.so/dll Capabilities

**STATUS:** 🔴 BLOCKING ALL IMPLEMENTATION

**What We Need to Know:**
Does libscratchbird.so/dll provide a **client-side wire protocol implementation**?

**Specific Questions:**
1. Does libscratchbird.so/dll exist and is it buildable?
2. What functions does it export? (C API? C++ API?)
3. Does it include:
   - Connection establishment with TLS 1.3?
   - Authentication (SCRAM-SHA-256, Certificate, etc.)?
   - Simple query execution (QUERY message)?
   - Extended query protocol (PARSE/BIND/EXECUTE)?
   - Result set retrieval and iteration?
   - Transaction management (COMMIT/ROLLBACK)?
   - Error handling and diagnostics?

**How to Research:**
- [ ] Check if `/src/libscratchbird/` directory exists
- [ ] Check if `/include/scratchbird/client/` directory exists
- [ ] Search for wire protocol client code:
  ```bash
  find /home/dcalford/CliWork/ScratchBird -name "*client*" -type f
  grep -r "wire.*protocol.*client" /home/dcalford/CliWork/ScratchBird/src
  grep -r "class.*Client" /home/dcalford/CliWork/ScratchBird/include
  ```
- [ ] Check for exported symbols in libscratchbird:
  ```bash
  nm -D /path/to/libscratchbird.so | grep -i connect
  nm -D /path/to/libscratchbird.so | grep -i query
  ```
- [ ] Review any existing client examples or tests

**Impact on Timeline:**
- **If EXISTS and complete:** +8-12 hours (integration work)
- **If EXISTS but incomplete:** +20-40 hours (finish implementation)
- **If DOES NOT EXIST:** +40-60 hours (implement from scratch)

**This is the #1 blocker. Nothing else can proceed without this answer.**

---

## 🟡 CRITICAL DECISIONS REQUIRED

### Decision 1: Complex Type Mapping Strategy

**STATUS:** 🟡 NEEDED BEFORE TYPE CONVERSION IMPLEMENTATION (Phase 2)

**Problem:**
ScratchBird has 86 native types, including many that ODBC doesn't support natively:
- RECORD (structured type)
- VARIANT (tagged union)
- ARRAY (variable length arrays)
- GEOMETRY (spatial types)
- JSON/XML (semi-structured)
- ENUM/SET (enumerated types)

**Question:**
How should these complex types be exposed via ODBC?

**Options:**

**Option A: String Serialization (Simplest)**
- RECORD → JSON string (SQL_C_CHAR)
- VARIANT → Tagged JSON (SQL_C_CHAR)
- ARRAY → JSON array (SQL_C_CHAR)
- GEOMETRY → WKT (Well-Known Text) string (SQL_C_CHAR)
- JSON/XML → As-is string (SQL_C_CHAR)
- ENUM/SET → String representation (SQL_C_CHAR)

**Pros:**
- Simple to implement
- Works with all ODBC tools
- Human-readable

**Cons:**
- Inefficient for large structures
- Client must parse JSON/WKT
- Loss of type fidelity

**Option B: Binary Serialization (Efficient)**
- RECORD → Binary format (SQL_C_BINARY)
- VARIANT → Tagged binary (SQL_C_BINARY)
- ARRAY → Binary array (SQL_C_BINARY)
- GEOMETRY → WKB (Well-Known Binary) (SQL_C_BINARY)
- JSON/XML → UTF-8 bytes (SQL_C_BINARY)
- ENUM/SET → Integer codes (SQL_C_LONG)

**Pros:**
- More efficient
- Preserves exact representation

**Cons:**
- Client must understand ScratchBird binary formats
- Less tool compatibility
- Not human-readable

**Option C: Hybrid (Pragmatic)**
- RECORD → JSON string (SQL_C_CHAR)
- VARIANT → JSON string (SQL_C_CHAR)
- ARRAY → JSON string (SQL_C_CHAR)
- GEOMETRY → WKB binary (SQL_C_BINARY) with WKT fallback
- JSON/XML → String (SQL_C_CHAR)
- ENUM → String name (SQL_C_CHAR)
- SET → Comma-separated string (SQL_C_CHAR)

**Pros:**
- Balances efficiency and compatibility
- Follows industry standards (WKB for GIS)
- Good tool support

**Cons:**
- Mixed approach, need to document clearly

**Recommendation:** Option C (Hybrid)

**User Decision Required:** Choose A, B, or C (or specify different mapping)

**Impact:** Affects Phase 2 implementation (15-20 hours)

---

### Decision 2: Autocommit Transaction Mapping

**STATUS:** 🟡 NEEDED BEFORE TRANSACTION IMPLEMENTATION (Phase 5)

**Problem:**
- **ODBC Model:** Autocommit ON (default) vs OFF (explicit transactions)
- **ScratchBird Model:** Always-in-transaction (no autocommit off state)

**Question:**
How should ODBC autocommit mode work with ScratchBird's always-in-transaction model?

**Options:**

**Option A: Strict ODBC Semantics (Autocommit = Immediate COMMIT)**
```
Autocommit ON:
  - Execute statement
  - Send COMMIT to server
  - Server immediately starts new transaction

Autocommit OFF:
  - Execute statements
  - Client tracks statements
  - Only send COMMIT when SQLEndTran called
```

**Pros:**
- Matches ODBC semantics exactly
- Predictable behavior

**Cons:**
- Performance overhead (COMMIT after every statement)
- Network round-trip for each statement in autocommit mode
- May not leverage ScratchBird's transaction batching

**Option B: Deferred Commit (Performance)**
```
Autocommit ON:
  - Execute statements without COMMIT
  - Only COMMIT on disconnect or explicit SQLEndTran

Autocommit OFF:
  - Same behavior (already in transaction)
```

**Pros:**
- Better performance
- Reduces network overhead
- Leverages ScratchBird's transaction model

**Cons:**
- **Violates ODBC semantics**
- Client may expect immediate commit
- Long-lived transactions (visibility issues)

**Option C: Smart Batching (Hybrid)**
```
Autocommit ON:
  - DDL statements (CREATE, DROP, ALTER) → Immediate COMMIT
  - DML statements (INSERT, UPDATE, DELETE) → Immediate COMMIT
  - SELECT statements → No COMMIT (read-only)

Autocommit OFF:
  - All statements in transaction
  - COMMIT only on SQLEndTran
```

**Pros:**
- Balances semantics and performance
- DDL auto-commits (standard behavior)
- DML auto-commits (expected by most tools)
- SELECT doesn't cause unnecessary commits

**Cons:**
- More complex logic
- Need to detect statement type

**Recommendation:** Option C (Smart Batching)

**User Decision Required:** Choose A, B, or C

**Impact:** Affects Phase 5 implementation (8-12 hours)

---

### Decision 3: ODBC Conformance Level Target

**STATUS:** 🟡 NEEDED FOR SCOPE DEFINITION

**Question:**
What ODBC conformance level should Plan 05 target?

**Levels:**

**Core Level:**
- Basic connection, statement, descriptor functions
- Simple query execution
- Basic result fetching
- **What's Missing:** Scrollable cursors, SQLBrowseConnect, positioned updates

**Level 1:**
- Core + Extended features
- SQLBrowseConnect (interactive connection)
- Scrollable cursors (SQL_CURSOR_TYPE)
- Positioned updates/deletes
- **What's Missing:** Bookmarks, advanced cursor features

**Level 2:**
- Level 1 + Advanced features
- Bookmarks
- SQLBulkOperations
- Advanced cursor operations

**Current Implementation Status:**
- Core: ~90% complete (most functions have skeletons)
- Level 1: ~40% complete (scrollable cursors partial, missing SQLBrowseConnect)
- Level 2: ~20% complete (missing most advanced features)

**Options:**

**Option A: Core Level Only**
- Estimated Effort: +30-50 hours (finish core functions)
- Pros: Faster delivery, covers 80% of use cases
- Cons: Some advanced tools may not work

**Option B: Level 1 Conformance**
- Estimated Effort: +55-80 hours (finish core + Level 1)
- Pros: Better tool compatibility, scrollable cursors
- Cons: More complex, longer timeline

**Option C: Level 2 Conformance (Full)**
- Estimated Effort: +80-110 hours (all features)
- Pros: Maximum compatibility
- Cons: Significant additional work, diminishing returns

**Recommendation:** Option B (Level 1) - Best balance of compatibility and effort

**User Decision Required:** Choose A, B, or C

**Impact:** Determines if Phase 6 (Advanced Features) is included

---

### Decision 4: Catalog Function Scope

**STATUS:** 🟡 NEEDED FOR PHASE 4 PLANNING

**Question:**
Which of the 10 catalog functions are REQUIRED for Alpha vs can be deferred?

**All 10 Catalog Functions:**

| Function | Priority | Reasoning | Effort |
|----------|----------|-----------|--------|
| **SQLTables** | 🔴 CRITICAL | Required by all schema browsers, IDEs | 4 hours |
| **SQLColumns** | 🔴 CRITICAL | Required by all schema browsers, IDEs | 5 hours |
| **SQLPrimaryKeys** | 🔴 CRITICAL | Required by schema tools, ORMs | 3 hours |
| **SQLForeignKeys** | 🟡 HIGH | Used by ER diagram tools, schema analyzers | 4 hours |
| **SQLStatistics** | 🟡 HIGH | Used by query optimizers, performance tools | 3 hours |
| **SQLSpecialColumns** | 🟢 MEDIUM | Rarely used (ROWID support) | 2 hours |
| **SQLTablePrivileges** | 🟢 MEDIUM | Used by security/admin tools | 3 hours |
| **SQLColumnPrivileges** | 🟢 MEDIUM | Used by security/admin tools | 3 hours |
| **SQLProcedures** | 🟠 LOW | Only if stored procedures supported | 2 hours |
| **SQLProcedureColumns** | 🟠 LOW | Only if stored procedures supported | 2 hours |

**Options:**

**Option A: Minimum Set (CRITICAL only)**
- SQLTables, SQLColumns, SQLPrimaryKeys
- Estimated Effort: 12 hours
- Pros: Fast delivery, covers most common tools
- Cons: Some tools won't work fully (ER diagrams, query optimizers)

**Option B: Standard Set (CRITICAL + HIGH)**
- SQLTables, SQLColumns, SQLPrimaryKeys, SQLForeignKeys, SQLStatistics
- Estimated Effort: 19 hours
- Pros: Good tool compatibility, covers 90% of use cases
- Cons: Still missing privilege functions

**Option C: Full Set (All 10)**
- All catalog functions
- Estimated Effort: 31 hours
- Pros: Complete catalog support, all tools work
- Cons: Includes rarely-used functions

**Recommendation:** Option B (Standard Set) unless stored procedures are planned, then Option C

**User Decision Required:** Choose A, B, or C

**Impact:** Determines Phase 4 scope (12-31 hours difference)

---

### Decision 5: Federation Visibility in Catalog Functions

**STATUS:** 🟡 NEEDED FOR CATALOG IMPLEMENTATION

**Question:**
Should ODBC catalog functions show federated databases and emulated databases?

**ScratchBird Architecture:**
- Native ScratchBird databases (e.g., `mydb`)
- Federated databases (Oracle, DB2, SQL Server engines)
- Emulated databases under emulation tree (e.g., `emulation.postgres.testdb`)

**Scenario:**
User connects to ScratchBird via ODBC and calls SQLTables with `catalog_name = NULL` (all catalogs).

**Options:**

**Option A: Show Only Current Database**
- SQLTables shows tables only in the connection's current database
- Ignores federation, ignores emulated databases
- **Pros:** Simple, fast, no confusion
- **Cons:** User can't browse other databases via ODBC

**Option B: Show All Native ScratchBird Databases**
- SQLTables shows tables in all ScratchBird native databases
- Excludes federated databases (Oracle, DB2, etc.)
- Excludes emulated databases
- **Pros:** Reasonable scope, pure ScratchBird view
- **Cons:** Hides federated/emulated data

**Option C: Show Everything (Full Transparency)**
- SQLTables shows:
  - All native ScratchBird databases
  - All federated database tables
  - All emulated database tables (under `emulation.*`)
- **Pros:** Complete visibility, user sees entire system
- **Cons:** Potentially overwhelming, complex to implement, security implications

**Option D: Configurable**
- Connection string parameter: `SHOW_FEDERATED=true/false`, `SHOW_EMULATED=true/false`
- Default: Show only current database
- **Pros:** User control, flexible
- **Cons:** More complex implementation

**Recommendation:** Option D (Configurable) with defaults to Option A (current database only)

**User Decision Required:** Choose A, B, C, or D

**Impact:** Affects catalog function complexity in Phase 4

---

### Decision 6: SSL/TLS Configuration

**STATUS:** 🟢 MEDIUM PRIORITY - NEEDS CLARIFICATION

**Question:**
How is TLS 1.3 configured and enforced?

**Wire Protocol Spec Says:** TLS 1.3 mandatory on port 3092

**Questions:**
1. Is TLS handled entirely by libscratchbird.so/dll?
2. Does ODBC driver need to expose TLS configuration options?
   - Certificate path?
   - Certificate validation (strict vs permissive)?
   - Self-signed certificate support for testing?
   - TLS version fallback (1.3 → 1.2)?
3. Connection string parameters needed?
   - `SSL_CERT=/path/to/cert.pem`?
   - `SSL_VERIFY=true/false`?
   - `SSL_CA=/path/to/ca.pem`?

**Assumption:**
libscratchbird handles TLS internally and ODBC driver just passes connection string parameters through.

**User Confirmation Needed:** Is this assumption correct?

**Impact:** May need connection string parsing in Phase 1

---

### Decision 7: Error Message Handling

**STATUS:** 🟢 LOW PRIORITY - DESIGN DECISION

**Question:**
How should ScratchBird error codes/messages be mapped to ODBC SQLSTATE?

**ScratchBird Errors:**
- Status enum with error codes
- Error messages from wire protocol
- Possibly ScratchBird-specific error codes

**ODBC Errors:**
- 5-character SQLSTATE (e.g., "42S02" = table not found)
- Native error code
- Error message text

**Options:**

**Option A: Generic Mapping**
- Map all ScratchBird errors to generic ODBC SQLSTATE codes
- Example: All "not found" errors → "42S02"
- **Pros:** Simple
- **Cons:** Loss of error detail

**Option B: Detailed Mapping**
- Create comprehensive mapping table
- ScratchBird error code → Specific SQLSTATE
- Preserve error nuance
- **Pros:** Better diagnostics
- **Cons:** Requires maintaining mapping table

**Option C: Preserve Native Codes**
- Use generic SQLSTATE classes
- Put ScratchBird error code in native error field
- Full message text preserved
- **Pros:** All info available to client
- **Cons:** Client must understand ScratchBird codes

**Recommendation:** Option C (Preserve Native Codes)

**User Decision Required:** Choose A, B, or C

**Impact:** Minor - affects error handling in all phases

---

## 🔵 OPTIONAL RESEARCH (Nice to Have)

### Research Task 2: Existing ODBC Test Tools

**Question:**
What ODBC test tools and suites should we use for validation?

**Potential Tools:**
- Microsoft ODBC Test Suite (if accessible)
- unixODBC `isql` command-line tool
- ODBC Test by OpenLink Software
- DBeaver (general SQL IDE with ODBC support)
- SQL Workbench/J
- Microsoft Excel (ODBC data import)
- Tableau (ODBC connector)

**Research Needed:**
- [ ] Identify freely available ODBC conformance test suites
- [ ] Determine which tools are priority for compatibility testing
- [ ] Set up test environment with multiple tools

**Impact:** Helps define Phase 7 (Testing) scope

---

### Research Task 3: System Catalog Schema

**Question:**
What system catalog tables exist in ScratchBird for catalog functions to query?

**Needed Tables (or equivalent):**
- `system.tables` or `information_schema.tables` (for SQLTables)
- `system.columns` or `information_schema.columns` (for SQLColumns)
- `system.indexes` or `information_schema.statistics` (for SQLStatistics, SQLPrimaryKeys)
- `system.foreign_keys` or `information_schema.referential_constraints` (for SQLForeignKeys)
- `system.privileges` or `information_schema.table_privileges` (for SQLTablePrivileges)
- `system.procedures` (for SQLProcedures, if applicable)

**Research Needed:**
- [ ] Document exact schema of system catalog tables
- [ ] Identify column names and data types
- [ ] Write sample SQL queries for each catalog function
- [ ] Verify catalog tables are populated correctly

**How to Research:**
```sql
-- Connect to ScratchBird and query:
SHOW TABLES IN system;
SHOW TABLES IN information_schema;
DESCRIBE system.tables;
DESCRIBE system.columns;
```

**Impact:** Required for Phase 4 implementation (Catalog Functions)

---

### Research Task 4: Existing Type System Documentation

**Question:**
Is there existing documentation of the 86 ScratchBird types?

**Needed Information:**
- Complete list of all 86 types
- Type IDs/codes used in wire protocol
- Size and alignment requirements
- Serialization format for each type
- NULL representation for each type

**Where to Look:**
- `/docs/specifications/types/` directory?
- Wire protocol specification type section
- Type definitions in header files:
  ```bash
  grep -r "enum.*Type" /home/dcalford/CliWork/ScratchBird/include
  ```

**Impact:** Speeds up Phase 2 implementation (Type Conversion)

---

## Summary of Immediate Actions

### BLOCKER (Cannot Start Without)
1. **✅ Research libscratchbird.so/dll** - Must verify wire protocol client exists

### CRITICAL DECISIONS (Needed Before Implementation Starts)
2. **✅ Decide: Complex type mapping** (Decision 1)
3. **✅ Decide: Autocommit mapping** (Decision 2)
4. **✅ Decide: ODBC conformance level** (Decision 3)
5. **✅ Decide: Catalog function scope** (Decision 4)

### IMPORTANT DECISIONS (Needed Before Specific Phases)
6. **✅ Decide: Federation visibility** (Decision 5) - Before Phase 4
7. **✅ Clarify: TLS configuration** (Decision 6) - Before Phase 1

### OPTIONAL RESEARCH (Can Do in Parallel)
8. **○ Research: System catalog schema** (Research Task 3) - Helps Phase 4
9. **○ Research: Type system documentation** (Research Task 4) - Helps Phase 2
10. **○ Research: ODBC test tools** (Research Task 2) - Helps Phase 7

---

## Proposed Next Steps

### Step 1: User Reviews This Document
- Understand all questions and options
- Gather information for Research Task 1 (libscratchbird)

### Step 2: User Provides Answers
Create a response document with:
- Research Task 1 results (libscratchbird capabilities)
- Decision 1 choice (A/B/C or custom)
- Decision 2 choice (A/B/C)
- Decision 3 choice (A/B/C)
- Decision 4 choice (A/B/C)
- Decision 5 choice (A/B/C/D)
- Decision 6 clarifications
- Decision 7 choice (A/B/C)

### Step 3: AI Creates Implementation Checklist
After decisions made:
- Create `PLAN_05_IMPLEMENTATION_CHECKLIST.md`
- Break down 6 phases into 80-100 specific tasks
- Similar format to `PLAN_04_IMPLEMENTATION_CHECKLIST.md`
- Include task dependencies and estimated hours per task

### Step 4: User Approves Implementation Plan
- Review checklist
- Approve scope and approach
- Give go-ahead to begin implementation

---

## Questions for User Right Now

1. **Can you verify libscratchbird.so/dll status?** (Research Task 1)
   - Does it exist?
   - What functions does it export?
   - Does it include wire protocol client?

2. **Are you ready to make the 7 decisions listed above?**
   - Or do you need more information first?

3. **Should I proceed to research system catalog schema** (Research Task 3)?
   - Would help prepare for catalog function implementation

4. **Any other concerns or questions about the Plan 05 scope?**

---

**Status:** READY FOR USER INPUT
**Blocking:** Research Task 1 (libscratchbird verification)
**Next Action:** User provides answers to questions and decisions

**Last Updated:** 2025-12-26
