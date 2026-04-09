# Beta 2 Emulation Family Model - FoundationDB

## Purpose
Define the Beta 2 emulation bundle required to present ScratchBird as a donor-compatible `FoundationDB` endpoint while keeping donor-specific parsing, protocol, catalog, bridge, and error behavior outside the core engine.

## Family Identity
- display name: `FoundationDB`
- `profile_id`: `foundationdb`
- primary surface class: `api_transactional`
- primary donor protocol or carrier: `foundationdb_native_client_protocol`
- shared lowering base: `transactional_api_family`
- listener mode: `optional`
- listener executable: `sb_listener_foundationdb`
- parser executable: `sb_parser_foundationdb`
- parser package: `sb_pkg_foundationdb_parser`
- compiler UDR package: `sb_pkg_foundationdb_compiler_udr`
- emulation UDR package: `sb_pkg_foundationdb_emulation_udr`
- compiler entrypoint: `compiler_foundationdb`
- engine generator entrypoint: `engine_foundationdb`
- bundle contract id: `sb_emulation_bundle_foundationdb/v2`
- supports `MESSAGE_BLR`: `no`
- supports `EXECUTABLE_BLR`: `no`

## Admission Reason
FoundationDB is a transactional API platform, so emulation depends on tuple, versionstamp, directory, and error semantics rather than SQL-wire behavior.

## Authoritative Reference Inputs
- local source-backed packet: `docs/reference/reference_library/emulation_1_to_1_engine_reference_packets_2026-04-02/foundationdb/README.md`
- official donor web supplement: `docs/reference/reference_library/emulation_1_to_1_engine_reference_packets_2026-04-02/foundationdb/OFFICIAL_WEB_REFERENCE_SUPPLEMENT.md`
- error-code packet root: `docs/reference/reference_library/error_code_reference_packets_2026-04-02/README.md`
- packet manifest state: protocol adapter `no`, parser agent `no`, bridge adapter `no`, catalog overlay `yes`, compatibility suite `no`
- official donor web references available now: `2/2`

## Current Reference Coverage Snapshot
- (a) Datatypes: donor `3/3` paths, ScratchBird `4/4` paths.
- (b) Indexes: donor `2/2` paths, ScratchBird `4/4` paths.
- (c) Parser to SB AST / V3 Dialect: donor `1/1` paths, ScratchBird `5/5` paths.
- (d) Wire Protocol: donor `2/2` paths, ScratchBird `2/2` paths.
- (e) Authentication: donor `3/3` paths, ScratchBird `4/4` paths.
- (f) Client Bridge / UDR Target Surface: donor `2/2` paths, ScratchBird `2/2` paths.
- (g) Plan Layout / Optimizer Output: donor `2/2` paths, ScratchBird `3/3` paths.
- (h) Error Codes: donor `1/1` paths, ScratchBird `4/4` paths.
- (i) Page Types and Storage Optimizations: donor `2/2` paths, ScratchBird `6/6` paths.
- (j) Regression Tests and Tooling: donor `3/3` paths, ScratchBird `3/3` paths.
- (k) Catalog / System Tables / New Empty Database: donor `3/3` paths, ScratchBird `3/3` paths.

## Parser Package Contract
1. Own the full `FoundationDB` client-facing request lifecycle for `foundationdb_native_client_protocol`.
2. Translate donor-visible datatypes, result metadata, errors, and plan requests into canonical ScratchBird AST, SBLR, and metrics packets.
3. Keep all donor-visible session state, prepared handles, streaming state, and async or notice surfaces inside the parser process boundary for one connection only.
4. Fail closed when the bundle is not `READY`, when the emulated database binding is unavailable, or when the request class is not admitted by the family capability set.

## Compiler UDR Contract
1. `"compiler_foundationdb"` is the only engine-side entrypoint allowed to translate donor-generated text or helper payloads for `FoundationDB`.
2. It must reuse the same capability, AST, datatype, and function admission rules as the parser package.
3. It must verify privilege class and request shape before emitting AST or SBLR.
4. It must never render donor protocol frames or donor-visible text directly to a client.

## Engine Generator And Emulation UDR Contract
1. `"engine_foundationdb"` owns the family-specific engine-side emulation support.
2. It must bootstrap and validate: directory-layer, keyspace, and metadata overlays rather than SQL system tables.
3. It must ship the internal donor client required for: internal FoundationDB-compatible client used for migration, tuple encoding, and bridge passthrough.
4. It must provide migration, bridge, catalog-bootstrap, empty-database-default, and validation routines for this family.

## Donor-Specific Beta 2 Deltas
- Parser family may ship primarily as a client-library or service shim rather than a mandatory listener.
- Compiler UDR must lower donor helper strings and generated transactional operations into shared API-capable SBLR envelopes.
- Engine UDR must expose directory-layer and metadata overlays while preserving branch sandboxing and stable synthetic identities.

## Regression And Bridge Requirements
- regression baseline: FoundationDB binding tests, tuple-layer fixtures, and transactional API goldens.
- internal bridge requirement: internal FoundationDB-compatible client used for migration, tuple encoding, and bridge passthrough.
- family plan renderer must convert canonical `RuntimePlan` output into donor-visible explain or profile formats for `FoundationDB`.
- family error map pack must cover every donor error code admitted by the local packet and official donor docs.

## Current Evidence Gaps To Preserve During Implementation
- comparison deepening required: `indexes` -> No source-backed 1:1 index rows emitted in this packet
- current ScratchBird implementation gap: `wire_protocol` -> No dedicated ScratchBird protocol adapter file detected
- current ScratchBird implementation gap: `parser_ast` -> No dedicated ScratchBird external parser agent detected
- current ScratchBird implementation gap: `client_bridge` -> No dedicated ScratchBird FDW adapter detected

## Sample Bundle Registration
```cpp
static const scratchbird::core::EmulationPackageScaffold kFoundationdbScaffold = {
    "foundationdb",
    "sb_listener_foundationdb",
    "sb_parser_foundationdb",
    "sb_pkg_foundationdb_parser",
    "sb_pkg_foundationdb_compiler_udr",
    "sb_pkg_foundationdb_emulation_udr",
    "sb_emulation_bundle_foundationdb/v2",
    true,
    true,
    false,
    false,
};

register_emulation_entrypoint("compiler_foundationdb", &FoundationdbCompiler::invoke);
register_emulation_entrypoint("engine_foundationdb", &FoundationdbEngine::invoke);
```

## Beta 2 Completion Rule
`FoundationDB` is `READY` only after parser, compiler UDR, emulation UDR, web and local reference inputs, virtual catalog goldens, plan render goldens, donor error-map pack, and regression harness evidence are all present.

