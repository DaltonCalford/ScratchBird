# Specification: SBLR Index Operations Opcodes

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | sblr |
| **Spec Version** | 1.0.0 |
| **Status** | 🟢 Approved |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird v3.0.0 |
| **Authors** | ScratchBird Team |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/v3_opcodes.generated.h:640-657`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/sblr/executor.cpp:624,660-690`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/sblr/executor.cpp:631-657`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/executor.h:171-198`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_sblr_jit_functions.cpp`

## Synopsis

This specification defines the Index operation opcodes for SBLR v3. These opcodes handle index maintenance (insert, update, delete), index scanning, and specialized index types including B-tree, Hash, GIN, GIST, HNSW, and Columnstore indexes.

## Scope

### In Scope

- Index maintenance during DML operations
- Index scanning and search
- Specialized index operations (GIN, HNSW, Columnstore)
- Index statistics and health checks
- Type-specific index operations

### Out of Scope

- Index DDL (CREATE/DROP INDEX) - see opcodes_ddl.md
- Query planning and optimization
- Storage engine internals

## Background

Indexes provide fast access paths to table data. SBLR supports multiple index types optimized for different workloads. The executor maintains indexes automatically during DML operations through the `updateIndexesOnInsert/Update/Delete` methods.

### Index Types Supported

| Type | Best For | Implementation |
|------|----------|----------------|
| B-tree | Equality, range, ordering | Balanced tree |
| Hash | Equality only | Hash table |
| GIN | Multi-value, full-text | Inverted index |
| GIST | Geospatial, range types | Generalized search tree |
| SP-GIST | Spatial partitioning | Space-partitioned GIST |
| BRIN | Large, naturally ordered tables | Block range index |
| R-tree | Spatial data | Rectangle-based tree |
| HNSW | Vector similarity | Hierarchical NSW graph |
| Bitmap | Low-cardinality columns | Bitmap vectors |
| Columnstore | Analytical workloads | Column-oriented storage |
| LSM-tree | Write-heavy workloads | Log-structured merge |

## Specification

### Index Maintenance Opcodes

| Opcode | Hex | Description |
|--------|-----|-------------|
| SBLR3_INDEX_INSERT | 0x0E10 | Insert entry into index |
| SBLR3_INDEX_DELETE | 0x0E0E | Delete entry from index |
| SBLR3_INDEX_UPDATE | 0x0E22 | Update index entry |
| SBLR3_INDEX_SCAN | 0x0E14 | Full index scan |
| SBLR3_INDEX_SCAN_START | 0x0E1A | Start conditional scan |
| SBLR3_INDEX_SCAN_NEXT | 0x0E18 | Advance scan position |
| SBLR3_INDEX_SCAN_END | 0x0E16 | End index scan |
| SBLR3_INDEX_SEARCH | 0x0E1C | Point lookup |
| SBLR3_INDEX_REINDEX | 0x0E12 | Rebuild index |
| SBLR3_INDEX_VACUUM | 0x0E24 | Clean up dead entries |
| SBLR3_INDEX_STATS | 0x0E1E | Collect statistics |
| SBLR3_INDEX_TYPE | 0x0E20 | Specify index type |
| SBLR3_INDEX_REF | 0x064C | Reference index by name/ID |

---

### Index Maintenance Functions

#### updateIndexesOnInsert

```cpp
// Source: src/sblr/executor.cpp:662-668
void updateIndexesOnInsert(
    uint64_t xid,                           // Transaction ID
    const ID& table_id,                     // Table being modified
    const TableInfo& table_info,            // Table metadata
    const std::vector<ColumnInfo>& all_columns,  // Column definitions
    uint32_t page_id,                       // Page containing tuple
    uint16_t item_id,                       // Item pointer within page
    const std::vector<Value>& row_values    // Column values
);
```

**Purpose**: Maintain all indexes after tuple insertion.

**Execution:**
```
1. Lookup table indexes from catalog
2. For each index on table:
   a. Determine if index is affected
   b. Extract index key values from row
   c. For expression indexes: evaluate expression
   d. Build index key tuple
   e. Check uniqueness constraints
   f. Insert key + TID into index
   g. Update index statistics
3. Record maintenance in transaction log
```

#### updateIndexesOnUpdate

```cpp
// Source: src/sblr/executor.cpp:670-678
void updateIndexesOnUpdate(
    uint64_t xid,
    const ID& table_id,
    const TableInfo& table_info,
    const std::vector<ColumnInfo>& all_columns,
    const std::vector<Value>& old_values,   // Pre-update values
    const std::vector<Value>& new_values,   // Post-update values
    TID old_tid,                            // Original tuple ID
    TID new_tid                             // New tuple ID (if moved)
);
```

**Purpose**: Maintain indexes after tuple update.

**Execution:**
```
1. Determine which columns changed
2. For each affected index:
   a. If indexed columns unchanged: skip (HOT optimization)
   b. Build old key from old_values
   c. Build new key from new_values
   d. Delete old key entry
   e. Insert new key entry
   f. Handle TID changes for moved tuples
3. Update statistics
```

#### updateIndexesOnDelete

```cpp
// Source: src/sblr/executor.cpp:680-686
void updateIndexesOnDelete(
    uint64_t xid,
    const ID& table_id,
    const TableInfo& table_info,
    const std::vector<ColumnInfo>& all_columns,
    const std::vector<Value>& row_values,   // Values of deleted row
    TID tid                                 // Tuple ID
);
```

**Purpose**: Remove index entries for deleted tuple.

**Execution:**
```
1. For each index on table:
   a. Extract key values from row_values
   b. Build index key
   c. Delete key + TID from index
   d. Mark entry as dead (for cleanup)
2. Schedule VACUUM if many dead entries
```

---

### SBLR3_INDEX_INSERT (0x0E10)

**Purpose**: Insert a single entry into an index.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| index_id | UUID | Target index |
| key_count | uint16_t | Number of key columns |
| keys | Value[] | Key values (encoded) |
| tid | TID | Tuple ID to associate |
| uniqueness_check | uint8_t | 0=skip, 1=enforce |

**B-tree Insert:**
```
1. Search for key position in tree
2. If unique constraint and key exists:
   - Raise uniqueness violation error
3. Insert key + TID at position
4. Split node if full
5. Propagate splits up tree
6. Update parent pointers
```

**Hash Insert:**
```
1. Compute hash of key
2. Find bucket for hash value
3. Add entry to bucket chain
4. Handle bucket overflow (expand or chain)
```

**GIN Insert:**
```
1. Extract elements from multi-value key
2. For each element:
   a. Find or create posting list
   b. Add TID to posting list
3. Update entry tree
```

---

### SBLR3_INDEX_DELETE (0x0E0E)

**Purpose**: Delete an entry from an index.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| index_id | UUID | Target index |
| key_count | uint16_t | Number of key columns |
| keys | Value[] | Key values |
| tid | TID | Specific TID to remove |
| exact_match | uint8_t | 0=delete all for key, 1=specific TID |

**Deletion Strategy:**
```
B-tree:
  1. Find key entry
  2. Mark as deleted (lazy delete)
  3. Or physically remove and rebalance

Hash:
  1. Compute hash
  2. Find bucket
  3. Remove entry from chain

GIN:
  1. Extract elements
  2. For each element's posting list:
     - Remove TID
  3. Clean up empty posting lists
```

---

### SBLR3_INDEX_SCAN (0x0E14)

**Purpose**: Full or range scan of index.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| index_id | UUID | Index to scan |
| scan_type | uint8_t | 1=FULL, 2=RANGE, 3=POINT |
| direction | uint8_t | 1=FORWARD, 2=BACKWARD |
| start_key_present | uint8_t | 0=no, 1=yes |
| start_key_inclusive | uint8_t | 0=exclusive, 1=inclusive |
| start_key_count | uint16_t | Start key column count |
| start_keys | Value[] | Start key values |
| end_key_present | uint8_t | 0=no, 1=yes |
| end_key_inclusive | uint8_t | 0=exclusive, 1=inclusive |
| end_key_count | uint16_t | End key column count |
| end_keys | Value[] | End key values |

**B-tree Scan Execution:**
```
1. Locate starting position:
   - For FULL: leftmost leaf (or rightmost for BACKWARD)
   - For RANGE: find start_key position
2. Follow leaf page chain
3. For each entry:
   a. Check if within range
   b. Return TID to caller
   c. Move to next entry
4. Stop at end of range or tree
```

---

### SBLR3_INDEX_SCAN_START / SCAN_NEXT / SCAN_END (0x0E1A, 0x0E18, 0x0E16)

**Purpose**: Iterator-style index scan.

**Execution Pattern:**
```cpp
// Cursor-based scan
INDEX_SCAN_START  // Initialize cursor
while (INDEX_SCAN_NEXT) {  // Advance and fetch
    // Process TID
}
INDEX_SCAN_END  // Cleanup
```

---

### SBLR3_INDEX_SEARCH (0x0E1C)

**Purpose**: Point lookup in index.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| index_id | UUID | Index to search |
| key_count | uint16_t | Number of key columns |
| keys | Value[] | Key values |
| multi_result | uint8_t | Allow multiple TIDs (non-unique) |

**Execution:**
```
1. Navigate to key position
2. If found:
   - Return associated TID(s)
3. If not found:
   - Return empty/no match
```

---

### SBLR3_INDEX_REINDEX (0x0E12)

**Purpose**: Rebuild index from scratch.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| index_id | UUID | Index to rebuild |
| concurrently | uint8_t | 0=locking, 1=non-blocking |

**Execution:**
```
Concurrently=0:
  1. Acquire exclusive lock on table
  2. Scan all table rows
  3. Build new index structure
  4. Replace old index atomically
  5. Release lock

Concurrently=1:
  1. Build new index in background
  2. Track changes with triggers/queue
  3. Apply changes to new index
  4. Brief lock for atomic swap
  5. Enable new index
```

---

### SBLR3_INDEX_VACUUM (0x0E24)

**Purpose**: Remove dead entries from index.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| index_id | UUID | Target index (0=all) |
| table_id | UUID | Target table |
| aggressive | uint8_t | 0=normal, 1=full cleanup |

**Execution:**
```
1. For each index page:
   a. Scan entries
   b. Check if TID is visible to any transaction
   c. Remove entries for dead tuples
2. Consolidate partially empty pages
3. Update free space maps
```

---

### Index Statistics (0x0E1E)

```cpp
// Source: src/sblr/executor.h:171-198
struct IndexMaintenanceStats {
    uint64_t entries_added = 0;
    uint64_t entries_removed = 0;
    uint64_t entries_updated = 0;
    uint64_t expression_evaluations = 0;
    uint64_t predicate_evaluations = 0;
    uint64_t invisible_skipped = 0;
    uint64_t indexes_maintained = 0;
    double total_eval_time_ms = 0.0;
    double total_insert_time_ms = 0.0;
    double total_remove_time_ms = 0.0;
};
```

**Purpose**: Track index maintenance metrics.

---

### Specialized Index Opcodes

#### GIN Index Operations

| Opcode | Hex | Description |
|--------|-----|-------------|
| SBLR3_GIN_INSERT | 0x0E06 | Insert to GIN index |
| SBLR3_GIN_SEARCH | 0x0E08 | Search GIN index |

**GIN Insert:**
```
For text/full-text:
  1. Tokenize text into lexemes
  2. For each lexeme:
     - Add TID to posting list
  3. Update entry tree

For array:
  1. Extract array elements
  2. For each element:
     - Add TID to posting list
```

**GIN Search:**
```
For & (AND) query:
  1. Find posting lists for each term
  2. Intersect lists
  3. Return matching TIDs

For | (OR) query:
  1. Find posting lists for each term
  2. Union lists
  3. Return matching TIDs
```

#### HNSW Index Operations (Vector Similarity)

| Opcode | Hex | Description |
|--------|-----|-------------|
| SBLR3_HNSW_INSERT | 0x0E0A | Insert vector to HNSW |
| SBLR3_HNSW_SEARCH | 0x0E0C | Approximate nearest neighbor search |

**HNSW Insert:**
```
1. Find entry point at top layer
2. Search for nearest neighbors in each layer
3. Add bidirectional connections
4. Prune connections if exceeds limit
5. Add to lower layers with probability
```

**HNSW Search:**
```
1. Start from entry point
2. Greedy search to find nearest in top layer
3. Use result as entry for next layer
4. Continue until bottom layer
5. Return k nearest neighbors
```

#### Columnstore Operations

| Opcode | Hex | Description |
|--------|-----|-------------|
| SBLR3_COLUMNSTORE_INSERT | 0x0E02 | Insert to columnstore |
| SBLR3_COLUMNSTORE_SCAN | 0x0E04 | Scan columnstore |

**Columnstore Insert:**
```
1. Append values to each column file
2. Update metadata
3. Flush when batch size reached
4. Compact periodically
```

---

### Index Type Specification (0x0E20)

**Purpose**: Declare index type for operations.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| index_type | uint8_t | Type code |
| params_len | uint16_t | Type-specific parameters length |
| params | bytes | Type-specific parameters |

**Index Type Codes:**
| Code | Type |
|------|------|
| 1 | BTREE |
| 2 | HASH |
| 3 | GIN |
| 4 | GIST |
| 5 | SPGIST |
| 6 | BRIN |
| 7 | RTREE |
| 8 | HNSW |
| 9 | BITMAP |
| 10 | COLUMNSTORE |
| 11 | LSM_TREE |

### Invariants

1. **Index Consistency**: After any DML, indexes match table data
2. **Uniqueness**: Unique indexes reject duplicate keys
3. **TID Validity**: All index entries point to valid or recently-dead tuples
4. **Visibility**: Index scans respect transaction visibility

## Error Handling

| Error Code | Condition | Recovery |
|------------|-----------|----------|
| `E_UNIQUE_VIOLATION` | Duplicate key in unique index | Fix data or use non-unique index |
| `E_INDEX_CORRUPTED` | Index structure corruption | REINDEX |
| `E_INDEX_OUT_OF_SPACE` | Cannot extend index | Add storage or VACUUM |

## Related Specifications

- [opcodes_ddl.md](./opcodes_ddl.md) - CREATE/DROP INDEX
- [opcodes_dml.md](./opcodes_dml.md) - DML with index maintenance

## Appendix

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
