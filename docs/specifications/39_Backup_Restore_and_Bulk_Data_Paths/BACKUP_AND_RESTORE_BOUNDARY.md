# Backup and Restore Boundary

## Authority model

ScratchBird is MGA-native. Recovery and restore authority is the durable database state, not a log.

Authoritative durability inputs, in order:
1. Primary filespace page images with valid header and payload checksums.
2. Registered durable tablespace page images with the same checksum rules.
3. Transaction inventory and terminal transaction state.
4. Startup reconciliation markers, checkpoint markers, and committed catalog state.
5. Version-chain visibility rules.

Non-authoritative but useful derivative artifacts:
1. Physical shadow filespaces.
2. Sweep-emitted wal_after export segments.
3. Sweep-emitted shadow capture manifests.
4. Temporal archive sinks built from derivative export data.

Derivative artifacts may be inspected, shipped, mirrored, or archived. They are never replayed as redo and they never outrank the committed MGA page state.

## Forced-write publication order

For any transaction whose effects become durable, the engine follows this order:
1. Produce the canonical page image with finalized checksums.
2. Write the primary filespace image.
3. Write all affected durable tablespace images.
4. Mirror the already-produced page image into every active shadow filespace for the affected source filespace.
5. Execute the engine-wide sync fence so primary, tablespaces, and active shadows are forced to stable storage.
6. Publish terminal transaction state and committed visibility.
7. Only after that durable local truth exists may downstream derivative lanes emit wal_after or shadow-capture output.

A failure at steps 1 through 5 fences write admission and fails closed. The engine must not publish success while durability is uncertain.

## Shadow filespace model

Physical shadowing is derivative durability hardening.

Required operations:
1. createShadowFilespace(source_tablespace_id, shadow_path, shadow_id_out)
2. dropShadowFilespace(shadow_id, keep_file)
3. promoteShadowFilespace(shadow_id)
4. listShadowFilespaces(shadows_out)

Each shadow snapshot records:
1. shadow_id
2. source_tablespace_id
3. source_path
4. shadow_path
5. created_time
6. last_sync_time
7. copied_pages
8. mirrored_writes
9. active
10. promoted

Required shadow rules:
1. The shadow is page-for-page derivative storage for one source tablespace.
2. Shadow writes mirror already-canonicalized page images only.
3. Promotion is an operator-controlled handoff action, not automatic recovery replay.
4. A promoted shadow may be selected as an input image for operator-driven restore or failover, but it does not retroactively make shadow traffic authoritative.
5. If shadow mirroring fails, the failure is recorded and write-admission fencing rules apply where required by the durability domain.

## Shadow creation and promotion gates

Required shadow creation gates:
1. shadow_path is required
2. the database must already be open
3. the shadow path must differ from the source filespace path
4. parent directories are created before filespace creation
5. the shadow file is created with exclusive-create semantics
6. the candidate path must not already be registered as another shadow path
7. the shadow is backfilled before registration completes

Required drop gates:
1. a missing shadow id returns not found
2. a promoted shadow cannot be dropped while it is the live route
3. keep_file controls whether the physical file is retained after unregistration

Required promotion gates:
1. a missing shadow id returns not found
2. a shadow already marked promoted is idempotent
3. promotion reopens the shadow path as the replacement live fd
4. primary tablespace promotion replaces the database primary fd and path
5. non-primary promotion replaces the registered tablespace fd for that source tablespace
6. on reopen failure or route lookup failure, the prior active state is restored and promotion fails closed
7. successful promotion marks the shadow as promoted, inactive for mirroring, and updates last_sync_time

## Restore rules

Restore consumes durable database images, catalog state, and transaction inventory. Restore does not rebuild the database by replaying wal_after segments.

Allowed restore sources:
1. Primary filespace snapshot.
2. Tablespace snapshots.
3. Operator-selected promoted shadow filespace copies.
4. Consistent page-image backups captured after forced-write completion.
5. Logical backup exports only when treated as snapshot-frozen logical images and, if policy allows, optionally advanced by the derivative wal_after lane to a selected target timestamp.

Disallowed restore source of truth:
1. wal_after export stream.
2. temporal archive event stream.
3. audit export stream.
4. page-audit findings.

These derivative lanes may assist forensic comparison, completeness checks, or point inspection, but not canonical reconstruction.

## Logical versus physical backup consistency boundary

ScratchBird distinguishes logical backups from physical page backups.

Required logical-backup rules:
1. a logical backup is snapshot-frozen at the backup start boundary
2. the logical backup represents committed MGA-visible state as of that start boundary, not as of backup completion
3. the optional wal_after lane may be used to advance that logical backup to a later target timestamp
4. wal_after advancement of a logical backup is a backup-reconstruction lane, not live-engine recovery truth
5. if wal_after advancement is incomplete, the resulting logical backup remains valid only up to the last fully applied target timestamp

Required physical-backup rules:
1. a physical page backup is taken from durable page-image truth
2. a physical page backup is current as of the end-of-backup completion boundary
3. physical backup completion requires that all included page images and integrity markers correspond to the completed backup end state
4. physical page backup restore does not require wal_after advancement to reach its own completion boundary

## SQL and parser surfaces

Firebird compatibility SQL is exposed through parser procedure calls:
1. CREATE SHADOW routes to fb_create_shadow.
2. DROP SHADOW routes to fb_drop_shadow.

These commands remain fully transaction-scoped. Successful commit publishes the metadata change and immediately starts the next transaction. Rollback retires the uncommitted shadow change and immediately starts the next transaction.

## Non-guarantees

ScratchBird does not define:
1. WAL replay restore.
2. redo-log shipping as recovery truth.
3. shadow traffic as a redo stream.
4. temporal archive replay as authoritative recovery.

## Operator-driven shadow failover procedure

Shadow promotion and failover are operator-driven procedures, not automatic redo recovery.

Required procedure:
1. fence new write admission on the affected source database or tablespace set
2. verify the selected shadow snapshot is active or explicitly promoted
3. verify page/header checksums on the candidate shadow image
4. verify transaction inventory and committed catalog readability on the candidate image
5. attach the candidate image as the new restore or failover input
6. run startup reconciliation on that image
7. only then admit new write publication

Required refusal rules:
1. if checksum verification fails, failover from that shadow is refused
2. if transaction inventory or committed catalog state is unreadable, failover from that shadow is refused
3. if startup reconciliation classifies the image as fatal, failover from that shadow is refused
4. if the shadow copy is stale beyond the operator policy window, failover must either refuse or enter read-only quarantine

## Restore orchestration classes

Restore is classified into the following classes:
1. page-image restore from primary or tablespace snapshot
2. shadow-backed restore from promoted shadow copy
3. inspection restore for forensic or support analysis

Required behavior by class:
1. page-image restore may return to normal service if reconciliation succeeds
2. shadow-backed restore follows the same reconciliation gates as primary page-image restore
3. inspection restore is read-only unless explicitly reclassified after reconciliation

## Promoted-shadow reconciliation and failback

Promotion is a route handoff, not a no-op rename.

Required post-promotion reconciliation:
1. the promoted shadow becomes the current live filespace authority for its source tablespace
2. the previously live filespace is downgraded to an operator-managed candidate only
3. before ordinary write admission is widened, the engine or operator must verify:
   a. the promoted image passes header and payload checksum validation
   b. transaction inventory is readable
   c. committed catalog state is readable
   d. startup reconciliation completed without fatal classification
4. any future shadow lane for that tablespace must be recreated from the newly live promoted route

Failback is a controlled restore-style operation, not a blind fd swap.

Required failback rules:
1. failback starts by fencing new write admission on the affected filespace set
2. the candidate failback image must be validated as if it were a restore input
3. if the previous primary route diverged after promotion, direct failback to that stale image is refused
4. when failback is allowed, the current live promoted image is treated as the authoritative source for rebuilding or reattaching the replacement route
5. startup reconciliation must run on the failback target before write admission resumes
6. after successful failback, fresh shadow mirroring must be re-established explicitly

## Derivative-lane authority after restore or failover

After restore, promotion, or failback:
1. derivative wal_after, archive, and shadow-capture lanes remain downstream only
2. backlog replay in those lanes may repair derivative completeness, but may not redefine restored MGA truth
3. any derivative segment produced before the restore boundary is forensic evidence, not post-restore commit truth

## Multi-tablespace shadow-group procedures

When a database uses more than one durable tablespace, operator shadow handling may be performed as a group procedure.

Required group identity:
1. group_id
2. source database identity
3. member tablespace ids
4. member shadow ids
5. creation_time
6. last_verified_time
7. group_state

Required group states:
1. CREATING
2. ACTIVE
3. DEGRADED
4. PROMOTION_READY
5. PROMOTED
6. RETIRED

Required group rules:
1. every member shadow must be individually valid before the group may enter ACTIVE
2. a DEGRADED member prevents the group from claiming full failover readiness
3. a group may contain the primary tablespace shadow plus zero or more secondary tablespace shadows
4. group promotion may be attempted only when every required member is present and verified

## Multi-tablespace promotion order

Promotion order for a full shadow group is explicit:
1. fence new write admission for the whole affected group
2. verify every member shadow path, checksums, and route mapping
3. reopen replacement descriptors for every member before route publication
4. switch non-primary tablespace routes
5. switch the primary route
6. run reconciliation over the resulting live route set
7. admit writes only after the group is coherent

Required refusal rules:
1. if any required member shadow is missing, full-group promotion is refused
2. if any replacement descriptor reopen fails, all route publication for that group is refused
3. if reconciliation after route switch fails, the promoted group enters quarantine or read-only recovery posture
