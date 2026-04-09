# Normative Checklist: Milvus gRPC Protocol Adapter (Alpha)

## Purpose
Define deterministic implementation requirements for Milvus protocol emulation using gRPC/protobuf transport in parser adapters.

## Scope
- gRPC transport/session behavior for Milvus clients.
- Protobuf request and response mapping for Milvus API surface.
- Parser-to-engine conversion for vector/search and catalog operations.

## Hard Invariants
1. Engine never speaks Milvus gRPC and never parses Milvus protobuf requests directly.
2. Parser handles all gRPC method dispatch, protobuf decode/encode, and status mapping.
3. Parser converts method calls to canonical request structures and UUID-bound SBLR operations.
4. Parser profile controls method availability and reject behavior.
5. Parser never executes API semantics locally.

## Alpha Wire Profile
- Transport:
  - HTTP/2 + gRPC framing.
  - protobuf-encoded request and response messages.
- Parser ingress service emulation:
  - Milvus client-facing RPC surface compatible with enabled profile.
- Authentication metadata:
  - token and auth metadata support is profile-driven.
  - auth validation path delegates to canonical engine auth controls.

## Service Surface Contract
- Parser MUST support configured Milvus RPC families required by Alpha profile:
  - collection/database definition operations (create/drop/describe/list)
  - partition operations
  - insert/search/query operations
  - index and load/release operations
  - metrics and health operations
- Parser MUST reject unenabled RPCs deterministically with profile-defined gRPC status and reason.

## Request Mapping Contract
1. Decode gRPC method id and protobuf payload.
2. Validate required fields and request message shape.
3. Apply capability profile gate (`IMPLEMENT`, `REMAP`, `REJECT`).
4. Canonicalize to parser internal command model.
5. Resolve object references by name/path to UUIDs through engine lookup.
6. Emit canonical engine request.

## Response Mapping Contract
- Unary RPC:
  - map engine response to protobuf response message.
  - map operation status to Milvus/common status fields.
- Streaming RPC (profile-enabled methods such as query stream):
  - preserve row/chunk ordering
  - enforce backpressure and bounded send queues
  - terminate stream with deterministic status mapping.

## Error Mapping Contract
- Parser MUST map parser and engine errors to deterministic gRPC status + Milvus status payload fields.
- Parser MUST preserve structured reason fields and correlation ids for diagnostics.
- Discoverability-safe error behavior MUST be preserved.

## Implementation Checklist

### MLW00 gRPC Method Router
- [ ] Implement deterministic method-dispatch table keyed by full RPC method name.
- [ ] Reject unknown methods with deterministic status.
- [ ] Enforce maximum request payload size before protobuf decode.

Pass condition:
- Method dispatch is deterministic and bounded.

### MLW01 Protobuf Decode and Validation
- [ ] Decode protobuf requests for all enabled methods.
- [ ] Validate required fields and message invariants.
- [ ] Return deterministic invalid-argument errors on schema violations.

Pass condition:
- No invalid protobuf request reaches semantic mapping.

### MLW02 Capability Gate
- [ ] Apply profile decision for each method and option.
- [ ] Enforce deterministic `IMPLEMENT/REMAP/REJECT`.
- [ ] Emit capability decision artifact fields for each request.

Pass condition:
- Zero ungated method paths remain.

### MLW03 Canonical Mapping and UUID Bind
- [ ] Map method payload to canonical operation model.
- [ ] Resolve collection/partition/index names to UUID references.
- [ ] Emit canonical engine request with required session metadata.

Pass condition:
- All enabled methods have deterministic canonical request mapping.

### MLW04 Unary Response Mapping
- [ ] Map engine status/results to protobuf response messages.
- [ ] Populate Milvus status structures consistently.
- [ ] Ensure deterministic field order/value normalization in serialized payload.

Pass condition:
- Unary responses are stable and compatible.

### MLW05 Streaming Response Mapping
- [ ] Implement stream send loops with bounded buffering.
- [ ] Preserve ordering guarantees and deterministic end-of-stream semantics.
- [ ] Map stream errors to terminal status consistently.

Pass condition:
- Streaming behavior is deterministic under load and failure.

### MLW06 Auth and Metadata
- [ ] Parse auth metadata and validate against engine auth controls.
- [ ] Enforce per-method authorization gates.
- [ ] Clear auth secrets from transient buffers after validation.

Pass condition:
- Auth metadata processing is secure and deterministic.

### MLW07 Error and Diagnostics
- [ ] Map parser/engine failures to gRPC status + Milvus status reason.
- [ ] Include deterministic correlation id fields.
- [ ] Prevent object-discoverability leakage in reasons.

Pass condition:
- Error mapping is complete and sanitized.

## Negative Requirements
- Parser MUST NOT bypass engine authorization.
- Parser MUST NOT expose internal UUIDs in client-visible protobuf fields unless explicitly defined by profile.
- Parser MUST NOT support unbounded stream buffering.

## Conformance Gates
- `P28-MLW-GATE-01`: method routing and protobuf validation tests pass.
- `P28-MLW-GATE-02`: capability gating and canonical mapping tests pass.
- `P28-MLW-GATE-03`: unary and streaming response-mapping tests pass.
- `P28-MLW-GATE-04`: auth and error-mapping tests pass.

## Evidence Artifacts
- `docs/specifications/work/conformance/wire/milvus/METHOD_ROUTING_RESULTS.json`
- `docs/specifications/work/conformance/wire/milvus/PROTOBUF_VALIDATION_RESULTS.csv`
- `docs/specifications/work/conformance/wire/milvus/CANONICAL_MAPPING_RESULTS.csv`
- `docs/specifications/work/conformance/wire/milvus/STREAMING_RESULTS.md`
- `docs/specifications/work/conformance/wire/milvus/AUTH_AND_ERROR_MAPPING_RESULTS.csv`

## Cross-Section Links
- `docs/specifications/28_Parser_Implementations/NORMATIVE_PARSER_QUERY_TO_SBLR_CHECKLIST.md`
- `docs/specifications/28_Parser_Implementations/ERROR_MAPPING_AND_DIAGNOSTICS.md`

## Audit normalization note (2026-03-28)
- This file is now treated as target-state-only checklist material.
- Current section-28 source proof does not show a shipped dedicated parser-agent and listener implementation for this family.
- Wire-vocabulary adjacency, native-V3 command vocabulary, or connector/runtime terminology do not promote this family into current wire-parser parity.
