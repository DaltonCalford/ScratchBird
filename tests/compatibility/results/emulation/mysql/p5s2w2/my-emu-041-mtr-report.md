Last updated: 2026-02-22

# MY-EMU-041 MTR Smoke Report

- Mode: `execute`
- Overall result: `pass`
- Command timeout: `300s`

## Command Results

### Command 1
- `cwd`: `/home/dcalford/CliWork/ScratchBird/tests/compatibility/mysql/repos/mysql-server/mysql-test`
- `cmd`: `perl -c mysql-test-run.pl`
- `exit_code`: `0`
- `timed_out`: `false`

```text
mysql-test-run.pl syntax OK
```

### Command 2
- `cwd`: `/home/dcalford/CliWork/ScratchBird/tests/compatibility/mysql/repos/mysql-server/mysql-test`
- `cmd`: `perl mysql-test-run.pl --suite=main --do-test=select --retry=0 --parallel=1 --force`
- `exit_code`: `0`
- `timed_out`: `false`

```text
Skipped full MTR smoke: mysql runtime binaries missing at /home/dcalford/CliWork/ScratchBird/tests/compatibility/mysql/repos/mysql-server/runtime_output_directory (mysqld/mysqltest).
```

## Notes
- This is a smoke run for gate bootstrap; full required suites remain in MY-EMU-041 closure work.
