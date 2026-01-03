# Plan 04 - Domain DDL Implementation Status

**Plan:** Domain DDL (CREATE/ALTER/DROP DOMAIN) with full WITH block support
**Version:** 1.0
**Last Updated:** 2026-01-03
**Overall Status:** ⚠️ IN PROGRESS - V2 domain DDL + executor enforcement complete; remaining work is emulated parser coverage, semantic validation guardrails, conflict opcodes, and comprehensive tests

---

## Quick Summary

| Phase | Status | Details |
|-------|--------|---------|
| **Specification** | ✅ COMPLETE | All specifications written and reviewed |
| **Prerequisites** | ✅ COMPLETE | Plan 02B core complete (alignment/testing pending); Plan 03B complete (verification pending) |
| **Implementation** | ⚠️ IN PROGRESS | V2 parser/semantic/bytecode/executor wired; remaining semantic guardrails, emulated parser DDL, conflict opcodes, and comprehensive tests |

---

## Work Completed (2025-12-21 to 2026-01-03)

### ✅ Phase 1: Specification Development (COMPLETE)

1. **DDL_DOMAINS_COMPREHENSIVE.md** ✅
   - Complete domain DDL syntax specification
   - All 5 domain types: BASIC, RECORD, ENUM, SET, VARIANT
   - Full WITH block specifications (SECURITY, INTEGRITY, VALIDATION, QUALITY)
   - Dual syntax support (Firebird + SQL Standard)
   - Parser architecture clarifications
   - NO DEFERRALS - all WITH blocks specified for Alpha implementation

2. **PLAN_04_IMPLEMENTATION_CHECKLIST.md** ✅
   - 80 implementation tasks across 14 sections
   - Complete parser implementation roadmap (V2, Firebird, PostgreSQL, MySQL)
   - SBLR opcode extensions (10 new opcodes for WITH blocks)
   - AST node extensions for all domain types
   - Semantic analyzer implementation tasks
   - Bytecode generator implementation tasks
   - Executor implementation with full enforcement
   - Comprehensive testing requirements

3. **PLAN_03B_DOMAIN_INFRASTRUCTURE.md** ✅
   - 35 infrastructure tasks required BEFORE Plan 04
   - Encryption infrastructure (AES-256/128-GCM, key management)
   - Data masking engine (PARTIAL/FULL with patterns)
   - Global uniqueness tracking (cross-table, MGA-aware)
   - Normalization infrastructure (built-in + custom functions)
   - Validation integration (custom functions, error messages)
   - Quality pipeline (parse/standardize/enrich chaining)
   - SBLR opcode definitions and executor handlers
   - Comprehensive test coverage
   - Timeline: 17-24 days single dev, 7.5-10.5 days with 3 devs

4. **PLAN_04_PREREQUISITES.md** ✅
   - Analysis of missing infrastructure
   - Identified 138-192 hours of prerequisite work (Plan 03B)
   - Comparison of existing vs missing components
   - Recommendation: Option 1 (implement all prerequisites first)
   - **UPDATED 2025-12-26:** Critical blocker section added

### ✅ Phase 2: Architecture Review and Corrections (COMPLETE)

1. **Dual Syntax Principle Corrected** ✅
   - Changed from "When SQL Standard and Firebird conflict, follow Firebird"
   - To: "Implement BOTH for Firebird compatibility while allowing SQL Standard to be followed moving forward"
   - Updated in both specifications

2. **Parser Architecture Clarified** ✅
   - ScratchBird V2: Context-aware with flexible keywords
   - Emulated parsers (Firebird/PostgreSQL/MySQL): Must match target engine's reserved keyword lists exactly
   - Each parser follows its target's limitations while ScratchBird retains advanced design

3. **NO DEFERRALS Violation Fixed** ✅
   - Removed all "Deferred to Beta" language for WITH blocks
   - Changed Task 4.7 from parsing-only to full implementation
   - Added Tasks 10.9-10.12 for WITH block enforcement (52 hours)
   - Updated task count from 76 to 80 tasks

### ✅ Phase 3: Blocker Resolved (2025-12-31)

1. **Schema/Database DDL Infrastructure Implemented** ✅
   - EXT_CREATE/EXT_DROP/EXT_ALTER SCHEMA and DATABASE opcodes added
   - PostgreSQL/MySQL parsers emit schema/database opcodes
   - Firebird parser supports CREATE/DROP/ALTER DATABASE (rename only)
   - Executor handlers implemented for schema/database DDL
   - Emulation view generator updated and integrated into CREATE/DROP DATABASE
   - Canonical emulation path resolved to `remote.emulated.<dialect>.<server>.<db>` (dot-path)

2. **Documentation Updated** ✅
   - `/docs/findings/CRITICAL_SCHEMA_DATABASE_OPCODE_GAP.md` - historical analysis
   - `PLAN_04_PREREQUISITES.md` updated with resolved status
   - `PLAN_04_IMPLEMENTATION_CHECKLIST.md` updated with unblocked status

### ✅ Phase 4: Domain DDL Wiring (2026-01-01)

1. **Parser V2 Domain DDL** ✅
   - CREATE DOMAIN now parses WITH SECURITY/INTEGRITY/VALIDATION/QUALITY
   - ALTER DOMAIN + DROP DOMAIN parsing added
   - CREATE/ALTER/DROP dispatchers updated

2. **AST/Resolved AST Extensions** ✅
   - CreateDomainStmt includes security/validation/quality options
   - AlterDomainStmt + DropDomainStmt added to v2 AST + resolved AST

3. **Bytecode + Executor Wiring** ✅
   - EXT_CREATE_DOMAIN payload extended for WITH blocks
   - EXT_ALTER_DOMAIN / EXT_DROP_DOMAIN opcodes emitted and executed

4. **Docs + Tests** ✅
   - SBLR domain payload spec added
   - Parser/bytecode tests added for WITH blocks + ALTER/DROP DOMAIN

### ✅ Phase 5: Advanced Domain Types + Payload Alignment (2026-01-01)

1. **Advanced Domain Types Wired (V2)** ✅
   - RECORD/ENUM/SET/VARIANT parsing + schema-qualified types
   - INHERITS, COLLATE, WITH DIALECT/COMPAT, WITH OPTIONS (ENUM WRAP)
   - Type references for domain-typed fields/elements

2. **Domain Catalog & Serialization Updates** ✅
   - DomainTypeRef for SET/VARIANT payloads
   - Dialect/compat persisted + default tag
   - Record field defaults stored in domain fields payload

3. **SBLR + Executor Updates** ✅
   - EXT_CREATE_DOMAIN payloads updated for all domain kinds
   - Executor reads domain kind/type refs and persists options
   - SHOW DOMAIN renders SET/VARIANT type refs

4. **Tests Added/Updated** ✅
   - Parser tests for RECORD/ENUM/SET/VARIANT + INHERITS/COLLATE
   - Bytecode tests for all domain kinds
   - Dialect/compat persistence in domain reload test

### ✅ Phase 6: Domain Enforcement Pipeline Corrections (2026-01-02)

1. **DML Constraint Enforcement** ✅
   - INSERT/UPDATE now call `DomainManager::validateValue` for domain CHECK/NOT NULL/inherited constraints
2. **WITH Block Enforcement Wiring** ✅
   - EXT_CHECK_DOMAIN_CONSTRAINT / MASKING / ENCRYPTION / NORMALIZATION / VALIDATION / QUALITY opcodes handled in executor

### ✅ Phase 7: Parser Quick Wins (2026-01-02)

1. **NULL-Safe Equality + GROUP BY Validation** ✅
   - Added `EXT_NULL_SAFE_EQ` and wired MySQL/PostgreSQL `<=>` / `IS` predicates
   - V2 semantic analyzer enforces GROUP BY grouping rules
2. **LIKE ESCAPE Handling** ✅
   - ESCAPE clauses emit `EXT_LIKE_ESCAPE` / `EXT_ILIKE_ESCAPE` and executor honors custom escapes
3. **Placeholder Handling** ✅
   - Added `EXT_PLACEHOLDER` and wired MySQL/PostgreSQL placeholders with executor binding
4. **Firebird Window Specs** ✅
   - Firebird parser now stores OVER(PARTITION/ORDER/FRAME) in AST
5. **Firebird Predicate Variants** ✅
   - LIKE/CONTAINING/STARTING/SIMILAR TO tracked with new match kind + NOT support
6. **PostgreSQL Array Subscript** ✅
   - `arr[idx]` now emits `EXT_ARRAY_SUBSCRIPT`
7. **MySQL Table Constraints + Geometry Mapping** ✅
   - CREATE TABLE constraints parsed; GEOMETRY/POINT/LINESTRING/POLYGON mapped in type emission
8. **MySQL DOMAIN Guardrails** ✅
   - CREATE/ALTER/DROP DOMAIN rejected with clear alternatives + parser tests added

### ✅ Phase 8: Domain Semantic Guardrails (2026-01-03)

1. **Duplicate Guardrails Added** ✅
   - Duplicate domain constraint names rejected (case-insensitive)
   - Duplicate RECORD field names rejected (case-insensitive)
   - Duplicate ENUM labels rejected (case-sensitive)
   - Duplicate domain names rejected (schema-scoped, IF NOT EXISTS allowed)
   - CHECK constraints require VALUE and reject subqueries
   - BASIC domain DEFAULT literal type compatibility enforced
   - RECORD/VARIANT domains must be non-empty; VARIANT types unique
2. **Tests Added** ✅
   - Semantic analyzer unit tests cover duplicate constraint/field/label cases

---

## Current Dependencies

### 🟡 Dependency #1: Plan 03B Domain Infrastructure

**Status:** ✅ COMPLETE (verification pending external runner)
**Severity:** RESOLVED

**Why it matters:**
- WITH blocks (SECURITY/INTEGRITY/VALIDATION/QUALITY) require the Plan 03B infrastructure.
- Domain enforcement depends on audit, masking, validation, and quality pipelines.

**Dependencies:**
- Plan 03B (Domain Infrastructure) - COMPLETE

**Note:** Plan 02B (Schema/Database DDL) is now core complete; remaining alignment/testing is tracked separately.

**Status Update:** Plan 03B enforcement work is complete; domain DDL wiring uses the enforcement pipeline.

**Detailed Analysis:** `/docs/findings/CRITICAL_SCHEMA_DATABASE_OPCODE_GAP.md`

---

### ✅ Dependency #2: Emulation Schema Path Alignment

**Status:** COMPLETE - adapters/compilers aligned to dot-path defaults
**Severity:** RESOLVED

**Current State:**
- Canonical path is `remote.emulated.<dialect>.<server>.<db>` (dot-path).
- MySQL/PostgreSQL parsers normalize slash paths to dot paths.
- Adapters and query compilers use dot-path defaults.

**Follow-ups:**
- Verify no legacy `emulation.<dialect>` references remain in code/docs (audit-only).

**Detailed Analysis:** `docs/audit/AUDIT_SCHEMA_DATABASE_DDL_GAP.md`

---

## Prerequisites Status

### Plan 03B: Domain Infrastructure (138-192 hours)

**Status:** COMPLETE (verification pending external runner)
**Assigned To:** This AI (Plan 04 team)
**Dependencies:** Plan 02B core complete; remaining alignment/testing can proceed in parallel

**Key Components:**
- ✅ Specification complete (35 tasks defined)
- ✅ Implementation complete (masking/encryption/normalization/validation/quality/uniqueness)
- ⚠️ Verification pending external runner

### Plan 02B: Schema/Database DDL (60-80 hours)

**Status:** CORE COMPLETE - See `docs/planning/PLAN_02B_SCHEMA_DATABASE_DDL.md`
**Critical For:** Plan 04 testing capability

**Delivered:**
- EXT_CREATE/EXT_DROP/EXT_ALTER SCHEMA and DATABASE opcodes + handlers
- PostgreSQL/MySQL parser emission (schema/database DDL)
- Firebird CREATE/DROP/ALTER DATABASE support (rename only for ALTER)
- Emulated database lifecycle + view generation integration

**Remaining:** Cascade semantics, tests

---

## Next Steps

1. Complete semantic analyzer validation (type checks, inheritance cycles, dependency checks).
2. Complete dialect guardrails + remaining parser stubs (Firebird/PostgreSQL/MySQL).
3. Define domain conflict opcodes (EXT_REBIND_DOMAIN / EXT_RESOLVE_DOMAIN_CONFLICT).
4. Track remaining Plan 02B testing items (cascade semantics, tests).

**Timeline:** Remaining work is validation + guardrails/stubs + testing.

---

## Files Modified/Created

### Specifications
- ✅ `/docs/specifications/DDL_DOMAINS_COMPREHENSIVE.md` (NEW)
- ✅ `/docs/specifications/SBLR_DOMAIN_PAYLOADS.md` (NEW)
- ✅ `/docs/planning/PLAN_04_IMPLEMENTATION_CHECKLIST.md` (NEW)
- ✅ `/docs/planning/PLAN_03B_DOMAIN_INFRASTRUCTURE.md` (NEW)
- ✅ `/docs/planning/PLAN_04_PREREQUISITES.md` (NEW)

### Findings
- ✅ `/docs/findings/CRITICAL_SCHEMA_DATABASE_OPCODE_GAP.md` (NEW)

### Status Tracking
- ✅ `/docs/planning/PLAN_04_STATUS.md` (THIS FILE - NEW)

---

## Implementation Progress (V2 pipeline complete; remaining items in emulated parsers/validation/tests)

**Current:** V2 domain DDL (all domain kinds + WITH blocks), SBLR emission, executor handling, and emulated parser domain DDL are complete. Remaining work is semantic validation guardrails, conflict opcodes, and comprehensive tests.

### Section 1: Schema Changes (1/1)
- [x] Task 1.1: Add dialect_tag/compat_name to domain schema

### Section 2: SBLR Opcodes (2/3)
- [ ] Task 2.1: Define extended domain conflict opcodes (EXT_REBIND_DOMAIN/EXT_RESOLVE_DOMAIN_CONFLICT)
- [x] Task 2.2: Document SBLR payload structures (BASIC/RECORD/ENUM/SET/VARIANT + ALTER/DROP)
- [x] Task 2.3: Define WITH block enforcement opcodes

### Section 3: AST Extensions (3/3)
- [x] Task 3.1: Extend CreateDomainStmt (V2 complete; V1 removed)
- [x] Task 3.2: Create AlterDomainStmt
- [x] Task 3.3: Create DropDomainStmt

### Section 4: ScratchBird V2 Parser (12/12)
- [x] Tasks 4.1-4.7: parseCreateDomain() all types + WITH blocks
- [x] Task 4.8: parseAlterDomain()
- [x] Task 4.9: parseDropDomain()
- [x] Task 4.10-4.12: Update dispatchers

### Section 5: Firebird Parser (0/3)
- [ ] Tasks 5.1-5.3: CREATE/ALTER/DROP DOMAIN (Firebird syntax)

### Section 6: PostgreSQL Parser (0/3)
- [ ] Tasks 6.1-6.3: CREATE/ALTER/DROP DOMAIN (align payload to SBLR v2)

### Section 7: MySQL Parser (0/3)
- [ ] Tasks 7.1-7.3: Reject domain DDL with clear errors

### Section 8: Semantic Analyzer (partial)
- [ ] Tasks 8.1-8.7: Full validation + dependency checks (core analysis wired; guardrails/tests pending)

### Section 9: Bytecode Generator (7/7)
- [x] Tasks 9.1-9.5: Emit CREATE DOMAIN for all types
- [x] Task 9.6: Emit ALTER DOMAIN
- [x] Task 9.7: Emit DROP DOMAIN

### Section 10: Executor (12/12)
- [x] Tasks 10.1-10.5: Execute CREATE DOMAIN for all types
- [x] Task 10.6: Execute ALTER DOMAIN
- [x] Task 10.7: Execute DROP DOMAIN
- [x] Task 10.8: Execute SHOW DOMAIN
- [x] Tasks 10.9-10.12: WITH block enforcement

### Section 11: Transaction Extensions (10/10)
- [x] Tasks 11.1-11.10: Parse + emit extended transaction features

### Section 12: Quick Wins (5/5)
- [x] Tasks 12.1-12.5: Firebird window specs, MySQL NULL-safe, ESCAPE, placeholders, GROUP BY

### Section 13: Testing (partial)
- [ ] Tasks 13.1-13.5: Comprehensive test coverage (V2 parser/bytecode tests added; emulated parser + negative tests pending)

### Section 14: Documentation (partial)
- [ ] Tasks 14.1-14.3: Update specs and create guides (domain payload doc added; remaining guides pending)

---

## Session Recovery Information

**In case of session error, the next AI should:**

1. Read this status document first
2. Read `/docs/findings/CRITICAL_SCHEMA_DATABASE_OPCODE_GAP.md` for historical context
3. Read `PLAN_04_PREREQUISITES.md` for full prerequisites list
4. Read `PLAN_04_IMPLEMENTATION_CHECKLIST.md` for task breakdown
5. Confirm emulated parser/domain DDL alignment tasks
6. Update this status document with any progress

**Key Context:**
- Plan 02B core implementation is complete; alignment/testing remains
- Plan 03B domain infrastructure is complete; verification is external-runner dependent
- Plan 04 remaining work centers on emulated parsers, semantic guardrails, conflict opcodes, and tests

---

**Last Updated:** 2026-01-03
**Next Update:** After emulated parser/domain validation progress
