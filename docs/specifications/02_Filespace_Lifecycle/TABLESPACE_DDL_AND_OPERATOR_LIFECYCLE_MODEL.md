# Tablespace DDL and Operator Lifecycle Model

Status: reconstructed_required_with_current_substrate

## Purpose

Define one operator-facing lifecycle for tablespace creation, attach, detach,
migrate, shrink, split, cutover, and durable lifecycle history.

## Governing rule

Beta 1 requires one explicit tablespace lifecycle state machine.

Current parser, catalog, page-manager, resolver, and executor surfaces are
implementation substrate for that lifecycle. They do not narrow the product
contract to only the paths already fully wired today.

## Lifecycle states

- `REGISTERED`: durable identity, path, and header contract exist, but the
  tablespace is not yet admitted for ordinary placement until registration/open
  steps complete
- `ATTACHED`: an external file has passed header, UUID, and page-size
  validation and is bound to catalog identity
- `ACTIVE`: the tablespace is a normal placement, allocation, and read/write
  participant
- `MIGRATING`: explicit source/target relocation is active for an object or
  partition scope and resolver metadata is published
- `CUTOVER_PENDING`: copy/rewrite validation is complete, but the final
  metadata switch and cross-filespace publication fence are still outstanding
- `DETACH_PENDING`: detach has been requested and the source is awaiting final
  emptiness proof or explicit forced migration closure
- `SHRINK_PENDING`: compaction/truncation planning is active and destructive
  file-size reduction is not yet durably published
- `SPLIT_PENDING`: a partition or object-scope split is in progress and final
  placement ownership is not yet switched
- `RETIRED`: the tablespace is no longer an ordinary placement target and is
  retained only for cleanup, evidence, or explicit operator removal
- `ABORTED_FAIL_CLOSED`: the requested lifecycle action was refused or
  interrupted and the durable history row names the refusal/rollback reason

## Required operations

### Create/register tablespace

1. Validate path, naming, page-size, and option legality.
2. Allocate durable operation id and tablespace UUID.
3. Create the file/header/FSM substrate and persist catalog identity.
4. Register/open the file with database runtime ownership.
5. Publish `ACTIVE` only after header, catalog, and runtime registration all
   succeed.

### Attach external tablespace file

1. Validate file path, header family, page size, and optional UUID
   compatibility.
2. Allocate durable operation id and register history intent.
3. Open/register the file under catalog ownership.
4. Publish `ATTACHED` and then `ACTIVE` only after validation and runtime
   registration succeed.
5. Fail closed on UUID mismatch, page-size mismatch, role conflict, or duplicate
   name/identity collision unless an explicit override rule owns that
   exception.

### Detach retired tablespace

1. Refuse detach of primary tablespace `0`.
2. Prove the scope is empty, or execute an explicit `FORCE` migration plan.
3. Flush dirty pages and movement-sensitive publication state.
4. Transition through `DETACH_PENDING` to `RETIRED`.
5. Unregister/close the file only after durable metadata no longer points at
   it.

### Migrate object or partition placement

1. Validate source and target are `ACTIVE` and page-size compatible.
2. Publish durable migration history and `MIGRATING` state.
3. Publish migration-target metadata used by resolver/runtime lookup.
4. Copy/rewrite heap, index, and oversized-value state while preserving stable
   row identity and GPID correctness.
5. Enter `CUTOVER_PENDING` only after copy/rewrite validation succeeds.
6. Perform the final metadata switch under a cross-filespace publication fence.
7. Clear migration flags and publish completion or fail-closed refusal.

### Shrink/compact an eligible tablespace

1. Validate no conflicting migration/cutover is active.
2. Publish `SHRINK_PENDING`.
3. Relocate pages/objects out of the truncation region.
4. Validate free-space, checkpoint, and publication safety.
5. Durably update headers/FSM before final truncate/compact publication.
6. Clear pending state only after the resized file and metadata are durable.

### Split/cut over partitioned placement

1. Publish durable split history and `SPLIT_PENDING`.
2. Define explicit source scope, target scope, and cutover criterion.
3. Copy/rewrite affected rows, indexes, and oversized-value state.
4. Validate completeness and backlog prerequisites.
5. Enter `CUTOVER_PENDING`.
6. Execute one explicit cutover fence and metadata switch.
7. Clear pending state or fail closed with durable refusal reason.

### Inspect durable lifecycle history and current state

The operator surface shall expose current lifecycle state plus durable history
for create, attach, alter, migrate, cutover, shrink, split, detach, refusal,
rollback, and completion events.

## Invariants

- all placement operations preserve GPID and row-identity rules or fail closed
- primary-file bootstrap authority remains distinct from secondary tablespace
  header authority
- cutover is explicit and observable
- attach/detach/split/shrink may not be inferred from lower-level page-manager
  activity
- parser/opcode surfaces are not sufficient by themselves; the lifecycle is
  complete only when dispatch, catalog, runtime registration, and history
  publication all align

## Durable history rule

Every operator lifecycle transition shall emit one durable lifecycle/history
record naming:

- operation id
- source/target tablespace
- object/partition scope
- phase state
- cutover state
- rollback/refusal reason
- initiating surface
- fence/publication status
- completion or failure timestamp

## Failure rules

1. No lifecycle transition may silently complete on partial metadata
   publication.
2. Unresolved attach/detach/cutover/shrink/split state must remain operator
   visible after restart.
3. Interrupted operations re-enter through durable history and explicit state,
   not by guessing from file shape alone.
4. Any missing prerequisite produces `ABORTED_FAIL_CLOSED`, never silent
   best-effort continuation.
