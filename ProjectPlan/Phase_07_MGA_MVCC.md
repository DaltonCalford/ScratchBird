# Phase 7: MVCC via MGA (Lock-Free Isolation)

## Objective
Implement Multi-Version Concurrency Control using MGA's natural versioning - NO LOCKS for readers.

## Prerequisites
- Phase 6 complete (MGA transactions)

## Architecture Note
**Lock-Free MVCC**: Firebird's MGA provides isolation through version visibility rules. Readers NEVER take locks or block writers.

## Tasks

### 7.1 Snapshot-Based Visibility
```cpp
struct MGASnapshot {
    uint64_t xmin;              // Oldest active XID
    uint64_t xmax;              // Next XID to allocate
    vector<uint64_t> active;    // Active XIDs at snapshot time
    
    bool can_see_version(MGATupleHeader* version) {
        // Version visible if:
        // - created_xid committed before snapshot
        // - deleted_xid not committed or after snapshot
        // NO LOCKS CHECKED - only version info
    }
};
```

### 7.2 Isolation Level Implementation
```cpp
enum IsolationLevel {
    ReadCommitted,    // New snapshot each statement
    RepeatableRead,   // Snapshot at transaction start
    Serializable      // RR + write conflict detection
};

// NO READ LOCKS IN ANY ISOLATION LEVEL
```

### 7.3 Version Chain Navigation
```cpp
TupleData* find_visible_version(RowID rid, MGASnapshot* snap) {
    auto* version = fetch_tuple(rid);
    while (version) {
        if (snap->can_see_version(version)) {
            return version;
        }
        if (version->backptr_rid == 0) break;
        version = fetch_tuple(version->backptr_rid);
    }
    return nullptr;  // No visible version
}
```

### 7.4 Write-Write Conflict Detection
```cpp
// Only writers conflict with writers
bool check_update_conflict(RowID rid, Transaction* txn) {
    auto* latest = fetch_latest_version(rid);
    if (latest->deleted_xid != 0 && 
        is_active(latest->deleted_xid)) {
        return true;  // Conflict: another active txn deleted
    }
    return false;
}
```

### 7.5 Garbage Collection (Sweep)
```cpp
void garbage_collect() {
    // Find oldest active snapshot
    auto oldest_snap = find_oldest_snapshot();
    
    // Remove versions not visible to any snapshot
    for (auto& page : heap_pages) {
        for (auto& version : page.versions) {
            if (!visible_to_any_snapshot(version, oldest_snap)) {
                mark_for_removal(version);
            }
        }
    }
}
```

## Files to Create/Modify
- `include/scratchbird/engine/mga_mvcc.h`
- `src/engine/mga_snapshot.cpp`
- `src/engine/mga_visibility.cpp`
- `src/engine/mga_garbage.cpp`

## Validation Tests
```cpp
// Read Committed - NO LOCKS
SET_ISOLATION(session1, ReadCommitted);
SET_ISOLATION(session2, ReadCommitted);

// Reader doesn't block writer
auto read_thread = async([]() {
    begin(session1);
    execute("SELECT * FROM large_table");  // Long read
});

auto write_thread = async([]() {
    begin(session2);
    execute("UPDATE large_table SET val = 2");  // Proceeds immediately
    commit(session2);
});

// Both complete without blocking
read_thread.wait();
write_thread.wait();

// Repeatable Read - Still NO LOCKS
SET_ISOLATION(session1, RepeatableRead);
begin(session1);
auto snap1 = execute("SELECT COUNT(*) FROM test");

// Other transaction modifies
begin(session2);
execute(session2, "INSERT INTO test VALUES (...)");
commit(session2);

// Original transaction still sees old count
auto snap2 = execute(session1, "SELECT COUNT(*) FROM test");
assert(snap1 == snap2);  // Repeatable via snapshot, not locks

// Garbage Collection Test
// Create many versions
for(int i = 0; i < 1000; i++) {
    begin();
    execute("UPDATE test SET val = ?", {i});
    commit();
}

// Check version chains before GC
auto chain_length = count_versions("test", row_id);
assert(chain_length >= 1000);

// Run garbage collection
garbage_collect();

// Only current version remains (no active transactions)
chain_length = count_versions("test", row_id);
assert(chain_length == 1);
```

## Exit Criteria
- Readers NEVER block writers
- Writers NEVER block readers  
- Isolation via snapshots, not locks
- Version chains properly navigated
- Garbage collection removes old versions
- All isolation levels work without read locks