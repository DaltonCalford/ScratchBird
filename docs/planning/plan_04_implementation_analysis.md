# Plan 04 Parser and Compatibility - Implementation Readiness Analysis

**Analysis Date:** 2025-12-21
**Analyzed By:** AI Assistant
**Status:** PRE-IMPLEMENTATION REVIEW

---

## Executive Summary

Plan 04 (Parser Coverage and Compatibility) is **PARTIALLY READY** for implementation with several **DEPENDENCIES** and **GREY AREAS** that need resolution before full implementation can begin.

### Readiness Status

| Component | Status | Blocker Level |
|-----------|--------|---------------|
| **SBLR v2 Infrastructure** | ✅ READY | None |
| **ScratchBird V2 Parser DDL** | ⚠️ NEEDS PLANNING | Medium |
| **Transaction Control** | ⚠️ NEEDS SPEC CLARITY | Medium |
| **Domain DDL** | ❌ BLOCKED | HIGH - Missing specs |
| **Firebird Parser** | ✅ MOSTLY READY | Low |
| **MySQL Parser** | ✅ READY | Low |
| **PostgreSQL Parser** | ✅ READY | Low |
| **Semantic Analyzer** | ⚠️ NEEDS EXPANSION | Medium |
| **Bytecode Generator** | ⚠️ NEEDS EXPANSION | Medium |

### Dependencies on Other Plans

1. **Plan 02 (UUID Resolution)** - PARTIAL DEPENDENCY
   - EXT_RENAME_OBJECT and EXT_MOVE_OBJECT opcodes are defined ✅
   - Resolver APIs needed for semantic analysis (can stub for now) ⚠️

2. **Plan 03 (SBLR v2)** - COMPLETE ✅
   - SBLR_VERSION = 2 already set
   - 16-bit extended opcodes defined
   - Transaction payload v2 structure specified

3. **Plan 03 (Security)** - NO DEPENDENCY for parser work ✅
   - Security context is executor concern, not parser

---

## Part 1: What's Already in Place

### 1.1 SBLR v2 Infrastructure (✅ COMPLETE)

**Location:** `include/scratchbird/sblr/opcodes.h:1428`

```cpp
constexpr uint8_t SBLR_VERSION = 2;
```

**Extended Opcodes Defined:**
```cpp
enum class ExtendedOpcode : uint16_t {
    // ... existing opcodes ...
    EXT_RENAME_OBJECT = 0x0100,  // ✅ Ready for Plan 02 integration
    EXT_MOVE_OBJECT = 0x0101,    // ✅ Ready for Plan 02 integration
    EXT_SET_AUTOCOMMIT = 0x0102, // ✅ Ready for Plan 04 transaction work
    // ... more ...
};
```

**Status:** Infrastructure is complete and ready for use.

### 1.2 Parser V2 Base Structure (✅ COMPLETE)

**Location:** `src/parser/parser_v2.cpp`

**Implemented DDL:**
- ✅ CREATE TABLE (lines 295-722)
- ✅ CREATE INDEX (lines 723-814)
- ✅ CREATE VIEW (lines 815-869)
- ✅ CREATE SEQUENCE (lines 870-940)
- ✅ ALTER TABLE (lines 951-1050)
- ✅ DROP TABLE (lines 1068-1091)
- ✅ DROP INDEX (lines 1092-1119)
- ✅ DROP VIEW (lines 1120-1149)

**Stubbed/TODO DDL:**
```cpp
// TODO: Add more CREATE types
// if (matchContextual("FUNCTION"))   return parseCreateFunction(or_replace);
// if (matchContextual("PROCEDURE"))  return parseCreateProcedure(or_replace);
// if (matchContextual("TRIGGER"))    return parseCreateTrigger();
```

**Status:** Core table/view/index/sequence DDL works. Missing advanced objects.

### 1.3 Transaction Control (PARTIAL)

**Implemented:**
- ✅ START TRANSACTION (basic form) - `parseStartTransaction()` line 2494
- ✅ COMMIT (basic form) - `parseCommit()` line 2559
- ✅ ROLLBACK (basic form) - `parseRollback()` line 2581

**Missing per Plan 04 spec:**
- ❌ Firebird legacy clauses (WAIT/NO WAIT, LOCK TIMEOUT, RESERVING)
- ❌ Conflict action syntax (ON CONFLICT ...)
- ❌ AUTOCOMMIT control (SET AUTOCOMMIT, SET TRANSACTION AUTOCOMMIT)
- ❌ RETAINING clauses (COMMIT RETAINING, ROLLBACK RETAINING)
- ❌ 2PC statements (PREPARE TRANSACTION, COMMIT PREPARED, ROLLBACK PREPARED)

**Status:** Basic transaction control exists but needs major expansion.

---

## Part 2: Clear Implementation Paths

### 2.1 Firebird Parser Window Specs (✅ STRAIGHTFORWARD)

**TODO Location:** `src/parser/firebird/firebird_parser.cpp:parseFunctionCall()`

**Current Code:**
```cpp
// TODO: Window specification
// OVER (PARTITION BY ... ORDER BY ... ROWS ...)
```

**Implementation Path:** CLEAR
1. Add `parseWindowSpec()` method
2. Parse OVER keyword
3. Parse PARTITION BY clause (reuse parseExpressionList)
4. Parse ORDER BY clause (reuse parseOrderBy)
5. Parse frame clause (ROWS/RANGE BETWEEN)
6. Store in AST node (may need WindowSpec struct)
7. Emit to SBLR (may need EXT_WINDOW_SPEC or inline encoding)

**Estimated Complexity:** Low - Similar patterns exist in V2 parser

### 2.2 MySQL NULL-Safe Equality (✅ STRAIGHTFORWARD)

**TODO Location:** `src/parser/mysql/mysql_parser.cpp:parseComparisonExpr()`

**Current Code:**
```cpp
// NULL-safe equality <=>
// TODO: emit dedicated opcode with NULL semantics
emit(EXPR_EQ);  // Placeholder
```

**Implementation Path:** CLEAR
1. Detect `<=>` operator in lexer (likely already tokenized)
2. Add new extended opcode: `EXT_NULL_SAFE_EQ = 0x0200` (or similar)
3. Update `parseComparisonExpr()` to emit new opcode
4. Document NULL <=> NULL → TRUE semantics

**Estimated Complexity:** Low - Simple opcode addition

### 2.3 PostgreSQL ESCAPE Handling (✅ STRAIGHTFORWARD)

**TODO Locations:**
- `src/parser/postgresql/pg_parser_expr.cpp:parseLikeExpr()`
- `src/parser/mysql/mysql_parser.cpp:parseLikeExpr()`

**Current Code:**
```cpp
// TODO: ESCAPE handling
```

**Implementation Path:** CLEAR
1. After parsing LIKE pattern, check for ESCAPE keyword
2. Parse escape character literal
3. Add `escape_char` field to EXPR_LIKE payload (or use EXT_LIKE_ESCAPE)
4. Update SBLR executor to honor escape character

**Estimated Complexity:** Low - Straightforward parsing + payload extension

### 2.4 Placeholder Handling (✅ STRAIGHTFORWARD)

**TODO Location:** `src/parser/mysql/mysql_parser.cpp:~line 1097`

**Current Code:**
```cpp
// TODO: Placeholder handling
emit(LITERAL_NULL);  // Placeholder proxy
```

**Implementation Path:** CLEAR
1. Add PLACEHOLDER opcode (or use EXTENDED variant)
2. Emit placeholder index (uint16_t) + optional type hint
3. Store in prepared statement metadata
4. Executor binds actual values at execution time

**Estimated Complexity:** Low - Standard prepared statement pattern

---

## Part 3: Grey Areas Requiring Decisions

### 3.1 ScratchBird Transaction Grammar (⚠️ NEEDS SPEC CLARITY)

**Issue:** Plan 04 specifies complex transaction syntax with:
- SQL-standard options (ISOLATION LEVEL, READ ONLY/WRITE, DEFERRABLE)
- Firebird legacy options (SNAPSHOT, WAIT/NO WAIT, LOCK TIMEOUT, RESERVING)
- Conflict action syntax (ON CONFLICT COMMIT/ROLLBACK/ERROR/KEEP)
- AUTOCOMMIT integration

**Questions:**

1. **Isolation Level Mapping:**
   ```
   Plan says: READ UNCOMMITTED → READ_COMMITTED (executor treats as RC)
   ```
   - Should we store `read_uncommitted` flag in payload even though executor ignores it?
   - Is this for future feature compatibility or just documentation?

2. **RESERVING Clause:**
   ```
   RESERVING <table> FOR {SHARED|PROTECTED} {READ|WRITE}
   ```
   - What's the SBLR encoding for table list + reservation mode?
   - Does this require table UUID resolution in semantic analysis?
   - Is this alpha-safe to defer (mark as unsupported for now)?

3. **Conflict Action Defaults:**
   ```
   Plan says: "If omitted: use per-user/role default (global fallback is ROLLBACK)"
   ```
   - Where are per-user/role defaults stored? (Catalog? Connection context?)
   - Is this a Plan 03 (Security) dependency?
   - Can parser emit conflict_action=DEFAULT and let executor resolve?

**Recommendation:**
- ✅ Implement SQL-standard options (clear mapping)
- ⚠️ Implement ON CONFLICT clause (emit to payload, executor uses default if omitted)
- ⚠️ DEFER RESERVING clause to post-alpha (emit error "RESERVING not yet supported")
- ✅ Implement WAIT/NO WAIT + LOCK TIMEOUT (emit flags to payload)

### 3.2 Domain DDL (❌ BLOCKED - Missing Detailed Spec)

**Issue:** Plan 04 mentions extensive domain features:
- RECORD domains (composite types)
- ENUM domains
- SET OF domains
- VARIANT domains
- INHERITS clause
- WITH SECURITY/INTEGRITY/VALIDATION/QUALITY blocks
- WITH DIALECT(...) and WITH COMPAT(...) clauses

**Missing Information:**

1. **Grammar Specification:**
   - Full BNF for each domain kind
   - Syntax for WITH blocks (what's valid inside each?)
   - Inheritance semantics (single/multiple? conflicts?)

2. **SBLR Encoding:**
   - Plan says: "Emit domain type descriptors (TYPE_DOMAIN + UUID)"
   - What's the full payload structure?
   - How are RECORD field definitions encoded?
   - How are ENUM value lists encoded?
   - How are WITH block constraints encoded?

3. **Semantic Validation:**
   - What validations are needed for INHERITS cycles?
   - How do domain conflicts get detected and resolved?
   - What are the dialect_tag/compat_name uniqueness rules?

**Current State:**
```cpp
// From parser_v2.cpp - DOMAIN parsing is completely absent
// No parseCreateDomain, parseAlterDomain, etc.
```

**Recommendation:**
- ❌ **BLOCK Domain DDL implementation** until:
  1. Detailed domain specification document is created
  2. SBLR payload formats are defined
  3. Catalog schema for domain storage is designed

- Alternative: Implement **basic domain support only** (alias for base type + constraints)
  ```sql
  CREATE DOMAIN email_address AS VARCHAR(255) CHECK (VALUE LIKE '%@%');
  ```
  This is standard SQL-92 and well-understood.

### 3.3 Semantic Analyzer Expansion (⚠️ NEEDS DESIGN)

**Issue:** Plan 04 requires semantic validation but current `SemanticAnalyzerV2` is minimal.

**Current State:**
```cpp
// From semantic_analyzer_v2.cpp
Statement* SemanticAnalyzerV2::analyzeAlterTable(AlterTableStmt* stmt) {
    // TODO: Implement ALTER TABLE semantic analysis
    LOG_WARNING(PARSER, "analyzeAlterTable not implemented yet");
    return nullptr;
}
```

**Missing Features:**
1. GROUP BY validation (reject non-aggregate columns not in GROUP BY)
2. Dependency collection (populate ResolvedStatement::object_uuids)
3. Object resolution (tables, columns, domains, sequences)
4. Type inference and checking
5. Window function validation

**Questions:**
1. Should semantic analysis use CatalogManager directly or go through resolver API?
2. For alpha, is it acceptable to defer semantic validation and let executor catch errors?
3. What's the minimum viable semantic analysis for DDL vs DML?

**Recommendation:**
- ✅ Implement GROUP BY validation (spec is clear)
- ⚠️ Implement basic object resolution (table/column existence checks)
- ⚠️ DEFER advanced type inference (let executor handle for now)
- ⚠️ DEFER dependency tracking (Plan 02 provides resolver API)

### 3.4 Dialect Guardrails (⚠️ IMPLEMENTATION STRATEGY UNCLEAR)

**Issue:** Plan 04 requires "dialect allowlists" to prevent feature bleed.

**Spec Says:**
```
For each emulated parser, build a dialect allowlist from its specification
document and reject any statement not in that allowlist with a clear error.
```

**Questions:**

1. **Allowlist Source:**
   - Are the dialect specs (`firebird_spec.md`, etc.) complete enough to build allowlists?
   - Should allowlists be code (switch statements) or data (config files)?

2. **Granularity:**
   - Statement level? (e.g., "MySQL has no CREATE DOMAIN")
   - Clause level? (e.g., "MySQL CREATE TABLE has no INHERITS clause")
   - Keyword level? (e.g., "Firebird has no DEFERRABLE")

3. **Error Messages:**
   - Just "Unsupported in MySQL dialect"?
   - Or "CREATE DOMAIN is not supported in MySQL. Use base types instead."?

**Current State:**
```cpp
// No allowlist enforcement exists in emulated parsers
// They accept whatever syntax is implemented
```

**Recommendation:**
- ⚠️ **Phase 1:** Add explicit errors for known unsupported features
  ```cpp
  // In mysql_parser.cpp
  if (matchContextual("DOMAIN")) {
      error("CREATE DOMAIN is not supported in MySQL dialect. Use base types.");
      return nullptr;
  }
  ```
- ⚠️ **Phase 2:** Build systematic allowlist (post-alpha)

---

## Part 4: Missing Specifications

### 4.1 CRITICAL: Domain Specification Document

**Required Before Implementation:**
1. **Full Grammar Definition**
   ```
   CREATE DOMAIN <name> AS <type_spec> [<domain_options>]

   <type_spec> ::=
       <base_type>
     | RECORD ( <field_list> )
     | ENUM ( <enum_values> )
     | SET OF <element_type>
     | VARIANT ( <variant_arms> )

   <domain_options> ::=
       [ DEFAULT <expr> ]
       [ NOT NULL | NULL ]
       [ CHECK ( <condition> ) ]
       [ WITH DIALECT ( <tag> ) ]
       [ WITH COMPAT ( <name> ) ]
       [ INHERITS ( <parent_domains> ) ]
       [ WITH SECURITY ( <security_spec> ) ]
       [ WITH INTEGRITY ( <integrity_spec> ) ]
       [ WITH VALIDATION ( <validation_spec> ) ]
       [ WITH QUALITY ( <quality_spec> ) ]
       [ WITH OPTIONS ( <option_list> ) ]
   ```

2. **SBLR Encoding Specification**
   - Complete payload structure for EXT_CREATE_DOMAIN
   - Field encoding for RECORD types
   - Value list encoding for ENUM types
   - Constraint encoding for WITH blocks

3. **Catalog Schema**
   - DomainRecord structure (what fields? how stored?)
   - Domain dependency tracking (how does INHERITS work?)
   - Conflict resolution rules (dialect_tag + compat_name uniqueness)

4. **Semantic Rules**
   - Inheritance semantics (override vs extend)
   - Type compatibility (when can domain A be cast to domain B?)
   - Constraint composition (how do CHECK constraints combine in inheritance?)

**Recommendation:** Create `docs/specifications/DDL_DOMAINS_COMPREHENSIVE.md`

### 4.2 NEEDED: Transaction Control Comprehensive Spec

**Current Spec Gaps:**
1. **Payload Encoding Details**
   - Exact byte layout for RESERVING clause
   - How are table lists encoded? (count + UUIDs? count + names?)

2. **Conflict Action Semantics**
   - What happens to open cursors on COMMIT/ROLLBACK with conflict action?
   - Can conflict actions be nested (transaction within transaction)?

3. **RETAINING Semantics**
   - What's the difference between COMMIT RETAINING vs COMMIT AND CHAIN?
   - Which Firebird snapshot semantics are preserved?

**Recommendation:** Expand `docs/specifications/FIREBIRD_TRANSACTION_MODEL_SPEC.md`

### 4.3 WANTED: Window Function Specification

**Current State:** Plan 04 mentions window specs but doesn't provide:
1. Full window grammar (PARTITION BY, ORDER BY, frame clauses)
2. SBLR encoding for window definitions
3. Executor semantics for window evaluation

**Recommendation:** Create `docs/specifications/WINDOW_FUNCTIONS.md` or expand existing SQL spec

---

## Part 5: Dependency Analysis

### 5.1 Plan 02 Dependencies (UUID Resolution)

**Parser Impact:**
- ⚠️ Rename/move opcodes are defined but parser doesn't emit them yet
- ⚠️ ALTER ... SET SCHEMA parsing is stubbed
- ⚠️ ALTER ... RENAME TO parsing is incomplete

**Semantic Analysis Impact:**
- ⚠️ Path resolution needed for multi-part identifiers (schema.table.column)
- ⚠️ Object UUID resolution for dependency tracking

**Can We Proceed?**
- ✅ YES for basic DDL (CREATE/ALTER TABLE without rename/move)
- ⚠️ PARTIAL for advanced DDL (can stub ALTER ... SET SCHEMA)
- ❌ NO for semantic dependency tracking (needs resolver API)

**Mitigation:**
```cpp
// In semantic_analyzer_v2.cpp
Statement* SemanticAnalyzerV2::analyzeAlterTable(AlterTableStmt* stmt) {
    // TEMPORARY: Skip UUID resolution until Plan 02 complete
    // TODO: Call resolver->resolveObjectPath() when available

    // For now, just validate syntax and emit bytecode
    return createResolvedStatement(stmt);
}
```

### 5.2 Plan 03 SBLR v2 Dependencies

**Status:** ✅ COMPLETE - No blockers

SBLR_VERSION = 2 and extended opcodes are ready.

### 5.3 Plan 03 Security Dependencies

**Status:** ✅ NO PARSER IMPACT

Security context is handled by executor, not parser.

---

## Part 6: Test Coverage Requirements

### 6.1 Parser Unit Tests (Need Creation/Expansion)

**Existing Tests:**
- `tests/unit/test_parser_v2_ddl.cpp` - Basic DDL ✅
- `tests/unit/test_parser_dml_v2.cpp` - Basic DML ✅
- `tests/unit/test_semantic_analyzer_v2.cpp` - Minimal ❌
- `tests/unit/test_bytecode_generator_v2.cpp` - Minimal ❌

**Missing Test Coverage:**
- ❌ ALTER TABLE coverage (only CREATE TABLE tested)
- ❌ Transaction control variations
- ❌ Window function parsing
- ❌ Domain DDL (once spec exists)
- ❌ Dialect guardrail tests

**Required New Tests:**
```cpp
// tests/unit/test_parser_v2_transaction.cpp - NEW
TEST(ParserV2Transaction, StartTransactionBasic)
TEST(ParserV2Transaction, StartTransactionWithIsolation)
TEST(ParserV2Transaction, StartTransactionWithConflictAction)
TEST(ParserV2Transaction, SetAutocommit)
TEST(ParserV2Transaction, CommitRetaining)
TEST(ParserV2Transaction, PrepareTwoPhaseCommit)

// tests/unit/test_parser_v2_dialect_guardrails.cpp - NEW
TEST(DialectGuardrails, MySQLRejectsCreateDomain)
TEST(DialectGuardrails, MySQLRejectsHierarchicalSchema)
TEST(DialectGuardrails, FirebirdRejectsScratchBirdShowCommands)
```

### 6.2 Integration Tests (Need Creation)

**Required:**
1. End-to-end DDL tests (parse → semantic → bytecode → execute)
2. Cross-dialect compatibility tests
3. Bytecode roundtrip tests (emit → decode → re-emit)

---

## Part 7: Implementation Sequencing

### Phase 1: Low-Hanging Fruit (✅ Can Start Immediately)

**Tasks:**
1. Firebird window specification parsing
2. MySQL NULL-safe equality
3. PostgreSQL/MySQL ESCAPE handling
4. MySQL placeholder handling
5. Basic GROUP BY validation in semantic analyzer

**Estimated Effort:** 2-3 days
**Risk:** Low
**Dependencies:** None

### Phase 2: Transaction Control (⚠️ Needs Spec Clarification First)

**Prerequisites:**
1. Clarify RESERVING clause encoding (or defer)
2. Clarify conflict action default resolution
3. Expand transaction spec document

**Tasks:**
1. Extend StartTransactionStmt AST with all fields
2. Parse SQL-standard + Firebird legacy clauses
3. Parse ON CONFLICT clause
4. Parse AUTOCOMMIT control statements
5. Parse RETAINING and 2PC statements
6. Emit updated START_TRANSACTION payload
7. Emit new extended opcodes for AUTOCOMMIT/RETAINING/2PC

**Estimated Effort:** 5-7 days
**Risk:** Medium (spec ambiguity)
**Dependencies:** Transaction spec clarification

### Phase 3: DDL Expansion (⚠️ Partial Readiness)

**Can Do Now:**
- CREATE/ALTER/DROP for FUNCTION/PROCEDURE/TRIGGER (basic forms)
- ALTER VIEW/SEQUENCE/INDEX

**Blocked:**
- Domain DDL (awaiting comprehensive spec)

**Estimated Effort:** 3-5 days
**Risk:** Low for non-domain DDL
**Dependencies:** None for basic DDL; domain spec for advanced

### Phase 4: Semantic Analysis (⚠️ Design Needed)

**Prerequisites:**
1. Define semantic validation scope for alpha
2. Decide on catalog vs resolver access
3. Design error reporting strategy

**Tasks:**
1. Implement analyzeAlterTable
2. Implement validateGroupBy
3. Implement basic object resolution
4. Populate dependency UUIDs (when Plan 02 ready)

**Estimated Effort:** 5-7 days
**Risk:** Medium (design decisions)
**Dependencies:** Partial Plan 02 for object resolution

### Phase 5: Dialect Guardrails (⚠️ Strategy Needed)

**Prerequisites:**
1. Audit dialect spec documents for completeness
2. Decide on allowlist implementation strategy

**Tasks:**
1. Add explicit unsupported feature errors
2. Document allowed features per dialect
3. Create systematic test suite

**Estimated Effort:** 3-4 days
**Risk:** Low
**Dependencies:** Complete dialect specs

---

## Part 8: Recommendations

### Immediate Actions (Before Starting Implementation)

1. ✅ **GO:** Phase 1 (Low-hanging fruit) - Start immediately
   - Firebird window specs
   - MySQL NULL-safe equality
   - ESCAPE handling
   - Placeholder handling
   - Basic GROUP BY validation

2. ⚠️ **CLARIFY FIRST:** Transaction Control
   - **ACTION REQUIRED:** Create decision document for:
     - RESERVING clause (implement, stub, or defer?)
     - Conflict action defaults (where stored? how resolved?)
     - RETAINING vs AND CHAIN semantics
   - **OWNER:** Architecture team or lead developer
   - **TIMELINE:** Before Phase 2 implementation

3. ❌ **BLOCK:** Domain DDL
   - **ACTION REQUIRED:** Create comprehensive domain specification
   - **SCOPE:** Full grammar, SBLR encoding, catalog schema, semantic rules
   - **OWNER:** Database design team
   - **TIMELINE:** Critical path blocker - 1-2 weeks to create spec

4. ⚠️ **DESIGN SESSION:** Semantic Analysis Strategy
   - **TOPICS:**
     - Validation scope for alpha (what can be deferred?)
     - Catalog access patterns
     - Error reporting and recovery
   - **ATTENDEES:** Parser team, executor team
   - **TIMELINE:** Before Phase 4 implementation

5. ⚠️ **DOCUMENT:** Dialect Allowlists
   - **ACTION REQUIRED:** Audit existing dialect specs
   - **OUTPUT:** Feature matrix (dialect × feature → supported/unsupported)
   - **OWNER:** Compatibility team
   - **TIMELINE:** Before Phase 5 implementation

### Testing Strategy

1. **Unit Tests First:** Write parser tests before implementing features
2. **Bytecode Verification:** Add SBLR decode tests to catch encoding bugs
3. **Cross-Dialect Tests:** Ensure emulated parsers behave correctly
4. **Negative Tests:** Test unsupported feature rejection (guardrails)

### Risk Mitigation

1. **Domain Spec Risk (HIGH):**
   - Fallback: Implement basic domain support only (standard SQL-92)
   - Timeline: If spec not ready in 2 weeks, proceed with basic version

2. **Transaction Spec Risk (MEDIUM):**
   - Fallback: Defer RESERVING clause to post-alpha
   - Timeline: Clarify within 1 week or defer

3. **Semantic Analysis Risk (MEDIUM):**
   - Fallback: Minimal validation, defer to executor
   - Timeline: Design session within 1 week

---

## Part 9: Estimated Timeline

### Conservative Estimate (With All Dependencies Resolved)

| Phase | Duration | Parallel? | Blockers |
|-------|----------|-----------|----------|
| Phase 1: Low-hanging fruit | 3 days | ✅ Can start now | None |
| Spec clarification | 1-2 weeks | ❌ Must complete | Architecture decisions |
| Phase 2: Transaction control | 7 days | After spec | Transaction spec |
| Phase 3: DDL expansion (no domain) | 5 days | ✅ Parallel with Phase 2 | None |
| Phase 4: Semantic analysis | 7 days | After design session | Design decisions |
| Phase 5: Dialect guardrails | 4 days | ✅ Parallel with Phase 4 | Dialect specs |
| Testing & integration | 5 days | After implementation | None |

**Total Duration:** 6-8 weeks (assuming spec work completes in 2 weeks)

### Aggressive Estimate (With Deferrals)

- Defer RESERVING clause → Save 2 days
- Defer domain DDL → Save indefinite waiting time (proceed with basic domains)
- Minimal semantic analysis → Save 3 days
- Lightweight guardrails → Save 2 days

**Reduced Duration:** 4-5 weeks

---

## Part 10: Critical Questions for Decision Makers

### Domain DDL
1. Is comprehensive domain support (RECORD/ENUM/VARIANT/INHERITS) required for alpha?
2. If no, is basic domain support (type alias + constraints) acceptable?
3. Who will write the comprehensive domain specification if needed?

### Transaction Control
4. Should RESERVING clause be implemented, stubbed, or deferred?
5. Where are per-user/role default conflict actions stored?
6. Is it acceptable for parser to emit conflict_action=DEFAULT and let executor resolve?

### Semantic Analysis
7. What's the minimum viable semantic analysis for alpha release?
8. Should semantic analyzer use CatalogManager directly or wait for Plan 02 resolver API?
9. Is it acceptable to defer type inference and let executor catch type errors?

### Dialect Guardrails
10. Should dialect allowlists be code-based or data-driven (config files)?
11. What level of error detail is required? ("Unsupported" vs detailed alternatives)
12. Are the dialect specification documents complete enough to build allowlists?

### Testing
13. What's the required test coverage percentage before merging?
14. Should we require bytecode round-trip tests for every feature?

---

## Conclusion

**Plan 04 is implementable but requires upfront decision-making and specification work.**

### GREEN LIGHT ✅
- Phases 1 (low-hanging fruit) can start immediately
- Basic DDL expansion (functions/procedures/triggers) can proceed
- SBLR infrastructure is ready

### YELLOW LIGHT ⚠️
- Transaction control needs specification clarification
- Semantic analysis needs design session
- Dialect guardrails need strategy decision

### RED LIGHT ❌
- Comprehensive domain DDL is blocked on missing specification
- Can proceed with basic domains as fallback

### Recommended Next Steps
1. **Start Phase 1 immediately** (3-day quick win)
2. **Schedule decision meetings** for transaction spec, semantic strategy, dialects
3. **Create domain specification** or **decide to defer** advanced domains
4. **Proceed with Phases 2-5** as decisions are made

**Total estimated time: 4-8 weeks depending on decisions and spec completion.**

---

**End of Analysis Report**
