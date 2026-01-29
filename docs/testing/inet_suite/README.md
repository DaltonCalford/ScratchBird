# Inet Grind Suite (Multi-Dialect)

This suite provides **network/inet** SQL scripts and wrapper runners to stress
ScratchBird via native and emulated listeners. It creates databases, schemas,
roles/users (native), domains, sequences, tables, views, procedures, and
large-row workloads (1-2 million rows per table by default).

The scripts are designed to be **run in parallel** across multiple users to
exercise transactions, concurrency, and index workloads.

## Layout

```
inet_suite/
  scripts/              # CLI wrappers (sb_isql + sb_pg_isql + sb_my_isql + Firebird isql)
  sql/
    native/             # ScratchBird native SQL
    postgresql/         # PostgreSQL dialect SQL
    mysql/              # MySQL dialect SQL
    firebird/           # Firebird SQL dialect
  results/              # Output logs
```

## Defaults

- Native: host=localhost port=3092
- Firebird wire protocol (Firebird isql): host=localhost port=3050
- PostgreSQL: host=localhost port=5432
- MySQL: host=localhost port=3306

Databases created:
- `sb_grind_native`
- `sb_grind_pg`
- `sb_grind_mysql`
- `sb_grind_fb`

Users (created in native bootstrap):
- `sb_reader` / `sb_reader_pw`
- `sb_writer` / `sb_writer_pw`
- `sb_runner` / `sb_runner_pw`

## Notes / Current Parser Gaps

- **Groups**: CREATE/DROP GROUP is not currently parsed in the native V2
  parser. Group commands are included as commented placeholders.
- **MySQL DCL**: CREATE/ALTER/DROP USER/ROLE and GRANT/REVOKE are not yet
  implemented in the MySQL parser. Use native bootstrap for user setup.
- **MySQL procedures**: the MySQL seed procedures use `generate_series` to
  keep each procedure definition a single statement for sb_my_isql.
  If you need pure MySQL syntax, replace with loop-based procedures.
- **PostgreSQL DCL**: CREATE USER/ROLE in the PG parser is not implemented.
  Use native bootstrap for user setup.
- **Firebird client selection**: default is `sb_fb_isql` for ScratchBird
  testing; set `WIRE_CLIENTS=firebird` (or `FB_CLIENT=firebird`) to use
  Firebird `isql` against port 3050.
- **WIRE_CLIENTS**: set `WIRE_CLIENTS=psql,mysql,firebird` to switch wrappers
  to native clients automatically. `PG_CLIENT`/`MY_CLIENT`/`FB_CLIENT` still
  override.
- **Firebird database path**: `run_firebird_create_db.sh` substitutes
  `{{FB_TARGET}}`, `{{FB_USER}}`, `{{FB_PASS}}` in
  `sql/firebird/00_create_db.sql`. `FB_TARGET` is `FB_CONN` when using
  Firebird `isql`, or `FB_DB` when using `sb_fb_isql`.
- **Admin database**: `sb_isql`/`sb_pg_isql`/`sb_my_isql` require a database
  argument; set `SB_ADMIN_DB`/`PG_ADMIN_DB`/`MY_ADMIN_DB` to a database that
  already exists and grants CREATE DATABASE privileges (default: `default`).

## Environment Overrides

Each wrapper accepts environment variables for host, port, credentials, and
admin/target database names. You can override defaults via:

```
WIRE_CLIENTS=psql,mysql,firebird

SB_HOST=localhost
SB_PORT=3092
SB_ADMIN_USER=admin
SB_ADMIN_PASS=admin_pw
SB_ADMIN_DB=default
SB_DB=sb_grind_native
SB_USER=sb_runner
SB_PASS=sb_runner_pw

FB_HOST=localhost
FB_PORT=3050
FB_CLIENT=sb            # sb or firebird (optional override)
FB_ISQL=isql            # path to Firebird isql when FB_CLIENT=firebird
FB_SB_ISQL=sb_fb_isql    # path to sb_fb_isql when FB_CLIENT=sb
FB_ADMIN_USER=SYSDBA
FB_ADMIN_PASS=masterkey
FB_DB=/var/lib/scratchbird/sb_grind_fb.sbdb
FB_CONN=localhost/3050:/var/lib/firebird/data/sb_grind_fb.fdb
FB_USER=SYSDBA
FB_PASS=masterkey

PG_HOST=localhost
PG_PORT=5432
PG_CLIENT=sb           # sb or psql (optional override)
PG_ISQL=sb_pg_isql      # path to sb_pg_isql when PG_CLIENT=sb
PG_PSQL=psql           # path to psql when PG_CLIENT=psql
PG_ADMIN_USER=admin
PG_ADMIN_PASS=admin_pw
PG_USER=sb_runner
PG_PASS=sb_runner_pw
PG_ADMIN_DB=default
PG_DB=sb_grind_pg

MY_HOST=localhost
MY_PORT=3306
MY_CLIENT=sb           # sb or mysql (optional override)
MY_ISQL=sb_my_isql      # path to sb_my_isql when MY_CLIENT=sb
MY_MYSQL=mysql         # path to mysql when MY_CLIENT=mysql
MY_ADMIN_USER=admin
MY_ADMIN_PASS=admin_pw
MY_USER=sb_runner
MY_PASS=sb_runner_pw
MY_ADMIN_DB=default
MY_DB=sb_grind_mysql
```

Row scale is controlled by:

```
ROWS_SMALL=100000
ROWS_MEDIUM=500000
ROWS_LARGE=1000000
```

## Typical Run Order

### ScratchBird (sb_* clients)

Non-native clients (sb_fb_isql/sb_pg_isql/sb_my_isql) run **after** the native
tests so the ScratchBird database exists before emulation tests.

Client mapping for ScratchBird tests:
- Native: `sb_isql`
- Firebird: `sb_fb_isql`
- PostgreSQL: `sb_pg_isql`
- MySQL: `sb_my_isql`

1) Native bootstrap/schema/load/reports
2) Emulated dialect create/schema/load
3) Parallel worker runs (all dialects)
4) Emulated reports

Use the helper script:

```
./scripts/run_scratchbird_sequence.sh
```

This script clears `WIRE_CLIENTS` and per-dialect overrides so the sb_* clients
are always used for ScratchBird testing.

### Real Server Validation (psql/mysql/isql)

Run the native server-specific sequence for each dialect, then compare outputs.
Set `WIRE_CLIENTS=psql,mysql,firebird` (or per‑dialect overrides) to switch to
native clients.

Example:

```
./scripts/run_native_bootstrap.sh
./scripts/run_native_schema.sh
./scripts/run_native_load.sh
./scripts/run_parallel_workers.sh
./scripts/run_native_reports.sh
```

## Output Logs

All wrappers write logs to `results/` with timestamps. Each run creates a
`.out` (query output via `-o`), `.log` (stdout), and `.err` (stderr) file for
debugging.

## Parallel Stress Runs

Use `run_parallel_stage.sh` to launch multiple concurrent runs of a single
stage (e.g., workers). Avoid running bootstrap/schema scripts in parallel.

Example:

```
./scripts/run_parallel_stage.sh run_native_worker.sh 8
```
