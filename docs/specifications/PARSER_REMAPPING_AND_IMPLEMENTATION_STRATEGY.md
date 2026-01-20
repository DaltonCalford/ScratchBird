# Parser Feature Remapping and Implementation Strategy

**Version:** 1.0
**Date:** 2026-01-07
**Status:** ACTIVE - Alpha Planning
**Purpose:** Decision framework for implementing vs remapping emulated parser features

**WAL Scope:** ScratchBird does not use write-after log (WAL) for recovery in Alpha; any WAL support is optional post-gold (replication/PITR).
Any WAL references in this document describe an optional post-gold stream for
replication/PITR only.

---

## Executive Summary

This document provides strategic guidance for handling features in emulated database parsers (MySQL, PostgreSQL, Firebird). For each feature category, we define whether to:
1. **IMPLEMENT** - Create full native support in SBLR
2. **REMAP** - Translate to existing SBLR functionality
3. **REJECT** - Explicitly not support with clear error
4. **DOCUMENT** - Accept but document limitations

**Guiding Principle:** Maximize compatibility with minimum implementation effort. Prefer remapping to existing SBLR features when semantically equivalent.

---

## Decision Framework

### Implementation Decision Matrix

| Criteria | IMPLEMENT | REMAP | REJECT | DOCUMENT |
|----------|-----------|-------|--------|----------|
| **Feature Criticality** | High/Critical | Medium | Low | Low |
| **Semantic Equivalence** | None available | Exists in SBLR | N/A | Exists with limitations |
| **Implementation Cost** | Justified by need | Too high | Too high | N/A |
| **Alpha Requirement** | Yes | Yes (as remap) | No | No |
| **Breaking Compatibility** | No | No | Yes (explicit) | No (documented) |

---

## Table of Contents

1. [IMPLEMENT - Full Native Support](#implement---full-native-support)
2. [REMAP - Translate to Existing SBLR](#remap---translate-to-existing-sblr)
3. [REJECT - Explicitly Not Support](#reject---explicitly-not-support)
4. [DOCUMENT - Accept with Limitations](#document---accept-with-limitations)
5. [Alpha Release Roadmap](#alpha-release-roadmap)

---

## IMPLEMENT - Full Native Support

These features require full implementation across the stack (Parser → AST → Bytecode → Executor → Catalog).

### 1. TEMPORARY TABLES (ALL PARSERS)

**Decision:** ✅ **IMPLEMENT** (Beta target)
**Workaround for Alpha:** ❌ **REJECT** with error

**Justification:**
- **Criticality:** CRITICAL - Core database feature
- **All 4 parsers** currently accept syntax but fail silently
- **No semantic equivalent** in existing SBLR
- MySQL, PostgreSQL, Firebird all have temporary tables
- Applications depend on this feature

**Implementation Scope:**
- AST: Add `TemporaryTableType` and `OnCommitAction` enums
- Bytecode: Extend CREATE_TABLE opcode payload with temp flags
- Catalog: Store temp metadata, track by session/transaction ID
- Executor: Implement visibility isolation and cleanup
- Session: Track temp tables per connection
- Transaction: Implement ON COMMIT semantics

**Effort:** 3-5 days
**Priority:** **BETA** (reject for Alpha, implement for Beta)

**Alpha Workaround:**
```cpp
if (matchKeyword(TokenType::KW_TEMPORARY)) {
    error("TEMPORARY tables not yet supported. Will be available in Beta release.");
}
```

**References:**
- FIREBIRD_V2_FEATURE_PARITY_SPECIFICATION.md - Section "Parsed But Not Implemented Features"
- Implementation plan with AST, bytecode, catalog, executor details

---

### 2. CREATE INDEX (MySQL Parser)

**Decision:** ✅ **IMPLEMENT** immediately

**Justification:**
- **Criticality:** CRITICAL - Required for MySQL compatibility
- Currently a **stub** (not implemented at all)
- Basic database feature
- No semantic difference from other parsers

**Implementation Scope:**
- Parse MySQL CREATE INDEX syntax
- Emit CREATE_INDEX opcode (already exists)
- Map USING BTREE/HASH to index types

**Effort:** 1-2 days
**Priority:** **ALPHA** (blocker)

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

    consume(TokenType::LEFT_PAREN, "Expected (");
    std::vector<std::string> columns;
    do {
        columns.push_back(parseIdentifier());
    } while (match(TokenType::COMMA));
    consume(TokenType::RIGHT_PAREN, "Expected )");

    // Optional: USING {BTREE | HASH}
    std::string index_type = "BTREE";  // default
    if (matchKeyword(TokenType::KW_USING)) {
        // Parse index type
    }

    emit(sblr::Opcode::CREATE_INDEX);
    emitString(index_name);
    emitString(table);
    // ... emit columns
}
```

---

### 3. CREATE VIEW (MySQL Parser)

**Decision:** ✅ **IMPLEMENT** immediately

**Justification:**
- **Criticality:** CRITICAL - Required for MySQL compatibility
- Currently a **stub** (not implemented at all)
- Basic database feature
- CREATE VIEW opcode already exists

**Implementation Scope:**
- Parse MySQL CREATE VIEW syntax
- Emit CREATE_VIEW opcode
- Handle OR REPLACE variant

**Effort:** 2-3 days
**Priority:** **ALPHA** (blocker)

---

### 4. ARRAY Type Support (PostgreSQL Parser)

**Decision:** ✅ **FIX CRITICAL BUG** immediately

**Justification:**
- **Criticality:** CRITICAL BUG - ARRAY types currently broken
- PostgreSQL arrays are fundamental data type
- Bug causes silent data corruption (ARRAY stored as VARCHAR)
- TYPE_ARRAY opcode already exists

**Implementation Scope:**
- Fix `typeToOpcode()` to return TYPE_ARRAY
- Track element type in PgDataType
- Emit array metadata (dimensions, element type)

**Effort:** 1 day
**Priority:** **ALPHA** (critical bug fix)

**Implementation:**
```cpp
// In pg_parser.h - Add fields:
struct PgDataType {
    Kind kind;
    PgDataType* element_type = nullptr;  // NEW: For arrays
    int array_dimensions = 0;             // NEW: Number of dimensions
    // ... existing fields
};

// In pg_parser.cpp - Fix typeToOpcode():
sblr::Opcode Parser::typeToOpcode(PgDataType::Kind kind) {
    switch (kind) {
        // ... existing cases
        case PgDataType::Kind::ARRAY:  // ADD THIS
            return sblr::Opcode::TYPE_ARRAY;
        default:
            return sblr::Opcode::TYPE_VARCHAR;
    }
}

// In pg_parser.cpp - Update emitTypeDefinition():
void Parser::emitTypeDefinition(const PgDataType& type) {
    if (type.kind == PgDataType::Kind::ARRAY) {
        emit(sblr::Opcode::TYPE_ARRAY);
        // Emit element type
        if (type.element_type) {
            emit(typeToOpcode(type.element_type->kind));
        }
        emitU32(type.array_dimensions);
        return;
    }
    // ... existing code
}
```

---

## REMAP - Translate to Existing SBLR

These features can be mapped to existing SBLR functionality without new implementation.

### 5. ON DUPLICATE KEY UPDATE (MySQL) → MERGE

**Decision:** ✅ **REMAP** to MERGE statement

**Justification:**
- MySQL-specific upsert syntax
- Semantically equivalent to SQL standard MERGE
- MERGE infrastructure already exists in SBLR
- Avoids implementing MySQL-specific logic

**Mapping:**
```sql
-- MySQL syntax (input):
INSERT INTO users (id, name) VALUES (1, 'Alice')
ON DUPLICATE KEY UPDATE name = VALUES(name);

-- Remap to (SBLR):
MERGE INTO users u
USING (SELECT 1 AS id, 'Alice' AS name FROM RDB$DATABASE) s
ON u.id = s.id
WHEN MATCHED THEN UPDATE SET name = s.name
WHEN NOT MATCHED THEN INSERT (id, name) VALUES (s.id, s.name);
```

**Implementation:**
1. Enable bytecode emission in ON DUPLICATE KEY parsing (currently disabled)
2. Generate EXT_MERGE opcodes instead of INSERT
3. Translate VALUES() function to source table column reference
4. Map UPDATE SET clause to WHEN MATCHED

**Effort:** 2-3 days
**Priority:** **ALPHA** (critical for MySQL compatibility)

**Semantic Differences:**
- ✅ Functionally equivalent for most use cases
- MySQL allows `VALUES(col)` to reference inserted values
- In MERGE, use source table column reference instead

---

### 6. REPLACE INTO (MySQL) → ON CONFLICT DO UPDATE

**Decision:** ✅ **ALREADY REMAPPED** - Keep current implementation

**Justification:**
- MySQL REPLACE has delete-then-insert semantics
- Currently mapped to ON CONFLICT DO UPDATE (update semantics)
- Functionally equivalent for most use cases
- Current implementation works

**Current Implementation:**
```cpp
void Parser::parseReplaceStmt() {
    emit(sblr::Opcode::INSERT);
    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ON_CONFLICT_DO_UPDATE));
    // ... parse table and values
}
```

**Effort:** 0 days (already done)
**Priority:** ✅ **COMPLETE**

**Document Difference:**
```
MySQL REPLACE vs ScratchBird:
- MySQL: DELETE + INSERT (two operations, OID changes)
- ScratchBird: UPDATE (one operation, OID preserved)
- Both produce same final data
- Difference only matters if tracking OIDs or using DELETE triggers
```

---

### 7. UNSIGNED (MySQL) → CHECK Constraint

**Decision:** ✅ **REMAP** to CHECK constraint

**Justification:**
- UNSIGNED means "value >= 0"
- CHECK constraint provides same validation
- No need for storage engine changes
- Standard SQL feature

**Mapping:**
```sql
-- MySQL syntax (input):
CREATE TABLE counts (value INT UNSIGNED);

-- Remap to (SBLR):
CREATE TABLE counts (
    value INT CHECK (value >= 0)
);
```

**Implementation:**
```cpp
void Parser::parseColumnDef() {
    // ... parse type

    if (type.unsigned_) {
        // Emit CHECK constraint
        emit(sblr::Opcode::CHECK_CONSTRAINT);
        // Generate bytecode for: column_name >= 0
        emit_column_ref(column_name);
        emit(sblr::Opcode::LITERAL_INT64);
        emitI64(0);
        emit(sblr::Opcode::OP_GTE);
    }
}
```

**Effort:** 1-2 days
**Priority:** **MEDIUM** (post-Alpha)

**Semantic Equivalence:** ✅ Exact

---

### 8. INSERT IGNORE (MySQL) → ON CONFLICT DO NOTHING

**Decision:** ✅ **REMAP** to ON CONFLICT DO NOTHING

**Justification:**
- INSERT IGNORE means "suppress duplicate key errors"
- ON CONFLICT DO NOTHING provides same semantics
- Standard SQL feature (PostgreSQL compatibility)

**Mapping:**
```sql
-- MySQL syntax (input):
INSERT IGNORE INTO users (id, name) VALUES (1, 'Alice');

-- Remap to (SBLR):
INSERT INTO users (id, name) VALUES (1, 'Alice')
ON CONFLICT DO NOTHING;
```

**Implementation:**
```cpp
bool ignore_flag = matchKeyword(TokenType::KW_IGNORE);

// ... parse INSERT statement

if (ignore_flag) {
    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ON_CONFLICT_DO_NOTHING));
}
```

**Effort:** 1 day
**Priority:** **MEDIUM** (post-Alpha)

**Semantic Equivalence:** ✅ Exact

---

### 9. SERIAL Types (PostgreSQL) → INTEGER + IDENTITY

**Decision:** ✅ **ALREADY REMAPPED** - Working correctly

**Justification:**
- SERIAL is PostgreSQL shorthand for INTEGER + auto-increment
- Already mapped to INTEGER + IDENTITY_COLUMN opcode
- Standard SQL IDENTITY feature

**Current Implementation:**
```cpp
case PgDataType::Kind::SERIAL:
case PgDataType::Kind::SMALLSERIAL:
    return sblr::Opcode::TYPE_INTEGER;
case PgDataType::Kind::BIGSERIAL:
    return sblr::Opcode::TYPE_BIGINT;
// Column constraints add IDENTITY_COLUMN opcode
```

**Effort:** 0 days (already done)
**Priority:** ✅ **COMPLETE**

**Semantic Equivalence:** ✅ Exact

---

## REJECT - Explicitly Not Support

These features should be explicitly rejected with clear error messages rather than silently failing.

### 10. UNLOGGED TABLES (PostgreSQL/V2)

**Decision for Alpha:** ❌ **REJECT** with warning
**Decision for Beta:** 🟡 **OPTIONAL** (only if write-after log is introduced)

**Justification:**
- Performance optimization feature (not core functionality)
- MGA has no write-after log (WAL, optional post-gold), so UNLOGGED semantics are effectively identical to regular tables
- If a write-after log (WAL, optional post-gold) is introduced later (for replication/PITR), UNLOGGED can bypass it
- Warning prevents user confusion

**Alpha Implementation:**
```cpp
bool is_unlogged = matchKeyword(TokenType::KW_UNLOGGED);
if (is_unlogged) {
    warning("UNLOGGED tables are treated as regular tables. Full support in Beta release.");
    // Continue parsing as regular table
}
```

**Effort:** 1 hour (warning only)
**Priority:** **ALPHA** (warning), **BETA** (full implementation)

---

### 11. Expression Indexes (PostgreSQL)

**Decision:** ❌ **REJECT** (already doing this)

**Justification:**
- Advanced indexing feature
- Currently rejected with clear error (good)
- Can be added post-Alpha if needed
- Most applications don't require this

**Current Implementation:**
```cpp
if (check(TokenType::LEFT_PAREN)) {
    // Expression index
    error("PostgreSQL expression indexes are not supported in current bytecode yet");
}
```

**Effort:** 0 days (already rejected)
**Priority:** **POST-BETA** (if requested)

**Recommendation:** ✅ Keep current rejection. Add in future if user demand exists.

---

### 12. INCLUDE Clause (PostgreSQL Covering Indexes)

**Decision:** ❌ **REJECT** (already doing this)

**Justification:**
- Index optimization feature
- Currently rejected with clear error (good)
- Performance feature, not correctness
- Can be added later if needed

**Current Implementation:**
```cpp
if (matchKeyword(TokenType::KW_INCLUDE)) {
    error("PostgreSQL INCLUDE indexes are not supported in current bytecode yet");
}
```

**Effort:** 0 days (already rejected)
**Priority:** **POST-BETA** (optimization)

**Recommendation:** ✅ Keep current rejection.

---

### 13. Table Inheritance (PostgreSQL INHERITS)

**Decision:** ❌ **REJECT** with error

**Justification:**
- Complex feature (requires polymorphic queries)
- PostgreSQL-specific (not standard SQL)
- Significant implementation effort (5-7 days)
- Low usage in practice
- Can use views/unions as workaround

**Alpha Implementation:**
```cpp
if (check(TokenType::KW_INHERITS)) {
    error("Table inheritance (INHERITS) is not supported in ScratchBird");
}
```

**Effort:** 1 hour (error message)
**Priority:** **ALPHA** (rejection), **NO PLANS** (implementation)

---

## DOCUMENT - Accept with Limitations

These features are accepted but have documented behavioral differences from native database.

### 14. INSERT Modifiers (MySQL: LOW_PRIORITY, HIGH_PRIORITY, DELAYED)

**Decision:** ✅ **DOCUMENT** as ignored

**Justification:**
- MySQL-specific query hints
- MyISAM-specific (deprecated storage engine)
- DELAYED deprecated in MySQL 5.6
- Safe to ignore (no semantic difference)
- Warning educates users

**Implementation:**
```cpp
if (matchKeyword(TokenType::KW_LOW_PRIORITY) ||
    matchKeyword(TokenType::KW_HIGH_PRIORITY) ||
    matchKeyword(TokenType::KW_DELAYED)) {
    warning("INSERT modifiers (LOW_PRIORITY/HIGH_PRIORITY/DELAYED) are ignored in ScratchBird");
}
```

**Effort:** 1 hour
**Priority:** **LOW** (optional warning)

**Documentation:**
```
MySQL INSERT modifiers are MyISAM-specific and have no effect in ScratchBird.
All inserts use ScratchBird's MVCC concurrency control.
```

---

### 15. Table Options (MySQL: ENGINE, ROW_FORMAT, etc.)

**Decision:** ✅ **DOCUMENT** which are supported vs ignored

**Justification:**
- Some options map to ScratchBird features (CHARSET, COLLATE, COMMENT)
- Some are MySQL-specific and should be ignored (ENGINE, ROW_FORMAT)
- Clear documentation prevents confusion

**Implementation:**
```cpp
if (matchKeyword(TokenType::KW_ENGINE)) {
    std::string engine = parseIdentifier();
    warning("ENGINE option ignored. ScratchBird uses native storage engine.");
}

if (matchKeyword(TokenType::KW_ROW_FORMAT)) {
    parseIdentifier();
    warning("ROW_FORMAT option ignored. ScratchBird uses native row format.");
}

if (matchKeyword(TokenType::KW_COMMENT)) {
    std::string comment = parseStringLiteral();
    // ✅ STORE in catalog metadata
}
```

**Effort:** 1-2 days (audit and document)
**Priority:** **MEDIUM** (post-Alpha)

**Documentation:**
| Option | Status | Notes |
|--------|--------|-------|
| ENGINE | Ignored | Always uses ScratchBird storage |
| AUTO_INCREMENT | ✅ Supported | Maps to IDENTITY |
| CHARSET | ✅ Supported | Maps to ScratchBird collation |
| COLLATE | ✅ Supported | Maps to ScratchBird collation |
| COMMENT | ✅ Supported | Stored in catalog |
| ROW_FORMAT | Ignored | ScratchBird uses native format |

---

### 16. ZEROFILL (MySQL)

**Decision for Alpha:** ✅ **DOCUMENT** as formatting only
**Decision for Beta:** ✅ **IMPLEMENT** formatter

**Justification:**
- Display formatting feature (not storage or validation)
- Can store as metadata for later formatting
- Low priority for Alpha (applications rarely depend on zero-padding)

**Alpha Implementation:**
```cpp
if (type.zerofill) {
    warning("ZEROFILL is stored as metadata but output formatting not yet implemented");
    type.unsigned_ = true;  // ZEROFILL implies UNSIGNED
    // Store zerofill flag in column metadata
}
```

**Effort:** 1 hour (warning), 2-3 days (full formatter)
**Priority:** **LOW** (warning for Alpha, implement for Beta)

---

## Alpha Release Roadmap

### Critical Path for Alpha (7-10 days)

**Week 1: Critical Bug Fixes**

| Task | Parser | Effort | Priority |
|------|--------|--------|----------|
| Fix ARRAY type bug | PostgreSQL | 1 day | ❌ **BLOCKER** |
| Fix DDL bytecode formats | PostgreSQL | 3-4 days | ❌ **BLOCKER** |
| Fix DML bytecode formats | PostgreSQL | 3-4 days | ❌ **BLOCKER** |

**Week 2: MySQL Critical Features**

| Task | Parser | Effort | Priority |
|------|--------|--------|----------|
| Implement CREATE INDEX | MySQL | 1-2 days | ❌ **BLOCKER** |
| Implement CREATE VIEW | MySQL | 2-3 days | ❌ **BLOCKER** |
| Remap ON DUPLICATE KEY → MERGE | MySQL | 2-3 days | ❌ **BLOCKER** |

**Week 3: Rejection/Warning Messages**

| Task | Parser | Effort | Priority |
|------|--------|--------|----------|
| Reject TEMPORARY TABLE | All | 1 hour | **HIGH** |
| Warn UNLOGGED TABLE | PostgreSQL/V2 | 1 hour | **MEDIUM** |
| Reject INHERITS | PostgreSQL | 1 hour | **MEDIUM** |

**Total:** 7-10 days (parallel work possible)

---

### Post-Alpha Roadmap (5-7 days)

**Phase 2: Remapping Features**

| Task | Effort | Priority |
|------|--------|----------|
| UNSIGNED → CHECK constraint | 1-2 days | **MEDIUM** |
| INSERT IGNORE → ON CONFLICT | 1 day | **MEDIUM** |
| Table options audit | 1 day | **MEDIUM** |
| DEFERRABLE verification | 1 day | **MEDIUM** |
| Network types (INET, CIDR) | 2-3 days | **LOW** |

**Total:** 5-7 days

---

### Beta Roadmap (10-15 days)

**Phase 3: Full Implementations**

| Task | Effort | Priority |
|------|--------|----------|
| TEMPORARY TABLES (full stack) | 3-5 days | **BETA** |
| UNLOGGED TABLES | 2-3 days | **BETA** |
| ZEROFILL formatter | 2-3 days | **BETA** |
| Expression indexes | 3-4 days | **POST-BETA** |
| INCLUDE clause | 2-3 days | **POST-BETA** |

**Total:** 10-15 days

---

## Summary Decision Table

| Feature | Decision | Effort | Alpha Priority | Implementation Phase |
|---------|----------|--------|----------------|---------------------|
| **IMPLEMENT** |
| TEMPORARY TABLES | Reject (Alpha), Implement (Beta) | 3-5 days | **REJECT** | **BETA** |
| CREATE INDEX (MySQL) | Implement | 1-2 days | ❌ **BLOCKER** | **ALPHA** |
| CREATE VIEW (MySQL) | Implement | 2-3 days | ❌ **BLOCKER** | **ALPHA** |
| ARRAY types (PostgreSQL) | Fix bug | 1 day | ❌ **BLOCKER** | **ALPHA** |
| **REMAP** |
| ON DUPLICATE KEY → MERGE | Remap | 2-3 days | ❌ **BLOCKER** | **ALPHA** |
| REPLACE INTO → ON CONFLICT | ✅ Done | 0 days | ✅ **COMPLETE** | **COMPLETE** |
| UNSIGNED → CHECK | Remap | 1-2 days | **MEDIUM** | **POST-ALPHA** |
| INSERT IGNORE → ON CONFLICT | Remap | 1 day | **MEDIUM** | **POST-ALPHA** |
| SERIAL → IDENTITY | ✅ Done | 0 days | ✅ **COMPLETE** | **COMPLETE** |
| **REJECT** |
| UNLOGGED (Alpha) | Warn | 1 hour | **MEDIUM** | **ALPHA** |
| Expression indexes | Reject | 0 days | ✅ **COMPLETE** | **N/A** |
| INCLUDE clause | Reject | 0 days | ✅ **COMPLETE** | **N/A** |
| INHERITS | Reject | 1 hour | **MEDIUM** | **ALPHA** |
| **DOCUMENT** |
| INSERT modifiers | Document/warn | 1 hour | **LOW** | **POST-ALPHA** |
| Table options | Audit/document | 1-2 days | **MEDIUM** | **POST-ALPHA** |
| ZEROFILL | Document | 1 hour | **LOW** | **ALPHA** |

---

## References

### Specification Documents
- `/docs/specifications/MYSQL_PARSER_IMPLEMENTATION_GAPS.md` - MySQL detailed analysis
- `/docs/specifications/POSTGRESQL_PARSER_IMPLEMENTATION_GAPS.md` - PostgreSQL detailed analysis
- `/docs/specifications/FIREBIRD_V2_FEATURE_PARITY_SPECIFICATION.md` - V2/Firebird analysis
- `/docs/audit/parsers/CRITICAL_FINDINGS.md` - Cross-parser critical issues

### Audit Documents
- `/docs/audit/parsers/COMPARISON_MATRIX.md` - Feature comparison
- `/docs/audit/parsers/SBLR_OPCODE_MAPPING.md` - Bytecode mappings
- `/docs/audit/19_postgresql_parser_correction_plan_checklist.md` - PostgreSQL bytecode fixes
- `/docs/audit/20_mysql_parser_correction_plan_checklist.md` - MySQL bytecode fixes

---

**End of Specification**
**Status:** ACTIVE - Decision Framework for Alpha Planning
**Next Steps:** Execute Alpha roadmap (Week 1: PostgreSQL fixes, Week 2: MySQL features)
