# MySQL Parser Implementation Gaps and Remediation Specification (Non-Authoritative Reference)

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Status Note:** Historical gap analysis only. MUST NOT be used for V3 implementation.
Use `PARSER_TO_SBLR_EMISSION_RULES.md` and `findings/DIALECT_GAP_EXAMPLES.md` for
authoritative emission rules.

**Version:** 1.0  
**Date:** 2026-01-07  
**Status:** ACTIVE - Alpha Requirement  
**Priority:** CRITICAL - Must complete before Alpha release

## Scope and Dialect Isolation (Normative)

- This document applies **only** to the **MySQL emulated parser**.
- The ScratchBird core parser must not accept MySQL syntax.
- The engine must not parse SQL at runtime; it only executes SBLR.
- Any MySQL feature listed here that is not explicitly implemented MUST be
  rejected by the MySQL parser with a clear error (SQLSTATE `0A000` feature not supported).

## Resolution Policy (Normative)

- Any item marked **UNKNOWN** is **out-of-scope** until explicitly defined.
- Any item marked **Not Implemented** MUST be rejected with SQLSTATE `0A000`
  unless this document provides an explicit, authoritative emission rule.
- The MySQL parser MUST NOT accept syntax it cannot emit into valid SBLR.

## Authoritative Emission Rules (No Rejection)

All remaining gaps below MUST be emitted into valid SBLR. Use the same payload
schemas as the ScratchBird dialect unless explicitly stated.

### TEMPORARY TABLES
- Emit `SBLR3_CREATE_TABLE` with `flags` containing `TEMPORARY`.
- Use `SCHEMA_DDL_CREATE_TABLE` payload; all columns/constraints identical to ScratchBird dialect.

### ON DUPLICATE KEY UPDATE
- Emit `SBLR3_INSERT` with `on_conflict` payload:
  - `target_kind = 0` (NONE)
  - `action = UPDATE`
  - `set_items` from UPDATE clause
  - `action_where` from WHERE (if present)

### UNSIGNED / ZEROFILL
- Map to unsigned type markers:
  - INT UNSIGNED → `TYPE_UINT32`
  - BIGINT UNSIGNED → `TYPE_UINT64`
  - SMALLINT/TINYINT UNSIGNED → `TYPE_UINT16`/`TYPE_UINT8`
- ZEROFILL is **formatting-only**; no storage change. Record a table option:
  - `OPTION_KV` key `zerofill_columns` with a `LITERAL_STRING` CSV list of column names
    in column order (e.g., `"id,code"`).

### INSERT MODIFIERS (IGNORE)
- Map `INSERT IGNORE` to `SBLR3_INSERT` with `on_conflict.action = NOTHING`.

### CREATE INDEX (Stub)
- Emit `SBLR3_CREATE_INDEX` using `SCHEMA_DDL_CREATE_INDEX`:
  - index keys map MySQL key parts to `INDEX_KEY` entries.

### CREATE VIEW (Stub)
- Emit `SBLR3_CREATE_VIEW` using `SCHEMA_DDL_CREATE_VIEW` with the parsed SELECT as `query`.

### TABLE OPTIONS (ENGINE, CHARSET, COLLATE, COMMENT)
- Emit options into `SCHEMA_DDL_CREATE_TABLE.options` as `OPTION_KV` pairs.
- Catalog persists these options as table metadata; executor ignores unknown options.

### REPLACE INTO
- Emit `SBLR3_INSERT` with `on_conflict.action = UPDATE` and `set_items` mirroring
  the target column assignments (ScratchBird semantics).

---

## Executive Summary

This specification documents all parsed-but-not-implemented features in the MySQL emulated parser and provides concrete remediation strategies. The MySQL parser currently accepts many MySQL-specific syntax features but fails to implement them, resulting in **silent failures** where syntax is accepted but functionality doesn't work.

**Critical Finding:** The MySQL parser has **40-50% executor compatibility** due to bytecode format mismatches and missing implementations.

**Alpha Requirement:** Full MySQL compatibility support must be achieved before Alpha release.

---

## Table of Contents

1. [Critical Implementation Gaps](#critical-implementation-gaps)
2. [MySQL-Specific Features (Parsed but Not Implemented)](#mysql-specific-features-parsed-but-not-implemented)
3. [Remapping Opportunities](#remapping-opportunities)
4. [Implementation Roadmap](#implementation-roadmap)
5. [Testing Requirements](#testing-requirements)

---

## Critical Implementation Gaps

### 1. TEMPORARY TABLES

**Status:** ❌ **CRITICAL - Silent Failure**
**Impact:** Creates permanent tables instead of temporary
**File:** `src/parser/mysql/mysql_parser.cpp:2703`

#### Current Behavior

```cpp
void Parser::parseCreateTable() {
    matchKeyword(TokenType::KW_TEMPORARY);  // ❌ Parsed but DISCARDED

    consumeKeyword(TokenType::KW_TABLE, "Expected TABLE");
    // ... no temporary flag emitted to bytecode
}
```

**Result:** `CREATE TEMPORARY TABLE test (id INT)` creates a **permanent** table.

#### Remediation

**Option A: Full Implementation (RECOMMENDED)**
- Implement temporary table support across full stack (see FIREBIRD_V2_FEATURE_PARITY_SPECIFICATION.md)
- Estimated effort: 3-5 days
- Priority: **CRITICAL**

**Option B: Explicit Rejection (Temporary measure)**
```cpp
if (matchKeyword(TokenType::KW_TEMPORARY)) {
    error("TEMPORARY tables are not yet supported in MySQL parser");
}
```
- Estimated effort: 1 hour
- Priority: **IMMEDIATE** (until Option A is complete)

**Recommendation:** Implement Option B immediately, then pursue Option A for full support.

---

### 2. ON DUPLICATE KEY UPDATE

**Status:** ⚠️ **CRITICAL - Parsed but Disabled**
**Impact:** Upsert functionality does not work
**File:** `src/parser/mysql/mysql_parser.cpp:2130-2144`

#### Current Behavior

```cpp
if (matchKeyword(TokenType::KW_ON)) {
    consumeKeyword(TokenType::KW_DUPLICATE, "Expected DUPLICATE");
    consumeKeyword(TokenType::KW_KEY, "Expected KEY");
    consumeKeyword(TokenType::KW_UPDATE, "Expected UPDATE");

    bool prev_emit = emit_enabled_;
    emit_enabled_ = false;  // ❌ DISABLE EMISSION
    do {
        parseIdentifier();
        if (match(TokenType::EQUAL)) {
            parseExpression();
        }
    } while (match(TokenType::COMMA));
    emit_enabled_ = prev_emit;  // ❌ No bytecode emitted
}
```

**Result:** `INSERT ... ON DUPLICATE KEY UPDATE` syntax is accepted but **update logic is not executed**.

#### Remediation

**Option A: Remap to MERGE Statement (RECOMMENDED)**

MySQL `ON DUPLICATE KEY UPDATE` can be remapped to SQL standard MERGE:

```sql
-- MySQL syntax (current):
INSERT INTO users (id, name) VALUES (1, 'Alice')
ON DUPLICATE KEY UPDATE name = VALUES(name);

-- Remap to MERGE (target):
MERGE INTO users u
USING (SELECT 1 AS id, 'Alice' AS name) s
ON u.id = s.id
WHEN MATCHED THEN UPDATE SET name = s.name
WHEN NOT MATCHED THEN INSERT (id, name) VALUES (s.id, s.name);
```

**Implementation Steps:**
1. Enable bytecode emission for ON DUPLICATE KEY clause (remove `emit_enabled_ = false`)
2. Generate EXT_MERGE_* opcodes instead of INSERT
3. Translate VALUES() function to source table reference
4. Map to WHEN MATCHED UPDATE / WHEN NOT MATCHED INSERT

**Effort:** 2-3 days
**Priority:** **CRITICAL**

**Option B: Implement as PostgreSQL-style ON CONFLICT**

Already has some infrastructure in place:
- Line 2315: `emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ON_CONFLICT_DO_UPDATE));`

But needs completion:
1. Emit conflict columns
2. Emit UPDATE SET clause
3. Implement executor support for EXT_ON_CONFLICT_DO_UPDATE

**Effort:** 2-3 days
**Priority:** **CRITICAL**

**Recommendation:** Option A (MERGE) is preferred as it uses SQL standard and existing MERGE infrastructure.

---

### 3. UNSIGNED and ZEROFILL Type Modifiers

**Status:** ⚠️ **MEDIUM - Parsed but Not Emitted**
**Impact:** Column constraints not enforced
**File:** `src/parser/mysql/mysql_parser.cpp:3336-3342`

#### Current Behavior

```cpp
// Type modifiers
if (matchKeyword(TokenType::KW_UNSIGNED)) {
    type.unsigned_ = true;  // ✅ Stored in local struct
}
if (matchKeyword(TokenType::KW_ZEROFILL)) {
    type.zerofill = true;   // ✅ Stored in local struct
    type.unsigned_ = true;   // ZEROFILL implies UNSIGNED
}
// ... but emitTypeDefinition() does NOT emit these flags
```

**Result:** `INT UNSIGNED`, `DECIMAL(10,2) ZEROFILL` accepted but constraints not enforced.

#### Remediation

**Option A: Emit as CHECK Constraints (RECOMMENDED)**

UNSIGNED can be mapped to `CHECK (column >= 0)`:

```cpp
void Parser::emitTypeDefinition(const MySQLDataType& type) {
    emit(typeToOpcode(type.kind));
    // ... existing type emission

    // NEW: Emit UNSIGNED as CHECK constraint
    if (type.unsigned_) {
        // After column definition, add CHECK constraint
        // This should be done in parseColumnDef() after type
    }
}
```

**Implementation:**
1. After emitting type, check for `unsigned_` flag
2. Emit CHECK_CONSTRAINT opcode with `>= 0` expression
3. For ZEROFILL, also store formatting hint in column metadata

**Effort:** 1-2 days
**Priority:** **MEDIUM**

**Option B: Map to Storage Engine Type**

Store as separate column flag in catalog:
- Enforce range checks in executor
- Format output with zero-padding for ZEROFILL

**Effort:** 3-4 days
**Priority:** **MEDIUM**

**Recommendation:** Option A (CHECK constraint) for quick implementation; Option B for complete MySQL compatibility.

---

### 4. INSERT Statement Modifiers

**Status:** ⚠️ **LOW - Parsed but Discarded**
**Impact:** Query hints not applied
**File:** `src/parser/mysql/mysql_parser.cpp:1982-1985`

#### Current Behavior

```cpp
void Parser::parseInsertStmt() {
    consume(TokenType::KW_INSERT, "Expected INSERT");

    // Optional modifiers
    matchKeyword(TokenType::KW_LOW_PRIORITY);  // ❌ Parsed but discarded
    matchKeyword(TokenType::KW_DELAYED);        // ❌ Parsed but discarded
    matchKeyword(TokenType::KW_HIGH_PRIORITY); // ❌ Parsed but discarded
    matchKeyword(TokenType::KW_IGNORE);         // ❌ Parsed but discarded

    consumeKeyword(TokenType::KW_INTO, "Expected INTO");
    // ... no modifiers emitted
}
```

**MySQL Modifier Semantics:**
- `LOW_PRIORITY`: Wait for no concurrent reads
- `HIGH_PRIORITY`: Jump read queue (MyISAM only)
- `DELAYED`: Asynchronous insert (deprecated in MySQL 5.6)
- `IGNORE`: Suppress duplicate key errors

**Result:** All modifiers accepted but ignored.

#### Remediation

**Option A: Document as Unsupported (RECOMMENDED)**

Most MySQL modifiers are MyISAM-specific and deprecated:

```cpp
if (matchKeyword(TokenType::KW_LOW_PRIORITY) ||
    matchKeyword(TokenType::KW_HIGH_PRIORITY) ||
    matchKeyword(TokenType::KW_DELAYED)) {
    // Warn but don't fail
    warning("INSERT modifiers (LOW_PRIORITY/HIGH_PRIORITY/DELAYED) are ignored in ScratchBird");
}

if (matchKeyword(TokenType::KW_IGNORE)) {
    // Can implement as INSERT ... ON CONFLICT DO NOTHING
    emit_on_conflict_do_nothing = true;
}
```

**Effort:** 1 day
**Priority:** **LOW**

**Recommendation:** Document modifiers as unsupported. Only implement `IGNORE` as it maps to standard ON CONFLICT behavior.

---

### 5. CREATE INDEX (Stub)

**Status:** ❌ **CRITICAL - Not Implemented**
**Impact:** Index creation fails silently
**File:** `src/parser/mysql/mysql_parser.cpp:3439`

#### Current Behavior

```cpp
void Parser::parseCreateIndex() {
    // Required behavior when MySQL index DDL is not enabled:
    // emit ERR_FEATURE_UNSUPPORTED and abort parsing.
    raiseError("ERR_FEATURE_UNSUPPORTED", "MySQL CREATE INDEX is not enabled");
}
```

**Result:** `CREATE INDEX idx ON table (col)` is **NOT PARSED** at all.

#### Remediation

**Option A: Implement MySQL Index Syntax (RECOMMENDED)**

MySQL index syntax:

```sql
CREATE [UNIQUE] INDEX index_name
    ON table_name (column_list)
    [USING {BTREE | HASH}]
    [COMMENT 'string']
```

**Implementation:**
```cpp
void Parser::parseCreateIndex() {
    bool unique = false;
    if (matchKeyword(TokenType::KW_UNIQUE)) {
        unique = true;
    }

    std::string index_name = parseIdentifier();

    consumeKeyword(TokenType::KW_ON, "Expected ON");

    std::string table = parseIdentifier();
    // ... parse column list

    emit(sblr::Opcode::CREATE_INDEX);
    emitString(index_name);
    emitString(table);
    // ... emit columns
}
```

**Effort:** 1-2 days
**Priority:** **CRITICAL**

**Recommendation:** Implement immediately. Required for MySQL compatibility.

---

### 6. CREATE VIEW (Stub)

**Status:** ❌ **CRITICAL - Not Implemented**
**Impact:** View creation fails silently
**File:** `src/parser/mysql/mysql_parser.cpp:3443`

#### Current Behavior

```cpp
void Parser::parseCreateView() {
    // Required behavior when MySQL view DDL is not enabled:
    // emit ERR_FEATURE_UNSUPPORTED and abort parsing.
    raiseError("ERR_FEATURE_UNSUPPORTED", "MySQL CREATE VIEW is not enabled");
}
```

**Result:** `CREATE VIEW v AS SELECT * FROM t` is **NOT PARSED** at all.

#### Remediation

**Option A: Implement MySQL View Syntax (RECOMMENDED)**

MySQL view syntax:

```sql
CREATE [OR REPLACE] VIEW view_name [(column_list)]
    AS select_statement
    [WITH [CASCADED | LOCAL] CHECK OPTION]
```

**Implementation:**
```cpp
void Parser::parseCreateView() {
    bool or_replace = false;
    if (matchKeyword(TokenType::KW_OR)) {
        consumeKeyword(TokenType::KW_REPLACE, "Expected REPLACE");
        or_replace = true;
    }

    std::string view_name = parseIdentifier();

    // Optional column list
    std::vector<std::string> columns;
    if (match(TokenType::LEFT_PAREN)) {
        do {
            columns.push_back(parseIdentifier());
        } while (match(TokenType::COMMA));
        consume(TokenType::RIGHT_PAREN, "Expected )");
    }

    consumeKeyword(TokenType::KW_AS, "Expected AS");

    // Parse SELECT statement
    emit(sblr::Opcode::CREATE_VIEW);
    emitByte(or_replace ? 0x01 : 0x00);
    emitString(view_name);
    // ... emit SELECT bytecode
}
```

**Effort:** 2-3 days
**Priority:** **CRITICAL**

**Recommendation:** Implement immediately. Required for MySQL compatibility.

---

## MySQL-Specific Features (Parsed but Not Implemented)

### 7. Table Options

**Status:** ⚠️ **UNKNOWN - Needs Verification**
**Impact:** Table metadata may not be stored
**File:** `src/parser/mysql/mysql_parser.cpp:2858-3000`

#### Parsed Table Options

The parser accepts 20+ MySQL table options:

```cpp
// Examples of parsed options:
if (matchKeyword(TokenType::KW_ENGINE)) {
    // Parse ENGINE=InnoDB
}
if (matchKeyword(TokenType::KW_AUTO_INCREMENT)) {
    // Parse AUTO_INCREMENT=1000
}
if (matchKeyword(TokenType::KW_CHARSET)) {
    // Parse CHARSET=utf8mb4
}
if (matchKeyword(TokenType::KW_COLLATE)) {
    // Parse COLLATE=utf8mb4_unicode_ci
}
if (matchKeyword(TokenType::KW_COMMENT)) {
    // Parse COMMENT='description'
}
// ... and 15+ more options
```

#### Remediation Required

**Investigation Needed:**
1. Check if these options are emitted to bytecode
2. Check if catalog stores them
3. Check if executor honors them

**Recommendation:**
- **ENGINE**: Document as "always uses ScratchBird storage engine"
- **CHARSET/COLLATE**: Map to ScratchBird collation system if available
- **COMMENT**: Store in catalog table metadata
- **AUTO_INCREMENT**: Already working (line 2793-2796)
- **ROW_FORMAT**: Document as unsupported (ScratchBird-specific storage)

**Priority:** **MEDIUM** - Audit required

---

### 8. REPLACE INTO

**Status:** ✅ **PARTIAL - Remapped to ON CONFLICT**
**Impact:** Works but uses PostgreSQL semantics
**File:** `src/parser/mysql/mysql_parser.cpp:2303-2325`

#### Current Behavior

```cpp
void Parser::parseReplaceStmt() {
    consume(TokenType::KW_REPLACE, "Expected REPLACE");

    // ... parse modifiers

    emit(sblr::Opcode::INSERT);
    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ON_CONFLICT_DO_UPDATE));

    // ... parse table and values
}
```

**MySQL REPLACE Semantics:**
1. Try INSERT
2. If duplicate key, DELETE old row
3. Then INSERT new row

**Current Implementation:** Maps to ON CONFLICT DO UPDATE (PostgreSQL semantics)

#### Remediation

**Option A: Keep Current Mapping (RECOMMENDED)**

ON CONFLICT DO UPDATE is functionally equivalent to REPLACE for most use cases. Document the difference:

```
MySQL REPLACE:
  - DELETE old row (two operations)
  - Old row's OID changes
  - Triggers fire for both DELETE and INSERT

ScratchBird REPLACE (mapped to ON CONFLICT):
  - UPDATE old row (one operation)
  - Same row OID preserved
  - Only UPDATE triggers fire
```

**Effort:** Documentation only
**Priority:** **LOW**

**Option B: Implement True REPLACE Semantics**

Requires separate code path:
1. Emit EXT_REPLACE opcode
2. Executor performs DELETE + INSERT
3. Fire appropriate triggers

**Effort:** 3-4 days
**Priority:** **LOW**

**Recommendation:** Option A (document difference). True REPLACE semantics rarely matter in practice.

---

## Remapping Opportunities

### Summary of Remapping Strategies

| MySQL Feature | Remap To | Effort | Priority |
|---------------|----------|--------|----------|
| **ON DUPLICATE KEY UPDATE** | MERGE statement | 2-3 days | **CRITICAL** |
| **REPLACE INTO** | ON CONFLICT (current) | 0 days | **COMPLETE** |
| **UNSIGNED** | CHECK constraint | 1-2 days | **MEDIUM** |
| **INSERT IGNORE** | ON CONFLICT DO NOTHING | 1 day | **MEDIUM** |
| **AUTO_INCREMENT** | IDENTITY_COLUMN (current) | 0 days | **COMPLETE** |
| **ZEROFILL** | Column metadata + formatter | 2-3 days | **LOW** |

---

## Implementation Roadmap

### Phase 1: Critical Fixes (Alpha Blocker) - 5-7 days

**Must complete before Alpha release:**

1. **TEMPORARY TABLES** (Option B: Reject with error) - 1 hour
   - Add error message rejecting TEMPORARY syntax
   - Document as "not yet supported"

2. **ON DUPLICATE KEY UPDATE** (Remap to MERGE) - 2-3 days
   - Generate MERGE bytecode instead of INSERT
   - Translate VALUES() references
   - Test with executor

3. **CREATE INDEX** - 1-2 days
   - Implement full parser
   - Emit CREATE_INDEX opcode
   - Test index creation

4. **CREATE VIEW** - 2-3 days
   - Implement full parser
   - Emit CREATE_VIEW opcode
   - Test view creation

**Total:** 5-7 days

---

### Phase 2: Important Features (optional extension) - 3-4 days

5. **UNSIGNED constraints** - 1-2 days
   - Emit CHECK constraints for UNSIGNED
   - Test range validation

6. **INSERT IGNORE** - 1 day
   - Map to ON CONFLICT DO NOTHING
   - Test conflict handling

7. **Table Options Audit** - 1 day
   - Verify CHARSET/COLLATE/COMMENT storage
   - Document unsupported options

**Total:** 3-4 days

---

### Phase 3: Complete Implementation (Beta Target) - 10-15 days

8. **TEMPORARY TABLES** (Full implementation) - 3-5 days
   - See FIREBIRD_V2_FEATURE_PARITY_SPECIFICATION.md
   - Implement full stack support

9. **ZEROFILL** - 2-3 days
   - Add column metadata storage
   - Implement output formatter

10. **Advanced Index Types** - 3-5 days
    - FULLTEXT indexes
    - SPATIAL indexes
    - Hash vs BTree differentiation

11. **Stored Procedures** - 2-3 days
    - CREATE PROCEDURE parsing
    - Map to SBLR procedural opcodes

**Total:** 10-15 days

---

## Testing Requirements

### Phase 1 Tests (Alpha)

```sql
-- Test 1: TEMPORARY TABLE rejection
CREATE TEMPORARY TABLE test (id INT);
-- Expected: Error with clear message

-- Test 2: ON DUPLICATE KEY UPDATE → MERGE
CREATE TABLE users (id INT PRIMARY KEY, name VARCHAR(50));
INSERT INTO users VALUES (1, 'Alice') ON DUPLICATE KEY UPDATE name = VALUES(name);
INSERT INTO users VALUES (1, 'Bob') ON DUPLICATE KEY UPDATE name = VALUES(name);
SELECT * FROM users;
-- Expected: (1, 'Bob')

-- Test 3: CREATE INDEX
CREATE INDEX idx_name ON users (name);
SHOW INDEXES FROM users;
-- Expected: idx_name listed

-- Test 4: CREATE VIEW
CREATE VIEW active_users AS SELECT * FROM users WHERE name IS NOT NULL;
SELECT * FROM active_users;
-- Expected: View works
```

### Phase 2 Tests

```sql
-- Test 5: UNSIGNED constraint
CREATE TABLE counts (id INT UNSIGNED);
INSERT INTO counts VALUES (-1);
-- Expected: Error (CHECK constraint violation)

-- Test 6: INSERT IGNORE
INSERT IGNORE INTO users VALUES (1, 'Charlie');
-- Expected: No error, no insert (conflict ignored)

-- Test 7: Table options
CREATE TABLE t (id INT) COMMENT='Test table' CHARSET=utf8mb4;
SHOW CREATE TABLE t;
-- Expected: Comment visible
```

---

## Compliance Matrix

| Feature | Parsed | Bytecode | Executor | Status | Priority |
|---------|--------|----------|----------|--------|----------|
| **DDL** |
| CREATE INDEX | ❌ | ❌ | ❌ | **STUB** | **CRITICAL** |
| CREATE VIEW | ❌ | ❌ | ❌ | **STUB** | **CRITICAL** |
| CREATE TEMPORARY TABLE | ✅ | ❌ | ❌ | **PARSED/IGNORED** | **CRITICAL** |
| **DML** |
| ON DUPLICATE KEY UPDATE | ✅ | ⚠️ DISABLED | ❌ | **PARSED/DISABLED** | **CRITICAL** |
| REPLACE INTO | ✅ | ✅ | ✅ | **REMAPPED** | **COMPLETE** |
| INSERT IGNORE | ✅ | ❌ | ❌ | **PARSED/IGNORED** | **MEDIUM** |
| **Type Modifiers** |
| UNSIGNED | ✅ | ❌ | ❌ | **BLOCKED (SBLR UINT* markers)** | **MEDIUM** |
| ZEROFILL | ✅ | ❌ | ❌ | **PARSED/IGNORED** | **LOW** |
| AUTO_INCREMENT | ✅ | ✅ | ✅ | **WORKS** | **COMPLETE** |
| **Types** |
| MULTI* geometry (MULTIPOINT/MULTILINESTRING/MULTIPOLYGON/GEOMETRYCOLLECTION) | ❌ | ❌ | ❌ | **BLOCKED (SBLR type marker)** | **ALPHA** |
| **Table Options** |
| ENGINE | ✅ | ❓ | ❓ | **UNKNOWN** | **MEDIUM** |
| CHARSET/COLLATE | ✅ | ❓ | ❓ | **UNKNOWN** | **MEDIUM** |
| COMMENT | ✅ | ❓ | ❓ | **UNKNOWN** | **MEDIUM** |

---

## SBLR Emission Examples (Full Payload Bytes)

Encoding rules: little-endian, `string = [len:varuint][utf8]`, `schema_path = [count][ident...]`, `list<T> = [count][T...]`.

### TEMPORARY TABLE (CREATE TEMPORARY TABLE t (id INT))
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

### ON DUPLICATE KEY UPDATE
Opcode: `SBLR3_INSERT`

Example: `INSERT INTO users(id,name) VALUES (1,'Alice') ON DUPLICATE KEY UPDATE name='Alice'`

Payload bytes (hex, field order):
```
target            = 01 05 75 73 65 72 73
alias.opt         = 00
columns.count     = 02 02 69 64 04 6E 61 6D 65
source            = 01
values.count      = 01
expr_list.count   = 02
expr1 (INT32)     = 0F 0C 00 00 04 00 00 00 01 00 00 00
expr2 (STRING)    = 13 0C 00 00 06 00 00 00 05 41 6C 69 63 65
select.opt        = 00
on_conflict.opt   = 01
on_conflict       = 00 00 00 00 02 01 04 6E 61 6D 65 13 0C 00 00 06 00 00 00 05 41 6C 69 63 65 00
returning.count   = 00
```

### INSERT IGNORE
Opcode: `SBLR3_INSERT` with `on_conflict.action = NOTHING`

Example: `INSERT IGNORE INTO t(id) VALUES (1)`

Payload bytes (hex, field order):
```
target            = 01 01 74
alias.opt         = 00
columns.count     = 01 02 69 64
source            = 01
values.count      = 01
expr_list.count   = 01
expr1 (INT32)     = 0F 0C 00 00 04 00 00 00 01 00 00 00
select.opt        = 00
on_conflict.opt   = 01
on_conflict       = 00 00 00 00 01 00 00
returning.count   = 00
```

### UNSIGNED TYPE (CREATE TABLE t (id INT UNSIGNED))
Opcode: `SBLR3_CREATE_TABLE`

Payload bytes (hex, field order):
```
flags             = 00 00
path              = 01 01 74
columns.count     = 01
column_def        = 02 69 64 2B 0B 00 00 00 00 00 00 00 00 00 00
constraints.count = 00
inherits.count    = 00
partitioning.opt  = 00
tablespace.opt    = 00
options.count     = 00
```

### CREATE INDEX (CREATE INDEX idx ON t(id))
Opcode: `SBLR3_CREATE_INDEX`

Payload bytes (hex, field order):
```
flags             = 00 00
index_path        = 01 03 69 64 78
table             = 01 01 74
keys.count        = 01
key               = 01 04 06 00 00 04 00 00 00 00 02 69 64 00 00 00 00
include.count     = 00
predicate.opt     = 00
index_type.opt    = 00
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

### TABLE OPTIONS (ENGINE=InnoDB)
Opcode: `SBLR3_CREATE_TABLE` with `options:OPTION_KV`

Payload bytes (hex, field order, options only):
```
options.count     = 01
key               = 06 45 4E 47 49 4E 45
value (STRING)    = 13 0C 00 00 07 00 00 00 06 49 6E 6E 6F 44 42
```

### TABLE OPTIONS (CHARSET/COLLATE/COMMENT)
Opcode: `SBLR3_CREATE_TABLE` with `options:OPTION_KV`

Example: `CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Test'`

Payload bytes (hex, field order, options only):
```
options.count     = 03
key1              = 07 43 48 41 52 53 45 54
val1 (STRING)     = 13 0C 00 00 08 00 00 00 07 75 74 66 38 6D 62 34
key2              = 07 43 4F 4C 4C 41 54 45
val2 (STRING)     = 13 0C 00 00 13 00 00 00 12 75 74 66 38 6D 62 34 5F 30 39 30 30 5F 61 69 5F 63 69
key3              = 07 43 4F 4D 4D 45 4E 54
val3 (STRING)     = 13 0C 00 00 05 00 00 00 04 54 65 73 74
```

### REPLACE INTO
Opcode: `SBLR3_INSERT` with `on_conflict.action = UPDATE`

Example: `REPLACE INTO users(id,name) VALUES (1,'Alice')`

Payload bytes (hex, field order):
```
target            = 01 05 75 73 65 72 73
alias.opt         = 00
columns.count     = 02 02 69 64 04 6E 61 6D 65
source            = 01
values.count      = 01
expr_list.count   = 02
expr1 (INT32)     = 0F 0C 00 00 04 00 00 00 01 00 00 00
expr2 (STRING)    = 13 0C 00 00 06 00 00 00 05 41 6C 69 63 65
select.opt        = 00
on_conflict.opt   = 01
on_conflict       = 00 00 00 00 02 01 04 6E 61 6D 65 13 0C 00 00 06 00 00 00 05 41 6C 69 63 65 00
returning.count   = 00
```

### WINDOW FRAME OFFSETS (WINDOW_SPEC)
Opcode: `SBLR3_WINDOW_SPEC`

Example: `OVER (ROWS BETWEEN 3 PRECEDING AND 2 FOLLOWING)`

Payload bytes (hex, field order):
```
partition_by.count = 00
order_by.count     = 00
frame.present      = 01
frame.type         = 01
start.type         = 02
start.offset.present = 01
start.offset.expr  = 0F 0C 00 00 04 00 00 00 03 00 00 00
end.present        = 01
end.type           = 04
end.offset.present = 01
end.offset.expr    = 0F 0C 00 00 04 00 00 00 02 00 00 00
```

### NAMED WINDOWS (INLINE RESOLUTION)
Opcode: `SBLR3_WINDOW_SPEC`

Example: `WINDOW w AS (PARTITION BY id) ... OVER (w)`

Emission: named windows are **inlined**. The emitted payload is the resolved
`WINDOW_SPEC` for the `OVER (w)` call.

Payload bytes (hex, field order):
```
partition_by.count = 01
partition_by.expr  = 04 06 00 00 04 00 00 00 00 02 69 64
order_by.count     = 00
frame.present      = 00
```

### DEFAULT IN MULTI-ROW INSERT/REPLACE
Opcode: `SBLR3_INSERT`

Example: `INSERT INTO t(id) VALUES (DEFAULT), (1)`

Payload bytes (hex, field order):
```
target            = 01 01 74
alias.opt         = 00
columns.count     = 01 02 69 64
source            = 01
values.count      = 02
row1.count        = 01
row1.default      = 10 01 00 00 00 00 00 00
row2.count        = 01
row2.int32        = 0F 0C 00 00 04 00 00 00 01 00 00 00
select.opt        = 00
on_conflict.opt   = 00
returning.count   = 00
```

### ALTER TABLE CHANGE COLUMN (RENAME + TYPE)
Opcode: `SBLR3_ALTER_TABLE` (two actions)

Example: `ALTER TABLE t CHANGE COLUMN a b INT`

Payload bytes (hex, field order):
```
rename.payload    = 01 01 74 00 00 0F 01 61 01 62
type.payload      = 01 01 74 00 00 05 01 62 3F 0B 00 00
```

### ALTER TABLE SET/DROP DEFAULT
Opcode: `SBLR3_ALTER_TABLE`

Example: `ALTER TABLE t ALTER COLUMN b SET DEFAULT 1`

Payload bytes (hex, field order):
```
set_default       = 01 01 74 00 00 07 01 62 0F 0C 00 00 04 00 00 00 01 00 00 00
drop_default      = 01 01 74 00 00 08 01 62
```

### GRANT/REVOKE ON ALL TABLES IN SCHEMA
Opcode: `SBLR3_GRANT` / `SBLR3_REVOKE`

Example: `GRANT SELECT ON ALL TABLES IN SCHEMA db TO PUBLIC`

Payload bytes (hex, field order):
```
grant.payload     = 01 06 53 45 4C 45 43 54 01 01 02 02 64 62 01 2A 00 01 00
revoke.payload    = 01 06 53 45 4C 45 43 54 01 01 02 02 64 62 01 2A 00 01 00 00
```

### ZEROFILL COLUMN LIST (OPTION_KV)
Opcode: `SBLR3_CREATE_TABLE` with `options:OPTION_KV`

Example: `CREATE TABLE t (id INT ZEROFILL) ZEROFILL_COLUMNS='id'`

Payload bytes (hex, field order, options only):
```
options.count     = 01
key               = 10 7A 65 72 6F 66 69 6C 6C 5F 63 6F 6C 75 6D 6E 73
value (STRING)    = 13 0C 00 00 03 00 00 00 02 69 64
```

---

## References

### Project Documentation
- `/docs/specifications/parser/v3/audit/parsers/MySQL/SUMMARY.md` - MySQL parser audit
- `/docs/specifications/parser/v3/audit/20_mysql_parser_correction_plan_checklist.md` - Bytecode format fixes
- `/docs/specifications/parser/v3/FIREBIRD_V2_FEATURE_PARITY_SPECIFICATION.md` - Temporary tables implementation
- `/docs/specifications/parser/v3/audit/parsers/COMPARISON_MATRIX.md` - Cross-parser feature comparison

### MySQL Documentation
- [MySQL 8.0 Reference Manual - INSERT Statement](https://dev.mysql.com/doc/refman/8.0/en/insert.html)
- [MySQL 8.0 Reference Manual - REPLACE Statement](https://dev.mysql.com/doc/refman/8.0/en/replace.html)
- [MySQL 8.0 Reference Manual - CREATE INDEX](https://dev.mysql.com/doc/refman/8.0/en/create-index.html)
- [MySQL 8.0 Reference Manual - Data Types](https://dev.mysql.com/doc/refman/8.0/en/data-types.html)

---

## Implementation Checklist

### Phase 1: Alpha Blockers (CRITICAL)
- [ ] Add TEMPORARY TABLE rejection with clear error message
- [ ] Implement ON DUPLICATE KEY UPDATE → MERGE remapping
  - [ ] Enable bytecode emission (remove emit_enabled_ = false)
  - [ ] Generate MERGE opcodes
  - [ ] Translate VALUES() function
  - [ ] Test with executor
- [ ] Implement CREATE INDEX parser
  - [ ] Parse index name and column list
  - [ ] Emit CREATE_INDEX opcode
  - [ ] Test index creation
- [ ] Implement CREATE VIEW parser
  - [ ] Parse view definition
  - [ ] Emit CREATE_VIEW opcode
  - [ ] Test view creation and queries
- [ ] Update executor bytecode format per audit document 20

### Phase 2: optional extension (HIGH)
- [ ] Implement UNSIGNED as CHECK constraint
- [ ] Implement INSERT IGNORE as ON CONFLICT DO NOTHING
- [ ] Audit table options bytecode emission
- [ ] Document unsupported table options

### Phase 3: Beta Target (MEDIUM)
- [ ] Implement temporary tables (full stack)
- [ ] Implement ZEROFILL formatting
- [ ] Implement remaining core index types
- [ ] Implement stored procedure parsing

---

## Bytecode Emission Rules (Alpha-Required, No Ambiguity)

This section defines exact bytecode emission rules for the remaining MySQL
parity gaps. It is **authoritative** for implementation.

### Window Frame Offsets

**Emission rule:**
- Emit `SBLR3_WINDOW` once per SELECT that contains any window functions.
- For each window spec, emit `SBLR3_WINDOW_SPEC` with:
  - `partition_by` list (count + expr bytecode for each)
  - `order_by` list (count + sort key expr + ASC/DESC)
  - `frame` clause via `SBLR3_FRAME_ROWS` / `SBLR3_FRAME_RANGE`:
    - start boundary `SBLR3_FRAME_UNBOUNDED_PRECEDING`, `SBLR3_FRAME_PRECEDING`
      (with `SBLR3_LITERAL_INT32`), or `SBLR3_FRAME_CURRENT_ROW`
    - end boundary `SBLR3_FRAME_CURRENT_ROW`, `SBLR3_FRAME_FOLLOWING`
      (with `SBLR3_LITERAL_INT32`), or `SBLR3_FRAME_UNBOUNDED_FOLLOWING`

### Named Windows

**Emission rule:**
- Named windows in `WINDOW <name> AS (...)` are **resolved and inlined** at
  parse time.
- The SBLR V3 window model carries **no window name**; each window function
  emits a `WINDOW_SPEC` that includes the fully-resolved `partition_by`,
  `order_by`, and `frame` clauses.
- Any chained window reference (`WINDOW w2 AS (w1 ...)`) must be flattened into
  a single resolved `WINDOW_SPEC` before emission.

### DEFAULT in Multi-Row INSERT/REPLACE

**Emission rule:**
- In each row value list, the token `DEFAULT` emits opcode `SBLR3_DEFAULT_VALUE`
  with no payload.
- Executor replaces `DEFAULT_VALUE` with the column default for that column.

### ALTER TABLE CHANGE COLUMN (Rename + Type)

**Emission rule for CHANGE COLUMN:**
- Emit **two** `ALTER_TABLE` statements in order:
  1. `RENAME_COLUMN` (action `15`) with old/new names.
  2. `ALTER_COLUMN_TYPE` (action `5`) with the new type specification.

### ALTER TABLE ALTER COLUMN SET/DROP DEFAULT

**Use SBLR V3 action bytes:**
- 7 = `ALTER_COLUMN_SET_DEFAULT`
- 8 = `ALTER_COLUMN_DROP_DEFAULT`

**Emission rule:**
- Same payload formats as defined in the PostgreSQL bytecode rules section.

### GRANT/REVOKE ON ALL Bytecode

**Emission rule:**
- Use `SBLR3_GRANT` / `SBLR3_REVOKE` payloads:
  - `privileges:list<ident>` (e.g., `SELECT`, `INSERT`)
  - `object_type:u8` (1=TABLE, 9=SCHEMA, etc.)
  - `objects:list<schema_path>`
  - `grantees:list<ident>`
  - `is_public:bool`
  - `with_grant_option:bool` / `grant_option_for:bool`

**ON ALL mapping:**
- `ON ALL TABLES IN SCHEMA db` → object_type=TABLE, object path `db.*`
- `ON ALL TABLES` (current database) → object_type=TABLE, object path `*`

### Named Windows + Frame Offsets Validation

**Validation rule:**
- Named windows must resolve to an existing definition.
- Frame offsets must be non-negative integers; emit parser error otherwise.

**End of Specification**
**Status:** ACTIVE - Implementation Required for Alpha
**Next Steps:** Begin Phase 1 implementation immediately
