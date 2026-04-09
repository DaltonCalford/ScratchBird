# Beta 2 Emulation Family Model - SQLite

## Purpose
Define the Beta 2 emulation bundle required to present ScratchBird as a donor-compatible `SQLite` endpoint while keeping donor-specific parsing, protocol, catalog, bridge, and error behavior outside the core engine.

## Family Identity
- display name: `SQLite`
- `profile_id`: `sqlite`
- primary surface class: `embedded_sql`
- primary donor protocol or carrier: `sqlite_embedded_api`
- shared lowering base: `embedded_sql_family`
- listener mode: `optional`
- listener executable: `sb_listener_sqlite`
- parser executable: `sb_parser_sqlite`
- parser package: `sb_pkg_sqlite_parser`
- compiler UDR package: `sb_pkg_sqlite_compiler_udr`
- emulation UDR package: `sb_pkg_sqlite_emulation_udr`
- compiler entrypoint: `compiler_sqlite`
- engine generator entrypoint: `engine_sqlite`
- bundle contract id: `sb_emulation_bundle_sqlite/v2`
- supports `MESSAGE_BLR`: `no`
- supports `EXECUTABLE_BLR`: `no`

## Admission Reason
SQLite is an embedded engine without a standalone server protocol, so the family needs a library-shim parser model and strong catalog/file references.

## Authoritative Reference Inputs
- local source-backed packet: `docs/reference/reference_library/emulation_1_to_1_engine_reference_packets_2026-04-02/sqlite/README.md`
- official donor web supplement: `docs/reference/reference_library/emulation_1_to_1_engine_reference_packets_2026-04-02/sqlite/OFFICIAL_WEB_REFERENCE_SUPPLEMENT.md`
- error-code packet root: `docs/reference/reference_library/error_code_reference_packets_2026-04-02/README.md`
- packet manifest state: protocol adapter `no`, parser agent `no`, bridge adapter `no`, catalog overlay `yes`, compatibility suite `no`
- official donor web references available now: `2/2`

## Current Reference Coverage Snapshot
- (a) Datatypes: donor `0/2` paths, ScratchBird `4/4` paths.
- (b) Indexes: donor `0/2` paths, ScratchBird `4/4` paths.
- (c) Parser to SB AST / V3 Dialect: donor `0/1` paths, ScratchBird `5/5` paths.
- (d) Wire Protocol: donor `0/1` paths, ScratchBird `2/2` paths.
- (e) Authentication: donor `0/1` paths, ScratchBird `4/4` paths.
- (f) Client Bridge / UDR Target Surface: donor `0/1` paths, ScratchBird `2/2` paths.
- (g) Plan Layout / Optimizer Output: donor `0/2` paths, ScratchBird `3/3` paths.
- (h) Error Codes: donor `0/2` paths, ScratchBird `4/4` paths.
- (i) Page Types and Storage Optimizations: donor `0/2` paths, ScratchBird `6/6` paths.
- (j) Regression Tests and Tooling: donor `0/1` paths, ScratchBird `3/3` paths.
- (k) Catalog / System Tables / New Empty Database: donor `0/2` paths, ScratchBird `3/3` paths.

## Parser Package Contract
1. Own the full `SQLite` client-facing request lifecycle for `sqlite_embedded_api`.
2. Translate donor-visible datatypes, result metadata, errors, and plan requests into canonical ScratchBird AST, SBLR, and metrics packets.
3. Keep all donor-visible session state, prepared handles, streaming state, and async or notice surfaces inside the parser process boundary for one connection only.
4. Fail closed when the bundle is not `READY`, when the emulated database binding is unavailable, or when the request class is not admitted by the family capability set.

## Compiler UDR Contract
1. `"compiler_sqlite"` is the only engine-side entrypoint allowed to translate donor-generated text or helper payloads for `SQLite`.
2. It must reuse the same capability, AST, datatype, and function admission rules as the parser package.
3. It must verify privilege class and request shape before emitting AST or SBLR.
4. It must never render donor protocol frames or donor-visible text directly to a client.

## Engine Generator And Emulation UDR Contract
1. `"engine_sqlite"` owns the family-specific engine-side emulation support.
2. It must bootstrap and validate: `sqlite_schema`, pragma, and introspection overlays with branch-root filtering.
3. It must ship the internal donor client required for: internal SQLite-compatible bridge used for migration and file-ingest or passthrough workflows.
4. It must provide migration, bridge, catalog-bootstrap, empty-database-default, and validation routines for this family.

## Donor-Specific Beta 2 Deltas
- Parser package is a library-shim and optional listener profile rather than a required socket listener family.
- Compiler UDR must own pragma lowering, generated SQL translation, and donor helper statements for embedded-use parity.
- Engine UDR must publish `sqlite_schema` and pragma-visible overlays plus empty-database bootstrap objects.

## Regression And Bridge Requirements
- regression baseline: SQLite shell fixtures, pragma goldens, and file-format compatibility suites.
- internal bridge requirement: internal SQLite-compatible bridge used for migration and file-ingest or passthrough workflows.
- family plan renderer must convert canonical `RuntimePlan` output into donor-visible explain or profile formats for `SQLite`.
- family error map pack must cover every donor error code admitted by the local packet and official donor docs.

## Current Evidence Gaps To Preserve During Implementation
- donor evidence gap: `datatypes` -> src/parse.y
- donor evidence gap: `datatypes` -> src/sqliteInt.h
- donor evidence gap: `indexes` -> src/build.c
- donor evidence gap: `indexes` -> src/btree.c
- donor evidence gap: `parser_ast` -> src/parse.y
- donor evidence gap: `wire_protocol` -> src/shell.c
- donor evidence gap: `authentication` -> src/auth.c
- donor evidence gap: `client_bridge` -> src/shell.c
- donor evidence gap: `plan_output` -> src/explain.c
- donor evidence gap: `plan_output` -> src/select.c
- donor evidence gap: `error_codes` -> src/sqliteInt.h
- donor evidence gap: `error_codes` -> src/main.c
- donor evidence gap: `page_optimizations` -> src/pager.c
- donor evidence gap: `page_optimizations` -> src/btree.c
- donor evidence gap: `catalogs_bootstrap` -> src/build.c
- donor evidence gap: `catalogs_bootstrap` -> src/pragma.c
- donor evidence gap: `regression_tests` -> test
- comparison deepening required: `indexes` -> No source-backed 1:1 index rows emitted in this packet
- current ScratchBird implementation gap: `wire_protocol` -> No dedicated ScratchBird protocol adapter file detected
- current ScratchBird implementation gap: `parser_ast` -> No dedicated ScratchBird external parser agent detected
- current ScratchBird implementation gap: `client_bridge` -> No dedicated ScratchBird FDW adapter detected

## Sample Bundle Registration
```cpp
static const scratchbird::core::EmulationPackageScaffold kSqliteScaffold = {
    "sqlite",
    "sb_listener_sqlite",
    "sb_parser_sqlite",
    "sb_pkg_sqlite_parser",
    "sb_pkg_sqlite_compiler_udr",
    "sb_pkg_sqlite_emulation_udr",
    "sb_emulation_bundle_sqlite/v2",
    true,
    true,
    false,
    false,
};

register_emulation_entrypoint("compiler_sqlite", &SqliteCompiler::invoke);
register_emulation_entrypoint("engine_sqlite", &SqliteEngine::invoke);
```

## Beta 2 Completion Rule
`SQLite` is `READY` only after parser, compiler UDR, emulation UDR, web and local reference inputs, virtual catalog goldens, plan render goldens, donor error-map pack, and regression harness evidence are all present.

