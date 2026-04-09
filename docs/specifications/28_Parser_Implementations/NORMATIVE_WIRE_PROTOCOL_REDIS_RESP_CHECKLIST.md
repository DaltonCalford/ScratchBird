# Normative Checklist: Redis RESP2/RESP3 Wire Adapter (Alpha)

## Purpose
Define deterministic implementation requirements for Redis protocol emulation over RESP2 and RESP3.

## Scope
- RESP request and response framing.
- HELLO-based protocol version selection.
- Command execution, push, and error mapping behavior.

## Hard Invariants
1. Engine never speaks RESP and never executes Redis commands directly.
2. Parser decodes RESP command requests and encodes RESP responses.
3. Parser maps command semantics to canonical UUID-bound SBLR operations.
4. Parser profile controls command exposure and feature gates.
5. Parser never performs local data execution.

## Alpha Wire Profile
- Supported client protocol modes:
  - RESP2
  - RESP3
- Protocol selection:
  - `HELLO` command supports selecting protocol version `2` or `3`.
  - `HELLO AUTH <user> <pass>` supported per auth policy.
- Primary command request format:
  - RESP array of bulk strings (`*<argc>\r\n` + repeated `$<len>\r\n<arg>\r\n`).

## RESP Type Contract
- String: `+`
- Error: `-`
- Integer: `:`
- Bulk String: `$`
- Array: `*`
- Set: `~` (RESP3)
- Map: `%` (RESP3)
- Boolean: `#` (RESP3)
- Double: `,` (RESP3)
- Null: `_` (RESP3)
- Big number: `(` (RESP3)
- Verbatim string: `=` (RESP3)
- Attributes: `|` (RESP3)

## Request Decode Contract
1. Parser MUST decode multibulk request frames exactly:
   - read `*count`
   - read each `$len` and payload
2. Parser MUST reject malformed count/length lines with deterministic protocol errors.
3. Parser MUST enforce maximum argument count and payload limits.
4. Parser MUST support profile-gated inline protocol handling only when explicitly enabled.

## Session and HELLO Contract
- `HELLO` command behavior:
  - validate version argument (`2` or `3`)
  - optionally process AUTH tuple
  - optionally process `SETNAME`
  - return negotiated protocol metadata map
- Parser MUST require authentication before non-auth commands according to policy.
- Parser MUST isolate session protocol mode and command exposure per connection.

## Command Mapping Contract
- Command text (argv[0]) is normalized by profile rules.
- Parser maps command name + argv vector to canonical command AST.
- Parser applies capability gates:
  - `IMPLEMENT`
  - `REMAP`
  - `REJECT`
- Parser emits canonical engine request and maps result to RESP2/RESP3 form.

## Response Mapping Contract
- RESP2 compatibility rules:
  - RESP3-only types must be converted to RESP2-compatible forms when required.
- RESP3 mode:
  - parser may emit native RESP3 structures including maps, sets, attributes, booleans.
- Push messages:
  - profile-gated and session-mode safe.

## Error Mapping Contract
- Parser MUST map parser/engine errors to Redis error replies with deterministic prefix and code text.
- Parser MUST not leak internal identifiers or protected object existence.

## Implementation Checklist

### RDW00 Request Frame Decoder
- [ ] Implement multibulk parser for command ingress.
- [ ] Enforce line termination and length validation.
- [ ] Enforce max request size and argument count limits.

Pass condition:
- Invalid request inputs are rejected before semantic mapping.

### RDW01 HELLO and Session Mode
- [ ] Implement `HELLO` parsing with version/auth/setname options.
- [ ] Switch session mode between RESP2 and RESP3 deterministically.
- [ ] Enforce authentication requirements before protected command classes.

Pass condition:
- Session protocol mode and auth state are deterministic.

### RDW02 Command Routing
- [ ] Parse command name and argv vector into canonical command representation.
- [ ] Apply capability profile gate and reject unsupported commands deterministically.
- [ ] Map accepted commands to canonical engine requests.

Pass condition:
- Command-to-canonical mapping is complete for enabled profile commands.

### RDW03 Response Encoder
- [ ] Encode success responses in mode-correct RESP format.
- [ ] Apply RESP3-to-RESP2 compatibility conversion where needed.
- [ ] Preserve deterministic order for arrays/maps/sets in protocol-defined cases.

Pass condition:
- Response payloads are valid and mode-correct.

### RDW04 Push and Streaming
- [ ] Implement profile-gated RESP3 push response path.
- [ ] Ensure push messages do not break command response sequencing.
- [ ] Enforce bounded output buffering and backpressure handling.

Pass condition:
- Push/stream behavior is deterministic and bounded.

### RDW05 Error Mapping
- [ ] Map parser and engine errors to Redis error reply strings deterministically.
- [ ] Apply discoverability-safe error policy.
- [ ] Ensure no unmapped error path exists.

Pass condition:
- Error behavior is deterministic and non-leaking.

## Negative Requirements
- Parser MUST NOT execute Redis command semantics locally.
- Parser MUST NOT emit RESP3-only types to RESP2 clients unless converted.
- Parser MUST NOT allow unauthenticated access to protected command classes.

## Conformance Gates
- `P28-RDW-GATE-01`: request-frame parse/validation tests pass.
- `P28-RDW-GATE-02`: HELLO/auth/session-mode tests pass.
- `P28-RDW-GATE-03`: command mapping and response encoding tests pass.
- `P28-RDW-GATE-04`: push/backpressure and error-mapping tests pass.

## Evidence Artifacts
- `docs/specifications/work/conformance/wire/redis/REQUEST_FRAME_RESULTS.json`
- `docs/specifications/work/conformance/wire/redis/HELLO_AND_AUTH_RESULTS.csv`
- `docs/specifications/work/conformance/wire/redis/COMMAND_MAPPING_RESULTS.csv`
- `docs/specifications/work/conformance/wire/redis/RESP2_RESP3_RESPONSE_RESULTS.csv`
- `docs/specifications/work/conformance/wire/redis/ERROR_MAPPING_RESULTS.csv`

## Cross-Section Links
- `docs/specifications/28_Parser_Implementations/NORMATIVE_PARSER_QUERY_TO_SBLR_CHECKLIST.md`
- `docs/specifications/28_Parser_Implementations/ERROR_MAPPING_AND_DIAGNOSTICS.md`

## Audit normalization note (2026-03-28)
- This file is now treated as target-state-only checklist material.
- Current section-28 source proof does not show a shipped dedicated parser-agent and listener implementation for this family.
- Wire-vocabulary adjacency, native-V3 command vocabulary, or connector/runtime terminology do not promote this family into current wire-parser parity.
