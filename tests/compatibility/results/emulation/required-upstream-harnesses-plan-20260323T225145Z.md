Last updated: 2026-03-23T22:51:45Z

# Required Upstream Harness Launcher

- Mode: `plan`
- Repo root: `/home/dcalford/CliWork/ScratchBird`

## Command templates
```bash
SCRATCHBIRD_MY_USE_UPSTREAM=1 SCRATCHBIRD_MY_COMPAT_RUN=1 bash "/home/dcalford/CliWork/ScratchBird/tests/compatibility/mysql/scripts/run_mysql_ctest.sh"
SCRATCHBIRD_PG_USE_UPSTREAM=1 SCRATCHBIRD_PG_COMPAT_RUN=1 bash "/home/dcalford/CliWork/ScratchBird/tests/compatibility/postgresql/scripts/run_postgresql_ctest.sh"
bash "/home/dcalford/CliWork/ScratchBird/scripts/emulation/start_upstream_suite_gates.sh" execute
```

