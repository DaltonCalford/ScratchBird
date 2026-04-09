# Reclaim Legality Authority and Decision Vocabulary

Status: reconstructed_required_with_current_substrate

## Purpose

Define one engine-owned reclaim legality contract so reclaim behavior can be
implemented or modified without guessing which subsystem owns the decision.

## Governing rule

This file is the authoritative reclaim-decision contract for Beta 1.

Current code already contains reclaim substrate in `HeapPage`, `SweepManager`,
and `StorageEngine`, but that distributed substrate does not weaken the
requirement for one shared reclaim vocabulary and one explicit decision surface.

## Current substrate

- `HeapPage` performs maturity scan, prune, reclaim, and dead-TID collection
- `SweepManager` performs foreground reclaim orchestration and progress
  persistence
- `StorageEngine` cleanup paths already reuse heap reclaim primitives
- index-family cleanup remains family-local, but reclaim legality is not

## Hard invariants

1. MGA transaction truth remains the only source of reclaim eligibility truth.
2. Reclaim legality is decided once and reused across sweep and storage-engine
   cleanup paths.
3. Cursor persistence is progress state, not reclaim authority.
4. Reclaim publication may not outrun required index cleanup.
5. Repair-state or restart-generation conflicts must return explicit defer or
   reject outcomes.

## Required control surface

### Required input type

```cpp
struct ReclaimLegalityInput {
    uint64_t sweep_generation;
    uint64_t checkpoint_generation;
    uint64_t horizon_oit;
    uint64_t horizon_oat;
    uint64_t horizon_ost;
    uint64_t restart_generation;
    bool repair_state_active;
    bool strict_audit;
    bool requires_index_cleanup;
    bool is_foreground_reclaim;
    TID stable_tid;
    uint32_t page_id;
    uint16_t slot_id;
};
```

### Required decision kind

```cpp
enum class ReclaimDecisionKind : uint8_t {
    RECLAIM_NOW,
    PRUNE_ONLY,
    DEFER_VISIBLE_HISTORY,
    DEFER_REPAIR_STATE,
    DEFER_RESTART_MISMATCH,
    DEFER_INDEX_BACKLOG,
    REJECT_INVALID_INPUT
};
```

### Required decision result

```cpp
struct ReclaimDecision {
    ReclaimDecisionKind kind;
    bool requires_dead_tid_collection;
    bool requires_progress_persist;
    bool may_compact_page;
    bool may_publish_reclaim;
    const char* reason_code;
};
```

### Required entry points

```cpp
auto classifyReclaimLegality(const ReclaimLegalityInput& in,
                             ReclaimDecision* out,
                             ErrorContext* ctx = nullptr) -> Status;

auto collectReclaimPrerequisites(const ReclaimLegalityInput& in,
                                 std::vector<TID>* dead_tids_out,
                                 ErrorContext* ctx = nullptr) -> Status;

auto validateReclaimPublicationOrder(const ReclaimDecision& decision,
                                     bool index_cleanup_complete,
                                     bool cursor_persisted,
                                     ErrorContext* ctx = nullptr) -> Status;
```

## Normative reason codes

The implementation must expose or map to these canonical reason codes:

- `RECLAIM_VISIBLE_HISTORY`
- `RECLAIM_REPAIR_STATE_ACTIVE`
- `RECLAIM_RESTART_GENERATION_MISMATCH`
- `RECLAIM_INDEX_BACKLOG_REQUIRED`
- `RECLAIM_PRUNE_ONLY`
- `RECLAIM_SLOT_RECLAIM_NOW`
- `RECLAIM_INVALID_INPUT`

## Normative behavior

1. Validate required generation and horizon inputs.
2. If repair state is active, return `DEFER_REPAIR_STATE`.
3. If restart generation is incompatible with current reclaim context, return
   `DEFER_RESTART_MISMATCH`.
4. Run one shared maturity classification.
5. If visible, retained, prepared, or uncertain history remains, return
   `DEFER_VISIBLE_HISTORY`.
6. If index cleanup is required, either:
   - return `PRUNE_ONLY` with `requires_dead_tid_collection=true`, or
   - return `DEFER_INDEX_BACKLOG`
7. Return `RECLAIM_NOW` only when reclaim is publication-safe.
8. When the decision requires progress persistence, `SweepManager` must persist
   progress before moving to the next reclaim checkpoint.
9. Storage-engine cleanup and sweep cleanup must produce the same decision for
   the same effective input.

## Publication-order rules

1. `PRUNE_ONLY` may compact or mark local reclaim intent, but it may not publish
   reclaim until index cleanup prerequisites are complete.
2. `RECLAIM_NOW` may publish reclaim only after page-image durability and
   free-space publication ordering both succeed.
3. `DEFER_*` and `REJECT_INVALID_INPUT` never permit reclaim publication.

## Required metrics and observability

- reclaim decision count by `reason_code`
- deferred reclaim count by cause
- rejected reclaim count by cause
- index-backlog-required count
- restart-mismatch defer count

## Required tests

- same input yields the same decision for sweep and storage-engine cleanup
- visible history always defers reclaim
- repair-state input always defers reclaim
- restart-generation mismatch never silently reclaims
- dead-TID prerequisite blocks reclaim publication until cleanup completes
- foreground reclaim persists progress when the decision requires it

## Proof obligations

- prove that `HeapPage`, `SweepManager`, and `StorageEngine` stop inventing
  separate reclaim decision vocabularies
- prove that index-cleanup prerequisites are explicit before reclaim
  publication
- prove that restart and repair-state defers are explicit and fail closed
- prove that metrics, tests, and operational reporting use the same decision
  vocabulary

## Implementation note

The runtime may temporarily realize this contract through multiple internal call
sites while the code is being aligned. That is implementation drift. The
section-owned behavior, audit vocabulary, and operator-visible semantics remain
the single contract defined here.
