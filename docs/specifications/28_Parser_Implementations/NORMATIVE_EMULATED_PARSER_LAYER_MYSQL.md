# Normative Checklist: Emulated Parser Layer - MySQL (Alpha)

## Purpose
Define the MySQL parser layer contract for 1:1 client compatibility while translating to ScratchBird server SBLR contracts.

## Scope
- MySQL protocol handshake, command, and prepared-statement compatibility.
- MySQL parser translation and response mapping contracts.

## Required Inputs
- `NORMATIVE_EMULATED_PARSER_LAYER_BASELINE.md`
- `NORMATIVE_WIRE_PROTOCOL_MYSQL_CHECKLIST.md`
- `NORMATIVE_PARSER_QUERY_TO_SBLR_CHECKLIST.md`

## Engine Family Binding
- Parser family id: `mysql`
- One listener port to one `mysql` parser family policy.

## Compatibility Contract
1. Handshake/capability/command/prepared-statement flows must match `NORMATIVE_WIRE_PROTOCOL_MYSQL_CHECKLIST.md`.
2. Parser must emit UUID-bound SBLR execution requests only.
3. Parser must fail authentication when precheck reports no open ScratchBird database.

## MySQL Layer Checklist
### MYP00 Precheck and Handshake Gate
- [ ] Run `IPC_AUTH_PRECHECK_REQ` before final auth accept.
- [ ] Map `DENY_NO_OPEN_DATABASE` to MySQL auth-failure envelope and close.

### MYP01 Dialect-to-SBLR
- [ ] Translate command and prepared-statement operations to canonical AST and SBLR.
- [ ] Preserve deterministic statement-handle lifecycle mapping.

### MYP02 Emulated Create Database
- [ ] Reject physical database-file creation via emulated parser path.
- [ ] Map emulated `CREATE DATABASE` to logical schema/catalog bootstrap inside open base database.

### MYP03 Disconnect Teardown
- [ ] Close session/transaction context deterministically.
- [ ] Zeroize auth/session buffers.
- [ ] Terminate assigned parser worker process on disconnect.

Pass condition:
- Fresh parser worker is used for every new MySQL client connection.

## Conformance Gates
- `P28-EMPL-MYSQL-01`: precheck and auth-failure behavior.
- `P28-EMPL-MYSQL-02`: 1:1 MySQL interface and SBLR translation behavior.
- `P28-EMPL-MYSQL-03`: emulated create-database mapping and teardown lifecycle.

## Evidence Artifacts
- `docs/specifications/work/conformance/parser_layer/mysql/PRECHECK_AND_HANDSHAKE_RESULTS.csv`
- `docs/specifications/work/conformance/parser_layer/mysql/COMMAND_TO_SBLR_RESULTS.csv`
- `docs/specifications/work/conformance/parser_layer/mysql/CREATE_DATABASE_MAPPING_RESULTS.md`
- `docs/specifications/work/conformance/parser_layer/mysql/WORKER_RECYCLE_RESULTS.csv`

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
