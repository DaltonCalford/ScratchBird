# Phase 16: WAL for Durability (SECONDARY to MGA)

## Objective
Add Write-Ahead Logging as a SECONDARY system for durability only. MGA already provides ACI of ACID.

## Prerequisites
- Phase 15 complete (aggregation)
- MGA fully functional (Phases 6-7)

## Architecture Note
**WAL is SECONDARY**: The database is fully functional without WAL (in-memory mode). WAL only adds the D in ACID. This is the Firebird architecture where MGA is primary.

## Tasks

### 16.1 Minimal WAL Records
```cpp
// WAL records what MGA already did
enum WalRecordType {
    Commit,           // Transaction committed in TIP
    DataPage,         // Page modified (after-image only)
    TIPUpdate,        // TIP page updated
    Checkpoint        // Consistent state marker
    // NO UNDO RECORDS - MGA handles rollback
};

struct WalRecord {
    uint64_t lsn;
    uint64_t xid;
    WalRecordType type;
    vector<uint8_t> data;  // After-image only
};
```

### 16.2 Write-After-Commit Protocol
```cpp
Status commit_with_durability(Transaction* txn) {
    // 1. Update TIP to Committed (MGA primary)
    tip_manager.set_committed(txn->xid);
    
    // 2. Transaction is now committed (visible via MGA)
    
    // 3. Write WAL for durability (secondary)
    if (wal_enabled) {
        wal.write_commit_record(txn->xid);
        wal.flush();  // Ensure durability
    }
    
    // Note: If crash between 2 and 3, transaction is
    // committed but not durable (acceptable in Firebird model)
}
```

### 16.3 Recovery Process (Restore MGA State)
```cpp
void recover_database() {
    // 1. Database has MGA structures but may be inconsistent
    
    // 2. Find last checkpoint in WAL
    auto checkpoint = wal.find_last_checkpoint();
    
    // 3. Replay ONLY committed transactions
    for (auto& record : wal.read_from(checkpoint)) {
        switch(record.type) {
            case Commit:
                // TIP already has state, just ensure pages written
                break;
            case DataPage:
                // Restore page data
                write_page(record.page_no, record.data);
                break;
        }
    }
    
    // 4. Run garbage collection to clean up
    mga_garbage_collect();
}
```

### 16.4 WAL-Optional Mode
```cpp
class Database {
    bool wal_enabled = true;  // Can be disabled
    
    void set_durability_mode(bool enable_wal) {
        wal_enabled = enable_wal;
        if (!wal_enabled) {
            log("WARNING: Running without WAL - no crash durability");
            // Database still fully functional via MGA
        }
    }
};
```

### 16.5 Checkpoint Strategy
```cpp
void checkpoint() {
    // 1. Ensure all committed XIDs in TIP are on disk
    tip_manager.flush_all_pages();
    
    // 2. Write dirty data pages
    buffer_manager.flush_dirty_pages();
    
    // 3. Record checkpoint in WAL
    if (wal_enabled) {
        wal.write_checkpoint_record();
    }
    
    // 4. Can truncate old WAL after checkpoint
}
```

## Files to Create/Modify
- `include/scratchbird/engine/wal_secondary.h`
- `src/engine/wal_writer_minimal.cpp`
- `src/engine/recovery_mga.cpp`

## Validation Tests
```cpp
// Test MGA works without WAL
set_durability_mode(false);  // Disable WAL
begin_transaction();
execute("INSERT INTO test VALUES (1, 'no-wal')");
commit();  // Works fine, just not durable

// Verify data visible
auto result = execute("SELECT * FROM test");
assert(result.rows.size() == 1);

// Crash test without WAL
simulate_crash();
restart_database();
result = execute("SELECT * FROM test");
assert(result.rows.size() == 0);  // Lost (no durability)

// Enable WAL for durability
set_durability_mode(true);
begin_transaction();
execute("INSERT INTO test VALUES (2, 'with-wal')");
commit();  // Now durable

// Crash test with WAL
simulate_crash();
restart_database();
result = execute("SELECT * FROM test");
assert(result.rows.size() == 1);  // Recovered
assert(result.rows[0]["value"] == "with-wal");

// Test minimal WAL size
// Traditional: records undo + redo
// MGA+WAL: only records redo (smaller)
auto traditional_size = measure_wal_size_traditional(operations);
auto mga_wal_size = measure_wal_size_mga(operations);
assert(mga_wal_size < traditional_size * 0.5);  // Much smaller

// Checkpoint reduces WAL
for(int i = 0; i < 10000; i++) {
    execute("INSERT INTO test VALUES (?)", {i});
}
auto wal_size_before = get_wal_size();
checkpoint();
auto wal_size_after = get_wal_size();
assert(wal_size_after < wal_size_before);
```

## Exit Criteria
- Database fully functional without WAL
- WAL adds durability when enabled
- Recovery restores MGA state correctly
- WAL size minimal (no undo records)
- Checkpoints reduce WAL size
- Clear separation: MGA=ACI, WAL=D