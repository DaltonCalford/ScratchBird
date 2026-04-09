# Normative Checklist: Cassandra Native Protocol Adapter (Alpha)

## Purpose
Define strict implementation rules for Cassandra native protocol emulation in parser adapters.

## Scope
- Cassandra protocol framing and envelope handling.
- Startup/auth/query/prepare/execute/batch/event flows.
- Parser-to-engine mapping and deterministic error translation.

## Hard Invariants
1. Engine does not speak Cassandra wire protocol.
2. Parser implements all Cassandra frame and envelope decode/encode behavior.
3. Parser maps CQL requests to canonical AST and UUID-based SBLR.
4. Parser profile controls feature gates and unsupported-surface rejects.
5. Parser never executes CQL semantics locally.

## Alpha Wire Profile
- Supported protocol versions:
  - v4 (compatibility profile)
  - v5 (primary Cassandra 5.x profile)
- v5 framing:
  - uncompressed frame header: 6 bytes + payload + CRC32 trailer
  - compressed frame header: 8 bytes + compressed payload + CRC32 trailer
  - frame integer fields are little-endian
- Envelope header:
  - fixed 9 bytes: `version`, `flags`, `stream`, `opcode`, `length`
  - envelope integer fields are big-endian

## Startup and Negotiation Contract
1. Before STARTUP completion in v5:
   - `STARTUP` and pre-startup `OPTIONS` are sent unframed.
   - corresponding `SUPPORTED`, `READY`, `AUTHENTICATE` are unframed.
2. After `READY` or `AUTHENTICATE` response:
   - parser switches to negotiated version framing mode.
3. Compression:
   - negotiated by STARTUP option.
   - STARTUP itself is never compressed.
   - for v5, LZ4 is the only compression profile in Alpha.

## Opcode Contract
- `0x00 ERROR`
- `0x01 STARTUP`
- `0x02 READY`
- `0x03 AUTHENTICATE`
- `0x05 OPTIONS`
- `0x06 SUPPORTED`
- `0x07 QUERY`
- `0x08 RESULT`
- `0x09 PREPARE`
- `0x0A EXECUTE`
- `0x0B REGISTER`
- `0x0C EVENT`
- `0x0D BATCH`
- `0x0E AUTH_CHALLENGE`
- `0x0F AUTH_RESPONSE`
- `0x10 AUTH_SUCCESS`

## Stream and Concurrency Contract
- Client requests MUST use non-negative stream ids.
- Server event pushes MUST use stream id `-1`.
- Response stream id MUST match originating request stream id.
- Parser MUST allow out-of-order completion across different stream ids.

## Request Mapping Contract
- `QUERY`, `PREPARE`, `EXECUTE`, `BATCH`:
  - parse/decode payload
  - capability gate by profile
  - canonicalize
  - UUID bind
  - emit SBLR
- `REGISTER`:
  - configure event subscriptions in parser session state.
- auth messages:
  - map to engine auth/credential checks via parser auth control path.

## Result and Paging Contract
- Parser MUST produce `RESULT` envelope kind consistent with request class:
  - `VOID`
  - `ROWS`
  - `SET_KEYSPACE`
  - `PREPARED`
  - `SCHEMA_CHANGE`
- Paging:
  - parser MUST preserve paging state token semantics.
  - parser MUST reject invalid paging state deterministically.

## Error Mapping Contract
- Parser MUST map engine and parser failures to Cassandra error envelopes with deterministic error code selection.
- No raw engine-internal errors are sent to clients.
- Discoverability-safe behavior MUST be preserved for unauthorized objects.

## Implementation Checklist

### CSW00 Frame Decoder/Encoder
- [ ] Implement v4 and v5 framing readers/writers.
- [ ] Validate CRC24/CRC32 when v5 framing is enabled.
- [ ] Enforce frame and envelope max size bounds.

Pass condition:
- Frame parsing is deterministic and corruption-safe.

### CSW01 Startup Negotiation
- [ ] Implement pre-startup unframed message handling for v5.
- [ ] Implement transition to framed mode after startup completion.
- [ ] Implement STARTUP option validation and compression negotiation.

Pass condition:
- Startup mode transition is deterministic and version-correct.

### CSW02 Stream Scheduler
- [ ] Implement per-stream request tracking.
- [ ] Permit concurrent in-flight requests across stream ids.
- [ ] Ensure response stream id integrity and event stream id `-1`.

Pass condition:
- Stream behavior is protocol-correct under concurrency.

### CSW03 Query/Prepare/Execute/Batch Mapping
- [ ] Implement canonical mapping path for each request class.
- [ ] Implement consistency option and flags parsing as profile-driven metadata.
- [ ] Implement parameter metadata and signature derivation deterministically.

Pass condition:
- All executable request classes map to deterministic SBLR envelopes.

### CSW04 Result and Paging
- [ ] Implement all required `RESULT` variants.
- [ ] Implement rows metadata and paging state handling.
- [ ] Implement deterministic rejection for invalid paging states.

Pass condition:
- Result/paging behavior is deterministic and client-compatible.

### CSW05 Auth and Events
- [ ] Implement `AUTHENTICATE`, `AUTH_CHALLENGE`, `AUTH_RESPONSE`, `AUTH_SUCCESS`.
- [ ] Implement `REGISTER` and `EVENT` lifecycle.
- [ ] Prevent event channel use before auth/session readiness.

Pass condition:
- Auth and event flows are state-safe and deterministic.

### CSW06 Error Mapping
- [ ] Map all parser and engine errors to Cassandra error codes.
- [ ] Emit deterministic and sanitized error text payloads.
- [ ] Ensure no unmapped fallback path exists.

Pass condition:
- Error mapping is complete and deterministic.

## Negative Requirements
- Parser MUST NOT downgrade protocol version or features silently.
- Parser MUST NOT reuse stream ids in ways that violate in-flight tracking.
- Parser MUST NOT execute CQL or policy logic locally.

## Conformance Gates
- `P28-CSW-GATE-01`: v4/v5 framing and startup transition tests pass.
- `P28-CSW-GATE-02`: stream concurrency and response-matching tests pass.
- `P28-CSW-GATE-03`: query/prepare/execute/batch mapping tests pass.
- `P28-CSW-GATE-04`: result/paging/auth/event/error mapping tests pass.

## Evidence Artifacts
- `docs/specifications/work/conformance/wire/cassandra/FRAME_AND_STARTUP_RESULTS.json`
- `docs/specifications/work/conformance/wire/cassandra/STREAM_CONCURRENCY_RESULTS.csv`
- `docs/specifications/work/conformance/wire/cassandra/QUERY_AND_BATCH_MAPPING_RESULTS.csv`
- `docs/specifications/work/conformance/wire/cassandra/RESULT_AND_PAGING_RESULTS.csv`
- `docs/specifications/work/conformance/wire/cassandra/ERROR_MAPPING_RESULTS.csv`

## Cross-Section Links
- `docs/specifications/28_Parser_Implementations/NORMATIVE_PARSER_QUERY_TO_SBLR_CHECKLIST.md`
- `docs/specifications/28_Parser_Implementations/ERROR_MAPPING_AND_DIAGNOSTICS.md`

## Audit normalization note (2026-03-28)
- This file is now treated as target-state-only checklist material.
- Current section-28 source proof does not show a shipped dedicated parser-agent and listener implementation for this family.
- Wire-vocabulary adjacency, native-V3 command vocabulary, or connector/runtime terminology do not promote this family into current wire-parser parity.
