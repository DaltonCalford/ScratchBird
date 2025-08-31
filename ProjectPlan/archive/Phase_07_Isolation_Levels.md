# Phase 7: Isolation Levels and MVCC

## Objective
Implement snapshot isolation with Read Committed and Repeatable Read.

## Prerequisites
- Phase 6 complete (basic transactions)

## Tasks

### 7.1 Snapshot Management
```cpp
struct Snapshot {
    uint64_t xmin;  // Oldest active XID
    uint64_t xmax;  // Next XID to allocate
    vector<uint64_t> active_xids;
    
    bool can_see(uint64_t created_xid, uint64_t deleted_xid);
};
```

### 7.2 Isolation Levels
```cpp
enum IsolationLevel {
    ReadUncommitted,
    ReadCommitted,
    RepeatableRead
};
```

### 7.3 Version Chains
- Link tuple versions via `backptr_rid`
- Navigate chain to find visible version
- Maintain version history

### 7.4 Conflict Detection
- Detect write-write conflicts
- Return appropriate error codes
- Implement "first-updater-wins" policy

### 7.5 Garbage Collection
- Identify unreachable versions
- Remove old versions safely
- Respect active snapshots

## Files to Create/Modify
- `include/scratchbird/engine/mvcc.h`
- `src/engine/mvcc.cpp`

## Validation Tests
```cpp
// Read Committed
SET_ISOLATION(session1, ReadCommitted);
begin(session1);
begin(session2);
update(session2, "UPDATE test SET val=2");
select(session1, "SELECT val FROM test");  // Sees old value
commit(session2);
select(session1, "SELECT val FROM test");  // Sees new value

// Repeatable Read
SET_ISOLATION(session1, RepeatableRead);
begin(session1);
select(session1, "SELECT val FROM test");  // Snapshot taken
begin(session2);
update(session2, "UPDATE test SET val=3");
commit(session2);
select(session1, "SELECT val FROM test");  // Still sees old value
```

## Exit Criteria
- Isolation levels behave correctly
- Version chains maintained
- Garbage collection works without breaking active transactions