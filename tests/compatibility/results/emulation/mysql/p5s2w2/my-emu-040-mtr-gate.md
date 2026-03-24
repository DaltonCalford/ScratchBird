Last updated: 2026-03-24

# MY-EMU-040 MTR Gate Integration

- Gate mode: `execute`

## Prerequisites
- `tests/compatibility/mysql/repos/mysql-server/mysql-test/mysql-test-run.pl`: `present`
- `tests/compatibility/mysql/repos/mysql-server/mysql-test`: `present`
- `full upstream mysql source/runtime auto-detected`: `present`

## Command templates
```bash
cd tests/compatibility/mysql/repos/mysql-server/mysql-test
# Optional full-run mode:
# export MYSQL_UPSTREAM_SOURCE_DIR=<path to mysql source root>
# export MYSQL_UPSTREAM_BUILD_DIR=<path to mysql build root>
perl mysql-test-run.pl --suite=main --do-test=select --retry=0 --parallel=1 --force
```

## Notes
- Dry-run initializes in-tree MTR gate command wiring and prerequisites.
- Execute mode runs full smoke only when mysql source tree and runtime binaries are available.
- Full suite execution and failure closure are tracked in MY-EMU-041/042.
- Evidence file path matches tracker row MY-EMU-040.
