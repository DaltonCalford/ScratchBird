# Beta 2 Emulation Family Model - MySQL

## Purpose
Define the Beta 2 emulation bundle required to present ScratchBird as a donor-compatible `MySQL` endpoint while keeping donor-specific parsing, protocol, catalog, bridge, and error behavior outside the core engine.

## Family Identity
- display name: `MySQL`
- `profile_id`: `mysql`
- primary surface class: `sql_wire`
- primary donor protocol or carrier: `mysql_classic_protocol`
- shared lowering base: `mysql_family`
- listener mode: `required`
- listener executable: `sb_listener_mysql`
- parser executable: `sb_parser_mysql`
- parser package: `sb_pkg_mysql_parser`
- compiler UDR package: `sb_pkg_mysql_compiler_udr`
- emulation UDR package: `sb_pkg_mysql_emulation_udr`
- compiler entrypoint: `compiler_mysql`
- engine generator entrypoint: `engine_mysql`
- bundle contract id: `sb_emulation_bundle_mysql/v2`
- supports `MESSAGE_BLR`: `no`
- supports `EXECUTABLE_BLR`: `no`

## Admission Reason
MySQL is a shipped SQL-wire compatibility target and anchors the broader MySQL-family bundle model.

## Authoritative Reference Inputs
- local source-backed packet: `docs/reference/reference_library/emulation_1_to_1_engine_reference_packets_2026-04-02/mysql/README.md`
- official donor web supplement: `docs/reference/reference_library/emulation_1_to_1_engine_reference_packets_2026-04-02/mysql/OFFICIAL_WEB_REFERENCE_SUPPLEMENT.md`
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
- (g) Plan Layout / Optimizer Output: donor `1/1` paths, ScratchBird `3/3` paths.
- (h) Error Codes: donor `1/1` paths, ScratchBird `4/4` paths.
- (i) Page Types and Storage Optimizations: donor `2/2` paths, ScratchBird `6/6` paths.
- (j) Regression Tests and Tooling: donor `2/2` paths, ScratchBird `3/3` paths.
- (k) Catalog / System Tables / New Empty Database: donor `2/2` paths, ScratchBird `4/4` paths.

## Parser Package Contract
1. Own the full `MySQL` client-facing request lifecycle for `mysql_classic_protocol`.
2. Translate donor-visible datatypes, result metadata, errors, and plan requests into canonical ScratchBird AST, SBLR, and metrics packets.
3. Keep all donor-visible session state, prepared handles, streaming state, and async or notice surfaces inside the parser process boundary for one connection only.
4. Fail closed when the bundle is not `READY`, when the emulated database binding is unavailable, or when the request class is not admitted by the family capability set.

## Compiler UDR Contract
1. `"compiler_mysql"` is the only engine-side entrypoint allowed to translate donor-generated text or helper payloads for `MySQL`.
2. It must reuse the same capability, AST, datatype, and function admission rules as the parser package.
3. It must verify privilege class and request shape before emitting AST or SBLR.
4. It must never render donor protocol frames or donor-visible text directly to a client.

## Engine Generator And Emulation UDR Contract
1. `"engine_mysql"` owns the family-specific engine-side emulation support.
2. It must bootstrap and validate: `mysql`, `information_schema`, `performance_schema`, and `sys` overlays filtered to the emulated root.
3. It must ship the internal donor client required for: internal libmysql-compatible client used by migration and bridge UDR operations.
4. It must provide migration, bridge, catalog-bootstrap, empty-database-default, and validation routines for this family.

## Donor-Specific Beta 2 Deltas
- Parser must own classic protocol handshake, auth plugin negotiation, prepared statements, multi-resultsets, and session state tracking.
- Compiler UDR must normalize MySQL-family dynamic SQL, helper statements, and donor-visible metadata probes to shared AST/SBLR.
- Engine UDR must publish MySQL catalog overlays and default empty-database rows without exposing out-of-branch objects.

## Regression And Bridge Requirements
- regression baseline: MySQL protocol harnesses plus `mysql-test-run.pl` compatibility suites.
- internal bridge requirement: internal libmysql-compatible client used by migration and bridge UDR operations.
- family plan renderer must convert canonical `RuntimePlan` output into donor-visible explain or profile formats for `MySQL`.
- family error map pack must cover every donor error code admitted by the local packet and official donor docs.

## Current Evidence Gaps To Preserve During Implementation
- no packet-level gaps are currently recorded for this family.

## Sample Bundle Registration
```cpp
static const scratchbird::core::EmulationPackageScaffold kMysqlScaffold = {
    "mysql",
    "sb_listener_mysql",
    "sb_parser_mysql",
    "sb_pkg_mysql_parser",
    "sb_pkg_mysql_compiler_udr",
    "sb_pkg_mysql_emulation_udr",
    "sb_emulation_bundle_mysql/v2",
    true,
    true,
    false,
    false,
};

register_emulation_entrypoint("compiler_mysql", &MysqlCompiler::invoke);
register_emulation_entrypoint("engine_mysql", &MysqlEngine::invoke);
```

## Beta 2 Completion Rule
`MySQL` is `READY` only after parser, compiler UDR, emulation UDR, web and local reference inputs, virtual catalog goldens, plan render goldens, donor error-map pack, and regression harness evidence are all present.

