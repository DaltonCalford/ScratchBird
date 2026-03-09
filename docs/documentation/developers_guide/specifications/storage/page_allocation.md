# Specification: Page Allocation

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

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/page_manager.h:35`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/page_manager.cpp`

## Synopsis

This specification defines page allocation and the Free Space Map (FSM) used by ScratchBird. The FSM tracks which pages are allocated using a bitmap stored on special bootstrap pages.

## Scope

### In Scope

- Free Space Map (FSM) structure and operations
- Page allocation/deallocation
- Tablespace-aware allocation
- FSM persistence and recovery
- File extension (autoextend)

### Out of Scope

- Heap page free space tracking (within-page free space)
- Index page free space tracking
- FSM free space prediction heuristics

## Background

The Free Space Map answers: "Which pages are allocated?"

Unlike PostgreSQL's FSM which tracks free space within pages, ScratchBird's FSM is a simple allocation bitmap:
- **0** = Free page
- **1** = Allocated page

Each tablespace has its own FSM stored on page 1 of the tablespace file.

## Specification

### Data Structures

#### FSMPage (Free Space Map Page)

```cpp
// From include/scratchbird/core/page_manager.h:375
struct FSMPage {
    PageHeader header;       // Standard 80-byte header
    uint32_t total_pages;    // Total pages tracked
    uint32_t free_pages;     // Count of free pages
    uint32_t next_fsm_page;  // Next FSM page (0 if last)
    uint8_t bitmap[];        // Variable-length bitmap
};
```

#### TablespaceFSM (In-Memory)

```cpp
// From include/scratchbird/core/page_manager.h:333
struct TablespaceFSM {
    uint32_t total_pages = 0;     // Total pages in tablespace
    uint32_t free_pages = 0;      // Number of free pages
    std::vector<uint8_t> bitmap;  // Allocation bitmap
    bool dirty = false;           // Needs flush?
};
```

#### TablespaceConfig

```cpp
struct TablespaceConfig {
    bool autoextend = true;           // Auto-grow enabled?
    uint32_t autoextend_size_mb = 64; // Growth increment
    uint64_t max_size_mb = 0;         // Max size (0 = unlimited)
    uint32_t prealloc_pages = 0;      // Pages to preallocate
};
```

### Interface Contracts

#### Function: `allocatePageInTablespace()`

```cpp
// Source: src/core/page_manager.cpp
Status PageManager::allocatePageInTablespace(
    uint16_t tablespace_id,
    GPID *gpid_out,
    ErrorContext *ctx
);
```

**Preconditions:**
- Tablespace exists (for tablespace_id > 0)
- FSM loaded for tablespace
- Either free pages exist or autoextend enabled

**Postconditions:**
- Free page found and marked allocated
- FSM marked dirty
- `*gpid_out` set to allocated GPID
- alloc_counter_ incremented

**Algorithm:**
```
1. IF tablespace_id == 0:
2.     ACQUIRE mutex_
3.     page_id = findFreePage()  // Scans bitmap
4.     IF page_id == NOT_FOUND:
5.         IF autoextend:
6.             extendFile(num_pages)
7.             page_id = findFreePage()
8.         ELSE:
9.             RELEASE mutex_
10.            RETURN PAGE_FULL
11.    setBit(page_id, true)
12.    free_pages_--
13.    dirty_ = true
14.    alloc_counter_++
15.    IF alloc_counter_ >= FLUSH_INTERVAL:
16.        flushUnlocked()
17.    RELEASE mutex_
18.    *gpid_out = makeGPID(0, page_id)
19. ELSE:
20.    // Tablespace allocation
21.    ACQUIRE tablespace_fsm_mutex_
22.    // Similar logic using tablespace_fsms_[tablespace_id]
23.    RELEASE tablespace_fsm_mutex_
24. RETURN OK
```

#### Function: `freePageGlobal()`

```cpp
// Source: src/core/page_manager.cpp
Status PageManager::freePageGlobal(
    GPID gpid,
    ErrorContext *ctx
);
```

**Preconditions:**
- Page is allocated in FSM
- Page is not in use (caller responsibility)

**Postconditions:**
- Page marked free in FSM
- FSM marked dirty
- free_pages_ incremented

#### Function: `findFreePage()`

```cpp
// Source: src/core/page_manager.cpp
uint32_t PageManager::findFreePage() const;
```

**Returns:**
- Page ID of first free page found
- UINT32_MAX if no free pages

**Algorithm:**
```
1. FOR each byte IN bitmap_:
2.     IF byte != 0xFF:  // Has at least one 0 bit
3.         FOR bit FROM 0 TO 7:
4.             IF (byte & (1 << bit)) == 0:
5.                 page_id = (byte_index * 8) + bit
6.                 IF page_id < total_pages_:
7.                     RETURN page_id
8. RETURN UINT32_MAX
```

**Complexity:**
- Time: O(total_pages / 8) - scans bitmap
- Space: O(1)

#### Function: `extendFile()`

```cpp
// Source: src/core/page_manager.cpp
Status PageManager::extendFile(
    uint32_t num_pages,
    ErrorContext *ctx
);
```

**Preconditions:**
- File can be extended (disk space available)
- num_pages > 0

**Postconditions:**
- File size increased by num_pages * page_size
- New pages marked free in FSM
- total_pages_ increased

**Algorithm:**
```
1. old_size = total_pages_ * page_size_
2. new_size = (total_pages_ + num_pages) * page_size_
3. 
4. // Use fallocate for efficient allocation (Linux)
5. #ifdef __linux__
6.     fallocate(fd, 0, old_size, num_pages * page_size_)
7. #else
8.     // Fallback: write zeros
9.     ftruncate(fd, new_size)
10.    zero_buffer = ALLOCATE(page_size_)
11.    FOR i FROM 0 TO num_pages - 1:
12.        pwrite(fd, zero_buffer, page_size_, old_size + (i * page_size_))
13.#endif

14. // Update FSM bitmap
15. old_bitmap_size = bitmap_.size()
16. new_bitmap_size = (new_size + 7) / 8
17. bitmap_.resize(new_bitmap_size)
18. // New bytes are zero (free)

19. total_pages_ += num_pages
20. free_pages_ += num_pages
21. dirty_ = true

22. RETURN OK
```

#### Function: `flush()`

```cpp
// Source: src/core/page_manager.cpp
Status PageManager::flush(ErrorContext *ctx);
```

**Postconditions:**
- FSM page written to disk
- dirty_ flag cleared
- Counters reset

### Algorithms

#### Algorithm: Bitmap Operations

**Set Bit (Allocate):**
```
setBit(page_id, allocated):
1. byte_idx = page_id / 8
2. bit_idx = page_id % 8
3. IF allocated:
4.     bitmap_[byte_idx] |= (1 << bit_idx)
5. ELSE:
6.     bitmap_[byte_idx] &= ~(1 << bit_idx)
```

**Get Bit (Check):**
```
getBit(page_id):
1. byte_idx = page_id / 8
2. bit_idx = page_id % 8
3. RETURN (bitmap_[byte_idx] & (1 << bit_idx)) != 0
```

#### Algorithm: Tablespace Creation

```
createTablespace(tablespace_id, name, path, config):
1. Validate inputs:
   - tablespace_id in [1, 65535]
   - name length <= 31
   - path is absolute

2. CREATE file at path (O_RDWR | O_CREAT | O_EXCL)

3. // Initialize TablespaceHeader (page 0)
4. header.magic = TABLESPACE_MAGIC
5. header.version = 1
6. header.page_size = page_size_
7. header.tablespace_id = tablespace_id
8. COPY name TO header.name
9. header.autoextend = config.autoextend
10. header.autoextend_size_mb = config.autoextend_size_mb
11. header.max_size_mb = config.max_size_mb
12. WRITE header to page 0

13. // Initialize FSM (page 1)
14. fsm.total_pages = 2  // Header + FSM pages
15. fsm.free_pages = 0
16. MARK pages 0 and 1 as allocated
17. WRITE fsm to page 1

18. IF config.prealloc_pages > 0:
19.    preallocatePages(tablespace_id, config.prealloc_pages)

20. REGISTER file descriptor in Database::tablespace_fds_
21. RETURN OK
```

## Invariants

1. **Bitmap Coverage**: `bitmap_.size() * 8 >= total_pages_`
   - Verification: Always maintained together
   
2. **Free Count Accuracy**: `free_pages_` equals actual count of 0 bits in bitmap
   - Verification: Periodically validated in debug builds
   
3. **Dirty Flag Consistency**: `dirty_ == true` iff FSM needs flush
   - Verification: Set on every modification, cleared on flush
   
4. **Reserved Pages**: Bootstrap pages (0-5) always marked allocated
   - Verification: Set during initialization, never freed

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `PAGE_FULL` | No free pages and autoextend disabled/offline | Return error, suggest VACUUM |
| `IO_ERROR` | File extend/write failure | Return error, may retry |
| `DISK_FULL` | No space for file extension | Return error, alert admin |
| `ALREADY_EXISTS` | Tablespace file already exists | Return error, use different path |

## Performance Considerations

### Bitmap Scanning
- **Naive scan**: O(total_pages) - acceptable for small databases
- **Word-at-a-time**: Check 64 bits at once using uint64_t
- **Free list cache**: Cache recently freed pages (future)

### File Extension
- **fallocate()**: Instant allocation on Linux (no zeroing)
- **posix_fallocate()**: Portable, may be slower
- **Manual zeroing**: Fallback, slowest option

### FSM Flush Frequency
- **Every 100 allocations**: Configurable threshold
- **On shutdown**: Always flush
- **Eager for critical ops**: Catalog page allocation

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_page_allocation.cpp` | Basic allocate/free |
| `tests/unit/test_page_manager.cpp` | FSM persistence |
| `tests/unit/test_tablespace.cpp` | Tablespace allocation |
| `tests/unit/test_autoextend.cpp` | File extension |

## Related Specifications

- [Page Types](./page_types.md) - Types of pages being allocated
- [File Layout](./file_layout.md) - On-disk file structure
- [FSM Management](./fsm_management.md) - FSM maintenance operations

## Appendix

### Bootstrap Page Map

| Page ID | Type | Purpose |
|---------|------|---------|
| 0 | DATABASE_HEADER | Database metadata |
| 1 | SYSTEM_STATE | Clean shutdown flag |
| 2 | CATALOG_ROOT | Catalog root |
| 3 | FSM_ROOT | Free Space Map |
| 4 | TX_MAP_ROOT | Transaction map |
| 5 | RESERVED | Reserved for future |

### Tablespace Header Layout

| Offset | Field | Size | Description |
|--------|-------|------|-------------|
| 0x00 | magic | 4 | 'SBTS' |
| 0x04 | version | 2 | Format version |
| 0x06 | page_size | 2 | Page size |
| 0x08 | tablespace_id | 2 | ID (1-65535) |
| 0x0A | name | 32 | Tablespace name |
| 0x2A | autoextend | 1 | Autoextend enabled |
| 0x2B | autoextend_size_mb | 4 | Growth increment |
| 0x2F | max_size_mb | 8 | Max size (0=unlimited) |
| 0x37 | total_pages | 4 | Current size |
| 0x3B | free_pages | 4 | Free count |

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | AI Agent |
