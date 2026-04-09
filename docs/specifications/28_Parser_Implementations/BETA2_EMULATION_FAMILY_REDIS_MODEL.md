# Beta 2 Emulation Family Model - Redis

## Purpose
Define the Beta 2 emulation bundle required to present ScratchBird as a donor-compatible `Redis` endpoint while keeping donor-specific parsing, protocol, catalog, bridge, and error behavior outside the core engine.

## Family Identity
- display name: `Redis`
- `profile_id`: `redis`
- primary surface class: `command_protocol`
- primary donor protocol or carrier: `resp2_resp3`
- shared lowering base: `key_command_family`
- listener mode: `required`
- listener executable: `sb_listener_redis`
- parser executable: `sb_parser_redis`
- parser package: `sb_pkg_redis_parser`
- compiler UDR package: `sb_pkg_redis_compiler_udr`
- emulation UDR package: `sb_pkg_redis_emulation_udr`
- compiler entrypoint: `compiler_redis`
- engine generator entrypoint: `engine_redis`
- bundle contract id: `sb_emulation_bundle_redis/v2`
- supports `MESSAGE_BLR`: `no`
- supports `EXECUTABLE_BLR`: `no`

## Admission Reason
Redis compatibility depends on RESP framing, command families, type metadata, and non-SQL result shaping.

## Authoritative Reference Inputs
- local source-backed packet: `docs/reference/reference_library/emulation_1_to_1_engine_reference_packets_2026-04-02/redis/README.md`
- official donor web supplement: `docs/reference/reference_library/emulation_1_to_1_engine_reference_packets_2026-04-02/redis/OFFICIAL_WEB_REFERENCE_SUPPLEMENT.md`
- error-code packet root: `docs/reference/reference_library/error_code_reference_packets_2026-04-02/README.md`
- packet manifest state: protocol adapter `no`, parser agent `no`, bridge adapter `no`, catalog overlay `yes`, compatibility suite `no`
- official donor web references available now: `2/2`

## Current Reference Coverage Snapshot
- (a) Datatypes: donor `2/2` paths, ScratchBird `4/4` paths.
- (b) Indexes: donor `3/3` paths, ScratchBird `4/4` paths.
- (c) Parser to SB AST / V3 Dialect: donor `2/2` paths, ScratchBird `5/5` paths.
- (d) Wire Protocol: donor `2/2` paths, ScratchBird `2/2` paths.
- (e) Authentication: donor `1/1` paths, ScratchBird `4/4` paths.
- (f) Client Bridge / UDR Target Surface: donor `2/2` paths, ScratchBird `2/2` paths.
- (g) Plan Layout / Optimizer Output: donor `1/1` paths, ScratchBird `3/3` paths.
- (h) Error Codes: donor `2/2` paths, ScratchBird `4/4` paths.
- (i) Page Types and Storage Optimizations: donor `3/3` paths, ScratchBird `6/6` paths.
- (j) Regression Tests and Tooling: donor `1/1` paths, ScratchBird `3/3` paths.
- (k) Catalog / System Tables / New Empty Database: donor `2/2` paths, ScratchBird `4/4` paths.

## Parser Package Contract
1. Own the full `Redis` client-facing request lifecycle for `resp2_resp3`.
2. Translate donor-visible datatypes, result metadata, errors, and plan requests into canonical ScratchBird AST, SBLR, and metrics packets.
3. Keep all donor-visible session state, prepared handles, streaming state, and async or notice surfaces inside the parser process boundary for one connection only.
4. Fail closed when the bundle is not `READY`, when the emulated database binding is unavailable, or when the request class is not admitted by the family capability set.

## Compiler UDR Contract
1. `"compiler_redis"` is the only engine-side entrypoint allowed to translate donor-generated text or helper payloads for `Redis`.
2. It must reuse the same capability, AST, datatype, and function admission rules as the parser package.
3. It must verify privilege class and request shape before emitting AST or SBLR.
4. It must never render donor protocol frames or donor-visible text directly to a client.

## Engine Generator And Emulation UDR Contract
1. `"engine_redis"` owns the family-specific engine-side emulation support.
2. It must bootstrap and validate: synthetic `INFO`, `COMMAND`, ACL, keyspace, and module metadata views.
3. It must ship the internal donor client required for: internal RESP-compatible client used for migration and bridge passthrough.
4. It must provide migration, bridge, catalog-bootstrap, empty-database-default, and validation routines for this family.

## Donor-Specific Beta 2 Deltas
- Parser must own RESP2/RESP3 framing, HELLO negotiation, command dispatch, push notifications, and error rendering.
- Compiler UDR must translate engine-origin command text or helper payloads into shared command-oriented SBLR envelopes.
- Engine UDR must expose synthetic metadata rows for `INFO`, ACL, modules, and keyspace introspection inside the emulated root.

## Regression And Bridge Requirements
- regression baseline: RESP2/RESP3 fixtures, command-golden corpus, and admin/introspection response goldens.
- internal bridge requirement: internal RESP-compatible client used for migration and bridge passthrough.
- family plan renderer must convert canonical `RuntimePlan` output into donor-visible explain or profile formats for `Redis`.
- family error map pack must cover every donor error code admitted by the local packet and official donor docs.

## Current Evidence Gaps To Preserve During Implementation
- current ScratchBird implementation gap: `wire_protocol` -> No dedicated ScratchBird protocol adapter file detected
- current ScratchBird implementation gap: `parser_ast` -> No dedicated ScratchBird external parser agent detected
- current ScratchBird implementation gap: `client_bridge` -> No dedicated ScratchBird FDW adapter detected

## Sample Bundle Registration
```cpp
static const scratchbird::core::EmulationPackageScaffold kRedisScaffold = {
    "redis",
    "sb_listener_redis",
    "sb_parser_redis",
    "sb_pkg_redis_parser",
    "sb_pkg_redis_compiler_udr",
    "sb_pkg_redis_emulation_udr",
    "sb_emulation_bundle_redis/v2",
    true,
    true,
    false,
    false,
};

register_emulation_entrypoint("compiler_redis", &RedisCompiler::invoke);
register_emulation_entrypoint("engine_redis", &RedisEngine::invoke);
```

## Beta 2 Completion Rule
`Redis` is `READY` only after parser, compiler UDR, emulation UDR, web and local reference inputs, virtual catalog goldens, plan render goldens, donor error-map pack, and regression harness evidence are all present.

