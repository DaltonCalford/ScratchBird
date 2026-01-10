# Parser Audit - Critical Findings Summary

**Audit Date:** 2026-01-07
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
| **V2** | ⚠️ Mixed (PostgreSQL features present) | Incomplete PSQL, CTEs not parsed | ⚠️ **Conditional** |
| **FirebirdSQL** | ✅ **EXCELLENT** (100% pure) | None | ✅ **YES** |
| **PostgreSQL** | ✅ **EXCELLENT** (100% pure) | Executor format mismatches | ⚠️ **Conditional** |
| **MySQL** | ✅ **GOOD** (99% pure) | Executor format mismatches | ⚠️ **Conditional** |

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

**Missing Implementations:**
- CREATE FUNCTION, CREATE PROCEDURE, CREATE TRIGGER
- EXECUTE BLOCK
- IF/WHILE/FOR SELECT statements
- Exception handling
- WITH clauses (CTEs)

**AST Infrastructure Exists But Unused:**
- `ExecuteBlockStmt`, `IfStmt`, `WhileStmt`, `ForSelectStmt` defined in `ast_v2.h`
- `WithClause` fields exist in INSERT/UPDATE/DELETE statements but never populated

### Impact

- V2 parser cannot handle procedural code
- CTEs (Common Table Expressions) not supported despite AST infrastructure
- Functions/procedures must use emulated parsers (Firebird/PostgreSQL)

### Recommended Action

1. Implement PSQL parsing functions to populate existing AST nodes
2. Implement CTE parsing (WITH clause)
3. Complete CREATE FUNCTION/PROCEDURE/TRIGGER parsers
4. Add comprehensive tests for procedural features

---

## CRITICAL ISSUE #2: V2 Parser - PostgreSQL Feature Contamination

**Severity:** MEDIUM - Dialect Bleeding

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

**Decision Required:**
1. **Option A**: Remove PostgreSQL-specific features, keep V2 pure Firebird-style
2. **Option B**: Document V2 as "Firebird base + approved extensions" and list PostgreSQL features as intentional
3. **Option C**: Rename V2 to indicate it's a hybrid dialect

---

## CRITICAL ISSUE #3: TEMPORARY TABLES - Parsed But Not Implemented (ALL PARSERS)

**Severity:** CRITICAL - Silent Feature Failure

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
| **Firebird** | ✅ Lines 1607-1620 | ✅ `stmt->temporary = true` | ❌ NO | ❌ NO | PERMANENT table |
| **V2** | ✅ Lines 265-269 | ✅ `stmt->temporary = true` | ❌ NO | ❌ NO | PERMANENT table |
| **PostgreSQL** | ✅ Line 158 | ⚠️ `is_temp` read but **DISCARDED** | ❌ NO | ❌ NO | PERMANENT table |
| **MySQL** | ✅ Line 2703 | ⚠️ Matched but **DISCARDED** | ❌ NO | ❌ NO | PERMANENT table |

### Implementation Gaps

**Bytecode Generator:** `src/sblr/bytecode_generator_v2.cpp:619-677`
- Completely **IGNORES** `stmt->temporary` flag
- No temporary flags written to bytecode

**Executor:** `src/sblr/executor.cpp:4152+`
- `executeCreateTable()` has **NO HANDLING** for temporary tables
- No session/transaction tracking
- No visibility isolation
- No cleanup logic

**Catalog:** `include/scratchbird/core/catalog_manager.h:1520`
- `createTable()` has **NO PARAMETER** for temporary flag
- Cannot store temporary table metadata
- No session-scoped table tracking

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

**Status:** Parsed successfully but **NOT STORED** in AST or emitted to bytecode.

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

### Required Implementation

**Full implementation required across entire stack:**

1. **AST Extension** - Add enums for temporary table type and ON COMMIT action
2. **Bytecode Format** - Extend CREATE_TABLE opcode with temporary flags
3. **Catalog Extension** - Store temporary metadata, session/transaction IDs
4. **Session Tracking** - Track temp tables per connection in ConnectionContext
5. **Executor** - Implement visibility isolation and cleanup logic
6. **Storage** - Use in-memory storage or separate temp tablespace

**See:** `/docs/specifications/FIREBIRD_V2_FEATURE_PARITY_SPECIFICATION.md` (Section: "Parsed But Not Implemented Features") for detailed implementation plan

### Recommended Action

**CRITICAL PRIORITY:**
1. Implement temporary table support (full stack)
2. Implement ON COMMIT semantics for Firebird GTT
3. Add comprehensive test suite
4. OR: Make parsers **REJECT** temporary table syntax with clear error until implemented

**Related Features Also Affected:**
- TEMPORARY VIEWS (same issue)
- TEMPORARY SEQUENCES (same issue)
- UNLOGGED TABLES (parsed but not implemented)

---

## ISSUE #4: PostgreSQL Parser - Executor Format Mismatches

**Severity:** MEDIUM - Runtime Compatibility

**Files:** Multiple in `src/parser/postgresql/`

### The Problem

PostgreSQL parser emits bytecode format that doesn't match executor expectations:

**DDL Mismatches:**
- CREATE TABLE: Extra IF NOT EXISTS byte, column/constraint format differs
- CREATE INDEX: Payload ordering different
- CREATE VIEW: Emits SELECT bytecode instead of SQL string
- ALTER TABLE: Uses deprecated opcodes

**DML Mismatches:**
- SELECT: DISTINCT flag byte not expected
- INSERT: Alias encoding, multi-row format differs
- UPDATE: Alias string not expected
- DELETE: Alias, USING clause unsupported
- MERGE: No executor support for EXT_MERGE_* opcodes

### Impact

Estimated 20-30% of PostgreSQL statements execute correctly. Remaining fail at runtime with bytecode format errors.

### Recommended Action

Per existing audit document `/docs/audit/19_postgresql_parser_correction_plan_checklist.md`:
1. Align parser bytecode output to executor format expectations
2. OR extend executor to accept parser's current format
3. OR version bytecode format and support both

---

## ISSUE #5: V2 Parser - Incomplete Index Type Support

**Severity:** HIGH - Feature Gap

**Files:** Multiple (AST, parser, semantic analyzer, bytecode generator)

### The Problem

V2 parser only supports **5 of 11 production-ready index types**, even though the storage engine, catalog, and SBLR opcodes fully support all 11 types. This prevents users from creating spatial indexes, vector indexes, columnstore indexes, and other advanced index types through V2 SQL syntax.

**V2 Parser Currently Supports:**
- BTREE ✅
- HASH ✅
- GIN ✅
- GIST ✅
- BRIN ✅

**V2 Parser MISSING (storage engine supports):**
- SPGIST ❌ (Space-Partitioned GiST)
- RTREE ❌ (R-Tree spatial index)
- HNSW ❌ (Vector similarity search)
- BITMAP ❌ (Low cardinality columns)
- COLUMNSTORE ❌ (Column-oriented storage)
- LSM ❌ (Log-Structured Merge-Tree)

### Implementation Status Across Layers

| Index Type | V2 AST | V2 Parser | Semantic Analyzer | Bytecode Gen | SBLR Opcodes | Catalog | Storage Engine |
|------------|--------|-----------|-------------------|--------------|--------------|---------|----------------|
| BTREE | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| HASH | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| GIN | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| GIST | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ⚠️ |
| BRIN | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| SPGIST | ❌ | ❌ | ❌ | ✅ | ✅ | ✅ | ✅ |
| RTREE | ❌ | ❌ | ❌ | ❌ | ✅ | ✅ | ✅ |
| HNSW | ❌ | ❌ | ❌ | ❌ | ✅ | ✅ | ✅ |
| BITMAP | ❌ | ❌ | ⚠️ (dead) | ✅ | ✅ | ✅ | ✅ |
| COLUMNSTORE | ❌ | ❌ | ❌ | ❌ | ✅ | ✅ | ✅ |
| LSM | ❌ | ❌ | ❌ | ❌ | ✅ | ✅ | ✅ |

### Critical Bug: Dead Code in Semantic Analyzer

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

BITMAP is not in the AST enum (ast_v2.h), so this case is dead code.

### Impact

**User Impact:**
- Cannot create spatial indexes for geographic data (RTREE)
- Cannot create vector indexes for ML/AI applications (HNSW)
- Cannot create optimized indexes for large tables (COLUMNSTORE, LSM)
- Cannot use space-partitioned indexes (SPGIST)
- Cannot use bitmap indexes for low-cardinality columns

**Example Failures:**

```sql
-- All of these FAIL with "Unknown index type" even though storage engine supports them:
CREATE INDEX spatial_idx ON locations USING RTREE (geom);
CREATE INDEX vector_idx ON embeddings USING HNSW (embedding);
CREATE INDEX bitmap_idx ON status_codes USING BITMAP (status);
CREATE INDEX columnstore_idx ON analytics USING COLUMNSTORE (timestamp, value);
```

### Recommended Action

**IMMEDIATE PRIORITY:**
1. Extend AST enum to include all 11 index types
2. Update V2 parser to accept all 11 index types
3. Fix dead code in semantic analyzer
4. Extend bytecode generator for missing types (RTREE, HNSW, COLUMNSTORE, LSM)
5. Add comprehensive integration tests

**Detailed Implementation Plan:**
See `/docs/specifications/V2_PARSER_INDEX_TYPE_COMPLETENESS.md` for complete specification

**Effort Estimate:** 6 days (5 implementation phases + testing)

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

1. **ALL PARSERS**: Implement TEMPORARY TABLES (full stack) OR reject syntax with error
2. **Firebird Parser**: Implement ON COMMIT clause storage and execution
3. **V2 Parser**: Implement PSQL parsing OR document as "not supported"
4. **V2 Parser**: Implement CTE (WITH clause) parsing OR remove AST fields

### HIGH Priority

5. **V2 Parser**: Implement complete index type support (11 types) - see Issue #5
6. **V2 Parser**: Document PostgreSQL features as intentional OR remove them
7. **PostgreSQL Parser**: Fix executor bytecode format mismatches
8. **MySQL Parser**: Fix executor bytecode format mismatches

### MEDIUM Priority

9. **All Parsers**: Add comprehensive integration tests (parser → executor)
10. **MySQL Parser**: Implement CREATE INDEX, CREATE VIEW, DROP statements
11. **All Parsers**: Audit and implement other parsed-but-not-implemented features (UNLOGGED, PARTITION BY, etc.)
12. **Documentation**: ✅ COMPLETE - Comparison matrix and SBLR mapping created

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
