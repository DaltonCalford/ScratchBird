# Specification: Checkpoint Mechanism

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | storage |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | Future (vNext) |
| **Authors** | Dalton Calford |

## Synopsis

This specification defines the checkpoint mechanism for creating consistent database states and limiting recovery time. Checkpoints flush dirty pages and create recovery starting points.

## Scope

### In Scope

- Checkpoint initiation triggers
- Checkpoint phases
- Checkpoint record format
- Dirty page flushing
- Checkpoint statistics

### Out of Scope

- Continuous archiving
- Point-in-time recovery setup
- Backup integration

## Background

A checkpoint:
1. **Flushes dirty pages**: All modified pages written to disk
2. **Writes checkpoint record**: Marks consistent state
3. **Updates control file**: Records checkpoint location
4. **Recycles WAL**: Old WAL can be removed/reused

**Note**: ScratchBird Alpha uses no-WAL architecture. Checkpoints for metadata consistency only.

## Specification

### Data Structures

#### Checkpoint Record

```cpp
struct CheckpointRecord {
    uint64_t checkpoint_lsn;        // This checkpoint's LSN
    uint64_t redo_lsn;              // Oldest LSN needed for recovery
    
    // Transaction state
    uint64_t next_xid;              // Next XID to allocate
    uint64_t oldest_xid;            // OIT
    uint64_t oldest_active_xid;     // OAT
    
    // Time
    uint64_t checkpoint_time;       // When checkpoint started
    uint32_t elapsed_ms;            // Time to complete
    
    // Statistics
    uint32_t buffers_written;       // Pages flushed
    uint32_t buffers_total;         // Total pages in cache
    
    // Flags
    uint32_t flags;                 // Shutdown checkpoint? etc.
};
```

#### System State Page (Page 1)

```cpp
// From include/scratchbird/core/ondisk.h:462
struct BootstrapSystemStatePage {
    PageHeader page_header;
    
    uint8_t clean_shutdown;         // 1 = clean, 0 = crash
    uint8_t engine_mode;            // Running mode
    uint8_t cluster_state;          // Primary/replica
    uint8_t reserved0;
    
    uint64_t last_checkpoint_txid;  // Transaction at checkpoint
    uint64_t last_checkpoint_time;  // When checkpoint occurred
    uint64_t startup_counter;       // Incremented each start
    uint64_t config_flags;          // Runtime flags
    
    // ScratchBird Alpha checkpoint info
    uint64_t last_checkpoint_page;  // Page with checkpoint
    uint32_t checkpoint_count;      // Number of checkpoints
    
    uint64_t reserved[11];
};
```

### Checkpoint Triggers

```
Checkpoint can be triggered by:

1. Time-based
   - Parameter: checkpoint_timeout (default 5 minutes)
   - Timer fires, initiates checkpoint

2. WAL-based (with WAL)
   - Parameter: checkpoint_segments (default 3)
   - When WAL reaches threshold

3. Manual
   - SQL: CHECKPOINT command
   - Immediate checkpoint

4. Shutdown
   - Shutdown checkpoint ensures clean state
   - Fast shutdown: minimal checkpoint
   - Immediate shutdown: no checkpoint

5. Database startup (recovery)
   - After crash recovery completes
   - Creates fresh checkpoint
```

### Checkpoint Phases

```
Algorithm: performCheckpoint()

Phase 1: Preparation
─────────────────────
1. Acquire checkpoint lock (exclusive)
2. Record start time
3. Block new backends (optional - immediate checkpoint)
4. Snapshot transaction state:
   - next_xid
   - oldest_xid
   - oldest_active_xid

Phase 2: Flush Buffers
──────────────────────
1. Scan buffer pool
2. FOR each dirty buffer:
3.     IF buffer.pin_count > 0:
4.         SKIP (cannot write pinned buffer)
5.     ELSE:
6.         writePageToDisk(buffer.gpid, buffer.data)
7.         buffer.is_dirty = false
8.         stats.buffers_written++
9. 
10. fsync() all tablespace files

Phase 3: Write Checkpoint Record
────────────────────────────────
1. Build CheckpointRecord:
   - checkpoint_lsn = current_lsn
   - redo_lsn = oldest_dirty_lsn
   - transaction markers
   - statistics

2. Write to WAL (if WAL enabled)
   OR
   Write to System State Page (Alpha)

3. fsync() WAL / System State Page

Phase 4: Update Control File
────────────────────────────
1. Update System State Page:
   - last_checkpoint_time
   - last_checkpoint_txid
   - checkpoint_count++

2. fsync() System State Page

Phase 5: Cleanup
────────────────
1. Release checkpoint lock
2. Update statistics
3. Notify waiters
4. Recycle old WAL files (if applicable)

Total time: typically 100ms - 10s depending on data volume
```

### Checkpoint Types

| Type | Trigger | Behavior |
|------|---------|----------|
| Regular | Time/WAL threshold | Normal checkpoint |
| Immediate | SQL command | Complete ASAP, may block |
| Shutdown | Shutdown request | Ensure clean state |
| Restartpoint | Replica | Recovery checkpoint |

### Checkpoint Cost Control

```
Algorithm: spreadCheckpointWrites()

To avoid I/O spikes, spread writes over checkpoint interval:

1. target_duration = checkpoint_completion_target * checkpoint_timeout
2. estimated_writes = dirty_page_count
3. write_rate = estimated_writes / target_duration

4. WHILE flushing buffers:
5.     write_batch(write_rate * sleep_interval)
6.     sleep(sleep_interval)
7.     
8.     IF ahead_of_schedule:
9.         sleep_extra()
10.    IF behind_schedule:
11.        increase_batch_size()
```

## Invariants

1. **Consistent State**: All pages flushed are internally consistent
   - Verification: Pages written with valid checksums
   
2. **LSN Ordering**: Checkpoint LSN >= any page's LSN
   - Verification: Pages flushed in LSN order
   
3. **Atomic Record**: Checkpoint record fully written or not at all
   - Verification: CRC check on read

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `CHECKPOINT_FAILURE` | Write failed | Retry, alert admin |
| `DISK_FULL` | No space for checkpoint | Emergency measures |

## Performance Considerations

### Checkpoint Impact
- **I/O spike**: All dirty pages written
- **Response time**: Queries may slow during checkpoint
- **Mitigation**: Spread writes, use checkpoint_completion_target

### Checkpoint Frequency Trade-offs
| Frequency | Recovery Time | Runtime Impact |
|-----------|---------------|----------------|
| High | Fast | More I/O |
| Low | Slow | Less I/O |

### Buffer Management
- **Dirty ratio**: Percentage of dirty buffers
- **Target**: < 50% dirty
- **Background writer**: Proactive flushing

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_checkpoint.cpp` | Basic checkpoint |
| `tests/unit/test_checkpoint_recovery.cpp` | Recovery from checkpoint |
| `tests/unit/test_checkpoint_concurrent.cpp` | Concurrent operations |

## Related Specifications

- [WAL Format](./wal_format.md) - Checkpoint records
- [Buffer Pool](./buffer_pool.md) - Dirty page management

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | AI Agent |
