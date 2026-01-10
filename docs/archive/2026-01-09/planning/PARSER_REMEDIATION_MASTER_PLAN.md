# Parser Remediation Master Plan - Alpha Release

**Document Version:** 1.0
**Date:** 2026-01-07
**Status:** ACTIVE - Implementation Required
**Target:** Alpha Release Readiness

---

## Executive Summary

This master plan consolidates **all critical parser findings** from the comprehensive audit into a single tracked roadmap. The audit of 4 parsers (~23,000 lines) identified **10 critical issues** requiring remediation before Alpha release.

**Total Effort Estimate:** 36.5-46 days (7-9 weeks)
**Critical Path Issues:** 10 issues
**Alpha Blockers:** ALL 10 issues (NO BYPASS TO BETA)
**Hard Requirement:** 100% parser completion mandatory for Alpha release

---

## Issue Tracking Summary

| Issue ID | Issue | Severity | Priority | Effort | Status | Owner |
|----------|-------|----------|----------|--------|--------|-------|
| **P-001** | TEMPORARY TABLES not implemented (all parsers) | CRITICAL | **ALPHA BLOCKER** | 8-10 days | 🔴 NOT STARTED | TBD |
| **P-002** | V2 Parser - Incomplete Index Type Support | HIGH | **ALPHA BLOCKER** | 6 days | 🔴 NOT STARTED | TBD |
| **P-003** | PostgreSQL ARRAY → VARCHAR Bug | CRITICAL | **ALPHA BLOCKER** | 0.5 days | 🔴 NOT STARTED | TBD |
| **P-004** | MySQL ON DUPLICATE KEY UPDATE Disabled | HIGH | **ALPHA BLOCKER** | 2-3 days | 🔴 NOT STARTED | TBD |
| **P-005** | V2 Parser - PostgreSQL Contamination | MEDIUM | **ALPHA BLOCKER** | 3-5 days | 🔴 NOT STARTED | TBD |
| **P-006** | V2 Parser - Incomplete PSQL (CTEs, Functions) | HIGH | **ALPHA BLOCKER** | 5-7 days | 🔴 NOT STARTED | TBD |
| **P-007** | PostgreSQL Parser - Bytecode Format Mismatches | HIGH | **ALPHA BLOCKER** | 5-7 days | 🔴 NOT STARTED | TBD |
| **P-008** | MySQL Parser - Bytecode Format Mismatches | HIGH | **ALPHA BLOCKER** | 3-5 days | 🔴 NOT STARTED | TBD |
| **P-009** | Firebird Parser - ON COMMIT Clause Discarded | MEDIUM | **ALPHA BLOCKER** | 2-3 days | 🔴 NOT STARTED | TBD |
| **P-010** | Firebird Parser - Context Variable Keywords Bug | LOW | **ALPHA BLOCKER** | 0.5 days | 🔴 NOT STARTED | TBD |

**Totals:**
- **ALL ISSUES ARE ALPHA BLOCKERS:** 10 issues, 36.5-46 days
- **NO DEFERRALS TO BETA ALLOWED**

**CRITICAL PATH:** 36.5-46 days total if done sequentially
**PARALLEL EXECUTION:** Can reduce to ~7-9 weeks with 2-3 developers
**ABSOLUTE REQUIREMENT:** 100% completion before Alpha release

---

## Issue Details and Remediation Plans

### 🔴 P-001: TEMPORARY TABLES Not Implemented (ALL PARSERS)

**Severity:** CRITICAL - Silent Feature Failure
**Priority:** ALPHA BLOCKER
**Affected Parsers:** All 4 (V2, Firebird, PostgreSQL, MySQL)
**Effort:** 8-10 days

#### Problem Statement

All 4 parsers accept TEMPORARY table syntax but silently create **permanent tables**. This is a critical data integrity and security issue.

```sql
-- ALL OF THESE CREATE PERMANENT TABLES:
CREATE TEMPORARY TABLE test (id INT);           -- V2/Firebird
CREATE TEMP TABLE test (id INT);                -- PostgreSQL
CREATE TEMPORARY TABLE test (id INT);           -- MySQL
CREATE GLOBAL TEMPORARY TABLE test (id INT)     -- Firebird
    ON COMMIT DELETE ROWS;
```

**Impact:**
- Users expect session-scoped tables
- No automatic cleanup
- Cross-session visibility leakage (security issue)
- Breaks PostgreSQL/MySQL/Firebird compatibility

#### Implementation Layers

| Layer | Status | Work Required |
|-------|--------|---------------|
| AST Extension | ❌ | Add TemporaryTableType and OnCommitAction enums |
| Parsers (all 4) | ⚠️ | Already parse syntax, need to store in AST |
| Bytecode Format | ❌ | Extend CREATE_TABLE opcode with temporary flags |
| Catalog | ❌ | Store temporary metadata, session/transaction IDs |
| ConnectionContext | ❌ | Track temp tables per connection |
| Executor | ❌ | Implement visibility isolation and cleanup |
| Storage | ❌ | Use in-memory storage or separate temp tablespace |

#### Detailed Specification

See: `/docs/specifications/FIREBIRD_V2_FEATURE_PARITY_SPECIFICATION.md` (Section: "Parsed But Not Implemented Features")

#### Implementation Plan

**Phase 1: AST & Type Definitions (1 day)**
- Define TemporaryTableType enum (NONE, SESSION, TRANSACTION, GLOBAL)
- Define OnCommitAction enum (NONE, DELETE_ROWS, PRESERVE_ROWS, DROP)
- Update CreateTableStmt AST nodes in all 4 parsers

**Phase 2: Parser Updates (2 days)**
- V2 parser: Store temporary flag and type
- Firebird parser: Store temporary flag, type, and ON COMMIT action
- PostgreSQL parser: Store temporary flag and ON COMMIT action
- MySQL parser: Store temporary flag

**Phase 3: Bytecode & Catalog (2 days)**
- Extend CREATE_TABLE bytecode format
- Update bytecode generator to emit temporary flags
- Extend catalog schema to store temporary metadata
- Add session_id/transaction_id tracking

**Phase 4: Executor & Storage (2 days)**
- Implement temporary table visibility rules
- Implement ON COMMIT DELETE ROWS (truncate on commit)
- Implement ON COMMIT PRESERVE ROWS (keep on commit)
- Implement ON COMMIT DROP (drop on commit - PostgreSQL only)
- Implement session/transaction cleanup

**Phase 5: Testing (1-2 days)**
- Session isolation tests
- ON COMMIT behavior tests
- Restart persistence tests (temp tables should NOT persist)
- Concurrent session tests

#### Success Criteria

- [ ] Temporary tables are session/transaction-scoped
- [ ] ON COMMIT DELETE ROWS truncates on commit
- [ ] ON COMMIT PRESERVE ROWS keeps rows on commit
- [ ] ON COMMIT DROP drops table on commit (PostgreSQL)
- [ ] Temporary tables cleaned up on disconnect
- [ ] Temporary tables do NOT persist after restart
- [ ] No cross-session visibility

#### Alternative: Reject Syntax Until Implemented

If full implementation is not feasible before Alpha:

**Effort:** 0.5 days (4 hours)
**Impact:** Breaking change for any existing code using TEMPORARY

```cpp
// In each parser's parseCreateTable():
if (temporary) {
    error("TEMPORARY tables are not yet supported in this release. "
          "This feature is planned for a future release. "
          "For now, please use regular tables with manual cleanup.");
}
```

**Recommendation:** Implement fully OR reject with error. Silent failure is unacceptable.

---

### 🔴 P-002: V2 Parser - Incomplete Index Type Support

**Severity:** HIGH - Feature Gap
**Priority:** ALPHA BLOCKER
**Affected Parsers:** V2 only
**Effort:** 6 days

#### Problem Statement

V2 parser only supports 5 of 11 production-ready index types, preventing users from creating spatial indexes, vector indexes, and other advanced index types.

**Missing from V2:**
- SPGIST (Space-Partitioned GiST)
- RTREE (R-Tree spatial index)
- HNSW (Vector similarity search)
- BITMAP (Low cardinality columns)
- COLUMNSTORE (Column-oriented storage)
- LSM (Log-Structured Merge-Tree)

**Dead Code Bug:** Semantic analyzer has unreachable BITMAP case

#### Detailed Specification

See: `/docs/specifications/V2_PARSER_INDEX_TYPE_COMPLETENESS.md`

#### Implementation Plan

See: `/docs/archive/2026-01-09/planning/V2_INDEX_TYPE_IMPLEMENTATION_ROADMAP.md`

**Timeline:**
- Day 1: AST + Semantic Analyzer
- Day 2: Parser Implementation
- Day 3: Bytecode Generator
- Days 4-5: Comprehensive Testing
- Day 6: Documentation

#### Success Criteria

- [ ] All 11 index types parseable in V2
- [ ] Dead code removed
- [ ] All 11 types map correctly through entire stack
- [ ] Test suite covers all 11 types
- [ ] Documentation complete

---

### 🔴 P-003: PostgreSQL ARRAY Type → VARCHAR Bug

**Severity:** CRITICAL - Data Type Corruption
**Priority:** ALPHA BLOCKER
**Affected Parsers:** PostgreSQL only
**Effort:** 0.5 days (4 hours)

#### Problem Statement

PostgreSQL ARRAY types fall through to default case and are stored as VARCHAR, causing data corruption.

**File:** `src/parser/postgresql/pg_parser.cpp:508-551`

```cpp
sblr::Opcode Parser::typeToOpcode(PgDataType::Kind kind) {
    switch (kind) {
        case PgDataType::Kind::SMALLINT:
        // ... many cases
        case PgDataType::Kind::JSON:
        case PgDataType::Kind::JSONB:
            return sblr::Opcode::TYPE_JSON;
        default:
            return sblr::Opcode::TYPE_VARCHAR;  // ❌ BUG: ARRAY falls here
    }
}
```

#### Fix Required

```cpp
case PgDataType::Kind::ARRAY:  // ✅ ADD THIS BEFORE DEFAULT
    return sblr::Opcode::TYPE_ARRAY;
default:
    return sblr::Opcode::TYPE_VARCHAR;
```

#### Implementation Plan

**Phase 1: Fix Type Mapping (1 hour)**
- Add ARRAY case to typeToOpcode()
- Verify TYPE_ARRAY opcode exists in opcodes.h

**Phase 2: Testing (2 hours)**
- Test: `CREATE TABLE test (arr INTEGER[])`
- Verify catalog stores TYPE_ARRAY, not TYPE_VARCHAR
- Test INSERT, SELECT with array data
- Test array operations (append, access, etc.)

**Phase 3: Regression Testing (1 hour)**
- Ensure other types still map correctly
- Run full PostgreSQL parser test suite

#### Success Criteria

- [ ] ARRAY types stored as TYPE_ARRAY
- [ ] Array operations work correctly
- [ ] No regression in other type mappings
- [ ] Test coverage for all array types (INTEGER[], TEXT[], etc.)

#### Detailed Specification

See: `/docs/specifications/POSTGRESQL_PARSER_IMPLEMENTATION_GAPS.md` (Section: "CRITICAL BUG: ARRAY Type Mapping")

---

### 🔴 P-004: MySQL ON DUPLICATE KEY UPDATE Disabled

**Severity:** HIGH - Feature Not Working
**Priority:** ALPHA BLOCKER
**Affected Parsers:** MySQL only
**Effort:** 2-3 days

#### Problem Statement

MySQL parser parses ON DUPLICATE KEY UPDATE syntax but explicitly disables bytecode emission, making the feature non-functional.

**File:** `src/parser/mysql/mysql_parser.cpp:2130-2144`

```cpp
if (matchKeyword(TokenType::KW_ON)) {
    consumeKeyword(TokenType::KW_DUPLICATE, "Expected DUPLICATE");
    consumeKeyword(TokenType::KW_KEY, "Expected KEY");
    consumeKeyword(TokenType::KW_UPDATE, "Expected UPDATE");

    bool prev_emit = emit_enabled_;
    emit_enabled_ = false;  // ❌ DISABLE EMISSION
    // ... parse update list
    emit_enabled_ = prev_emit;  // ❌ No bytecode emitted
}
```

#### Remediation Options

**Option A: Remap to MERGE Statement (RECOMMENDED)**
- Effort: 2-3 days
- Transform ON DUPLICATE KEY UPDATE to equivalent MERGE
- No new SBLR opcodes required

**Option B: Enable Bytecode Emission**
- Effort: 1-2 days
- Re-enable emit_enabled_
- Extend INSERT bytecode format
- Update executor to handle ON DUPLICATE

#### Implementation Plan (Option A: Remap to MERGE)

**Phase 1: Transform Logic (1 day)**
```sql
-- MySQL syntax:
INSERT INTO users (id, name) VALUES (1, 'Alice')
ON DUPLICATE KEY UPDATE name = VALUES(name);

-- Transform to MERGE:
MERGE INTO users u
USING (SELECT 1 AS id, 'Alice' AS name) src
ON u.id = src.id
WHEN MATCHED THEN
    UPDATE SET name = src.name
WHEN NOT MATCHED THEN
    INSERT (id, name) VALUES (src.id, src.name);
```

**Phase 2: AST Transformation (1 day)**
- Detect ON DUPLICATE KEY UPDATE during parsing
- Generate MERGE AST instead of INSERT AST
- Map VALUES(col) to source table references

**Phase 3: Testing (0.5-1 day)**
- Test basic upsert operations
- Test multi-row inserts with ON DUPLICATE
- Test complex UPDATE expressions
- Verify unique constraint violations handled correctly

#### Success Criteria

- [ ] ON DUPLICATE KEY UPDATE works correctly
- [ ] Upsert behavior matches MySQL semantics
- [ ] Multi-row inserts handled
- [ ] Test coverage complete

#### Detailed Specification

See: `/docs/specifications/PARSER_REMAPPING_AND_IMPLEMENTATION_STRATEGY.md` (Section: "CRITICAL - MySQL ON DUPLICATE KEY UPDATE")

---

### 🟡 P-005: V2 Parser - PostgreSQL Contamination

**Severity:** MEDIUM - Dialect Bleeding
**Priority:** HIGH (Should fix before Alpha)
**Affected Parsers:** V2 only
**Effort:** 3-5 days

#### Problem Statement

V2 parser contains PostgreSQL-specific syntax that conflicts with its Firebird-style mandate:
- INSERT ... ON CONFLICT (PostgreSQL 9.5+)
- UPDATE ... FROM clause (PostgreSQL extension)
- DELETE ... USING clause (PostgreSQL-specific)
- DROP ... CASCADE (different semantics)

**Specification Violation:**
> "The V2 parser should follow the FirebirdSQL standard in style/formatting."

#### Remediation Options

**Option A: Remove PostgreSQL Features (Breaking Change)**
- Effort: 3-4 days
- Replace with Firebird equivalents
- Deprecation period: 12-24 months

**Option B: Document as Intentional Extensions**
- Effort: 1-2 days
- Add compatibility mode flag
- Document differences from pure Firebird

#### Implementation Plan (Option A: RECOMMENDED)

**Phase 1: Replace ON CONFLICT (1 day)**
```sql
-- PostgreSQL style (REMOVE):
INSERT INTO users VALUES (1, 'Alice') ON CONFLICT (id) DO UPDATE SET name = EXCLUDED.name;

-- Firebird style (ADD):
UPDATE OR INSERT INTO users VALUES (1, 'Alice') MATCHING (id);
-- OR
MERGE INTO users ...
```

**Phase 2: Replace UPDATE...FROM (1 day)**
```sql
-- PostgreSQL style (REMOVE):
UPDATE orders SET status = 'shipped' FROM shipments WHERE orders.id = shipments.order_id;

-- Firebird style (ADD):
UPDATE orders SET status = 'shipped' WHERE id IN (SELECT order_id FROM shipments);
-- OR
MERGE INTO orders ...
```

**Phase 3: Replace DELETE...USING (1 day)**
```sql
-- PostgreSQL style (REMOVE):
DELETE FROM orders USING shipments WHERE orders.id = shipments.order_id;

-- Firebird style (ADD):
DELETE FROM orders WHERE id IN (SELECT order_id FROM shipments);
```

**Phase 4: Testing & Migration (1-2 days)**
- Update all tests using PostgreSQL syntax
- Provide migration guide for users
- Add deprecation warnings (if phased approach)

#### Success Criteria

- [ ] V2 parser follows Firebird syntax style
- [ ] All Firebird-style alternatives work
- [ ] Migration path documented
- [ ] No breaking changes for Firebird-compatible code

#### Detailed Specification

See: `/docs/specifications/V2_PARSER_FIREBIRD_ALIGNMENT_SPECIFICATION.md`

---

### 🟡 P-006: V2 Parser - Incomplete PSQL Implementation

**Severity:** HIGH - Feature Gap
**Priority:** HIGH (Should fix before Alpha)
**Affected Parsers:** V2 only
**Effort:** 5-7 days

#### Problem Statement

V2 parser defines AST nodes for procedural SQL but never parses them. Missing implementations:
- CREATE FUNCTION, CREATE PROCEDURE, CREATE TRIGGER
- EXECUTE BLOCK
- IF/WHILE/FOR SELECT statements
- Exception handling
- WITH clauses (CTEs)

**File:** `src/parser/parser_v2.cpp:299-302`

```cpp
// TODO: Add more CREATE types
// if (matchContextual("FUNCTION"))   return parseCreateFunction(or_replace);
// if (matchContextual("PROCEDURE"))  return parseCreateProcedure(or_replace);
// if (matchContextual("TRIGGER"))    return parseCreateTrigger();
```

#### Remediation Options

**Option A: Implement PSQL Parsing (RECOMMENDED)**
- Effort: 5-7 days
- Full procedural SQL support
- CTE (WITH clause) support
- Alpha-complete feature set

**Option B: Document as "Not Supported"**
- Effort: 0.5 days
- Remove AST nodes or mark as reserved
- Defer to Beta release

#### Implementation Plan (Option A: RECOMMENDED)

**Phase 1: CTE (WITH Clause) - HIGHEST PRIORITY (2 days)**

CTEs are widely used and have existing AST infrastructure:

```cpp
// V2 parser already has WithClause fields in INSERT/UPDATE/DELETE
// Just need to implement parseWithClause()

WithClause* Parser::parseWithClause() {
    consume(TokenType::KW_WITH, "Expected WITH");

    bool recursive = false;
    if (matchKeyword(TokenType::KW_RECURSIVE)) {
        recursive = true;
    }

    std::vector<CommonTableExpr*> ctes;
    do {
        auto* cte = arena_.create<CommonTableExpr>();
        cte->name = parseIdentifier();

        // Optional column list
        if (match(TokenType::LEFT_PAREN)) {
            // Parse column names
            consume(TokenType::RIGHT_PAREN);
        }

        consume(TokenType::KW_AS, "Expected AS");
        consume(TokenType::LEFT_PAREN, "Expected (");
        cte->query = parseSelect();
        consume(TokenType::RIGHT_PAREN, "Expected )");

        ctes.push_back(cte);
    } while (match(TokenType::COMMA));

    auto* with_clause = arena_.create<WithClause>();
    with_clause->recursive = recursive;
    with_clause->ctes = std::move(ctes);
    return with_clause;
}
```

**Phase 2: CREATE FUNCTION (1.5 days)**
- Parse function signature (name, parameters, return type)
- Parse function body (SQL or procedural)
- Support RETURNS clause
- Support LANGUAGE SQL / LANGUAGE PLPGSQL

**Phase 3: CREATE PROCEDURE (1.5 days)**
- Similar to function but no RETURNS clause
- Support IN/OUT/INOUT parameters
- Parse procedure body

**Phase 4: CREATE TRIGGER (1 day)**
- Parse trigger timing (BEFORE/AFTER)
- Parse trigger events (INSERT/UPDATE/DELETE)
- Parse FOR EACH ROW / FOR EACH STATEMENT
- Parse WHEN condition
- Parse trigger body

**Phase 5: EXECUTE BLOCK (1 day)**
- Parse variable declarations
- Parse BEGIN...END block
- Support anonymous procedures

**Phase 6: Testing (1 day)**
- CTE tests (recursive and non-recursive)
- Function creation and execution tests
- Procedure tests
- Trigger tests

#### Success Criteria

- [ ] CTE (WITH clause) fully functional
- [ ] CREATE FUNCTION works
- [ ] CREATE PROCEDURE works
- [ ] CREATE TRIGGER works
- [ ] EXECUTE BLOCK works
- [ ] Test coverage complete

---

### 🟡 P-007: PostgreSQL Parser - Bytecode Format Mismatches

**Severity:** HIGH - Runtime Compatibility
**Priority:** HIGH (Should fix before Alpha)
**Affected Parsers:** PostgreSQL only
**Effort:** 5-7 days
**Estimated Success Rate:** 20-30% of statements currently work

#### Problem Statement

PostgreSQL parser emits bytecode format that doesn't match executor expectations, causing runtime failures.

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

#### Implementation Plan

**Phase 1: Audit Executor Expectations (1 day)**
- Document expected bytecode format for each operation
- Compare with PostgreSQL parser output
- Create compatibility matrix

**Phase 2: DDL Fixes (2 days)**
- Fix CREATE TABLE bytecode format
- Fix CREATE INDEX bytecode format
- Fix CREATE VIEW (emit SQL string, not SELECT bytecode)
- Fix ALTER TABLE (use current opcodes)

**Phase 3: DML Fixes (2 days)**
- Fix SELECT (remove DISTINCT flag byte)
- Fix INSERT (align alias encoding, multi-row format)
- Fix UPDATE (remove alias string)
- Fix DELETE (remove alias, handle USING in executor or remap)

**Phase 4: Testing (1-2 days)**
- Test each DDL statement type
- Test each DML statement type
- Integration tests (parser → executor)
- Verify 100% success rate

#### Success Criteria

- [ ] 100% of PostgreSQL DDL statements execute correctly
- [ ] 100% of PostgreSQL DML statements execute correctly
- [ ] No bytecode format errors
- [ ] Full integration test coverage

#### Detailed Specification

See: `/docs/audit/parsers/PostgreSQL/SUMMARY.md` (Section: "Executor Format Mismatches")

---

### 🟡 P-008: MySQL Parser - Bytecode Format Mismatches

**Severity:** HIGH - Runtime Compatibility
**Priority:** HIGH (Should fix before Alpha)
**Affected Parsers:** MySQL only
**Effort:** 3-5 days

#### Problem Statement

Similar to PostgreSQL parser, MySQL parser has bytecode format mismatches, plus missing implementations for CREATE INDEX and CREATE VIEW.

**Missing Implementations:**
- CREATE INDEX (stub only)
- CREATE VIEW (stub only)
- DROP statements (stubs)

**Bytecode Mismatches:**
- INSERT: Multi-row format differs
- UPDATE: Join syntax unsupported
- Column type modifiers (UNSIGNED, ZEROFILL) parsed but not emitted

#### Implementation Plan

**Phase 1: Implement CREATE INDEX (1 day)**
- Emit CREATE_INDEX opcode
- Map MySQL index types to ScratchBird types
- Support index options (UNIQUE, FULLTEXT, SPATIAL)

**Phase 2: Implement CREATE VIEW (1 day)**
- Emit CREATE_VIEW opcode
- Store view SQL definition
- Support OR REPLACE

**Phase 3: Fix Bytecode Formats (1-2 days)**
- Align INSERT multi-row format with executor
- Fix UPDATE join syntax or document as unsupported
- Either emit UNSIGNED/ZEROFILL or document as ignored

**Phase 4: Testing (1 day)**
- Test CREATE INDEX with all types
- Test CREATE VIEW
- Test INSERT multi-row
- Integration tests

#### Success Criteria

- [ ] CREATE INDEX works for all MySQL index types
- [ ] CREATE VIEW works
- [ ] INSERT multi-row works
- [ ] Column modifiers handled correctly (emit or document)
- [ ] Full test coverage

#### Detailed Specification

See: `/docs/specifications/MYSQL_PARSER_IMPLEMENTATION_GAPS.md`

---

### 🟢 P-009: Firebird Parser - ON COMMIT Clause Discarded

**Severity:** MEDIUM - Feature Not Working
**Priority:** MEDIUM (Can defer to Beta)
**Affected Parsers:** Firebird only
**Effort:** 2-3 days
**Note:** Depends on P-001 (TEMPORARY TABLES)

#### Problem Statement

Firebird parser correctly parses ON COMMIT clause for global temporary tables but discards it without storing in AST or emitting to bytecode.

**File:** `src/parser/firebird/firebird_parser.cpp:1943-1952`

```cpp
if (temporary && matchKeyword(TokenType::KW_ON)) {
    consume(TokenType::KW_COMMIT, "Expected COMMIT after ON");
    if (matchKeyword(TokenType::KW_DELETE)) {
        // ON COMMIT DELETE ROWS (default for Firebird)
    } else if (matchKeyword(TokenType::KW_PRESERVE)) {
        // ON COMMIT PRESERVE ROWS
    }
    matchKeyword(TokenType::KW_ROWS);  // Optional ROWS keyword
}
// ❌ Parsed but NOT STORED in AST or emitted to bytecode
```

#### Implementation Plan

**Prerequisite:** Complete P-001 (TEMPORARY TABLES implementation)

**Phase 1: Store in AST (0.5 days)**
- Add OnCommitAction field to CreateTableStmt
- Store DELETE ROWS or PRESERVE ROWS in AST

**Phase 2: Emit to Bytecode (0.5 days)**
- Extend bytecode format (part of P-001)
- Emit ON COMMIT action

**Phase 3: Executor (1 day)**
- Already implemented in P-001
- Verify Firebird-specific semantics

**Phase 4: Testing (0.5-1 day)**
- Test ON COMMIT DELETE ROWS
- Test ON COMMIT PRESERVE ROWS
- Test default behavior (DELETE ROWS)

#### Success Criteria

- [ ] ON COMMIT clause stored in AST
- [ ] ON COMMIT action emitted in bytecode
- [ ] Executor handles Firebird GTT semantics correctly
- [ ] Test coverage complete

---

### 🟢 P-010: Firebird Parser - Context Variable Keywords Bug

**Severity:** LOW - Feature Not Working
**Priority:** MEDIUM (Should fix)
**Affected Parsers:** Firebird only
**Effort:** 0.5 days (4 hours)

#### Problem Statement

Firebird context variable keywords (GEN_ID, GEN_UUID, RDB$GET_CONTEXT, RDB$SET_CONTEXT) are defined in lexer but not in isNonReservedKeyword() list, preventing them from parsing correctly.

**File:** `src/parser/firebird/firebird_parser.cpp:210-266`

#### Fix Required

```cpp
// Add to isNonReservedKeyword():
case TokenType::KW_GEN_ID:
case TokenType::KW_GEN_UUID:
case TokenType::KW_RDB_GET_CONTEXT:
case TokenType::KW_RDB_SET_CONTEXT:
    return true;
```

#### Implementation Plan

**Phase 1: Fix (1 hour)**
- Add 4 keywords to isNonReservedKeyword()

**Phase 2: Testing (2 hours)**
- Test: `SELECT GEN_ID(gen_name, 1) FROM RDB$DATABASE`
- Test: `SELECT GEN_UUID() FROM RDB$DATABASE`
- Test: `SELECT RDB$GET_CONTEXT('USER_SESSION', 'var') FROM RDB$DATABASE`
- Test: `SELECT RDB$SET_CONTEXT('USER_SESSION', 'var', 'value') FROM RDB$DATABASE`

**Phase 3: Regression (1 hour)**
- Ensure no conflicts with other keywords
- Run full Firebird parser test suite

#### Success Criteria

- [ ] GEN_ID() parses correctly
- [ ] GEN_UUID() parses correctly
- [ ] RDB$GET_CONTEXT() parses correctly
- [ ] RDB$SET_CONTEXT() parses correctly
- [ ] No regression in other parsing

---

## Implementation Roadmap - ALL PHASES REQUIRED FOR ALPHA

### ⚠️ CRITICAL REQUIREMENT: 100% COMPLETION MANDATORY

**All 10 issues MUST be resolved before Alpha release. No deferrals to Beta allowed.**

### Full Sequential Implementation Timeline: 36.5-46 days

```
WEEK 1-2: TEMPORARY TABLES (P-001) - 8-10 days [FOUNDATION]
  ├─ Day 1: AST & Type Definitions
  ├─ Day 2-3: Parser Updates (all 4 parsers)
  ├─ Day 4-5: Bytecode & Catalog
  ├─ Day 6-7: Executor & Storage
  └─ Day 8-10: Testing
  NOTE: P-009 depends on this

WEEK 3: V2 PARSER FIXES (P-002, P-003) - 6.5 days
  ├─ Day 1-6: V2 Index Types (P-002)
  └─ Day 6 (4 hours): PostgreSQL ARRAY Fix (P-003)

WEEK 4: MYSQL FIXES (P-004, P-008) - 5-8 days
  ├─ Day 1-3: ON DUPLICATE KEY (P-004)
  └─ Day 4-8: Bytecode Format Fixes (P-008)

WEEK 5-6: V2 IMPROVEMENTS (P-005, P-006) - 8-12 days
  ├─ Day 1-5: PostgreSQL Contamination (P-005)
  └─ Day 6-12: PSQL Implementation (P-006)
    ├─ MUST include: CTE (WITH clause)
    ├─ MUST include: CREATE FUNCTION
    ├─ MUST include: CREATE PROCEDURE
    └─ MUST include: CREATE TRIGGER

WEEK 7-8: POSTGRESQL BYTECODE (P-007) - 5-7 days
  ├─ Day 1: Audit Executor Expectations
  ├─ Day 2-3: DDL Fixes
  ├─ Day 4-5: DML Fixes
  └─ Day 6-7: Testing
  TARGET: 100% statement success rate (not 80%)

WEEK 8-9: FIREBIRD POLISH (P-009, P-010) - 2.5-3.5 days
  ├─ Day 1-3: ON COMMIT Implementation (P-009)
  └─ Day 3 (4 hours): Context Keywords Fix (P-010)

WEEK 9: INTEGRATION & VALIDATION - 2-3 days
  ├─ Full regression testing (all 4 parsers)
  ├─ Cross-parser integration tests
  ├─ Performance validation
  └─ Documentation final review
```

**Total Sequential:** 36.5-46 days (7.3-9.2 weeks)

---

## Resource Allocation - 100% Completion Required

### ⚠️ UPDATED TIMELINE: ALL 10 ISSUES MUST BE COMPLETE

### Single Developer Timeline (NOT RECOMMENDED)

**Sequential implementation - ALL 10 issues:**
- Weeks 1-2: P-001 TEMPORARY TABLES - 8-10 days
- Week 3: P-002, P-003 - 6.5 days
- Week 4: P-004, P-008 start - 5-8 days
- Weeks 5-6: P-005, P-006 - 8-12 days
- Weeks 7-8: P-007 - 5-7 days
- Week 8-9: P-009, P-010 - 2.5-3.5 days
- Week 9: Integration - 2-3 days

**Total: 37-46 days (7.5-9.5 weeks)**
**Risk: VERY HIGH - single point of failure**
**Recommendation: NOT VIABLE FOR ALPHA**

---

### Two Developer Timeline (MINIMUM RECOMMENDED)

**Parallel tracks - ALL 10 issues:**

**Developer 1: Foundation & V2 Parser (23-32 days)**
- Week 1-2: P-001 TEMPORARY TABLES - 8-10 days
- Week 3: P-002 V2 Index Types - 6 days
- Week 4-6: P-006 V2 PSQL (FULL) - 5-7 days
  - CTE, Functions, Procedures, Triggers (ALL required)
- Week 6-7: P-005 V2 PostgreSQL Contamination - 3-5 days
- Week 7: P-010 Firebird Context Keywords - 0.5 days

**Developer 2: Emulated Parser Fixes (18.5-22 days)**
- Week 1: P-003 PostgreSQL ARRAY Fix - 0.5 days
- Week 1-2: P-004 MySQL ON DUPLICATE KEY - 2-3 days
- Week 2-4: P-007 PostgreSQL Bytecode (100%) - 5-7 days
- Week 4-6: P-008 MySQL Bytecode - 3-5 days
- Week 6-7: Assist with P-005 or P-006

**Both Developers:**
- Week 7-8: P-009 Firebird ON COMMIT - 2-3 days (depends on P-001)
- Week 8: Integration testing - 2-3 days

**Total: 7-8 weeks**
**Risk: MEDIUM - workload imbalance**
**Recommendation: MINIMUM VIABLE for Alpha**

---

### Three Developer Timeline (STRONGLY RECOMMENDED)

**Balanced parallel tracks - ALL 10 issues:**

**Developer 1: Foundation (Critical Path) (13.5-16.5 days)**
- Week 1-2: P-001 TEMPORARY TABLES - 8-10 days
- Week 3: P-002 V2 Index Types - 6 days
- Week 4: Integration support - 2-3 days

**Developer 2: V2 Parser & Firebird (14-17.5 days)**
- Week 1-2: P-006 PSQL Implementation (FULL) - 5-7 days
  - CTE, Functions, Procedures, Triggers (ALL)
- Week 3-4: P-005 PostgreSQL Contamination - 3-5 days
- Week 4-5: P-009 Firebird ON COMMIT - 2-3 days
- Week 5: P-010 Firebird Context Keywords - 0.5 days

**Developer 3: Emulated Parser Fixes (11-17 days)**
- Week 1: P-003 PostgreSQL ARRAY Fix - 0.5 days
- Week 1-2: P-004 MySQL ON DUPLICATE KEY - 2-3 days
- Week 2-4: P-007 PostgreSQL Bytecode - 5-7 days
- Week 4-5: P-008 MySQL Bytecode - 3-5 days

**All Developers:**
- Week 5-6: Integration testing & validation - 2-3 days

**Total: 5-6 weeks**
**Risk: LOW - balanced workload**
**Recommendation: OPTIMAL for Alpha**

---

### Four Developer Timeline (ACCELERATED)

**Maximum parallelization - ALL 10 issues:**

**Developer 1: TEMPORARY TABLES**
- Week 1-2: P-001 only - 8-10 days
- Week 3+: Integration support

**Developer 2: V2 Parser**
- Week 1-2: P-002 Index Types - 6 days
- Week 2-4: P-006 PSQL (FULL) - 5-7 days
- Week 4-5: P-005 PostgreSQL Contamination - 3-5 days

**Developer 3: PostgreSQL Parser**
- Week 1: P-003 ARRAY Fix - 0.5 days
- Week 1-4: P-007 Bytecode Fixes - 5-7 days
- Week 4+: Support other tracks

**Developer 4: MySQL & Firebird**
- Week 1-2: P-004 ON DUPLICATE KEY - 2-3 days
- Week 2-4: P-008 MySQL Bytecode - 3-5 days
- Week 4-5: P-009 Firebird ON COMMIT - 2-3 days
- Week 5: P-010 Firebird Context Keywords - 0.5 days

**All Developers:**
- Week 5: Integration testing - 2-3 days

**Total: 4-5 weeks**
**Risk: VERY LOW**
**Recommendation: BEST if resources available**

---

## Tracking Mechanism

### Status Tracking

**Status Codes:**
- 🔴 NOT STARTED - Issue identified, not yet started
- 🟡 IN PROGRESS - Active development
- 🟢 TESTING - Implementation complete, in testing
- ✅ COMPLETE - Testing passed, merged
- ⚠️ BLOCKED - Cannot proceed (waiting on dependency)
- 🚫 DEFERRED - Intentionally delayed to later release

### Progress Reporting

**Weekly Status Report Template:**

```markdown
# Parser Remediation - Week N Status

## Completed This Week
- [P-XXX] Issue name - ✅ COMPLETE
  - All success criteria met
  - Tests passing
  - Documentation updated

## In Progress
- [P-XXX] Issue name - 🟡 IN PROGRESS (60% complete)
  - Phase 1: ✅ Complete
  - Phase 2: 🟡 In Progress
  - Phase 3: 🔴 Not Started
  - Blockers: None
  - ETA: 3 days

## Blocked
- [P-XXX] Issue name - ⚠️ BLOCKED
  - Waiting on: P-001 completion
  - Expected unblock: Next week

## Upcoming Next Week
- [P-XXX] Issue name - starting Monday
- [P-XXX] Issue name - starting Wednesday

## Risks/Concerns
- List any risks or concerns

## Overall Progress
- Alpha Blockers: X/4 complete (Y%)
- High Priority: X/4 complete (Y%)
- Medium Priority: X/2 complete (Y%)
```

### Issue Tracker Integration

**Recommended:** Create GitHub issues for each P-XXX item

```bash
# Example GitHub issue creation
gh issue create --title "P-001: TEMPORARY TABLES Not Implemented" \
  --body "See: /docs/archive/2026-01-09/planning/PARSER_REMEDIATION_MASTER_PLAN.md#p-001" \
  --label "parser,critical,alpha-blocker" \
  --milestone "Alpha Release"

gh issue create --title "P-002: V2 Parser - Incomplete Index Type Support" \
  --body "See: /docs/archive/2026-01-09/planning/PARSER_REMEDIATION_MASTER_PLAN.md#p-002" \
  --label "parser,v2,alpha-blocker" \
  --milestone "Alpha Release"

# ... repeat for all 10 issues
```

### Burndown Chart

Track remaining effort weekly:

| Week | Alpha Blockers Remaining | High Priority Remaining | Total Days Remaining |
|------|-------------------------|-------------------------|----------------------|
| 1 | 17-22.5 days | 16-24 days | 35.5-50 days |
| 2 | 12-17.5 days | 16-24 days | 30.5-45 days |
| ... | ... | ... | ... |

---

## Risk Management

### High-Risk Issues

**P-001 (TEMPORARY TABLES) - Highest Risk:**
- **Risk:** Complex cross-cutting change affecting all 4 parsers + executor + catalog
- **Mitigation:**
  - Implement incrementally (start with V2, then extend to others)
  - Extensive testing at each layer
  - Consider "reject syntax" fallback if timeline slips
- **Contingency:** Reject TEMPORARY syntax with error message if not complete by Alpha

**P-006 (PSQL Implementation) - Medium Risk:**
- **Risk:** Large scope, many sub-features
- **Mitigation:**
  - Prioritize CTE implementation (most requested)
  - Defer EXECUTE BLOCK to Beta if needed
- **Contingency:** Implement CTE only, defer functions/procedures/triggers to Beta

**P-007 & P-008 (Bytecode Mismatches) - Medium Risk:**
- **Risk:** Requires deep understanding of both parser and executor
- **Mitigation:**
  - Document bytecode format expectations first
  - Implement fixes incrementally
  - Extensive integration testing
- **Contingency:** Focus on most common statements, defer edge cases to Beta

### Dependencies

**Critical Dependencies:**
- P-009 depends on P-001 (ON COMMIT needs TEMPORARY TABLES infrastructure)
- P-005 decision affects P-006 scope (if keeping PostgreSQL features, PSQL scope changes)

### Timeline Risks

**Best Case:** 32 days (with 3 developers, no issues)
**Expected Case:** 40 days (with 2 developers, minor issues)
**Worst Case:** 60+ days (with 1 developer or major blockers)

**Recommendation:** Allocate at least 2 developers to stay within 2-month Alpha timeline

---

## Success Criteria - Alpha Release

### ⚠️ ABSOLUTE REQUIREMENT: 100% COMPLETION

Before declaring Alpha ready, **ALL 10 ISSUES MUST BE 100% COMPLETE**. No exceptions, no deferrals.

### ALL ISSUES (100% REQUIRED)

**Parser Foundation:**
- [ ] **P-001:** Temporary tables fully implemented (MUST: full implementation, NOT reject-with-error)
  - Session/transaction scoping works
  - ON COMMIT semantics implemented
  - All 4 parsers support temporary tables
  - Cleanup on disconnect verified

**V2 Parser:**
- [ ] **P-002:** V2 parser supports all 11 index types (MUST: all 11, not 5)
  - BTREE, HASH, GIN, GIST, BRIN ✓ (existing)
  - SPGIST, RTREE, HNSW, BITMAP, COLUMNSTORE, LSM ✓ (new)
  - Dead code removed from semantic analyzer
- [ ] **P-005:** V2 PostgreSQL contamination removed (MUST: pure Firebird style)
  - ON CONFLICT replaced with UPDATE OR INSERT or MERGE
  - UPDATE...FROM replaced with subqueries or MERGE
  - DELETE...USING replaced with subqueries
  - Migration guide provided
- [ ] **P-006:** V2 PSQL implementation complete (MUST: FULL implementation)
  - CTE (WITH clause) ✓ REQUIRED
  - CREATE FUNCTION ✓ REQUIRED
  - CREATE PROCEDURE ✓ REQUIRED
  - CREATE TRIGGER ✓ REQUIRED
  - EXECUTE BLOCK ✓ REQUIRED (not deferred)

**PostgreSQL Parser:**
- [ ] **P-003:** PostgreSQL ARRAY types work correctly (MUST: no VARCHAR fallthrough)
  - ARRAY types stored as TYPE_ARRAY
  - Array operations functional
- [ ] **P-007:** PostgreSQL bytecode 100% compatible (MUST: 100%, not 80%)
  - All DDL statements execute correctly
  - All DML statements execute correctly
  - No bytecode format errors

**MySQL Parser:**
- [ ] **P-004:** MySQL ON DUPLICATE KEY UPDATE functional (MUST: working implementation)
  - Remapped to MERGE or native implementation
  - Upsert semantics correct
  - Multi-row inserts supported
- [ ] **P-008:** MySQL bytecode 100% compatible (MUST: 100%, not 80%)
  - CREATE INDEX implemented (not stub)
  - CREATE VIEW implemented (not stub)
  - All bytecode formats aligned with executor

**Firebird Parser:**
- [ ] **P-009:** Firebird ON COMMIT clause stored and executed (MUST: not discarded)
  - ON COMMIT DELETE ROWS works
  - ON COMMIT PRESERVE ROWS works
  - Default behavior correct
- [ ] **P-010:** Firebird context variable keywords work (MUST: all 4 keywords)
  - GEN_ID() parses correctly
  - GEN_UUID() parses correctly
  - RDB$GET_CONTEXT() parses correctly
  - RDB$SET_CONTEXT() parses correctly

### Quality Gates (ALL REQUIRED)

- [ ] All parser test suites passing (100% pass rate)
- [ ] No critical bugs in issue tracker
- [ ] No medium bugs in issue tracker
- [ ] Integration tests passing (all 4 parsers)
- [ ] Cross-parser compatibility tests passing
- [ ] Documentation updated (all parsers)
- [ ] Migration guides provided for breaking changes
- [ ] Performance regression tests passing
- [ ] Memory leak tests passing (valgrind clean)
- [ ] Security audit complete (no temp table leakage)

### Verification Checklist

**Before marking Alpha complete, verify:**

1. **Functional Verification:**
   - [ ] Create temporary tables in all 4 parsers → verify session-scoped
   - [ ] Create all 11 index types in V2 → verify catalog entries
   - [ ] Execute PostgreSQL statements with ARRAY types → verify correct storage
   - [ ] Execute MySQL ON DUPLICATE KEY UPDATE → verify upsert behavior
   - [ ] Execute V2 Firebird-style statements → verify no PostgreSQL syntax
   - [ ] Create functions/procedures/triggers in V2 → verify execution
   - [ ] Test PostgreSQL parser with 100 random statements → verify 100% success
   - [ ] Test MySQL parser with 100 random statements → verify 100% success
   - [ ] Test Firebird ON COMMIT → verify correct behavior
   - [ ] Test Firebird context variables → verify all work

2. **Non-Functional Verification:**
   - [ ] Performance benchmarks → verify no regression
   - [ ] Concurrent access tests → verify thread safety
   - [ ] Crash recovery tests → verify temp tables cleaned up
   - [ ] Large dataset tests → verify scalability

3. **Documentation Verification:**
   - [ ] User documentation complete for all features
   - [ ] Developer documentation updated
   - [ ] API documentation current
   - [ ] Migration guides accurate

---

## Post-Alpha Work

### Beta Release Targets

**With all parser work complete in Alpha, Beta can focus on:**
- Advanced query optimization
- Additional SQL features beyond the 4 supported dialects
- Performance tuning
- Additional index algorithms
- Query plan visualization
- Advanced security features

### Technical Debt (Post-Alpha Cleanup)

Items to address after Alpha (non-blocking):
- Unify bytecode format across all parsers (eliminate duplication)
- Consolidate duplicate code between parsers (DRY violations)
- Add parser versioning/compatibility layer
- Improve error messages and error recovery
- Add query rewrite/optimization layer
- Refactor large parser files (some >3000 lines)

---

## Appendix: Related Documents

### Specifications
- `/docs/specifications/FIREBIRD_V2_FEATURE_PARITY_SPECIFICATION.md` - P-001, P-009
- `/docs/specifications/V2_PARSER_INDEX_TYPE_COMPLETENESS.md` - P-002
- `/docs/specifications/V2_PARSER_FIREBIRD_ALIGNMENT_SPECIFICATION.md` - P-005
- `/docs/specifications/POSTGRESQL_PARSER_IMPLEMENTATION_GAPS.md` - P-003, P-007
- `/docs/specifications/MYSQL_PARSER_IMPLEMENTATION_GAPS.md` - P-004, P-008
- `/docs/specifications/PARSER_REMAPPING_AND_IMPLEMENTATION_STRATEGY.md` - General strategy

### Roadmaps
- `/docs/archive/2026-01-09/planning/V2_INDEX_TYPE_IMPLEMENTATION_ROADMAP.md` - P-002 detailed plan

### Audit Reports
- `/docs/audit/parsers/CRITICAL_FINDINGS.md` - All issues summary
- `/docs/audit/parsers/V2/SUMMARY.md` - V2 parser detailed audit
- `/docs/audit/parsers/FirebirdSQL/SUMMARY.md` - Firebird parser detailed audit
- `/docs/audit/parsers/PostgreSQL/SUMMARY.md` - PostgreSQL parser detailed audit
- `/docs/audit/parsers/MySQL/SUMMARY.md` - MySQL parser detailed audit

---

**End of Master Plan**
**Status:** Ready for Team Review and Resource Allocation
**Next Steps:**
1. Review and approve plan
2. Assign owners to each P-XXX issue
3. Create GitHub issues with milestones
4. Begin Phase 1 implementation
