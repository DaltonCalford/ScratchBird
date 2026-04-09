# Beta 2 Emulation Family Model - Neo4j

## Purpose
Define the Beta 2 emulation bundle required to present ScratchBird as a donor-compatible `Neo4j` endpoint while keeping donor-specific parsing, protocol, catalog, bridge, and error behavior outside the core engine.

## Family Identity
- display name: `Neo4j`
- `profile_id`: `neo4j`
- primary surface class: `graph_bolt`
- primary donor protocol or carrier: `bolt`
- shared lowering base: `graph_query_family`
- listener mode: `required`
- listener executable: `sb_listener_neo4j`
- parser executable: `sb_parser_neo4j`
- parser package: `sb_pkg_neo4j_parser`
- compiler UDR package: `sb_pkg_neo4j_compiler_udr`
- emulation UDR package: `sb_pkg_neo4j_emulation_udr`
- compiler entrypoint: `compiler_neo4j`
- engine generator entrypoint: `engine_neo4j`
- bundle contract id: `sb_emulation_bundle_neo4j/v2`
- supports `MESSAGE_BLR`: `no`
- supports `EXECUTABLE_BLR`: `no`

## Admission Reason
Neo4j exposes a graph data model, Bolt wire protocol, and procedure or explain surfaces that differ materially from SQL-wire engines.

## Authoritative Reference Inputs
- local source-backed packet: `docs/reference/reference_library/emulation_1_to_1_engine_reference_packets_2026-04-02/neo4j/README.md`
- official donor web supplement: `docs/reference/reference_library/emulation_1_to_1_engine_reference_packets_2026-04-02/neo4j/OFFICIAL_WEB_REFERENCE_SUPPLEMENT.md`
- error-code packet root: `docs/reference/reference_library/error_code_reference_packets_2026-04-02/README.md`
- packet manifest state: protocol adapter `no`, parser agent `no`, bridge adapter `no`, catalog overlay `yes`, compatibility suite `no`
- official donor web references available now: `2/2`

## Current Reference Coverage Snapshot
- (a) Datatypes: donor `2/2` paths, ScratchBird `4/4` paths.
- (b) Indexes: donor `3/3` paths, ScratchBird `4/4` paths.
- (c) Parser to SB AST / V3 Dialect: donor `1/1` paths, ScratchBird `5/5` paths.
- (d) Wire Protocol: donor `1/1` paths, ScratchBird `2/2` paths.
- (e) Authentication: donor `1/1` paths, ScratchBird `4/4` paths.
- (f) Client Bridge / UDR Target Surface: donor `2/2` paths, ScratchBird `2/2` paths.
- (g) Plan Layout / Optimizer Output: donor `2/2` paths, ScratchBird `3/3` paths.
- (h) Error Codes: donor `2/2` paths, ScratchBird `4/4` paths.
- (i) Page Types and Storage Optimizations: donor `2/2` paths, ScratchBird `6/6` paths.
- (j) Regression Tests and Tooling: donor `2/2` paths, ScratchBird `3/3` paths.
- (k) Catalog / System Tables / New Empty Database: donor `2/2` paths, ScratchBird `4/4` paths.

## Parser Package Contract
1. Own the full `Neo4j` client-facing request lifecycle for `bolt`.
2. Translate donor-visible datatypes, result metadata, errors, and plan requests into canonical ScratchBird AST, SBLR, and metrics packets.
3. Keep all donor-visible session state, prepared handles, streaming state, and async or notice surfaces inside the parser process boundary for one connection only.
4. Fail closed when the bundle is not `READY`, when the emulated database binding is unavailable, or when the request class is not admitted by the family capability set.

## Compiler UDR Contract
1. `"compiler_neo4j"` is the only engine-side entrypoint allowed to translate donor-generated text or helper payloads for `Neo4j`.
2. It must reuse the same capability, AST, datatype, and function admission rules as the parser package.
3. It must verify privilege class and request shape before emitting AST or SBLR.
4. It must never render donor protocol frames or donor-visible text directly to a client.

## Engine Generator And Emulation UDR Contract
1. `"engine_neo4j"` owns the family-specific engine-side emulation support.
2. It must bootstrap and validate: graph metadata, `SHOW` surfaces, and procedure-backed synthetic system views.
3. It must ship the internal donor client required for: internal Bolt-compatible client used by graph migration and bridge UDR routines.
4. It must provide migration, bridge, catalog-bootstrap, empty-database-default, and validation routines for this family.

## Donor-Specific Beta 2 Deltas
- Parser must own Bolt handshake, message framing, Cypher lowering, and graph-value translation.
- Compiler UDR must lower donor procedure calls, plan requests, and generated Cypher text into shared graph-capable SBLR structures.
- Engine UDR must bootstrap Neo4j-visible metadata views and empty graph/system defaults under the emulated root.

## Regression And Bridge Requirements
- regression baseline: Bolt protocol fixtures, Cypher compatibility corpus, and plan output goldens.
- internal bridge requirement: internal Bolt-compatible client used by graph migration and bridge UDR routines.
- family plan renderer must convert canonical `RuntimePlan` output into donor-visible explain or profile formats for `Neo4j`.
- family error map pack must cover every donor error code admitted by the local packet and official donor docs.

## Current Evidence Gaps To Preserve During Implementation
- current ScratchBird implementation gap: `wire_protocol` -> No dedicated ScratchBird protocol adapter file detected
- current ScratchBird implementation gap: `parser_ast` -> No dedicated ScratchBird external parser agent detected
- current ScratchBird implementation gap: `client_bridge` -> No dedicated ScratchBird FDW adapter detected

## Sample Bundle Registration
```cpp
static const scratchbird::core::EmulationPackageScaffold kNeo4JScaffold = {
    "neo4j",
    "sb_listener_neo4j",
    "sb_parser_neo4j",
    "sb_pkg_neo4j_parser",
    "sb_pkg_neo4j_compiler_udr",
    "sb_pkg_neo4j_emulation_udr",
    "sb_emulation_bundle_neo4j/v2",
    true,
    true,
    false,
    false,
};

register_emulation_entrypoint("compiler_neo4j", &Neo4JCompiler::invoke);
register_emulation_entrypoint("engine_neo4j", &Neo4JEngine::invoke);
```

## Beta 2 Completion Rule
`Neo4j` is `READY` only after parser, compiler UDR, emulation UDR, web and local reference inputs, virtual catalog goldens, plan render goldens, donor error-map pack, and regression harness evidence are all present.

