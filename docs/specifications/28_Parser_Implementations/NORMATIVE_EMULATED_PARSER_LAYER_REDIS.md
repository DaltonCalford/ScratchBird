# Normative Checklist: Emulated Parser Layer - Redis (Alpha)

## Purpose
Define the Redis parser layer contract for 1:1 client compatibility while translating to ScratchBird server SBLR contracts.

## Scope
- RESP2/RESP3 protocol compatibility.
- Redis parser translation and response mapping contracts.

## Required Inputs
- `NORMATIVE_EMULATED_PARSER_LAYER_BASELINE.md`
- `NORMATIVE_WIRE_PROTOCOL_REDIS_RESP_CHECKLIST.md`
- `NORMATIVE_PARSER_QUERY_TO_SBLR_CHECKLIST.md`

## Engine Family Binding
- Parser family id: `redis`
- One listener port to one `redis` parser family policy.

## Compatibility Contract
1. RESP framing/command behavior must match `NORMATIVE_WIRE_PROTOCOL_REDIS_RESP_CHECKLIST.md`.
2. Parser must emit UUID-bound SBLR execution requests only.
3. Parser must fail authentication when precheck reports no open ScratchBird database.

## Redis Layer Checklist
### RDP00 Precheck and Auth Gate
- [ ] Run `IPC_AUTH_PRECHECK_REQ` before `AUTH`/`HELLO` success.
- [ ] Map `DENY_NO_OPEN_DATABASE` to Redis auth-failure envelope and close.

### RDP01 Dialect-to-SBLR
- [ ] Translate Redis command units to canonical AST and SBLR.
- [ ] Preserve deterministic pipeline and transaction command mapping.

### RDP02 Emulated Create Database
- [ ] Reject physical database-file creation via emulated parser path.
- [ ] Map emulated database create flows to logical schema/catalog bootstrap inside open base database.

### RDP03 Disconnect Teardown
- [ ] Close session/transaction context deterministically.
- [ ] Zeroize auth/session buffers.
- [ ] Terminate assigned parser worker process on disconnect.

Pass condition:
- Fresh parser worker is used for every new Redis client connection.

## Conformance Gates
- `P28-EMPL-REDIS-01`: precheck and auth-failure behavior.
- `P28-EMPL-REDIS-02`: 1:1 Redis interface and SBLR translation behavior.
- `P28-EMPL-REDIS-03`: emulated create-database mapping and teardown lifecycle.

## Evidence Artifacts
- `docs/specifications/work/conformance/parser_layer/redis/PRECHECK_AND_AUTH_RESULTS.csv`
- `docs/specifications/work/conformance/parser_layer/redis/COMMAND_TO_SBLR_RESULTS.csv`
- `docs/specifications/work/conformance/parser_layer/redis/CREATE_DATABASE_MAPPING_RESULTS.md`
- `docs/specifications/work/conformance/parser_layer/redis/WORKER_RECYCLE_RESULTS.csv`

## Audit normalization note (2026-03-28)
- This file is now treated as target-state-only or checklist-only material.
- Current section-28 source proof does not show a shipped dedicated parser implementation, parser-agent executable, and listener/runtime lane for this family.
- Nearby native-V3 feature vocabulary, catalog entries, or runtime terminology are not sufficient to promote this family into current dedicated parser parity.
