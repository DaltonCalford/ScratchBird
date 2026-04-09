# Beta 2 Emulation Family Model - YugabyteDB

## Purpose
Define the Beta 2 emulation bundle required to present ScratchBird as a donor-compatible `YugabyteDB` endpoint while keeping donor-specific parsing, protocol, catalog, bridge, and error behavior outside the core engine.

## Family Identity
- display name: `YugabyteDB`
- `profile_id`: `yugabytedb`
- primary surface class: `sql_wire`
- primary donor protocol or carrier: `postgresql_frontend_backend`
- shared lowering base: `postgresql_family`
- listener mode: `required`
- listener executable: `sb_listener_yugabytedb`
- parser executable: `sb_parser_yugabytedb`
- parser package: `sb_pkg_yugabytedb_parser`
- compiler UDR package: `sb_pkg_yugabytedb_compiler_udr`
- emulation UDR package: `sb_pkg_yugabytedb_emulation_udr`
- compiler entrypoint: `compiler_yugabytedb`
- engine generator entrypoint: `engine_yugabytedb`
- bundle contract id: `sb_emulation_bundle_yugabytedb/v2`
- supports `MESSAGE_BLR`: `no`
- supports `EXECUTABLE_BLR`: `no`

## Admission Reason
The current donor packet evidences the SQL-facing YugabyteDB family; that family builds on PostgreSQL wire with donor-specific distributed metadata and explain output.

## Authoritative Reference Inputs
- local source-backed packet: `docs/reference/reference_library/emulation_1_to_1_engine_reference_packets_2026-04-02/yugabytedb/README.md`
- official donor web supplement: `docs/reference/reference_library/emulation_1_to_1_engine_reference_packets_2026-04-02/yugabytedb/OFFICIAL_WEB_REFERENCE_SUPPLEMENT.md`
- error-code packet root: `docs/reference/reference_library/error_code_reference_packets_2026-04-02/README.md`
- packet manifest state: protocol adapter `no`, parser agent `no`, bridge adapter `no`, catalog overlay `yes`, compatibility suite `no`
- official donor web references available now: `2/2`

## Current Reference Coverage Snapshot
- (a) Datatypes: donor `1/1` paths, ScratchBird `4/4` paths.
- (b) Indexes: donor `2/2` paths, ScratchBird `4/4` paths.
- (c) Parser to SB AST / V3 Dialect: donor `1/1` paths, ScratchBird `5/5` paths.
- (d) Wire Protocol: donor `2/2` paths, ScratchBird `2/2` paths.
- (e) Authentication: donor `2/2` paths, ScratchBird `4/4` paths.
- (f) Client Bridge / UDR Target Surface: donor `2/2` paths, ScratchBird `2/2` paths.
- (g) Plan Layout / Optimizer Output: donor `1/1` paths, ScratchBird `3/3` paths.
- (h) Error Codes: donor `2/2` paths, ScratchBird `4/4` paths.
- (i) Page Types and Storage Optimizations: donor `2/2` paths, ScratchBird `6/6` paths.
- (j) Regression Tests and Tooling: donor `3/3` paths, ScratchBird `3/3` paths.
- (k) Catalog / System Tables / New Empty Database: donor `3/3` paths, ScratchBird `3/3` paths.

## Parser Package Contract
1. Own the full `YugabyteDB` client-facing request lifecycle for `postgresql_frontend_backend`.
2. Translate donor-visible datatypes, result metadata, errors, and plan requests into canonical ScratchBird AST, SBLR, and metrics packets.
3. Keep all donor-visible session state, prepared handles, streaming state, and async or notice surfaces inside the parser process boundary for one connection only.
4. Fail closed when the bundle is not `READY`, when the emulated database binding is unavailable, or when the request class is not admitted by the family capability set.

## Compiler UDR Contract
1. `"compiler_yugabytedb"` is the only engine-side entrypoint allowed to translate donor-generated text or helper payloads for `YugabyteDB`.
2. It must reuse the same capability, AST, datatype, and function admission rules as the parser package.
3. It must verify privilege class and request shape before emitting AST or SBLR.
4. It must never render donor protocol frames or donor-visible text directly to a client.

## Engine Generator And Emulation UDR Contract
1. `"engine_yugabytedb"` owns the family-specific engine-side emulation support.
2. It must bootstrap and validate: PostgreSQL-compatible catalogs plus YugabyteDB metadata and distributed-system overlays.
3. It must ship the internal donor client required for: internal PostgreSQL-compatible client used for YSQL migration and bridge UDR operations.
4. It must provide migration, bridge, catalog-bootstrap, empty-database-default, and validation routines for this family.

## Donor-Specific Beta 2 Deltas
- Parser reuses PostgreSQL wire foundations for the SQL-facing YugabyteDB family while preserving donor-specific syntax and metadata.
- Compiler UDR must preserve Yugabyte helper SQL, distributed metadata probes, and explain variants.
- Engine UDR must publish YugabyteDB-visible catalogs and system metadata overlays inside the emulated root.

## Regression And Bridge Requirements
- regression baseline: PostgreSQL-wire fixtures plus Yugabyte SQL and admin compatibility suites.
- internal bridge requirement: internal PostgreSQL-compatible client used for YSQL migration and bridge UDR operations.
- family plan renderer must convert canonical `RuntimePlan` output into donor-visible explain or profile formats for `YugabyteDB`.
- family error map pack must cover every donor error code admitted by the local packet and official donor docs.

## Current Evidence Gaps To Preserve During Implementation
- comparison deepening required: `indexes` -> No source-backed 1:1 index rows emitted in this packet
- current ScratchBird implementation gap: `wire_protocol` -> No dedicated ScratchBird protocol adapter file detected
- current ScratchBird implementation gap: `parser_ast` -> No dedicated ScratchBird external parser agent detected
- current ScratchBird implementation gap: `client_bridge` -> No dedicated ScratchBird FDW adapter detected

## Sample Bundle Registration
```cpp
static const scratchbird::core::EmulationPackageScaffold kYugabytedbScaffold = {
    "yugabytedb",
    "sb_listener_yugabytedb",
    "sb_parser_yugabytedb",
    "sb_pkg_yugabytedb_parser",
    "sb_pkg_yugabytedb_compiler_udr",
    "sb_pkg_yugabytedb_emulation_udr",
    "sb_emulation_bundle_yugabytedb/v2",
    true,
    true,
    false,
    false,
};

register_emulation_entrypoint("compiler_yugabytedb", &YugabytedbCompiler::invoke);
register_emulation_entrypoint("engine_yugabytedb", &YugabytedbEngine::invoke);
```

## Beta 2 Completion Rule
`YugabyteDB` is `READY` only after parser, compiler UDR, emulation UDR, web and local reference inputs, virtual catalog goldens, plan render goldens, donor error-map pack, and regression harness evidence are all present.

