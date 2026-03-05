# EPFC-026 PostgreSQL upstream harness evidence

- Captured UTC: `2026-03-03T23:27:30Z`
- Scope: upstream PostgreSQL `pg_regress --use-existing` lane against ScratchBird PostgreSQL emulation endpoint.

## Command

```bash
SCRATCHBIRD_PG_USE_UPSTREAM=1 \
SCRATCHBIRD_PG_COMPAT_RUN=1 \
SCRATCHBIRD_PG_HOST=127.0.0.1 \
SCRATCHBIRD_PG_PORT=16432 \
SCRATCHBIRD_PG_USER=pg_admin \
SCRATCHBIRD_PG_PASSWORD='PgAdmin_Compat1!' \
SCRATCHBIRD_PG_DB=main \
SCRATCHBIRD_PG_OWNER_DB=regression \
bash tests/compatibility/postgresql/scripts/run_postgresql_ctest.sh
```

## Result

1. Upstream lane executed and produced full `pg_regress` output artifacts.
2. Exit status: `failed`.
3. Summary: `236/236 tests failed`.
4. Primary failure signature in `test_setup` and repeated suite-wide:
4.1 `psql: ... FATAL: Authentication failed`.

## Artifacts

1. Launcher log:
1.1 `ScratchBird/tests/compatibility/results/emulation/ctest-upstream-postgresql-explicit-16432-fallback-20260303T232730Z.log`
2. Upstream harness outputs:
2.1 `ScratchBird/tests/compatibility/postgresql/results/ctest/20260303_182730/upstream/pg_regress.out`
2.2 `ScratchBird/tests/compatibility/postgresql/results/ctest/20260303_182730/upstream/regression.out`
2.3 `ScratchBird/tests/compatibility/postgresql/results/ctest/20260303_182730/upstream/regression.diffs`
2.4 `ScratchBird/tests/compatibility/postgresql/results/ctest/20260303_182730/upstream/results/test_setup.out`

## Notes

1. Harness runner was updated to emit a deterministic `PGPASSFILE` for upstream mode:
1.1 `ScratchBird/tests/compatibility/postgresql/scripts/run_postgresql_ctest.sh`
2. `PGPASSFILE` removed the immediate missing-password path, but `psql` authentication still fails against the emulated endpoint.
3. Current blocker is PostgreSQL wire/auth compatibility for stock `psql`/`pg_regress`, not parser acceptance.
