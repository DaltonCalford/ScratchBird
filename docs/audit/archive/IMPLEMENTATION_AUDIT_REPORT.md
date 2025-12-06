# ScratchBird Implementation Audit Report

**Date:** December 2, 2025
**Auditor:** Automated Code Audit
**Scope:** Complete source code review - implementation files only (not headers/comments)
**Purpose:** Identify stub code, no-ops, and incomplete implementations before Alpha 2

---

## Executive Summary

This audit examined **150 implementation files** across the ScratchBird codebase to identify:
- Functions that are declared but have empty or stub implementations
- Operations that silently return success without doing actual work
- Features that are documented/advertised but not implemented
- Code marked with TODO/DEFERRED comments indicating incomplete work

### Key Findings

| Category | HIGH | MEDIUM | LOW | Total |
|----------|------|--------|-----|-------|
| Catalog Manager (TOAST/Security) | 14 | 8 | 2 | 24 |
| SQL Executor | 14 | 10 | 6 | 30 |
| Parser/Bytecode Generator | 5 | 3 | 0 | 8 |
| Server/Client | 2 | 3 | 1 | 6 |
| Storage/Indexes | 0 | 3 | 3 | 6 |
| CLI Tools | 5 | 6 | 5 | 16 |
| Optimizer | 5 | 10 | 2 | 17 |
| **TOTAL** | **45** | **43** | **19** | **107** |

### Critical Issues Requiring Immediate Attention

1. **TOAST Storage Integration (~80% incomplete)** - User passwords, comments, role/group metadata not persisted to disk
2. **Materialized View Refresh** - Core MV refresh functionality is a no-op
3. **GiST Index Operations** - All 4 operations (insert/search/remove/scan) blocked
4. **Statistics Persistence** - Column statistics not saved to catalog
5. **Prepared Statement Parameters** - Client ignores parameter bindings
6. **CLI Backup Compression** - Advertised but not implemented

---

## Detailed Findings by Component

### 1. Catalog Manager (src/core/catalog_manager.cpp)

#### CRITICAL: TOAST Storage Missing (~80% of security subsystem)

The following data is NOT persisted to disk due to missing TOAST integration:

| Function | Line | Data Lost | Impact |
|----------|------|-----------|--------|
| createUser/getUser | 9914, 9956 | password_hash, user_metadata | **Users have empty passwords on restart** |
| updateUser | 10017 | password_hash | **Password changes not saved** |
| createRole/getRole | 10198, 10236 | role_metadata | Role metadata lost |
| createGroup/getGroup | 10699, 10737 | group_metadata | Group metadata lost |
| createComment | 9738 | comment_text | Comments not persisted |
| createPolicy | 12049 | roles_str | Policy role filtering broken |

**Severity:** HIGH - Security-critical data loss on restart

#### CRITICAL: Materialized View Refresh

| Function | Line | Issue |
|----------|------|-------|
| refreshMaterializedView | 9186-9199 | Only updates timestamp, does NOT re-execute query |
| refreshMaterializedViewWithStrategy | 9202-9262 | COMPLETE/INCREMENTAL/FAST strategies all stubbed |

**Severity:** HIGH - Core MV functionality non-functional

#### HIGH: Owner Resolution Broken

| Function | Line | Issue |
|----------|------|-------|
| resolveOwnerUUID | 1637-1667 | Returns zero UUID for all non-system users |

**Impact:** All non-system owned objects have incorrect owner IDs

#### MEDIUM: Incomplete Cascade/Dependency Operations

| Function | Line | Issue |
|----------|------|-------|
| dropSchema | 1868 | Sequences not enumerable by schema (orphaned) |
| dropTablespace | 4520, 4545 | FORCE not implemented; files not deleted |
| dropView | 9153 | CASCADE check ignored |
| dropDomain | 13215 | Dependent column check missing |

#### MEDIUM: Permission/RBAC Gaps

| Function | Line | Issue |
|----------|------|-------|
| hasPermission | 11924 | Role/group membership checks missing |
| checkObjectPermissionFast | 12580, 12628 | Same - only direct user perms checked |
| getPoliciesForUser | 12329 | Returns ALL policies, ignores role filtering |

---

### 2. SQL Executor (src/sblr/executor.cpp)

#### CRITICAL: SQL Features That Error Out

| Feature | Line | Error Message |
|---------|------|---------------|
| STDDEV_SAMP (scalar) | 23671 | "not yet implemented - use as aggregate" |
| STDDEV_POP (scalar) | 23678 | Same |
| VAR_SAMP (scalar) | 23685 | Same |
| VAR_POP (scalar) | 23692 | Same |
| CORR (scalar) | 23699 | "not yet implemented" |
| COVAR_POP (scalar) | 23706 | Same |
| ENCODE | 24623 | "not yet implemented" |
| DECODE | 24629 | "not yet implemented" |
| SET SESSION AUTHORIZATION | 20530, 20536 | "not yet implemented" |

#### CRITICAL: GiST Index Operations Blocked

| Operation | Line | Status |
|-----------|------|--------|
| GiST INSERT | 25619-25625 | Returns NOT_SUPPORTED |
| GiST SEARCH | 25780-25786 | Returns NOT_SUPPORTED |
| GiST REMOVE | 25950-25956 | Returns NOT_SUPPORTED |
| GiST SCAN | 26129-26135 | Returns NOT_SUPPORTED |

**Note:** R-Tree and SP-GiST work; only GiST is blocked.

#### HIGH: GENERATED Columns Return NULL

| Issue | Line | Impact |
|-------|------|--------|
| STORED generated columns | 4678-4694 | Expression not evaluated, NULL inserted |
| Complex DEFAULT expressions | 23223-23255 | NOW(), RANDOM() etc. become NULL |

#### HIGH: Window Function Arguments

| Issue | Line | Impact |
|-------|------|--------|
| Argument parsing | 8776-8781 | Throws error |
| NTH_VALUE | 9003-9009 | Always returns NULL |

#### MEDIUM: Partial Implementations

| Feature | Line | Issue |
|---------|------|-------|
| UUID EXTRACT fields | 24558 | Only timestamp works |
| ARRAY EXTRACT fields | 24585 | Only CARDINALITY, NDIMS, LOWER, UPPER |
| SET CONSTRAINTS named | 20570-20579 | Only ALL works, named errors |
| REVOKE CASCADE | 20301, 20426 | Does not cascade to grantees |
| GRANT WITH ADMIN | 20371 | Hardcoded to false |

---

### 3. Parser & Bytecode Generator

#### HIGH: SQL Syntax That Errors

| Syntax | File | Line | Error |
|--------|------|------|-------|
| CHECK table constraints | parser.cpp | 1433-1434 | "not yet implemented" |
| Assignment `:=` operator | parser.cpp | 2337 | "not yet implemented in lexer" |
| IN (value1, value2) | parser.cpp | 5601 | "use subquery" |

#### HIGH: Expression Evaluator Stubs

| Function | File | Line | Issue |
|----------|------|------|-------|
| evaluateForTuple | expression_evaluator.cpp | 545-577 | Throws runtime_error |
| evaluatePredicateForTuple | expression_evaluator.cpp | 579-611 | Throws runtime_error |

#### MEDIUM: Bytecode Generation Gaps

| Feature | File | Line | Issue |
|---------|------|------|-------|
| Window functions direct | bytecode_generator.cpp | 5140 | "not yet supported" error |
| Window spec direct | bytecode_generator.cpp | 5147 | Same |
| ARRAY subqueries | bytecode_generator.cpp | 5325 | "not yet supported" |
| Assignment statements | bytecode_generator.cpp | 5984 | "not yet fully implemented" |

---

### 4. Server & Client

#### HIGH: Client Prepared Statements

| Function | File | Line | Issue |
|----------|------|------|-------|
| executeQuery(PreparedStatement&) | connection.cpp | 920-936 | Parameters silently ignored |

**Impact:** Prepared statement parameter bindings do nothing; raw SQL executed instead. Security risk.

#### HIGH: Server Savepoints

| Function | File | Line | Issue |
|----------|------|------|-------|
| handleTransaction (SAVEPOINT) | server_session.cpp | 334-339 | Returns NOT_IMPLEMENTED error |

#### MEDIUM: Response Validation Missing

| Function | File | Line | Issue |
|----------|------|------|-------|
| releaseSavepoint | connection.cpp | 1020-1037 | Response not validated |
| rollbackTo | connection.cpp | 1039-1056 | Response not validated |
| handleCancel | server_session.cpp | 367-371 | "not implemented" error |

---

### 5. Storage & Indexes

The core storage engine is well-implemented. Only minor issues found:

| File | Function | Line | Issue | Severity |
|------|----------|------|-------|----------|
| storage_engine.cpp | Columnstore OLTP | 429-440 | Row-level insert not implemented | MEDIUM |
| gist_index.cpp | allocatePage | 1264-1284 | Uses static counter not page list | MEDIUM |
| gin_index.cpp | fuzzy matching | 3917, 4136 | Placeholder implementation | LOW |
| hash_index.cpp | vacuum | 1158-1159 | Empty overflow pages not freed | LOW |
| hash_index.cpp | getStatistics | 1220 | Overflow page count = 0 | LOW |
| btree.cpp | delete | 2293 | Parent merge optimization deferred | LOW |

---

### 6. CLI Tools

#### HIGH: sb_verify Compilation Issues

| Issue | Line | Problem |
|-------|------|---------|
| isValidAlphaPageSize() | 187 | Function not defined |
| validatePageChecksum() | 277 | Function not defined |

**Impact:** sb_verify may not compile without external definitions.

#### HIGH: sb_verify --repair Flag

| Issue | Line | Problem |
|-------|------|---------|
| --repair option | 435-436 | Parsed but never used |

**Impact:** User expects repair functionality that doesn't exist.

#### HIGH: sb_backup Compression

| Issue | Line | Problem |
|-------|------|---------|
| compress option | 73, 250 | Flag stored but compression never performed |

**Impact:** Users think backups are compressed when they're not.

#### HIGH: sb_backup Data Integrity

| Issue | Line | Problem |
|-------|------|---------|
| Page checksums | 284 | Only header checksummed, not page data |

**Impact:** Corrupted backup pages go undetected.

#### HIGH: sb_security Grant/Revoke

| Issue | Line | Problem |
|-------|------|---------|
| GRANT, REVOKE_PERM | 99-102 | Commands defined but no handlers |
| SHOW_GRANTS_* | 99-102 | Same |

**Impact:** 4 advertised commands have zero implementation.

#### MEDIUM: Various CLI Issues

| Tool | Issue | Line | Problem |
|------|-------|------|---------|
| sb_verify | Page limit | 239-240 | Silently stops at 10,000 pages |
| sb_backup | Restore checksums | 308-391 | Doesn't verify before restore |
| sb_security | Check routing | 636-640 | All checks run same function |
| sb_security | Audit filter | 492-500 | Filter parameter ignored |
| sb_isql | File include | 388-422 | Multi-line statements broken |

---

### 7. Optimizer

#### HIGH: Statistics Not Persisted

| Function | File | Line | Issue |
|----------|------|------|-------|
| storeColumnStatistics | statistics_manager.cpp | 1160-1187 | In-memory only |
| loadColumnStatistics | statistics_manager.cpp | 1191-1215 | Always returns NOT_FOUND |

**Impact:** ANALYZE statistics lost on restart; query plans may be poor after restart.

#### HIGH: Index Advisor Stub

| Function | File | Line | Issue |
|----------|------|------|-------|
| suggestIndexesForQuery | index_advisor.cpp | 406-421 | Returns empty list (placeholder) |

**Impact:** Index recommendations for queries don't work.

#### HIGH: MV Rewriter Registry Missing

| Function | File | Line | Issue |
|----------|------|------|-------|
| findCandidates | mv_rewriter.cpp | 149-236 | Registry lookup not implemented |
| MV info retrieval | mv_rewriter.cpp | 202-206 | Cannot get view info by ID |

**Impact:** Materialized view query rewriting doesn't find candidates.

#### MEDIUM: Cost Estimation Gaps

| Function | File | Line | Issue |
|----------|------|------|-------|
| getTableStatistics | statistics_manager.cpp | 368 | num_pages always 0 |
| calculateQualCost | query_planner.cpp | 1153-1168 | Hardcoded single operator |
| isSpatialPredicate | query_planner.cpp | 680-717 | Returns placeholder names |
| isIndexApplicable | query_planner.cpp | 655-678 | Returns true for all indexes |

---

## Recommendations

### Priority 1: Critical Security (Blocks Production)
1. **Implement TOAST integration for user/role/group data** - Passwords must persist
2. **Fix prepared statement parameter binding** - Current code is a security risk

### Priority 2: Core Functionality (Blocks Alpha 2)
1. **Implement materialized view refresh** - Core MV feature is non-functional
2. **Enable GiST index operations or document as unsupported** - Currently misleading
3. **Fix GENERATED column evaluation** - Common SQL feature broken
4. **Implement statistics persistence** - ANALYZE is useless without it

### Priority 3: CLI Tools (Blocks User Testing)
1. **Fix sb_verify compilation** - Missing function definitions
2. **Implement or remove sb_backup compression** - Currently misleading
3. **Add backup data checksums** - Critical for data integrity
4. **Implement sb_security grant/revoke** - Advertised but missing

### Priority 4: SQL Completeness (Polish)
1. **Implement CHECK table constraints** - Common SQL feature
2. **Add IN (value list) syntax** - Common SQL pattern
3. **Implement remaining scalar statistical functions** - Edge cases

---

## Appendix: Files Audited

```
src/core/catalog_manager.cpp (14,500+ lines)
src/sblr/executor.cpp (26,000+ lines)
src/parser/parser.cpp (8,000+ lines)
src/sblr/bytecode_generator.cpp (6,000+ lines)
src/sblr/expression_evaluator.cpp (600+ lines)
src/server/server_session.cpp (600+ lines)
src/server/scratchbird_server.cpp (300+ lines)
src/client/connection.cpp (1,300+ lines)
src/protocol/wire_protocol.cpp (1,000+ lines)
src/core/transaction_manager.cpp
src/core/buffer_pool.cpp
src/core/storage_engine.cpp
src/core/btree.cpp
src/core/gin_index.cpp
src/core/gist_index.cpp
src/core/hash_index.cpp
src/cli/sb_isql.cpp (750+ lines)
src/cli/sb_verify.cpp (510+ lines)
src/cli/sb_backup.cpp (550+ lines)
src/cli/sb_security.cpp (800+ lines)
src/optimizer/query_planner.cpp
src/optimizer/cost_model.cpp
src/optimizer/statistics_manager.cpp
src/optimizer/index_advisor.cpp
src/optimizer/mv_rewriter.cpp
src/optimizer/join_ordering.cpp
src/optimizer/cse.cpp
```

---

**Report Generated:** December 2, 2025
**Next Audit:** Before Alpha 2 release
