# Beta 2 Emulation Family Model - FirebirdSQL

## Purpose
Define the Beta 2 emulation bundle required to present ScratchBird as a donor-compatible `FirebirdSQL` endpoint while keeping donor-specific parsing, protocol, catalog, bridge, and error behavior outside the core engine.

## Family Identity
- display name: `FirebirdSQL`
- `profile_id`: `firebirdsql`
- primary surface class: `sql_wire`
- primary donor protocol or carrier: `firebird_remote_protocol`
- shared lowering base: `firebird_dedicated_family`
- listener mode: `required`
- listener executable: `sb_listener_fb`
- parser executable: `sb_parser_fb`
- parser package: `sb_pkg_firebirdsql_parser`
- compiler UDR package: `sb_pkg_firebirdsql_compiler_udr`
- emulation UDR package: `sb_pkg_firebirdsql_emulation_udr`
- compiler entrypoint: `compiler_firebirdsql`
- engine generator entrypoint: `engine_firebirdsql`
- bundle contract id: `sb_emulation_bundle_firebirdsql/v2`
- supports `MESSAGE_BLR`: `yes`
- supports `EXECUTABLE_BLR`: `yes`

## Admission Reason
native Firebird wire, BLR-adjacent payloads, system tables, and error surfaces must remain first-class.

## Authoritative Reference Inputs
- local source-backed packet: `docs/reference/reference_library/emulation_1_to_1_engine_reference_packets_2026-04-02/firebird/README.md`
- official donor web supplement: `docs/reference/reference_library/emulation_1_to_1_engine_reference_packets_2026-04-02/firebird/OFFICIAL_WEB_REFERENCE_SUPPLEMENT.md`
- error-code packet root: `docs/reference/reference_library/error_code_reference_packets_2026-04-02/README.md`
- packet manifest state: protocol adapter `yes`, parser agent `yes`, bridge adapter `yes`, catalog overlay `yes`, compatibility suite `yes`
- official donor web references available now: `2/2`

## Current Reference Coverage Snapshot
- (a) Datatypes: donor `2/2` paths, ScratchBird `4/4` paths.
- (b) Indexes: donor `2/2` paths, ScratchBird `4/4` paths.
- (c) Parser to SB AST / V3 Dialect: donor `1/1` paths, ScratchBird `9/9` paths.
- (d) Wire Protocol: donor `2/2` paths, ScratchBird `3/3` paths.
- (e) Authentication: donor `2/2` paths, ScratchBird `4/4` paths.
- (f) Client Bridge / UDR Target Surface: donor `2/2` paths, ScratchBird `3/3` paths.
- (g) Plan Layout / Optimizer Output: donor `2/2` paths, ScratchBird `3/3` paths.
- (h) Error Codes: donor `2/2` paths, ScratchBird `4/4` paths.
- (i) Page Types and Storage Optimizations: donor `3/3` paths, ScratchBird `6/6` paths.
- (j) Regression Tests and Tooling: donor `3/3` paths, ScratchBird `3/3` paths.
- (k) Catalog / System Tables / New Empty Database: donor `2/2` paths, ScratchBird `4/4` paths.

## Parser Package Contract
1. Own the full `FirebirdSQL` client-facing request lifecycle for `firebird_remote_protocol`.
2. Translate donor-visible datatypes, result metadata, errors, and plan requests into canonical ScratchBird AST, SBLR, and metrics packets.
3. Keep all donor-visible session state, prepared handles, streaming state, and async or notice surfaces inside the parser process boundary for one connection only.
4. Fail closed when the bundle is not `READY`, when the emulated database binding is unavailable, or when the request class is not admitted by the family capability set.

## Compiler UDR Contract
1. `"compiler_firebirdsql"` is the only engine-side entrypoint allowed to translate donor-generated text or helper payloads for `FirebirdSQL`.
2. It must reuse the same capability, AST, datatype, and function admission rules as the parser package.
3. It must verify privilege class and request shape before emitting AST or SBLR.
4. It must never render donor protocol frames or donor-visible text directly to a client.

## Engine Generator And Emulation UDR Contract
1. `"engine_firebirdsql"` owns the family-specific engine-side emulation support.
2. It must bootstrap and validate: RDB$ system-table overlays filtered to the emulated database root.
3. It must ship the internal donor client required for: internal Firebird-compatible attachment client used by migration and bridge UDR flows.
4. It must provide migration, bridge, catalog-bootstrap, empty-database-default, and validation routines for this family.

## Donor-Specific Beta 2 Deltas
- Parser must preserve Firebird attachment, transaction, blob, and service-manager framing semantics.
- Compiler UDR must admit SQL text plus Firebird BLR-origin dynamic payload helpers for engine-owned translation paths.
- Engine UDR must bootstrap `RDB$*` overlays with empty-database defaults and Firebird-style object visibility.

## Regression And Bridge Requirements
- regression baseline: isql-driven Firebird compatibility suites and Firebird project regression tooling.
- internal bridge requirement: internal Firebird-compatible attachment client used by migration and bridge UDR flows.
- family plan renderer must convert canonical `RuntimePlan` output into donor-visible explain or profile formats for `FirebirdSQL`.
- family error map pack must cover every donor error code admitted by the local packet and official donor docs.

## Current Evidence Gaps To Preserve During Implementation
- no packet-level gaps are currently recorded for this family.

## Sample Bundle Registration
```cpp
static const scratchbird::core::EmulationPackageScaffold kFirebirdsqlScaffold = {
    "firebirdsql",
    "sb_listener_fb",
    "sb_parser_fb",
    "sb_pkg_firebirdsql_parser",
    "sb_pkg_firebirdsql_compiler_udr",
    "sb_pkg_firebirdsql_emulation_udr",
    "sb_emulation_bundle_firebirdsql/v2",
    true,
    true,
    true,
    true,
};

register_emulation_entrypoint("compiler_firebirdsql", &FirebirdsqlCompiler::invoke);
register_emulation_entrypoint("engine_firebirdsql", &FirebirdsqlEngine::invoke);
```

## Beta 2 Completion Rule
`FirebirdSQL` is `READY` only after parser, compiler UDR, emulation UDR, web and local reference inputs, virtual catalog goldens, plan render goldens, donor error-map pack, and regression harness evidence are all present.

