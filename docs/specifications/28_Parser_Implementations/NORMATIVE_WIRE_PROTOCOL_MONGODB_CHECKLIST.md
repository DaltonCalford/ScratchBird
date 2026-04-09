# Normative Checklist: MongoDB Wire Protocol Adapter (Alpha)

## Purpose
Define deterministic implementation requirements for MongoDB wire-protocol emulation in parser adapters.

## Scope
- MongoDB message framing and opcode handling.
- OP_MSG and OP_COMPRESSED decode/encode.
- Command-document mapping to canonical engine operations.

## Hard Invariants
1. Engine never speaks MongoDB wire protocol.
2. Parser performs all BSON message decode and encode.
3. Parser maps command semantics to canonical UUID-bound SBLR requests.
4. Parser profile governs command availability and rejects.
5. Parser never executes command semantics locally.

## Alpha Wire Profile
- Message header (`MSGHEADER::Layout`, little-endian):
  - `int32 messageLength`
  - `int32 requestID`
  - `int32 responseTo`
  - `int32 opCode`
- Max message size:
  - `MaxMessageSizeBytes = 48 * 1000 * 1000`.
- Required opcodes:
  - `dbMsg = 2013` (primary command transport)
  - `dbCompressed = 2012` (compressed transport wrapper)
- Legacy opcodes (`dbQuery`, `dbInsert`, `dbUpdate`, `dbDelete`, `dbGetMore`, `dbKillCursors`) are profile-gated compatibility surfaces.

## OP_MSG Contract
- Flags:
  - required low bits:
    - `kChecksumPresent` bit 0
    - `kMoreToCome` bit 1
  - optional high bits:
    - `kExhaustSupported` bit 16
- Section kinds:
  - `0`: body document section
  - `1`: document-sequence section
  - `2`: security-token section (profile-gated)
- Unknown required flags (low 16 bits) MUST cause deterministic protocol error.

## Connection and Command Contract
1. Client sends command messages using OP_MSG body documents.
2. Parser MUST accept `hello`/compat handshake command family and initialize session context.
3. Parser MUST decode command name as first key in body document.
4. Parser MUST map command and payload to canonical parser request envelope:
   - request id
   - session context
   - command document and sequences
   - profile version
5. Parser MUST emit engine request and map result/error to OP_MSG reply document.

## Compression Contract
- If `opCode=dbCompressed`:
  - parser MUST decompress payload before command handling.
  - parser MUST enforce decompressed-size limits before parse.
- Compression algorithm availability is profile-configured.
- Unknown compression algorithms MUST hard-fail with deterministic protocol error.

## Cursor and Streaming Contract
- Parser MUST support cursor-style response surfaces by command mapping:
  - initial batch
  - follow-up getMore behavior
  - cursor id lifecycle.
- `kMoreToCome` handling MUST be state-safe:
  - no duplicate terminal response
  - deterministic request correlation.

## Error Mapping Contract
- Parser MUST map parser and engine failures into MongoDB-compatible command error document shape.
- Error payloads MUST include deterministic correlation id.
- Discoverability rules MUST prevent protected-object existence leaks.

## Implementation Checklist

### MGW00 Header and Opcode Decode
- [ ] Implement strict decode for 16-byte message header.
- [ ] Validate `messageLength`, `opCode`, and size bounds.
- [ ] Reject unsupported opcode or malformed header with deterministic protocol error.

Pass condition:
- No malformed message reaches command-decode stage.

### MGW01 OP_MSG Parse/Serialize
- [ ] Parse flags and section stream.
- [ ] Implement section kind `0`, `1`, and profile-gated `2`.
- [ ] Reject unknown required flags and unknown section kinds.

Pass condition:
- OP_MSG parser is deterministic and rejects invalid structures safely.

### MGW02 OP_COMPRESSED Path
- [ ] Parse compressed message wrapper.
- [ ] Decompress to canonical inner message.
- [ ] Enforce post-decompression size and structure validation.

Pass condition:
- Compression path is safe and deterministic.

### MGW03 Command Routing
- [ ] Extract command name and argument document deterministically.
- [ ] Apply profile capability gate for command name.
- [ ] Map command payload to canonical request model and emit SBLR.

Pass condition:
- All accepted commands have deterministic canonical mappings.

### MGW04 Reply and Cursor Mapping
- [ ] Encode success replies as OP_MSG body documents.
- [ ] Implement cursor id and batch mapping for find/getMore-style flows.
- [ ] Honor `kMoreToCome` semantics when configured.

Pass condition:
- Reply shapes and cursor lifecycle are stable and protocol-correct.

### MGW05 Error and Diagnostics
- [ ] Map parser and engine errors to MongoDB error document format.
- [ ] Include deterministic correlation id and sanitized message text.
- [ ] Ensure unmapped errors use deterministic generic mapping.

Pass condition:
- Zero unmapped error paths and no discoverability leaks.

## Negative Requirements
- Parser MUST NOT expose engine-internal UUIDs or SBLR details on wire.
- Parser MUST NOT bypass engine authorization for any command.
- Parser MUST NOT accept oversized or invalid BSON payloads.

## Conformance Gates
- `P28-MGW-GATE-01`: header/opcode/size validation tests pass.
- `P28-MGW-GATE-02`: OP_MSG and OP_COMPRESSED parse/serialize tests pass.
- `P28-MGW-GATE-03`: command routing and cursor lifecycle tests pass.
- `P28-MGW-GATE-04`: error-document mapping tests pass.

## Evidence Artifacts
- `docs/specifications/work/conformance/wire/mongodb/HEADER_AND_OPCODE_RESULTS.json`
- `docs/specifications/work/conformance/wire/mongodb/OP_MSG_SECTION_RESULTS.csv`
- `docs/specifications/work/conformance/wire/mongodb/COMMAND_MAPPING_RESULTS.csv`
- `docs/specifications/work/conformance/wire/mongodb/CURSOR_FLOW_RESULTS.csv`
- `docs/specifications/work/conformance/wire/mongodb/ERROR_MAPPING_RESULTS.csv`

## Cross-Section Links
- `docs/specifications/28_Parser_Implementations/NORMATIVE_PARSER_QUERY_TO_SBLR_CHECKLIST.md`
- `docs/specifications/28_Parser_Implementations/ERROR_MAPPING_AND_DIAGNOSTICS.md`

## Audit normalization note (2026-03-28)
- This file is now treated as target-state-only checklist material.
- Current section-28 source proof does not show a shipped dedicated parser-agent and listener implementation for this family.
- Wire-vocabulary adjacency, native-V3 command vocabulary, or connector/runtime terminology do not promote this family into current wire-parser parity.
