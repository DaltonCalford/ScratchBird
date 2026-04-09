# Native Forensic Replay and Audit Control Surface

## Purpose
Define the canonical native SQL and approved tooling control surface for replay,
audit policy management, sink profiles, and schema-history inspection.

## Scope
- replay invocation verbs
- retention-policy and sink-profile management
- schema-history inspection verbs
- replay/export inspection and control
- tooling/CLI mapping requirements

## Hard Invariants
1. Native control surface manages replay and audit policy; emulated clients do not automatically inherit these commands.
2. Replay control never creates a non-transactional query mode.
3. Control commands must map 1:1 to authoritative catalog/state transitions.
4. Scope and privilege checks are deterministic and auditable.

## Canonical Command Families
### Replay Session and Query Control
Supported native command families:
1. `OPEN FORENSIC REPLAY ...`
2. `CLOSE FORENSIC REPLAY`
3. `SHOW FORENSIC REPLAY STATUS`
4. `EXPLAIN FORENSIC REPLAY BOUNDARY ...`

Selectors supported by `OPEN FORENSIC REPLAY`:
- `TX UUID <uuid>`
- `TXID <u64>`
- `COMMIT SEQNO <u64>`
- `CAPSULE UUID <uuid>`
- `AT TIMESTAMP <utc-ts>`

Optional clauses:
- `SCOPE DATABASE|SCHEMA|OBJECT ...`
- `PAYLOAD SUMMARY|DETAILED|DDL_HISTORY`
- `EXPECT SCHEMA EPOCH <uuid>`

### Audit and Retention Policy Control
1. `CREATE AUDIT SINK PROFILE`
2. `ALTER AUDIT SINK PROFILE`
3. `DROP AUDIT SINK PROFILE`
4. `CREATE RETENTION POLICY`
5. `ALTER RETENTION POLICY`
6. `DROP RETENTION POLICY`
7. `BIND RETENTION POLICY TO TABLE|SCHEMA|OBJECT`
8. `UNBIND RETENTION POLICY ...`

### Schema History and Audit Inspection
1. `SHOW SCHEMA HISTORY ...`
2. `SHOW DDL LINEAGE ...`
3. `SHOW AUDIT SEGMENTS ...`
4. `SHOW SHADOW CAPTURES ...`
5. `SHOW PAGE AUDIT FINDINGS ...`
6. `SHOW DERIVATIVE DELIVERY STATUS ...`
7. `SHOW SHADOW GROUP STATUS ...`
8. `SHOW RESTORE BOUNDARY STATUS ...`

### Replay Export Control
1. `EXPORT FORENSIC REPLAY ...`
2. `VALIDATE FORENSIC EXPORT ...`
3. `SHOW FORENSIC EXPORT STATUS ...`

### Derivative and Shadow Operational Control
1. `SHOW DERIVATIVE QUARANTINE ...`
2. `RETRY DERIVATIVE DELIVERY ...`
3. `SHOW SHADOW GROUP STATUS ...`
4. `PROMOTE SHADOW GROUP ...`
5. `SHOW FAILBACK STATUS ...`

## Result Contracts
1. `SHOW FORENSIC REPLAY STATUS` returns fixed columns:
   - `replay_session_uuid`
   - `replay_tx_uuid`
   - `selector_kind`
   - `resolved_tx_uuid`
   - `resolved_txid`
   - `resolved_commit_seqno`
   - `resolved_schema_epoch_uuid`
   - `payload_class`
   - `status`
2. `SHOW SCHEMA HISTORY` returns fixed columns:
   - `schema_epoch_uuid`
   - `origin_tx_uuid`
   - `origin_txid`
   - `commit_seqno`
   - `created_time`
3. `SHOW DDL LINEAGE` returns fixed columns:
   - `ddl_event_uuid`
   - `tx_uuid`
   - `operation_class`
   - `schema_epoch_before_uuid`
   - `schema_epoch_after_uuid`
   - `object_uuid`
   - `created_time`
4. `SHOW DERIVATIVE DELIVERY STATUS` returns fixed columns:
   - `sink_profile_id`
   - `sink_type`
   - `delivery_order_scope`
   - `queue_depth`
   - `oldest_pending_age`
   - `retryable_failure_count`
   - `quarantined_failure_count`
   - `backpressure_class`
5. `SHOW SHADOW GROUP STATUS` returns fixed columns:
   - `group_id`
   - `group_state`
   - `required_member_count`
   - `ready_member_count`
   - `last_verified_time`
6. `SHOW RESTORE BOUNDARY STATUS` returns fixed columns:
   - `boundary_id`
   - `boundary_kind`
   - `created_time`
   - `source_route_identity`
   - `target_route_identity`

## Tooling Mapping Rules
1. Approved admin tooling and drivers must expose replay control through deterministic wrappers over the native control surface.
2. CLI surface may provide ergonomic aliases, but alias behavior must map 1:1 to the native contract.
3. Emulated shells may expose replay inspection only when explicitly profile-mapped; native replay DDL/admin verbs are hidden by default.
4. Tooling must render derivative delivery and shadow-group state without collapsing them into generic recovery status.

## Deterministic Error Classes
- `SB-FOR-0001` replay selector invalid
- `SB-FOR-0002` replay scope denied
- `SB-FOR-0003` history unavailable
- `SB-FOR-0004` schema history unavailable
- `SB-FOR-0005` sink profile invalid
- `SB-FOR-0006` retention policy binding invalid
- `SB-FOR-0007` export request denied

## Test Contract
Required tests:
1. each replay selector family parses and persists deterministic replay state
2. `SHOW` surfaces return fixed column order and value typing
3. sink-profile and retention-policy verbs map 1:1 to authoritative catalog rows
4. schema-history inspection returns deterministic epoch/DDL lineage results
5. export control failures map to stable `SB-FOR-XXXX` errors
6. derivative delivery and shadow-group `SHOW` surfaces preserve fixed column order, stable typing, and the MGA-versus-derivative distinction

## Operational distinction rule

The native control surface must preserve a three-way distinction:
1. local MGA durability health
2. derivative delivery health
3. shadow-group readiness or degradation

It is non-conforming to collapse these into one generic `recovery_status`.

## Cross-Section References
- `21_V3_Dialect_Surface/NATIVE_ADMIN_LANGUAGE_DEFINITION.md`
- `26_Native_Wire_Protocol/FORENSIC_REPLAY_SESSION_PROFILE.md`
- `27_Native_Handshake/FORENSIC_REPLAY_CONNECTION_NEGOTIATION.md`
- `24_Catalog_Model_and_Virtual_Overlays/CATALOG_TABLE_SCHEMA_FORENSIC_AUDIT_AND_SHADOW_CAPTURE.md`

## 2026-03-28 Audit Normalization Update

- Section `30` is normalized to the code-backed `partial` standard.
- Current authority is bounded to the shipped `ScratchBird-driver` surfaces, especially `tracks/p3/drivers/*`, shared connectivity docs, and the concrete CLI/runtime seams.
- Direct native and manager-proxy are the current portable client contract.
- Local runtime modes such as `embedded` and `local-ipc` are bounded tooling/runtime surfaces, not universal parity claims for every maintained language driver.
- The C/C++ lane in the current driver repo is intentionally IP-only; current CLI `embedded` mode is routed through local IPC in the present beta C++ runtime.
- Tool command truth is bounded to the shipped `sb_isql`, `sb_admin`, `sb_backup`, `sb_security`, `sb_verify`, and `sbdriver-conformance` surfaces.
- Recovery language follows MGA/session-repair rules and explicitly excludes WAL-style transaction replay.
- Forensic replay, migration/passthrough, and replication control narratives remain bounded, checklist-only, or target-state-only unless a shipped lane-local control surface is proven.
- Driver-lane claims must stay tied to the current maintained lane set and must not assume universal cross-language parity from section-outline text alone.

## 2026-03-28 Hardening Promotion Update

- Section `30` now carries explicit bounded authority for current maintained `ScratchBird-driver` `p3` lanes.
- Embedded and linked-library language is bounded by the current IP-only C/C++ lane plus tool-local `embedded` or `local-ipc` seams.
- Direct native and manager-proxy remain the current portable client baseline.
- CLI authority is bounded to shipped `sb_isql`, `sb_admin`, `sb_backup`, `sb_security`, `sb_verify`, and `sbdriver-conformance`.
- Error and reconnect language is bounded to deterministic MGA/session repair and explicitly excludes whole-transaction replay.
- Installer, replay, migration, passthrough, and replication client-control claims remain bounded or `target_state_only` unless maintained lane-local proof is promoted.
