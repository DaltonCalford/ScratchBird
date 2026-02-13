# PostgreSQL Parser Implementation Gaps and Remediation Specification (Non-Authoritative Reference)

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Status Note:** Historical gap analysis only. MUST NOT be used for V3 implementation.
Use `PARSER_TO_SBLR_EMISSION_RULES.md` and `findings/DIALECT_GAP_EXAMPLES.md` for
authoritative emission rules.


**Authoritative MGA/Lock/GC References:**
- [TRANSACTION_MGA_CORE.md](transaction/TRANSACTION_MGA_CORE.md)
- [TRANSACTION_LOCK_MANAGER.md](transaction/TRANSACTION_LOCK_MANAGER.md)
- [MGA_IMPLEMENTATION.md](storage/MGA_IMPLEMENTATION.md)
- [FIREBIRD_GC_SWEEP_GLOSSARY.md](transaction/FIREBIRD_GC_SWEEP_GLOSSARY.md)
- [FIREBIRD_CONSTANTS_REFERENCE.md](transaction/FIREBIRD_CONSTANTS_REFERENCE.md)


**Version:** 1.0
**Date:** 2026-01-07
**Status:** ACTIVE - Alpha Requirement
**Priority:** CRITICAL - Must complete before Alpha release

**WAL Scope:** ScratchBird does not use write-after log (WAL) for recovery in Alpha; any WAL support is optional extension (replication/PITR).
Any WAL references in this document describe an optional extension stream for
replication/PITR only.

## Scope and Dialect Isolation (Normative)

- This document applies **only** to the **PostgreSQL emulated parser**.
- The ScratchBird core parser must not accept PostgreSQL syntax.
- The engine must not parse SQL at runtime; it only executes SBLR.
- Any PostgreSQL feature listed here that is not explicitly implemented MUST be
  rejected by the PostgreSQL parser with a clear error (SQLSTATE `0A000` feature not supported).

## Resolution Policy (Normative)

- Any item marked **UNKNOWN** is **out-of-scope** until explicitly defined.
- Any item marked **Not Implemented** MUST be rejected with SQLSTATE `0A000`
  unless this document provides an explicit, authoritative emission rule.
- The PostgreSQL parser MUST NOT accept syntax it cannot emit into valid SBLR.

## Authoritative Emission Rules (No Rejection)

All remaining gaps below MUST be emitted into valid SBLR. Use the same payload
schemas as the ScratchBird dialect unless explicitly stated.

### UNLOGGED TABLES
- Emit `SBLR3_CREATE_TABLE` with `flags` containing `UNLOGGED`.

### EXPRESSION INDEXES
- Emit `SBLR3_CREATE_INDEX` with `INDEX_KEY.kind = EXPRESSION` and `name_or_expr` set to the expression AST.

### INCLUDE CLAUSE (Covering Indexes)
- Emit `SBLR3_CREATE_INDEX` with `include:list<ident>` populated from INCLUDE clause.

### INHERITS
- Emit `SBLR3_CREATE_TABLE` with `inherits:list<schema_path>` populated from INHERITS list.

### NETWORK TYPES (INET/CIDR/MACADDR)
- Map to ScratchBird type markers (`TYPE_INET`, `TYPE_CIDR`, `TYPE_MACADDR`, `TYPE_MACADDR8`) and emit standard type payloads.

### TEMPORARY TABLES
- Emit `SBLR3_CREATE_TABLE` with `flags` containing `TEMPORARY`.

### ARRAY TYPES
- Emit `TYPE_ARRAY` with `HAS_ELEMENT_TYPE` and the resolved element `TYPE_SPEC`.
- If dimensions are specified, also set `HAS_DIMENSIONS` and emit the lower/upper bounds.

### JSONB / XML / INTERVAL / MONEY / COMPOSITE / TIME_TZ / TIMESTAMP_TZ
- Map to their SBLR type markers: `TYPE_JSONB`, `TYPE_XML`, `TYPE_INTERVAL`, `TYPE_MONEY`,
  `TYPE_COMPOSITE`, `TYPE_TIME_TZ`, `TYPE_TIMESTAMP_TZ`.

### CREATE TABLE / INDEX / VIEW (Format Alignment)
- Emit using the exact payload order from `SBLR_V3_OPCODE_PAYLOADS.md`.
- `IF NOT EXISTS` must be encoded only in `flags` (no extra byte).
- `CREATE VIEW` must embed a full `stmt` bytecode for the view query (not SQL text).

### ALTER TABLE (Format Alignment)
- Emit `SBLR3_ALTER_TABLE` with `SCHEMA_DDL_ALTER_TABLE` payload; do not use deprecated opcodes.
- `ALTER COLUMN ... USING` must use action `29` with `TYPE_SPEC` and a nested `expr`.

### SELECT DISTINCT (Format Alignment)
- Encode DISTINCT via `SCHEMA_SELECT.flags` bit `0x0001`; do not emit a standalone DISTINCT byte.

### INSERT / UPDATE / DELETE (Format Alignment)
- `INSERT` aliases must only appear in `alias:opt<ident>`.
- Multi-row VALUES use `values:list<expr_list>`; no per-row format deviations.
- `UPDATE` and `DELETE` aliases must only appear in `alias:opt<ident>`.
- `DELETE ... USING` must emit `using:opt<stmt>` and `using_joins:stmt_list`.

### MERGE (Executor-Compatible Form)
- Emit `SBLR3_MERGE` using `SCHEMA_MERGE` payload; do not use EXT_MERGE_* opcodes.

---

## Executive Summary

This specification documents all parsed-but-not-implemented features in the PostgreSQL emulated parser and provides concrete remediation strategies. The PostgreSQL parser has **excellent dialect purity (100%)** but suffers from bytecode format mismatches with the executor and several critical bugs in type handling.

**Critical Finding:** The PostgreSQL parser has **20-30% executor compatibility** due to bytecode format mismatches.

**Alpha Requirement:** Full PostgreSQL compatibility support must be achieved before Alpha release.

---

## Table of Contents

1. [Critical Bugs](#critical-bugs)
2. [Critical Implementation Gaps](#critical-implementation-gaps)
3. [Bytecode Format Mismatches](#bytecode-format-mismatches)
4. [PostgreSQL-Specific Features](#postgresql-specific-features)
5. [Implementation Roadmap](#implementation-roadmap)
6. [Testing Requirements](#testing-requirements)

---

## Critical Bugs

### BUG #1: ARRAY Types Mapped to VARCHAR

**Status:** ❌ **CRITICAL BUG**
**Impact:** ARRAY columns stored incorrectly
**File:** `src/parser/postgresql/pg_parser.cpp:508-551`

#### The Bug

```cpp
sblr::Opcode Parser::typeToOpcode(PgDataType::Kind kind) {
    switch (kind) {
        case PgDataType::Kind::SMALLINT:
        // ... many cases
        case PgDataType::Kind::JSON:
        case PgDataType::Kind::JSONB:
            return sblr::Opcode::TYPE_JSON;
        default:
            return sblr::Opcode::TYPE_VARCHAR;  // ❌ BUG: ARRAY falls through to default
    }
}
```

**Array Parsing** (pg_parser_ddl.cpp:718-725):
```cpp
// Check for array modifier
while (match(TokenType::LEFT_BRACKET)) {
    type.kind = PgDataType::Kind::ARRAY;  // ✅ Parsed correctly
    if (check(TokenType::INTEGER_LITERAL)) {
        advance();  // Array dimension
    }
    consume(TokenType::RIGHT_BRACKET, "Expected ]");
}
```

**Result:**
```sql
CREATE TABLE test (tags TEXT[]);
-- ❌ Column 'tags' is stored as VARCHAR, not ARRAY
-- ❌ Array operations fail
```

#### Remediation

**IMMEDIATE FIX REQUIRED:**

```cpp
sblr::Opcode Parser::typeToOpcode(PgDataType::Kind kind) {
    switch (kind) {
        // ... existing cases
        case PgDataType::Kind::JSON:
        case PgDataType::Kind::JSONB:
            return sblr::Opcode::TYPE_JSON;
        case PgDataType::Kind::ARRAY:  // ✅ ADD THIS
            return sblr::Opcode::TYPE_ARRAY;
        default:
            return sblr::Opcode::TYPE_VARCHAR;
    }
}

// Also update emitTypeDefinition to emit array element type:
void Parser::emitTypeDefinition(const PgDataType& type) {
    if (type.kind == PgDataType::Kind::ARRAY) {
        emit(sblr::Opcode::TYPE_ARRAY);
        // Emit element type
        emit(typeToOpcode(type.element_type));  // NEW: Need to track element type
        emitU32(type.array_dimensions);         // NEW: Emit dimensions
        return;
    }
    // ... existing code
}
```

**Additional Changes Needed:**
1. **pg_parser.h**: Add `element_type` and `array_dimensions` fields to `PgDataType`
2. **pg_parser_ddl.cpp**: Store element type before setting `kind = ARRAY`

**Effort:** 1 day
**Priority:** ❌ **CRITICAL - Alpha Blocker**

---

## Critical Implementation Gaps

### 1. TEMPORARY TABLES

**Status:** ❌ **CRITICAL - Silent Failure**
**Impact:** Creates permanent tables instead of temporary
**File:** `src/parser/postgresql/pg_parser_ddl.cpp:158`

#### Current Behavior

```cpp
void Parser::parseCreate() {
    // ... handle CREATE OR REPLACE

    // Handle TEMP/TEMPORARY
    bool is_temp = matchKeyword(TokenType::KW_TEMP) || matchKeyword(TokenType::KW_TEMPORARY);
    // ❌ is_temp is READ but NEVER USED

    // Handle UNLOGGED
    bool is_unlogged = matchKeyword(TokenType::KW_UNLOGGED);
    // ❌ is_unlogged is READ but NEVER USED

    // What to create?
    if (matchKeyword(TokenType::KW_TABLE)) {
        parseCreateTable();  // ❌ No temp flag passed
    }
    // ...
}
```

**Result:** `CREATE TEMP TABLE test (id INT)` creates a **permanent** table.

#### Remediation

**Option A: Full Implementation (RECOMMENDED for Beta)**
- Implement temporary table support across full stack (see FIREBIRD_V2_FEATURE_PARITY_SPECIFICATION.md)
- Estimated effort: 3-5 days
- Priority: **BETA**

**Option B: Explicit Rejection (Alpha)**
```cpp
bool is_temp = matchKeyword(TokenType::KW_TEMP) || matchKeyword(TokenType::KW_TEMPORARY);
if (is_temp) {
    error("TEMPORARY tables are not yet supported in PostgreSQL parser");
}
```
- Estimated effort: 30 minutes
- Priority: **IMMEDIATE**

**Recommendation:** Implement Option B for Alpha, Option A for Beta.

---

### 2. UNLOGGED TABLES

**Status:** ⚠️ **MEDIUM - Parsed but Ignored**
**Impact:** Performance optimization unavailable
**File:** `src/parser/postgresql/pg_parser_ddl.cpp:160-161`

#### Current Behavior

```cpp
// Handle UNLOGGED
bool is_unlogged = matchKeyword(TokenType::KW_UNLOGGED);
// ❌ Flag read but never used
```

**PostgreSQL UNLOGGED semantics:**
- Table is not written to the optional write-after log (WAL, optional extension)
- Faster writes (no write-after log (WAL, optional extension) overhead)
- Truncated on crash recovery
- Not replicated to standby servers

**ScratchBird MGA note:**
- MGA does not use write-after log (WAL, optional extension) for recovery, so UNLOGGED tables are effectively the same as regular tables today.
- If an optional write-after log (WAL, optional extension) is introduced later (replication/PITR), UNLOGGED can bypass that stream.

**Result:** `CREATE UNLOGGED TABLE test (id INT)` creates a normal table. Under MGA, this is acceptable; any difference only appears if a write-after log (WAL, optional extension) is added later.

#### Remediation

**Option A: Implement UNLOGGED Support**

1. Pass `is_unlogged` flag to `parseCreateTable()`
2. Emit flag in bytecode
3. Store in catalog metadata
4. Skip write-after log (WAL, optional extension) if introduced
5. Truncate on crash recovery only if a write-after log (WAL, optional extension) durability path is introduced

**Effort:** 2-3 days
**Priority:** **optional extension** (optimization feature)

**Option B: Document as Unsupported**
```cpp
if (is_unlogged) {
    warning("UNLOGGED tables are treated as regular tables in ScratchBird");
}
```

**Effort:** 1 hour
**Priority:** **ALPHA**

**Recommendation:** Option B for Alpha (document), Option A for Beta (implement).

---

### 3. Expression Indexes

**Status:** ❌ **HIGH - Explicitly Not Supported**
**Impact:** Advanced indexing unavailable
**File:** `src/parser/postgresql/pg_parser_ddl.cpp:931`

#### Current Behavior

```cpp
void Parser::parseCreateIndex() {
    // ... parse index name and table

    consume(TokenType::LEFT_PAREN, "Expected (");
    do {
        if (check(TokenType::LEFT_PAREN)) {
            // Expression index: CREATE INDEX idx ON t ((lower(name)))
            error("PostgreSQL expression indexes are not supported in current bytecode yet");
        }
        // ... parse column
    } while (match(TokenType::COMMA));
}
```

**Result:** Expression indexes are **rejected with error** (good - no silent failure).

#### Remediation

**Option A: Implement Expression Indexes**

PostgreSQL expression index syntax:
```sql
CREATE INDEX idx_lower_name ON users ((lower(name)));
CREATE INDEX idx_full_name ON users ((first_name || ' ' || last_name));
```

**Implementation:**
1. Parse expression instead of column name
2. Generate expression bytecode
3. Store in index metadata
4. Evaluate expression on INSERT/UPDATE

**Effort:** 3-4 days
**Priority:** **optional extension**

**Option B: Keep Current Behavior**

Current error is clear and prevents silent failures.

**Recommendation:** Keep Option B for Alpha. Expression indexes are advanced feature.

---

### 4. INCLUDE Clause (Covering Indexes)

**Status:** ❌ **MEDIUM - Explicitly Not Supported**
**Impact:** Covering index optimization unavailable
**File:** `src/parser/postgresql/pg_parser_ddl.cpp:934`

#### Current Behavior

```cpp
if (matchKeyword(TokenType::KW_INCLUDE)) {
    error("PostgreSQL INCLUDE indexes are not supported in current bytecode yet");
}
```

**PostgreSQL INCLUDE semantics:**
```sql
CREATE INDEX idx_user ON users (email) INCLUDE (name, created_at);
-- Index can satisfy: SELECT name, created_at FROM users WHERE email = 'x'
-- Without accessing heap (index-only scan)
```

**Result:** INCLUDE indexes are **rejected with error** (good).

#### Remediation

**Option A: Implement INCLUDE Support**

1. Parse INCLUDE column list
2. Store in index metadata
3. Include columns in index leaf nodes
4. Enable index-only scans

**Effort:** 2-3 days
**Priority:** **optional extension**

**Option B: Keep Current Behavior**

Current error is clear.

**Recommendation:** Keep Option B for Alpha. Covering indexes are optimization feature.

---

### 5. INHERITS Clause

**Status:** ⚠️ **LOW - Partially Parsed**
**Impact:** Table inheritance unavailable
**File:** `src/parser/postgresql/pg_parser_ddl.cpp:1638, 1692, 1855`

#### Current Behavior

Multiple locations emit placeholder:
```cpp
emitByte(0);  // no inherits
```

**PostgreSQL INHERITS semantics:**
```sql
CREATE TABLE employees (id INT, name TEXT);
CREATE TABLE managers (department TEXT) INHERITS (employees);
-- managers table has: id, name, department
```

**Result:** INHERITS syntax would fail during parsing (not currently accepted).

#### Remediation

**Option A: Implement Table Inheritance**

Complex feature requiring:
1. Parse INHERITS clause
2. Copy parent columns to child
3. Implement inheritance queries (SELECT from parent includes children)
4. Handle INSERT/UPDATE/DELETE polymorphism

**Effort:** 5-7 days
**Priority:** **optional extension**

**Option B: Reject with Error**
```cpp
if (check(TokenType::KW_INHERITS)) {
    error("Table inheritance (INHERITS) is not supported in ScratchBird");
}
```

**Effort:** 1 hour
**Priority:** **ALPHA**

**Recommendation:** Option B for Alpha. Table inheritance is advanced feature.

---

## Bytecode Format Mismatches

**Source:** `/docs/specifications/parser/v3/audit/19_postgresql_parser_correction_plan_checklist.md`

### DDL Statement Mismatches

| Statement | Mismatch | Impact | Priority |
|-----------|----------|--------|----------|
| **CREATE TABLE** | Extra IF NOT EXISTS byte, column/constraint format differs | Executor rejects | **CRITICAL** |
| **CREATE INDEX** | Payload ordering different | Executor rejects | **CRITICAL** |
| **CREATE VIEW** | Emits SELECT bytecode instead of SQL string | Executor rejects | **CRITICAL** |
| **ALTER TABLE** | Uses deprecated opcodes | Executor rejects | **HIGH** |

### DML Statement Mismatches

| Statement | Mismatch | Impact | Priority |
|-----------|----------|--------|----------|
| **SELECT** | DISTINCT flag byte not expected | Executor rejects | **CRITICAL** |
| **INSERT** | Alias encoding, multi-row format differs | Executor rejects | **CRITICAL** |
| **UPDATE** | Alias string not expected | Executor rejects | **CRITICAL** |
| **DELETE** | Alias, USING clause unsupported | Executor rejects | **CRITICAL** |
| **MERGE** | No executor support for EXT_MERGE_* opcodes | Executor rejects | **HIGH** |

### Remediation Strategy

**Per audit document 19, choose one:**

**Option A: Fix Parser Output (RECOMMENDED)**
- Align PostgreSQL parser bytecode to match executor expectations
- Effort: 5-7 days
- Priority: **ALPHA BLOCKER**

**Option B: Extend Executor**
- Modify executor to accept PostgreSQL parser format
- Effort: 7-10 days
- Risk: May break other parsers

**Option C: Bytecode Versioning**
- Support multiple bytecode versions
- Effort: 10-15 days
- Complexity: High

**Recommendation:** Option A - Fix parser output to match executor.

---

## PostgreSQL-Specific Features

### 6. SERIAL Types

**Status:** ✅ **WORKS - Mapped to IDENTITY**
**File:** `src/parser/postgresql/pg_parser.cpp:512-517`

#### Current Behavior

```cpp
case PgDataType::Kind::SMALLINT:
case PgDataType::Kind::INTEGER:
case PgDataType::Kind::SERIAL:        // ✅ Maps to INTEGER
case PgDataType::Kind::SMALLSERIAL:   // ✅ Maps to INTEGER
    return sblr::Opcode::TYPE_INTEGER;
case PgDataType::Kind::BIGINT:
case PgDataType::Kind::BIGSERIAL:     // ✅ Maps to BIGINT
    return sblr::Opcode::TYPE_BIGINT;
```

**Result:** SERIAL types work correctly.

**Recommendation:** ✅ **No action needed**.

---

### 7. UUID, JSON, JSONB Types

**Status:** ✅ **WORKS - Mapped Correctly**
**File:** `src/parser/postgresql/pg_parser.cpp:543-547`

#### Current Behavior

```cpp
case PgDataType::Kind::UUID:
    return sblr::Opcode::TYPE_UUID;
case PgDataType::Kind::JSON:
case PgDataType::Kind::JSONB:
    return sblr::Opcode::TYPE_JSON;
```

**Result:** UUID and JSON types work correctly.

**Recommendation:** ✅ **No action needed**.

---

### 8. Network Types (INET, CIDR, MACADDR)

**Status:** ⚠️ **PARSED - Implementation Unknown**
**File:** `src/parser/postgresql/pg_parser_ddl.cpp:687-694`

#### Current Behavior

Parser accepts:
```sql
CREATE TABLE network_data (
    ip_addr INET,
    network CIDR,
    mac MACADDR,
    mac8 MACADDR8
);
```

But `typeToOpcode()` has no case for these types, so they fall through to `default: TYPE_VARCHAR`.

#### Remediation

**Option A: Add Specialized Network Types**

If SBLR supports network types:
```cpp
case PgDataType::Kind::INET:
    return sblr::Opcode::TYPE_INET;  // If opcode exists
case PgDataType::Kind::CIDR:
    return sblr::Opcode::TYPE_CIDR;
case PgDataType::Kind::MACADDR:
    return sblr::Opcode::TYPE_MACADDR;
```

**Option B: Map to VARCHAR with Validation**

Store as VARCHAR with CHECK constraints:
```cpp
case PgDataType::Kind::INET:
case PgDataType::Kind::CIDR:
case PgDataType::Kind::MACADDR:
case PgDataType::Kind::MACADDR8:
    return sblr::Opcode::TYPE_VARCHAR;
// Add CHECK constraint for format validation
```

**Effort:** Option A (if opcodes exist): 1 day; Option B: 2 days
**Priority:** **optional extension**

**Recommendation:** Investigate if SBLR has network type opcodes. If not, use Option B.

---

### 9. DEFERRABLE Constraints

**Status:** ✅ **WORKS - Bytecode Emitted**
**File:** `src/parser/postgresql/pg_parser_ddl.cpp:415-422`

#### Current Behavior

```cpp
uint8_t deferrable_flags = 0;
if (fk.deferrable) {
    deferrable_flags |= 0x01;
}
if (fk.initially_deferred) {
    deferrable_flags |= 0x02;
}
emitByte(deferrable_flags);  // ✅ Emitted to bytecode
```

**Status:** Bytecode is emitted. Needs executor verification.

**Action Required:** Test that executor enforces deferred constraint checking.

**Priority:** **ALPHA** (verification only)

---

## Implementation Roadmap

### Phase 1: Critical Bugs and Alpha Blockers - 7-10 days

**Must complete before Alpha release:**

1. **FIX ARRAY TYPE BUG** - 1 day
   - Add ARRAY case to typeToOpcode()
   - Track element type in PgDataType
   - Emit array metadata
   - Test array columns

2. **TEMPORARY TABLE REJECTION** - 30 minutes
   - Add error for TEMP/TEMPORARY syntax
   - Document as unsupported

3. **UNLOGGED TABLE WARNING** - 1 hour
   - Add warning that UNLOGGED is treated as regular

4. **FIX DDL BYTECODE FORMATS** - 3-4 days
   - Align CREATE TABLE format with executor
   - Align CREATE INDEX format with executor
   - Align CREATE VIEW format with executor
   - Test all DDL statements

5. **FIX DML BYTECODE FORMATS** - 3-4 days
   - Fix SELECT DISTINCT format
   - Fix INSERT multi-row format
   - Fix UPDATE format
   - Fix DELETE format
   - Test all DML statements

**Total:** 7-10 days

---

### Phase 2: optional extension Features - 5-7 days

6. **DEFERRABLE Constraint Verification** - 1 day
   - Test SET CONSTRAINTS DEFERRED
   - Test deferred checking on COMMIT
   - Add test suite

7. **Network Type Support** - 2-3 days
   - Investigate SBLR network type opcodes
   - Implement or map to VARCHAR with validation

8. **INHERITS Rejection** - 1 hour
   - Add error for INHERITS syntax
   - Document as unsupported

9. **Table Inheritance Investigation** - 2-3 days
   - Research ScratchBird table inheritance support
   - Document compatibility

**Total:** 5-7 days

---

### Phase 3: Beta Features - 10-15 days

10. **TEMPORARY TABLES** (Full implementation) - 3-5 days
    - See FIREBIRD_V2_FEATURE_PARITY_SPECIFICATION.md

11. **UNLOGGED TABLES** - 2-3 days
    - Implement write-after log (WAL, optional extension) bypass (optional)
    - Implement crash recovery truncation if write-after log (WAL, optional extension) durability is introduced

12. **Expression Indexes** - 3-4 days
    - Parse expressions
    - Evaluate on INSERT/UPDATE

13. **INCLUDE Clause** - 2-3 days
    - Store covering columns
    - Enable index-only scans

**Total:** 10-15 days

---

## Testing Requirements

### Phase 1 Tests (Alpha) - Critical

```sql
-- Test 1: ARRAY type correctness
CREATE TABLE test_array (tags TEXT[]);
INSERT INTO test_array VALUES (ARRAY['tag1', 'tag2']);
SELECT tags[1] FROM test_array;
-- Expected: 'tag1'

-- Test 2: TEMP TABLE rejection
CREATE TEMP TABLE test (id INT);
-- Expected: Clear error message

-- Test 3: UNLOGGED warning
CREATE UNLOGGED TABLE test (id INT);
-- Expected: Warning message, table created as regular

-- Test 4: DDL bytecode format
CREATE TABLE users (id SERIAL PRIMARY KEY, name TEXT);
CREATE INDEX idx_name ON users (name);
CREATE VIEW active_users AS SELECT * FROM users;
-- Expected: All succeed

-- Test 5: DML bytecode format
INSERT INTO users (name) VALUES ('Alice'), ('Bob');
UPDATE users SET name = 'Charlie' WHERE id = 1;
DELETE FROM users WHERE id = 2;
SELECT DISTINCT name FROM users;
-- Expected: All succeed
```

### Phase 2 Tests (optional extension)

```sql
-- Test 6: DEFERRABLE constraints
CREATE TABLE parent (id INT PRIMARY KEY);
CREATE TABLE child (
    id INT PRIMARY KEY,
    parent_id INT REFERENCES parent(id) DEFERRABLE INITIALLY DEFERRED
);
BEGIN;
INSERT INTO child VALUES (1, 1);  -- Should succeed (deferred)
INSERT INTO parent VALUES (1);    -- Satisfy constraint
COMMIT;  -- Should succeed
-- Expected: All succeed

-- Test 7: Network types
CREATE TABLE network (ip INET, mac MACADDR);
INSERT INTO network VALUES ('192.168.1.1', '08:00:2b:01:02:03');
SELECT * FROM network;
-- Expected: Values stored and retrieved correctly

-- Test 8: INHERITS rejection
CREATE TABLE employees (id INT);
CREATE TABLE managers (dept TEXT) INHERITS (employees);
-- Expected: Clear error message
```

---

## Compliance Matrix

| Feature | Parsed | Bytecode | Executor | Status | Priority |
|---------|--------|----------|----------|--------|----------|
| **Critical Bugs** |
| ARRAY types | ✅ | ❌ **BUG** | ❌ | **BROKEN** | ❌ **CRITICAL** |
| **DDL** |
| CREATE TEMP TABLE | ✅ | ❌ | ❌ | **IGNORED** | **CRITICAL** |
| CREATE UNLOGGED TABLE | ✅ | ❌ | ❌ | **IGNORED** | **MEDIUM** |
| Expression indexes | ❌ | ❌ | ❌ | **REJECTED** | **optional extension** |
| INCLUDE clause | ❌ | ❌ | ❌ | **REJECTED** | **optional extension** |
| INHERITS clause | ⚠️ | ❌ | ❌ | **PARTIAL** | **LOW** |
| **Types** |
| SERIAL/BIGSERIAL | ✅ | ✅ | ✅ | **WORKS** | **COMPLETE** |
| UUID | ✅ | ✅ | ✅ | **WORKS** | **COMPLETE** |
| JSON | ✅ | ✅ | ✅ | **WORKS** | **COMPLETE** |
| JSONB (typed marker) | ✅ | ❌ | ❌ | **BLOCKED (SBLR type marker)** | **ALPHA** |
| XML | ✅ | ❌ | ❌ | **BLOCKED (SBLR type marker)** | **ALPHA** |
| INTERVAL | ✅ | ❌ | ❌ | **BLOCKED (SBLR type marker)** | **ALPHA** |
| MONEY | ✅ | ❌ | ❌ | **BLOCKED (SBLR type marker)** | **ALPHA** |
| COMPOSITE | ✅ | ❌ | ❌ | **BLOCKED (SBLR type marker)** | **ALPHA** |
| INET/CIDR/MACADDR/MACADDR8 | ✅ | ❌ | ❌ | **BLOCKED (SBLR type marker)** | **ALPHA** |
| TIME/TIMESTAMP WITH TIME ZONE | ✅ | ❌ | ❌ | **BLOCKED (SBLR type marker)** | **ALPHA** |
| **Constraints** |
| DEFERRABLE | ✅ | ✅ | ❓ | **VERIFY** | **ALPHA** |
| **Bytecode Format** |
| CREATE TABLE format | ✅ | ⚠️ | ❌ | **MISMATCH** | ❌ **CRITICAL** |
| CREATE INDEX format | ✅ | ⚠️ | ❌ | **MISMATCH** | ❌ **CRITICAL** |
| CREATE VIEW format | ✅ | ⚠️ | ❌ | **MISMATCH** | ❌ **CRITICAL** |
| SELECT format | ✅ | ⚠️ | ❌ | **MISMATCH** | ❌ **CRITICAL** |
| INSERT format | ✅ | ⚠️ | ❌ | **MISMATCH** | ❌ **CRITICAL** |
| UPDATE format | ✅ | ⚠️ | ❌ | **MISMATCH** | ❌ **CRITICAL** |
| DELETE format | ✅ | ⚠️ | ❌ | **MISMATCH** | ❌ **CRITICAL** |

---

**Note:** SBLR type/literal gaps are tracked in `/docs/specifications/parser/v3/findings/SBLR_TYPE_OPCODE_GAPS.md`
and `docs/planning/SBLR_TYPE_OPCODE_REMEDIATION_PLAN.md`.

## References

### Project Documentation
- `/docs/specifications/parser/v3/audit/parsers/PostgreSQL/SUMMARY.md` - PostgreSQL parser audit
- `/docs/specifications/parser/v3/audit/19_postgresql_parser_correction_plan_checklist.md` - Bytecode format fixes (CRITICAL)
- `/docs/specifications/parser/v3/FIREBIRD_V2_FEATURE_PARITY_SPECIFICATION.md` - Temporary tables implementation
- `/docs/specifications/parser/v3/audit/parsers/COMPARISON_MATRIX.md` - Cross-parser feature comparison

### PostgreSQL Documentation
- [PostgreSQL 16 Documentation - Data Types](https://www.postgresql.org/docs/16/datatype.html)
- [PostgreSQL 16 Documentation - Arrays](https://www.postgresql.org/docs/16/arrays.html)
- [PostgreSQL 16 Documentation - Indexes](https://www.postgresql.org/docs/16/indexes.html)
- [PostgreSQL 16 Documentation - Table Inheritance](https://www.postgresql.org/docs/16/ddl-inherit.html)
- [PostgreSQL 16 Documentation - Constraints](https://www.postgresql.org/docs/16/ddl-constraints.html)

---

## Implementation Checklist

### Phase 1: Critical (ALPHA BLOCKERS)
- [ ] **FIX ARRAY TYPE BUG** - IMMEDIATE
  - [ ] Add PgDataType::ARRAY case to typeToOpcode()
  - [ ] Add element_type field to PgDataType struct
  - [ ] Store element type before setting kind = ARRAY
  - [ ] Update emitTypeDefinition() to emit array metadata
  - [ ] Test array column creation and queries
- [ ] **TEMPORARY TABLE REJECTION**
  - [ ] Add error when TEMP/TEMPORARY detected
  - [ ] Add clear error message
  - [ ] Document as unsupported
- [ ] **UNLOGGED TABLE WARNING**
  - [ ] Add warning when UNLOGGED detected
  - [ ] Document behavior (treated as regular table)
- [ ] **FIX DDL BYTECODE FORMATS** (per audit document 19)
  - [ ] Align CREATE TABLE format with executor expectations
  - [ ] Align CREATE INDEX format with executor expectations
  - [ ] Align CREATE VIEW format with executor expectations
  - [ ] Test all DDL statements execute correctly
- [ ] **FIX DML BYTECODE FORMATS** (per audit document 19)
  - [ ] Fix SELECT DISTINCT bytecode format
  - [ ] Fix INSERT multi-row bytecode format
  - [ ] Fix UPDATE bytecode format
  - [ ] Fix DELETE bytecode format (remove USING if unsupported)
  - [ ] Test all DML statements execute correctly

### Phase 2: optional extension
- [ ] Verify DEFERRABLE constraints work with executor
- [ ] Investigate network type support (INET, CIDR, MACADDR)
- [ ] Add INHERITS rejection with error
- [ ] Document table inheritance as unsupported

### Phase 3: Beta
- [ ] Implement temporary tables (full stack)
- [ ] Implement UNLOGGED table support
- [ ] Implement expression indexes
- [ ] Implement INCLUDE clause for covering indexes

---

## Bytecode Emission Rules (Alpha-Required, No Ambiguity)

This section defines exact bytecode emission rules for remaining PostgreSQL
parity gaps. It is **authoritative** for implementation and must be kept in
sync with `include/scratchbird/sblr/opcodes.h` and
`src/sblr/bytecode_generator_v2.cpp`.

### JSONPATH

**Emission rule (WHERE/expr context):**
- Emit `SBLR3_FUNC_JSON_EXISTS` with 2 arguments:
  1. JSON expression bytecode
  2. JSONPATH literal as `SBLR3_LITERAL_JSONPATH` (UTF-8, raw path text)
- Any JSONPATH-specific functions must normalize to this opcode and argument order.

### Array Domains

**Emission rule:**
- When a domain is used as an array element type, emit:
  - `SBLR3_TYPE_ARRAY` with `HAS_ELEMENT_TYPE`
  - `SBLR3_TYPE_DOMAIN` (with `HAS_DOMAIN_ID` + domain UUID)
  - Optional `HAS_DIMENSIONS` with encoded bounds (per `TYPE_SPEC`)

### Table-Level CHECK Constraints

**CREATE TABLE:**
- Emit a `TABLE_CONSTRAINT` entry with `type=CHECK` and `check_expr` populated,
  inside `SCHEMA_DDL_CREATE_TABLE.constraints`.

**ALTER TABLE ADD CONSTRAINT CHECK:**
- Emit `SBLR3_ALTER_TABLE` with action `2` (ADD_CONSTRAINT) and an
  `ALTER_TABLE_ACTION` payload that is a full `TABLE_CONSTRAINT`:
  - `type=CHECK`
  - `name` set if provided
  - `check_expr` populated with the constraint expression

### CREATE DOMAIN Base Type Support

**Emission rule:**
- Emit `SBLR3_CREATE_DOMAIN` using the payload format defined in
  `/docs/specifications/parser/v3/SBLR_V3_OPCODE_PAYLOADS.md`
  and `/docs/specifications/parser/v3/SBLR_V3_OPCODE_SPEC.md`:
  - `domain_kind = BASIC`
  - `base_type = ResolvedType` (precision/scale/length included)
  - `nullable/default/constraints` as parsed

### ALTER TABLE DROP CONSTRAINT

**Emission rule:**
- `ALTER_TABLE` opcode
- qualified table name
- action byte `4` (DROP_CONSTRAINT)
- constraint name
- cascade flag (u8, 1 if CASCADE else 0)

### ALTER TABLE ALTER COLUMN SET/DROP DEFAULT, SET/DROP NOT NULL

**Action byte values (authoritative, per SBLR V3):**
- 7 = `ALTER_COLUMN_SET_DEFAULT`
- 8 = `ALTER_COLUMN_DROP_DEFAULT`
- 9 = `ALTER_COLUMN_SET_NOT_NULL`
- 10 = `ALTER_COLUMN_DROP_NOT_NULL`
- 29 = `ALTER_COLUMN_USING` (see below)

**Payload formats:**
SET DEFAULT payload: `ALTER_TABLE` opcode, qualified table name, action byte `7`,
column name, expression bytecode bytes.

DROP DEFAULT payload: `ALTER_TABLE` opcode, qualified table name, action byte `8`,
column name.

SET NOT NULL payload: `ALTER_TABLE` opcode, qualified table name, action byte `9`,
column name.

DROP NOT NULL payload: `ALTER_TABLE` opcode, qualified table name, action byte `10`,
column name.

### ALTER TABLE ALTER COLUMN ... USING

**Emission rule:**
- `ALTER_TABLE` opcode
- qualified table name
- action byte `29` (ALTER_COLUMN_USING)
- column name
- target `TYPE_SPEC` (full type specification)
- USING expression bytecode bytes (nested expr instruction)

### TRUNCATE Options

**Emission rule update (authoritative):**
- `TRUNCATE_TABLE` opcode payload becomes:
  - table path string
  - `mode` byte (0=ASYNC, 1=SYNC)
  - `flags` byte:
    - bit0 = RESTART IDENTITY
    - bit1 = CASCADE

### JOIN USING

**Emission rule:**
- `JOIN_CONDITION` opcode followed by an `EXPR_AND` chain of `EXPR_EQ` pairs:
  - `EXPR_EQ (COLUMN_REF left.col_i, COLUMN_REF right.col_i)` for each USING column.
- For `SELECT *`, expand output:
  - For each USING column, emit `COALESCE(left.col_i, right.col_i)` via `SBLR3_COALESCE`.
  - Emit remaining non-USING columns from left then right.

### DEFAULT in Multi-Row INSERT

**Emission rule:**
- In each row value list, token `DEFAULT` emits opcode `SBLR3_DEFAULT_VALUE`
  with no payload.
- Executor replaces `DEFAULT_VALUE` with column default for that column.

### MERGE USING Subqueries

**Emission rule:**
- Emit a single `SBLR3_MERGE` with `SCHEMA_MERGE` payload:
  - `source_table` when USING <table>
  - `source_query` when USING (<subquery>)
  - `on` as a nested expression bytecode
  - `when_*` lists populated per clauses


## SBLR Emission Examples (Full Payload Bytes)

Encoding rules: little-endian, `string = [len:varuint][utf8]`, `schema_path = [count][ident...]`, `list<T> = [count][T...]`.

### TEMPORARY TABLE (CREATE TEMP TABLE t (id INT))
Opcode: `SBLR3_CREATE_TABLE`

Payload bytes (hex, field order):
```
flags             = 02 00
path              = 01 01 74
columns.count     = 01
column_def        = 02 69 64 3F 0B 00 00 00 00 00 00 00 00 00 00
constraints.count = 00
inherits.count    = 00
partitioning.opt  = 00
tablespace.opt    = 00
options.count     = 00
```

### UNLOGGED TABLE (CREATE UNLOGGED TABLE t (id INT))
Opcode: `SBLR3_CREATE_TABLE`

Payload bytes (hex, field order):
```
flags             = 04 00
path              = 01 01 74
columns.count     = 01
column_def        = 02 69 64 3F 0B 00 00 00 00 00 00 00 00 00 00
constraints.count = 00
inherits.count    = 00
partitioning.opt  = 00
tablespace.opt    = 00
options.count     = 00
```

### ARRAY TYPE (CREATE TABLE t (tags TEXT[]))
Opcode: `SBLR3_CREATE_TABLE`

Payload bytes (hex, field order):
```
flags             = 00 00
path              = 01 01 74
columns.count     = 01
column_def        = 04 74 61 67 73 31 0B 20 00 41 0B 00 00 00 00 00 00 00 00 00
constraints.count = 00
inherits.count    = 00
partitioning.opt  = 00
tablespace.opt    = 00
options.count     = 00
```

### EXPRESSION INDEX (CREATE INDEX idxe ON t ((id)))
Opcode: `SBLR3_CREATE_INDEX`

Payload bytes (hex, field order):
```
flags             = 00 00
index_path        = 01 04 69 64 78 65
table             = 01 01 74
keys.count        = 01
key               = 02 04 06 00 00 04 00 00 00 00 02 69 64 00 00 00 00
include.count     = 00
predicate.opt     = 00
index_type.opt    = 00
options.count     = 00
```

### INCLUDE CLAUSE (CREATE INDEX idx_inc ON t (id) INCLUDE (name))
Opcode: `SBLR3_CREATE_INDEX`

Payload bytes (hex, field order):
```
flags             = 00 00
index_path        = 01 07 69 64 78 5F 69 6E 63
table             = 01 01 74
keys.count        = 01
key               = 01 04 06 00 00 04 00 00 00 00 02 69 64 00 00 00 00
include.count     = 01 04 6E 61 6D 65
predicate.opt     = 00
index_type.opt    = 00
options.count     = 00
```

### INHERITS (CREATE TABLE child (id INT) INHERITS (parent))
Opcode: `SBLR3_CREATE_TABLE`

Payload bytes (hex, field order):
```
flags             = 00 00
path              = 01 05 63 68 69 6C 64
columns.count     = 01
column_def        = 02 69 64 3F 0B 00 00 00 00 00 00 00 00 00 00
constraints.count = 00
inherits.count    = 01 01 06 70 61 72 65 6E 74
partitioning.opt  = 00
tablespace.opt    = 00
options.count     = 00
```

### CREATE VIEW (CREATE VIEW v AS SELECT 1)
Opcode: `SBLR3_CREATE_VIEW`

Payload bytes (hex, field order):
```
flags             = 00 00
path              = 01 01 76
columns.count     = 00
query (SELECT)    = 12 02 00 00 1C 00 00 00 00 00 01 0F 0C 00 00 04 00 00 00 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

### ALTER TABLE SET DEFAULT (ALTER TABLE t ALTER COLUMN id SET DEFAULT 1)
Opcode: `SBLR3_ALTER_TABLE`

Payload bytes (hex, field order):
```
set_default       = 01 01 74 00 00 07 02 69 64 0F 0C 00 00 04 00 00 00 01 00 00 00
```

### SELECT DISTINCT (SELECT DISTINCT 1)
Opcode: `SBLR3_SELECT`

Payload bytes (hex, field order):
```
flags             = 01 00
select_items      = 01 0F 0C 00 00 04 00 00 00 01 00 00 00
from.opt          = 00
joins.count       = 00
where.opt         = 00
group_by.count    = 00
grouping_sets.cnt = 00
grouping_type     = 00
having.opt        = 00
order_by.count    = 00
limit.opt         = 00
offset.opt        = 00
fetch.opt         = 00
set_op.opt        = 00
with.opt          = 00
```

### JSONPATH (json_col @@ '$.a')
Opcode: `SBLR3_FUNC_JSON_EXISTS`

Payload bytes (hex, field order):
```
args.count        = 02
arg1 (COLUMN_REF) = 04 06 00 00 04 00 00 00 00 03 64 6F 63
arg2 (JSONPATH)   = 1D 0C 00 00 05 00 00 00 00 03 24 2E 61
```

### INSERT ALIAS (INSERT INTO t AS tt (id) VALUES (1))
Opcode: `SBLR3_INSERT`

Payload bytes (hex, field order):
```
target            = 01 01 74
alias.opt         = 01 02 74 74
columns.count     = 01 02 69 64
source            = 01
values.count      = 01
row.count         = 01
row.int32         = 0F 0C 00 00 04 00 00 00 01 00 00 00
select.opt        = 00
on_conflict.opt   = 00
returning.count   = 00
```

### UPDATE ALIAS (UPDATE t AS tt SET id = 1)
Opcode: `SBLR3_UPDATE`

Payload bytes (hex, field order):
```
target            = 01 01 74
alias.opt         = 01 02 74 74
set_items.count   = 01
set_item          = 02 69 64 0F 0C 00 00 04 00 00 00 01 00 00 00
from.opt          = 00
joins.count       = 00
where.opt         = 00
returning.count   = 00
```

### DELETE USING (DELETE FROM t AS tt USING u)
Opcode: `SBLR3_DELETE`

Payload bytes (hex, field order):
```
target            = 01 01 74
alias.opt         = 01 02 74 74
using.opt         = 01 5C 06 00 00 09 00 00 00 01 01 01 75 00 00 00 00 00
using_joins.count = 00
where.opt         = 00
returning.count   = 00
```

### MERGE (MERGE INTO t USING s ON TRUE WHEN NOT MATCHED THEN INSERT (id) VALUES (1))
Opcode: `SBLR3_MERGE`

Payload bytes (hex, field order):
```
target            = 01 01 74
target_alias.opt  = 00
source_table.opt  = 01 01 01 73
source_query.opt  = 00
source_alias.opt  = 00
on.expr           = 09 0C 00 00 01 00 00 00 01
when_matched.cnt  = 00
when_not_matched.cnt = 01
not_matched       = 00 01 02 69 64 01 0F 0C 00 00 04 00 00 00 01 00 00 00
when_not_matched_by_source.cnt = 00
```

### TYPE_SPEC BYTES (TYPE MARKER GAPS)
Opcode: `TYPE_SPEC` payloads (hex, `[type_opcode:u16][flags:u16]`)
```
TYPE_JSONB        = 0F 0B 00 00
TYPE_XML          = 30 0B 00 00
TYPE_INTERVAL     = 0E 0B 00 00
TYPE_MONEY        = 15 0B 00 00
TYPE_COMPOSITE    = 02 0B 00 00
TYPE_TIME_TZ      = 20 0B 00 00
TYPE_TIMESTAMP_TZ = 1F 0B 00 00
TYPE_INET         = 08 0B 00 00
TYPE_CIDR         = 01 0B 00 00
TYPE_MACADDR      = 13 0B 00 00
TYPE_MACADDR8     = 14 0B 00 00
```

**End of Specification**
**Status:** ACTIVE - Critical Fixes Required for Alpha
**Next Steps:** Fix ARRAY type bug and bytecode format mismatches immediately
