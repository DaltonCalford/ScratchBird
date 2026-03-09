# Specification: Tuple Versions

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | storage |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird Alpha |
| **Authors** | Dalton Calford |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/heap_page.h:91`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_tuple_versions.cpp`

## Synopsis

This specification defines tuple versioning in ScratchBird's Multi-Generational Architecture, including version metadata (xmin/xmax), version chains, and how multiple versions of a row coexist.

## Scope

### In Scope

- Tuple version header (xmin, xmax)
- Version chain mechanics
- Version creation (INSERT, UPDATE, DELETE)
- Version visibility per MGA rules
- Version garbage collection eligibility

### Out of Scope

- Physical version storage layout (see Version Chain Format)
- TOAST versioning (see TOAST Storage)
- Index version handling (see Index specs)

## Background

Each tuple version has:
- **xmin**: Transaction that created this version
- **xmax**: Transaction that deleted/updated this version (0 = current)

Multiple versions of the same logical row form a **version chain**, with each version pointing to its predecessor.

## Specification

### Data Structures

#### Tuple Version Header

```cpp
// From include/scratchbird/core/heap_page.h:91
struct TupleHeader {
    uint64_t xmin;        // Creating transaction
    uint64_t xmax;        // Deleting transaction (0 = current)
    
    // Version chain pointer
    uint64_t back_version_gpid;  // Previous version location
    uint16_t back_version_slot;  // Previous version slot
    
    // Identity
    ID row_uuid;          // Stable row identifier
    uint16_t infomask;    // Version flags
};
```

#### Version Lifecycle States

```
Version States by (xmin, xmax, infomask):

State                    xmin     xmax      infomask
─────────────────────────────────────────────────────────────
Insert in progress       ACTIVE   0         -
Live (committed insert)  COMMIT   0         XMIN_COMMITTED
Delete in progress       COMMIT   ACTIVE    XMIN_COMMITTED
Deleted (committed)      COMMIT   COMMIT    XMIN_COMMITTED, XMAX_COMMITTED
Update in progress       COMMIT   ACTIVE    XMIN_COMMITTED, UPDATED
Update chain (old)       COMMIT   COMMIT    XMIN_COMMITTED, UPDATED, XMAX_COMMITTED
Aborted insert           ABORT    0         XMIN_INVALID
Aborted delete           COMMIT   ABORT     XMIN_COMMITTED, XMAX_INVALID
Aborted update           COMMIT   ABORT     XMIN_COMMITTED, UPDATED, XMAX_INVALID
Frozen                   FROZEN   *         XMIN_FROZEN
```

### Version Creation

#### INSERT

```
Algorithm: createVersion(tuple_data, xmin)

1. Initialize TupleHeader:
2.     header.xmin = xmin
3.     header.xmax = 0
4.     header.back_version_gpid = INVALID_GPID
5.     header.row_uuid = generateUUID()
6.     header.infomask = 0

7. Write tuple to page
8. RETURN item_id
```

**State After INSERT:**
- If xmin commits: Version becomes visible to newer transactions
- If xmin aborts: Version marked XMIN_INVALID, never visible

#### DELETE

```
Algorithm: deleteVersion(item_id, xmax)

1. Read tuple header at item_id
2. ASSERT header.xmax == 0  // Not already deleted

3. header.xmax = xmax
4. // Don't set XMAX_COMMITTED yet - wait for commit

5. Mark item as deleted in infomask if needed
```

**State After DELETE:**
- If xmax commits: Version becomes invisible (tombstone)
- If xmax aborts: Delete undone, version still visible

#### UPDATE

```
Algorithm: updateVersion(item_id, new_data, xmax, new_xmin)

1. Read old tuple at item_id
2. 
3. // Phase 1: Create back version preserving old state
4. back_version_offset = allocateSpace(page, old_tuple_size)
5. copy old tuple to back_version_offset
6. old_header.xmax = xmax
7. old_header.infomask |= HEAP_UPDATED
8. 
9. // Phase 2: Overwrite primary location with new version
10. new_header.xmin = new_xmin
11. new_header.xmax = 0
12. new_header.back_version_gpid = MAKE_GPID(page_id)
13. new_header.back_version_slot = back_version_offset
14. new_header.row_uuid = old_header.row_uuid  // Preserve identity
15. 
16. Write new tuple to primary location
17. RETURN item_id (same - TID stable)
```

**Version Chain After UPDATE:**
```
[Version N+1] (primary location)
     │
     │ back_version
     ▼
[Version N] (back version)
     │
     │ back_version (if more updates)
     ▼
   ...
```

### Version Visibility

```
Is version V visible to transaction T?

1. IF V.xmin == T.xid: RETURN true  // Own version

2. IF V.xmin <= FROZEN_XID: RETURN true  // Frozen

3. IF getState(V.xmin) != COMMITTED: RETURN false  // Insert not committed

4. IF V.xmin >= T.xid: RETURN false  // Created after T started

5. // xmin committed and older than T - check xmax
6. IF V.xmax == 0: RETURN true  // Not deleted

7. IF V.xmax == T.xid: RETURN true  // Deleted by T (can still see)

8. IF getState(V.xmax) != COMMITTED: RETURN true  // Delete not committed

9. IF V.xmax >= T.xid: RETURN true  // Deleted after T started

10. RETURN false  // Deleted by committed transaction before T
```

### Version Garbage Collection

A version is eligible for GC when:

```
Version is DEAD if:
    (xmax != 0) AND                // Was deleted
    (getState(xmax) == COMMITTED) AND  // Delete committed
    (xmax < OIT)                   // Delete visible to all

Back version is PRUNABLE if:
    (infomask & HEAP_UPDATED) AND  // Was updated
    (hasBackVersion()) AND         // Has newer version
    (isDead())                     // Is dead per above
```

## Invariants

1. **xmin Validity**: xmin is always a valid XID (not 0)
   - Verification: Assert on tuple creation
   
2. **Version Ordering**: xmax > xmin (for completed operations)
   - Verification: Time-based, naturally true
   
3. **Chain Integrity**: Back version pointer always points to older version
   - Verification: Assert back_version.xmin < current.xmin
   
4. **Row UUID Stability**: All versions of same row have same row_uuid
   - Verification: Preserved during UPDATE

## Decision Trees

```
Version State Determination:
│
├─ xmin not committed ──────────────────────────► INSERT IN PROGRESS
│   └─ IF xmin aborts ──────────────────────────► ABORTED (invisible)
│
├─ xmin committed ──────────────────────────────► Check xmax
│   │
│   ├─ xmax == 0 ───────────────────────────────► LIVE
│   │
│   ├─ xmax not committed ──────────────────────► DELETE/UPDATE IN PROGRESS
│   │   ├─ xmax aborts ─────────────────────────► LIVE (undo)
│   │   └─ xmax commits ────────────────────────► DEAD
│   │
│   └─ xmax committed ──────────────────────────► Check ordering
│       ├─ xmax > xmin ─────────────────────────► DEAD
│       └─ xmax <= xmin ────────────────────────► CORRUPT
│
└─ xmin == FROZEN_XID ──────────────────────────► FROZEN (always visible)
```

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `PAGE_CORRUPT` | xmin > xmax (committed) | Log error, skip version |
| `VERSION_CHAIN_BROKEN` | Back version invalid | Stop traversal |

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_tuple_versions.cpp` | Version lifecycle |
| `tests/unit/test_version_visibility.cpp` | Visibility rules |
| `tests/unit/test_version_gc.cpp` | GC eligibility |

## Related Specifications

- [Version Chain Format](./version_chain_format.md) - Physical chain structure
- [MGA Visibility Rules](./mga_visibility_rules.md) - Visibility computation
- [GC Sweep](./gc_sweep.md) - Dead version cleanup
- [Heap Format](./heap_format.md) - Tuple header details

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | AI Agent |
