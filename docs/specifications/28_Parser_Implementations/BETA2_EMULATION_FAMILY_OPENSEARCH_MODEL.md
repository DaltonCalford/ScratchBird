# Beta 2 Emulation Family Model - OpenSearch

## Purpose
Define the Beta 2 emulation bundle required to present ScratchBird as a donor-compatible `OpenSearch` endpoint while keeping donor-specific parsing, protocol, catalog, bridge, and error behavior outside the core engine.

## Family Identity
- display name: `OpenSearch`
- `profile_id`: `opensearch`
- primary surface class: `rest_json`
- primary donor protocol or carrier: `http_json_rest`
- shared lowering base: `search_document_family`
- listener mode: `required`
- listener executable: `sb_listener_opensearch`
- parser executable: `sb_parser_opensearch`
- parser package: `sb_pkg_opensearch_parser`
- compiler UDR package: `sb_pkg_opensearch_compiler_udr`
- emulation UDR package: `sb_pkg_opensearch_emulation_udr`
- compiler entrypoint: `compiler_opensearch`
- engine generator entrypoint: `engine_opensearch`
- bundle contract id: `sb_emulation_bundle_opensearch/v2`
- supports `MESSAGE_BLR`: `no`
- supports `EXECUTABLE_BLR`: `no`

## Admission Reason
OpenSearch exposes REST/JSON semantics, mapping types, plan-style explain output, and cluster metadata instead of SQL-wire surfaces.

## Authoritative Reference Inputs
- local source-backed packet: `docs/reference/reference_library/emulation_1_to_1_engine_reference_packets_2026-04-02/opensearch/README.md`
- official donor web supplement: `docs/reference/reference_library/emulation_1_to_1_engine_reference_packets_2026-04-02/opensearch/OFFICIAL_WEB_REFERENCE_SUPPLEMENT.md`
- error-code packet root: `docs/reference/reference_library/error_code_reference_packets_2026-04-02/README.md`
- packet manifest state: protocol adapter `no`, parser agent `no`, bridge adapter `no`, catalog overlay `yes`, compatibility suite `no`
- official donor web references available now: `2/2`

## Current Reference Coverage Snapshot
- (a) Datatypes: donor `1/1` paths, ScratchBird `4/4` paths.
- (b) Indexes: donor `2/2` paths, ScratchBird `4/4` paths.
- (c) Parser to SB AST / V3 Dialect: donor `2/2` paths, ScratchBird `5/5` paths.
- (d) Wire Protocol: donor `2/2` paths, ScratchBird `2/2` paths.
- (e) Authentication: donor `2/2` paths, ScratchBird `4/4` paths.
- (f) Client Bridge / UDR Target Surface: donor `2/2` paths, ScratchBird `2/2` paths.
- (g) Plan Layout / Optimizer Output: donor `2/2` paths, ScratchBird `3/3` paths.
- (h) Error Codes: donor `2/2` paths, ScratchBird `4/4` paths.
- (i) Page Types and Storage Optimizations: donor `2/2` paths, ScratchBird `6/6` paths.
- (j) Regression Tests and Tooling: donor `1/1` paths, ScratchBird `3/3` paths.
- (k) Catalog / System Tables / New Empty Database: donor `1/1` paths, ScratchBird `4/4` paths.

## Parser Package Contract
1. Own the full `OpenSearch` client-facing request lifecycle for `http_json_rest`.
2. Translate donor-visible datatypes, result metadata, errors, and plan requests into canonical ScratchBird AST, SBLR, and metrics packets.
3. Keep all donor-visible session state, prepared handles, streaming state, and async or notice surfaces inside the parser process boundary for one connection only.
4. Fail closed when the bundle is not `READY`, when the emulated database binding is unavailable, or when the request class is not admitted by the family capability set.

## Compiler UDR Contract
1. `"compiler_opensearch"` is the only engine-side entrypoint allowed to translate donor-generated text or helper payloads for `OpenSearch`.
2. It must reuse the same capability, AST, datatype, and function admission rules as the parser package.
3. It must verify privilege class and request shape before emitting AST or SBLR.
4. It must never render donor protocol frames or donor-visible text directly to a client.

## Engine Generator And Emulation UDR Contract
1. `"engine_opensearch"` owns the family-specific engine-side emulation support.
2. It must bootstrap and validate: index, shard, cluster, and `_cat`-style synthetic metadata views under the emulated branch.
3. It must ship the internal donor client required for: internal HTTP/JSON client used by migration, validation, and passthrough UDR flows.
4. It must provide migration, bridge, catalog-bootstrap, empty-database-default, and validation routines for this family.

## Donor-Specific Beta 2 Deltas
- Parser must own HTTP routing, JSON request decoding, mapping and search API normalization, and response rendering.
- Compiler UDR must lower donor-generated query JSON, script wrappers, and helper operations to shared document-search SBLR carriers.
- Engine UDR must publish synthetic `_cat`, index settings, and cluster metadata views filtered to the emulated root.

## Regression And Bridge Requirements
- regression baseline: REST API fixtures, query explain goldens, and admin-surface compatibility tests.
- internal bridge requirement: internal HTTP/JSON client used by migration, validation, and passthrough UDR flows.
- family plan renderer must convert canonical `RuntimePlan` output into donor-visible explain or profile formats for `OpenSearch`.
- family error map pack must cover every donor error code admitted by the local packet and official donor docs.

## Current Evidence Gaps To Preserve During Implementation
- current ScratchBird implementation gap: `wire_protocol` -> No dedicated ScratchBird protocol adapter file detected
- current ScratchBird implementation gap: `parser_ast` -> No dedicated ScratchBird external parser agent detected
- current ScratchBird implementation gap: `client_bridge` -> No dedicated ScratchBird FDW adapter detected

## Sample Bundle Registration
```cpp
static const scratchbird::core::EmulationPackageScaffold kOpensearchScaffold = {
    "opensearch",
    "sb_listener_opensearch",
    "sb_parser_opensearch",
    "sb_pkg_opensearch_parser",
    "sb_pkg_opensearch_compiler_udr",
    "sb_pkg_opensearch_emulation_udr",
    "sb_emulation_bundle_opensearch/v2",
    true,
    true,
    false,
    false,
};

register_emulation_entrypoint("compiler_opensearch", &OpensearchCompiler::invoke);
register_emulation_entrypoint("engine_opensearch", &OpensearchEngine::invoke);
```

## Beta 2 Completion Rule
`OpenSearch` is `READY` only after parser, compiler UDR, emulation UDR, web and local reference inputs, virtual catalog goldens, plan render goldens, donor error-map pack, and regression harness evidence are all present.

