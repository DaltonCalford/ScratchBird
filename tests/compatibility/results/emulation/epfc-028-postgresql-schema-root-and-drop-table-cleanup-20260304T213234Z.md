# EPFC-028 PostgreSQL Schema-Root + DROP TABLE Cleanup Increment (2026-03-04T21:32:34Z)

## Scope

Applied two compatibility increments aligned to emulated-root semantics and bounded upstream hygiene:

1. `CatalogManager::dropTable` stale-owned cleanup tolerance (for owned trigger/index/constraint/sequence NOT_FOUND paths) so DROP can complete and clear stale dependency edges.
2. PostgreSQL parser `resolveTableName` update so explicit schema-qualified names under manager-bound alias defaults (e.g. `default_schema=main`) are anchored under emulated database root (`main.<schema>`), while unqualified names remain unqualified.

## Code Changes

- `src/core/catalog_manager.cpp`
  - Added stale-owned dependency pruning helpers inside `dropTable`.
  - Treat owned-object `NOT_FOUND` as stale metadata cleanup and continue.
  - Treat missing on-disk table record / missing tablespace metadata during drop as cleanup-tolerant `NOT_FOUND` and finalize cache/dependency cleanup.
- `src/parser/postgresql/pg_parser.cpp`
  - In manager-bound alias mode, explicit schema qualifiers now resolve relative to emulated database root.
- `tests/unit/test_table_dependencies.cpp`
  - Added `DropTableIgnoresStaleOwnedIndexDependency`.
- `tests/unit/test_postgresql_parser.cpp`
  - Added `DropTableQualifiesSchemaUnderDatabaseAliasRoot`.

## Validation

### Targeted build + tests

- Build: `cmake --build /home/dcalford/CliWork/ScratchBird/build -j 12` ✅
- Focused test filter (10 tests) ✅
  - Includes:
    - `TableDependencyTest.DropTableIgnoresStaleOwnedIndexDependency`
    - `PostgreSQLParserTest.DropTableUsesUnqualifiedPathForDatabaseAliasDefault`
    - `PostgreSQLParserTest.DropTableQualifiesSchemaUnderDatabaseAliasRoot`
    - Prior EPFC-028 parser/executor guard set

### Bounded upstream lane (`drop_if_exists tablespace`)

Command:

```bash
SCRATCHBIRD_PG_HOST=127.0.0.1 \
SCRATCHBIRD_PG_PORT=16432 \
SCRATCHBIRD_PG_DB=main \
SCRATCHBIRD_PG_USER=pg_admin \
SCRATCHBIRD_PG_USE_UPSTREAM=1 \
SCRATCHBIRD_PG_REGRESS_TESTS='drop_if_exists tablespace' \
tests/compatibility/postgresql/scripts/run_postgresql_ctest.sh
```

Runs captured:

1. `20260304_161732` (before clean stack restart): `0/2` fail; stale `DROP TABLE ... status 4002` still present.
2. Dynamic stack restarted via `scripts/example_db_manager.sh dynamic-setup` (bundle import disabled) and lane rerun `20260304_162758`: still `0/2`, but `DROP TABLE ... status 4002` **no longer present**.
3. Post parser root patch rerun `20260304_163145`: still `0/2`; `Schema path not found` count in `regression.diffs` changed from `65` to `64` vs run `20260304_162758`.
4. Final clean cycle (`dynamic-setup` then lane) run `20260304_163514`: still `0/2`, `DROP TABLE ... status 4002` remains absent, and `Schema path not found` count is `31`.

## Observed Parity Movement

- Resolved failure class: stale `dropTable` internal NOT_FOUND leak (`status 4002`) in bounded lane artifacts after clean restart.
- Clean-cycle measurement confirms `status 4002` is removed and schema-path misses are materially lower (`31` lines) than pre-clean baseline (`65` lines).
- Remaining primary gaps:
  - PostgreSQL `IF EXISTS` NOTICE/result-shape parity remains open.
  - System catalog emulation gaps (`pg_tablespace`, `pg_class`) remain large blockers for `tablespace`.
  - Schema-path/root parity is still incomplete across broader tablespace script surfaces (residual `Schema path not found` deltas remain).

## Next Actions

1. Continue parser/emitter/executor root-resolution alignment for schema-qualified DDL/DML beyond `resolveTableName` table paths.
2. Implement/normalize PostgreSQL NOTICE behavior for `IF EXISTS` families.
3. Advance filtered catalog-view parity (`pg_tablespace`, `pg_class`) under emulated-root scoping.
4. Keep bounded lane reruns behind fresh dynamic stack setup to avoid stale-fixture contamination.
