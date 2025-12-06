# Required Fixes Before Alpha 2

**Date:** December 2, 2025
**Purpose:** Prioritized list of issues that must be addressed before proceeding to Alpha 2

---

## Critical Path Items (MUST FIX)

These issues represent security vulnerabilities, data loss risks, or core features that don't work.

### 1. TOAST Integration for Security Data

**Priority:** P0 - BLOCKER
**Estimated Effort:** 16-24 hours
**Files:** src/core/catalog_manager.cpp

The user authentication system is fundamentally broken because password hashes and metadata are not persisted to disk.

| What to Fix | Line | Current | Required |
|-------------|------|---------|----------|
| createUser | 9914 | password_hash_oid = 0 | Store via storeStringInToast() |
| getUser/getUserByName | 9956, 9986 | password_hash = "" | Read via loadStringFromToast() |
| updateUser | 10017 | No update | Update TOAST record |
| createRole/getRole | 10198, 10236 | role_metadata_oid = 0 | Store/load metadata |
| createGroup/getGroup | 10699, 10737 | group_metadata_oid = 0 | Store/load metadata |

**Impact if not fixed:** All users have empty passwords after database restart.

---

### 2. Prepared Statement Parameter Binding

**Priority:** P0 - SECURITY RISK
**Estimated Effort:** 4-8 hours
**Files:** src/client/connection.cpp

| What to Fix | Line | Current | Required |
|-------------|------|---------|----------|
| executeQuery(PreparedStatement&) | 920-936 | Ignores params | Substitute safely |

**Impact if not fixed:** SQL injection vulnerability; parameters silently ignored.

---

### 3. CLI Tool Compilation/Function Issues

**Priority:** P0 - BLOCKER
**Estimated Effort:** 2-4 hours
**Files:** src/cli/sb_verify.cpp

| What to Fix | Line | Current | Required |
|-------------|------|---------|----------|
| isValidAlphaPageSize() | 187 | Undefined | Define or include header |
| validatePageChecksum() | 277 | Undefined | Define or include header |

**Impact if not fixed:** sb_verify won't compile.

---

### 4. Statistics Persistence

**Priority:** P1 - HIGH
**Estimated Effort:** 8-12 hours
**Files:** src/optimizer/statistics_manager.cpp

| What to Fix | Line | Current | Required |
|-------------|------|---------|----------|
| storeColumnStatistics | 1160-1187 | In-memory cache only | Persist to pg_statistic |
| loadColumnStatistics | 1191-1215 | Returns NOT_FOUND | Load from pg_statistic |

**Impact if not fixed:** ANALYZE is useless; query plans degrade after restart.

---

### 5. Materialized View Refresh

**Priority:** P1 - HIGH
**Estimated Effort:** 16-24 hours
**Files:** src/core/catalog_manager.cpp

| What to Fix | Line | Current | Required |
|-------------|------|---------|----------|
| refreshMaterializedView | 9186-9199 | Only updates timestamp | Execute view query |
| refreshMaterializedViewWithStrategy | 9202-9262 | All strategies stub | Implement COMPLETE at minimum |

**Impact if not fixed:** Materialized views never get refreshed data.

---

## Should Fix Items (IMPORTANT)

### 6. GiST Index Operations

**Priority:** P2 - MEDIUM
**Estimated Effort:** 8-16 hours
**Files:** src/sblr/executor.cpp

| What to Fix | Lines | Current | Options |
|-------------|-------|---------|---------|
| GiST INSERT | 25619-25625 | NOT_SUPPORTED | Implement OR document as unsupported |
| GiST SEARCH | 25780-25786 | NOT_SUPPORTED | Same |
| GiST REMOVE | 25950-25956 | NOT_SUPPORTED | Same |
| GiST SCAN | 26129-26135 | NOT_SUPPORTED | Same |

**Recommendation:** If GiST won't be supported in Alpha 1, remove it from index type enum and document. R-Tree and SP-GiST work.

---

### 7. GENERATED Column Evaluation

**Priority:** P2 - MEDIUM
**Estimated Effort:** 8-12 hours
**Files:** src/sblr/executor.cpp

| What to Fix | Line | Current | Required |
|-------------|------|---------|----------|
| STORED generated columns | 4678-4694 | NULL | Evaluate expression |
| Complex DEFAULT expressions | 23223-23255 | NULL | Evaluate NOW(), RANDOM(), etc. |

---

### 8. Backup Compression

**Priority:** P2 - MEDIUM
**Estimated Effort:** 4-8 hours
**Files:** src/cli/sb_backup.cpp

| What to Fix | Line | Current | Options |
|-------------|------|---------|---------|
| compress option | 73, 250 | Flag stored, no compression | Implement LZ4/zlib OR remove option |

---

### 9. Backup Data Integrity

**Priority:** P2 - MEDIUM
**Estimated Effort:** 4-6 hours
**Files:** src/cli/sb_backup.cpp

| What to Fix | Line | Current | Required |
|-------------|------|---------|----------|
| Page checksums | 284 | Only header | Checksum each page |
| Restore verification | 308-391 | No verification | Verify before write |

---

### 10. Server Savepoints

**Priority:** P2 - MEDIUM
**Estimated Effort:** 8-12 hours
**Files:** src/server/server_session.cpp

| What to Fix | Line | Current | Required |
|-------------|------|---------|----------|
| handleTransaction SAVEPOINT | 334-339 | NOT_IMPLEMENTED | Wire to TransactionManager |

---

## Nice to Have (FUTURE)

These can be deferred to later phases:

### Parser/SQL Syntax
- CHECK table constraints (parser.cpp:1433)
- Assignment := operator (parser.cpp:2337)
- IN (value list) syntax (parser.cpp:5601)

### Permission System
- Role/group membership checks (catalog_manager.cpp:11924)
- REVOKE CASCADE (executor.cpp:20301)
- GRANT WITH ADMIN (executor.cpp:20371)

### Optimizer
- Index advisor query suggestions (index_advisor.cpp:406)
- MV rewriter registry (mv_rewriter.cpp:149)

### Scalar Statistical Functions
- STDDEV_SAMP/POP scalar (executor.cpp:23671-23678)
- VAR_SAMP/POP scalar (executor.cpp:23685-23692)
- CORR/COVAR_POP scalar (executor.cpp:23699-23706)

---

## Effort Summary

| Priority | Items | Total Hours |
|----------|-------|-------------|
| P0 (BLOCKER) | 3 | 22-36 |
| P1 (HIGH) | 2 | 24-36 |
| P2 (MEDIUM) | 5 | 32-54 |
| FUTURE | 12+ | TBD |
| **TOTAL (P0-P2)** | **10** | **78-126 hours** |

---

## Verification Checklist

After fixes, verify:

- [ ] User passwords persist across restart
- [ ] Prepared statement parameters work in client
- [ ] sb_verify compiles and runs
- [ ] ANALYZE statistics persist across restart
- [ ] REFRESH MATERIALIZED VIEW populates data
- [ ] Backup compression works (if kept)
- [ ] Backup restore verifies checksums
- [ ] Server savepoints work via client

---

**Last Updated:** December 2, 2025
