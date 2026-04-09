# Normative Checklist: Emulated Parser Layer - Neo4j (Alpha)

## Purpose
Define the Neo4j parser layer contract for 1:1 client compatibility while translating to ScratchBird server SBLR contracts.

## Scope
- Bolt protocol compatibility.
- Neo4j parser translation and response mapping contracts.

## Required Inputs
- `NORMATIVE_EMULATED_PARSER_LAYER_BASELINE.md`
- `NORMATIVE_WIRE_PROTOCOL_NEO4J_BOLT_CHECKLIST.md`
- `NORMATIVE_PARSER_QUERY_TO_SBLR_CHECKLIST.md`

## Engine Family Binding
- Parser family id: `neo4j`
- One listener port to one `neo4j` parser family policy.

## Compatibility Contract
1. Bolt negotiation/message behavior must match `NORMATIVE_WIRE_PROTOCOL_NEO4J_BOLT_CHECKLIST.md`.
2. Parser must emit UUID-bound SBLR execution requests only.
3. Parser must fail authentication when precheck reports no open ScratchBird database.

## Neo4j Layer Checklist
### N4P00 Precheck and Auth Gate
- [ ] Run `IPC_AUTH_PRECHECK_REQ` before auth/session success.
- [ ] Map `DENY_NO_OPEN_DATABASE` to Neo4j auth-failure envelope and close.

### N4P01 Dialect-to-SBLR
- [ ] Translate Cypher/Bolt request units to canonical AST and SBLR.
- [ ] Preserve deterministic transaction and stream cursor lifecycle mapping.

### N4P02 Emulated Create Database
- [ ] Reject physical database-file creation via emulated parser path.
- [ ] Map emulated database create flows to logical schema/catalog bootstrap inside open base database.

### N4P03 Disconnect Teardown
- [ ] Close session/transaction context deterministically.
- [ ] Zeroize auth/session buffers.
- [ ] Terminate assigned parser worker process on disconnect.

Pass condition:
- Fresh parser worker is used for every new Neo4j client connection.

## Conformance Gates
- `P28-EMPL-NEO4J-01`: precheck and auth-failure behavior.
- `P28-EMPL-NEO4J-02`: 1:1 Neo4j interface and SBLR translation behavior.
- `P28-EMPL-NEO4J-03`: emulated create-database mapping and teardown lifecycle.

## Evidence Artifacts
- `docs/specifications/work/conformance/parser_layer/neo4j/PRECHECK_AND_AUTH_RESULTS.csv`
- `docs/specifications/work/conformance/parser_layer/neo4j/BOLT_TO_SBLR_RESULTS.csv`
- `docs/specifications/work/conformance/parser_layer/neo4j/CREATE_DATABASE_MAPPING_RESULTS.md`
- `docs/specifications/work/conformance/parser_layer/neo4j/WORKER_RECYCLE_RESULTS.csv`

## Audit normalization note (2026-03-28)
- This file is now treated as target-state-only or checklist-only material.
- Current section-28 source proof does not show a shipped dedicated parser implementation, parser-agent executable, and listener/runtime lane for this family.
- Nearby native-V3 feature vocabulary, catalog entries, or runtime terminology are not sufficient to promote this family into current dedicated parser parity.
