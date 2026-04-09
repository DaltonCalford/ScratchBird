# Sweep Archive and Retention Policy (MGA)

## Purpose
Define how sweep/GC transitions old committed versions and lineage evidence to archive storage while preserving deterministic replay and provenance guarantees.

## Scope
- Retention classes and eligibility.
- Sweep-to-archive transfer flow.
- Verification, integrity, and legal-hold controls.
- Post-archive pruning from primary storage.

## Hard Invariants
1. Active/visible versions are never archived or removed.
2. Archive transfer is verification-gated before prune.
3. Archive data is immutable after commit.
4. Lineage/replay references remain resolvable after archival.
5. Retention policy changes are non-retroactive unless explicitly approved.
6. Archive policy does not alter MGA visibility rules.

## Retention Classes
| Class | Typical Scope | Default Behavior |
| --- | --- | --- |
| `SHORT` | high-churn operational tables | retain recent windows, archive quickly |
| `STANDARD` | default application objects | balanced retention and archive |
| `LONG` | regulated/compliance objects | extended retention before archive |
| `IMMUTABLE` | legal hold / forensic | no prune allowed |

Retention class binds to table/object policies and lineage channels.

## Eligibility Algorithm
A record/version is archive-eligible only when all are true:
1. creating/deleting tx states are terminal and below archive horizon.
2. version is not visible to transactions at or above OAT.
3. no open lock/hold references.
4. object retention class allows archival.
5. legal hold policy does not block action.

## Archive Pipeline
1. Select eligible candidates by filespace/object shard.
2. Create immutable archive batch manifest.
3. Export versions + lineage references in deterministic key order.
4. Include transactional DDL lineage and schema-epoch references when policy
   requires historical schema replay.
5. Compute checksums for payload and manifest.
6. Validate archive write and checksum integrity.
7. Persist archive commit marker.
8. Mark primary copies as prune-eligible.
9. Prune primary copies in bounded chunks.
10. Persist prune completion event.

If any step fails before archive commit marker:
- mark batch failed,
- keep primary data untouched,
- emit deterministic failure class.

## Legal Hold Semantics
1. Objects/transactions under legal hold are excluded from prune.
2. Legal-hold scope may still permit duplicate archive copy for durability.
3. Hold release is explicit and auditable.
4. Retroactive purge under hold is forbidden.

## Integrity and Replay Requirements
1. Every archive batch has `manifest_hash` and `payload_hash`.
2. Archive batches chain to prior batch hash per object scope.
3. Replay tools must verify hash chain before restore/replay.
4. Missing hash links are hard failures.

## Restore and Replay Contract
1. Archived versions can be materialized for replay without mutating current primary state.
2. Replay reads from archive must preserve original tx ordering.
3. Restored lineage references must match original checksums.

## Deterministic Error Classes
| Code | Condition |
| --- | --- |
| `ARCHIVE_ELIGIBILITY_POLICY_BLOCK` | retention/legal hold blocks candidate |
| `ARCHIVE_WRITE_FAILED` | archive storage write failure |
| `ARCHIVE_CHECKSUM_MISMATCH` | post-write integrity mismatch |
| `ARCHIVE_COMMIT_MARKER_MISSING` | batch incomplete; prune forbidden |
| `ARCHIVE_PRUNE_BLOCKED` | prune attempted before verified archive commit |
| `ARCHIVE_CHAIN_BROKEN` | archive hash chain integrity failure |

## Required Metrics
1. candidate count and archive throughput by class.
2. archive lag (oldest eligible age).
3. failed batch count by error class.
4. prune backlog size.
5. legal-hold blocked candidate count.

## Test Contract
Required tests:
1. eligibility correctness vs OIT/OAT and lock states.
2. archive-before-prune enforcement.
3. failed transfer rollback behavior.
4. hash chain integrity validation.
5. legal hold enforcement and release behavior.
6. replay from archived versions with deterministic outcomes.
7. retained schema history survives archive/prune while replay window remains
   open.

## Cross-Section References
- `10_GC_and_Sweep/GC_SWEEP_ALGORITHM.md`
- `08_Transaction_Core/TRANSACTION_LINEAGE_AND_PROVENANCE_MODEL.md`
- `20_Diagnostics_Audit_and_Observability/STORAGE_METRICS.md`
- `31_Conformance_Performance_and_Reliability_Gates/EVIDENCE_ARTIFACTS_AND_REPLAY_REQUIREMENTS.md`

## Code-backed audit addendum (2026-03-27)

### Status
`current_authority_with_reconstructed_expansion`

### Main finding
The reviewed code proves retained lineage and retained evidence metadata, immutable shadow-capture manifests with optional retention deadlines, immutable forensic snapshot capsules with mandatory retention deadlines and optional archive locators, and support-bundle surfaces that expose retained sweep evidence. It does not prove the broad archive-transfer and verified archive-before-prune pipeline described in the current prose.

### Implementation code map
- Shadow-capture manifest retention fields: `ScratchBird/include/scratchbird/core/catalog_manager.h:4785`
- Forensic snapshot capsule retention and archive locator fields: `ScratchBird/include/scratchbird/core/catalog_manager.h:4819`, `ScratchBird/include/scratchbird/core/catalog_manager.h:4820`, `ScratchBird/include/scratchbird/core/catalog_manager.h:4821`
- Forensic snapshot capsule catalog APIs: `ScratchBird/include/scratchbird/core/catalog_manager.h:8497`, `ScratchBird/include/scratchbird/core/catalog_manager.h:8499`
- Retained transaction lineage catalog root: `ScratchBird/include/scratchbird/core/catalog_manager.h:13576`
- Immutable transaction-lineage append rules: `ScratchBird/src/core/catalog_manager.cpp:64928`, `ScratchBird/src/core/catalog_manager.cpp:64950`, `ScratchBird/src/core/catalog_manager.cpp:65017`, `ScratchBird/src/core/catalog_manager.cpp:65023`, `ScratchBird/src/core/catalog_manager.cpp:65035`, `ScratchBird/src/core/catalog_manager.cpp:65080`, `ScratchBird/src/core/catalog_manager.cpp:65182`
- Shadow-capture retention validation and persistence: `ScratchBird/src/core/catalog_manager.cpp:65810`, `ScratchBird/src/core/catalog_manager.cpp:65813`, `ScratchBird/src/core/catalog_manager.cpp:65859`, `ScratchBird/src/core/catalog_manager.cpp:65862`, `ScratchBird/src/core/catalog_manager.cpp:65903`, `ScratchBird/src/core/catalog_manager.cpp:65935`
- Forensic snapshot capsule append and immutability: `ScratchBird/src/core/catalog_manager.cpp:65968`, `ScratchBird/src/core/catalog_manager.cpp:66015`, `ScratchBird/src/core/catalog_manager.cpp:66018`, `ScratchBird/src/core/catalog_manager.cpp:66055`, `ScratchBird/src/core/catalog_manager.cpp:66074`, `ScratchBird/src/core/catalog_manager.cpp:66075`, `ScratchBird/src/core/catalog_manager.cpp:66076`
- Forensic snapshot capsule readback: `ScratchBird/src/core/catalog_manager.cpp:66111`, `ScratchBird/src/core/catalog_manager.cpp:66159`, `ScratchBird/src/core/catalog_manager.cpp:66160`, `ScratchBird/src/core/catalog_manager.cpp:66162`
- Support-bundle retained sweep artifact counts: `ScratchBird/src/core/support_bundle_builder.cpp:679`, `ScratchBird/src/core/support_bundle_builder.cpp:687`, `ScratchBird/src/core/support_bundle_builder.cpp:702`, `ScratchBird/src/core/support_bundle_builder.cpp:719`, `ScratchBird/src/core/support_bundle_builder.cpp:816`, `ScratchBird/src/core/support_bundle_builder.cpp:818`, `ScratchBird/src/core/support_bundle_builder.cpp:820`

### Current status
- Retention metadata is real.
- Shadow-capture manifests may carry retention deadlines.
- Forensic snapshot capsules require retention deadlines and may carry archive locator UUIDs.
- Transaction lineage rows are immutable and contiguous, which materially supports retained replay provenance.
- Support-bundle output exposes counts for retained shadow manifests, page-audit findings, and derivative `wal_after` segments.

### Non-blocking expansion candidates
- This pass did not prove a real archive batch pipeline, archive commit marker, archive hash chain, or archive-before-prune enforcement subsystem.
- This pass did not prove legal-hold semantics as a distinct runtime surface.
- This pass did not prove replay directly from archived versions; it proved retained forensic and lineage metadata instead.
- The current code-backed truth is closer to retained evidence and forensic-capsule metadata than to a full MGA archive tier.

### Suggestions
- Narrow the current canonical spec to retained evidence and forensic retention surfaces unless a real archive-transfer subsystem is implemented.
- Split “retention metadata and forensic capsules” from “archive transfer and replay from archive” so low-capability implementers do not assume the latter already exists.
- Make transaction-lineage immutability and contiguous event-sequence rules first-class in this spec because they are currently stronger than any proven archive-chain implementation.
- Treat archive locator support as metadata-carried optional linkage, not proof of a full archive store.

### Known contradictions and drift
- The spec claims a verified archive-before-prune pipeline; the reviewed code proves retention metadata and immutable evidence catalogs more strongly than archive transfer.
- The spec claims archive hash chaining and commit markers; those were not proven in this pass.
- The spec frames archived-version replay as implemented; the reviewed code more clearly proves forensic capsules and lineage preservation than a general archive replay path.
