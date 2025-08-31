# Phase 16: Write-Ahead Logging and Recovery

## Objective
Implement WAL for crash recovery and durability.

## Prerequisites
- Phase 15 complete (aggregation)

## Tasks

### 16.1 WAL Record Types
```cpp
enum WalRecordType {
    BeginTransaction,
    Commit,
    Abort,
    InsertTuple,
    DeleteTuple,
    UpdateTuple,
    Checkpoint
};

struct WalRecord {
    uint64_t lsn;  // Log Sequence Number
    WalRecordType type;
    uint64_t xid;
    vector<uint8_t> data;
};
```

### 16.2 WAL Writer
- Write records before page modifications
- Ensure write ordering
- Flush on commit

### 16.3 Recovery Process
- Find last checkpoint
- Replay WAL from checkpoint
- Redo committed transactions
- Undo uncommitted transactions

### 16.4 Checkpointing
- Periodic checkpoints
- Write dirty pages to disk
- Record checkpoint in WAL

## Files to Create/Modify
- `include/scratchbird/engine/wal.h`
- `src/engine/wal_writer.cpp`
- `src/engine/recovery.cpp`

## Validation Tests
```cpp
// Normal operation with WAL
begin_transaction();
execute("INSERT INTO test VALUES (1, 'data')");
commit();  // WAL flushed

// Crash recovery test
begin_transaction();
execute("INSERT INTO test VALUES (2, 'uncommitted')");
// Simulate crash (no commit)
kill_process();

// Restart and recover
auto db = open_database("test.db");
auto result = execute("SELECT * FROM test");
assert(result.rows.size() == 1);  // Only committed data
assert(result.rows[0][0] == "1");  // Uncommitted rolled back

// Checkpoint test
for(int i = 0; i < 1000; i++) {
    execute("INSERT INTO test VALUES (?, ?)", {i, "data"});
}
force_checkpoint();
// Verify checkpoint reduces WAL size
```

## Exit Criteria
- WAL records all modifications
- Recovery restores consistent state
- Checkpoints reduce recovery time