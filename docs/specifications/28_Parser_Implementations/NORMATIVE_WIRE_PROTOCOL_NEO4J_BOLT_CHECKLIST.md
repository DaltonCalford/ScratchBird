# Normative Checklist: Neo4j Bolt Wire Protocol Adapter (Alpha)

## Purpose
Define strict implementation requirements for Neo4j Bolt protocol emulation in parser adapters.

## Scope
- Bolt handshake and version negotiation.
- Chunked transport framing and message boundaries.
- Core Cypher command message mapping and response encoding.

## Hard Invariants
1. Engine never parses Cypher and never speaks Bolt.
2. Parser handles all Bolt framing, Packstream decode/encode, and state transitions.
3. Parser maps Cypher requests to canonical AST and UUID-bound SBLR.
4. Parser profile gates Bolt version capabilities and message availability.
5. Parser never executes Cypher semantics locally.

## Alpha Wire Profile
- Handshake magic preamble:
  - `0x6060B017`.
- Protocol negotiation request:
  - 4-byte magic + four 4-byte protocol-version proposals.
- Version structure:
  - major, minor, and optional range fields as encoded negotiation versions.
- Chunk framing:
  - 16-bit unsigned chunk length prefix.
  - message ends with zero-length chunk (`0x0000`).
  - empty chunk without prior chunks acts as keepalive.

## Core Message Signatures

### Requests
- `HELLO = 0x01`
- `GOODBYE = 0x02`
- `RESET = 0x0F`
- `RUN = 0x10`
- `BEGIN = 0x11`
- `COMMIT = 0x12`
- `ROLLBACK = 0x13`
- `DISCARD = 0x2F`
- `PULL = 0x3F`
- `ROUTE = 0x66`
- `LOGON = 0x6A`
- `LOGOFF = 0x6B`
- profile-gated: `TELEMETRY = 0x54`

### Responses
- `SUCCESS = 0x70`
- `IGNORED = 0x7E`
- `FAILURE = 0x7F`

## Handshake Contract
1. Read and validate 4-byte Bolt magic preamble.
2. Read four proposed protocol versions.
3. Select highest supported version per profile policy.
4. Return selected version or explicit unsupported-version response.
5. Enter negotiated Bolt message state only after successful negotiation.

## Session State Contract
- Required parser states:
  - `NEGOTIATION`
  - `AUTHENTICATING`
  - `READY`
  - `AUTO_COMMIT`
  - `IN_TX`
  - `FAILED`
- Required rules:
  - `RUN` in `READY` transitions to execution state.
  - `BEGIN` opens explicit transaction state.
  - `RESET` interrupts and drains pending work deterministically.
  - `GOODBYE` closes session cleanly.

## Parser to Engine Mapping Contract
- `RUN`:
  - parse Cypher text and params
  - canonicalize
  - UUID bind
  - emit SBLR
- `BEGIN/COMMIT/ROLLBACK`:
  - map to engine transaction control with session transaction identity.
- `PULL/DISCARD`:
  - map to result-stream cursor controls.
- `ROUTE`:
  - map to configured routing-table provider surface.

## Streaming and Backpressure Contract
- Parser MUST reconstruct full message from chunk sequence before semantic decode.
- Parser MUST enforce maximum assembled-message size per profile limit.
- Parser MUST support incremental `PULL` and `DISCARD` semantics.
- Parser MUST preserve record ordering and metadata ordering per stream.

## Error Mapping Contract
- Parser and engine failures MUST map to Bolt `FAILURE` metadata shape.
- Non-fatal invalid-state paths defined by Bolt semantics MUST produce `IGNORED`.
- Parser MUST sanitize engine-internal details before returning errors.

## Implementation Checklist

### NBW00 Handshake Decoder
- [ ] Validate magic preamble `0x6060B017`.
- [ ] Decode negotiation proposals and select version deterministically.
- [ ] Reject unsupported negotiation with deterministic failure response.

Pass condition:
- Version negotiation is deterministic and profile-constrained.

### NBW01 Chunk Framing
- [ ] Implement 16-bit chunk-length reader.
- [ ] Assemble messages from one or more chunks until terminator `0x0000`.
- [ ] Treat stand-alone empty chunk as keepalive/no-op.

Pass condition:
- Frame assembly is lossless and boundary-correct.

### NBW02 Packstream Message Decode
- [ ] Decode message signatures and argument structures.
- [ ] Validate message-state legality before semantic mapping.
- [ ] Reject unknown signatures with deterministic failure mapping.

Pass condition:
- Signature and state validation are complete.

### NBW03 Cypher and Transaction Mapping
- [ ] Map `RUN` to canonical parse -> UUID bind -> SBLR pipeline.
- [ ] Map `BEGIN/COMMIT/ROLLBACK` to engine transaction controls.
- [ ] Map `PULL/DISCARD` to stream cursor controls.

Pass condition:
- Core request signatures have deterministic canonical execution paths.

### NBW04 Response Encoding
- [ ] Encode `SUCCESS`, `IGNORED`, and `FAILURE` responses correctly.
- [ ] Encode result records and metadata in deterministic order.
- [ ] Preserve correlation to originating request sequence.

Pass condition:
- Response encoding is protocol-correct and deterministic.

### NBW05 Reset and Failure Recovery
- [ ] Implement interrupt-and-drain behavior for `RESET`.
- [ ] Ensure stacked/reset-on-reset behavior is deterministic.
- [ ] Restore session to usable state after successful reset.

Pass condition:
- Reset semantics are safe under concurrent queued requests.

## Negative Requirements
- Parser MUST NOT expose engine SBLR internals or UUIDs on Bolt wire.
- Parser MUST NOT bypass auth or role checks in any state transition.
- Parser MUST NOT process message payloads exceeding configured limits.

## Conformance Gates
- `P28-NBW-GATE-01`: handshake/version-negotiation tests pass.
- `P28-NBW-GATE-02`: chunk framing/assembly and keepalive tests pass.
- `P28-NBW-GATE-03`: core signature mapping tests pass.
- `P28-NBW-GATE-04`: reset/recovery and failure-mapping tests pass.

## Evidence Artifacts
- `docs/specifications/work/conformance/wire/neo4j/HANDSHAKE_RESULTS.json`
- `docs/specifications/work/conformance/wire/neo4j/CHUNK_FRAMING_RESULTS.csv`
- `docs/specifications/work/conformance/wire/neo4j/SIGNATURE_MAPPING_RESULTS.csv`
- `docs/specifications/work/conformance/wire/neo4j/RESET_AND_RECOVERY_RESULTS.md`
- `docs/specifications/work/conformance/wire/neo4j/ERROR_MAPPING_RESULTS.csv`

## Cross-Section Links
- `docs/specifications/28_Parser_Implementations/NORMATIVE_PARSER_QUERY_TO_SBLR_CHECKLIST.md`
- `docs/specifications/28_Parser_Implementations/ERROR_MAPPING_AND_DIAGNOSTICS.md`

## Audit normalization note (2026-03-28)
- This file is now treated as target-state-only checklist material.
- Current section-28 source proof does not show a shipped dedicated parser-agent and listener implementation for this family.
- Wire-vocabulary adjacency, native-V3 command vocabulary, or connector/runtime terminology do not promote this family into current wire-parser parity.
