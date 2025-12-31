# Plan 04 - Domain DDL Implementation Status

**Plan:** Domain DDL (CREATE/ALTER/DROP DOMAIN) with full WITH block support
**Version:** 1.0
**Last Updated:** 2025-12-31
**Overall Status:** ⚠️ WAITING ON PLAN 03B - Schema/Database DDL core complete

---

## Quick Summary

| Phase | Status | Details |
|-------|--------|---------|
| **Specification** | ✅ COMPLETE | All specifications written and reviewed |
| **Prerequisites** | ⚠️ PARTIAL | Plan 02B resolved; Plan 03B pending |
| **Implementation** | ❌ NOT STARTED | Awaiting Plan 03B |

---

## Work Completed (2025-12-21 to 2025-12-26)

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

---

## Current Dependencies

### 🟡 Dependency #1: Plan 03B Domain Infrastructure

**Status:** BLOCKING - Required before Plan 04 implementation
**Severity:** CRITICAL

**Why it matters:**
- WITH blocks (SECURITY/INTEGRITY/VALIDATION/QUALITY) require the Plan 03B infrastructure.
- Domain enforcement depends on audit, masking, validation, and quality pipelines.

**Dependencies:**
- Plan 03B (Domain Infrastructure) - 138-192 hours

**Note:** Plan 02B (Schema/Database DDL) is now core complete; remaining alignment/testing is tracked separately.

**User Decision Required:** Approval to begin Plan 03B.

**Detailed Analysis:** `/docs/findings/CRITICAL_SCHEMA_DATABASE_OPCODE_GAP.md`

---

### 🟡 Dependency #2: Emulation Schema Path Alignment

**Status:** IN PROGRESS - Canonical path resolved; adapter/compiler alignment pending
**Severity:** HIGH

**Current State:**
- Canonical path is `remote.emulated.<dialect>.<server>.<db>` (dot-path).
- MySQL/PostgreSQL parsers normalize slash paths to dot paths.
- Adapters and query compilers still use slash defaults in places.

**Remaining Work:**
- Align adapters and query compilers to dot-path defaults.
- Remove legacy `emulation.<dialect>` references where present.

**Detailed Analysis:** `docs/audit/AUDIT_SCHEMA_DATABASE_DDL_GAP.md`

---

## Prerequisites Status

### Plan 03B: Domain Infrastructure (138-192 hours)

**Status:** NOT STARTED - Awaiting user approval
**Assigned To:** This AI (Plan 04 team)
**Dependencies:** Plan 02B core complete; remaining alignment/testing can proceed in parallel

**Key Components:**
- ✅ Specification complete (35 tasks defined)
- ❌ Implementation not started
- ❌ Testing not started

**Waiting For:** User approval to begin

### Plan 02B: Schema/Database DDL (60-80 hours)

**Status:** CORE COMPLETE - See `docs/planning/PLAN_02B_SCHEMA_DATABASE_DDL.md`
**Critical For:** Plan 04 testing capability

**Delivered:**
- EXT_CREATE/EXT_DROP/EXT_ALTER SCHEMA and DATABASE opcodes + handlers
- PostgreSQL/MySQL parser emission (schema/database DDL)
- Firebird CREATE/DROP/ALTER DATABASE support (rename only for ALTER)
- Emulated database lifecycle + view generation integration

**Remaining:** Adapter/query compiler path alignment, cascade semantics, tests

---

## Next Steps

1. Begin Plan 03B (Domain Infrastructure).
2. Track remaining Plan 02B alignment/testing items (path defaults, cascade semantics, tests).
3. Start Plan 04 after Plan 03B completion.

**Timeline:** 138-192 hours before Plan 04 starts (Plan 03B).

---

## Files Modified/Created

### Specifications
- ✅ `/docs/specifications/DDL_DOMAINS_COMPREHENSIVE.md` (NEW)
- ✅ `/docs/planning/PLAN_04_IMPLEMENTATION_CHECKLIST.md` (NEW)
- ✅ `/docs/planning/PLAN_03B_DOMAIN_INFRASTRUCTURE.md` (NEW)
- ✅ `/docs/planning/PLAN_04_PREREQUISITES.md` (NEW)

### Findings
- ✅ `/docs/findings/CRITICAL_SCHEMA_DATABASE_OPCODE_GAP.md` (NEW)

### Status Tracking
- ✅ `/docs/planning/PLAN_04_STATUS.md` (THIS FILE - NEW)

---

## Implementation Progress (0/80 tasks)

**Current:** 0% complete (awaiting Plan 03B)

### Section 1: Schema Changes (0/1)
- [ ] Task 1.1: Add dialect_tag/compat_name to domain schema

### Section 2: SBLR Opcodes (0/3)
- [ ] Task 2.1: Define extended domain opcodes
- [ ] Task 2.2: Document SBLR payload structures
- [ ] Task 2.3: Define WITH block enforcement opcodes

### Section 3: AST Extensions (0/3)
- [ ] Task 3.1: Extend CreateDomainStmt
- [ ] Task 3.2: Create AlterDomainStmt
- [ ] Task 3.3: Create DropDomainStmt

### Section 4: ScratchBird V2 Parser (0/12)
- [ ] Tasks 4.1-4.7: parseCreateDomain() all types + WITH blocks
- [ ] Task 4.8: parseAlterDomain()
- [ ] Task 4.9: parseDropDomain()
- [ ] Task 4.10-4.12: Update dispatchers

### Section 5: Firebird Parser (0/3)
- [ ] Tasks 5.1-5.3: CREATE/ALTER/DROP DOMAIN

### Section 6: PostgreSQL Parser (0/3)
- [ ] Tasks 6.1-6.3: CREATE/ALTER/DROP DOMAIN

### Section 7: MySQL Parser (0/3)
- [ ] Tasks 7.1-7.3: Reject domain DDL with clear errors

### Section 8: Semantic Analyzer (0/7)
- [ ] Tasks 8.1-8.7: Analyze all domain types + ALTER/DROP

### Section 9: Bytecode Generator (0/7)
- [ ] Tasks 9.1-9.7: Emit all domain types + ALTER/DROP

### Section 10: Executor (0/12)
- [ ] Tasks 10.1-10.8: Execute all domain types + ALTER/DROP + SHOW
- [ ] Tasks 10.9-10.12: WITH block enforcement

### Section 11: Transaction Extensions (0/10)
- [ ] Tasks 11.1-11.10: Parse + emit extended transaction features

### Section 12: Quick Wins (0/5)
- [ ] Tasks 12.1-12.5: Firebird window specs, MySQL NULL-safe, ESCAPE, placeholders, GROUP BY

### Section 13: Testing (0/5)
- [ ] Tasks 13.1-13.5: Comprehensive test coverage

### Section 14: Documentation (0/3)
- [ ] Tasks 14.1-14.3: Update specs and create guides

---

## Questions for User

1. **Approval to start Plan 03B?**
   - Can we proceed with domain infrastructure implementation?

2. **Confirm Plan 02B alignment priorities?**
   - Adapter/query compiler path defaults
   - Cascade semantics for DROP SCHEMA/DATABASE
   - Dedicated tests for schema/database DDL

3. **Timeline expectations?**
   - 138-192 hours of prerequisites (Plan 03B) before Plan 04
   - Is this acceptable?

---

## Session Recovery Information

**In case of session error, the next AI should:**

1. Read this status document first
2. Read `/docs/findings/CRITICAL_SCHEMA_DATABASE_OPCODE_GAP.md` for historical context
3. Read `PLAN_04_PREREQUISITES.md` for full prerequisites list
4. Read `PLAN_04_IMPLEMENTATION_CHECKLIST.md` for task breakdown
5. Confirm Plan 03B scope and start if approved
6. Update this status document with any progress

**Key Context:**
- Plan 02B core implementation is complete; alignment/testing remains
- Plan 04 is waiting on Plan 03B domain infrastructure
- All specifications are complete and committed

---

**Last Updated:** 2025-12-31
**Next Update:** After Plan 03B approval or completion
