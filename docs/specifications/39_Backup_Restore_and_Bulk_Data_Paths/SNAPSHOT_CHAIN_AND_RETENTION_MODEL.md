# Snapshot Chain and Retention Model

## Scope

This section defines the derivative snapshot and retention chain that sits downstream of committed MGA truth.

It covers:
1. local sweep evidence work items
2. wal_after export segments
3. page-audit findings
4. shadow-capture manifests
5. retention and prune ordering

It does not redefine transaction truth. The durable database remains the authority.

## Sweep progress order

The sweep pipeline uses an explicit stage order:
1. LOCAL_EVIDENCE_PENDING
2. PAGE_AUDIT_PENDING
3. SHADOW_CAPTURE_PENDING
4. WAL_AFTER_PENDING
5. RECLAIM_PENDING

This order is normative.

Required consequence:
1. derivative evidence is emitted before destructive reclaim
2. reclaim is never allowed to outrun required evidence or required downstream capture lanes

## Local evidence work item

A local evidence work item binds sweep execution to a committed transaction lineage slice.

Required fields:
1. work_item_id
2. tx_uuid
3. txid
4. sink_profile_id
5. sweep_oit_before
6. sweep_oit_after
7. segment_seq
8. created_time
9. evidence_class
10. delivery_state
11. spool_path
12. lanes_csv

These rows are local control evidence. They support downstream export but are not recovery truth.

## Wal-after export segment

A wal_after segment is an append-only derivative export record emitted after local MGA truth is already durable.

Required fields:
1. segment_id
2. sink_profile_id
3. source_work_item_id
4. source_sink_profile_id
5. tx_uuid
6. txid
7. stream_seq
8. commit_time
9. created_time
10. sink_type
11. delivery_state
12. destination_hint
13. shipping_mode
14. statement_hashes_csv
15. segment_path

Required wal_after rules:
1. wal_after is post-commit only
2. wal_after is append-only
3. wal_after export failure is an export failure, not a recovery-state failure
4. wal_after backlog depth is observable
5. wal_after is never interpreted as write-ahead redo

## Wal-after delivery profiles

Wal-after delivery is driven by an audit sink profile with a wal_after profile kind.

Required profile rules:
1. disabled profiles are ignored
2. config_json, when present, must be valid JSON object text
3. profile_kind must resolve to SWEEP_WAL_AFTER_LOG for wal_after delivery selection
4. sink_type defaults to LOCAL_APPEND_ONLY when omitted
5. shipping_mode defaults to DEBUG when omitted

Supported sink types:
1. LOCAL_APPEND_ONLY
2. KAFKA_CHANNEL
3. REMOTE_DATABASE

Required sink-type behavior:
1. LOCAL_APPEND_ONLY
   - queue_root defaults to the local wal_after_log root when omitted
2. KAFKA_CHANNEL
   - queue_root defaults to a kafka_stage path under the local wal_after root when omitted
   - topic defaults to wal_after when omitted
3. REMOTE_DATABASE
   - database_path is required
   - remote delivery is refused when database_path is absent

## Remote database delivery boundary

REMOTE_DATABASE delivery is derivative shipping into another database file, not recovery truth propagation.

Required behavior:
1. the source database must be open
2. the target parent directory is created when necessary
3. the target database is created on first delivery when absent
4. duplicate delivery detection is based on source_work_item identity
5. remote delivery failure remains a derivative-lane failure
6. remote delivery does not convert the target database into authoritative redo state for the source

## Shadow capture manifest

A shadow capture manifest is a logical capture record emitted by the sweep worker.

Required fields:
1. manifest_id
2. tx_uuid
3. object_uuid
4. sink_profile_id
5. created_time
6. has_retention_deadline_time
7. retention_deadline_time
8. capture_scope
9. capture_format
10. payload_manifest
11. is_valid

Required manifest rules:
1. capture_scope must be valid
2. capture_format must be valid
3. at least one of tx_uuid or object_uuid must be present
4. payload_manifest is required
5. if retention_deadline_time is present it must be nonzero
6. manifest rows are immutable once written

## Temporal archive semantics

Temporal archive is a downstream history sink built from committed MGA evidence.

Normative rules:
1. The archive consumes derivative work items, wal_after segments, and logical capture manifests.
2. The archive may preserve committed and rolled-back lineage when a policy lane requires it.
3. The archive may support historical inspection of database state across time.
4. The archive does not become the source of truth for restart, recovery, or ordinary visibility decisions.
5. The live database remains queryable without rebuilding from archive streams.

## Retention and prune rules

Retention is policy-driven but must obey the following order:
1. local durable MGA truth exists
2. required derivative evidence exists
3. required page-audit or shadow-capture lanes complete or fail closed according to policy
4. reclaim eligibility is reevaluated against MGA visibility horizons
5. prune may proceed only when retention obligations are satisfied

A retention deadline may control downstream archive lifecycle. It does not authorize reclaim if MGA visibility or evidence rules still require preservation.

## Temporal archive sink classes

The derivative temporal archive may contain one or more sink classes:
1. committed lineage sink
2. rollback lineage sink
3. page-audit sink
4. shadow-capture manifest sink

Required sink rules:
1. committed lineage is the default archive class
2. rollback lineage is policy-gated and must be explicitly enabled
3. page-audit and shadow-capture sinks are supplementary observability/archive classes
4. no sink class becomes authoritative for restart or ordinary visibility

## Archive identity and ordering

Every archive row must preserve enough identity to reconstruct event order for inspection.

Minimum ordering identity:
1. transaction identity
2. commit or creation time
3. stream or segment sequence
4. sink profile identity
5. object or statement linkage when present

Ordering is for archive inspection only. It does not replace MGA visibility rules inside the live database.

## Remote temporal-archive sink profiles

Remote archive and wal_after delivery profiles are derivative sink contracts.

Required profile fields:
1. sink_profile_id
2. sink_type
3. shipping_mode
4. delivery_order_scope
5. retry_class
6. quarantine_root
7. idempotency_key_mode
8. retention_class

Required profile rules:
1. delivery_order_scope must be one of DATABASE, TABLESPACE, or PROFILE
2. idempotency_key_mode must be derived from committed source identity and may not be random per retry
3. retry_class must classify delivery as IMMEDIATE_RETRY, DEFERRED_RETRY, or QUARANTINE
4. quarantine_root is required for profiles that allow deferred retry or quarantine

## Derivative delivery state machine

Derivative archive and wal_after rows use the following state order:
1. LOCAL_DURABLE
2. READY_TO_SHIP
3. IN_FLIGHT
4. DELIVERED
5. ARCHIVED

Failure states:
1. FAILED_RETRYABLE
2. FAILED_QUARANTINED

Required transition rules:
1. LOCAL_DURABLE may be entered only after committed local MGA truth exists
2. READY_TO_SHIP may be entered only after required local evidence lanes complete
3. FAILED_RETRYABLE does not authorize prune
4. FAILED_QUARANTINED requires operator visibility and preserved identity
5. DELIVERED or ARCHIVED in a derivative sink does not change recovery authority

## Post-restore archive continuity

Restore, promotion, and failback create archive continuity boundaries.

Required rules:
1. archive sinks must preserve the boundary identity of the restore or promotion event
2. post-boundary derivative rows must not be merged into pre-boundary identity ranges without an explicit lineage marker
3. archive inspection tooling must be able to distinguish:
   a. pre-restore lineage
   b. restored live lineage
   c. post-failback lineage
4. continuity markers aid inspection only and never redefine live visibility or recovery truth

## Backpressure and retry policy

Derivative sinks must expose deterministic backpressure behavior.

Required queue-state fields:
1. queue_depth
2. oldest_pending_age
3. retryable_failure_count
4. quarantined_failure_count
5. last_delivery_success_time
6. last_delivery_failure_time

Required backpressure classes:
1. NONE
2. OBSERVE_ONLY
3. THROTTLE_DERIVATIVE_ONLY
4. FENCE_OPTIONAL_POLICY

Required rules:
1. derivative sink backlog may never silently redefine local commit behavior
2. NONE and OBSERVE_ONLY never fence commit publication
3. THROTTLE_DERIVATIVE_ONLY may slow new derivative emission but may not undo local MGA truth
4. FENCE_OPTIONAL_POLICY is allowed only where an operator-selected policy explicitly requires derivative durability alongside local durability
5. the active backpressure class must be observable

## Retry and quarantine procedure

Retry is identity-preserving.

Required rules:
1. retries reuse the original committed source identity and derivative sequence identity
2. a retryable failure remains in FAILED_RETRYABLE until a successful delivery occurs
3. quarantine preserves payload identity, failure class, and last attempted sink profile
4. quarantine release creates a new delivery attempt against the same source identity, not a new source event
5. retention or prune may not discard a quarantined row while policy still requires downstream preservation

## KAFKA_CHANNEL ordering and retry contract

`KAFKA_CHANNEL` delivery is ordered by the configured delivery scope and partition key.

Required rules:
1. the partition key must be derived from committed source identity and configured delivery_order_scope
2. all rows sharing the same delivery_order_scope key must be emitted in source sequence order
3. retries must reuse the same key and committed identity
4. downstream acknowledgment marks derivative delivery progress only
5. partition movement, rebalance, or producer retry may not be interpreted as local commit ambiguity
6. if strict in-order delivery for a configured scope cannot be maintained, the row enters retryable failure or quarantine state

## REMOTE_DATABASE ordering and retry contract

`REMOTE_DATABASE` delivery is ordered by durable source identity plus committed source sequence.

Required rules:
1. the target row identity must include source work-item identity and committed source sequence
2. duplicate delivery attempts for the same identity are idempotent and must not create multiple logical rows
3. retries must preserve the original source identity tuple
4. remote insertion success changes derivative state only; it does not redefine source recovery authority
5. if target schema or identity enforcement rejects the row, the delivery enters retryable failure or quarantine rather than inventing a new source identity
