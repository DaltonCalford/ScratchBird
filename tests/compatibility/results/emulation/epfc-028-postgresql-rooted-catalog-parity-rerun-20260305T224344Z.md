# EPFC-028 PostgreSQL Rooted Catalog Parity Rerun (2026-03-05T22:43:44Z)

## Scope

Bounded upstream PostgreSQL regression lane rerun after rooted `search_path` and executor acceptance updates for virtual `pg_catalog` handling.

## Runtime/code context under test

1. `ScratchBird/src/protocol/adapters/postgresql_adapter.cpp`
2. `ScratchBird/src/sblr/executor.cpp`

## Command sequence

```bash
cd /home/dcalford/CliWork/ScratchBird
scripts/example_db_manager.sh dynamic-setup
SCRATCHBIRD_PG_USE_UPSTREAM=1 \
SCRATCHBIRD_PG_COMPAT_RUN=1 \
SCRATCHBIRD_PG_HOST=127.0.0.1 \
SCRATCHBIRD_PG_PORT=16432 \
SCRATCHBIRD_PG_USER=postgres \
SCRATCHBIRD_PG_PASSWORD=postgres \
SCRATCHBIRD_PG_DB=main \
SCRATCHBIRD_PG_REGRESS_TESTS='drop_if_exists tablespace' \
tests/compatibility/postgresql/scripts/run_postgresql_ctest.sh
scripts/example_db_manager.sh dynamic-teardown
```

## Primary artifacts

1. `ScratchBird/tests/compatibility/postgresql/results/ctest/20260305_174300/RUN_MANIFEST.json`
2. `ScratchBird/tests/compatibility/postgresql/results/ctest/20260305_174300/upstream/pg_regress.out`
3. `ScratchBird/tests/compatibility/postgresql/results/ctest/20260305_174300/upstream/regression.diffs`
4. `ScratchBird/tests/compatibility/postgresql/results/ctest/20260305_174300/upstream/results/drop_if_exists.out`
5. `ScratchBird/tests/compatibility/postgresql/results/ctest/20260305_174300/upstream/results/tablespace.out`

## Result summary

1. Bounded upstream lane remains failing (`2/2` tests failing).
2. `drop_if_exists` parity remains open:
2.1. Some notice behavior improved (`DROP TABLE IF EXISTS` missing-object notice appears).
2.2. Error/notice/result shape mismatches remain for multiple object families (`VIEW`, `INDEX`, `SCHEMA`, function-like objects, and object-class specific message text).
3. `tablespace` parity remains open:
3.1. Unqualified catalog lookups still miss for filtered catalog objects (`pg_tablespace`, `pg_class`).
3.2. `Schema path not found` remains frequent in `testschema.*` operations (count: `27` in current run output).
3.3. `Target tablespace not found` still appears during expected `pg_default` transitions.

## Dynamic setup behavior note

1. In a direct `scripts/example_db_manager.sh dynamic-setup` run with default bundle import enabled, startup may fail after bundle-import restart with:
1.1. `startup attempt 1/2 failed: native listener failed to come up on 127.0.0.1:16092; retrying`
1.2. `error: native listener failed to come up on 127.0.0.1:16092`
2. Compatibility fixture runs set `SCRATCHBIRD_EXAMPLE_IMPORT_BUNDLE=0`, so this restart-path issue does not prevent bounded PostgreSQL lane execution.

## Gate impact

1. `EPFC-026`: still `InProgress` (required upstream PostgreSQL harness lane not closed).
2. `EPFC-028`: still `InProgress` (runtime parity deltas remain for rooted catalog visibility and PostgreSQL error/notice/result shape behavior).
3. `EPFC-030`: remains blocked by `EPFC-026` and `EPFC-028`.
