# EPFC-028 PostgreSQL extension-surface parser evidence

- Captured UTC: `2026-03-04T15:52:05Z`
- Tracker row: `EPFC-028`
- Scope: parser/runtime-contract closure evidence for PostgreSQL extension families (`EEXT-PG-001..008` routing layer)

## Implemented in this increment

1. Added explicit PostgreSQL parser contract tests for canonical internal mapping keys:
1.1 `CHECKPOINT` -> `SweepDatabaseStmt`
1.2 `CLUSTER ...` -> `AlterSystemStmt` key `maintenance.cluster`
1.3 `WAIT FOR LSN ...` -> `AlterSystemStmt` key `admin.wait_for_lsn` with `LSN=` payload
1.4 `LOAD`, `CREATE/ALTER/DROP EXTENSION`
1.5 `CREATE/ALTER/DROP PUBLICATION`
1.6 `CREATE/ALTER/DROP SUBSCRIPTION`
2. Extended PostgreSQL parser DDL support for `DROP TABLESPACE ... FORCE` (direct form) in addition to existing `WITH (FORCE)` handling.

## Files changed

1. `ScratchBird/src/parser/postgresql/pg_parser_ddl.cpp`
2. `ScratchBird/tests/unit/test_postgresql_parser.cpp`

## Targeted validation

Command executed:

```bash
cmake --build /home/dcalford/CliWork/ScratchBird/build --target scratchbird_tests -j8
ctest --output-on-failure -R "PostgreSQLParserTest\.(DropTablespaceSurfaceMapsFlags|CheckpointClusterAndWaitMapToCanonicalInternalRoutes|ExtensionPublicationAndSubscriptionLifecycleMapToCanonicalKeys)"
```

Result:

1. `PostgreSQLParserTest.DropTablespaceSurfaceMapsFlags` -> PASS
2. `PostgreSQLParserTest.CheckpointClusterAndWaitMapToCanonicalInternalRoutes` -> PASS
3. `PostgreSQLParserTest.ExtensionPublicationAndSubscriptionLifecycleMapToCanonicalKeys` -> PASS
4. Summary: `3/3` passed, `0` failed.

## EPFC-028 impact

1. Confirms parser-level canonical mapping coverage for PostgreSQL extension/simulation surfaces in `PGMAP-011`, `PGMAP-038..041`, `PGMAP-042..050`.
2. Rows remain pending broader upstream harness/result-shape closure under `EPFC-026`/`EPFC-028` execution lanes.
