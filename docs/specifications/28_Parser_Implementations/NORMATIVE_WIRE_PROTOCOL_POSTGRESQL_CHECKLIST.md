# Normative Checklist: PostgreSQL Wire Protocol Adapter (Alpha)

## Purpose
Define a deterministic implementation contract for PostgreSQL frontend/backend protocol emulation in the parser layer.

## Scope
- PostgreSQL startup and normal-operation message flows.
- Simple query, extended query, and copy sub-protocol handling.
- Parser-to-engine mapping and dialect-native response encoding.

## Hard Invariants
1. Engine never parses SQL and never speaks PostgreSQL wire messages.
2. Parser implements all PostgreSQL wire decode/encode behavior.
3. Parser emits UUID-based SBLR execution requests only.
4. Parser profile drives compatibility behavior; no fallback to native parser.
5. Parser must preserve message-order semantics and ReadyForQuery boundaries.

## Alpha Wire Profile
- Protocol major version: `3`
- Supported minors:
  - `3.0` compatibility baseline
  - `3.2` behavior for PostgreSQL 18.x profile
- Startup special requests:
  - `SSLRequest` code `80877103`
  - `CancelRequest` code `80877102`
  - `GSSENCRequest` code `80877104` (profile-gated)

## Frame Contract
- Startup message:
  - no type byte
  - `Int32 length`, `Int32 protocol_or_request_code`, payload fields.
- Normal messages:
  - `Byte1 type`
  - `Int32 length` (includes length field, excludes type byte)
  - typed payload.

## Required Message Families

### Startup/Auth
- Client: `StartupMessage`, `SSLRequest`, `CancelRequest`, optional `GSSENCRequest`.
- Server: `Authentication*`, `ParameterStatus`, `BackendKeyData`, `ReadyForQuery`, `ErrorResponse`.

### Simple Query
- Client: `Query`
- Server: `RowDescription`, `DataRow`, `CommandComplete`, `EmptyQueryResponse`, `ErrorResponse`, `NoticeResponse`, `ReadyForQuery`.

### Extended Query
- Client: `Parse`, `Bind`, `Describe`, `Execute`, `Close`, `Sync`, `Flush`.
- Server: `ParseComplete`, `BindComplete`, `ParameterDescription`, `RowDescription`, `DataRow`, `PortalSuspended`, `CommandComplete`, `CloseComplete`, `ErrorResponse`, `ReadyForQuery`.

### Copy and Async
- Copy: `CopyInResponse`, `CopyOutResponse`, `CopyBothResponse`, `CopyData`, `CopyDone`, `CopyFail`.
- Async: `NotificationResponse`, `NoticeResponse`, `ErrorResponse`.

## State Machine Contract
- Parser connection states:
  - `STARTUP`
  - `AUTH_EXCHANGE`
  - `READY`
  - `IN_EXTENDED_PIPELINE`
  - `IN_COPY_IN`
  - `IN_COPY_OUT`
  - `TERMINATED`
- Required transitions:
  - `STARTUP -> AUTH_EXCHANGE` after valid startup request.
  - `AUTH_EXCHANGE -> READY` after auth success and startup completion messages.
  - `READY -> IN_EXTENDED_PIPELINE` after `Parse|Bind|Execute` before `Sync`.
  - `IN_EXTENDED_PIPELINE -> READY` only on `Sync` completion and `ReadyForQuery`.
  - `READY <-> IN_COPY_*` according to Copy responses and Copy completion.

## Parser to Engine Mapping Contract
- `Query` path:
  - parse SQL text
  - capability gate
  - UUID bind
  - emit SBLR
- Extended path:
  - store named/unnamed statement and portal metadata in parser session cache
  - bind values and format codes deterministically
  - map each execute to canonical engine request
- Cancel path:
  - validate backend key tuple
  - dispatch cancel to active engine execution context

## Result and Error Mapping Contract
- Parser MUST map engine result metadata to `RowDescription` deterministically:
  - column order
  - type oid mapping table
  - format codes
- Parser MUST map row values to requested text/binary formats.
- Parser MUST emit `ReadyForQuery` transaction status indicator correctly.
- Parser MUST map engine failures to `ErrorResponse` fields using profile mapping.

## Implementation Checklist

### PGW00 Frame Decoder
- [ ] Implement startup-frame decoder (no type byte).
- [ ] Implement normal-frame decoder (typed frames).
- [ ] Enforce max message length and hard-fail malformed lengths.

Pass condition:
- Message boundary synchronization is preserved after malformed input handling.

### PGW01 Startup and Auth
- [ ] Support `SSLRequest`, `CancelRequest`, and protocol startup message selection.
- [ ] Implement auth challenge-response flow and startup completion message set.
- [ ] Cache backend key data for cancellation.

Pass condition:
- Startup/auth exchange is deterministic for valid and invalid paths.

### PGW02 Simple Query Pipeline
- [ ] Implement `Query` request path end-to-end.
- [ ] Always terminate request cycle with `ReadyForQuery`.
- [ ] Abort remaining statements in a query string after first error.

Pass condition:
- Simple-query sequence is protocol-correct and deterministic.

### PGW03 Extended Query Pipeline
- [ ] Implement `Parse`, `Bind`, `Describe`, `Execute`, `Close`, `Sync`, `Flush`.
- [ ] Track named and unnamed prepared statement/portal lifetimes exactly.
- [ ] Enforce one `ReadyForQuery` per `Sync` in extended flow.

Pass condition:
- Extended pipeline semantics are byte-stable and state-correct.

### PGW04 Copy Sub-Protocol
- [ ] Implement copy-in, copy-out, and copy-both handling with correct state guards.
- [ ] Enforce allowed message classes while in copy modes.
- [ ] Map copy stream payloads to engine streaming requests/responses.

Pass condition:
- Copy mode behaves correctly under success, cancellation, and error conditions.

### PGW05 Cancel and Async
- [ ] Implement out-of-band `CancelRequest` path.
- [ ] Implement asynchronous notification delivery in normal operation state.
- [ ] Prevent async messages from breaking client state machine expectations.

Pass condition:
- Cancel and async events preserve protocol safety and correlation.

### PGW06 Error Mapping
- [ ] Map parser and engine errors to PostgreSQL error fields (code, severity, message, position when available).
- [ ] Ensure unmapped errors use deterministic generic mapping.
- [ ] Prevent object-discoverability leaks.

Pass condition:
- All failures are mapped and deterministic.

## Negative Requirements
- Parser MUST NOT leak engine-internal SQL or UUIDs to PostgreSQL clients.
- Parser MUST NOT skip `ReadyForQuery` at end of request cycle.
- Parser MUST NOT execute SQL locally.

## Conformance Gates
- `P28-PGW-GATE-01`: startup/auth/cancel paths pass protocol fixtures.
- `P28-PGW-GATE-02`: simple and extended query sequences pass deterministic fixtures.
- `P28-PGW-GATE-03`: copy modes pass flow and error recovery tests.
- `P28-PGW-GATE-04`: error and status mapping has zero unmapped cases.

## Evidence Artifacts
- `docs/specifications/work/conformance/wire/postgresql/STARTUP_AND_AUTH_RESULTS.json`
- `docs/specifications/work/conformance/wire/postgresql/SIMPLE_QUERY_SEQUENCE_RESULTS.csv`
- `docs/specifications/work/conformance/wire/postgresql/EXTENDED_QUERY_SEQUENCE_RESULTS.csv`
- `docs/specifications/work/conformance/wire/postgresql/COPY_MODE_RESULTS.md`
- `docs/specifications/work/conformance/wire/postgresql/ERROR_MAPPING_RESULTS.csv`

## Cross-Section Links
- `docs/specifications/26_Native_Wire_Protocol/IPC_SBWP_FRAME_SPEC.md`
- `docs/specifications/28_Parser_Implementations/NORMATIVE_PARSER_QUERY_TO_SBLR_CHECKLIST.md`
- `docs/specifications/28_Parser_Implementations/ERROR_MAPPING_AND_DIAGNOSTICS.md`

## Audit normalization note (2026-03-28)
- This file is retained as a checklist, not as stand-alone proof of full protocol parity.
- Current code-backed evidence in section `28` is limited to the shipped parser-agent and listener subset for this family.
- Exhaustive protocol fidelity, complete message-family coverage, and full production parity remain bounded and require family-local runtime evidence beyond this section-level checklist.

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
