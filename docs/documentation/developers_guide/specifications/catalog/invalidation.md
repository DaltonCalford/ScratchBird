# Specification: Plan Cache Invalidation

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | catalog |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird 0.1.0 |
| **Authors** | ScratchBird Team |

## Synopsis

This specification defines plan cache invalidation strategies when catalog objects change, ensuring query plans are recompiled when underlying schema changes.

## Scope

### In Scope

- Plan cache invalidation triggers
- Statement cache versioning
- Epoch-based invalidation
- Cross-session invalidation
- Materialized view refresh triggering

### Out of Scope

- Plan cache data structures (see executor specs)
- Query plan generation (see optimizer specs)

## Specification

### Invalidation Events

| Event | Invalidation Scope | Action |
|-------|-------------------|--------|
| ALTER TABLE | Table plans + dependent views | Recompile |
| CREATE/DROP INDEX | Table plans | Recompile |
| ALTER COLUMN | Column plans + dependent objects | Recompile |
| CREATE/DROP VIEW | View plans + dependent views | Recompile |
| ALTER FUNCTION | Function callers | Recompile |
| GRANT/REVOKE | Object plans | Recompile |
| ANALYZE | Table statistics | Update stats, may recompile |

### Policy Epoch

**Source:** `include/scratchbird/core/catalog_manager.h:451`

```cpp
struct TableInfo {
    // ... other fields ...
    uint64_t policy_epoch = 0;      // Security policy epoch
};
```

The policy_epoch is incremented when security policies change:
- GRANT/REVOKE on table
- RLS policy changes
- Owner changes

### Invalidation Strategy

```cpp
struct PlanCacheEntry {
    uint64_t creation_epoch;        // Epoch when plan created
    ID object_id;                   // Primary object
    std::vector<ID> dependencies;   // All dependent objects
    std::shared_ptr<QueryPlan> plan;
};

bool isPlanValid(const PlanCacheEntry& entry) {
    // Check if any dependency has changed
    for (ID dep_id : entry.dependencies) {
        uint64_t current_epoch = getObjectEpoch(dep_id);
        if (current_epoch > entry.creation_epoch) {
            return false;  // Object changed since plan created
        }
    }
    return true;
}
```

### Catalog Change Notification

```cpp
// Called after any DDL operation
void notifyCatalogChange(ObjectType type, ID object_id) {
    1. Increment object epoch
    2. Broadcast invalidation to all sessions
    3. Invalidate local plan cache entries
    4. Trigger MV refresh if needed
}

// Session receives invalidation
void handleInvalidation(ObjectType type, ID object_id) {
    // Remove affected plans from session cache
    plan_cache.removeIf([object_id](const auto& entry) {
        return entry.hasDependency(object_id);
    });
}
```

### Materialized View Invalidation

```cpp
void invalidateMaterializedViews(ID base_table_id) {
    // Find MVs that depend on this table
    auto mvs = findDependentMaterializedViews(base_table_id);
    
    for (ID mv_id : mvs) {
        auto& mv_info = getViewInfo(mv_id);
        
        if (mv_info.refresh_on_commit) {
            // Schedule immediate refresh
            scheduleRefresh(mv_id);
        } else {
            // Mark as stale
            mv_info.last_refresh_time = 0;
        }
    }
}
```

### Invalidation Scopes

**Local (Session):**
- Session-local temporary tables
- Prepared statements in session

**Global (All Sessions):**
- Permanent tables
- Views
- Functions
- Security policies

**Deferred:**
- Statistics updates
- Non-critical index changes

## Algorithms

### Algorithm: Invalidate Plans

```
Input:  Changed object ID, change type
Output: Invalidation complete

1. Get current epoch for object
2. Increment epoch
3. Persist new epoch to catalog

4. Determine invalidation scope:
   a. Find all cached plans referencing object
   b. Find all plans referencing dependent objects
   
5. For each affected plan:
   a. Mark as invalid
   b. Remove from cache
   
6. Broadcast invalidation:
   a. Local: clear session cache entries
   b. Global: notify other sessions

7. Trigger dependent actions:
   a. MV refresh if needed
   b. Statistics update if needed
```

## Invariants

| ID | Invariant | Verification |
|----|-----------|-------------|
| `INV_INV_001` | Epoch monotonically increases | Counter check |
| `INV_INV_002` | All sessions see invalidations | Broadcast confirmation |
| `INV_INV_003` | Stale plans never execute | Epoch check before use |

## Related Specifications

- [dependency_tracking.md](./dependency_tracking.md) - Dependencies
- [views.md](./views.md) - Materialized view refresh

## Appendix

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
