# Alpha 1 Completion Master Plan

**Created:** December 2, 2025
**Purpose:** Complete all 107 identified stub/incomplete implementations before Alpha 2
**Status:** NOT STARTED

---

## Overview

This plan addresses all issues identified in the December 2, 2025 code audit. Every item must be completed regardless of severity - there are no exceptions or deferrals.

### Statistics

| Category | Issues | Est. Hours |
|----------|--------|------------|
| TOAST Integration | 9 | 24-32 |
| Catalog Operations | 13 | 32-40 |
| SQL Executor | 30 | 48-64 |
| Parser/Bytecode | 8 | 16-24 |
| Server/Client | 6 | 16-20 |
| Storage/Indexes | 6 | 12-16 |
| CLI Tools | 16 | 24-32 |
| Optimizer | 17 | 28-36 |
| **TOTAL** | **107** | **200-264** |

---

## Work Packages

### WP-1: TOAST Storage Integration (9 issues)
**File:** src/core/catalog_manager.cpp
**Tracking:** [WORKPACKAGE_WP1_TOAST.md](WORKPACKAGE_WP1_TOAST.md)

| ID | Function | Line | Task |
|----|----------|------|------|
| TOAST-1 | createUser | 9914 | Store password_hash via storeStringInToast() |
| TOAST-2 | updateUser | 10017 | Update password_hash in TOAST |
| TOAST-3 | getUser/getUserByName | 9956, 9987 | Load password_hash/user_metadata from TOAST |
| TOAST-4 | listUsers | 10141-10142 | Load password_hash/user_metadata for each |
| TOAST-5 | createRole/getRole | 10198, 10236, 10264 | Store/load role_metadata |
| TOAST-6 | createGroup/getGroup | 10699, 10737, 10765 | Store/load group_metadata |
| TOAST-7 | listGroups | 10862 | Load group_metadata for each |
| TOAST-8 | createComment/readCommentRecords | 9738, 9763 | Store/load comment_text |
| TOAST-9 | createPolicy | 12049 | Store roles_str in TOAST |

---

### WP-2: Catalog Manager Operations (13 issues)
**File:** src/core/catalog_manager.cpp
**Tracking:** [WORKPACKAGE_WP2_CATALOG.md](WORKPACKAGE_WP2_CATALOG.md)

| ID | Function | Line | Task |
|----|----------|------|------|
| CAT-1 | refreshMaterializedView | 9186-9199 | Execute view query, populate table |
| CAT-2 | refreshMaterializedViewWithStrategy | 9202-9262 | Implement COMPLETE/INCREMENTAL/FAST |
| CAT-3 | resolveOwnerUUID | 1637-1667 | Look up user in Users table |
| CAT-4 | startOnlineMigration | 6466 | Get XID from TransactionManager |
| CAT-5 | dropTablespace | 4520, 4545 | Implement FORCE, delete file |
| CAT-M1 | dropSchema | 1868, 1898, 1930 | Add sequence enumeration by schema |
| CAT-M2 | createIndex | 2298-2302 | Update root page index count |
| CAT-M3 | createIndex (expression) | 2367, 2417 | Extract columns, TOAST large exprs |
| CAT-M4 | alterTablespaceAutoextend | 4668, 4682 | Validate MAXSIZE, persist header |
| CAT-M5 | renameTablespace | 4754 | Update tablespace file header |
| CAT-M6 | dropView | 9153 | Check dependent views if !CASCADE |
| CAT-M7 | dropDomain | 13215 | Check dependent columns |
| CAT-M8 | deleteEmulationServer/Database | 13990, 14226 | Check/cascade dependencies |
| CAT-L1 | deleteGroup | 10806 | Add group mapping cleanup |
| CAT-L2 | completeMigration | 6752 | Persist migration history |

---

### WP-3: Permission/RBAC (3 issues)
**File:** src/core/catalog_manager.cpp
**Tracking:** [WORKPACKAGE_WP3_RBAC.md](WORKPACKAGE_WP3_RBAC.md)

| ID | Function | Line | Task |
|----|----------|------|------|
| PERM-1 | hasPermission | 11924 | Check role/group memberships |
| PERM-2 | checkObjectPermissionFast | 12580, 12628 | Check role/group memberships |
| PERM-3 | getPoliciesForUser | 12329 | Filter by user's roles |

---

### WP-4: SQL Executor - Functions (14 issues)
**File:** src/sblr/executor.cpp
**Tracking:** [WORKPACKAGE_WP4_EXEC_FUNCTIONS.md](WORKPACKAGE_WP4_EXEC_FUNCTIONS.md)

| ID | Function | Line | Task |
|----|----------|------|------|
| EXEC-1 | executeStdDevSamp | 23671 | Implement scalar version |
| EXEC-2 | executeStdDevPop | 23678 | Implement scalar version |
| EXEC-3 | executeVarSamp | 23685 | Implement scalar version |
| EXEC-4 | executeVarPop | 23692 | Implement scalar version |
| EXEC-5 | executeCorr | 23699 | Implement scalar version |
| EXEC-6 | executeCovarPop | 23706 | Implement scalar version |
| EXEC-7 | executeEncode | 24623 | Implement base64/hex/escape |
| EXEC-8 | executeDecode | 24629 | Implement base64/hex/escape |
| EXEC-M1 | EXTRACT UUID | 24558 | Add all UUID fields |
| EXEC-M2 | EXTRACT ARRAY | 24585 | Add all ARRAY fields |
| EXEC-L1 | GRANT WITH ADMIN | 20371 | Parse from bytecode |
| EXEC-L2 | SQL LIKE pattern | 20944 | Implement %, _, escape |
| EXEC-L3 | ST_AsWKT multi-geometry | 13252 | Handle MULTI* types |
| EXEC-L4 | EXTRACT unsupported types | 24616 | Add remaining types |

---

### WP-5: SQL Executor - Features (16 issues)
**File:** src/sblr/executor.cpp
**Tracking:** [WORKPACKAGE_WP5_EXEC_FEATURES.md](WORKPACKAGE_WP5_EXEC_FEATURES.md)

| ID | Feature | Line | Task |
|----|---------|------|------|
| EXEC-9 | GENERATED columns | 4678-4694 | Evaluate expression bytecode |
| EXEC-10 | Complex DEFAULT | 23223-23255 | Evaluate NOW(), RANDOM(), etc. |
| EXEC-11 | GiST operations | 25619-26135 | Implement or integrate properly |
| EXEC-12 | Window function args | 8776-8781 | Parse and store arguments |
| EXEC-13 | NTH_VALUE | 9003-9009 | Evaluate nth argument |
| EXEC-14 | Aggregates in scalar | 11361-11362 | Support in expressions |
| EXEC-M3 | SET/RESET SESSION AUTH | 20530, 20536 | Add session user tracking |
| EXEC-M4 | SET CONSTRAINTS named | 20570-20579 | Add constraint name lookup |
| EXEC-M5 | REVOKE CASCADE | 20301, 20426 | Cascade to grantees |
| EXEC-M6 | TOAST CHECK exprs | 22227-22233 | Load from TOAST |
| EXEC-M7 | Object type lookup | 20050, 20229 | Support non-table objects |
| EXEC-M8 | PARTITION BY exprs | 8799-8804 | Full expression support |
| EXEC-M9 | DISTINCT 2-var aggs | 6976 | Handle DISTINCT properly |
| EXEC-M10 | Schema-qualified GRANT | 20002, 20181 | Parse schema.table |
| EXEC-L5 | Schema-qualified names | 20002, 20181 | Same as above |
| EXEC-L6 | DISTINCT 2-var opt | 6976 | Same as EXEC-M9 |

---

### WP-6: Parser & Bytecode Generator (8 issues)
**Files:** src/parser/parser.cpp, src/sblr/bytecode_generator.cpp, src/sblr/expression_evaluator.cpp
**Tracking:** [WORKPACKAGE_WP6_PARSER.md](WORKPACKAGE_WP6_PARSER.md)

| ID | Feature | File | Line | Task |
|----|---------|------|------|------|
| PARSE-1 | CHECK constraints | parser.cpp | 1433-1434 | Implement parsing |
| PARSE-2 | Assignment := | parser.cpp | 2337 | Add to lexer |
| PARSE-3 | IN (value list) | parser.cpp | 5601 | Implement parsing |
| PARSE-4 | evaluateForTuple | expression_evaluator.cpp | 545-577 | Full implementation |
| PARSE-5 | evaluatePredicateForTuple | expression_evaluator.cpp | 579-611 | Full implementation |
| PARSE-M1 | Window func direct | bytecode_generator.cpp | 5140 | Implement codegen |
| PARSE-M2 | Window spec direct | bytecode_generator.cpp | 5147 | Implement codegen |
| PARSE-M3 | ARRAY subqueries | bytecode_generator.cpp | 5325 | Implement codegen |

---

### WP-7: Server & Client (6 issues)
**Files:** src/server/server_session.cpp, src/client/connection.cpp
**Tracking:** [WORKPACKAGE_WP7_NETWORK.md](WORKPACKAGE_WP7_NETWORK.md)

| ID | Feature | File | Line | Task |
|----|---------|------|------|------|
| NET-1 | PreparedStatement params | connection.cpp | 920-936 | Implement substitution |
| NET-2 | Savepoints | server_session.cpp | 334-339 | Wire to TransactionManager |
| NET-M1 | handleCancel | server_session.cpp | 367-371 | Implement cancellation |
| NET-M2 | releaseSavepoint validation | connection.cpp | 1020-1037 | Validate response |
| NET-M3 | rollbackTo validation | connection.cpp | 1039-1056 | Validate response |
| NET-L1 | Transaction status | connection.cpp | 653-656 | Parse and apply status |

---

### WP-8: Storage & Indexes (6 issues)
**Files:** Various in src/core/
**Tracking:** [WORKPACKAGE_WP8_STORAGE.md](WORKPACKAGE_WP8_STORAGE.md)

| ID | Feature | File | Line | Task |
|----|---------|------|------|------|
| STOR-M1 | Columnstore row OLTP | storage_engine.cpp | 429-440, 1522-1531 | Buffer rows, batch insert |
| STOR-M2 | GiST page allocation | gist_index.cpp | 1264-1284 | Use proper page list |
| STOR-M3 | GIN fuzzy matching | gin_index.cpp | 3917, 4136 | Full BK-tree impl |
| STOR-L1 | Hash overflow cleanup | hash_index.cpp | 1158-1159 | Free empty pages |
| STOR-L2 | Hash overflow stats | hash_index.cpp | 1220 | Count overflow pages |
| STOR-L3 | B-tree parent merge | btree.cpp | 2293 | Implement optimization |

---

### WP-9: CLI Tools (16 issues)
**Files:** src/cli/sb_*.cpp
**Tracking:** [WORKPACKAGE_WP9_CLI.md](WORKPACKAGE_WP9_CLI.md)

| ID | Tool | Issue | Line | Task |
|----|------|-------|------|------|
| CLI-1 | sb_verify | isValidAlphaPageSize | 187 | Define or include |
| CLI-2 | sb_verify | validatePageChecksum | 277 | Define or include |
| CLI-3 | sb_verify | --repair flag | 435-436 | Implement repair |
| CLI-4 | sb_backup | Compression | 73, 250 | Implement LZ4 |
| CLI-5 | sb_security | GRANT/REVOKE | 99-102 | Implement handlers |
| CLI-M1 | sb_verify | Page limit | 239-240 | Remove limit or warn |
| CLI-M2 | sb_backup | Restore checksums | 308-391 | Verify before write |
| CLI-M3 | sb_backup | Page checksums | 284 | Checksum each page |
| CLI-M4 | sb_security | Check routing | 636-640 | Route to specific checks |
| CLI-M5 | sb_security | Audit filter | 492-500 | Implement filtering |
| CLI-M6 | sb_isql | Multi-line include | 388-422 | Accumulate statements |
| CLI-L1 | sb_isql | Quote parsing | 623-640 | Handle escapes |
| CLI-L2 | sb_isql | Unknown meta-cmd | 529-530 | Return false |
| CLI-L3 | sb_isql | Password prompt | 787-789 | Add --no-password |
| CLI-L4 | sb_backup | Size display | 448-478 | Show actual compression |
| CLI-L5 | sb_verify | Report severity | 344-381 | Add remediation guidance |

---

### WP-10: Optimizer (17 issues)
**Files:** src/optimizer/*.cpp
**Tracking:** [WORKPACKAGE_WP10_OPTIMIZER.md](WORKPACKAGE_WP10_OPTIMIZER.md)

| ID | Feature | File | Line | Task |
|----|---------|------|------|------|
| OPT-1 | storeColumnStatistics | statistics_manager.cpp | 1160-1187 | Persist to catalog |
| OPT-2 | loadColumnStatistics | statistics_manager.cpp | 1191-1215 | Load from catalog |
| OPT-3 | suggestIndexesForQuery | index_advisor.cpp | 406-421 | Parse and analyze |
| OPT-4 | findCandidates (MV) | mv_rewriter.cpp | 149-236 | Registry lookup |
| OPT-5 | MV info retrieval | mv_rewriter.cpp | 202-206 | Get view by ID |
| OPT-M1 | num_pages stats | statistics_manager.cpp | 368 | Calculate from table |
| OPT-M2 | Column type support | statistics_manager.cpp | 769 | Add all types |
| OPT-M3 | isIndexApplicable | query_planner.cpp | 655-678 | Actual predicate check |
| OPT-M4 | isSpatialPredicate | query_planner.cpp | 680-717 | Real function names |
| OPT-M5 | calculateQualCost | query_planner.cpp | 1153-1168 | Traverse expression |
| OPT-M6 | extractHashKeys | query_planner.cpp | 1526-1558 | Verify table ownership |
| OPT-M7 | LSM merge cost | cost_model.cpp | 157-159 | K-way merge estimate |
| OPT-M8 | estimateIndexSize | index_advisor.cpp | 736-756 | Fallback estimate |
| OPT-M9 | MV staleness | mv_rewriter.cpp | 212-213 | Calculate from metadata |
| OPT-M10 | MV cost | mv_rewriter.cpp | 216 | Estimate from stats |
| OPT-L1 | invalidateCache | statistics_manager.cpp | 456-461 | Targeted invalidation |
| OPT-L2 | Filter placeholders | query_planner.cpp | 899-1010 | Real expressions |

---

## Execution Order

### Phase 1: Foundation (WP-1, WP-7 partial)
1. **WP-1: TOAST Integration** - Security-critical, blocks user management
2. **NET-1: PreparedStatement params** - Security risk

### Phase 2: Core Functionality (WP-2, WP-4, WP-5 partial)
3. **CAT-1, CAT-2: MV Refresh** - Core feature
4. **EXEC-9, EXEC-10: GENERATED/DEFAULT** - Common SQL
5. **EXEC-11: GiST operations** - Complete or remove

### Phase 3: SQL Completeness (WP-4, WP-5, WP-6)
6. **Parser additions** - CHECK, :=, IN (value list)
7. **Window function fixes** - Arguments, NTH_VALUE
8. **Remaining executor issues**

### Phase 4: Infrastructure (WP-3, WP-7, WP-8, WP-10)
9. **RBAC fixes** - Role/group checks
10. **Server/client** - Savepoints, cancellation
11. **Optimizer** - Statistics persistence
12. **Storage** - Index optimizations

### Phase 5: Polish (WP-9, remaining items)
13. **CLI tools** - All 16 issues
14. **Remaining LOW severity items**

---

## Success Criteria

- [ ] All 107 issues addressed
- [ ] All tests continue to pass (1020/1020)
- [ ] No regressions in existing functionality
- [ ] Each work package verified independently
- [ ] Documentation updated for any API changes

---

## Progress Tracking

| Work Package | Total | Complete | Remaining | Status |
|--------------|-------|----------|-----------|--------|
| WP-1: TOAST | 9 | 0 | 9 | NOT STARTED |
| WP-2: Catalog | 15 | 0 | 15 | NOT STARTED |
| WP-3: RBAC | 3 | 0 | 3 | NOT STARTED |
| WP-4: Exec Functions | 14 | 0 | 14 | NOT STARTED |
| WP-5: Exec Features | 16 | 16 | 0 | ✅ COMPLETE |
| WP-6: Parser | 8 | 0 | 8 | NOT STARTED |
| WP-7: Network | 6 | 0 | 6 | NOT STARTED |
| WP-8: Storage | 6 | 0 | 6 | NOT STARTED |
| WP-9: CLI | 16 | 0 | 16 | NOT STARTED |
| WP-10: Optimizer | 17 | 17 | 0 | ✅ COMPLETE |
| **TOTAL** | **110** | **33** | **77** | **30%** |

Note: 110 total due to some issues appearing in multiple categories for clarity.

---

**Last Updated:** December 4, 2025
**Next Review:** After Phase 1 completion
