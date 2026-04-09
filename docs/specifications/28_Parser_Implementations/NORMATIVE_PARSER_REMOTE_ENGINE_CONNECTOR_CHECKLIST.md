# Normative Checklist: Parser Remote Engine Connector Flow (Alpha)

## Purpose
Define deterministic parser behavior for native remote connector SQL/control surfaces so a low-capability model can implement request handling without inference.

## Scope
- Native parser only for remote connector statements in section 21.
- Capability gate, UUID binding, control-op mapping, and response mapping.
- Remote metadata and passthrough execution envelopes.

## Hard Invariants
1. Parser never executes remote protocol calls directly; parser emits control requests only.
2. Parser uses section-21 feature keys and section-28 capability rows for every request.
3. Parser must enforce deterministic clause parsing and option validation.
4. Parser must preserve correlation ids across all remote operations.
5. Parser must not leak restricted metadata when discoverability policy is disabled.

## Required Inputs
- Active parser profile id/version.
- Session context (`connection_uuid`, `session_uuid`, `transaction_uuid`).
- Request SQL text and normalized tokens.
- Section-24 connector metadata state.

## Required Outputs
- Canonical control envelope for one remote connector operation.
- Deterministic feature key and result shape id.
- Source map and correlation id.
- Deterministic error envelope on rejection.

## Implementation Checklist

### PR00 Family Classification
- [ ] Classify statement as `FG_FDW`.
- [ ] Resolve one feature key from section 21 remote connector SQL.
- [ ] Reject unresolved feature key with `FEATURE_NOT_REGISTERED`.

Pass condition:
- One statement -> exactly one feature key.

### PR01 Capability Gate
- [ ] Lookup `(profile_id, feature_key)` capability row.
- [ ] Apply decision (`IMPLEMENT|REMAP|REJECT`) without fallback.
- [ ] Reject missing row with `PROFILE_ENTRY_MISSING`.

Pass condition:
- Parser decision is deterministic for every request.

### PR02 Name-to-UUID Binding
- [ ] Resolve wrapper/server/user/foreign-table names to UUIDs.
- [ ] Apply discoverability-safe object lookup for non-admin sessions.
- [ ] Reject ambiguous name resolution with deterministic code.

Pass condition:
- Control envelope contains UUID identities only.

### PR03 Option Normalization
- [ ] Parse option lists in canonical key order.
- [ ] Reject unknown option keys.
- [ ] Enforce scalar type validation for each option value.

Pass condition:
- Normalized options are byte-stable for equivalent input.

### PR04 Connector Policy Preflight
- [ ] Load connector policy for execution class (`QUERY|DML|DDL|ADMIN`).
- [ ] Validate transaction mode compatibility.
- [ ] Validate capability expectations for `EXPECT CAPABILITIES`.

Pass condition:
- Rejected operations fail before control dispatch.

### PR05 Metadata Control Envelope
- [ ] Build envelopes for:
  - `F_REMOTE_ANALYZE_METADATA`
  - `F_REMOTE_REFRESH_METADATA`
  - `F_REMOTE_SHOW_CAPABILITIES`
  - `F_REMOTE_SHOW_OBJECTS`
  - `F_REMOTE_SHOW_COLUMNS`
  - `F_REMOTE_SHOW_STATISTICS`
- [ ] Include scope/filter payload in canonical field order.
- [ ] Include required timeout and bounded-work fields.

Pass condition:
- Metadata operation envelopes are complete and deterministic.

### PR06 Passthrough Execute Envelope
- [ ] Build `CTL_REMOTE_EXECUTE` envelope with:
  - operation class
  - transaction mode
  - expected capabilities
  - parameter map
  - statement text checksum
- [ ] Compute deterministic statement fingerprint.
- [ ] Attach request limits (`timeout_ms`, `max_rows`, `max_bytes`).

Pass condition:
- Execute envelope can be replayed to produce identical control payload.

### PR07 Prepared Statement Envelope
- [ ] Build `CTL_REMOTE_PREPARE`.
- [ ] Build `CTL_REMOTE_EXECUTE_PREPARED`.
- [ ] Build `CTL_REMOTE_DEALLOCATE_PREPARED`.
- [ ] Enforce statement-name uniqueness per session+connector.

Pass condition:
- Prepared lifecycle cannot enter undefined state transitions.

### PR08 Remote Transaction Envelope
- [ ] Build `CTL_REMOTE_BEGIN_TXN`, `CTL_REMOTE_COMMIT_TXN`, `CTL_REMOTE_ROLLBACK_TXN`.
- [ ] Enforce `JOIN_LOCAL` requires active local transaction.
- [ ] Reject terminal actions when no open binding exists.

Pass condition:
- Transaction binding operations are deterministic and safe.

### PR09 Response Mapping
- [ ] Map control response to declared `result_shape_id`.
- [ ] Validate column names/order for all `SHOW REMOTE *` statements.
- [ ] Reject unmapped result shape with deterministic parser error.

Pass condition:
- Same response input yields byte-identical output envelope.

### PR10 Error Mapping
- [ ] Map connector and engine error classes to dialect-native error codes.
- [ ] Keep raw remote code in diagnostics payload only.
- [ ] Never expose secrets or private endpoint fields in error text.

Pass condition:
- Same failure input maps to same parser-visible error envelope.

### PR11 Audit Binding
- [ ] Ensure each remote control request includes `request_uuid`.
- [ ] Ensure parser-provided metadata supports section-24 audit writes.
- [ ] Reject dispatch if required audit fields are missing.

Pass condition:
- All remote operations are auditable end-to-end.

### PR12 Emulated Parser Guard
- [ ] Emulated parsers must reject native-only remote SQL unless explicit profile remap exists.
- [ ] Reject code must be deterministic and dialect-correct.
- [ ] No fallback to native parser grammar in emulated parsers.

Pass condition:
- Cross-parser behavior stays profile-gated and non-leaky.

## Conformance Gates
- `P28-REMOTE-01`: PR00..PR03 parser normalization and binding.
- `P28-REMOTE-02`: PR04..PR08 control-envelope completeness and txn semantics.
- `P28-REMOTE-03`: PR09..PR10 deterministic response/error mapping.
- `P28-REMOTE-04`: PR11..PR12 audit guarantees and emulated-parser isolation.

## Cross-Section Links
- `17_Functions_and_Procedures/NORMATIVE_UDR_REMOTE_ENGINE_CONNECTOR_CHECKLIST.md`
- `21_V3_Dialect_Surface/NATIVE_UDR_REMOTE_CONNECTOR_SQL.md`
- `24_Catalog_Model_and_Virtual_Overlays/CATALOG_TABLE_SCHEMA_REMOTE_ENGINE_CONNECTOR.md`

## Audit normalization note (2026-03-28)
- This file is treated as a target-state worklist, not as present-day implementation proof.
- Current section `28` source authority is narrower and is centered on the native V3 parser stack plus the shipped Firebird, PostgreSQL, and MySQL parser-family seams.
- Broader normalization-gate, distributed-policy, passthrough, replication, connector, and fabric-parser claims require separate bounded proof before promotion.
