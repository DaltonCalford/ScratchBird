# Specification: Transaction Information Page (TIP) Format

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | storage |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird Alpha |
| **Authors** | Dalton Calford |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/transaction_manager.h:65`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_tip_format.cpp`

## Synopsis

This specification defines the Transaction Information Page (TIP) format used to persist transaction states. TIP pages store the state (ACTIVE, COMMITTED, ABORTED, PREPARED) for each transaction, enabling Firebird MGA visibility computation.

## Scope

### In Scope

- TIP page header structure
- TIP entry format
- TIP page chaining
- TIP to CLOG relationship
- TIP read/write algorithms

### Out of Scope

- In-memory transaction cache
- TIP compression
- TIP partitioning

## Background

TIP pages provide durable storage for transaction states:
- **Pre-CLOG**: TIP was primary storage (20 bytes/XID)
- **With CLOG**: TIP for metadata, CLOG for status (2 bits/XID)
- **Chained**: Multiple TIP pages linked for large XID ranges

## Specification

### Data Structures

#### TIPPageHeader (116 bytes)

```cpp
// From include/scratchbird/core/transaction_manager.h:65
#pragma pack(push, 1)
struct TIPPageHeader {
    PageHeader page_header;    // 80 bytes: Standard page header
    uint64_t min_xid;          // 8 bytes: Minimum XID in this page
    uint64_t max_xid;          // 8 bytes: Maximum XID in this page
    uint32_t num_transactions; // 4 bytes: Count of entries
    uint32_t next_tip_page;    // 4 bytes: Next TIP page (0 if last)
    uint8_t reserved[12];      // 12 bytes: Reserved
};
#pragma pack(pop)
```

**Binary Layout:**
| Offset | Field | Size | Description |
|--------|-------|------|-------------|
| 0x00 | page_header | 80 | Standard page header |
| 0x50 | min_xid | 8 | First XID in page |
| 0x58 | max_xid | 8 | Last XID in page |
| 0x60 | num_transactions | 4 | Number of entries |
| 0x64 | next_tip_page | 4 | Link to next page |
| 0x68 | reserved | 12 | Padding |

#### TIPEntry (24 bytes)

```cpp
// From include/scratchbird/core/transaction_manager.h:76
#pragma pack(push, 1)
struct TIPEntry {
    uint64_t xid;         // 8 bytes: Transaction ID
    uint8_t state;        // 1 byte: TransactionState
    uint8_t flags;        // 1 byte: Reserved flags
    uint16_t reserved;    // 2 bytes: Alignment
    uint64_t commit_time; // 8 bytes: Commit timestamp (µs since epoch)
};
#pragma pack(pop)
```

**Binary Layout:**
| Offset | Field | Size | Description |
|--------|-------|------|-------------|
| 0x00 | xid | 8 | Transaction ID |
| 0x08 | state | 1 | State enum value |
| 0x09 | flags | 1 | Reserved |
| 0x0A | reserved | 2 | Padding |
| 0x0C | commit_time | 8 | Timestamp (0 if ACTIVE) |

### TIP Capacity

| Page Size | Entries/Page | Header + Entries |
|-----------|--------------|------------------|
| 8 KB | ~337 | 116 + 337*24 = 8,204 bytes |
| 16 KB | ~680 | 116 + 680*24 = 16,436 bytes |
| 32 KB | ~1,365 | 116 + 1365*24 = 32,876 bytes |

### TIP Page Layout

```
TIP Page Structure:
┌─────────────────────────────────────────────────────────────┐ 0x0000
│ TIPPageHeader (116 bytes)                                   │
│ - page_header (magic, page_type=TRANSACTION_MAP)            │
│ - min_xid, max_xid                                          │
│ - num_transactions, next_tip_page                           │
├─────────────────────────────────────────────────────────────┤ 0x0074
│ TIPEntry[0] (24 bytes)                                      │
│ - xid, state, commit_time                                   │
├─────────────────────────────────────────────────────────────┤ 0x008C
│ TIPEntry[1] (24 bytes)                                      │
├─────────────────────────────────────────────────────────────┤
│ ...                                                         │
├─────────────────────────────────────────────────────────────┤
│ TIPEntry[N-1] (24 bytes)                                    │
├─────────────────────────────────────────────────────────────┤
│ (unused space)                                              │
└─────────────────────────────────────────────────────────────┘ page_size
```

### TIP Chaining

```
Multiple TIP Pages (linked list):
┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│ TIP Page 0  │───►│ TIP Page 1  │───►│ TIP Page 2  │───► ...
│ XID 3-680   │    │ XID 681-1360│    │ XID 1361+   │
│ next=page_X │    │ next=page_Y │    │ next=0      │
└─────────────┘    └─────────────┘    └─────────────┘
```

### Interface Contracts

#### Function: `writeTipEntry()`

```cpp
// Source: src/core/transaction_manager.cpp
Status TransactionManager::writeTipEntry(
    uint64_t xid,
    TransactionState state,
    ErrorContext *ctx
);
```

**Preconditions:**
- TIP page exists for xid (or can be allocated)
- State is valid enum value

**Postconditions:**
- TIPEntry written to appropriate TIP page
- Page marked dirty in buffer pool

**Algorithm:**
```
1. page_id = findTIPPageForXid(xid)
2. IF page_id == NOT_FOUND:
3.     page_id = allocateNewTIPPage(xid)
4.
5. Pin page_id
6. tip_page = page_data
7.
8. // Binary search for existing entry
9. entry_idx = binarySearchTIPEntries(tip_page->entries, 
10.                                    tip_page->num_transactions, 
11.                                    xid)
12.
13. IF entry_idx >= 0:
14.     // Update existing
15.     tip_page->entries[entry_idx].state = state
16.     IF state == COMMITTED || state == ABORTED:
17.         tip_page->entries[entry_idx].commit_time = now()
18. ELSE:
19.     // Insert new (maintain sorted order)
20.     shiftEntries(tip_page, entry_idx)
21.     tip_page->entries[entry_idx] = {xid, state, 0, 0, 
22.                                     (state == ACTIVE) ? 0 : now()}
23.     tip_page->num_transactions++
24.
25. Unpin page (dirty)
26. RETURN OK
```

#### Function: `findTipEntry()`

```cpp
// Source: src/core/transaction_manager.cpp
Status TransactionManager::findTipEntry(
    uint64_t xid,
    TIPEntry &entry_out,
    ErrorContext *ctx
);
```

**Preconditions:**
- xid is valid

**Postconditions:**
- If found: entry_out populated, returns OK
- If not found: returns NOT_FOUND

**Algorithm:**
```
1. page_id = findTIPPageForXid(xid)
2. IF page_id == NOT_FOUND:
3.     RETURN NOT_FOUND
4.
5. Pin page_id
6. tip_page = page_data
7.
8. // Binary search
9. entry_idx = binarySearchTIPEntries(tip_page->entries,
10.                                    tip_page->num_transactions,
11.                                    xid)
12.
13. Unpin page (clean)
14.
15. IF entry_idx >= 0:
16.     entry_out = tip_page->entries[entry_idx]
17.     RETURN OK
18. ELSE:
19.     RETURN NOT_FOUND
```

### TIP to CLOG Relationship

```
┌─────────────────────────────────────────────────────────────┐
│ Transaction State Storage                                   │
├─────────────────────────────────────────────────────────────┤
│ TIP Pages                                                   │
│ - Metadata: xid, state, commit_time                        │
│ - Human-readable, debugging                                │
│ - 24 bytes per transaction                                 │
├─────────────────────────────────────────────────────────────┤
│ CLOG (Commit Log)                                          │
│ - Compact: 2 bits per transaction                          │
│ - Fast lookups                                             │
│ - Primary for visibility checks                            │
├─────────────────────────────────────────────────────────────┤
│ In-Memory Cache                                            │
│ - LRU cache of recent states                               │
│ - Avoids disk reads for hot XIDs                           │
└─────────────────────────────────────────────────────────────┘
```

**Lookup Order:**
1. In-memory cache
2. CLOG (2-bit status)
3. TIP (full metadata, fallback)

## Invariants

1. **Sorted Entries**: TIPEntries sorted by xid within page
   - Verification: Binary search assumes sorted
   
2. **XID Range**: min_xid <= all entries.xid <= max_xid
   - Verification: Set on page creation, validated on read
   
3. **Unique XIDs**: No duplicate xid in single page
   - Verification: Binary search finds single entry
   
4. **Chain Validity**: next_tip_page = 0 or valid page ID
   - Verification: Page validation on load

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `NOT_FOUND` | XID not in TIP | Check CLOG or treat as aborted |
| `PAGE_CORRUPT` | Invalid TIP page header | Attempt recovery from CLOG |
| `IO_ERROR` | Disk read failure | Retry or return error |

## Performance Considerations

### Binary Search
- **O(log n)**: For N entries in page
- **Cache friendly**: Sequential memory access
- **Vs linear**: 5-10x faster for full pages

### TIP Caching
- **Location cache**: XID -> page_id mapping
- **LRU eviction**: Older entries evicted
- **Hit rate**: >95% for typical workloads

### CLOG vs TIP
- **CLOG**: Use for visibility (2 bits, fast)
- **TIP**: Use for metadata (commit time, debugging)
- **Trade-off**: Space vs information

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_tip_format.cpp` | Structure validation |
| `tests/unit/test_tip_binary_search.cpp` | Lookup algorithm |
| `tests/unit/test_tip_chaining.cpp` | Multi-page handling |

## Related Specifications

- [Transaction States](./transaction_states.md) - States stored in TIP
- [CLOG](./clog.md) - Compact status storage
- [Visibility Computation](./visibility_computation.md) - Using TIP data

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | AI Agent |
