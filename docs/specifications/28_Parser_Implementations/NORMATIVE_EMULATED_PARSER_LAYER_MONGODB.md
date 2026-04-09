# Normative Checklist: Emulated Parser Layer - MongoDB (Alpha)

## Purpose
Define the MongoDB parser layer contract for 1:1 client compatibility while translating to ScratchBird server SBLR contracts.

## Scope
- MongoDB wire command compatibility.
- MongoDB parser translation and response mapping contracts.

## Required Inputs
- `NORMATIVE_EMULATED_PARSER_LAYER_BASELINE.md`
- `NORMATIVE_WIRE_PROTOCOL_MONGODB_CHECKLIST.md`
- `NORMATIVE_PARSER_QUERY_TO_SBLR_CHECKLIST.md`

## Engine Family Binding
- Parser family id: `mongodb`
- One listener port to one `mongodb` parser family policy.

## Compatibility Contract
1. MongoDB command framing/opcode behavior must match `NORMATIVE_WIRE_PROTOCOL_MONGODB_CHECKLIST.md`.
2. Parser must emit UUID-bound SBLR execution requests only.
3. Parser must fail authentication when precheck reports no open ScratchBird database.

## MongoDB Layer Checklist
### MGP00 Precheck and Auth Gate
- [ ] Run `IPC_AUTH_PRECHECK_REQ` before auth/session success.
- [ ] Map `DENY_NO_OPEN_DATABASE` to MongoDB auth-failure envelope and close.

### MGP01 Dialect-to-SBLR
- [ ] Translate MongoDB command documents to canonical AST and SBLR.
- [ ] Preserve deterministic command and cursor lifecycle mapping.

### MGP02 Emulated Create Database
- [ ] Reject physical database-file creation via emulated parser path.
- [ ] Map emulated database create flows to logical schema/catalog bootstrap inside open base database.

### MGP03 Disconnect Teardown
- [ ] Close session/transaction context deterministically.
- [ ] Zeroize auth/session buffers.
- [ ] Terminate assigned parser worker process on disconnect.

Pass condition:
- Fresh parser worker is used for every new MongoDB client connection.

## Conformance Gates
- `P28-EMPL-MONGODB-01`: precheck and auth-failure behavior.
- `P28-EMPL-MONGODB-02`: 1:1 MongoDB interface and SBLR translation behavior.
- `P28-EMPL-MONGODB-03`: emulated create-database mapping and teardown lifecycle.

## Evidence Artifacts
- `docs/specifications/work/conformance/parser_layer/mongodb/PRECHECK_AND_AUTH_RESULTS.csv`
- `docs/specifications/work/conformance/parser_layer/mongodb/COMMAND_TO_SBLR_RESULTS.csv`
- `docs/specifications/work/conformance/parser_layer/mongodb/CREATE_DATABASE_MAPPING_RESULTS.md`
- `docs/specifications/work/conformance/parser_layer/mongodb/WORKER_RECYCLE_RESULTS.csv`

## Audit normalization note (2026-03-28)
- This file is now treated as target-state-only or checklist-only material.
- Current section-28 source proof does not show a shipped dedicated parser implementation, parser-agent executable, and listener/runtime lane for this family.
- Nearby native-V3 feature vocabulary, catalog entries, or runtime terminology are not sufficient to promote this family into current dedicated parser parity.
