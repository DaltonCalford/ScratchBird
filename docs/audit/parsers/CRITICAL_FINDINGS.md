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
| **V2** | ⚠️ Mixed (approved PG-style extensions) | Parsed-only audit cleanup in progress (Phase 5) | ⚠️ **Conditional** |
| **FirebirdSQL** | ✅ **EXCELLENT** (100% pure) | None | ✅ **YES** |
| **PostgreSQL** | ✅ **EXCELLENT** (100% pure) | Executor format mismatches (partial fixes applied) | ⚠️ **Conditional** |
| **MySQL** | ✅ **GOOD** (99% pure) | Executor format mismatches (partial fixes applied) | ⚠️ **Conditional** |

---

## Phase 0 Verification (2026-01-14) - Outstanding Deferrals

Goal: validate that every parsed feature executes end-to-end (parser -> semantic -> bytecode -> executor) with **no stubs**.
Result: **remaining deferrals exist** and must be resolved before network listener work.

### V2 Parser Pipeline Gaps (native ScratchBird)
- CREATE DOMAIN `WITH` blocks are parsed but rejected (`src/parser/parser_v2.cpp:2039`).
- CREATE INDEX `INCLUDE` parsed, but semantic analyzer rejects (`src/sblr/semantic_analyzer_v2.cpp:3818`). ✅ **RESOLVED**
- CREATE SEQUENCE `OWNED BY` is parsed but ignored (warning) (`src/sblr/semantic_analyzer_v2.cpp:3963`). ✅ **RESOLVED**
- CHECK constraint subqueries parse but are rejected (`src/sblr/semantic_analyzer_v2.cpp:4314`, `4650`).
- ALTER TABLE ONLY parses but is ignored (warning) (`src/sblr/semantic_analyzer_v2.cpp:4988`). ✅ **RESOLVED**
- ALTER TABLE constraint operations parse but are rejected (`src/sblr/semantic_analyzer_v2.cpp:5139`). ✅ **RESOLVED**
- COPY FORMAT/ENCODING options parse but are rejected (`src/sblr/semantic_analyzer_v2.cpp:5908`, `5913`; `src/sblr/executor.cpp:47185`, `47233`). ✅ **RESOLVED** (TEXT/CSV accepted; ENCODING supports UTF8/UTF-8; BINARY remains unsupported in Alpha).
- TRUNCATE CASCADE/RESTART IDENTITY parse but are ignored (warnings) (`src/sblr/bytecode_generator_v2.cpp:2950`, `2953`).
- SIMILAR TO ESCAPE parses but is ignored (warning) (`src/sblr/bytecode_generator_v2.cpp:3905`).
- Aggregation limits (e.g., JOIN/CTE aggregation, SELECT * with aggregation) are parsed but rejected at execution (`src/sblr/executor.cpp:19595`, `19599`, `19638`).

### PostgreSQL Parser Gaps
- Expression indexes / INCLUDE indexes parsed but rejected by bytecode (`src/parser/postgresql/pg_parser_ddl.cpp:1046`, `1049`).
- CREATE TYPE RANGE rejected (`src/parser/postgresql/pg_parser_ddl.cpp:1923`).
- CREATE DOMAIN base type rejected (`src/parser/postgresql/pg_parser_ddl.cpp:2062`).
- Table-level CHECK constraints rejected (`src/parser/postgresql/pg_parser_ddl.cpp:373`).
- DEFAULT values in multi-row INSERT rejected (`src/parser/postgresql/pg_parser_dml.cpp:905`).
- ALTER TABLE DROP CONSTRAINT / ALTER COLUMN SET/DROP DEFAULT/NOT NULL / USING rejected (`src/parser/postgresql/pg_parser_ddl.cpp:2577`, `2594`, `2599`, `2612`).
- TRUNCATE options rejected (`src/parser/postgresql/pg_parser_ddl.cpp:3115`).
- GRANT/REVOKE ON ALL rejected in bytecode (`src/parser/postgresql/pg_parser_misc.cpp:588`, `715`).

### MySQL Parser Gaps
- DEFAULT values in multi-row INSERT/REPLACE rejected (`src/parser/mysql/mysql_parser.cpp:2263`, `2693`).
- ALTER TABLE ADD/DROP INDEX rejected (`src/parser/mysql/mysql_parser.cpp:3266`, `3282`).
- ALTER TABLE CHANGE COLUMN rename / ALTER COLUMN rejected (`src/parser/mysql/mysql_parser.cpp:3318`, `3333`).
- MySQL partition options rejected (`src/parser/mysql/mysql_parser.cpp:3765`).
- Unsupported table options are rejected (explicit errors) (`src/parser/mysql/mysql_parser.cpp:3848`, `3927`).
- LOCK TABLES / UNLOCK TABLES rejected (`src/parser/mysql/mysql_parser.cpp:5488`, `5496`).

### Firebird Parser Gaps
- ALTER/DROP/RECREATE for some object types reject with not-implemented errors
  (`src/parser/firebird/firebird_parser.cpp:1982`, `2068`, `2223`).
- ALTER DATABASE options rejected (`src/parser/firebird/firebird_parser.cpp:2201`).
- ALTER TABLE SET rejected (`src/parser/firebird/firebird_parser.cpp:2423`).

### Cross-cutting Executor Gaps (Parsed Features)
- Unsupported ALTER SCHEMA / ALTER DATABASE action enums in executor
  (`src/sblr/executor.cpp:7743`, `9420`).
- ON CONFLICT unsupported action variants rejected (`src/sblr/executor.cpp:11137`).
- SELECT aggregation limitations (JOIN/CTE/SELECT * restrictions) block parsed queries
  (`src/sblr/executor.cpp:19595`, `19599`, `19638`).

**Status:** These must be resolved or parsing must be removed to eliminate stubs/deferrals before
network listener work begins.

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
**Status:** PARTIALLY RESOLVED - PG CREATE TABLE column count uses uvarint; remaining mismatches still require audit/cleanup.

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
7. **PostgreSQL Parser**: Fix remaining executor bytecode format mismatches (PARTIAL)
8. **MySQL Parser**: Fix remaining executor bytecode format mismatches (PARTIAL)

### MEDIUM Priority

9. **All Parsers**: Expand integration tests (parser → executor) (IN PROGRESS)
10. **MySQL Parser**: Implement remaining CREATE INDEX/VIEW/DROP gaps (VERIFY STATUS)
11. **All Parsers**: Audit and implement parsed-but-not-implemented features (Phase 5 target)
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
