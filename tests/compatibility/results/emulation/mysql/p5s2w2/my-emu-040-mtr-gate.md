Last updated: 2026-02-22

# MY-EMU-040 MTR Gate Integration

- Gate mode: `execute`

## Prerequisites
- `tests/compatibility/mysql/repos/mysql-server/mysql-test/mysql-test-run.pl`: `present`
- `tests/compatibility/mysql/repos/mysql-server/mysql-test`: `present`

## Command templates
```bash
cd tests/compatibility/mysql/repos/mysql-server/mysql-test
perl mysql-test-run.pl --suite=main,auth,binlog,replication --retry=0 --force
```

## Notes
- Dry-run initializes in-tree MTR gate command wiring and prerequisites.
- Execute mode runs full smoke only when mysql runtime binaries are available.
- Full suite execution and failure closure are tracked in MY-EMU-041/042.
- Evidence file path matches tracker row MY-EMU-040.
