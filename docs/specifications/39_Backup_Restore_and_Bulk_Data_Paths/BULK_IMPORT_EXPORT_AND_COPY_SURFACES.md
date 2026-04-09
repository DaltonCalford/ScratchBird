# Bulk Import Export and Copy Surfaces

## Scope

Section 39 bulk surfaces include operator-driven copy, export, mirror, and archive lanes. These lanes are divided into authoritative data movement and derivative evidence movement.

## Authoritative versus derivative movement

Authoritative movement:
1. page-image backup
2. filespace copy taken after forced-write completion
3. restore from durable page images
4. operator-driven shadow promotion followed by controlled attach or restore

Derivative movement:
1. wal_after export segments
2. shadow capture manifests
3. support-bundle and audit export summaries
4. temporal archive shipping
5. optional logical-backup rollforward

Derivative movement may be used for replication, archive, audit, or support workflows. It is not used as redo or restart authority.

## Bulk import and copy rules

All import and copy activity remains transaction-scoped.

Normative rules:
1. ScratchBird is always in a transaction.
2. DDL and DML obey the same transaction rules during bulk movement.
3. Autocommit commits after successful statements only.
4. Statement failure leaves the current transaction active and uncommitted.
5. Bulk import may not bypass MGA visibility, commit publication, forced-write, or checksum rules.

## Export and replication boundary

Wal_after export exists for downstream shipping and replication.

Required rules:
1. export happens after committed local durability
2. export failure does not rewrite local recovery truth
3. export backlog and failures are observable operator metrics
4. downstream consumers must treat the export as derivative lineage, not redo
5. wal_after selection is profile-driven and limited to the supported sink types in the snapshot-chain contract
6. remote database delivery is a shipping target, not a recovery journal
7. optional use of wal_after to advance a logical backup applies only to the logical-backup advancement lane and does not make wal_after live restart authority

## Logical versus physical backup export boundary

Required rules:
1. logical backup export is snapshot-frozen at backup start
2. physical page backup export is current as of backup completion
3. optional wal_after advancement may move a logical backup forward to a selected target timestamp
4. physical page backup does not require wal_after advancement to be current for its own completion boundary

## Shadow copy boundary

Shadow filespaces are physical mirror copies.

Required rules:
1. shadow copy is page-for-page derivative mirroring
2. shadow copy happens after canonical page-image production
3. shadow copy never licenses read-write divergence from the source tablespace
4. shadow promotion is explicit and operator-controlled
5. shadow promotion swaps the live filespace route only after the replacement fd is successfully opened
6. promoted shadows leave the mirror lane and become the operator-selected live route

## Temporal archive boundary

Temporal archive receives committed lineage and optional policy-selected rolled-back lineage.

Required rules:
1. archive records preserve transaction identity and timing
2. archive transport does not redefine current transaction visibility
3. archive playback may be used for forensic inspection only when explicitly invoked
4. live restart and ordinary recovery never depend on archive playback
5. archive sinks may be local append-only, staged message-channel, or remote database delivery, but all remain derivative lanes

## Operator visibility

The executor support-bundle summary exposes derivative-lane counts, including:
1. shadow_capture_manifest_count
2. wal_after_segment_count
3. audit_export_segment_count

These counters are required observability outputs for section 39 operations.

## Derivative-lane failure handling

Derivative-lane failures are classified separately from local durability failures.

Required classifications:
1. local durability failure
2. shadow mirror failure
3. wal_after export failure
4. archive sink delivery failure

Required handling:
1. local durability failure fences publication and fails the transaction or maintenance action
2. shadow mirror failure is recorded and handled according to the active durability policy domain
3. wal_after export failure increments export failure counters and backlog depth, but does not rewrite local transaction truth
4. archive sink delivery failure is observable and retryable, but does not convert archive into recovery authority

## Copy and export refusal rules

The engine must refuse copy or export claims that would blur MGA truth boundaries.

The following are non-conforming:
1. treating wal_after as write-ahead redo
2. treating temporal archive as restart authority
3. reporting a derivative export as successful local commit publication
4. allowing bulk copy paths to bypass forced-write or checksum finalization

## Remote sink delivery contract

Remote sink delivery is a post-commit shipping concern.

Required rules:
1. every shipped unit carries committed source identity, sink profile identity, and delivery sequence
2. retries reuse the same committed identity tuple
3. sink acknowledgment updates derivative delivery state only
4. sink acknowledgment may not retroactively declare a local transaction committed
5. if a remote sink duplicates already-delivered identity, delivery is treated as idempotent acceptance or safe no-op

### `KAFKA_CHANNEL`

Required behavior:
1. the emitted record key is derived from committed source identity and configured ordering scope
2. records sharing the same ordering scope must preserve source sequence order
3. retry may re-emit the same committed identity, but must not invent a new one
4. acknowledgment is transport progress only

### `REMOTE_DATABASE`

Required behavior:
1. the target schema must enforce identity uniqueness on the source identity tuple
2. successful replay of the same identity is idempotent
3. target-side rejection enters retry or quarantine state
4. remote durability does not upgrade the derivative lane into restart authority

### Sink-specific refusal rules

The engine must refuse:
1. `KAFKA_CHANNEL` delivery that cannot preserve the declared ordering scope
2. `REMOTE_DATABASE` delivery that lacks stable identity uniqueness or target schema compatibility
3. any sink configuration that would require treating downstream acknowledgment as local commit proof

## Operator failback runbook boundary

The failback runbook is part of section 39 because it governs how derivative shadow tooling interacts with authoritative restore inputs.

Required operator runbook steps:
1. freeze new write admission for the affected source routes
2. capture the current promoted-live identity and derivative backlog position
3. validate the candidate failback image using the same checksum and reconciliation gates used for restore
4. refuse blind failback to a stale route that no longer matches current committed lineage
5. attach or rebuild the failback target from authoritative page-image truth
6. run reconciliation before reopening write admission
7. recreate derivative shadow and archive profiles after the authoritative route is stable again

## Derivative-lane backpressure runbook

Operators must manage derivative backlog without confusing it with recovery truth.

Required operator procedure:
1. inspect queue depth, oldest pending age, and failure class for every active derivative profile
2. distinguish local durability health from derivative shipping health
3. if derivative backlog exceeds policy limits, choose one of:
   a. observe only
   b. throttle derivative generation
   c. enter policy-driven fenced mode when configured
4. if quarantine is used, preserve identity and sequence continuity before replay
5. after recovery of the sink, resume shipping from the oldest preserved committed identity

## Multi-tablespace shadow fleet runbook

When shadows protect multiple tablespaces, operators manage them as a fleet.

Required runbook steps:
1. verify every member shadow is mapped to the correct source tablespace id
2. verify every member has current sync evidence
3. classify the fleet as ACTIVE, DEGRADED, or PROMOTION_READY
4. refuse full-group promotion while any required member is DEGRADED or missing
5. after promotion or failback, rebuild the shadow fleet from the new live route set
