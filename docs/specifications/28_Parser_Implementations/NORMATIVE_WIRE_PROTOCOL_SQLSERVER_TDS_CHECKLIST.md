# Normative Checklist: Microsoft SQL Server / Azure SQL TDS Wire Protocol Adapter (Beta 2 Target)

## Purpose
Define the deterministic implementation contract for `Microsoft SQL Server / Azure SQL` protocol emulation in the parser layer using `TDS`.

## Scope
- `TDS` prelogin, encryption negotiation, and login flows.
- SQL batch, RPC, transaction-manager, attention, bulk-load, and tabular-result flows.
- Parser-to-engine mapping and deterministic donor-visible result and error encoding.

## Hard Invariants
1. Engine never speaks `TDS` and never parses donor SQL directly.
2. Parser implements all `TDS` frame, token, and request or reply decode and encode behavior.
3. Parser maps donor-visible requests to canonical AST, SBLR, and UUID-bound operations only.
4. Parser profile controls `SQL Server / Azure SQL` feature exposure and reject paths.
5. Parser never executes `T-SQL` semantics locally.

## Beta 2 Wire Profile
- carrier: `TDS` request and response packets over `TCP/IP`
- family coverage:
  - `SQL Server`
  - `Azure SQL Database`
- required front-door flow includes:
  - `PRELOGIN`
  - encryption or TLS negotiation when enabled
  - login exchange
- required execution flow includes:
  - SQL batch
  - RPC request
  - transaction-manager request
  - attention or cancel
  - tabular result tokens
  - bulk-load request path

## Connection And Login Contract
1. Parser must implement `PRELOGIN` negotiation before login acceptance.
2. Parser must implement encryption or TLS mode transitions according to the active family profile and listener policy.
3. Parser must implement login success and deterministic login failure responses without exposing ScratchBird internal text.
4. Parser must initialize session state only after successful login completion.

## Required Packet And Token Families
- connection:
  - `PRELOGIN`
  - login exchange
  - authentication continuation when the negotiated auth mode requires it
- executable request classes:
  - SQL batch
  - RPC request
  - transaction-manager request
  - bulk-load request
  - attention or cancel
- result token families:
  - column metadata
  - row payload
  - `DONE`-class completion tokens
  - environment change tokens
  - informational and error tokens
  - return status and output-parameter tokens when RPC paths require them

## Request Mapping Contract
- SQL batch requests:
  - decode batch text
  - capability gate
  - UUID bind
  - emit SBLR
- RPC requests:
  - decode procedure name or numeric id plus parameter metadata
  - capability gate and canonicalize
  - emit canonical procedure or statement request envelopes
- transaction-manager requests:
  - map to canonical transactional control envelopes
- attention:
  - interrupt the active canonical execution context deterministically

## Result And Error Mapping Contract
- Parser MUST map canonical result metadata to donor-visible tabular metadata deterministically.
- Parser MUST preserve `DONE` token ordering, status flags, row-count visibility, and transaction-state reporting.
- Parser MUST map parser and engine failures to deterministic `SQL Server` or `Azure SQL` error-number and state payloads.
- Parser MUST sanitize discoverability-sensitive failures and never leak ScratchBird internal text or UUIDs.

## Implementation Checklist

### TDSW00 Packet Framing
- [ ] Implement request and response packet framing readers and writers.
- [ ] Validate packet lengths, sequence integrity, and bounded message assembly.
- [ ] Reject malformed or truncated packet streams deterministically.

Pass condition:
- packet framing is deterministic and desynchronization-safe.

### TDSW01 Prelogin And Login
- [ ] Implement `PRELOGIN` negotiation and listener-policy binding.
- [ ] Implement encryption or TLS transition rules for enabled profiles.
- [ ] Implement login success and failure flows with deterministic state transitions.

Pass condition:
- connection establishment is protocol-correct and fail-closed.

### TDSW02 SQL Batch And RPC
- [ ] Implement SQL batch request mapping.
- [ ] Implement RPC request decode, parameter binding, and result shaping.
- [ ] Persist parser-side request metadata needed for deterministic output-parameter and status rendering.

Pass condition:
- batch and RPC flows are deterministic and donor-compatible.

### TDSW03 Transaction And Attention
- [ ] Implement transaction-manager request mapping.
- [ ] Implement attention or cancel handling.
- [ ] Preserve deterministic session-state transitions on commit, rollback, and interrupt.

Pass condition:
- transaction and cancel behavior is protocol-correct and bounded.

### TDSW04 Result Tokens
- [ ] Implement column-metadata, row, and `DONE`-class token rendering.
- [ ] Implement environment change, informational, and error token rendering.
- [ ] Implement RPC return-status and output-parameter rendering when required.

Pass condition:
- tabular result token sequences are complete and deterministic.

### TDSW05 Bulk Load
- [ ] Implement profile-gated bulk-load request mapping.
- [ ] Validate metadata and row-shape compatibility before execution dispatch.
- [ ] Reject unsupported bulk shapes deterministically.

Pass condition:
- bulk-load path is protocol-correct and fail-closed.

## Negative Requirements
- Parser MUST NOT execute `T-SQL` semantics locally.
- Parser MUST NOT expose catalog data outside the bound database root.
- Parser MUST NOT depend on external `SQL Server` or `Azure SQL` client libraries to speak `TDS`.

## Conformance Gates
- `P28-TDSW-GATE-01`: packet framing and prelogin or login tests pass.
- `P28-TDSW-GATE-02`: SQL batch and RPC lifecycle tests pass.
- `P28-TDSW-GATE-03`: transaction-manager and attention tests pass.
- `P28-TDSW-GATE-04`: result-token, bulk-load, and error mapping tests pass.

## Evidence Artifacts
- `docs/specifications/work/conformance/wire/sqlserver/PACKET_AND_PRELOGIN_RESULTS.json`
- `docs/specifications/work/conformance/wire/sqlserver/LOGIN_AND_AUTH_RESULTS.csv`
- `docs/specifications/work/conformance/wire/sqlserver/BATCH_AND_RPC_RESULTS.csv`
- `docs/specifications/work/conformance/wire/sqlserver/TXN_AND_ATTENTION_RESULTS.csv`
- `docs/specifications/work/conformance/wire/sqlserver/RESULT_TOKEN_AND_ERROR_RESULTS.csv`

## Cross-Section Links
- `docs/specifications/28_Parser_Implementations/BETA2_EMULATION_FAMILY_SQLSERVER_AZURESQL_MODEL.md`
- `docs/specifications/28_Parser_Implementations/NORMATIVE_PARSER_QUERY_TO_SBLR_CHECKLIST.md`
- `docs/specifications/28_Parser_Implementations/ERROR_MAPPING_AND_DIAGNOSTICS.md`

## Audit normalization note (2026-04-03)
- This file is a target-state checklist, not stand-alone proof of shipped `SQL Server / Azure SQL` parity.
- Current source-backed authority comes from the captured public `MS-TDS` specification, not from a shipped ScratchBird `TDS` parser implementation.
- Runtime parity remains bounded until family-local parser, compiler UDR, emulation UDR, and conformance artifacts exist.
