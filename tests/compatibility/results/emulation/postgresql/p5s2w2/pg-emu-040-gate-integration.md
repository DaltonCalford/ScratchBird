Last updated: 2026-03-24

# PG-EMU-040 pg_regress Gate Integration

- Gate mode: `execute`

## Prerequisites
- `tests/compatibility/postgresql/repos/postgres/src/test/regress/GNUmakefile`: `present`
- `tests/compatibility/postgresql/repos/postgres/src/test/regress/sql`: `present`
- `full upstream pg build cwd configured via PG_UPSTREAM_BUILD_DIR`: `present`

## Command templates
```bash
cd tests/compatibility/postgresql/repos/postgres/src/test/regress
test -f GNUmakefile
# Optional full-run mode:
# export PG_UPSTREAM_BUILD_DIR=<path to full upstream postgres build tree>
make -C src/test/regress check
make check-world
make installcheck-world
```

## Notes
- Dry-run initializes in-tree pg_regress gate command wiring and prerequisites.
- Subsuite execution and closure are tracked in PG-EMU-041/042/043.
- Evidence file path matches tracker row PG-EMU-040.
