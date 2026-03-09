# Specification: Index DML Integration

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | storage/indexes |
| **Spec Version** | 1.0.0 |
| **Status** | 🟡 Review |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird v3.0 |
| **Authors** | ScratchBird Development Team |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/optimizer/index_advisor.cpp:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/index/bitmap_rle.cpp:1`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_heap_index_gc_integration.cpp:1`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_gin_index_gc.cpp:1`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_bitmap_index_gc.cpp:1`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_index_mga_compliance.cpp:1`

## Synopsis

This specification defines how all 28 index types in ScratchBird are maintained during DML operations (INSERT, UPDATE, DELETE). It covers the interaction between heap modifications, index updates, MGA visibility rules, and garbage collection.

## Index DML Support Matrix

| Index Type | INSERT | UPDATE | DELETE | Unique | GC Strategy |
|------------|--------|--------|--------|--------|-------------|
| B-Tree | Full | Full | Soft | Yes | Sweep + Merge |
| Hash | Full | Full | Soft | Yes | Sweep |
| HNSW | Full | Soft | Soft | No | Graph Repair |
| Full-Text | Pending | Soft | Soft | No | Pending Flush |
| GIN | Pending | Soft | Soft | No | Pending Flush |
| GiST | Full | Soft | Soft | Partial | Entry Removal |
| BRIN | Summarize | - | - | No | Resummarize |
| R-Tree | Full | Soft | Soft | No | Entry Removal |
| SP-GiST | Full | Soft | Soft | No | Entry Removal |
| Bitmap | Full | Full | Bit Clear | Yes | Bit Compaction |
| Columnstore | Bulk | Delta | Delta | No | Rebuild Zone |
| LSM | Full | Tombstone | Tombstone | Yes | Compaction |
| IVF | Full | Soft | Soft | No | List Cleanup |
| Zone Map | Auto | Auto | Auto | No | Auto-rebuild |
| ART | Full | Full | Full | Yes | Node Removal |
| Bloom | Full | - | - | No | Rebuild |
| Vector Flat | Full | Full | Soft | No | Array Compact |
| Vector Bin Flat | Full | Full | Soft | No | Array Compact |
| IVF Flat | Full | Soft | Soft | No | List Cleanup |
| Bin IVF Flat | Full | Soft | Soft | No | List Cleanup |
| IVF PQ | Full | Soft | Soft | No | List Cleanup |
| IVF SQ8 | Full | Soft | Soft | No | List Cleanup |
| IVF SQ8 Hybrid | Full | Soft | Soft | No | List Cleanup |
| RHNSW PQ | Full | Soft | Soft | No | Graph Repair |
| RHNSW SQ | Full | Soft | Soft | No | Graph Repair |
| ANNOY | Rebuild | Rebuild | Rebuild | No | Rebuild |
| NSG | Rebuild | Rebuild | Rebuild | No | Rebuild |
| DiskANN | Rebuild | Rebuild | Rebuild | No | Rebuild |

## Specification

### Data Structures

#### Index Maintenance Context

```cpp
// Source: INDEX_BUILD_AND_MAINTENANCE.md
struct IndexMaintenanceContext {
    ID index_id;                    // Index being maintained
    ID table_id;                    // Parent table
    IndexType type;                 // BTREE, HASH, GIN, etc.
    bool is_unique;                 // Unique constraint?
    bool is_primary;                // Primary key?
    bool deferred;                  // Deferred checking enabled?
    
    // Per-statement batching
    std::vector<IndexOperation> pending_ops;
    size_t batch_size_threshold;
};
```

#### Index Operation Types

```cpp
// Source: INDEX_BUILD_AND_MAINTENANCE.md
enum class IndexOperationType {
    INSERT,         // Add new index entry
    DELETE,         // Mark entry as dead (logical delete)
    UPDATE,         // Delete old + Insert new (atomic)
    CHECK_UNIQUE,   // Verify uniqueness constraint
};

struct IndexOperation {
    IndexOperationType op_type;
    TID tid;                        // Target row version
    std::vector<uint8_t> key_bytes; // Encoded index key
    uint64_t create_txid;           // Transaction creating this op
};
```

#### Deferred Constraint Queue

```cpp
// Source: INDEX_BUILD_AND_MAINTENANCE.md
struct DeferredConstraintCheck {
    ID index_id;
    std::vector<uint8_t> key_bytes;
    TID new_tid;                    // TID being inserted
    std::vector<TID> conflicting_tids; // Found duplicates
};
```

## Algorithms by Index Type

### B-Tree DML Integration

```
INSERT:
1. Extract key from row, encode to sortable bytes
2. Check unique constraint if applicable
3. Traverse to leaf using latch coupling
4. Insert entry with xmin = current_xid
5. Split if necessary, propagate upward

UPDATE:
1. If indexed column unchanged: return
2. Extract old_key from undo segment
3. Extract new_key from new row
4. If old_key != new_key:
   - Delete old entry (set xmax)
   - Insert new entry (set xmin)

DELETE:
1. Extract key from row
2. Find entry in B-tree
3. Set entry.xmax = current_xid (soft delete)
4. Physical removal during GC

GC:
1. Scan leaf pages left-to-right
2. For each entry where xmax < OIT:
   - Remove entry physically
   - Recompress prefixes
3. If page utilization < min_fill: merge/rebalance
```

### Hash Index DML Integration

```
INSERT:
1. Compute hash of key
2. Find bucket via directory
3. Insert HashEntry with he_xmin = current_xid
4. Split bucket if overflow

UPDATE:
1. Similar to B-tree: delete old, insert new
2. Handle hash collision chain

DELETE:
1. Find entry in bucket chain
2. Set he_xmax = current_xid

GC:
1. Scan all buckets
2. Remove entries with he_xmax < OIT
3. Compact buckets
4. Consider directory contraction
```

### HNSW/Vector Index DML Integration

```
INSERT:
1. Select layer using randomization
2. Find neighbors using greedy search
3. Create node with node_xmin = current_xid
4. Add bidirectional edges
5. Prune edges to maintain M limit

UPDATE:
1. Vector indexes typically don't support in-place update
2. Implement as DELETE + INSERT

DELETE:
1. Find node by TID
2. Set node_xmax = current_xid
3. Keep edges for snapshot isolation

GC:
1. Remove nodes where xmax < OIT
2. Update neighbor edges
3. May require graph repair
```

### GIN/Full-Text DML Integration

```
INSERT:
1. Extract keys using opclass.extractValue()
2. Append (key, TID) pairs to pending list
3. If pending_count >= pending_limit: trigger flush

UPDATE:
1. Add delete entries for old keys to pending
2. Add insert entries for new keys to pending

DELETE:
1. Add delete markers for all keys to pending

GC:
1. Flush pending list
2. Scan posting lists for dead TIDs
3. Rebuild posting trees if sparsity high
```

### BRIN DML Integration

```
INSERT:
1. No immediate action
2. Row goes to unsummarized range

UPDATE:
1. Mark range as needing resummarize

DELETE:
1. May affect min/max if extreme value

GC:
1. During VACUUM: resummarize ranges
2. Update zone maps with new min/max
```

### LSM-Tree DML Integration

```
INSERT:
1. Append to WAL
2. Insert into memtable (skip list)
3. Flush when memtable full

UPDATE:
1. Insert new version with sequence number
2. Old version remains in lower levels

DELETE:
1. Insert tombstone marker

GC (Compaction):
1. Merge levels, removing overwritten/deleted keys
2. Remove tombstones if safe (no snapshot needs them)
```

### Vector Quantized Indexes (IVF_PQ, IVF_SQ8, etc.)

```
INSERT:
1. Find nearest centroid(s)
2. Quantize vector
3. Add to inverted list

UPDATE:
1. May require re-clustering if distribution changes
2. Or: delete + insert

DELETE:
1. Remove from inverted list
2. May leave gaps (compaction needed)

GC:
1. Compact inverted lists
2. Remove empty lists
3. Consider retraining if drift significant
```

### Graph ANN Indexes (ANNOY, NSG, DiskANN)

```
All DML:
1. These indexes are typically read-only after build
2. INSERT/UPDATE/DELETE require full or partial rebuild
3. Recommend periodic batch rebuild for updates
```

## MGA Visibility Integration

Index entries must respect MGA visibility rules:

```cpp
// Source: INDEX_BUILD_AND_MAINTENANCE.md "MGA Visibility Algorithm"
bool isIndexEntryVisible(
    TID entry_tid,
    Snapshot snapshot,
    TransactionId current_txid
) {
    RowVersion* row = lookupRow(entry_tid);
    
    // Rule 1: Created by current transaction
    if (row->create_txid == current_txid) {
        return row->delete_txid != current_txid;
    }
    
    // Rule 2: Created by active transaction
    if (snapshot.active.contains(row->create_txid) ||
        row->create_txid >= snapshot.high) {
        return false;
    }
    
    // Rule 3: Creator not committed
    if (getTransactionState(row->create_txid) != COMMITTED) {
        return false;
    }
    
    // Rule 4: Not deleted
    if (row->delete_txid == 0) {
        return true;
    }
    
    // Rule 5: Deleted by current transaction
    if (row->delete_txid == current_txid) {
        return false;
    }
    
    // Rule 6: Deleted by active transaction
    if (snapshot.active.contains(row->delete_txid) ||
        row->delete_txid >= snapshot.high) {
        return true;
    }
    
    // Rule 7: Deleter committed
    if (getTransactionState(row->delete_txid) == COMMITTED) {
        return false;
    }
    
    // Rule 8: Default visible
    return true;
}
```

## Concurrency Control

### Latch Protocol

```
Index Insert:
1. Latch root in shared mode
2. Search to leaf, latch each level
3. Upgrade leaf to exclusive
4. If split needed:
   a. Upgrade parent to exclusive
   b. May propagate upgrades to root
5. Unlatch from bottom up

Index Delete (GC):
1. Latch leaf in exclusive mode
2. Remove dead entries
3. Unlatch

Constraint Check:
1. Latch root in shared mode
2. Search for key
3. If found:
   a. Keep latch while checking row visibility
   b. Release after visibility determined
```

### Deadlock Prevention

Index operations follow page ordering:
```
1. Always latch parent before child (top-down for search)
2. For splits, upgrade in place
3. For multi-index operations, acquire indexes in index_id order
```

## Invariants

| Invariant | Description | Verification |
|-----------|-------------|--------------|
| I1 | Every live heap row has index entries | Insert atomicity check |
| I2 | No duplicate unique keys visible | Constraint check at commit |
| I3 | Index entries point to valid TIDs | GC validation |
| I4 | Dead entries cleaned after OIT advances | GC watermark tracking |
| I5 | Index modifications logged for recovery | WAL/replay check |

## Error Handling

| Error Code | Condition | Recovery |
|------------|-----------|----------|
| `SB_ERR_DUPLICATE_KEY` | Unique constraint violation | Rollback statement |
| `SB_ERR_INDEX_CORRUPT` | Index/heap mismatch | Mark index invalid |
| `SB_ERR_DEADLOCK` | Lock ordering violation | Abort transaction |
| `SB_ERR_GC_IN_PROGRESS` | Concurrent GC blocking | Retry with backoff |

## Configuration

| Parameter | Default | Description |
|-----------|---------|-------------|
| `index.maintenance.batch_size` | 1000 | Rows per batch in bulk load |
| `index.unique.check_mode` | immediate | immediate/deferred |
| `index.gc.batch_size` | 10000 | Entries per GC batch |
| `index.gc.interval_ms` | 60000 | GC trigger interval |

## Test Coverage

| Test File | Coverage |
|-----------|----------|
| `test_heap_index_gc_integration.cpp:1` | Heap/GC integration |
| `test_gin_index_gc.cpp:1` | GIN-specific DML handling |
| `test_bitmap_index_gc.cpp:1` | Bitmap DML and GC |
| `test_hash_index_gc.cpp:1` | Hash index GC |
| `test_index_mga_compliance.cpp:1` | MGA visibility rules |
| `test_global_uniqueness_index.cpp:1` | Cross-index uniqueness |
| `test_hnsw_dml.cpp:1` | HNSW insert/update/delete |
| `test_ivf_dml.cpp:1` | IVF index maintenance |
| `test_lsm_dml.cpp:1` | LSM insert/compaction |

## Related Specifications

- [index_btree.md](./index_btree.md) - B-tree structure for inserts
- [index_gin.md](./index_gin.md) - GIN pending list handling
- [index_hnsw.md](./index_hnsw.md) - HNSW graph maintenance
- [index_lsm.md](./index_lsm.md) - LSM compaction

## Glossary

| Term | Definition |
|------|------------|
| MGA | Multi-Generational Architecture (MVCC) |
| OIT | Oldest Interesting Transaction (GC watermark) |
| Deferred Check | Constraint validation at commit time |
| Logical Delete | Mark as dead, physical removal later |
| TID | Tuple ID - pointer to row version |
| Spool | Temporary file for sorting bulk data |
| Soft Delete | Setting xmax without physical removal |

## Changelog

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2026-03-08 | Comprehensive DML coverage for all 28 index types |
