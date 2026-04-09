# Beta 2 Emulation Family Model - IBM Db2

## Purpose
Define the Beta 2 emulation bundle required to present ScratchBird as a donor-compatible `IBM Db2` endpoint while keeping donor-specific parsing, protocol, catalog, bridge, and error behavior outside the core engine.

## Family Identity
- display name: `IBM Db2`
- `profile_id`: `db2`
- primary surface class: `sql_wire`
- primary donor protocol or carrier: `drda_ddm`
- shared lowering base: `db2_family`
- listener mode: `required`
- listener executable: `sb_listener_db2`
- parser executable: `sb_parser_db2`
- parser package: `sb_pkg_db2_parser`
- compiler UDR package: `sb_pkg_db2_compiler_udr`
- emulation UDR package: `sb_pkg_db2_emulation_udr`
- compiler entrypoint: `compiler_db2`
- engine generator entrypoint: `engine_db2`
- bundle contract id: `sb_emulation_bundle_db2/v2`
- supports `MESSAGE_BLR`: `no`
- supports `EXECUTABLE_BLR`: `no`

## Admission Reason
Db2 is promoted into Beta 2 commercial-family scope because the local reference library now contains the formal `DRDA Version 4` protocol-standard volumes plus IBM requester/server and trace material strong enough to design a native ScratchBird-grade front door without third-party donor drivers.

## Authoritative Reference Inputs
- commercial readiness packet root: `docs/reference/reference_library/commercial_protocol_readiness_2026-04-03/`
- readiness report: `docs/reference/reference_library/commercial_protocol_readiness_2026-04-03/COMMERCIAL_PROTOCOL_READINESS_REPORT.md`
- readiness matrix: `docs/reference/reference_library/commercial_protocol_readiness_2026-04-03/COMMERCIAL_PROTOCOL_READINESS_MATRIX.csv`
- focused protocol discovery: `docs/reference/reference_library/commercial_protocol_readiness_2026-04-03/ORACLE_DB2_PROTOCOL_SOURCE_DISCOVERY.md`
- official commercial web-source index: `docs/reference/workspace_library/technical_specs/COMMERCIAL_PROTOCOL_READINESS_WEB_SOURCES_20260403.md`
- formal protocol volumes:
  - `docs/reference/workspace_library/technical_specs/db2/drda_v4_vol1_20260403.pdf`
  - `docs/reference/workspace_library/technical_specs/db2/drda_v4_vol2_20260403.pdf`
  - `docs/reference/workspace_library/technical_specs/db2/drda_v4_vol3_20260403.pdf`
- official requester/server and trace references:
  - `docs/reference/workspace_library/technical_specs/db2/drda_overview_db2_i_74_20260403.html`
  - `docs/reference/workspace_library/technical_specs/db2/remote-operation-through-drda_20260403.html`
  - `docs/reference/workspace_library/technical_specs/db2/drda-trace-output-file-samples_db2_11_5_20260403.html`
  - `docs/reference/workspace_library/technical_specs/db2/db2-connect-trace_vr6_20260403.html`
- error-code packet root: `docs/reference/reference_library/error_code_reference_packets_2026-04-02/README.md`
- packet manifest state: protocol adapter `no`, parser agent `no`, bridge adapter `no`, catalog overlay `no`, compatibility suite `no`
- commercial protocol verdict: `public_packet_standard_available_not_yet_packetized`

## Current Reference Coverage Snapshot
- public packet-level protocol standard: `yes`
- manager negotiation and security-flow authority: `yes`
- query, package, and result-path authority: `yes`
- trace-backed code-point and startup evidence: `yes`
- bulk or special transport evidence: `partial`
- family-local 1:1 emulation packet under the emulation packet root: `no`
- family-local official web supplement under the emulation packet root: `no`
- family-local error-map pack and catalog bootstrap goldens: `no`
- dedicated ScratchBird protocol adapter, parser worker, and bridge client: `no`

## Parser Package Contract
1. Own the full `IBM Db2` client-facing request lifecycle for `drda_ddm`.
2. Translate donor-visible package state, section handles, cursor rules, datatypes, result metadata, errors, and plan requests into canonical ScratchBird AST, SBLR, and metrics packets.
3. Keep all donor-visible session state, package and section state, prepared handles, block-fetch state, and async surfaces inside the parser process boundary for one connection only.
4. Fail closed when the bundle is not `READY`, when the emulated database binding is unavailable, or when the request class is not admitted by the family capability set.

## Compiler UDR Contract
1. `"compiler_db2"` is the only engine-side entrypoint allowed to translate donor-generated text or helper payloads for `IBM Db2`.
2. It must reuse the same capability, AST, datatype, and function admission rules as the parser package.
3. It must verify privilege class, package or section requirements, and request shape before emitting AST or SBLR.
4. It must never render donor protocol frames or donor-visible text directly to a client.

## Engine Generator And Emulation UDR Contract
1. `"engine_db2"` owns the family-specific engine-side emulation support.
2. It must bootstrap and validate: `SYSIBM`, `SYSCAT`, `SYSSTAT`, and Db2-visible special-register and package overlays filtered to the bound database root.
3. It must ship the internal donor client required for: internal DRDA requester used by migration, validation, passthrough, and bridge UDR flows.
4. It must provide migration, bridge, catalog-bootstrap, empty-database-default, and validation routines for this family.

## Donor-Specific Beta 2 Deltas
- Parser must implement `EXCSAT`, `ACCSEC`, `SECCHK`, `ACCRDB`, package and section lifecycle, cursor block retrieval, and DRDA reply-message sequencing.
- Compiler UDR must lower Db2-generated dynamic SQL, package-preparation helpers, special-register probes, and CLP-style metadata statements into shared AST or SBLR structures.
- Engine UDR must publish Db2 dictionary overlays, package-visible identity mappings, empty-database bootstrap rows, and bridge routines that do not depend on external Db2 client libraries.

## Regression And Bridge Requirements
- regression baseline: DRDA flow fixtures, Db2 CLI/CLP compatibility suites, package and section lifecycle goldens, and new-empty-database catalog goldens.
- internal bridge requirement: internal DRDA requester used by migration, validation, passthrough, and bridge UDR flows.
- family plan renderer must convert canonical `RuntimePlan` output into donor-visible explain or plan-text formats for `IBM Db2`.
- family error map pack must cover every donor error code admitted by the family-local packet set and captured official protocol references.

## Current Evidence Gaps To Preserve During Implementation
- family-local 1:1 emulation packet and official web supplement are not yet generated from the captured `DRDA` corpus.
- family-local datatype, index, function, catalog, plan, and error packets are not yet normalized under the emulation packet root.
- current ScratchBird implementation gap: `wire_protocol` -> no dedicated ScratchBird `DRDA/DDM` protocol adapter file detected.
- current ScratchBird implementation gap: `parser_ast` -> no dedicated ScratchBird external parser agent detected for `Db2`.
- current ScratchBird implementation gap: `client_bridge` -> no dedicated ScratchBird internal `DRDA` bridge client detected.

## Sample Bundle Registration
```cpp
static const scratchbird::core::EmulationPackageScaffold kDb2Scaffold = {
    "db2",
    "sb_listener_db2",
    "sb_parser_db2",
    "sb_pkg_db2_parser",
    "sb_pkg_db2_compiler_udr",
    "sb_pkg_db2_emulation_udr",
    "sb_emulation_bundle_db2/v2",
    true,
    true,
    false,
    false,
};

register_emulation_entrypoint("compiler_db2", &Db2Compiler::invoke);
register_emulation_entrypoint("engine_db2", &Db2Engine::invoke);
```

## Beta 2 Completion Rule
`IBM Db2` is `READY` only after parser, compiler UDR, emulation UDR, family-local emulation packet, official supplement, virtual catalog goldens, plan render goldens, donor error-map pack, and DRDA regression harness evidence are all present.
