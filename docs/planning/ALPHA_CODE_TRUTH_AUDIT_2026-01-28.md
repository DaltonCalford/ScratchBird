# Alpha Code-Truth Audit (2026-01-28)

## Purpose
Validate Alpha completion claims against **source code** (read-only), and
record what is **actually implemented** vs **still open**. This document
feeds the active Alpha tracker and remediation work.

## Scope
- Core engine workstreams in `ALPHA_COMPLETION_MASTER_PLAN.md`
- Monitoring parity and backup/restore parity
- Index GC wiring and advanced index coverage (Alpha scope)
- V2 parser + PSQL runtime completion (spot-verified)
- Resources/i18n loaders and data coverage

## Evidence Summary (Code-Truth)

### ✅ Done (verified in code)

1) **Tablespace routing (normal table + index root pages)**
- Table root allocation uses tablespace-aware GPID allocation.
  - `ScratchBird/src/core/catalog_manager.cpp:6714-6749`
- Standard index root allocation uses tablespace-aware GPID allocation.
  - `ScratchBird/src/core/catalog_manager.cpp:7403-7412`

2) **Backup/restore multi-file tablespaces**
- Restore builds per-tablespace file ranges and writes pages to the correct file.
  - `ScratchBird/src/core/backup_manager.cpp:723-836`
  - Page write selects file by range and adjusts local page offset.
  - `ScratchBird/src/core/backup_manager.cpp:918-1007`

3) **Monitoring parity (sys.* catalog tables)**
- Sys catalog exposes sessions/transactions/locks/statements/performance.
  - `ScratchBird/src/catalog/sys_catalog.cpp:173-188`
- Per-table handlers exist for sessions/transactions/locks/statements/performance.
  - `ScratchBird/src/catalog/sys_catalog.cpp:380-510` (dispatch)

4) **Index GC wiring (including IVF/ZONEMAP/COLUMNSTORE/LSM/FULLTEXT)**
- GC switch includes IVF, ZONEMAP (BRIN), COLUMNSTORE, LSM, FULLTEXT, etc.
  - `ScratchBird/src/core/garbage_collector.cpp:923-1048`
- Columnstore GC implementation present.
  - `ScratchBird/src/core/columnstore.cpp:754-1040`
- LSM GC implementation present.
  - `ScratchBird/src/core/lsm_tree_index.cpp:1118-1326`
- Inverted index GC implementation present.
  - `ScratchBird/src/core/inverted_index.cpp:3782-3865`

5) **V2 parser + PSQL spot verification**
- RESET parser exists.
  - `ScratchBird/src/parser/parser_v2.cpp:243, 8475-8485`
- COMMENT ON parser + bytecode opcode.
  - `ScratchBird/src/parser/parser_v2.cpp:9196-9273`
  - `ScratchBird/src/sblr/bytecode_generator_v2.cpp:1337`
- RELEASE SAVEPOINT parser + bytecode opcode.
  - `ScratchBird/src/parser/parser_v2.cpp:238, 8034`
  - `ScratchBird/src/sblr/bytecode_generator_v2.cpp:4159`
- DESCRIBE/DESC parser exists.
  - `ScratchBird/src/parser/parser_v2.cpp:244, 8752-8762`
- CASE expressions + CASE statement form present (parser/bytecode/executor).
  - `ScratchBird/src/parser/parser_v2.cpp:6553-6574, 9684-9690`
  - `ScratchBird/src/sblr/bytecode_generator_v2.cpp:5593-5603`
  - `ScratchBird/src/sblr/executor.cpp:26113-26178`

### ❌ Still Open (verified in code or resources)

1) **Expression/partial index root allocation is NOT tablespace-aware**
- Expression/partial index create path uses `allocatePage()` (primary tablespace)
  instead of `allocatePageInTablespace()`.
  - `ScratchBird/src/core/catalog_manager.cpp:7642-7663`

2) **Resources/i18n coverage + loader tooling**
- Resource lists and loader tooling are still incomplete per audit.
  - `ScratchBird/docs/findings/RESOURCES_I18N_TIMEZONE_AUDIT.md`
  - Missing Firebird/MySQL/PG charset/collation coverage
  - `sb_charset_loader` deprecated/non-compiling; timezone loader flag mismatch

## Notes / Caveats

- Parser completion was **spot-verified** only. The acceptance checklist in
  `ScratchBird/docs/planning/PLAN_V2_PARSER_COMPLETION_finished.md` claims
  full coverage with tests; this audit does not re-run those tests.
- Firebird MON$ to sys.* mapping is not explicitly referenced in parser code.
  Sys catalog tables exist under `sys`, and `sys.mon` schema exists, but
  explicit `MON$` name mapping was not found in source; confirm if required.

## Alpha Status (based on code-truth)

**Alpha is not complete** due to:
1) Expression/partial index root allocation (tablespace-aware fix needed).
2) Resources/i18n loaders + coverage completeness.

All other items in the Alpha master plan appear implemented per code evidence.

