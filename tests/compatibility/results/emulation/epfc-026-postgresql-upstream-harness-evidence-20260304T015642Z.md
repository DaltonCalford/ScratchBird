# EPFC-026 PostgreSQL upstream harness evidence (bounded `test_setup` blocker closure)

- Captured UTC: `2026-03-04T01:56:42Z`
- Scope: bounded upstream PostgreSQL `pg_regress --use-existing` lane (`SCRATCHBIRD_PG_REGRESS_TESTS='test_setup'`) after parser/runtime duplicate-function tolerance updates.

## Environment setup used

```bash
SCRATCHBIRD_PG_ADAPTER_FORCE_PASSWORD=1 \
SCRATCHBIRD_EXAMPLE_IMPORT_BUNDLE=0 \
scripts/example_db_manager.sh dynamic-setup
```

## Upstream command (bounded lane)

```bash
SCRATCHBIRD_PG_USE_UPSTREAM=1 \
SCRATCHBIRD_PG_COMPAT_RUN=1 \
SCRATCHBIRD_PG_HOST=127.0.0.1 \
SCRATCHBIRD_PG_PORT=16432 \
SCRATCHBIRD_PG_USER=pg_admin \
SCRATCHBIRD_PG_PASSWORD='PgAdmin_Compat1!' \
SCRATCHBIRD_PG_DB=main \
SCRATCHBIRD_PG_OWNER_DB=regression \
SCRATCHBIRD_PG_REGRESS_TESTS='test_setup' \
bash tests/compatibility/postgresql/scripts/run_postgresql_ctest.sh
```

## Result

1. Run artifacts are present under `ScratchBird/tests/compatibility/postgresql/results/ctest/20260303_205246/upstream`.
2. `test_setup.out` has no `ERROR:` rows.
3. Previously observed first-order blocker
   `Failed to create function 'fipshash': Function already exists. Use OR REPLACE to update.`
   is no longer present in this bounded lane.

Validation command:

```bash
rg -n "ERROR:" \
  ScratchBird/tests/compatibility/postgresql/results/ctest/20260303_205246/upstream/results/test_setup.out
```

Validation output: no matches.

## Artifacts

1. `ScratchBird/tests/compatibility/postgresql/results/ctest/20260303_205246/upstream/results/test_setup.out`
2. `ScratchBird/tests/compatibility/postgresql/results/ctest/20260303_205246/upstream/pg_regress.out`
3. `ScratchBird/tests/compatibility/postgresql/results/ctest/20260303_205246/upstream/regression.diffs`

## Notes

1. This evidence closes the specific bounded `test_setup` duplicate-function blocker checkpoint.
2. `EPFC-026` remains `InProgress` for broader upstream parity burn-down and full result-shape closure across required harness packs.
