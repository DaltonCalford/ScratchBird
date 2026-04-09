# Normative Checklist: Emulated Parser Layer - Cassandra (Alpha)

## Purpose
Define the Cassandra parser layer contract for 1:1 client compatibility while translating to ScratchBird server SBLR contracts.

## Scope
- CQL binary protocol handshake/auth/query/prepare/execute compatibility.
- Cassandra parser translation and response mapping contracts.

## Required Inputs
- `NORMATIVE_EMULATED_PARSER_LAYER_BASELINE.md`
- `NORMATIVE_WIRE_PROTOCOL_CASSANDRA_CHECKLIST.md`
- `NORMATIVE_PARSER_QUERY_TO_SBLR_CHECKLIST.md`

## Engine Family Binding
- Parser family id: `cassandra`
- One listener port to one `cassandra` parser family policy.

## Compatibility Contract
1. CQL protocol framing/opcode behavior must match `NORMATIVE_WIRE_PROTOCOL_CASSANDRA_CHECKLIST.md`.
2. Parser must emit UUID-bound SBLR execution requests only.
3. Parser must fail authentication when precheck reports no open ScratchBird database.

## Cassandra Layer Checklist
### CSP00 Precheck and Startup Gate
- [ ] Run `IPC_AUTH_PRECHECK_REQ` before startup/auth success.
- [ ] Map `DENY_NO_OPEN_DATABASE` to Cassandra auth-failure envelope and close.

### CSP01 Dialect-to-SBLR
- [ ] Translate CQL query/prepare/execute/batch operations to canonical AST and SBLR.
- [ ] Preserve deterministic prepared-id lifecycle mapping.

### CSP02 Emulated Create Database
- [ ] Reject physical database-file creation via emulated parser path.
- [ ] Map emulated keyspace/database create forms to logical schema/catalog bootstrap inside open base database.

### CSP03 Disconnect Teardown
- [ ] Close session/transaction context deterministically.
- [ ] Zeroize auth/session buffers.
- [ ] Terminate assigned parser worker process on disconnect.

Pass condition:
- Fresh parser worker is used for every new Cassandra client connection.

## Conformance Gates
- `P28-EMPL-CASSANDRA-01`: precheck and auth-failure behavior.
- `P28-EMPL-CASSANDRA-02`: 1:1 Cassandra interface and SBLR translation behavior.
- `P28-EMPL-CASSANDRA-03`: emulated create-database mapping and teardown lifecycle.

## Evidence Artifacts
- `docs/specifications/work/conformance/parser_layer/cassandra/PRECHECK_AND_STARTUP_RESULTS.csv`
- `docs/specifications/work/conformance/parser_layer/cassandra/CQL_TO_SBLR_RESULTS.csv`
- `docs/specifications/work/conformance/parser_layer/cassandra/CREATE_DATABASE_MAPPING_RESULTS.md`
- `docs/specifications/work/conformance/parser_layer/cassandra/WORKER_RECYCLE_RESULTS.csv`

## Audit normalization note (2026-03-28)
- This file is now treated as target-state-only or checklist-only material.
- Current section-28 source proof does not show a shipped dedicated parser implementation, parser-agent executable, and listener/runtime lane for this family.
- Nearby native-V3 feature vocabulary, catalog entries, or runtime terminology are not sufficient to promote this family into current dedicated parser parity.
