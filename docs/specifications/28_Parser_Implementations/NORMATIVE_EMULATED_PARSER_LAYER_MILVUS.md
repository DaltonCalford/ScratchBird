# Normative Checklist: Emulated Parser Layer - Milvus (Alpha)

## Purpose
Define the Milvus parser layer contract for 1:1 client compatibility while translating to ScratchBird server SBLR contracts.

## Scope
- Milvus gRPC surface compatibility.
- Milvus parser translation and response mapping contracts.

## Required Inputs
- `NORMATIVE_EMULATED_PARSER_LAYER_BASELINE.md`
- `NORMATIVE_WIRE_PROTOCOL_MILVUS_GRPC_CHECKLIST.md`
- `NORMATIVE_PARSER_QUERY_TO_SBLR_CHECKLIST.md`

## Engine Family Binding
- Parser family id: `milvus`
- One listener port to one `milvus` parser family policy.

## Compatibility Contract
1. Milvus gRPC method/routing behavior must match `NORMATIVE_WIRE_PROTOCOL_MILVUS_GRPC_CHECKLIST.md`.
2. Parser must emit UUID-bound SBLR execution requests only.
3. Parser must fail authentication when precheck reports no open ScratchBird database.

## Milvus Layer Checklist
### MLP00 Precheck and Auth Gate
- [ ] Run `IPC_AUTH_PRECHECK_REQ` before auth/session success.
- [ ] Map `DENY_NO_OPEN_DATABASE` to Milvus auth-failure envelope and close.

### MLP01 Dialect-to-SBLR
- [ ] Translate Milvus API requests to canonical AST and SBLR.
- [ ] Preserve deterministic request-id and streaming response mapping.

### MLP02 Emulated Create Database
- [ ] Reject physical database-file creation via emulated parser path.
- [ ] Map emulated database create flows to logical schema/catalog bootstrap inside open base database.

### MLP03 Disconnect Teardown
- [ ] Close session/transaction context deterministically.
- [ ] Zeroize auth/session buffers.
- [ ] Terminate assigned parser worker process on disconnect.

Pass condition:
- Fresh parser worker is used for every new Milvus client connection.

## Conformance Gates
- `P28-EMPL-MILVUS-01`: precheck and auth-failure behavior.
- `P28-EMPL-MILVUS-02`: 1:1 Milvus interface and SBLR translation behavior.
- `P28-EMPL-MILVUS-03`: emulated create-database mapping and teardown lifecycle.

## Evidence Artifacts
- `docs/specifications/work/conformance/parser_layer/milvus/PRECHECK_AND_AUTH_RESULTS.csv`
- `docs/specifications/work/conformance/parser_layer/milvus/REQUEST_TO_SBLR_RESULTS.csv`
- `docs/specifications/work/conformance/parser_layer/milvus/CREATE_DATABASE_MAPPING_RESULTS.md`
- `docs/specifications/work/conformance/parser_layer/milvus/WORKER_RECYCLE_RESULTS.csv`

## Audit normalization note (2026-03-28)
- This file is now treated as target-state-only or checklist-only material.
- Current section-28 source proof does not show a shipped dedicated parser implementation, parser-agent executable, and listener/runtime lane for this family.
- Nearby native-V3 feature vocabulary, catalog entries, or runtime terminology are not sufficient to promote this family into current dedicated parser parity.
