# Beta 2 Emulation Family Model - DuckDB

## Purpose
Define the Beta 2 emulation bundle required to present ScratchBird as a donor-compatible `DuckDB` endpoint while keeping donor-specific parsing, protocol, catalog, bridge, and error behavior outside the core engine.

## Family Identity
- display name: `DuckDB`
- `profile_id`: `duckdb`
- primary surface class: `embedded_sql`
- primary donor protocol or carrier: `embedded_client_api`
- shared lowering base: `embedded_sql_family`
- listener mode: `optional`
- listener executable: `sb_listener_duckdb`
- parser executable: `sb_parser_duckdb`
- parser package: `sb_pkg_duckdb_parser`
- compiler UDR package: `sb_pkg_duckdb_compiler_udr`
- emulation UDR package: `sb_pkg_duckdb_emulation_udr`
- compiler entrypoint: `compiler_duckdb`
- engine generator entrypoint: `engine_duckdb`
- bundle contract id: `sb_emulation_bundle_duckdb/v2`
- supports `MESSAGE_BLR`: `no`
- supports `EXECUTABLE_BLR`: `no`

## Admission Reason
DuckDB uses an embedded SQL surface with donor-visible datatypes and plan formatting that do not map cleanly to wire families.

## Authoritative Reference Inputs
- local source-backed packet: `docs/reference/reference_library/emulation_1_to_1_engine_reference_packets_2026-04-02/duckdb/README.md`
- official donor web supplement: `docs/reference/reference_library/emulation_1_to_1_engine_reference_packets_2026-04-02/duckdb/OFFICIAL_WEB_REFERENCE_SUPPLEMENT.md`
- error-code packet root: `docs/reference/reference_library/error_code_reference_packets_2026-04-02/README.md`
- packet manifest state: protocol adapter `no`, parser agent `no`, bridge adapter `no`, catalog overlay `yes`, compatibility suite `no`
- official donor web references available now: `2/2`

## Current Reference Coverage Snapshot
- (a) Datatypes: donor `2/2` paths, ScratchBird `4/4` paths.
- (b) Indexes: donor `3/3` paths, ScratchBird `4/4` paths.
- (c) Parser to SB AST / V3 Dialect: donor `2/2` paths, ScratchBird `5/5` paths.
- (d) Wire Protocol: donor `1/1` paths, ScratchBird `2/2` paths.
- (e) Authentication: donor `1/1` paths, ScratchBird `4/4` paths.
- (f) Client Bridge / UDR Target Surface: donor `2/2` paths, ScratchBird `2/2` paths.
- (g) Plan Layout / Optimizer Output: donor `2/2` paths, ScratchBird `3/3` paths.
- (h) Error Codes: donor `1/1` paths, ScratchBird `4/4` paths.
- (i) Page Types and Storage Optimizations: donor `1/1` paths, ScratchBird `6/6` paths.
- (j) Regression Tests and Tooling: donor `1/1` paths, ScratchBird `3/3` paths.
- (k) Catalog / System Tables / New Empty Database: donor `2/2` paths, ScratchBird `4/4` paths.

## Parser Package Contract
1. Own the full `DuckDB` client-facing request lifecycle for `embedded_client_api`.
2. Translate donor-visible datatypes, result metadata, errors, and plan requests into canonical ScratchBird AST, SBLR, and metrics packets.
3. Keep all donor-visible session state, prepared handles, streaming state, and async or notice surfaces inside the parser process boundary for one connection only.
4. Fail closed when the bundle is not `READY`, when the emulated database binding is unavailable, or when the request class is not admitted by the family capability set.

## Compiler UDR Contract
1. `"compiler_duckdb"` is the only engine-side entrypoint allowed to translate donor-generated text or helper payloads for `DuckDB`.
2. It must reuse the same capability, AST, datatype, and function admission rules as the parser package.
3. It must verify privilege class and request shape before emitting AST or SBLR.
4. It must never render donor protocol frames or donor-visible text directly to a client.

## Engine Generator And Emulation UDR Contract
1. `"engine_duckdb"` owns the family-specific engine-side emulation support.
2. It must bootstrap and validate: DuckDB system-table and pragma overlays exposed inside the emulated schema root.
3. It must ship the internal donor client required for: internal embedded-client bridge used for migration and UDR passthrough without external donor libraries.
4. It must provide migration, bridge, catalog-bootstrap, empty-database-default, and validation routines for this family.

## Donor-Specific Beta 2 Deltas
- Parser package is still mandatory, but the family may ship as a library shim plus optional listener rather than a mandatory socket server.
- Compiler UDR must own donor-specific PRAGMA, system-table, and helper statement translation.
- Engine UDR must bootstrap DuckDB-visible catalog surfaces and extension-visible metadata views inside the branch sandbox.

## Regression And Bridge Requirements
- regression baseline: DuckDB SQL test corpus and local-shell style explain/query fixtures.
- internal bridge requirement: internal embedded-client bridge used for migration and UDR passthrough without external donor libraries.
- family plan renderer must convert canonical `RuntimePlan` output into donor-visible explain or profile formats for `DuckDB`.
- family error map pack must cover every donor error code admitted by the local packet and official donor docs.

## Current Evidence Gaps To Preserve During Implementation
- current ScratchBird implementation gap: `wire_protocol` -> No dedicated ScratchBird protocol adapter file detected
- current ScratchBird implementation gap: `parser_ast` -> No dedicated ScratchBird external parser agent detected
- current ScratchBird implementation gap: `client_bridge` -> No dedicated ScratchBird FDW adapter detected

## Sample Bundle Registration
```cpp
static const scratchbird::core::EmulationPackageScaffold kDuckdbScaffold = {
    "duckdb",
    "sb_listener_duckdb",
    "sb_parser_duckdb",
    "sb_pkg_duckdb_parser",
    "sb_pkg_duckdb_compiler_udr",
    "sb_pkg_duckdb_emulation_udr",
    "sb_emulation_bundle_duckdb/v2",
    true,
    true,
    false,
    false,
};

register_emulation_entrypoint("compiler_duckdb", &DuckdbCompiler::invoke);
register_emulation_entrypoint("engine_duckdb", &DuckdbEngine::invoke);
```

## Beta 2 Completion Rule
`DuckDB` is `READY` only after parser, compiler UDR, emulation UDR, web and local reference inputs, virtual catalog goldens, plan render goldens, donor error-map pack, and regression harness evidence are all present.

