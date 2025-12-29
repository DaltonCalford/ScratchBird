# Plan 04 - Domain DDL Implementation Status

**Plan:** Domain DDL (CREATE/ALTER/DROP DOMAIN) with full WITH block support
**Version:** 1.0
**Last Updated:** 2025-12-26
**Overall Status:** ⚠️ BLOCKED - Awaiting Schema/Database DDL infrastructure

---

## Quick Summary

| Phase | Status | Details |
|-------|--------|---------|
| **Specification** | ✅ COMPLETE | All specifications written and reviewed |
| **Prerequisites** | ⚠️ BLOCKED | Critical blocker discovered (Schema/Database DDL missing) |
| **Implementation** | ❌ NOT STARTED | Cannot start until blocker resolved |

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

### ⚠️ Phase 3: Critical Blocker Discovery (2025-12-26)

1. **Schema/Database DDL Opcode Gap Identified** ⚠️
   - Discovered missing SBLR opcodes for schema/database management
   - EXT_CREATE_SCHEMA, EXT_DROP_SCHEMA, EXT_CREATE_DATABASE, EXT_DROP_DATABASE all MISSING
   - PostgreSQL parser uses wrong placeholder opcode
   - PostgreSQL parseCreateSchema() emits NO opcode (corrupt bytecode)
   - MySQL/Firebird parsers have stubs or missing implementations
   - No executor handlers for schema/database DDL
   - **Impact:** Blocks Plan 04 testing (domains are schema-scoped)
   - **Impact:** Blocks ALL emulated database support

2. **Documentation Created** ✅
   - `/docs/findings/CRITICAL_SCHEMA_DATABASE_OPCODE_GAP.md` - Detailed analysis
   - Updated `PLAN_04_PREREQUISITES.md` with blocker section
   - Updated `PLAN_04_IMPLEMENTATION_CHECKLIST.md` with blocker warning

---

## Current Blockers

### 🔴 BLOCKER #1: Schema/Database DDL Infrastructure Missing

**Status:** BLOCKING - Cannot proceed with Plan 04 implementation
**Severity:** CRITICAL
**Discovered:** 2025-12-26

**Problem:**
- Domains are schema-scoped
- SBLR opcodes for CREATE/DROP SCHEMA and CREATE/DROP DATABASE do not exist
- Emulated parsers cannot create test schemas for domain testing
- PostgreSQL parser generates corrupt SBLR bytecode

**Dependencies:**
- Plan 02B (Schema/Database DDL) - 60-80 hours
- Plan 03B (Domain Infrastructure) - 138-192 hours
- **Total:** 198-272 hours before Plan 04 can start

**Resolution Options:**
1. Wait for Plan 02B completion (other AI working on it)
2. Create minimal schema opcodes in Plan 04 scope
3. User provides interim fix

**User Decision Required:** How to proceed?

**Detailed Analysis:** `/docs/findings/CRITICAL_SCHEMA_DATABASE_OPCODE_GAP.md`

---

### 🔴 BLOCKER #2: Emulation Schema Path Normalization

**Status:** BLOCKING - Must resolve before any emulation DDL work
**Severity:** CRITICAL
**Discovered:** 2025-12-28 (audit)

**Problem:**
- Plans and adapters assume `remote.emulated.<dialect>.<server>.<db>`
- Catalog defaults include `emulation.<dialect>` under root
- MySQL parser/compiler uses slash paths (`/remote/emulated/mysql/...`)
- Catalog schema lookup splits by dot path, so dotted names created as a single schema are not discoverable

**Resolution Required:**
- Decide canonical emulation path format in Plan 02B and update adapters, parsers, and catalog usage.

**Detailed Analysis:** `docs/audit/AUDIT_SCHEMA_DATABASE_DDL_GAP.md`

---

## Prerequisites Status

### Plan 03B: Domain Infrastructure (138-192 hours)

**Status:** NOT STARTED - Awaiting user approval
**Assigned To:** This AI (Plan 04 team)
**Dependencies:** Plan 02B must complete first

**Key Components:**
- ✅ Specification complete (35 tasks defined)
- ❌ Implementation not started
- ❌ Testing not started

**Waiting For:** User approval to begin

### Plan 02B: Schema/Database DDL (60-80 hours)

**Status:** DRAFT - See `docs/planning/PLAN_02B_SCHEMA_DATABASE_DDL.md`
**Critical For:** Plan 04 testing capability

**Key Components Needed:**
- EXT_CREATE_SCHEMA opcode + handler
- EXT_DROP_SCHEMA opcode + handler
- EXT_CREATE_DATABASE opcode + handler
- EXT_DROP_DATABASE opcode + handler
- PostgreSQL parser fixes
- MySQL parser implementation
- Firebird parser implementation

**Waiting For:** Plan 02B execution and path decision

---

## Next Steps (Pending User Decision)

### Option A: Wait for Dependencies
1. Wait for Plan 02B completion (Schema/Database DDL)
2. Implement Plan 03B (Domain Infrastructure)
3. Begin Plan 04 implementation

**Timeline:** 198-272 hours before Plan 04 starts

### Option B: Proceed with Minimal Scope
1. Implement minimal schema opcodes in Plan 04
2. Begin Plan 04 with basic domain types only
3. Add WITH blocks when Plan 03B completes

**Issue:** Violates separation of concerns
**Issue:** May duplicate work from Plan 02B

### Option C: User Provides Interim Fix
1. User provides working schema DDL opcodes
2. Begin Plan 04 implementation
3. Integrate with Plan 02B/03B when ready

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

**Current:** 0% complete (blocked before start)

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

1. **How should we proceed with the Schema/Database DDL blocker?**
   - Wait for Plan 02B?
   - Implement minimal schema opcodes in Plan 04?
   - Other solution?

2. **Are other AIs working on Plan 02 and Plan 03 as mentioned?**
   - What is their status?
   - Can we coordinate with them?

3. **Is the emulation architecture understanding correct?**
   - CREATE DATABASE → schema under emulation tree?
   - DROP DATABASE → CASCADE prune schema tree?

4. **Timeline expectations?**
   - 198-272 hours of prerequisites before Plan 04
   - Is this acceptable?

---

## Session Recovery Information

**In case of session error, the next AI should:**

1. Read this status document first
2. Read `/docs/findings/CRITICAL_SCHEMA_DATABASE_OPCODE_GAP.md` for blocker details
3. Read `PLAN_04_PREREQUISITES.md` for full prerequisites list
4. Read `PLAN_04_IMPLEMENTATION_CHECKLIST.md` for task breakdown
5. Check with user on blocker resolution before proceeding
6. Update this status document with any progress

**Key Context:**
- Plan 04 is BLOCKED by missing Schema/Database DDL infrastructure
- All specifications are complete and committed
- Awaiting user decision on how to proceed
- DO NOT start implementation until blocker is resolved

---

**Last Updated:** 2025-12-26 by Claude Code
**Next Update:** After user decision on blocker resolution
