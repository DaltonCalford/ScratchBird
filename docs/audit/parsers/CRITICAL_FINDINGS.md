# Parser Audit - Critical Findings Summary

**Audit Date:** 2026-01-07  
**Status Updated:** 2026-01-18 (parser remediation complete; full `ctest --test-dir build` pass with gated network skips)
**Audited By:** Comprehensive code analysis (automated + manual review)
**Total Lines Analyzed:** ~23,000 lines across 4 parsers

---

## Executive Summary

Four parsers were audited for dialect purity and cross-contamination:
1. **V2 Parser** (native ScratchBird) - 5,394 lines
2. **FirebirdSQL Parser** (emulated) - 3,800 lines
3. **PostgreSQL Parser** (emulated) - 9,338 lines
4. **MySQL Parser** (emulated) - 4,117 lines

### Overall Verdict

| Parser | Dialect Purity | Critical Issues | Production Ready |
|--------|---------------|-----------------|------------------|
| **V2** | ⚠️ Mixed (approved PG-style extensions) | None (parser remediation complete) | ✅ **YES** (Alpha) |
| **FirebirdSQL** | ✅ **EXCELLENT** (100% pure) | None | ✅ **YES** |
| **PostgreSQL** | ✅ **EXCELLENT** (100% pure) | None (bytecode alignment complete) | ✅ **YES** (Alpha) |
| **MySQL** | ✅ **GOOD** (99% pure) | None (bytecode alignment complete) | ✅ **YES** (Alpha) |

---

## Phase 0 Verification (2026-01-14) - Resolution Status (2026-01-18)

Goal: validate that every parsed feature executes end-to-end (parser → semantic → bytecode → executor) with **no stubs**.
Result: **parser remediation complete**; core dialect bytecode alignment and parsed-feature wiring now pass the full test suite.

### Status Summary
- **V2 pipeline gaps resolved** (domains, ALTER TABLE constraints, COPY options, temp objects, CTE/PSQL).
- **Dialect bytecode alignment complete** for PostgreSQL/MySQL/Firebird core DDL/DML.
- **Parsed-only features eliminated** (implemented or explicitly rejected with clear errors/warnings).

### Known Alpha Limitations (Explicit Warnings/Errors, Not Silent)
- TRUNCATE `CASCADE` / `RESTART IDENTITY`: warning + proceed without cascade/restart.
- `SIMILAR TO ... ESCAPE`: warning; ESCAPE ignored.
- COPY `ENCODING BINARY`: unsupported (UTF8/UTF-8 only in Alpha).
- Aggregation with joins/CTE and `SELECT *` aggregation: executor limitation (tracked in core engine plan).
- Dialect guardrails: MySQL partition clauses and `LOCK/UNLOCK TABLES` remain explicit errors; non‑dialect DDL is rejected by allowlists.

---

## NOTE: MySQL/PostgreSQL Transaction Isolation Mapping (INTENTIONAL)

**Status:** ✅ **WORKING AS DESIGNED** - Not a contamination issue

**File:** `src/parser/mysql/mysql_parser.cpp` (lines 3664-3734), `src/parser/postgresql/pg_parser.cpp`

### Background

MySQL and PostgreSQL parsers map their MVCC-based transaction isolation levels to Firebird MGA isolation constants. This is **INTENTIONAL SEMANTIC MAPPING**, not dialect contamination.

```cpp
// INTENTIONAL: MySQL/PostgreSQL MVCC → Firebird MGA mapping
constexpr uint8_t kIsoReadCommitted = 0;          // Firebird MGA
constexpr uint8_t kIsoSnapshot = 2;                // Firebird MGA
constexpr uint8_t kIsoSnapshotTableStability = 3;  // Firebird MGA

// MySQL SERIALIZABLE → Firebird SNAPSHOT TABLE STABILITY (closest match)
if (matchKeyword(TokenType::KW_SERIALIZABLE)) {
    isolation = kIsoSnapshotTableStability;
}

// MySQL REPEATABLE READ → Firebird SNAPSHOT (closest match)
else if (matchKeyword(TokenType::KW_REPEATABLE)) {
    isolation = kIsoSnapshot;
}
```

### Why This Is Correct

1. **ScratchBird uses Firebird MGA architecture**, not PostgreSQL/MySQL MVCC
2. **Emulated parsers must map to available semantics** - no choice but to translate
3. **Closest semantic match** - mappings provide reasonable behavior:
   - MySQL/PostgreSQL `READ UNCOMMITTED` → Firebird `READ COMMITTED` (MGA doesn't support dirty reads)
   - MySQL/PostgreSQL `READ COMMITTED` → Firebird `READ COMMITTED` (exact match)
   - MySQL/PostgreSQL `REPEATABLE READ` → Firebird `SNAPSHOT` (closest approximation)
   - MySQL/PostgreSQL `SERIALIZABLE` → Firebird `SNAPSHOT TABLE STABILITY` (closest approximation)

### Documentation Requirements

✅ Transaction isolation mapping is documented in:
- `/docs/audit/parsers/SBLR_OPCODE_MAPPING.md` (Transaction Isolation Mapping section)
- `/docs/audit/parsers/COMPARISON_MATRIX.md` (Transaction Control section)
- This document

**Verdict:** This is correct architectural behavior. MySQL/PostgreSQL applications using ScratchBird will execute with Firebird MGA semantics, which is the expected behavior for emulated database parsers running on a Firebird-based engine.

---

## CRITICAL ISSUE #1: V2 Parser - Incomplete PSQL Implementation

**Severity:** HIGH - Feature Gap
**Status:** RESOLVED - V2 parser now implements PSQL + CTE parsing and is wired through semantic analysis and bytecode generation with tests.

**File:** `src/parser/parser_v2.cpp`
**Lines:** 299-302 (TODO comments), AST nodes defined but not parsed

### The Problem

V2 parser defines AST nodes for procedural SQL but never parses them:

```cpp
// TODO: Add more CREATE types
// if (matchContextual("FUNCTION"))   return parseCreateFunction(or_replace);
// if (matchContextual("PROCEDURE"))  return parseCreateProcedure(or_replace);
// if (matchContextual("TRIGGER"))    return parseCreateTrigger();
```

**Previously Missing Implementations (now resolved):**
- CREATE FUNCTION, CREATE PROCEDURE, CREATE TRIGGER
- EXECUTE BLOCK
- IF/WHILE/FOR SELECT statements
- Exception handling
- WITH clauses (CTEs)

**AST Infrastructure Status:**
- `ExecuteBlockStmt`, `IfStmt`, `WhileStmt`, `ForSelectStmt` now parsed and populated in V2
- `WithClause` fields in INSERT/UPDATE/DELETE are now populated and emitted as bytecode

### Impact (Resolved)

- V2 parser handles procedural code, CTEs, and CREATE FUNCTION/PROCEDURE/TRIGGER without falling back

### Recommended Action

1. Implement PSQL parsing functions to populate existing AST nodes
2. Implement CTE parsing (WITH clause)
3. Complete CREATE FUNCTION/PROCEDURE/TRIGGER parsers
4. Add comprehensive tests for procedural features

---

## CRITICAL ISSUE #2: V2 Parser - PostgreSQL Feature Contamination

**Severity:** MEDIUM - Dialect Bleeding
**Status:** ACCEPTED - V2 keeps PG-style syntax as approved extensions (Firebird base + explicit extensions).

**File:** `src/parser/parser_v2.cpp`
**Violations:** Multiple PostgreSQL-specific features in V2 parser

### PostgreSQL Features Found in V2

1. **UPDATE ... FROM clause** (lines 2782-2799)
   - PostgreSQL extension, not standard SQL
   - Firebird uses different syntax

2. **DELETE ... USING clause** (lines 2868-2885)
   - PostgreSQL-specific
   - Firebird doesn't support USING

3. **INSERT ... ON CONFLICT** (lines 2657-2741)
   - PostgreSQL 9.5+ upsert syntax
   - Firebird uses MERGE or UPDATE OR INSERT

4. **DROP ... CASCADE**
   - PostgreSQL-style cascade (accepted in V2)
   - Firebird also has CASCADE but with different semantics

### Why This Matters

**V2 Parser is supposed to follow FirebirdSQL style** per specification:
> "The V2 parser which is the core parser of the project, will have many more expansions than any of the other three but in Style/Formatting it should follow the FirebirdSQL standard."

Having PostgreSQL-specific syntax in V2 creates confusion:
- Is V2 supposed to support PostgreSQL extensions?
- How do we distinguish "V2 expansions" from "PostgreSQL syntax"?
- What happens when PostgreSQL and Firebird syntax conflict?

### Recommended Action

**Decision:** Option B - Document V2 as "Firebird base + approved extensions" and list PG-style features as intentional.

---

## CRITICAL ISSUE #3: TEMPORARY TABLES - Parsed But Not Implemented (ALL PARSERS)

**Severity:** CRITICAL - Silent Feature Failure
**Status:** RESOLVED - temp table metadata, bytecode, executor isolation, and ON COMMIT semantics implemented with tests.

**Files:** All 4 parsers
**Impact:** All parsers accept temporary table syntax but silently create **PERMANENT** tables

### The Problem

All four parsers parse TEMPORARY/TEMP keywords and set AST flags, but the entire implementation chain **ignores these flags**:

```sql
-- ALL OF THESE CREATE PERMANENT TABLES (SILENT FAILURE):
CREATE TEMPORARY TABLE test (id INT);           -- V2/Firebird
CREATE GLOBAL TEMPORARY TABLE test (id INT)     -- Firebird
    ON COMMIT DELETE ROWS;
CREATE TEMP TABLE test (id INT);                -- PostgreSQL
CREATE TEMPORARY TABLE test (id INT);           -- MySQL
```

### Parser Implementation Status

| Parser | Syntax Parsing | Flag Set | Bytecode Emitted | Executor Handles | Result |
|--------|---------------|----------|------------------|------------------|---------|
| **Firebird** | ✅ | ✅ | ✅ | ✅ | TEMP table |
| **V2** | ✅ | ✅ | ✅ | ✅ | TEMP table |
| **PostgreSQL** | ✅ | ✅ | ✅ | ✅ | TEMP table |
| **MySQL** | ✅ | ✅ | ✅ | ✅ | TEMP table |

### Implementation Status (Resolved)

- Bytecode generator writes temp/on-commit flags for CREATE TABLE.
- Executor enforces session/transaction isolation and ON COMMIT behavior.
- Catalog stores temp metadata and session ownership.

### Additional Issue: ON COMMIT Clause (Firebird)

Firebird parser **parses but discards** ON COMMIT clause:

**File:** `src/parser/firebird/firebird_parser.cpp:1943-1952`
```cpp
// Optional ON COMMIT clause for GTT
if (temporary && matchKeyword(TokenType::KW_ON)) {
    consume(TokenType::KW_COMMIT, "Expected COMMIT after ON");
    if (matchKeyword(TokenType::KW_DELETE)) {
        // ON COMMIT DELETE ROWS (default for Firebird)
    } else if (matchKeyword(TokenType::KW_PRESERVE)) {
        // ON COMMIT PRESERVE ROWS
    }
    matchKeyword(TokenType::KW_ROWS);  // Optional ROWS keyword
}
```

**Status:** RESOLVED - ON COMMIT action stored in AST and enforced at execution time.

### Impact

**User Impact:**
- Users expect temporary tables to be session-scoped
- Users expect ON COMMIT DELETE ROWS to truncate on commit
- Instead, they get permanent tables that persist after disconnect
- **Silent failure** - no error, no warning

**Data Integrity:**
- Temporary tables pollute main schema
- No automatic cleanup
- Cross-session visibility leakage (security issue)

**Compliance:**
- Breaks PostgreSQL/MySQL/Firebird compatibility
- Applications migrating from these databases will behave incorrectly

### Resolution Summary

- AST enums for temp type/on-commit added and populated by all parsers.
- CREATE TABLE bytecode carries temp flags.
- Catalog stores temp metadata and session/transaction ownership.
- Executor isolates visibility and applies ON COMMIT rules.
- Tests cover parsing + lifecycle semantics.

**Related Features (Phase 5):**
- TEMPORARY VIEWS: session-scoped temp metadata; non-persistent in Alpha.
- TEMPORARY SEQUENCES: session-scoped temp metadata; non-persistent in Alpha.
- UNLOGGED TABLES: warn + treated as regular tables under MGA.

---

## ISSUE #4: PostgreSQL Parser - Executor Format Mismatches

**Severity:** MEDIUM - Runtime Compatibility  
**Status:** RESOLVED - PostgreSQL bytecode payloads align with executor format; core DDL/DML pass in full test run.

**Files:** `src/parser/postgresql/` (alignment complete)

### Resolution Summary

- Bytecode payload order/count now matches executor for CREATE/ALTER/INSERT/UPDATE/DELETE/SELECT.
- Parser + integration tests cover the aligned formats.

---

## ISSUE #5: V2 Parser - Incomplete Index Type Support

**Severity:** HIGH - Feature Gap
**Status:** RESOLVED - V2 AST/parser/semantic/bytecode now cover all 11 index types with tests.

**Files:** Multiple (AST, parser, semantic analyzer, bytecode generator)

### The Problem

V2 parser only supports **5 of 11 production-ready index types**, even though the storage engine, catalog, and SBLR opcodes fully support all 11 types. This prevents users from creating spatial indexes, vector indexes, columnstore indexes, and other advanced index types through V2 SQL syntax.

**V2 Parser Currently Supports:**
- BTREE ✅
- HASH ✅
- GIN ✅
- GIST ✅
- BRIN ✅

**Previously Missing in V2 (now implemented):**
- SPGIST (Space-Partitioned GiST)
- RTREE (R-Tree spatial index)
- HNSW (Vector similarity search)
- BITMAP (Low cardinality columns)
- COLUMNSTORE (Column-oriented storage)
- LSM (Log-Structured Merge-Tree)

### Implementation Status Across Layers

| Index Type | V2 AST | V2 Parser | Semantic Analyzer | Bytecode Gen | SBLR Opcodes | Catalog | Storage Engine |
|------------|--------|-----------|-------------------|--------------|--------------|---------|----------------|
| BTREE | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| HASH | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| GIN | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| GIST | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ⚠️ |
| BRIN | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| SPGIST | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| RTREE | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| HNSW | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| BITMAP | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| COLUMNSTORE | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| LSM | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |

### Critical Bug: Dead Code in Semantic Analyzer (Resolved)

**File:** `src/sblr/semantic_analyzer_v2.cpp:3210`

```cpp
switch (stmt->index_type) {
    case IndexType::BTREE: resolved->index_method = internString("btree"); break;
    case IndexType::HASH: resolved->index_method = internString("hash"); break;
    case IndexType::GIN: resolved->index_method = internString("gin"); break;
    case IndexType::GIST: resolved->index_method = internString("gist"); break;
    case IndexType::BRIN: resolved->index_method = internString("brin"); break;
    case IndexType::BITMAP: resolved->index_method = internString("bitmap"); break;  // ❌ UNREACHABLE
}
```

BITMAP is now in the AST enum and semantic analyzer mapping is active.

### Impact

**User Impact:**
- Cannot create spatial indexes for geographic data (RTREE)
- Cannot create vector indexes for ML/AI applications (HNSW)
- Cannot create optimized indexes for large tables (COLUMNSTORE, LSM)
- Cannot use space-partitioned indexes (SPGIST)
- Cannot use bitmap indexes for low-cardinality columns

**Example Failures (Resolved):**

```sql
-- All of these FAIL with "Unknown index type" even though storage engine supports them:
CREATE INDEX spatial_idx ON locations USING RTREE (geom);
CREATE INDEX vector_idx ON embeddings USING HNSW (embedding);
CREATE INDEX bitmap_idx ON status_codes USING BITMAP (status);
CREATE INDEX columnstore_idx ON analytics USING COLUMNSTORE (timestamp, value);
```

### Resolution Summary

- AST enum, parser, semantic analyzer, and bytecode generator updated for all 11 types.
- Integration tests added for CREATE/INSERT/SELECT across index types.

---

## Non-Critical Findings

### FirebirdSQL Parser: EXCELLENT ✅

**Verdict:** Production-ready, no contamination detected

- 100% Firebird SQL syntax purity
- Complete RDB$/MON$/SEC$ catalog support
- No PostgreSQL/MySQL/V2 features found
- Proper namespace isolation
- Modern Firebird 5.0 features supported

**Recommendation:** APPROVED for production use.

### V2 Parser: Context Sensitivity ✅

**Finding:** V2 parser correctly implements context-sensitive keyword handling

- Gatekeeper model with 175+ contextual keywords
- Reduces reserved word collisions as intended
- Proper use of `matchContextual()` for non-reserved keywords

**Recommendation:** Context sensitivity implementation is correct.

---

## Summary of Required Actions

### CRITICAL (Must Fix Before Production)

1. **ALL PARSERS**: TEMPORARY TABLES implemented end-to-end (DONE)
2. **Firebird Parser**: ON COMMIT storage + execution implemented (DONE)
3. **V2 Parser**: PSQL parsing implemented (DONE)
4. **V2 Parser**: CTE (WITH clause) parsing implemented (DONE)

### HIGH Priority

5. **V2 Parser**: Complete index type support (11 types) (DONE)
6. **V2 Parser**: PG-style extensions documented as intentional (DONE)
7. **PostgreSQL Parser**: Fix remaining executor bytecode format mismatches (DONE)
8. **MySQL Parser**: Fix remaining executor bytecode format mismatches (DONE)

### MEDIUM Priority

9. **All Parsers**: Expand integration tests (parser → executor) (DONE)
10. **MySQL Parser**: Implement remaining CREATE INDEX/VIEW/DROP gaps (DONE)
11. **All Parsers**: Audit and implement parsed-but-not-implemented features (DONE; remaining limits are explicit warnings/errors)
12. **Documentation**: ✅ COMPLETE - comparison matrix, SBLR mapping, and status updates

---

## Detailed Findings Location

Complete audit reports with line-by-line analysis:

- **V2 Parser:** `docs/audit/parsers/V2/SUMMARY.md`
- **FirebirdSQL Parser:** `docs/audit/parsers/FirebirdSQL/SUMMARY.md`
- **PostgreSQL Parser:** `docs/audit/parsers/PostgreSQL/SUMMARY.md`
- **MySQL Parser:** `docs/audit/parsers/MySQL/SUMMARY.md`

Comparison matrix: `docs/audit/parsers/COMPARISON_MATRIX.md`

---

**End of Critical Findings**
**Review Required By:** Lead Developer
**Next Steps:** Prioritize fixes, create GitHub issues, assign owners
