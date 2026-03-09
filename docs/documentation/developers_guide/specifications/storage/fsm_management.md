# Specification: FSM Management

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | storage |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird Alpha |
| **Authors** | Dalton Calford |

## Synopsis

This specification defines Free Space Map (FSM) management for tracking page allocation and maintaining FSM consistency through crashes and recovery.

## Scope

### In Scope

- FSM page structure
- FSM initialization and loading
- FSM updates on allocation/free
- FSM reconstruction from pages
- FSM vacuum (page density tracking)

### Out of Scope

- Heap page free space tracking (within-page)
- FSM compression
- Multi-level FSM tree

## Background

ScratchBird uses a simple bitmap-based FSM:
- **1 bit per page**: 0 = free, 1 = allocated
- **Stored on page 3**: Bootstrap FSM root
- **In-memory copy**: For fast lookups
- **Periodic flush**: To disk on changes

## Specification

### Data Structures

#### FSMPage Structure

```cpp
// From include/scratchbird/core/page_manager.h:375
struct FSMPage {
    PageHeader header;       // 80 bytes
    uint32_t total_pages;    // Pages tracked
    uint32_t free_pages;     // Free count
    uint32_t next_fsm_page;  // Chain (0 = last)
    uint8_t bitmap[];        // 1 bit per page
};
```

#### In-Memory FSM

```cpp
// From include/scratchbird/core/page_manager.h:333
struct TablespaceFSM {
    uint32_t total_pages = 0;
    uint32_t free_pages = 0;
    std::vector<uint8_t> bitmap;
    bool dirty = false;
};
```

### Bitmap Operations

```
Page Allocation Bitmap:
┌─────────────────────────────────────────────────────────────┐
│ Byte 0                                                      │
│ ┌──┬──┬──┬──┬──┬──┬──┬──┐                                   │
│ │P0│P1│P2│P3│P4│P5│P6│P7│  P0-P7 = Pages 0-7               │
│ └──┴──┴──┴──┴──┴──┴──┴──┘                                   │
│  1   1   0   1   0   0   1   0                             │
│  │   │   │   │   │   │   │   │                             │
│  A   A   F   A   F   F   A   F                              │
│                                                             │
│ A = Allocated (1), F = Free (0)                            │
└─────────────────────────────────────────────────────────────┘

Operations:
- allocatePage(p): bitmap[p/8] |= (1 << (p % 8))
- freePage(p): bitmap[p/8] &= ~(1 << (p % 8))
- isAllocated(p): return (bitmap[p/8] >> (p % 8)) & 1
```

### FSM Initialization

```
Algorithm: initializeFSM()

1. // For new database
2. total_pages = BOOTSTRAP_FIXED_PAGE_COUNT  // 6 pages
3. bitmap_size = (total_pages + 7) / 8       // 1 byte
4. 
5. bitmap_.resize(bitmap_size, 0xFF)  // All allocated
6. 
7. // Mark bootstrap pages as allocated
8. FOR p FROM 0 TO total_pages - 1:
9.     setBit(p, true)
10.
11. free_pages_ = 0
12. dirty_ = true
13. flush()  // Persist
```

### FSM Loading

```
Algorithm: loadFSM()

1. Pin FSM root page (page 3)
2. fsm_page = page_data
3. 
4. total_pages_ = fsm_page->total_pages
5. free_pages_ = fsm_page->free_pages
6. 
7. bitmap_size = (total_pages_ + 7) / 8
8. bitmap_.resize(bitmap_size)
9. COPY fsm_page->bitmap TO bitmap_
10.
11. Unpin page
12. dirty_ = false
```

### FSM Updates

```
Algorithm: updateFSMOnAllocate(page_id)

1. ACQUIRE mutex_
2. 
3. IF getBit(page_id):
4.     RELEASE mutex_
5.     RETURN ALREADY_ALLOCATED  // Shouldn't happen
6. 
7. setBit(page_id, true)
8. free_pages_--
9. dirty_ = true
10. alloc_counter_++
11. 
12. IF alloc_counter_ >= FLUSH_INTERVAL:
13.     flushUnlocked()
14.     alloc_counter_ = 0
15. 
16. RELEASE mutex_
17. RETURN OK
```

### FSM Reconstruction

After crash, FSM might be out of sync. Reconstruct:

```
Algorithm: reconstructFSM()

1. // Scan all pages to rebuild FSM
2. bitmap_.clear()
3. total_pages_ = getFileSize() / page_size_
4. bitmap_.resize((total_pages_ + 7) / 8, 0)
5. 
6. free_pages_ = 0
7. 
8. FOR page_id FROM 0 TO total_pages_ - 1:
9.     Pin page
10.    header = page_data
11.    
12.    // Check if page is in use
13.    IF header->magic == K_MAGIC_SBRD:
14.        setBit(page_id, true)
15.    ELSE:
16.        setBit(page_id, false)
17.        free_pages_++
18.    
19.    Unpin page
20.
21. dirty_ = true
22. flush()
23. 
24. LOG info: "FSM reconstructed: %u pages, %u free", 
25.           total_pages_, free_pages_
```

## Invariants

1. **Bitmap Coverage**: `bitmap.size() * 8 >= total_pages`
   - Verification: Resize when extending file
   
2. **Free Count**: `free_pages` equals count of 0 bits
   - Verification: Recalculate on reconstruction
   
3. **Dirty Tracking**: `dirty` true iff changes not flushed
   - Verification: Set on modification, clear on flush
   
4. **Bootstrap Pages**: Pages 0-5 always allocated
   - Verification: Never freed

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `FSM_MISMATCH` | FSM disagrees with actual pages | Reconstruct FSM |
| `IO_ERROR` | FSM page read failure | Reconstruct from pages |

## Performance Considerations

### Bitmap Scanning
- **Worst case**: O(total_pages) for allocation
- **Optimization**: Cache last allocated position
- **Parallel**: Could partition bitmap (future)

### Flush Frequency
- **Eager**: Flush every 100 allocations
- **On shutdown**: Always flush
- **Benefit**: Limits data loss window

### Memory Overhead
- **Bitmap**: 1 bit per page = 0.012% of database size
- **For 1TB DB**: ~125 MB bitmap
- **Acceptable**: For simplicity and reliability

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_fsm_basic.cpp` | Allocation/free |
| `tests/unit/test_fsm_reconstruction.cpp` | Recovery |
| `tests/unit/test_fsm_persistence.cpp` | Flush/load |

## Related Specifications

- [Page Allocation](./page_allocation.md) - Using FSM
- [Vacuum](./vacuum.md) - FSM updates during cleanup

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | AI Agent |
