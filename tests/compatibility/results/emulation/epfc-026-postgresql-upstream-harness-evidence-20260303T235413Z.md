# EPFC-026 PostgreSQL upstream harness evidence (forced-password stage)

- Captured UTC: `2026-03-03T23:54:13Z`
- Scope: upstream PostgreSQL `pg_regress --use-existing` lane against ScratchBird emulation, after enabling forced password adapter mode to unblock stock `psql` authentication.

## Environment setup used

```bash
SCRATCHBIRD_PG_ADAPTER_FORCE_PASSWORD=1 \
SCRATCHBIRD_EXAMPLE_IMPORT_BUNDLE=0 \
scripts/example_db_manager.sh dynamic-setup
```

Validation:
```bash
PGPASSWORD='PgAdmin_Compat1!' psql -h 127.0.0.1 -p 16432 -U pg_admin -d main -c "SELECT 1;"
```
Result: login succeeds with one-row result.

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

1. Harness enters upstream mode and executes `test_setup`.
2. `test_setup` fails with SQL-level parity gaps (not wire-auth failure).
3. Representative first-order failures:
3.1 `SET variable not supported: synchronous_commit`
3.2 `V3 GRANT/REVOKE missing object_path`
3.3 `unknown SBLR vNext opcode ... SBLR3_CREATE_TABLESPACE`
3.4 `COPY only supports STDIN/STDOUT targets`
3.5 `Domain not found` for PostgreSQL built-in aliases (`name`, `point`, `path`)
3.6 `INHERITS` and `CREATE TABLE ... AS SELECT` parse/mapping gaps
3.7 `CREATE TYPE` bridge/runtime closure gaps (`BRG_0406`)

## Artifacts

1. Upstream run directory:
1.1 `ScratchBird/tests/compatibility/postgresql/results/ctest/20260303_185413/upstream`
2. Key files:
2.1 `ScratchBird/tests/compatibility/postgresql/results/ctest/20260303_185413/upstream/results/test_setup.out`
2.2 `ScratchBird/tests/compatibility/postgresql/results/ctest/20260303_185413/upstream/regression.diffs`
2.3 `ScratchBird/tests/compatibility/postgresql/results/ctest/20260303_185413/upstream/pg_regress.out`

## Notes

1. `run_postgresql_ctest.sh` now writes upstream `PGPASSFILE` to avoid dependency on `PGPASSWORD` propagation.
2. Forced-password listener mode (`SCRATCHBIRD_PG_ADAPTER_FORCE_PASSWORD=1`) is currently required for stock `psql` interoperability in this harness path.
3. Remaining work has moved from authentication gating to parser/runtime/result-shape parity closure.
