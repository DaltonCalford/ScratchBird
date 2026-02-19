# ScratchBird

ScratchBird is a Firebird-style MGA database engine with a native SQL parser,
SBLR runtime, and multi-protocol listener layer.

## Release State

- Version: `0.1.0` (initial early beta)
- Release date: `2026-02-19`
- Test baseline: `3355/3355` passing (`ctest`)
- Runtime package baseline: `release/beta/bin` with 12 executables

## What Is Implemented in 0.1.0

- Core engine lifecycle: database create/open/close, catalog/bootstrap, storage,
  transaction/MGA visibility, locking, GC/sweep, and runtime services.
- Native parser and SBLR pipeline: SQL -> AST -> SBLR container -> executor.
- Listener/server stack:
  - `sb_server`
  - `sb_listener_native`, `sb_listener_pg`, `sb_listener_mysql`, `sb_listener_fb`
  - `sb_parser_native`, `sb_parser_pg`, `sb_parser_mysql`, `sb_parser_fb`
- Listener ownership model:
  - listeners are database-owned
  - startup checks port collisions before listener/parser bootstrap
  - listener startup passes database owner and engine endpoint metadata
- UDR native SQL render endpoint contracts and deterministic error-code handling.

## 0.2.0 Focus (Next Milestone)

- Complete detailed specs + implementation plans for every partial/planned item.
- Catalog refactor and optimization.
- Finish emulation parser parity and run source-engine conformance suites.
- Native parser normalization for dialect-style consistency.
- Driver regression validation after parser/catalog work.
- Performance benchmarking against emulated source engines on identical hardware.
- Go/no-go and redesign gates based on benchmark outcomes.
- Installation bundle strategy (installers) in addition to release packages.

## Build and Test

```bash
cmake -S . -B build
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

## Release Packaging

Current beta packaging output:

- `release/beta/bin` (runtime executables)
- `release/beta/tests` (test executables)
- `release/beta/packages/runtime-only`
- `release/beta/packages/qa`
- `release/scratchbird-beta-20260218-full.tar.gz`

## Documentation

- Main docs index: `docs/INDEX.md`
- Current project status: `docs/status/PROJECT_STATUS_2026-02-19.md`
- Developer guide (full): `docs/guides/DEVELOPER_GUIDE_BETA_0_1_0.md`
- Native parser language reference (full):
  `docs/user-documentation/language-guide/NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md`
- Wiki home: `wiki/content/Home.md`
- In-tree planning/audit baseline:
  - `docs/audit/BETA_0_1_0_IMPLEMENTATION_AUDIT_2026-02-19.md`
  - `docs/planning/BETA_0_2_0_WORKPLAN_2026-02-19.md`
  - `docs/planning/BETA_0_2_0_SPEC_BACKLOG_2026-02-19.md`

## Archive Policy

Alpha and legacy artifacts are retained for traceability in archive directories,
but they are not the active baseline for beta planning or implementation.

## License

Licensed under the Initial Developer's Public License Version 1.0 (IDPL 1.0).
