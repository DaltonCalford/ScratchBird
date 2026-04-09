# Beta 2 Emulation Family Model - Microsoft SQL Server / Azure SQL

## Purpose
Define the Beta 2 emulation bundle required to present ScratchBird as a donor-compatible `Microsoft SQL Server / Azure SQL` endpoint while keeping donor-specific parsing, protocol, catalog, bridge, and error behavior outside the core engine.

## Family Identity
- display name: `Microsoft SQL Server / Azure SQL`
- `profile_id`: `sqlserver`
- primary surface class: `sql_wire`
- primary donor protocol or carrier: `tds`
- shared lowering base: `sqlserver_family`
- listener mode: `required`
- listener executable: `sb_listener_sqlserver`
- parser executable: `sb_parser_sqlserver`
- parser package: `sb_pkg_sqlserver_parser`
- compiler UDR package: `sb_pkg_sqlserver_compiler_udr`
- emulation UDR package: `sb_pkg_sqlserver_emulation_udr`
- compiler entrypoint: `compiler_sqlserver`
- engine generator entrypoint: `engine_sqlserver`
- bundle contract id: `sb_emulation_bundle_sqlserver/v2`
- supports `MESSAGE_BLR`: `no`
- supports `EXECUTABLE_BLR`: `no`

## Admission Reason
`SQL Server / Azure SQL` is promoted into Beta 2 commercial-family scope because the local reference library now contains the official packet-level `MS-TDS` specification, which is strong enough to drive a native ScratchBird-grade protocol adapter without third-party donor drivers.

## Authoritative Reference Inputs
- commercial readiness packet root: `docs/reference/reference_library/commercial_protocol_readiness_2026-04-03/`
- readiness report: `docs/reference/reference_library/commercial_protocol_readiness_2026-04-03/COMMERCIAL_PROTOCOL_READINESS_REPORT.md`
- readiness matrix: `docs/reference/reference_library/commercial_protocol_readiness_2026-04-03/COMMERCIAL_PROTOCOL_READINESS_MATRIX.csv`
- official commercial web-source index: `docs/reference/workspace_library/technical_specs/COMMERCIAL_PROTOCOL_READINESS_WEB_SOURCES_20260403.md`
- official protocol capture: `docs/reference/workspace_library/technical_specs/sqlserver/ms-tds_protocol_20260403.html`
- commercial ranking capture: `docs/reference/workspace_library/technical_specs/commercial_relational_db_engines_ranking_apr_2026.html`
- error-code packet root: `docs/reference/reference_library/error_code_reference_packets_2026-04-02/README.md`
- packet manifest state: protocol adapter `no`, parser agent `no`, bridge adapter `no`, catalog overlay `no`, compatibility suite `no`
- commercial protocol verdict: `public_packet_spec_available_not_yet_packetized`

## Current Reference Coverage Snapshot
- public packet-level protocol standard: `yes`
- auth and TLS negotiation authority: `yes`
- SQL batch, RPC, result, and transaction-manager authority: `yes`
- bulk and special transport authority: `yes`
- Azure-family protocol sharing evidence: `yes`
- Azure-specific family supplement inside the emulation packet root: `no`
- family-local 1:1 emulation packet under the emulation packet root: `no`
- family-local official web supplement under the emulation packet root: `no`
- family-local error-map pack and catalog bootstrap goldens: `no`
- dedicated ScratchBird protocol adapter, parser worker, and bridge client: `no`

## Parser Package Contract
1. Own the full `Microsoft SQL Server / Azure SQL` client-facing request lifecycle for `tds`.
2. Translate donor-visible batch SQL, RPC requests, cursor state, transaction-manager requests, datatypes, result metadata, errors, and plan requests into canonical ScratchBird AST, SBLR, and metrics packets.
3. Keep all donor-visible session state, prepared handles, cursor or concurrent-request state, result-token sequencing, and attention or cancel state inside the parser process boundary for one connection only.
4. Fail closed when the bundle is not `READY`, when the emulated database binding is unavailable, or when the request class is not admitted by the family capability set.

## Compiler UDR Contract
1. `"compiler_sqlserver"` is the only engine-side entrypoint allowed to translate donor-generated text or helper payloads for `Microsoft SQL Server / Azure SQL`.
2. It must reuse the same capability, AST, datatype, and function admission rules as the parser package.
3. It must verify privilege class, request shape, and profile-gated T-SQL features before emitting AST or SBLR.
4. It must never render donor protocol frames or donor-visible text directly to a client.

## Engine Generator And Emulation UDR Contract
1. `"engine_sqlserver"` owns the family-specific engine-side emulation support.
2. It must bootstrap and validate: `sys`, `INFORMATION_SCHEMA`, and profile-gated DMV-visible overlays filtered to the bound database root.
3. It must ship the internal donor client required for: internal `TDS` client used by migration, validation, passthrough, and bridge UDR flows.
4. It must provide migration, bridge, catalog-bootstrap, empty-database-default, and validation routines for this family.

## Donor-Specific Beta 2 Deltas
- Parser must own `PRELOGIN`, encryption negotiation, login, batch SQL, RPC, tabular results, attention, transaction-manager requests, and bulk-load framing.
- Compiler UDR must lower T-SQL helper SQL, profile-gated `SET` or metadata probes, stored-procedure helper text, and system-object discovery calls through shared AST or SBLR structures.
- Engine UDR must publish `sys`, `INFORMATION_SCHEMA`, and SQL Server-family metadata overlays, empty-database bootstrap rows, and stable donor-visible identity mappings required by management tools and drivers.

## Regression And Bridge Requirements
- regression baseline: TDS protocol fixtures, `sqlcmd`/driver compatibility suites, bulk-copy and RPC lifecycle tests, and new-empty-database catalog goldens.
- internal bridge requirement: internal `TDS` client used by migration, validation, passthrough, and bridge UDR flows.
- family plan renderer must convert canonical `RuntimePlan` output into donor-visible explain, showplan, or profile formats for `Microsoft SQL Server / Azure SQL`.
- family error map pack must cover every donor error code admitted by the family-local packet set and captured official protocol references.

## Current Evidence Gaps To Preserve During Implementation
- family-local 1:1 emulation packet and official web supplement are not yet generated from the captured `MS-TDS` corpus.
- Azure-specific authentication and capability deltas are not yet normalized into a family-local supplement.
- family-local datatype, index, function, catalog, plan, and error packets are not yet normalized under the emulation packet root.
- current ScratchBird implementation gap: `wire_protocol` -> no dedicated ScratchBird `TDS` protocol adapter file detected.
- current ScratchBird implementation gap: `parser_ast` -> no dedicated ScratchBird external parser agent detected for `SQL Server / Azure SQL`.
- current ScratchBird implementation gap: `client_bridge` -> no dedicated ScratchBird internal `TDS` bridge client detected.

## Sample Bundle Registration
```cpp
static const scratchbird::core::EmulationPackageScaffold kSqlServerScaffold = {
    "sqlserver",
    "sb_listener_sqlserver",
    "sb_parser_sqlserver",
    "sb_pkg_sqlserver_parser",
    "sb_pkg_sqlserver_compiler_udr",
    "sb_pkg_sqlserver_emulation_udr",
    "sb_emulation_bundle_sqlserver/v2",
    true,
    true,
    false,
    false,
};

register_emulation_entrypoint("compiler_sqlserver", &SqlServerCompiler::invoke);
register_emulation_entrypoint("engine_sqlserver", &SqlServerEngine::invoke);
```

## Beta 2 Completion Rule
`Microsoft SQL Server / Azure SQL` is `READY` only after parser, compiler UDR, emulation UDR, family-local emulation packet, official supplement, virtual catalog goldens, plan render goldens, donor error-map pack, and TDS regression harness evidence are all present.
