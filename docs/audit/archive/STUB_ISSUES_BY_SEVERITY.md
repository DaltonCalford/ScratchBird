# ScratchBird Stub/Incomplete Implementation Issues

**Date:** December 2, 2025
**Total Issues Found:** 107

---

## HIGH SEVERITY (45 issues)

These are critical issues where features are advertised/documented but don't work.

### TOAST Storage (9 issues)
| ID | Location | Line(s) | Issue |
|----|----------|---------|-------|
| TOAST-1 | catalog_manager.cpp | 9914, 9956 | User password_hash not stored in TOAST |
| TOAST-2 | catalog_manager.cpp | 10017 | User password updates not saved |
| TOAST-3 | catalog_manager.cpp | 9957, 9987 | user_metadata read as empty string |
| TOAST-4 | catalog_manager.cpp | 10141-10142 | listUsers returns empty password/metadata |
| TOAST-5 | catalog_manager.cpp | 10198, 10236, 10264 | Role metadata not stored/read |
| TOAST-6 | catalog_manager.cpp | 10699, 10737, 10765 | Group metadata not stored/read |
| TOAST-7 | catalog_manager.cpp | 10862 | listGroups returns empty metadata |
| TOAST-8 | catalog_manager.cpp | 9738, 9763-9765 | Comments not stored in TOAST |
| TOAST-9 | catalog_manager.cpp | 12049 | Policy roles_str not stored |

### Catalog Operations (5 issues)
| ID | Location | Line(s) | Issue |
|----|----------|---------|-------|
| CAT-1 | catalog_manager.cpp | 9186-9199 | refreshMaterializedView is a no-op |
| CAT-2 | catalog_manager.cpp | 9202-9262 | refreshMaterializedViewWithStrategy stub |
| CAT-3 | catalog_manager.cpp | 1637-1667 | resolveOwnerUUID returns zero for all |
| CAT-4 | catalog_manager.cpp | 6466 | startOnlineMigration uses hardcoded XID=1 |
| CAT-5 | catalog_manager.cpp | 4520, 4545 | dropTablespace FORCE not implemented |

### SQL Executor (14 issues)
| ID | Location | Line(s) | Issue |
|----|----------|---------|-------|
| EXEC-1 | executor.cpp | 23671 | STDDEV_SAMP scalar throws error |
| EXEC-2 | executor.cpp | 23678 | STDDEV_POP scalar throws error |
| EXEC-3 | executor.cpp | 23685 | VAR_SAMP scalar throws error |
| EXEC-4 | executor.cpp | 23692 | VAR_POP scalar throws error |
| EXEC-5 | executor.cpp | 23699 | CORR throws error |
| EXEC-6 | executor.cpp | 23706 | COVAR_POP scalar throws error |
| EXEC-7 | executor.cpp | 24623 | ENCODE throws error |
| EXEC-8 | executor.cpp | 24629 | DECODE throws error |
| EXEC-9 | executor.cpp | 4678-4694 | GENERATED columns return NULL |
| EXEC-10 | executor.cpp | 23223-23255 | Complex DEFAULT expressions → NULL |
| EXEC-11 | executor.cpp | 25619-26135 | All 4 GiST operations blocked |
| EXEC-12 | executor.cpp | 8776-8781 | Window function arguments error |
| EXEC-13 | executor.cpp | 9003-9009 | NTH_VALUE always NULL |
| EXEC-14 | executor.cpp | 11361-11362 | Aggregates in scalar context error |

### Parser/Bytecode (5 issues)
| ID | Location | Line(s) | Issue |
|----|----------|---------|-------|
| PARSE-1 | parser.cpp | 1433-1434 | CHECK table constraints error |
| PARSE-2 | parser.cpp | 2337 | Assignment := operator error |
| PARSE-3 | parser.cpp | 5601 | IN (value list) error |
| PARSE-4 | expression_evaluator.cpp | 545-577 | evaluateForTuple throws |
| PARSE-5 | expression_evaluator.cpp | 579-611 | evaluatePredicateForTuple throws |

### Server/Client (2 issues)
| ID | Location | Line(s) | Issue |
|----|----------|---------|-------|
| NET-1 | connection.cpp | 920-936 | PreparedStatement params ignored |
| NET-2 | server_session.cpp | 334-339 | Savepoints return NOT_IMPLEMENTED |

### CLI Tools (5 issues)
| ID | Location | Line(s) | Issue |
|----|----------|---------|-------|
| CLI-1 | sb_verify.cpp | 187 | isValidAlphaPageSize() undefined |
| CLI-2 | sb_verify.cpp | 277 | validatePageChecksum() undefined |
| CLI-3 | sb_verify.cpp | 435-436 | --repair flag never used |
| CLI-4 | sb_backup.cpp | 73, 250 | Compression flag ignored |
| CLI-5 | sb_security.cpp | 99-102 | GRANT/REVOKE commands missing |

### Optimizer (5 issues)
| ID | Location | Line(s) | Issue |
|----|----------|---------|-------|
| OPT-1 | statistics_manager.cpp | 1160-1187 | storeColumnStatistics in-memory only |
| OPT-2 | statistics_manager.cpp | 1191-1215 | loadColumnStatistics always fails |
| OPT-3 | index_advisor.cpp | 406-421 | suggestIndexesForQuery returns empty |
| OPT-4 | mv_rewriter.cpp | 149-236 | findCandidates registry lookup missing |
| OPT-5 | mv_rewriter.cpp | 202-206 | Cannot retrieve MV info by ID |

---

## MEDIUM SEVERITY (43 issues)

These are incomplete features that work partially or have missing edge cases.

### Catalog Operations (8 issues)
| ID | Location | Line(s) | Issue |
|----|----------|---------|-------|
| CAT-M1 | catalog_manager.cpp | 1868, 1898, 1930 | dropSchema misses sequences |
| CAT-M2 | catalog_manager.cpp | 2298-2302 | createIndex root page update skipped |
| CAT-M3 | catalog_manager.cpp | 2367, 2417 | Expression index TOAST missing |
| CAT-M4 | catalog_manager.cpp | 4668, 4682 | alterTablespaceAutoextend not persisted |
| CAT-M5 | catalog_manager.cpp | 4754 | renameTablespace header not updated |
| CAT-M6 | catalog_manager.cpp | 9153 | dropView CASCADE check ignored |
| CAT-M7 | catalog_manager.cpp | 13215 | dropDomain dependency check missing |
| CAT-M8 | catalog_manager.cpp | 13990, 14226 | Emulation server/db cascade missing |

### Permission/RBAC (3 issues)
| ID | Location | Line(s) | Issue |
|----|----------|---------|-------|
| PERM-1 | catalog_manager.cpp | 11924 | hasPermission ignores roles/groups |
| PERM-2 | catalog_manager.cpp | 12580, 12628 | checkObjectPermissionFast same |
| PERM-3 | catalog_manager.cpp | 12329 | getPoliciesForUser returns all |

### SQL Executor (10 issues)
| ID | Location | Line(s) | Issue |
|----|----------|---------|-------|
| EXEC-M1 | executor.cpp | 24558 | UUID EXTRACT limited fields |
| EXEC-M2 | executor.cpp | 24585 | ARRAY EXTRACT limited fields |
| EXEC-M3 | executor.cpp | 20530, 20536 | SET/RESET SESSION AUTH error |
| EXEC-M4 | executor.cpp | 20570-20579 | SET CONSTRAINTS named errors |
| EXEC-M5 | executor.cpp | 20301, 20426 | REVOKE CASCADE not implemented |
| EXEC-M6 | executor.cpp | 22227-22233 | TOAST CHECK expressions blocked |
| EXEC-M7 | executor.cpp | 20050, 20229 | Object type lookup (non-table) |
| EXEC-M8 | executor.cpp | 8799-8804 | PARTITION BY expressions simplified |
| EXEC-M9 | executor.cpp | 6976 | DISTINCT for 2-var aggregates |
| EXEC-M10 | executor.cpp | 20002, 20181 | Schema-qualified names in GRANT |

### Parser/Bytecode (3 issues)
| ID | Location | Line(s) | Issue |
|----|----------|---------|-------|
| PARSE-M1 | bytecode_generator.cpp | 5140 | Window func direct codegen |
| PARSE-M2 | bytecode_generator.cpp | 5147 | Window spec direct codegen |
| PARSE-M3 | bytecode_generator.cpp | 5325 | ARRAY subqueries |

### Server/Client (3 issues)
| ID | Location | Line(s) | Issue |
|----|----------|---------|-------|
| NET-M1 | server_session.cpp | 367-371 | handleCancel returns error |
| NET-M2 | connection.cpp | 1020-1037 | releaseSavepoint no validation |
| NET-M3 | connection.cpp | 1039-1056 | rollbackTo no validation |

### Storage/Indexes (3 issues)
| ID | Location | Line(s) | Issue |
|----|----------|---------|-------|
| STOR-M1 | storage_engine.cpp | 429-440, 1522-1531 | Columnstore row OLTP |
| STOR-M2 | gist_index.cpp | 1264-1284 | Page allocation ad-hoc |
| STOR-M3 | gin_index.cpp | 3917, 4136 | Fuzzy matching placeholder |

### CLI Tools (6 issues)
| ID | Location | Line(s) | Issue |
|----|----------|---------|-------|
| CLI-M1 | sb_verify.cpp | 239-240 | Silently stops at 10k pages |
| CLI-M2 | sb_backup.cpp | 308-391 | Restore doesn't verify checksums |
| CLI-M3 | sb_backup.cpp | 284 | Only header checksummed |
| CLI-M4 | sb_security.cpp | 636-640 | All checks run same function |
| CLI-M5 | sb_security.cpp | 492-500 | Audit filter ignored |
| CLI-M6 | sb_isql.cpp | 388-422 | Multi-line file include broken |

### Optimizer (10 issues)
| ID | Location | Line(s) | Issue |
|----|----------|---------|-------|
| OPT-M1 | statistics_manager.cpp | 368 | num_pages always 0 |
| OPT-M2 | statistics_manager.cpp | 769 | Limited column type support |
| OPT-M3 | query_planner.cpp | 655-678 | isIndexApplicable always true |
| OPT-M4 | query_planner.cpp | 680-717 | isSpatialPredicate placeholder |
| OPT-M5 | query_planner.cpp | 1153-1168 | calculateQualCost hardcoded |
| OPT-M6 | query_planner.cpp | 1526-1558 | extractHashKeys simplified |
| OPT-M7 | cost_model.cpp | 157-159 | LSM merge cost missing |
| OPT-M8 | index_advisor.cpp | 736-756 | estimateIndexSize returns 0 |
| OPT-M9 | mv_rewriter.cpp | 212-213 | Staleness hardcoded 0 |
| OPT-M10 | mv_rewriter.cpp | 216 | MV cost hardcoded 50.0 |

---

## LOW SEVERITY (19 issues)

Enhancements, optimizations, and minor edge cases.

### Catalog (2 issues)
| ID | Location | Line(s) | Issue |
|----|----------|---------|-------|
| CAT-L1 | catalog_manager.cpp | 10806 | Group mapping cleanup missing |
| CAT-L2 | catalog_manager.cpp | 6752 | Migration history not persisted |

### SQL Executor (6 issues)
| ID | Location | Line(s) | Issue |
|----|----------|---------|-------|
| EXEC-L1 | executor.cpp | 20371 | GRANT WITH ADMIN hardcoded false |
| EXEC-L2 | executor.cpp | 20944 | SQL LIKE simplified matching |
| EXEC-L3 | executor.cpp | 13252 | Nested multi-geometry types |
| EXEC-L4 | executor.cpp | 24616 | EXTRACT unsupported types |
| EXEC-L5 | executor.cpp | 20002, 20181 | Schema-qualified names |
| EXEC-L6 | executor.cpp | 6976 | DISTINCT 2-var optimization |

### Storage/Indexes (3 issues)
| ID | Location | Line(s) | Issue |
|----|----------|---------|-------|
| STOR-L1 | hash_index.cpp | 1158-1159 | Empty overflow pages not freed |
| STOR-L2 | hash_index.cpp | 1220 | Overflow page count = 0 |
| STOR-L3 | btree.cpp | 2293 | Parent merge optimization |

### CLI Tools (5 issues)
| ID | Location | Line(s) | Issue |
|----|----------|---------|-------|
| CLI-L1 | sb_isql.cpp | 623-640 | Quote parsing simplified |
| CLI-L2 | sb_isql.cpp | 529-530 | Unknown meta-command handling |
| CLI-L3 | sb_isql.cpp | 787-789 | Password always prompted |
| CLI-L4 | sb_backup.cpp | 448-478 | Size display (reflects compression stub) |
| CLI-L5 | sb_verify.cpp | 344-381 | Report doesn't explain severity |

### Server/Client (1 issue)
| ID | Location | Line(s) | Issue |
|----|----------|---------|-------|
| NET-L1 | connection.cpp | 653-656 | Transaction status not parsed |

### Optimizer (2 issues)
| ID | Location | Line(s) | Issue |
|----|----------|---------|-------|
| OPT-L1 | statistics_manager.cpp | 456-461 | Cache invalidation clears all |
| OPT-L2 | query_planner.cpp | 899-1010 | Filter/condition placeholder strings |

---

## Summary Statistics

| Severity | Count | Percentage |
|----------|-------|------------|
| HIGH | 45 | 42% |
| MEDIUM | 43 | 40% |
| LOW | 19 | 18% |
| **TOTAL** | **107** | 100% |

### By Component

| Component | HIGH | MEDIUM | LOW | Total |
|-----------|------|--------|-----|-------|
| Catalog Manager | 14 | 11 | 2 | 27 |
| SQL Executor | 14 | 10 | 6 | 30 |
| Parser/Bytecode | 5 | 3 | 0 | 8 |
| Server/Client | 2 | 3 | 1 | 6 |
| Storage/Indexes | 0 | 3 | 3 | 6 |
| CLI Tools | 5 | 6 | 5 | 16 |
| Optimizer | 5 | 10 | 2 | 17 |

---

**Last Updated:** December 2, 2025
