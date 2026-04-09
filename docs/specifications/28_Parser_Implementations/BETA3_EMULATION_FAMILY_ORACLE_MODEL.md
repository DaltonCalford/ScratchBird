# Beta 3 Emulation Family Model - Oracle Database

## Purpose
Define the Beta 3 deferred emulation bundle required to present ScratchBird as a donor-compatible `Oracle Database` endpoint while preserving parser-boundary invariants and explicitly recording the remaining public `TTC` grammar gaps that keep the family out of Beta 2.

## Family Identity
- display name: `Oracle Database`
- `profile_id`: `oracle`
- primary surface class: `sql_wire`
- primary donor protocol or carrier: `oracle_net_ttc`
- shared lowering base: `oracle_family`
- listener mode: `required`
- listener executable: `sb_listener_oracle`
- parser executable: `sb_parser_oracle`
- parser package: `sb_pkg_oracle_parser`
- compiler UDR package: `sb_pkg_oracle_compiler_udr`
- emulation UDR package: `sb_pkg_oracle_emulation_udr`
- compiler entrypoint: `compiler_oracle`
- engine generator entrypoint: `engine_oracle`
- bundle contract id: `sb_emulation_bundle_oracle/v3`
- supports `MESSAGE_BLR`: `no`
- supports `EXECUTABLE_BLR`: `no`

## Admission Reason
`Oracle Database` remains a high-priority commercial target, but the current evidence base is still weaker than `MS-TDS` and `DRDA`: official public material now proves Oracle Net packet formats, `TTC` examples, auth and crypto negotiation, and thin-client stack structure, yet a standalone exhaustive `TTC` command and field grammar is still not closed. That keeps Oracle in Beta 3 planning or implementation status instead of Beta 2.

## Authoritative Reference Inputs
- commercial readiness packet root: `docs/reference/reference_library/commercial_protocol_readiness_2026-04-03/`
- readiness report: `docs/reference/reference_library/commercial_protocol_readiness_2026-04-03/COMMERCIAL_PROTOCOL_READINESS_REPORT.md`
- focused Oracle/Db2 discovery report: `docs/reference/reference_library/commercial_protocol_readiness_2026-04-03/ORACLE_DB2_PROTOCOL_SOURCE_DISCOVERY.md`
- open-source thin-driver mining report: `docs/reference/reference_library/commercial_protocol_readiness_2026-04-03/ORACLE_TTC_OPEN_SOURCE_DRIVER_MINING_REPORT.md`
- official commercial web-source index: `docs/reference/workspace_library/technical_specs/COMMERCIAL_PROTOCOL_READINESS_WEB_SOURCES_20260403.md`
- official Oracle captures:
  - `docs/reference/workspace_library/technical_specs/oracle/jdbc-thin-features_12_2_20260403.html`
  - `docs/reference/workspace_library/technical_specs/oracle/configuring-thin-jdbc-client-network_12_2_20260403.html`
  - `docs/reference/workspace_library/technical_specs/oracle/troubleshooting-oracle-net-services_12_2_20260403.html`
  - `docs/reference/workspace_library/technical_specs/oracle/database-net-services-administrators-guide_21_20260403.pdf`
  - `docs/reference/workspace_library/technical_specs/oracle/architecture-of-oracle-net-services_11g_20260403.html`
- open-source corroboration source index: `docs/reference/workspace_library/third_party_implementations/ORACLE_TTC_OPEN_SOURCE_DRIVER_SOURCES_20260403.md`
- error-code packet root: `docs/reference/reference_library/error_code_reference_packets_2026-04-02/README.md`
- packet manifest state: protocol adapter `no`, parser agent `no`, bridge adapter `no`, catalog overlay `no`, compatibility suite `no`
- commercial protocol verdict: `official_packet_formats_and_ttc_examples_available_but_full_ttc_grammar_not_found`

## Current Reference Coverage Snapshot
- Oracle Net packet-format authority: `yes`
- trace-decoding and `TTC` example authority: `yes`
- auth, encryption, and negotiation authority: `yes`
- open-source thin-driver corroboration: `yes`
- standalone exhaustive public `TTC` command and field grammar: `no`
- family-local 1:1 emulation packet under the emulation packet root: `no`
- family-local official web supplement under the emulation packet root: `no`
- family-local wire checklist admitted for Beta 2 implementation: `no`
- dedicated ScratchBird protocol adapter, parser worker, and bridge client: `no`

## Parser Package Contract
1. Own the full `Oracle Database` client-facing request lifecycle for `oracle_net_ttc`.
2. Translate donor-visible cursor state, datatypes, result metadata, errors, and plan requests into canonical ScratchBird AST, SBLR, and metrics packets.
3. Keep all donor-visible session state, auth state, cursor state, piecewise payload state, and notification state inside the parser process boundary for one connection only.
4. Fail closed when the bundle is not `READY`, when the emulated database binding is unavailable, or when the request class is not admitted by the family capability set.

## Compiler UDR Contract
1. `"compiler_oracle"` is the only engine-side entrypoint allowed to translate donor-generated text or helper payloads for `Oracle Database`.
2. It must reuse the same capability, AST, datatype, and function admission rules as the parser package.
3. It must verify privilege class, request shape, and Oracle-family feature admission before emitting AST or SBLR.
4. It must never render donor protocol frames or donor-visible text directly to a client.

## Engine Generator And Emulation UDR Contract
1. `"engine_oracle"` owns the family-specific engine-side emulation support.
2. It must bootstrap and validate: `SYS`, `ALL_*`, `USER_*`, `DBA_*`, and profile-gated `V$` or `GV$` overlays filtered to the bound database root.
3. It must ship the internal donor client required for: internal Oracle-compatible direct client used by migration, validation, passthrough, and bridge UDR flows.
4. It must provide migration, bridge, catalog-bootstrap, empty-database-default, and validation routines for this family.

## Donor-Specific Beta 3 Deltas
- Parser must eventually own Oracle Net negotiation, `TTC` connect and auth exchange, cursor lifecycle, execute and fetch message families, profile-gated piecewise LOB transport, and any admitted advanced notification or AQ flows.
- Compiler UDR must lower Oracle-generated metadata probes, dynamic SQL, PL or SQL helper statements, and dictionary queries through shared AST or SBLR structures.
- Engine UDR must publish Oracle dictionary overlays, stable donor-visible identity mappings, empty-database bootstrap rows, and a bridge client that does not depend on external Oracle client libraries.

## Regression And Bridge Requirements
- regression baseline: SQL*Plus and thin-client login fixtures, cursor lifecycle goldens, piecewise LOB flow tests, and new-empty-database catalog goldens.
- internal bridge requirement: internal Oracle-compatible direct client used by migration, validation, passthrough, and bridge UDR flows.
- family plan renderer must convert canonical `RuntimePlan` output into donor-visible explain or plan display formats for `Oracle Database`.
- family error map pack must cover every donor error code admitted by the future family-local packet set and captured Oracle protocol references.

## Current Evidence Gaps To Preserve During Implementation
- no standalone exhaustive public `TTC` command and field grammar has been found in the official Oracle material currently captured.
- family-local 1:1 emulation packet and official web supplement are not yet generated from the current Oracle protocol corpus.
- family-local datatype, index, function, catalog, plan, and error packets are not yet normalized under the emulation packet root.
- current ScratchBird implementation gap: `wire_protocol` -> no dedicated ScratchBird `Oracle Net/TTC` protocol adapter file detected.
- current ScratchBird implementation gap: `parser_ast` -> no dedicated ScratchBird external parser agent detected for `Oracle`.
- current ScratchBird implementation gap: `client_bridge` -> no dedicated ScratchBird internal Oracle-compatible bridge client detected.

## Sample Bundle Registration
```cpp
static const scratchbird::core::EmulationPackageScaffold kOracleScaffold = {
    "oracle",
    "sb_listener_oracle",
    "sb_parser_oracle",
    "sb_pkg_oracle_parser",
    "sb_pkg_oracle_compiler_udr",
    "sb_pkg_oracle_emulation_udr",
    "sb_emulation_bundle_oracle/v3",
    true,
    true,
    false,
    false,
};

register_emulation_entrypoint("compiler_oracle", &OracleCompiler::invoke);
register_emulation_entrypoint("engine_oracle", &OracleEngine::invoke);
```

## Beta 3 Promotion Rule
`Oracle Database` remains `Beta 3` until all of the following are true:
1. a family-local 1:1 emulation packet and official supplement exist under the emulation packet root,
2. a normalized `TTC` message-family and field inventory exists with authoritative source mapping,
3. a dedicated section-28 Oracle wire checklist is admitted,
4. parser, compiler UDR, and emulation UDR readiness can be verified without relying on external donor client libraries.
