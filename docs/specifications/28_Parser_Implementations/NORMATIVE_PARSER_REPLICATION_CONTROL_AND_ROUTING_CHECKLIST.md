# Normative Checklist: Parser Replication Control and Routing (Alpha)

## Purpose
Define deterministic parser behavior for one-way and bi-directional replication control surfaces, execution routing, and diagnostics so low-capability implementations do not need inference.

## Scope
- Native parser replication SQL/control surfaces.
- Emulated parser mappings for publication/subscription compatibility commands.
- Replication status and conflict diagnostics output contracts.

## Hard Invariants
1. Parser maps dialect forms to canonical replication control operations only; engine remains SQL-agnostic.
2. Parser never performs replication apply locally.
3. Parser must include `EXPECT VERSION` for any channel mutation.
4. Parser must enforce deterministic routing intent for one-way and bi-directional channels.
5. Parser must preserve origin/sequence metadata in all replication control envelopes.

## Required Inputs
- Frozen request context from parser base checklist.
- Replication channel metadata (`channel_uuid`, `direction`, `channel_state`, `mode_version`).
- Statement class (`CONTROL`, `STATUS`, `CONFLICT_RESOLUTION`).
- Dialect profile and feature-gate decisions.

## Required Output Artifacts
- Canonical control envelope:
  - `replication_channel_uuid`
  - `expected_mode_version`
  - `control_op`
  - `direction`
  - `policy_payload`
  - `request_uuid`
- Deterministic error envelope (`SB-REPL-XXXX` mapped to dialect-native response).
- Response shape contract id for status/conflict queries.

## Implementation Checklist

### PR00 Grammar and Feature Gate
- [ ] Parse native replication control SQL defined in section 30.
- [ ] Gate each control token against capability profile.
- [ ] Reject unregistered feature keys deterministically.

Pass condition:
- Every replication control request maps to one canonical feature key and one control op.

### PR01 Channel Resolution
- [ ] Resolve `channel_name` to `replication_channel_uuid`.
- [ ] Reject unknown channel names without object-existence leakage for unauthorized users.
- [ ] Freeze channel metadata snapshot for request lifetime.

Pass condition:
- Request binds to one authorized channel UUID.

### PR02 Mutation Guard Fields
- [ ] Require `EXPECT VERSION` for:
  - state transitions
  - policy changes
  - conflict policy changes
  - DDL policy changes
- [ ] Reject missing or malformed version fields.

Pass condition:
- All mutable operations include optimistic version guard.

### PR03 Direction-Aware Validation
- [ ] For `ONE_WAY`, reject peer-only operations and require explicit source/target role mapping.
- [ ] For `BIDIRECTIONAL`, require loop-prevention fields and conflict policy.
- [ ] Reject direction-incompatible commands before dispatch.

Pass condition:
- Control commands are valid for channel direction semantics.

### PR04 DDL Policy Mapping
- [ ] Map parsed DDL policy token to canonical enum label (`BLOCK`, `MANUAL_APPROVE`, `SAFE_ONLY`, `FULL`).
- [ ] Validate policy transitions against channel state.
- [ ] Reject unsafe transitions while channel is streaming unless pause/fence precondition is met.

Pass condition:
- DDL policy mutations are deterministic and state-safe.

### PR05 Conflict Policy Mapping
- [ ] Map conflict policy token to canonical enum label.
- [ ] Validate required supporting fields:
  - `ORIGIN_PRIORITY` requires origin priority metadata.
  - `MANUAL_REQUIRED` requires conflict queue enabled.
- [ ] Reject incomplete policy payloads.

Pass condition:
- Conflict policy envelope is structurally complete and deterministic.

### PR06 Status Query Mapping
- [ ] Map `SHOW REPLICATION STATUS|LAG|CURSORS|CONFLICTS` to canonical read control ops.
- [ ] Emit fixed response shape ids:
  - `RS_REPLICATION_CHANNEL_STATUS`
  - `RS_REPLICATION_LAG`
  - `RS_REPLICATION_CURSOR_STATUS`
  - `RS_REPLICATION_CONFLICTS`
- [ ] Enforce deterministic column ordering.

Pass condition:
- Replication status outputs are stable across equivalent requests.

### PR07 Conflict Resolution Commands
- [ ] Parse and validate conflict resolution commands.
- [ ] Require conflict UUID and action payload.
- [ ] Validate action compatibility with current conflict kind.

Pass condition:
- Conflict resolution command envelope is deterministic and fully typed.

### PR08 Bi-Directional Loop Prevention Envelope
- [ ] Include source origin UUID and source commit sequence in apply-control envelopes.
- [ ] Include loop-prevention required flag for bi-directional operations.
- [ ] Reject commands lacking origin sequencing metadata when required.

Pass condition:
- Bi-directional requests carry sufficient metadata for loop-prevention checks.

### PR09 Error Mapping
- [ ] Map replication control failures to deterministic internal errors:
  - version mismatch
  - illegal state transition
  - split-brain fence active
  - conflict unresolved
  - policy invalid for direction
- [ ] Map internal errors to dialect-native errors without altering deterministic error class.

Pass condition:
- All replication control failures are deterministic and traceable.

### PR10 Emulated Parser Compatibility
- [ ] Support parser-level remap of emulated publication/subscription commands to canonical control ops.
- [ ] Enforce emulated limitations explicitly (reject unsupported controls).
- [ ] Preserve wire-level compatibility for each emulated parser.

Pass condition:
- Emulated parsers remain 1:1 compatible while using shared control internals.

## Negative Requirements
- Parser must not apply replication batches.
- Parser must not auto-resolve conflicts silently.
- Parser must not bypass split-brain fence.

## Conformance Gates
- `P28-REPL-GATE-01`: PR00..PR03 pass (grammar, binding, version guards, direction validation).
- `P28-REPL-GATE-02`: PR04..PR06 pass (policy mapping and deterministic status outputs).
- `P28-REPL-GATE-03`: PR07..PR09 pass (conflict operations and deterministic errors).
- `P28-REPL-GATE-04`: PR10 pass (emulated parser compatibility behavior).

## Cross-Section Links
- `24_Catalog_Model_and_Virtual_Overlays/CATALOG_TABLE_SCHEMA_REPLICATION_RUNTIME_AND_CONFLICT_RESOLUTION.md`
- `29_Listener_and_Server_Orchestration/NORMATIVE_LISTENER_ONE_WAY_AND_BIDIRECTIONAL_REPLICATION_CHECKLIST.md`
- `30_Client_Tooling/NATIVE_REPLICATION_SQL_CONTROL_CONTRACT.md`

## Audit normalization note (2026-03-28)
- This file is treated as a target-state worklist, not as present-day implementation proof.
- Current section `28` source authority is narrower and is centered on the native V3 parser stack plus the shipped Firebird, PostgreSQL, and MySQL parser-family seams.
- Broader normalization-gate, distributed-policy, passthrough, replication, connector, and fabric-parser claims require separate bounded proof before promotion.
