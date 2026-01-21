# TOAST/LOB Storage Implementation

## Overview

TOAST (The Oversized-Attribute Storage Technique) is a mechanism for storing large values that exceed the normal tuple size limits. It allows ScratchBird to handle large binary objects (LOBs), text, and other oversized data by storing them out-of-line in a separate TOAST table.

**MGA Reference:** See `MGA_RULES.md` for Multi-Generational Architecture semantics (visibility, TIP usage, recovery).
**WAL Scope:** ScratchBird does not use write-after log (WAL) for recovery in Alpha; any WAL support is optional post-gold (replication/PITR).
Any WAL references in this document describe an optional post-gold stream for
replication/PITR only.
**Table Footnote:** In comparison tables below, ScratchBird WAL references are optional post-gold (replication/PITR).

## Architecture

### Key Components

1. **ToastManager** - Manages TOAST operations for a table
2. **ToastPointer** - Stored in main tuple, points to TOAST data
3. **ToastChunk** - Individual pieces of large values
4. **TOAST Tables** - Special tables storing chunked data

### Design Principles

- **Transparency**: Applications see full values, not pointers
- **Efficiency**: Only TOAST values above threshold
- **Flexibility**: Multiple storage strategies
- **Chunking**: Large values split into manageable pieces

## Storage Strategies

| Strategy | Description | When Used |
|----------|-------------|-----------|
| PLAIN | Store inline (no TOAST) | Small values < 2KB |
| EXTENDED | Out-of-line, uncompressed | Medium values or incompressible |
| COMPRESSED | Inline, compressed | Not implemented (optional post-alpha) |
| EXTERNAL | Out-of-line, compressed | Large compressible values |

## Implementation Details

### TOAST Threshold

```cpp
constexpr uint32_t TOAST_TUPLE_THRESHOLD = 2000;  // 2KB
constexpr uint32_t TOAST_MAX_CHUNK_SIZE = 1996;   // Legacy 8KB default; dynamic sizing uses ToastSettings
```

Values larger than `TOAST_TUPLE_THRESHOLD` or 1/4 of the page size are candidates for TOASTing.

### TOAST Pointer Structure

```cpp
struct ToastPointer {
    uint8_t  va_header;      // 0x01 = TOAST marker
    uint8_t  va_tag;         // Strategy type
    uint32_t va_rawsize;     // Original size
    uint32_t va_extsize;     // Stored size
    uint32_t va_valueid;     // Unique ID
    uint32_t va_toastrelid;  // TOAST table ID
};
```

### TOAST Table Schema

Each regular table can have an associated TOAST table named `pg_toast_<table_id>` with chunks stored as tuples.

**TOAST Chunk Format** (MGA-Compliant, TupleHeader + metadata):

```cpp
struct ToastChunk {
    TupleHeader header;      // MGA/TIP visibility metadata

    // TOAST Metadata (12 bytes)
    uint32_t value_id;       // TOAST value ID
    uint32_t chunk_seq;      // Chunk sequence number (0-based)
    uint32_t chunk_size;     // Size of chunk_data in bytes

    // Chunk Data (variable length, up to TOAST_MAX_CHUNK_SIZE)
    uint8_t  chunk_data[];   // Actual chunk bytes
};
```

**Total Header Size**: sizeof(TupleHeader) + 12 bytes (TupleHeader + chunk metadata)

**Key MGA Compliance Features**:
- **header.xmin**: Transaction that created this chunk (for visibility)
- **header.xmax**: Transaction that deleted this chunk (0 = active, non-zero = deleted)
- **TIP-based visibility**: Chunk visibility determined by TIP state, NOT snapshots
- **Independent lifecycle**: Each chunk is independently versioned

### Chunking Process

1. Large values are assigned a unique value ID
2. Data is split into chunks of max `TOAST_MAX_CHUNK_SIZE`
3. Each chunk is stored as a tuple in the TOAST table
4. Chunks are numbered sequentially (0-based)

## Operations

### TOASTing a Value

```cpp
ToastManager toast_mgr(db, table_id);
toast_mgr.initialize();  // Creates TOAST table if needed

ToastPointer pointer;
Status status = toast_mgr.toast_value(
    data, size,              // Value to TOAST
    ToastStrategy::EXTENDED, // Strategy
    xmin,                    // Transaction ID
    &pointer                 // Output pointer
);
```

### Detoasting a Value

```cpp
std::vector<uint8_t> data;
Status status = toast_mgr.detoast_value(
    &pointer,    // TOAST pointer
    &data,       // Output buffer
    xmin         // Transaction ID
);
```

### Deleting TOAST Values

```cpp
Status status = toast_mgr.delete_toast_value(
    value_id,    // TOAST value ID
    xmax         // Deleting transaction
);
```

**Note**: Deletion sets xmax on all chunks for the value. Physical deletion happens during sweep/GC.

## TIP-Based Visibility (MGA Compliance)

**CRITICAL**: TOAST chunks use **TIP (Transaction Inventory Pages)** for visibility, NOT snapshot-based visibility like PostgreSQL.

### Visibility Rules

A TOAST chunk is **visible** to a transaction if:

1. **xmin is committed** (via TIP lookup)
   - TIP shows xmin transaction as `TX_COMMITTED`
   - Chunk was successfully created

2. **xmax is NOT committed** (via TIP lookup)
   - If `xmax == 0`: Chunk not deleted
   - If `xmax != 0` and TIP shows `TX_ACTIVE` or `TX_ABORTED`: Deletion not finalized

**Visibility Check** (from `ToastVisibility::isChunkVisible`):

```cpp
bool isChunkVisible(uint64_t chunk_xmin, uint64_t chunk_xmax,
                   uint64_t current_xmin, TransactionManager* tm)
{
    // Check xmin: Must be committed
    if (!tm->isTransactionVisible(chunk_xmin, current_xmin)) {
        return false;  // Creating transaction not visible
    }

    // Check xmax: If set, must NOT be committed
    if (chunk_xmax != 0) {
        if (tm->isTransactionVisible(chunk_xmax, current_xmin)) {
            return false;  // Deleting transaction committed
        }
    }

    return true;  // Chunk is visible
}
```

### Key Differences from PostgreSQL MVCC

| Aspect | PostgreSQL (MVCC/write-after log (WAL)) | ScratchBird (MGA/TIP) |
|--------|----------------------|----------------------|
| Visibility Source | Snapshot + write-after log (WAL) | TIP state only |
| Crash Recovery | write-after log (WAL) replay | TIP state check |
| Transaction State | In-memory + write-after log (WAL) | TIP (2-bit state) |
| Chunk Lifecycle | Snapshot-based | TIP-based |
| Garbage Collection | Snapshot horizon | TIP state + orphan detection |

### Transaction States (TIP)

Each transaction has one of 4 states in TIP:

- **TX_ACTIVE** (0b00): Transaction in progress
- **TX_COMMITTED** (0b01): Transaction completed successfully
- **TX_ABORTED** (0b10): Transaction rolled back
- **TX_LIMBO** (0b11): Two-phase commit pending

**TOAST Visibility Logic**:
- Chunks created by `TX_ACTIVE` → Invisible to other transactions
- Chunks created by `TX_COMMITTED` → Visible to later transactions
- Chunks created by `TX_ABORTED` → Invisible to all (orphans)
- Chunks deleted by `TX_COMMITTED` → Invisible to all
- Chunks deleted by `TX_ABORTED` → Still visible (delete rolled back)

### Crash Recovery

**Without write-after log (WAL, optional post-gold)** (MGA approach):

1. Database crashes during TOAST operation
2. On restart, check TIP for transaction state
3. If transaction is `TX_ACTIVE` → Mark as `TX_ABORTED` in TIP
4. TOAST chunks with aborted xmin become invisible
5. Garbage collection (sweep) physically removes orphaned chunks

**NO write-after log (WAL, optional post-gold) replay needed** - all state recovered from TIP.

## Garbage Collection (MGA-Compliant)

TOAST chunks are garbage collected during database sweep/GC using a **3-phase approach**:

### Orphan Detection (Alpha)

**Problem**: TOAST chunks whose parent tuples have been deleted or aborted.

**Solution**: Scan heap tables for TOAST pointer references, identify unreferenced chunks.

```cpp
GarbageCollector* gc = db->garbage_collector();
std::unordered_set<uint32_t> orphaned_value_ids;

// Detect orphans: chunks not referenced by any heap tuple
Status status = gc->detectOrphanedToastChunks(
    toast_table_id,
    &orphaned_value_ids,
    &ctx
);
```

**Algorithm**:
1. Scan all heap tables looking for TOAST pointers
2. Build set of referenced value IDs
3. Scan TOAST table for all value IDs
4. Find orphans: value IDs in TOAST but not referenced

### Orphan Cleanup (Alpha)

**Solution**: Physically delete all chunks for orphaned values.

```cpp
uint64_t chunks_deleted = 0;

Status status = gc->cleanOrphanedToastChunks(
    toast_table_id,
    orphaned_value_ids,  // From orphan detection
    &chunks_deleted,
    &ctx
);
```

**Note**: Orphaned chunks have no parent tuples, so safe to delete physically without transaction coordination.

### TIP-Based GC (Alpha)

**Problem**: TOAST chunks where xmax transaction has committed (deletion finalized).

**Solution**: Use TIP to check xmax state, physically delete chunks with committed xmax.

```cpp
uint64_t chunks_deleted = 0;

Status status = gc->cleanToastChunksByTIP(
    toast_table_id,
    &chunks_deleted,
    &ctx
);
```

**Algorithm**:
1. Scan TOAST table
2. For each chunk with `xmax != 0`:
   - Check TIP state of xmax transaction
   - If xmax is `TX_COMMITTED`: Physically delete chunk
   - If xmax is `TX_ABORTED`: TODO - Clear xmax (chunk still alive)

### Vacuum Integration

TOAST garbage collection is integrated into database sweep/GC:

```cpp
// From src/core/vacuum.cpp
if (table.table_type == CatalogManager::TableType::TOAST) {
    auto* gc = db_->garbage_collector();

    // Detect orphans
    std::unordered_set<uint32_t> orphaned_value_ids;
    gc->detectOrphanedToastChunks(table.table_id, &orphaned_value_ids, ctx);

    // Clean orphans
    if (!orphaned_value_ids.empty()) {
        uint64_t orphans_deleted = 0;
        gc->cleanOrphanedToastChunks(table.table_id, orphaned_value_ids,
                                    &orphans_deleted, ctx);
    }

    // TIP-based GC
    uint64_t tip_deleted = 0;
    gc->cleanToastChunksByTIP(table.table_id, &tip_deleted, ctx);
}
```

### GC Performance

**Time Complexity**:
- Orphan detection: O(H + T) where H = heap tuples, T = TOAST chunks
- Orphan cleanup: O(T * O) where O = orphans found
- TIP-based GC: O(T) - full TOAST table scan

**Space Complexity**:
- Orphan detection: O(R + T) where R = references
- Orphan cleanup: O(O) for TID collection
- TIP-based GC: O(D) where D = chunks to delete

**Optimization Opportunities**:
- Parallelize orphan detection (partition heap scan)
- Cache TIP pages (reduce lookup overhead)
- Skip clean TOAST tables (no orphans in N passes)

## Compression Integration

When `ToastStrategy::EXTERNAL` is used:
1. Data is compressed using the pluggable compression framework
2. If compression saves >10%, compressed data is stored
3. Otherwise, falls back to uncompressed storage
4. Detoasting automatically handles decompression

## Performance Characteristics

### Space Efficiency
- Reduces main table size
- Allows tuples with very large attributes
- Compression can save 50-90% for text data

### Access Patterns
- Small values: No overhead
- TOASTed values: Extra I/O for retrieval
- Sequential scan: Can skip TOAST retrieval if column not needed

### Chunk Size Trade-offs
- Smaller chunks: More flexible, more overhead
- Larger chunks: Less overhead, may waste space
- Current: ~2KB balances both concerns

## Testing

### Unit Tests

**Files**:
- `tests/unit/test_toast_operations.cpp` - Basic TOAST operations
- `tests/unit/test_toast_cleanup.cpp` - Cleanup operations
- `tests/unit/test_toast_format.cpp` - Chunk format validation
- `tests/unit/test_toast_tip_visibility.cpp` - **TIP visibility rules (8 tests)**

**Key Tests** (test_toast_tip_visibility.cpp):
1. **ActiveTransaction_ChunkInvisible** - Chunks from active txns invisible to others
2. **CommittedTransaction_ChunkVisible** - Chunks from committed txns visible
3. **AbortedTransaction_ChunkInvisible** - Chunks from aborted txns invisible
4. **DeletedByCommittedTxn_ChunkInvisible** - Committed xmax makes chunks invisible
5. **DeletedByAbortedTxn_ChunkVisible** - Aborted xmax keeps chunks visible
6. **DeletedByActiveTxn_ChunkVisibleToOthers** - Active xmax keeps chunks visible to others
7. **TIPStateTransitions_TransactionLifecycle** - Validates TX_ACTIVE → TX_COMMITTED
8. **SnapshotIsolation_SameTransactionSeesOwnChanges** - Transaction sees own uncommitted chunks

### Integration Tests

**Files**:
- `tests/integration/test_storage_toast_indexing.cpp` - Storage integration, index detoasting
- `tests/integration/test_toast_garbage_collection.cpp` - Orphan detection, TIP-based GC (6 tests)
- `tests/integration/test_toast_crash_recovery_mga.cpp` - **MGA crash recovery (6 tests)**

**Key Tests** (test_toast_crash_recovery_mga.cpp):
1. **CrashBeforeCommit_ChunksInvisible** - Validates TIP marks crashed transactions aborted
2. **CrashAfterCommit_ChunksVisible** - Validates committed data persists via TIP
3. **CrashDuringDelete_XmaxHandling** - Validates TIP-based xmax visibility
4. **MultipleCrashes_IdempotentRecovery** - Validates recovery is idempotent
5. **CrashWithSweep_FullCleanup** - Validates sweep uses TIP for GC
6. **TIPStatePersistence** - Validates TIP persists across restarts

### Stress Tests

**Files**:
- `tests/stress/test_toast_concurrency.cpp` - **Concurrent TOAST operations (5 tests)**

**Key Tests**:
1. **ConcurrentInserts_NoConflicts** - 10 threads × 10 inserts (100 concurrent operations)
2. **ConcurrentReads_SnapshotIsolation** - 20 threads reading concurrently
3. **ConcurrentInsertAndRead_Isolation** - 5 writers + 10 readers for 5 seconds
4. **ConcurrentDeletes_TIPVisibility** - 10 threads deleting concurrently
5. **HighContention_ManyTransactionsSameValue** - 50 threads reading same value

### Test Coverage Summary

**Overall**:
- 8 test files (~3,550 lines total)
- 43+ test cases (unit, integration, stress)
- ✅ All page sizes (8KB - 128KB)
- ✅ Values from 0 to 100+ chunks
- ✅ Compressed and uncompressed
- ✅ Strategy selection logic
- ✅ Error handling
- ✅ **MGA compliance (TIP-based visibility)**
- ✅ **Crash recovery (TIP state, no write-after log (WAL, optional post-gold))**
- ✅ **Concurrent operations (snapshot isolation)**
- ✅ **Garbage collection (3-phase GC)**

## Future Enhancements

1. **Inline Compression** (COMPRESSED strategy)
   - Compress small-medium values inline
   - Avoid TOAST table overhead

2. **Partial Detoasting**
   - Retrieve only needed chunks
   - Substring operations without full detoast

3. **TOAST Indexes**
   - Index on (chunk_id, chunk_seq)
   - Faster chunk retrieval

4. **Shared TOAST Tables**
   - Multiple tables share TOAST storage
   - Better space utilization

5. **Alternative Chunk Sizes**
   - Configurable per table
   - Optimize for workload

## Integration Notes

### With Storage Engine
- TOAST tables are regular tables
- Use standard tuple operations
- Subject to same MGA visibility rules (TIP-based)

### With Catalog Manager
- TOAST tables created in same schema
- Named systematically: `pg_toast_<UUID>`
- Tracked in system catalog

### With Buffer Pool
- TOAST chunks cached like regular pages
- LRU eviction applies
- No special handling needed

### With Indexes (Critical for MGA)

**Problem**: Indexes store actual values, not TOAST pointers. If a TOASTed value is indexed, the index must store the detoasted value.

**Solution**: `IndexKeyExtractor` helper class detoasts values before index insertion.

```cpp
// From src/core/btree.cpp
IndexKeyExtractor key_extractor(schema, db);

// Extract and detoast indexed columns
std::vector<IndexKey> keys;
Status status = key_extractor.extractKeys(tuple, index_columns, &keys, ctx);

// Keys now contain detoasted values, ready for index insertion
for (const auto& key : keys) {
    btree->insert(key, tid, ctx);
}
```

**Why This Matters**:
- Index lookups must return actual values, not pointers
- Index comparisons must work on actual data
- Index scans must not require TOAST table access

**Implementation**: `src/core/index_key_extractor.cpp` (lines 85-150)
- Detects TOAST pointers via magic byte (0x01)
- Calls `detoastValue()` to retrieve full value
- Handles errors gracefully (falls back to pointer if detoast fails)

## Best Practices

1. **Schema Design**
   - Use appropriate data types
   - Consider TOAST overhead in capacity planning
   - Monitor TOAST table growth

2. **Application Development**
   - Minimize large value updates
   - Use streaming APIs when available
   - Consider client-side compression

3. **Performance Tuning**
   - Monitor TOAST fetch frequency
   - Adjust thresholds if needed
   - Consider storage strategy per column

## Status

⚠️ **PARTIALLY IMPLEMENTED** - The TOAST/LOB storage framework is implemented but requires integration with the main tuple storage system. The following components are complete:

✅ Core TOAST structures and interfaces
✅ Chunking and reassembly logic
✅ Compression integration
✅ TOAST table creation
✅ Basic operations (toast/detoast/delete)

The following work remains:
- Integration with HeapPage for automatic TOASTing
- Modification of insert_tuple to handle TOAST pointers
- Update of get_tuple to automatically detoast
- Transaction visibility for TOAST values

Once these integrations are complete, TOAST will be fully functional for Stage 1.1.
