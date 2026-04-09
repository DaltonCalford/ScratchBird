# Normative Checklist: Firebird Wire Protocol Adapter (Alpha)

## Purpose
Define a deterministic implementation contract for the Firebird parser wire adapter so a low-capability implementation model can implement 1:1 Firebird-compatible transport behavior while preserving ScratchBird engine boundaries.

## Scope
- Firebird client to parser wire handling.
- Parser-to-engine mapping for Firebird wire operations.
- Error and response remapping to Firebird wire envelopes.

## Hard Invariants
1. Engine never parses SQL and never speaks Firebird wire protocol.
2. Parser performs all Firebird wire decode and encode.
3. Parser emits UUID-bound SBLR requests only.
4. Firebird compatibility is profile-gated; no fallback to native parser surface.
5. Parser cannot bypass engine auth or authorization decisions.

## Alpha Wire Profile
- Protocol versions:
  - Parser MUST negotiate highest mutually supported version from client proposal set.
  - Parser MUST support Firebird protocol family values required for Firebird 5.x clients.
  - Parser MUST accept compatible versions in the `PROTOCOL_VERSION13..PROTOCOL_VERSION20` range.
  - Parser MUST reject versions outside that range with deterministic protocol-version error mapping.
- Protocol types:
  - `ptype_batch_send`
  - `ptype_out_of_band`
  - `ptype_lazy_send`
- Protocol flags:
  - `pflag_compress` when both sides support compression.

## Wire Operation Families

### Session and Authentication
- `op_connect=1`
- `op_accept=3`
- `op_accept_data=94`
- `op_cond_accept=98`
- `op_reject=4`
- `op_trusted_auth=90`
- `op_cont_auth=92`
- `op_crypt=96`
- `op_crypt_key_callback=97`
- `op_disconnect=6`

### Database and Transaction
- `op_attach=19`, `op_create=20`, `op_detach=21`
- `op_transaction=29`, `op_commit=30`, `op_rollback=31`
- `op_prepare=32`, `op_prepare2=51`
- `op_commit_retaining=50`, `op_rollback_retaining=86`

### DSQL and Cursor
- `op_allocate_statement=62`
- `op_prepare_statement=68`
- `op_execute=63`, `op_execute2=76`, `op_exec_immediate=64`, `op_exec_immediate2=75`
- `op_fetch=65`, `op_fetch_scroll=112`, `op_fetch_response=66`
- `op_set_cursor=69`, `op_info_cursor=113`
- `op_free_statement=67`
- `op_sql_response=78`

### Blob and Slice
- `op_create_blob=34`, `op_create_blob2=57`
- `op_open_blob=35`, `op_open_blob2=56`
- `op_get_segment=36`, `op_put_segment=37`
- `op_batch_segments=44`
- `op_seek_blob=61`
- `op_get_slice=58`, `op_put_slice=59`, `op_slice=60`
- `op_close_blob=39`, `op_cancel_blob=38`
- `op_inline_blob=114`

### Service and Events
- `op_service_attach=82`, `op_service_detach=83`
- `op_service_info=84`, `op_service_start=85`
- `op_que_events=48`, `op_cancel_events=49`, `op_event=52`

### Generic Responses and Control
- `op_response=9`, `op_response_piggyback=72`
- `op_info_database=40`, `op_info_request=41`, `op_info_transaction=42`, `op_info_blob=43`, `op_info_sql=70`, `op_info_batch=111`
- `op_cancel=91`
- `op_ping=93`

## Deterministic Handshake Contract
1. Parse `op_connect` connect block and candidate protocol versions.
2. Select highest supported protocol version.
3. Select transport type and flags from intersection:
   - type preference order: `ptype_lazy_send`, `ptype_out_of_band`, `ptype_batch_send`
   - compression enabled only when negotiated.
4. Emit one of:
   - `op_accept`
   - `op_accept_data`
   - `op_cond_accept` (when auth continuation is required)
   - `op_reject` (on incompatibility or auth hard-failure)
5. Continue auth exchange using `op_cont_auth` until completion.
6. Transition to attached state only after engine auth success.

## Parser to Engine Mapping Contract
- Parser MUST convert all statement/object references to UUID before SBLR emission.
- Parser MUST send metadata required by section 28 query-to-SBLR checklist:
  - dialect id `firebird`
  - profile id and version
  - canonicalized SQL
  - capability decision log
  - source map
- Parser MUST map service-manager operations to native control SBLR operations and never execute service semantics locally.

## Response and Error Mapping Contract
- Parser MUST convert engine response objects to Firebird wire response packets:
  - `op_response`, `op_sql_response`, `op_fetch_response`, `op_event`.
- Parser MUST map engine failures to Firebird-compatible status vectors.
- Parser MUST preserve deterministic correlation id in parser diagnostics.
- Parser MUST avoid object discoverability leaks:
  - non-discoverable object errors map as not-found unless admin diagnostics mode is enabled.

## Flow Control and Streaming Rules
- Parser MUST support chunked fetch behavior using fetch packets and cursor info.
- Parser MUST support async cancel via `op_cancel`.
- Parser MUST support event subscription lifecycle and async event callbacks.
- Parser MUST enforce per-connection packet size limits before decode.

## Implementation Checklist

### FBW00 Transport Decoder
- [ ] Decode packet operation id and packet body deterministically.
- [ ] Reject unknown opcodes with dialect-native protocol error.
- [ ] Enforce max packet size gate before body decode.

Pass condition:
- No malformed packet reaches semantic mapping stage.

### FBW01 Handshake Negotiation
- [ ] Implement handshake steps defined in "Deterministic Handshake Contract".
- [ ] Record selected protocol version, type, and flags in immutable session state.
- [ ] Refuse attach/create operations until handshake is complete.

Pass condition:
- Handshake state machine is deterministic and replay-safe.

### FBW02 Auth Continuation
- [ ] Implement `op_cont_auth` loop handling.
- [ ] Zero temporary auth buffers immediately after auth completion or failure.
- [ ] Reject unauthenticated post-handshake operations.

Pass condition:
- Auth lifecycle obeys strict zeroization and state guards.

### FBW03 Statement and Transaction Mapping
- [ ] Map DSQL operations to canonical AST and SBLR.
- [ ] Map transaction operations to canonical transaction control requests.
- [ ] Preserve Firebird transaction-control semantics at parser layer.

Pass condition:
- All DSQL and transaction wire operations have deterministic SBLR mapping paths.

### FBW04 Blob and Slice Mapping
- [ ] Implement blob lifecycle and segment/slice operations as parser-to-engine mappings.
- [ ] Enforce bounded segment lengths and deterministic sequence handling.
- [ ] Map blob errors to Firebird-compatible wire errors.

Pass condition:
- Blob operations are deterministic and bounded.

### FBW05 Service Channel Mapping
- [ ] Map service attach/start/info/detach to native control operations.
- [ ] Preserve Firebird service wire semantics for client compatibility.
- [ ] Reject unsupported service operations with profile-driven errors.

Pass condition:
- Service channel operations are compatible and policy-gated.

### FBW06 Event and Async Mapping
- [ ] Implement `op_que_events` and `op_cancel_events`.
- [ ] Emit `op_event` callbacks with deterministic event payload mapping.
- [ ] Ensure cancel and event paths do not bypass authorization.

Pass condition:
- Event lifecycle and async paths are deterministic and secure.

### FBW07 Response Encoder
- [ ] Encode response packets with correct operation id and payload format.
- [ ] Preserve fetch ordering and cursor state invariants.
- [ ] Encode errors as Firebird-compatible status vectors.

Pass condition:
- Client receives byte-valid Firebird protocol responses for all mapped operations.

## Negative Requirements
- Parser MUST NOT execute SQL, PSQL, or service operations locally.
- Parser MUST NOT infer missing profile capability rows.
- Parser MUST NOT leak hidden object existence via error wording or codes.

## Conformance Gates
- `P28-FBW-GATE-01`: handshake negotiation, auth continuation, and attach guards pass.
- `P28-FBW-GATE-02`: DSQL, transaction, blob, and service mappings pass deterministic fixtures.
- `P28-FBW-GATE-03`: async cancel and event flows pass stress tests.
- `P28-FBW-GATE-04`: error/status-vector mapping is complete with no fallback gaps.

## Evidence Artifacts
- `docs/specifications/work/conformance/wire/firebird/HANDSHAKE_NEGOTIATION_RESULTS.json`
- `docs/specifications/work/conformance/wire/firebird/OPCODE_MAPPING_RESULTS.csv`
- `docs/specifications/work/conformance/wire/firebird/ERROR_MAPPING_RESULTS.csv`
- `docs/specifications/work/conformance/wire/firebird/CANCEL_AND_EVENT_STRESS_RESULTS.md`

## Cross-Section Links
- `docs/specifications/26_Native_Wire_Protocol/IPC_SBWP_FRAME_SPEC.md`
- `docs/specifications/28_Parser_Implementations/NORMATIVE_PARSER_QUERY_TO_SBLR_CHECKLIST.md`
- `docs/specifications/28_Parser_Implementations/ERROR_MAPPING_AND_DIAGNOSTICS.md`
- `docs/specifications/30_Client_Tooling/README.md`

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
