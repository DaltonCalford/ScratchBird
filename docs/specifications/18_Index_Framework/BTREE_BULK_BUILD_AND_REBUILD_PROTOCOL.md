# B-tree Bulk Build and Rebuild Protocol

## Purpose
Define crash-safe bulk build, rebuild, validation, and root cutover for B-tree
creation and maintenance.

## Scope
- build phases
- persistent build-state markers
- validation and publication
- interruption and restart semantics

## Hard Invariants
1. An interrupted build may not publish a corrupt or partially linked tree.
2. Root cutover is metapage-driven and durable.
3. Validation is mandatory before publication.

## Build States
- `INIT`
- `SPOOLING`
- `LEAF_LOAD`
- `UPPER_LOAD`
- `VALIDATING`
- `PUBLISHING`
- `COMPLETE`
- `ABORTED`

The current state is stored in the metapage and build control record.

## Protocol
1. create metapage with `INIT`
2. build spool and sort visible entries
3. load leaf pages
4. build upper levels and provisional root
5. run structural validation
6. publish root and height through metapage
7. mark `COMPLETE` and release old pages to quarantine if this is rebuild

## Restart Behavior
On restart:
- `INIT` and `SPOOLING` builds are discarded
- `LEAF_LOAD` and `UPPER_LOAD` builds are discarded unless validation can prove
  the build root is complete
- `VALIDATING` resumes validation or discards the build
- `PUBLISHING` completes or repairs metapage publication using the SMO protocol

## Metrics
Required build metrics:
- pages built per phase
- duplicates coalesced during build
- validation failures
- publication latency

## Acceptance Criteria
- interrupted build never publishes a corrupt tree
- completed build validates before publication
- bulk build scaling is measurable after sort phase

## Cross-Section References
- `BTREE_PERSISTENT_METADATA_AND_ROOT_MANAGEMENT.md`
- `BTREE_STRUCTURAL_MODIFICATION_DURABILITY_PROTOCOL.md`
- `INDEX_BUILD_AND_MAINTENANCE.md`

## Legacy Mapping
| Historical source | Material preserved here |
| --- | --- |
| `specifications_old/indexes/BTREE_SPEC.md` | offline bulk build refined with state markers and durable cutover |

## Gap Closure Mapping
- `SB-BTR-009`
