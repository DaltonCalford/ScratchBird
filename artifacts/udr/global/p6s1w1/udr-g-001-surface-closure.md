# UDR-G-001 Parser/Feature Surface Closure Audit
Last-Modified: 2026-02-23

## Scope
Audit `G1` closure against the mandatory section-21 native remote SQL feature set and current canonical parser/opcode registrations.

Reference surface:
1. Section-21 native UDR remote SQL surface (mandatory `F_FDW_*` + `F_REMOTE_*` keys).
2. Section-28 parser remote connector checklist (`PR00..PR12`).

## Implemented in This Cycle
1. Added full FDW + REMOTE opcode identity mappings (including alter/import/show/execute/txn forms):
   - `src/sblr/v3_opcode_identity.cpp:74`
2. Added canonical feature rows for full section-21 remote surface:
   - `src/sblr/v3_canonical_feature_map.generated.cpp:92`
3. Extended opcode identity coverage assertions:
   - `tests/unit/test_sblr_v3_opcode_identity.cpp:60`
4. Verified via focused test execution:
   - `artifacts/udr/catalog/p6s1w2/udr-foundation-focused-tests.log:1549`
5. Verified direct canonical feature/opcode identity tests:
   - `build/tests/scratchbird_tests --gtest_filter='SBLRV3CanonicalFeatureMap.LoadsAuthoritativeRows:SBLRV3OpcodeIdentity.MapsExpandedStatementFamilies'`

## Current Canonical Coverage (FG_FDW / FG_REMOTE)
Implemented feature keys (27):
1. `F_FDW_CREATE_WRAPPER`
2. `F_FDW_ALTER_WRAPPER`
3. `F_FDW_DROP_WRAPPER`
4. `F_FDW_CREATE_SERVER`
5. `F_FDW_ALTER_SERVER`
6. `F_FDW_USER_MAPPING`
7. `F_FDW_ALTER_USER_MAPPING`
8. `F_FDW_FOREIGN_TABLE`
9. `F_FDW_ALTER_FOREIGN_TABLE`
10. `F_FDW_DROP_SERVER`
11. `F_FDW_DROP_FOREIGN_TABLE`
12. `F_FDW_DROP_USER_MAPPING`
13. `F_FDW_IMPORT_SCHEMA`
14. `F_REMOTE_ANALYZE_METADATA`
15. `F_REMOTE_REFRESH_METADATA`
16. `F_REMOTE_SHOW_CAPABILITIES`
17. `F_REMOTE_SHOW_OBJECTS`
18. `F_REMOTE_SHOW_COLUMNS`
19. `F_REMOTE_SHOW_STATISTICS`
20. `F_REMOTE_EXECUTE`
21. `F_REMOTE_PREPARE`
22. `F_REMOTE_EXECUTE_PREPARED`
23. `F_REMOTE_DEALLOCATE_PREPARED`
24. `F_REMOTE_BEGIN_TXN`
25. `F_REMOTE_COMMIT_TXN`
26. `F_REMOTE_ROLLBACK_TXN`
27. `F_REMOTE_SHOW_SESSION_STATE`

## Gate Decision
1. `UDR-GATE-01`: PASS.
2. `G2` may proceed (`CTL_REMOTE` envelope routing + executor closure).
3. Remaining risk:
   - symbol/feature closure is complete, but runtime execution path remains dependent on `G2/G3`.
