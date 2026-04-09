# Normative Checklist: Emulated Parser Layer - PostgreSQL (Alpha)

## Purpose
Define the PostgreSQL parser layer contract for 1:1 client compatibility while translating to ScratchBird server SBLR contracts.

## Scope
- PostgreSQL frontend/backend protocol compatibility.
- PostgreSQL parser translation and response mapping contracts.

## Required Inputs
- `NORMATIVE_EMULATED_PARSER_LAYER_BASELINE.md`
- `NORMATIVE_WIRE_PROTOCOL_POSTGRESQL_CHECKLIST.md`
- `NORMATIVE_PARSER_QUERY_TO_SBLR_CHECKLIST.md`

## Engine Family Binding
- Parser family id: `postgresql`
- One listener port to one `postgresql` parser family policy.

## Compatibility Contract
1. Startup/auth/simple/extended/copy flows must match `NORMATIVE_WIRE_PROTOCOL_POSTGRESQL_CHECKLIST.md`.
2. Parser must emit UUID-bound SBLR execution requests only.
3. Parser must fail authentication when precheck reports no open ScratchBird database.

## PostgreSQL Layer Checklist
### PGP00 Precheck and Startup Gate
- [ ] Run `IPC_AUTH_PRECHECK_REQ` before startup auth completion.
- [ ] Map `DENY_NO_OPEN_DATABASE` to PostgreSQL auth-failure envelope and close.

### PGP01 Dialect-to-SBLR
- [ ] Translate simple and extended query flows to canonical AST and SBLR.
- [ ] Preserve deterministic statement/portal lifecycle mapping.

### PGP02 Emulated Create Database
- [ ] Reject physical database-file creation via emulated parser path.
- [ ] Map emulated `CREATE DATABASE` to logical schema/catalog bootstrap inside open base database.

### PGP03 Disconnect Teardown
- [ ] Close session/transaction context deterministically.
- [ ] Zeroize auth/session buffers.
- [ ] Terminate assigned parser worker process on disconnect.

Pass condition:
- Fresh parser worker is used for every new PostgreSQL client connection.

## Conformance Gates
- `P28-EMPL-POSTGRESQL-01`: precheck and auth-failure behavior.
- `P28-EMPL-POSTGRESQL-02`: 1:1 PostgreSQL interface and SBLR translation behavior.
- `P28-EMPL-POSTGRESQL-03`: emulated create-database mapping and teardown lifecycle.

## Evidence Artifacts
- `docs/specifications/work/conformance/parser_layer/postgresql/PRECHECK_AND_STARTUP_RESULTS.csv`
- `docs/specifications/work/conformance/parser_layer/postgresql/QUERY_TO_SBLR_RESULTS.csv`
- `docs/specifications/work/conformance/parser_layer/postgresql/CREATE_DATABASE_MAPPING_RESULTS.md`
- `docs/specifications/work/conformance/parser_layer/postgresql/WORKER_RECYCLE_RESULTS.csv`

## Audit normalization note (2026-03-28)
- This file is now treated as a target-state checklist layered on top of a narrower current proof set.
- Current source-backed proof is limited to the shipped dedicated parser code, parser-agent executable, listener front door, and package-scaffold lane for this family.
- Full family parity, complete wire fidelity, exhaustive feature coverage, and complete package/runtime lifecycle claims remain non-authoritative unless separately proven by the live source tree.

## Hardening promotion note (2026-03-28)
- section `28` now carries explicit capability-state vocabulary for parser implementation proof lanes:
  - `supported_native_v3`
  - `supported_emulated_sql_family`
  - `supported_scaffold_or_udr_boundary`
  - `bounded_shipped_front_door`
  - `checklist_only`
  - `target_state_only`
  - `fail_closed`
- dedicated parser-family proof must be anchored to live parser code plus shipped parser-agent or listener/runtime seams, not to checklist presence alone
- native-V3 internal feature vocabulary must not be promoted into dedicated external parser-family parity without family-local source proof
- universal capability-profile generation, universal corpus cardinality, and universal wire parity claims remain non-authoritative unless backed by generated or runtime evidence
