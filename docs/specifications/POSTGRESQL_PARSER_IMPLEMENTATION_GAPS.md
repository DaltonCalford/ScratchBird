# PostgreSQL Parser Implementation Gaps and Remediation Specification

**Version:** 1.0
**Date:** 2026-01-07
**Status:** ACTIVE - Alpha Requirement
**Priority:** CRITICAL - Must complete before Alpha release

**WAL Scope:** ScratchBird does not use write-after log (WAL) for recovery in Alpha; any WAL support is optional post-gold (replication/PITR).
Any WAL references in this document describe an optional post-gold stream for
replication/PITR only.

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
- Table is not written to the optional write-after log (WAL, optional post-gold)
- Faster writes (no write-after log (WAL, optional post-gold) overhead)
- Truncated on crash recovery
- Not replicated to standby servers

**ScratchBird MGA note:**
- MGA does not use write-after log (WAL, optional post-gold) for recovery, so UNLOGGED tables are effectively the same as regular tables today.
- If an optional write-after log (WAL, optional post-gold) is introduced later (replication/PITR), UNLOGGED can bypass that stream.

**Result:** `CREATE UNLOGGED TABLE test (id INT)` creates a normal table. Under MGA, this is acceptable; any difference only appears if a write-after log (WAL, optional post-gold) is added later.

#### Remediation

**Option A: Implement UNLOGGED Support**

1. Pass `is_unlogged` flag to `parseCreateTable()`
2. Emit flag in bytecode
3. Store in catalog metadata
4. Skip write-after log (WAL, optional post-gold) if introduced
5. Truncate on crash recovery only if a write-after log (WAL, optional post-gold) durability path is introduced

**Effort:** 2-3 days
**Priority:** **POST-ALPHA** (optimization feature)

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
**Priority:** **POST-ALPHA**

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
**Priority:** **POST-ALPHA**

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
**Priority:** **POST-BETA**

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

**Source:** `/docs/audit/19_postgresql_parser_correction_plan_checklist.md`

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
**Priority:** **POST-ALPHA**

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

### Phase 2: Post-Alpha Features - 5-7 days

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
    - Implement write-after log (WAL, optional post-gold) bypass (optional)
    - Implement crash recovery truncation if write-after log (WAL, optional post-gold) durability is introduced

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

### Phase 2 Tests (Post-Alpha)

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
| Expression indexes | ❌ | ❌ | ❌ | **REJECTED** | **POST-ALPHA** |
| INCLUDE clause | ❌ | ❌ | ❌ | **REJECTED** | **POST-ALPHA** |
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

**Note:** SBLR type/literal gaps are tracked in `docs/findings/SBLR_TYPE_OPCODE_GAPS.md`
and `docs/planning/SBLR_TYPE_OPCODE_REMEDIATION_PLAN.md`.

## References

### Project Documentation
- `/docs/audit/parsers/PostgreSQL/SUMMARY.md` - PostgreSQL parser audit
- `/docs/audit/19_postgresql_parser_correction_plan_checklist.md` - Bytecode format fixes (CRITICAL)
- `/docs/specifications/FIREBIRD_V2_FEATURE_PARITY_SPECIFICATION.md` - Temporary tables implementation
- `/docs/audit/parsers/COMPARISON_MATRIX.md` - Cross-parser feature comparison

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

### Phase 2: Post-Alpha
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
- Emit `EXT_FUNC_JSON_EXISTS` (extended opcode `0x0324`) with 2 arguments:
  1. JSON expression bytecode
  2. JSONPATH literal as `LITERAL_STRING` (UTF-8, raw path text)
- Any JSONPATH-specific functions must normalize to this opcode and argument order.

### Array Domains

**Emission rule:**
- When a domain is used as an array element type, emit:
  - `TYPE_ARRAY` (0xB9)
  - `TYPE_DOMAIN` (0xB8) + domain_id payload (as used elsewhere)
  - `array_dimensions` (u32, 0 if unspecified)

### Table-Level CHECK Constraints

**CREATE TABLE:**
- Emit `CHECK_CONSTRAINT` (0x92) followed by the check expression bytecode.

**ALTER TABLE ADD CONSTRAINT CHECK:**
- `ALTER_TABLE` opcode (0x21)
- qualified table name
- action byte `3` (ADD_CONSTRAINT)
- constraint type byte `3` (CHECK)
- constraint name string (empty if none)
- expression bytecode length (u32) + bytes

### CREATE DOMAIN Base Type Support

**Emission rule:**
- Emit `EXT_CREATE_DOMAIN` (0x5C) using the payload format defined in
  `docs/specifications/sblr/SBLR_DOMAIN_PAYLOADS.md` and
  `src/sblr/bytecode_generator_v2.cpp`:
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

**New action byte values (authoritative):**
- 15 = `ALTER_COLUMN_SET_DEFAULT`
- 16 = `ALTER_COLUMN_DROP_DEFAULT`
- 17 = `ALTER_COLUMN_SET_NOT_NULL`
- 18 = `ALTER_COLUMN_DROP_NOT_NULL`
- 19 = `ALTER_COLUMN_USING` (see below)

**Payload formats:**
- **SET DEFAULT**:
  - `ALTER_TABLE` opcode
  - qualified table name
  - action byte `15`
  - column name (string id)
  - expression bytecode length (u32) + bytes
- **DROP DEFAULT**:
  - `ALTER_TABLE` opcode
  - qualified table name
  - action byte `16`
  - column name
- **SET NOT NULL**:
  - `ALTER_TABLE` opcode
  - qualified table name
  - action byte `17`
  - column name
- **DROP NOT NULL**:
  - `ALTER_TABLE` opcode
  - qualified table name
  - action byte `18`
  - column name

### ALTER TABLE ALTER COLUMN ... USING

**Emission rule:**
- `ALTER_TABLE` opcode
- qualified table name
- action byte `19` (ALTER_COLUMN_USING)
- column name
- target type marker (u16 DataType enum)
- precision (u32) and scale (u32)
- USING expression bytecode length (u32) + bytes

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
  - For each USING column, emit `COALESCE(left.col_i, right.col_i)` via opcode `COALESCE` (0xF8).
  - Emit remaining non-USING columns from left then right.

### DEFAULT in Multi-Row INSERT

**Emission rule:**
- In each row value list, token `DEFAULT` emits opcode `DEFAULT_VALUE` (0x91)
  with no payload.
- Executor replaces `DEFAULT_VALUE` with column default for that column.

### MERGE USING Subqueries

**Emission rule:**
- Emit:
  1. `EXT_MERGE_START` (0x4F)
  2. `EXT_MERGE_SOURCE` (0x50)
     - `source_kind` byte: 0=table, 1=subquery
     - If table: qualified table name string
     - If subquery: `subquery_len` (u32) + subquery bytecode bytes
     - Source alias string (empty if none)
  3. `EXT_MERGE_ON` (0x51) + predicate bytecode
  4. `EXT_MERGE_WHEN_MATCHED` / `EXT_MERGE_WHEN_NOT_MATCHED` payloads per clause
  5. `EXT_MERGE_END` (0x55)

**End of Specification**
**Status:** ACTIVE - Critical Fixes Required for Alpha
**Next Steps:** Fix ARRAY type bug and bytecode format mismatches immediately
