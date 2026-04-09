# Normative Checklist: Parser Passthrough and Live Migration Control (Alpha)

## Purpose
Define deterministic parser behavior for passthrough query routing and live cross-database migration so low-capability implementations can execute mode-correct behavior without inference.

## Scope
- Native parser and all emulated parsers.
- Cross-database migration control flow and execution flow.
- Read/write/DDL/admin routing decisions per migration mode.

## Hard Invariants
1. Parser performs dialect mapping only; engine remains SQL-agnostic.
2. Parser must never route by local guess; routing is driven by persisted `migration_job` mode and version.
3. Parser cannot skip audit writes in modes that require audit evidence.
4. Parser cannot expose legacy source details to clients outside configured diagnostic policy.
5. Parser must preserve deterministic source-map and correlation identifiers for all routed requests.

## Required Inputs Per Routed Statement
- Frozen context from `NORMATIVE_PARSER_QUERY_TO_SBLR_CHECKLIST.md`.
- `migration_uuid` (optional; required when statement is migration-bound).
- `migration_mode` and `mode_version` from `migration_job`.
- Statement class:
  - `READ`
  - `WRITE`
  - `DDL`
  - `CONTROL`
- Routed target capability from section 29 listener contract.

## Required Output Artifacts
- Routed execution envelope with:
  - `migration_uuid`
  - `mode_version`
  - `statement_class`
  - `route_intent` (`LEGACY`, `EMULATED`, `DUAL_WRITE`, `DUAL_READ_COMPARE`, `MIRROR_WRITE`)
  - `request_uuid`
- Audit payloads for mode-required write/read audits.
- Deterministic dialect error envelope on routing rejection.

## Mode Routing Matrix (Parser Contract)

| Mode | READ | WRITE | DDL | CONTROL |
| --- | --- | --- | --- | --- |
| `PROXY_ONLY` | route `LEGACY` | route `LEGACY` | reject (except migration control) | allow |
| `EMULATED_BUILD` | route `LEGACY` | route `LEGACY` | route `EMULATED` only | allow |
| `DUAL_WRITE` | route `LEGACY` | route `DUAL_WRITE` (`legacy` then `emulated`) | route `EMULATED` only | allow |
| `DUAL_READ_AUDIT` | route `DUAL_READ_COMPARE` | route `DUAL_WRITE` (`legacy` then `emulated`) | route `EMULATED` only | allow |
| `PRIMARY_EMULATED` | route `EMULATED` | route `EMULATED` | route `EMULATED` | allow |
| `MIRROR_LEGACY` | route `EMULATED` | route `MIRROR_WRITE` (`emulated` then `legacy`) | route `EMULATED` only | allow |
| `RETIRED` | route `EMULATED` | route `EMULATED` | route `EMULATED` | allow (drop/archive only) |

## Implementation Checklist

### PM00 Control Plane Binding
- [ ] Parse migration control SQL/commands defined in section 30.
- [ ] Resolve migration object by `migration_name` -> `migration_uuid`.
- [ ] Reject unknown or duplicate migration names deterministically.

Pass condition:
- Every migration control command is resolved to a single `migration_uuid`.

### PM01 Mode Snapshot Freeze
- [ ] Read `runtime_mode` and `mode_version` from `migration_job` in one snapshot read.
- [ ] Attach mode fields to request envelope.
- [ ] Reject routing if migration is not `is_valid=true`.

Pass condition:
- Routing decisions use immutable mode snapshot for request lifetime.

### PM02 Statement Classification
- [ ] Classify statement as `READ`, `WRITE`, `DDL`, or `CONTROL` using canonical classifier.
- [ ] For mixed statements, split into deterministic sub-statements and classify each.
- [ ] Reject unclassified statement forms.

Pass condition:
- Every executable unit has exactly one class.

### PM03 Route Intent Derivation
- [ ] Apply Mode Routing Matrix exactly.
- [ ] Include `route_intent` in envelope.
- [ ] Reject illegal mode/class pairs before engine dispatch.

Pass condition:
- Parser route intent matches mode matrix with no fallback behavior.

### PM04 Dual-Write Envelope Build
- [ ] For `DUAL_WRITE`, build ordered sub-operations:
  - `op1=legacy`
  - `op2=emulated`
- [ ] Attach policy from `write_policy` (`STRICT` or `LENIENT`).
- [ ] Include deterministic retry budget fields.

Pass condition:
- Dual-write intent contains deterministic order and policy.

### PM05 Dual-Read Compare Envelope Build
- [ ] For `DUAL_READ_AUDIT`, build paired read operations:
  - `read_legacy`
  - `read_emulated`
- [ ] Attach `compare_policy` and `return_source`.
- [ ] Require `statement_fingerprint` and `parameter_signature`.

Pass condition:
- Compare envelope has all fields required for audit persistence.

### PM06 Mirror-Write Envelope Build
- [ ] For `MIRROR_LEGACY`, build ordered sub-operations:
  - `op1=emulated`
  - `op2=legacy`
- [ ] Apply `mirror_policy`.
- [ ] Mark source-of-truth as `emulated`.

Pass condition:
- Mirror-write behavior is deterministic and policy-bound.

### PM07 Parser Error Mapping
- [ ] Map route derivation failure to dialect-native code:
  - mode violation -> `MIGRATION_MODE_VIOLATION`
  - stale mode version -> `MIGRATION_STALE_MODE_VERSION`
  - blocked cutover gate -> `MIGRATION_GUARD_BLOCKED`
- [ ] Preserve parser correlation id and source span.
- [ ] Never return raw internal enum ids to client.

Pass condition:
- Every migration routing failure has deterministic dialect error mapping.

### PM08 Audit Payload Assembly
- [ ] Build write audit payload for `DUAL_WRITE` and `MIRROR_LEGACY`.
- [ ] Build read audit payload for `DUAL_READ_AUDIT`.
- [ ] Include request UUID, mode version, and timing fields.

Pass condition:
- Section 24 audit tables can be written without schema inference.

### PM09 Mode Version Guard
- [ ] On dispatch, include expected `mode_version`.
- [ ] If listener/server reports version mismatch, abort and reload mode.
- [ ] Re-run PM01..PM09 once; if mismatch persists, return deterministic error.

Pass condition:
- No request executes under stale mode.

### PM10 Emulated Dialect Semantics
- [ ] Keep wire-protocol behavior 1:1 for each emulated parser.
- [ ] Allow parser-level gating: emulated parser may reject features that native parser allows.
- [ ] Never expose native-only control tokens through emulated parser unless explicitly mapped.

Pass condition:
- Emulated clients receive dialect-correct behavior while using shared migration control internals.

## Negative Requirements
- No parser-side local execution of legacy source queries.
- No parser-side bypass of listener route arbitration.
- No best-effort mode guessing when migration metadata is missing.

## Conformance Gates
- `P28-MIG-GATE-01`: PM00..PM03 pass for deterministic mode/class routing.
- `P28-MIG-GATE-02`: PM04..PM06 pass for dual-write/dual-read/mirror envelope correctness.
- `P28-MIG-GATE-03`: PM07..PM09 pass for deterministic errors and stale-mode protection.
- `P28-MIG-GATE-04`: PM10 pass for emulated parser compatibility constraints.

## Cross-Section Links
- `24_Catalog_Model_and_Virtual_Overlays/CATALOG_TABLE_SCHEMA_LIVE_MIGRATION_AND_PASSTHROUGH.md`
- `29_Listener_and_Server_Orchestration/NORMATIVE_LISTENER_PASSTHROUGH_AND_LIVE_MIGRATION_CHECKLIST.md`
- `30_Client_Tooling/NATIVE_MIGRATION_AND_PASSTHROUGH_SQL_CONTROL_CONTRACT.md`

## Audit normalization note (2026-03-28)
- This file is treated as a target-state worklist, not as present-day implementation proof.
- Current section `28` source authority is narrower and is centered on the native V3 parser stack plus the shipped Firebird, PostgreSQL, and MySQL parser-family seams.
- Broader normalization-gate, distributed-policy, passthrough, replication, connector, and fabric-parser claims require separate bounded proof before promotion.
