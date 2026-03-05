# EPFC-026 PostgreSQL upstream harness evidence (isolated `test_setup` pass)

- Captured UTC: `2026-03-04T02:42:16Z`
- Scope: isolated upstream `pg_regress` lane for `test_setup` against ScratchBird PostgreSQL emulation after inherit-merge notice parity updates.

## Environment setup used

```bash
SCRATCHBIRD_PG_ADAPTER_FORCE_PASSWORD=1 \
SCRATCHBIRD_EXAMPLE_IMPORT_BUNDLE=0 \
scripts/example_db_manager.sh dynamic-setup
```

## Upstream command (isolated lane)

```bash
OUTDIR=/home/dcalford/CliWork/ScratchBird/tests/compatibility/postgresql/results/ctest/20260304_024216_single_test_setup/upstream
rm -rf "$OUTDIR"
mkdir -p "$OUTDIR"
PGPASSFILE="$OUTDIR/pgpass"
printf '127.0.0.1:16432:main:pg_admin:PgAdmin_Compat1!\n' > "$PGPASSFILE"
chmod 600 "$PGPASSFILE"
PGPASSFILE="$PGPASSFILE" \
  /usr/lib/postgresql/18/lib/pgxs/src/test/regress/pg_regress \
  --use-existing \
  --host=127.0.0.1 \
  --port=16432 \
  --user=pg_admin \
  --dbname=main \
  --inputdir=/home/dcalford/CliWork/ScratchBird/tests/compatibility/postgresql/repos/postgres/src/test/regress \
  --outputdir="$OUTDIR" \
  --expecteddir=/home/dcalford/CliWork/ScratchBird/tests/compatibility/postgresql/repos/postgres/src/test/regress \
  --bindir="$(dirname "$(which psql)")" \
  test_setup
```

## Result

1. `pg_regress` summary: `ok 1 - test_setup`, `All 1 tests passed`.
2. No `regression.diffs` file is produced for this run.
3. This confirms parity for the previously failing `test_setup` deltas:
   - duplicate `fipshash` create blocker,
   - missing `INHERITS` merge `NOTICE` lines.

## Artifacts

1. `ScratchBird/tests/compatibility/postgresql/results/ctest/20260304_024216_single_test_setup/upstream/regression.out`
2. `ScratchBird/tests/compatibility/postgresql/results/ctest/20260304_024216_single_test_setup/upstream/results/test_setup.out`
3. `ScratchBird/tests/compatibility/postgresql/results/ctest/20260304_024216_single_test_setup/upstream/pgpass`

## Notes

1. This evidence is a bounded pass checkpoint for `EPFC-026`; full upstream schedule closure remains in progress.
2. Next closure work is full-pack upstream parity and result-shape verification across the broader PostgreSQL regression matrix.
