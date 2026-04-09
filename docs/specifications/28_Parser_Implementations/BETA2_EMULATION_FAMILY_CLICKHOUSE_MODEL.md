# Beta 2 Emulation Family Model - ClickHouse

## Purpose
Define the Beta 2 emulation bundle required to present ScratchBird as a donor-compatible `ClickHouse` endpoint while keeping donor-specific parsing, protocol, catalog, bridge, and error behavior outside the core engine.

## Family Identity
- display name: `ClickHouse`
- `profile_id`: `clickhouse`
- primary surface class: `sql_wire`
- primary donor protocol or carrier: `clickhouse_native_tcp`
- shared lowering base: `clickhouse_sql_family`
- listener mode: `required`
- listener executable: `sb_listener_clickhouse`
- parser executable: `sb_parser_clickhouse`
- parser package: `sb_pkg_clickhouse_parser`
- compiler UDR package: `sb_pkg_clickhouse_compiler_udr`
- emulation UDR package: `sb_pkg_clickhouse_emulation_udr`
- compiler entrypoint: `compiler_clickhouse`
- engine generator entrypoint: `engine_clickhouse`
- bundle contract id: `sb_emulation_bundle_clickhouse/v2`
- supports `MESSAGE_BLR`: `no`
- supports `EXECUTABLE_BLR`: `no`

## Admission Reason
ClickHouse introduces donor-visible datatypes, explain layouts, and index families not covered by SQL-wire families alone.

## Authoritative Reference Inputs
- local source-backed packet: `docs/reference/reference_library/emulation_1_to_1_engine_reference_packets_2026-04-02/clickhouse/README.md`
- official donor web supplement: `docs/reference/reference_library/emulation_1_to_1_engine_reference_packets_2026-04-02/clickhouse/OFFICIAL_WEB_REFERENCE_SUPPLEMENT.md`
- error-code packet root: `docs/reference/reference_library/error_code_reference_packets_2026-04-02/README.md`
- packet manifest state: protocol adapter `no`, parser agent `no`, bridge adapter `no`, catalog overlay `yes`, compatibility suite `no`
- official donor web references available now: `2/2`

## Current Reference Coverage Snapshot
- (a) Datatypes: donor `1/1` paths, ScratchBird `4/4` paths.
- (b) Indexes: donor `1/1` paths, ScratchBird `4/4` paths.
- (c) Parser to SB AST / V3 Dialect: donor `1/1` paths, ScratchBird `5/5` paths.
- (d) Wire Protocol: donor `2/2` paths, ScratchBird `2/2` paths.
- (e) Authentication: donor `1/1` paths, ScratchBird `4/4` paths.
- (f) Client Bridge / UDR Target Surface: donor `1/1` paths, ScratchBird `2/2` paths.
- (g) Plan Layout / Optimizer Output: donor `1/1` paths, ScratchBird `3/3` paths.
- (h) Error Codes: donor `1/1` paths, ScratchBird `4/4` paths.
- (i) Page Types and Storage Optimizations: donor `2/2` paths, ScratchBird `6/6` paths.
- (j) Regression Tests and Tooling: donor `1/1` paths, ScratchBird `3/3` paths.
- (k) Catalog / System Tables / New Empty Database: donor `2/2` paths, ScratchBird `4/4` paths.

## Parser Package Contract
1. Own the full `ClickHouse` client-facing request lifecycle for `clickhouse_native_tcp`.
2. Translate donor-visible datatypes, result metadata, errors, and plan requests into canonical ScratchBird AST, SBLR, and metrics packets.
3. Keep all donor-visible session state, prepared handles, streaming state, and async or notice surfaces inside the parser process boundary for one connection only.
4. Fail closed when the bundle is not `READY`, when the emulated database binding is unavailable, or when the request class is not admitted by the family capability set.

## Compiler UDR Contract
1. `"compiler_clickhouse"` is the only engine-side entrypoint allowed to translate donor-generated text or helper payloads for `ClickHouse`.
2. It must reuse the same capability, AST, datatype, and function admission rules as the parser package.
3. It must verify privilege class and request shape before emitting AST or SBLR.
4. It must never render donor protocol frames or donor-visible text directly to a client.

## Engine Generator And Emulation UDR Contract
1. `"engine_clickhouse"` owns the family-specific engine-side emulation support.
2. It must bootstrap and validate: `system` database overlays and MergeTree-visible metadata surfaces.
3. It must ship the internal donor client required for: internal ClickHouse native client used by migration and plan-validation flows.
4. It must provide migration, bridge, catalog-bootstrap, empty-database-default, and validation routines for this family.

## Donor-Specific Beta 2 Deltas
- Parser must admit ClickHouse SQL modifiers, settings, and explain forms already mapped into the Beta2 donor dialect canon.
- Compiler UDR must lower MergeTree helper SQL, system-table queries, and family-owned command forms to shared SBLR.
- Engine UDR must expose `system` overlays and ClickHouse-style storage or settings introspection without leaking cross-root metadata.

## Regression And Bridge Requirements
- regression baseline: ClickHouse query suites, native-client fixtures, and explain plan goldens.
- internal bridge requirement: internal ClickHouse native client used by migration and plan-validation flows.
- family plan renderer must convert canonical `RuntimePlan` output into donor-visible explain or profile formats for `ClickHouse`.
- family error map pack must cover every donor error code admitted by the local packet and official donor docs.

## Current Evidence Gaps To Preserve During Implementation
- current ScratchBird implementation gap: `wire_protocol` -> No dedicated ScratchBird protocol adapter file detected
- current ScratchBird implementation gap: `parser_ast` -> No dedicated ScratchBird external parser agent detected
- current ScratchBird implementation gap: `client_bridge` -> No dedicated ScratchBird FDW adapter detected

## Sample Bundle Registration
```cpp
static const scratchbird::core::EmulationPackageScaffold kClickhouseScaffold = {
    "clickhouse",
    "sb_listener_clickhouse",
    "sb_parser_clickhouse",
    "sb_pkg_clickhouse_parser",
    "sb_pkg_clickhouse_compiler_udr",
    "sb_pkg_clickhouse_emulation_udr",
    "sb_emulation_bundle_clickhouse/v2",
    true,
    true,
    false,
    false,
};

register_emulation_entrypoint("compiler_clickhouse", &ClickhouseCompiler::invoke);
register_emulation_entrypoint("engine_clickhouse", &ClickhouseEngine::invoke);
```

## Beta 2 Completion Rule
`ClickHouse` is `READY` only after parser, compiler UDR, emulation UDR, web and local reference inputs, virtual catalog goldens, plan render goldens, donor error-map pack, and regression harness evidence are all present.

