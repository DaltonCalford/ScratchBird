# Normative Checklist: Parser Cluster Fabric Control Surface (Alpha)

## Purpose
Define deterministic parser behavior for native SQL control of parserless ScratchBird cluster fabric links.

## Scope
- Native parser control SQL only.
- No parser involvement in fabric data-plane task execution.

## Hard Invariants
1. Parser maps cluster fabric SQL to control ops only.
2. Parser must not execute or transform fabric data-plane payloads.
3. Emulated parsers reject native cluster fabric SQL unless explicit profile remap exists.
4. Feature-key and result-shape mapping must be deterministic for every fabric control statement.

## Implementation Checklist

### PF00 Classification and Feature Mapping
- [ ] Classify cluster fabric statements in cluster-control family.
- [ ] Resolve one feature key from section-21 cluster fabric SQL contract.
- [ ] Reject unknown statements with `FEATURE_NOT_REGISTERED`.

Pass condition:
- Each statement maps to one canonical feature key.

### PF01 Capability Gate
- [ ] Apply `(profile_id, feature_key)` capability decision.
- [ ] Reject missing capability rows with `PROFILE_ENTRY_MISSING`.
- [ ] Reject unsupported emulated parser surfaces deterministically.

Pass condition:
- No implicit parser fallback behavior exists.

### PF02 UUID Binding
- [ ] Resolve `link_name` to `cluster_fabric_link_uuid`.
- [ ] Resolve optional user/role/group/schema identifiers to UUIDs.
- [ ] Apply discoverability-safe lookup rules.

Pass condition:
- Control envelopes are UUID-bound and policy-safe.

### PF03 Option and Clause Validation
- [ ] Enforce strict clause presence/order rules.
- [ ] Reject unknown option keys and invalid enum labels.
- [ ] Enforce `EXPECT VERSION` requirements on mutable state/policy commands.

Pass condition:
- Parser normalization is deterministic and non-heuristic.

### PF04 Session and Task Control Envelope
- [ ] Build control payload for session open/close commands.
- [ ] Build control payload for task submit/cancel commands.
- [ ] Validate `PASSTHROUGH_SBLR_EXECUTE` requires payload UUID reference.

Pass condition:
- Session and task control payloads are complete and deterministic.

### PF05 Response Mapping
- [ ] Map all `SHOW CLUSTER FABRIC *` responses to fixed `RS_ROWSET` schemas.
- [ ] Validate fixed column order and types.
- [ ] Reject mismatched response shapes deterministically.

Pass condition:
- Output envelopes are deterministic and schema-stable.

## Conformance Gates
- `P28-FABRIC-01`: PF00..PF01 feature/capability gate correctness.
- `P28-FABRIC-02`: PF02..PF04 binding and control-envelope correctness.
- `P28-FABRIC-03`: PF05 response-shape determinism and emulated parser rejection rules.

## Cross-Section Links
- `21_V3_Dialect_Surface/NATIVE_CLUSTER_FABRIC_SQL.md`
- `24_Catalog_Model_and_Virtual_Overlays/CATALOG_TABLE_SCHEMA_SB_CLUSTER_FABRIC.md`
- `29_Listener_and_Server_Orchestration/NORMATIVE_SERVER_CLUSTER_UDR_FABRIC_CHECKLIST.md`

## Audit normalization note (2026-03-28)
- This file is treated as a target-state worklist, not as present-day implementation proof.
- Current section `28` source authority is narrower and is centered on the native V3 parser stack plus the shipped Firebird, PostgreSQL, and MySQL parser-family seams.
- Broader normalization-gate, distributed-policy, passthrough, replication, connector, and fabric-parser claims require separate bounded proof before promotion.
