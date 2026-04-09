# Normative Checklist: Emulated Parser Layer - Firebird (Alpha)

## Purpose
Define the Firebird parser layer contract for 1:1 client compatibility while translating to ScratchBird server SBLR contracts.

## Scope
- Firebird client `<->` Firebird parser interface behavior.
- Firebird parser `<->` ScratchBird server IPC/SBWP behavior.

## Required Inputs
- `NORMATIVE_EMULATED_PARSER_LAYER_BASELINE.md`
- `NORMATIVE_WIRE_PROTOCOL_FIREBIRD_CHECKLIST.md`
- `NORMATIVE_PARSER_QUERY_TO_SBLR_CHECKLIST.md`

## Engine Family Binding
- Parser family id: `firebird`
- One listener port to one `firebird` parser family policy.

## Compatibility Contract
1. Firebird wire operation families and status-vector behavior must match `NORMATIVE_WIRE_PROTOCOL_FIREBIRD_CHECKLIST.md`.
2. Parser must emit UUID-bound SBLR execution requests only.
3. Parser must fail authentication when precheck reports no open ScratchBird database.

## Firebird Layer Checklist
### FBP00 Precheck and Attach Gate
- [ ] Run `IPC_AUTH_PRECHECK_REQ` before `op_attach` and `op_create` acceptance.
- [ ] Map `DENY_NO_OPEN_DATABASE` to Firebird auth-failure envelope and close.

### FBP01 Dialect-to-SBLR
- [ ] Convert accepted Firebird DSQL/PSQL operations to canonical AST and SBLR.
- [ ] Bind all persistent objects by UUID before SBLR emission.

### FBP02 Emulated Create Database
- [ ] Reject physical database-file creation via emulated parser path.
- [ ] Map emulated `CREATE DATABASE` to logical schema/catalog bootstrap inside open base database.

### FBP03 Disconnect Teardown
- [ ] Close session/transaction context deterministically.
- [ ] Zeroize auth/session buffers.
- [ ] Terminate assigned parser worker process on disconnect.

Pass condition:
- Fresh parser worker is used for every new Firebird client connection.

## Conformance Gates
- `P28-EMPL-FIREBIRD-01`: precheck and auth-failure behavior.
- `P28-EMPL-FIREBIRD-02`: 1:1 Firebird interface and SBLR translation behavior.
- `P28-EMPL-FIREBIRD-03`: emulated create-database mapping and teardown lifecycle.

## Evidence Artifacts
- `docs/specifications/work/conformance/parser_layer/firebird/PRECHECK_AND_ATTACH_RESULTS.csv`
- `docs/specifications/work/conformance/parser_layer/firebird/DSQL_TO_SBLR_RESULTS.csv`
- `docs/specifications/work/conformance/parser_layer/firebird/CREATE_DATABASE_MAPPING_RESULTS.md`
- `docs/specifications/work/conformance/parser_layer/firebird/WORKER_RECYCLE_RESULTS.csv`

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
