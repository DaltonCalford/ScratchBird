Last updated: 2026-03-03T16:17:42Z

# Required Upstream Harness Launcher

- Mode: `execute`
- Repo root: `/home/dcalford/CliWork/ScratchBird`

## Command templates
```bash
SCRATCHBIRD_MY_USE_UPSTREAM=1 SCRATCHBIRD_MY_COMPAT_RUN=1 bash "/home/dcalford/CliWork/ScratchBird/tests/compatibility/mysql/scripts/run_mysql_ctest.sh"
SCRATCHBIRD_PG_USE_UPSTREAM=1 SCRATCHBIRD_PG_COMPAT_RUN=1 bash "/home/dcalford/CliWork/ScratchBird/tests/compatibility/postgresql/scripts/run_postgresql_ctest.sh"
bash "/home/dcalford/CliWork/ScratchBird/scripts/emulation/start_upstream_suite_gates.sh" execute
```

## MySQL upstream MTR lane

- Command: `SCRATCHBIRD_MY_USE_UPSTREAM=1 SCRATCHBIRD_MY_COMPAT_RUN=1 bash "/home/dcalford/CliWork/ScratchBird/tests/compatibility/mysql/scripts/run_mysql_ctest.sh"`
- Exit code: `1`

## PostgreSQL upstream pg_regress lane

- Command: `SCRATCHBIRD_PG_USE_UPSTREAM=1 SCRATCHBIRD_PG_COMPAT_RUN=1 bash "/home/dcalford/CliWork/ScratchBird/tests/compatibility/postgresql/scripts/run_postgresql_ctest.sh"`
- Exit code: `1`

## Firebird upstream firebird-qa lane

- Command: `bash "/home/dcalford/CliWork/ScratchBird/scripts/emulation/start_upstream_suite_gates.sh" execute`
- Exit code: `0`

