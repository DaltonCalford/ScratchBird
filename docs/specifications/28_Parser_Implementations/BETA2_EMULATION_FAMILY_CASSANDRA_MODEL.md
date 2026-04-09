# Beta 2 Emulation Family Model - Cassandra

## Purpose
Define the Beta 2 emulation bundle required to present ScratchBird as a donor-compatible `Cassandra` endpoint while keeping donor-specific parsing, protocol, catalog, bridge, and error behavior outside the core engine.

## Family Identity
- display name: `Cassandra`
- `profile_id`: `cassandra`
- primary surface class: `cql_wire`
- primary donor protocol or carrier: `cassandra_native_v5`
- shared lowering base: `cql_family`
- listener mode: `required`
- listener executable: `sb_listener_cassandra`
- parser executable: `sb_parser_cassandra`
- parser package: `sb_pkg_cassandra_parser`
- compiler UDR package: `sb_pkg_cassandra_compiler_udr`
- emulation UDR package: `sb_pkg_cassandra_emulation_udr`
- compiler entrypoint: `compiler_cassandra`
- engine generator entrypoint: `engine_cassandra`
- bundle contract id: `sb_emulation_bundle_cassandra/v2`
- supports `MESSAGE_BLR`: `no`
- supports `EXECUTABLE_BLR`: `no`

## Admission Reason
Cassandra requires a CQL-native protocol, schema metadata, and indexing model rather than an SQL-wire clone.

## Authoritative Reference Inputs
- local source-backed packet: `docs/reference/reference_library/emulation_1_to_1_engine_reference_packets_2026-04-02/cassandra/README.md`
- official donor web supplement: `docs/reference/reference_library/emulation_1_to_1_engine_reference_packets_2026-04-02/cassandra/OFFICIAL_WEB_REFERENCE_SUPPLEMENT.md`
- error-code packet root: `docs/reference/reference_library/error_code_reference_packets_2026-04-02/README.md`
- packet manifest state: protocol adapter `no`, parser agent `no`, bridge adapter `no`, catalog overlay `yes`, compatibility suite `no`
- official donor web references available now: `2/2`

## Current Reference Coverage Snapshot
- (a) Datatypes: donor `1/1` paths, ScratchBird `4/4` paths.
- (b) Indexes: donor `1/1` paths, ScratchBird `4/4` paths.
- (c) Parser to SB AST / V3 Dialect: donor `2/2` paths, ScratchBird `5/5` paths.
- (d) Wire Protocol: donor `1/1` paths, ScratchBird `2/2` paths.
- (e) Authentication: donor `2/2` paths, ScratchBird `4/4` paths.
- (f) Client Bridge / UDR Target Surface: donor `2/2` paths, ScratchBird `2/2` paths.
- (g) Plan Layout / Optimizer Output: donor `2/2` paths, ScratchBird `3/3` paths.
- (h) Error Codes: donor `1/1` paths, ScratchBird `4/4` paths.
- (i) Page Types and Storage Optimizations: donor `2/2` paths, ScratchBird `6/6` paths.
- (j) Regression Tests and Tooling: donor `3/3` paths, ScratchBird `3/3` paths.
- (k) Catalog / System Tables / New Empty Database: donor `2/2` paths, ScratchBird `4/4` paths.

## Parser Package Contract
1. Own the full `Cassandra` client-facing request lifecycle for `cassandra_native_v5`.
2. Translate donor-visible datatypes, result metadata, errors, and plan requests into canonical ScratchBird AST, SBLR, and metrics packets.
3. Keep all donor-visible session state, prepared handles, streaming state, and async or notice surfaces inside the parser process boundary for one connection only.
4. Fail closed when the bundle is not `READY`, when the emulated database binding is unavailable, or when the request class is not admitted by the family capability set.

## Compiler UDR Contract
1. `"compiler_cassandra"` is the only engine-side entrypoint allowed to translate donor-generated text or helper payloads for `Cassandra`.
2. It must reuse the same capability, AST, datatype, and function admission rules as the parser package.
3. It must verify privilege class and request shape before emitting AST or SBLR.
4. It must never render donor protocol frames or donor-visible text directly to a client.

## Engine Generator And Emulation UDR Contract
1. `"engine_cassandra"` owns the family-specific engine-side emulation support.
2. It must bootstrap and validate: `system`, `system_schema`, and `system_views` overlays with keyspace-root filtering.
3. It must ship the internal donor client required for: internal Cassandra-native client used for migration and validation against donor clusters.
4. It must provide migration, bridge, catalog-bootstrap, empty-database-default, and validation routines for this family.

## Donor-Specific Beta 2 Deltas
- Parser must own native protocol framing, CQL statement classification, paging state, and metadata response shaping.
- Compiler UDR must lower CQL DDL, DML, and schema-introspection commands to shared AST/SBLR without inventing SQL-only semantics.
- Engine UDR must publish `system_*` overlays and bootstrap keyspaces so every emulated database root is sandboxed.

## Regression And Bridge Requirements
- regression baseline: native protocol fixtures, `cqlsh`, and donor distributed/unit test suites.
- internal bridge requirement: internal Cassandra-native client used for migration and validation against donor clusters.
- family plan renderer must convert canonical `RuntimePlan` output into donor-visible explain or profile formats for `Cassandra`.
- family error map pack must cover every donor error code admitted by the local packet and official donor docs.

## Current Evidence Gaps To Preserve During Implementation
- current ScratchBird implementation gap: `wire_protocol` -> No dedicated ScratchBird protocol adapter file detected
- current ScratchBird implementation gap: `parser_ast` -> No dedicated ScratchBird external parser agent detected
- current ScratchBird implementation gap: `client_bridge` -> No dedicated ScratchBird FDW adapter detected

## Sample Bundle Registration
```cpp
static const scratchbird::core::EmulationPackageScaffold kCassandraScaffold = {
    "cassandra",
    "sb_listener_cassandra",
    "sb_parser_cassandra",
    "sb_pkg_cassandra_parser",
    "sb_pkg_cassandra_compiler_udr",
    "sb_pkg_cassandra_emulation_udr",
    "sb_emulation_bundle_cassandra/v2",
    true,
    true,
    false,
    false,
};

register_emulation_entrypoint("compiler_cassandra", &CassandraCompiler::invoke);
register_emulation_entrypoint("engine_cassandra", &CassandraEngine::invoke);
```

## Beta 2 Completion Rule
`Cassandra` is `READY` only after parser, compiler UDR, emulation UDR, web and local reference inputs, virtual catalog goldens, plan render goldens, donor error-map pack, and regression harness evidence are all present.

