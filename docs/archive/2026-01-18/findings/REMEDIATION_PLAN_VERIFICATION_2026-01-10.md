# Remediation Plan Verification Audit (2026-01-12)

Scope: Verify items from `ScratchBird/docs/planning/CONSOLIDATED_FINDINGS_REMEDIATION_PLAN.md`
against code in `ScratchBird/`. Tests executed: `ctest --test-dir build` (2,053 tests, 42 skipped);
runtime-only behaviors still marked as needs-audit unless observed.

Legend: VERIFIED, PARTIAL, MISSING, DOC-ONLY, NEEDS-AUDIT

## F-001 Schema/Database DDL parity, cascade/force semantics, parser gaps
Status: PARTIAL
Implemented:
- DROP SCHEMA reads cascade flag and calls CatalogManager dropSchema (ScratchBird/src/sblr/executor.cpp:7055, ScratchBird/src/sblr/executor.cpp:7097).
- CatalogManager dropSchema implements RESTRICT counts + CASCADE drop traversal (ScratchBird/src/core/catalog_manager.cpp:5017).
- DROP DATABASE uses FORCE semantics and routes to dropSchema for ScratchBird dialect (ScratchBird/src/sblr/executor.cpp:8211, ScratchBird/src/sblr/executor.cpp:8429).
- MySQL ALTER DATABASE emits EXT_ALTER_DATABASE with options (ScratchBird/src/parser/mysql/mysql_parser.cpp:2986).
Missing / concerns:
- Firebird ALTER DATABASE options rejected; OWNER support TODO (ScratchBird/src/parser/firebird/firebird_parser.cpp:2134, ScratchBird/src/parser/firebird/firebird_parser.cpp:2142).
- MySQL ALTER DATABASE RENAME still rejected (ScratchBird/src/parser/mysql/mysql_parser.cpp:2995).
- DROP DATABASE uses FORCE flag for cascade; if spec requires separate CASCADE/RESTRICT semantics, this is not explicit.
Spec refs: ScratchBird/docs/specifications/ddl/DDL_SCHEMAS.md, ScratchBird/docs/specifications/ddl/DDL_DATABASES.md, ScratchBird/docs/specifications/ddl/CASCADE_DROP_SPECIFICATION.md, ScratchBird/docs/specifications/parser/MYSQL_PARSER_SPECIFICATION.md, ScratchBird/docs/specifications/reference/firebird/FirebirdReferenceDocument.md

## F-002 Datetime/UUID storage format documentation
Status: DOC-ONLY
Implemented:
- Documentation updated in archived findings; no code impact required.
Missing: None.

## F-003 Deadlock fix for dropFunction/dropProcedure
Status: VERIFIED
Implemented:
- Consistent lock order + internal helpers in dropFunction/dropProcedure (ScratchBird/src/core/catalog_manager.cpp:13111).
Missing: Runtime verification not executed.

## F-004 Dedicated ISQL clients + shared CLI library
Status: VERIFIED
Implemented:
- sb_fb_isql, sb_pg_isql, sb_my_isql targets wired in build (ScratchBird/src/CMakeLists.txt:745-794).
- Source files present: ScratchBird/src/cli/sb_fb_isql.cpp, ScratchBird/src/cli/sb_pg_isql.cpp, ScratchBird/src/cli/sb_my_isql.cpp, ScratchBird/src/cli/isql_common.h.
Missing: None observed.

## F-005 dropTable/dropSequence lock ordering fixes
Status: VERIFIED
Implemented:
- dropTable lock ordering documented + enforced via scoped_lock (ScratchBird/src/core/catalog_manager.cpp:13321).
- dropSequence lock ordering enforced (ScratchBird/src/core/catalog_manager.cpp:17147).
Missing / concerns:
- dropTable comment indicates RESTRICT-only policy; cascade flag unused (ScratchBird/src/core/catalog_manager.cpp:13321).
Spec refs: ScratchBird/docs/specifications/ddl/CASCADE_DROP_SPECIFICATION.md

## F-006 Executor transaction timeout analysis (autocommit + createTable locking)
Status: NEEDS-AUDIT
Implemented:
- Autocommit and lock-timeout wiring in executor (ScratchBird/src/sblr/executor.cpp:19868).
- Long transaction monitor initialized and started (ScratchBird/src/core/database.cpp:1020).
Missing:
- No runtime verification for timeouts/locking behaviors.
Spec refs: ScratchBird/docs/specifications/transaction/TRANSACTION_LOCK_MANAGER.md, ScratchBird/docs/specifications/sblr/Appendix_A_SBLR_BYTECODE.md

## F-007 Final test results + timeout enforcement + long transaction monitor init
Status: NEEDS-AUDIT
Implemented:
- CTest TIMEOUT properties configured (ScratchBird/tests/CMakeLists.txt:325).
- Long transaction monitor started (ScratchBird/src/core/database.cpp:1020).
Missing:
- CTest timeout enforcement not validated in this audit.
Spec refs: ScratchBird/docs/specifications/testing/ALPHA3_TEST_PLAN.md

## F-008 Findings remediation mapping
Status: DOC-ONLY
Implemented:
- Mapping is documented; no code to validate.
Missing: None.

## F-009 FK deadlock fix + constraint name lookup
Status: VERIFIED
Implemented:
- createForeignKey uses same lock order as dropTable and uses internal dependency helpers (ScratchBird/src/core/catalog_manager.cpp:28839).
Missing: Runtime verification not executed.

## F-010 sb_isql command line analysis (-i alias, -par parser selection)
Status: VERIFIED
Implemented:
- -i/--input alias and -par/--parser supported (ScratchBird/src/cli/sb_isql.cpp:2842, ScratchBird/src/cli/sb_isql.cpp:2972).
Missing: None observed.

## F-011 Session summary
Status: DOC-ONLY
Implemented: Documentation only.
Missing: None.

## F-012 SQL compatibility test repositories
Status: NEEDS-AUDIT
Implemented:
- Compatibility repo structure exists under ScratchBird/tests/compatibility/.
Missing:
- No runtime validation performed for repo wiring.
Spec refs: ScratchBird/docs/specifications/testing/ALPHA3_TEST_PLAN.md

## F-013 Test execution time analysis
Status: VERIFIED
Implemented:
- Per-test TIMEOUT properties configured in CMake (ScratchBird/tests/CMakeLists.txt:325).
- Full `ctest --test-dir build` run completed (2,053 tests; 42 skipped).
Missing:
- None observed in this audit.
Spec refs: ScratchBird/docs/specifications/testing/ALPHA3_TEST_PLAN.md

## F-014 Test fixes + resolver cache gaps
Status: NEEDS-AUDIT
Implemented:
- No direct code markers verified in this audit.
Missing:
- Requires test execution + resolver cache validation.
Spec refs: ScratchBird/docs/specifications/testing/ALPHA3_TEST_PLAN.md (no dedicated resolver-cache spec located).

## F-015 Test suite failures (missing executables)
Status: NEEDS-AUDIT
Implemented:
- sb_fb_isql/sb_pg_isql/sb_my_isql targets are present (ScratchBird/src/CMakeLists.txt:745-794).
Missing:
- Requires test execution to verify suite failures resolved.
Spec refs: ScratchBird/docs/specifications/testing/ALPHA3_TEST_PLAN.md

## F-016 Test suite split
Status: VERIFIED
Implemented:
- gtest_discover_tests labels for unit/smoke/stress/perf configured (ScratchBird/tests/CMakeLists.txt:325-365).
Missing:
- Runtime verification pending.
Spec refs: ScratchBird/docs/specifications/testing/ALPHA3_TEST_PLAN.md

## F-017 / F-018 Test timeout analysis + reanalysis
Status: NEEDS-AUDIT
Implemented:
- Lock ordering fixes present (see F-003, F-005, F-009).
Missing:
- Requires execution of regression tests.
Spec refs: ScratchBird/docs/specifications/testing/ALPHA3_TEST_PLAN.md

## F-019 Cluster compatibility audit
Status: MISSING
Implemented:
- Catalog server registry + cluster_id storage exist (ScratchBird/src/core/catalog_manager.cpp:18784).
Missing:
- Cluster routing/sharding implementation absent (no cluster module present).
Spec refs: ScratchBird/docs/specifications/Cluster Specification Work/SBCLUSTER-SUMMARY.md, ScratchBird/docs/specifications/Cluster Specification Work/SBCLUSTER-05-SHARDING.md, ScratchBird/docs/specifications/Cluster Specification Work/SBCLUSTER-06-DISTRIBUTED-QUERY.md

## F-021 Database lifecycle upgrade plan (cluster)
Status: NEEDS-AUDIT
Implemented:
- No code found; plan appears doc-only.
Missing:
- Requires cluster design/implementation.
Spec refs: ScratchBird/docs/specifications/Cluster Specification Work/SBCLUSTER-01-CLUSTER-CONFIG-EPOCH.md (no explicit upgrade-plan spec located).

## F-022 Domain support gaps (defaults, info_schema, TYPE_DOMAIN)
Status: PARTIAL
Implemented:
- Domain defaults applied during INSERT when column default is absent (ScratchBird/src/sblr/executor.cpp:10076, ScratchBird/src/sblr/executor.cpp:10818).
Missing:
- TYPE_DOMAIN column encoding and information_schema exposure not verified in this audit.
Spec refs: ScratchBird/docs/specifications/types/DDL_DOMAINS_COMPREHENSIVE.md, ScratchBird/docs/specifications/catalog/SYSTEM_CATALOG_STRUCTURE.md

## F-023 Engine gap report (COPY/EXPLAIN/STDIN/STDOUT)
Status: PARTIAL
Implemented:
- COPY supports file-based TO/FROM; error paths for STDIN/STDOUT are explicit (ScratchBird/src/sblr/executor.cpp:45668, ScratchBird/src/sblr/executor.cpp:45895).
Missing:
- COPY STDIN/STDOUT protocol paths remain unimplemented.
- COPY option coverage and richer EXPLAIN metadata not verified.
Spec refs: ScratchBird/docs/specifications/dml/04_DML_STATEMENTS_OVERVIEW.md (COPY spec marked TBD), ScratchBird/docs/specifications/query/QUERY_OPTIMIZER_SPEC.md

## F-024 Firebird emulation parity gaps
Status: PARTIAL
Implemented:
- Firebird ALTER DATABASE ALIAS ADD/DROP supported (ScratchBird/src/parser/firebird/firebird_parser.cpp:2112).
Missing:
- ALTER DATABASE options rejected + OWNER TODO (ScratchBird/src/parser/firebird/firebird_parser.cpp:2134, ScratchBird/src/parser/firebird/firebird_parser.cpp:2142).
- Remaining DDL/DML/PSQL parity gaps not audited.
Spec refs: ScratchBird/docs/specifications/FIREBIRD_V2_FEATURE_PARITY_SPECIFICATION.md, ScratchBird/docs/specifications/reference/firebird/FirebirdReferenceDocument.md

## F-025 Firebird wire protocol gaps
Status: DOC-ONLY
Implemented:
- Spec updates only; no code verification needed here.
Missing: None.

## F-026 MySQL emulation parity gaps
Status: VERIFIED (code present) / NEEDS-AUDIT (runtime)
Implemented:
- MySQL ALTER DATABASE emits EXT_ALTER_DATABASE (ScratchBird/src/parser/mysql/mysql_parser.cpp:2986).
Missing:
- Runtime parity tests not executed.
Spec refs: ScratchBird/docs/specifications/parser/MYSQL_PARSER_SPECIFICATION.md, ScratchBird/docs/specifications/MYSQL_PARSER_IMPLEMENTATION_GAPS.md

## F-027 MySQL wire protocol gaps
Status: DOC-ONLY
Implemented:
- Spec updates only; no code verification needed here.
Missing: None.

## F-028 PostgreSQL emulation parity gaps
Status: NEEDS-AUDIT
Implemented:
- No direct code proof gathered in this audit.
Missing:
- Requires targeted review of PG parser/adapter parity and runtime tests.
Spec refs: ScratchBird/docs/specifications/parser/POSTGRESQL_PARSER_SPECIFICATION.md, ScratchBird/docs/specifications/POSTGRESQL_PARSER_IMPLEMENTATION_GAPS.md

## F-029 PostgreSQL wire protocol gaps
Status: DOC-ONLY
Implemented:
- Spec updates only; no code verification needed here.
Missing: None.
