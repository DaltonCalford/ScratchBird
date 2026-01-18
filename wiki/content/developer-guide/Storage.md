# Storage

**Purpose:** Documents ScratchBird's storage engine - MGA-first design, buffer pool, heap pages, and indexing architecture.

**Status:** Alpha documentation (in progress)

---

## Design Philosophy

ScratchBird's storage engine is built on **MGA-first** principles:

- Write-after log (WAL) is optional and not required for recovery
- Back-versioning keeps old versions for readers (not forward-versioning like PostgreSQL)
- In-place updates with stable TIDs mean indexes rarely change
- Version chains go newest-to-oldest (N2O)

---

## Storage Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    STORAGE ENGINE                            │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────────────────────────────────────────────┐    │
│  │                   BUFFER POOL                        │    │
│  │  ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐           │    │
│  │  │Page │ │Page │ │Page │ │Page │ │Page │ ...       │    │
│  │  └─────┘ └─────┘ └─────┘ └─────┘ └─────┘           │    │
│  └─────────────────────────────────────────────────────┘    │
│                           │                                  │
│                           ▼                                  │
│  ┌─────────────────────────────────────────────────────┐    │
│  │              PAGE TYPES                              │    │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐            │    │
│  │  │   Heap   │ │  Index   │ │   TIP    │            │    │
│  │  │  Pages   │ │  Pages   │ │  Pages   │            │    │
│  │  └──────────┘ └──────────┘ └──────────┘            │    │
│  └─────────────────────────────────────────────────────┘    │
│                           │                                  │
│                           ▼                                  │
│  ┌─────────────────────────────────────────────────────┐    │
│  │                    DISK                              │    │
│  │  Database files (.sbd), Index files (.sbx)          │    │
│  └─────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
```

---

## Buffer Pool

**Location:** `src/core/buffer_pool.cpp`

The buffer pool caches database pages in memory for fast access.

### Configuration

```yaml
# In sb_server.conf
storage:
  buffer_pool_size: 256MB      # Total buffer pool memory
  page_size: 8192              # Page size in bytes (8KB default)
  checkpoint_threshold: 0.9    # Dirty page ratio to trigger checkpoint
```

### Supported Page Sizes

| Size | Use Case |
|------|----------|
| 4KB | Small databases, limited memory |
| 8KB | Default, balanced performance |
| 16KB | Large rows, OLAP workloads |
| 32KB | LOB-heavy applications |

### Buffer Pool Operations

```cpp
// Pin a page for reading
BufferFrame* pin_page(PageId page_id, PinMode mode);

// Unpin when done
void unpin_page(BufferFrame* frame);

// Mark page as dirty after modification
void mark_dirty(BufferFrame* frame);

// Force page to disk
void flush_page(PageId page_id);
```

### Eviction Policy

The buffer pool uses **clock sweep** (second-chance) eviction:

1. Pages have a reference bit
2. When accessed, reference bit is set to 1
3. During eviction, clock hand sweeps:
   - If reference bit = 1, set to 0 and skip
   - If reference bit = 0, evict (flush if dirty)

---

## Page Structure

### Common Page Header

All pages share a common header:

```cpp
struct PageHeader {
    uint32_t page_id;           // Page number
    uint8_t  page_type;         // PAGE_TYPE_HEAP, PAGE_TYPE_INDEX, etc.
    uint8_t  flags;             // Page flags
    uint16_t free_space;        // Free space in page
    uint32_t checksum;          // Page checksum for integrity
    TransactionId last_modified;// Transaction that last modified page
};
```

### Page Types

| Type | Code | Description |
|------|------|-------------|
| `PAGE_TYPE_HEAP` | 0x01 | Data records |
| `PAGE_TYPE_INDEX` | 0x02 | B-Tree/Hash index nodes |
| `PAGE_TYPE_TIP` | 0x03 | Transaction Inventory Page |
| `PAGE_TYPE_BLOB` | 0x04 | Large object storage |
| `PAGE_TYPE_FREE` | 0x00 | Free/unallocated page |

---

## Heap Pages

**Location:** `src/core/heap_page.cpp`, `include/scratchbird/core/heap_page.h`

Heap pages store table data with MGA back-versioning.

### Heap Page Layout

```
┌────────────────────────────────────────────────────────────┐
│                     PAGE HEADER (fixed)                     │
├────────────────────────────────────────────────────────────┤
│  Line Pointer Array (grows downward from header)            │
│  [LP0][LP1][LP2][LP3]...                                   │
├────────────────────────────────────────────────────────────┤
│                                                            │
│                    FREE SPACE                               │
│                                                            │
├────────────────────────────────────────────────────────────┤
│  Record Data (grows upward from bottom)                    │
│  ...data3...data2...data1...data0                          │
└────────────────────────────────────────────────────────────┘
```

### Record Header (rhd)

Each record has a header for MGA:

```cpp
struct RecordHeader {
    uint32_t rhd_transaction;   // Transaction ID that created this version
    uint32_t rhd_b_page;        // Back version page (0 = no back version)
    uint16_t rhd_b_line;        // Back version line number
    uint16_t rhd_flags;         // Record flags
    uint8_t  rhd_format;        // Format version
    uint8_t  rhd_data[];        // Variable-length record data
};
```

### Record Flags

```cpp
#define rhd_deleted    0x01     // Logically deleted
#define rhd_chain      0x02     // Has back version
#define rhd_fragment   0x04     // Multi-fragment record
#define rhd_incomplete 0x08     // First fragment
#define rhd_delta      0x10     // Delta-compressed back version
#define rhd_gc_active  0x20     // Being garbage collected
#define rhd_damaged    0x40     // Corrupted
```

---

## MGA Back-Versioning

ScratchBird uses **back-versioning** (Firebird-style), NOT forward-versioning (PostgreSQL-style).

### Update Flow

```
UPDATE salary FROM 50000 TO 60000:

BEFORE:
┌────────────────────────────────┐
│ Primary Record (Page 5, Line 3)│
│   rhd_transaction: 50          │
│   rhd_b_page: 0 (no back)      │
│   data: salary=50000           │
└────────────────────────────────┘

AFTER:
┌────────────────────────────────┐     ┌────────────────────────────────┐
│ Primary Record (Page 5, Line 3)│     │ Back Version (Page 7, Line 12) │
│   rhd_transaction: 100  (new)  │ ──▶ │   rhd_transaction: 50          │
│   rhd_b_page: 7                │     │   rhd_b_page: 0                │
│   rhd_b_line: 12               │     │   data: salary=50000 (old)     │
│   data: salary=60000  (new)    │     └────────────────────────────────┘
└────────────────────────────────┘

INDEXES UNCHANGED - still point to Page 5, Line 3
```

### Key Benefits

1. **Stable TIDs:** Index entries never need to change unless indexed column changes
2. **No index bloat:** Unlike PostgreSQL where 100 updates = 100 index entries
3. **Efficient reads:** Primary record always has newest data
4. **Predictable I/O:** Version chains grow separately from primary data

---

## Indexing

**Location:** `src/core/`, various index implementations

### Supported Index Types

| Index Type | Implementation | Status |
|------------|----------------|--------|
| B-Tree | `src/core/btree_index.cpp` | Alpha |
| Hash | `src/core/hash_index.cpp` | Alpha |
| Bitmap | `src/core/bitmap_index.cpp` | Alpha |
| GIN | `src/core/gin_index.cpp` | Alpha |
| GIST | `src/core/gist_index.cpp` | Alpha |
| SP-GIST | `src/core/spgist_index.cpp` | Alpha |

### Index Factory

```cpp
// Create index by type
std::unique_ptr<Index> IndexFactory::create(
    IndexType type,
    const IndexDefinition& def
);
```

### Index Update Rules (MGA)

Indexes are only updated when **indexed columns change**:

```cpp
void update_indexes(Table* table, TID primary_tid,
                   const Record* old_data, const Record* new_data) {
    for (Index* idx : table->indexes) {
        bool indexed_col_changed = false;
        for (uint32_t col : idx->columns) {
            if (old_data->columns[col] != new_data->columns[col]) {
                indexed_col_changed = true;
                break;
            }
        }

        if (indexed_col_changed) {
            // Only update if indexed column changed
            idx->remove(old_key, primary_tid);
            idx->insert(new_key, primary_tid);
        }
        // Otherwise, index entry remains unchanged
    }
}
```

---

## Free Space Management

### Free Space Map

Each table has a free space map (FSM) that tracks available space:

```cpp
// Find page with at least required_space bytes free
PageId find_free_page(TableId table_id, size_t required_space);

// Update FSM after insert/delete
void update_free_space(PageId page_id, size_t new_free_space);
```

### Visibility Map

Tracks pages where all tuples are visible to all transactions:

- Used for index-only scans
- Updated by VACUUM/sweep

---

## Large Objects (LOB/TOAST)

Records larger than ~2000 bytes are stored out-of-line:

```
┌─────────────────────────────────────────────────────────────┐
│ Normal Record                                               │
│   col1: INT (inline)                                        │
│   col2: VARCHAR(100) (inline)                               │
│   col3: TEXT -> [LOB pointer: page=100, offset=0]          │
└─────────────────────────────────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│ LOB Page (page 100)                                         │
│   Large text data stored here...                            │
└─────────────────────────────────────────────────────────────┘
```

---

## File Organization

### Database Files

```
/var/lib/scratchbird/data/
├── mydb.sbd              # Main database file
├── mydb.sbx              # Index file
├── mydb.sbm              # Free space map
├── mydb.sbv              # Visibility map
├── mydb.sbt              # TIP pages
└── mydb.sbl              # LOB storage
```

### On-Disk Format

Database files are organized as:

```
┌────────────────────────────────────────────────────────────┐
│ Database Header (Page 0)                                    │
│   - Magic number, version                                   │
│   - Page size, page count                                   │
│   - Transaction markers (OIT, OAT, OST, Next)              │
│   - Checksum algorithm                                      │
├────────────────────────────────────────────────────────────┤
│ TIP Pages (Pages 1-N)                                       │
│   - Transaction state bitmap (2 bits per TID)              │
├────────────────────────────────────────────────────────────┤
│ Catalog Pages                                               │
│   - System tables, indexes                                  │
├────────────────────────────────────────────────────────────┤
│ User Data Pages                                             │
│   - Heap pages, index pages                                 │
└────────────────────────────────────────────────────────────┘
```

---

## Checkpointing

Checkpointing writes dirty pages to disk:

```cpp
// Triggered when dirty_ratio > checkpoint_threshold
void checkpoint() {
    // 1. Acquire checkpoint lock
    // 2. Flush all dirty pages
    // 3. Update database header
    // 4. Sync to disk
    // 5. Release lock
}
```

### Checkpoint Configuration

```yaml
storage:
  checkpoint_interval: 300     # Seconds between checkpoints
  checkpoint_threshold: 0.9    # Dirty page ratio trigger
```

---

## Garbage Collection (Sweep)

Sweep removes obsolete back versions:

```cpp
void sweep_table(Table* table, TransactionId oit) {
    for (TID primary_tid : table->all_records()) {
        Record* primary = fetch_record(primary_tid);
        TID back_tid = TID(primary->rhd_b_page, primary->rhd_b_line);

        while (!is_null(back_tid)) {
            Record* back = fetch_record(back_tid);

            // If older than OIT, no transaction needs it
            if (back->rhd_transaction < oit) {
                TID next_back = TID(back->rhd_b_page, back->rhd_b_line);
                free_record(back_tid);
                primary->rhd_b_page = next_back.page;
                primary->rhd_b_line = next_back.line;
                back_tid = next_back;
            } else {
                break;  // Still needed
            }
        }
    }
}
```

---

## Source Code Reference

| Component | Header | Implementation |
|-----------|--------|----------------|
| Storage Engine | `include/scratchbird/core/storage_engine.h` | `src/core/storage_engine.cpp` |
| Buffer Pool | | `src/core/buffer_pool.cpp` |
| Heap Page | `include/scratchbird/core/heap_page.h` | `src/core/heap_page.cpp` |
| Bitmap Index | `include/scratchbird/core/bitmap_index.h` | `src/core/bitmap_index.cpp` |
| Index Factory | | `src/core/index_factory.cpp` |
| SP-GIST Index | | `src/core/spgist_index.cpp` |

---

## Related Documents

- [Transactions](Transactions.md) - MGA transaction model and TIP
- [Core Engine](Core-Engine.md) - Query execution and storage coordination
- [Architecture](Architecture.md) - Overall system design
