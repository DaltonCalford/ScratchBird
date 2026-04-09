# Beta 2 Emulation Family Model - MongoDB

## Purpose
Define the Beta 2 emulation bundle required to present ScratchBird as a donor-compatible `MongoDB` endpoint while keeping donor-specific parsing, protocol, catalog, bridge, and error behavior outside the core engine.

## Family Identity
- display name: `MongoDB`
- `profile_id`: `mongodb`
- primary surface class: `document_command`
- primary donor protocol or carrier: `op_msg`
- shared lowering base: `document_command_family`
- listener mode: `required`
- listener executable: `sb_listener_mongodb`
- parser executable: `sb_parser_mongodb`
- parser package: `sb_pkg_mongodb_parser`
- compiler UDR package: `sb_pkg_mongodb_compiler_udr`
- emulation UDR package: `sb_pkg_mongodb_emulation_udr`
- compiler entrypoint: `compiler_mongodb`
- engine generator entrypoint: `engine_mongodb`
- bundle contract id: `sb_emulation_bundle_mongodb/v2`
- supports `MESSAGE_BLR`: `no`
- supports `EXECUTABLE_BLR`: `no`

## Admission Reason
MongoDB requires BSON, document commands, OP_MSG framing, and collection/index metadata beyond SQL-wire assumptions.

## Authoritative Reference Inputs
- local source-backed packet: `docs/reference/reference_library/emulation_1_to_1_engine_reference_packets_2026-04-02/mongodb/README.md`
- official donor web supplement: `docs/reference/reference_library/emulation_1_to_1_engine_reference_packets_2026-04-02/mongodb/OFFICIAL_WEB_REFERENCE_SUPPLEMENT.md`
- error-code packet root: `docs/reference/reference_library/error_code_reference_packets_2026-04-02/README.md`
- packet manifest state: protocol adapter `no`, parser agent `no`, bridge adapter `no`, catalog overlay `yes`, compatibility suite `no`
- official donor web references available now: `2/2`

## Current Reference Coverage Snapshot
- (a) Datatypes: donor `1/1` paths, ScratchBird `4/4` paths.
- (b) Indexes: donor `2/2` paths, ScratchBird `4/4` paths.
- (c) Parser to SB AST / V3 Dialect: donor `2/2` paths, ScratchBird `5/5` paths.
- (d) Wire Protocol: donor `1/1` paths, ScratchBird `2/2` paths.
- (e) Authentication: donor `1/1` paths, ScratchBird `4/4` paths.
- (f) Client Bridge / UDR Target Surface: donor `1/1` paths, ScratchBird `2/2` paths.
- (g) Plan Layout / Optimizer Output: donor `2/2` paths, ScratchBird `3/3` paths.
- (h) Error Codes: donor `1/1` paths, ScratchBird `4/4` paths.
- (i) Page Types and Storage Optimizations: donor `2/2` paths, ScratchBird `6/6` paths.
- (j) Regression Tests and Tooling: donor `1/1` paths, ScratchBird `3/3` paths.
- (k) Catalog / System Tables / New Empty Database: donor `2/2` paths, ScratchBird `4/4` paths.

## Parser Package Contract
1. Own the full `MongoDB` client-facing request lifecycle for `op_msg`.
2. Translate donor-visible datatypes, result metadata, errors, and plan requests into canonical ScratchBird AST, SBLR, and metrics packets.
3. Keep all donor-visible session state, prepared handles, streaming state, and async or notice surfaces inside the parser process boundary for one connection only.
4. Fail closed when the bundle is not `READY`, when the emulated database binding is unavailable, or when the request class is not admitted by the family capability set.

## Compiler UDR Contract
1. `"compiler_mongodb"` is the only engine-side entrypoint allowed to translate donor-generated text or helper payloads for `MongoDB`.
2. It must reuse the same capability, AST, datatype, and function admission rules as the parser package.
3. It must verify privilege class and request shape before emitting AST or SBLR.
4. It must never render donor protocol frames or donor-visible text directly to a client.

## Engine Generator And Emulation UDR Contract
1. `"engine_mongodb"` owns the family-specific engine-side emulation support.
2. It must bootstrap and validate: `admin`, `local`, `config`, and synthetic collection metadata overlays.
3. It must ship the internal donor client required for: internal MongoDB-compatible client used by migration, sync, and bridge UDR flows.
4. It must provide migration, bridge, catalog-bootstrap, empty-database-default, and validation routines for this family.

## Donor-Specific Beta 2 Deltas
- Parser must own OP_MSG framing, command envelopes, BSON conversion, and explain/result document shaping.
- Compiler UDR must lower donor command helpers, generated queries, and metadata probes into shared document-oriented SBLR carriers.
- Engine UDR must expose collection and system metadata views plus empty-database bootstrap defaults for donor-visible admin collections.

## Regression And Bridge Requirements
- regression baseline: MongoDB command fixtures, shell-driven tests, and explain or stats goldens.
- internal bridge requirement: internal MongoDB-compatible client used by migration, sync, and bridge UDR flows.
- family plan renderer must convert canonical `RuntimePlan` output into donor-visible explain or profile formats for `MongoDB`.
- family error map pack must cover every donor error code admitted by the local packet and official donor docs.

## Current Evidence Gaps To Preserve During Implementation
- current ScratchBird implementation gap: `wire_protocol` -> No dedicated ScratchBird protocol adapter file detected
- current ScratchBird implementation gap: `parser_ast` -> No dedicated ScratchBird external parser agent detected
- current ScratchBird implementation gap: `client_bridge` -> No dedicated ScratchBird FDW adapter detected

## Sample Bundle Registration
```cpp
static const scratchbird::core::EmulationPackageScaffold kMongodbScaffold = {
    "mongodb",
    "sb_listener_mongodb",
    "sb_parser_mongodb",
    "sb_pkg_mongodb_parser",
    "sb_pkg_mongodb_compiler_udr",
    "sb_pkg_mongodb_emulation_udr",
    "sb_emulation_bundle_mongodb/v2",
    true,
    true,
    false,
    false,
};

register_emulation_entrypoint("compiler_mongodb", &MongodbCompiler::invoke);
register_emulation_entrypoint("engine_mongodb", &MongodbEngine::invoke);
```

## Beta 2 Completion Rule
`MongoDB` is `READY` only after parser, compiler UDR, emulation UDR, web and local reference inputs, virtual catalog goldens, plan render goldens, donor error-map pack, and regression harness evidence are all present.

