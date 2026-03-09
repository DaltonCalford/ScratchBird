# Specification: Buffer Pool

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

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/buffer_pool.h:43`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/buffer_pool.cpp`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_buffer_pool.cpp`

## Synopsis

This specification defines the ScratchBird Buffer Pool, an in-memory page cache that manages database pages using the Clock Sweep eviction algorithm. The buffer pool provides thread-safe page pinning, dirty page tracking, and background writeback.

## Scope

### In Scope

- Clock Sweep page replacement algorithm
- Frame allocation and eviction
- Page pinning/unpinning (legacy 32-bit and GPID-based APIs)
- Dirty page tracking and writeback
- Background writer thread
- Access strategies (Normal, Sequential, Vacuum, BulkWrite)

### Out of Scope

- WAL logging (see WAL specifications)
- Checkpoint coordination (see Checkpoint specification)
- Specific page type handling (handled by page managers)

## Background

The Buffer Pool caches database pages in memory to reduce disk I/O. Key design decisions:

1. **Clock Sweep Algorithm**: Provides O(1) eviction decisions with better performance than pure LRU
2. **Partitioned Page Table**: Reduces lock contention using 64 partitions
3. **Background Writer**: Proactively flushes dirty pages to prevent checkpoint storms
4. **GPID Support**: Full 64-bit Global Page ID support for multi-tablespace databases

## Specification

### Data Structures

#### Frame (Buffer Pool Entry)

```cpp
// From include/scratchbird/core/buffer_pool.h:345
struct Frame {
    GPID gpid = INVALID_GPID;                    // Global Page ID
    std::atomic<uint32_t> pin_count{0};          // Number of pins
    std::atomic<bool> is_dirty{false};           // Modified since read?
    std::atomic<uint32_t> usage_count{0};        // Clock Sweep counter (0-5)
    std::unique_ptr<uint8_t[]> data = nullptr;   // Page data buffer
    std::unique_ptr<std::mutex> content_mutex;   // Per-page content lock
    
    static constexpr uint32_t MAX_USAGE_COUNT = 5;
};
```

#### BufferPool::Config

```cpp
// From include/scratchbird/core/buffer_pool.h:62
struct Config {
    uint32_t pool_size = 32;              // Number of frames
    uint32_t page_size = 16384;           // Page size in bytes
    PoolLayout layout = PoolLayout::Single;
    
    // Background writer settings
    bool enable_background_writer = true;
    uint32_t bgwriter_delay_ms = 200;
    uint32_t bgwriter_max_pages = 100;
    double dirty_ratio_low = 0.25;        // Start flushing at 25%
    double dirty_ratio_high = 0.50;       // Aggressive at 50%
    double dirty_ratio_checkpoint = 0.75; // Emergency at 75%
};
```

#### AccessStrategy Enum

```cpp
// From include/scratchbird/core/buffer_pool.h:46
enum class AccessStrategy {
    Normal,     // Standard LRU behavior
    Sequential, // Sequential scan (don't pollute cache)
    Vacuum,     // VACUUM operation (special handling)
    BulkWrite   // Bulk load (write-heavy)
};
```

### Interface Contracts

#### Function: `pinPageGlobal()`

```cpp
// Source: src/core/buffer_pool.cpp
Status BufferPool::pinPageGlobal(
    GPID gpid,                    // Global Page ID to pin
    void **buffer,                // Output: pointer to page data
    ErrorContext *ctx,
    AccessStrategy strategy       // Access pattern hint
);
```

**Preconditions:**
- Buffer pool is initialized
- `gpid` is valid (not INVALID_GPID)
- `buffer` is non-null

**Postconditions:**
- If page in pool: pin_count incremented, usage_count set to MAX
- If page not in pool: loaded from disk, allocated frame, pinned
- `*buffer` points to page data

**Thread Safety:**
- Thread-safe (uses partitioned locks)
- Multiple threads can pin same page (pin_count tracks refs)

#### Function: `unpinPageGlobal()`

```cpp
// Source: src/core/buffer_pool.cpp
Status BufferPool::unpinPageGlobal(
    GPID gpid,                    // Page to unpin
    bool is_dirty,                // True if page was modified
    ErrorContext *ctx
);
```

**Preconditions:**
- Page is currently pinned by this caller
- `gpid` matches previously pinned page

**Postconditions:**
- pin_count decremented
- If `is_dirty`: is_dirty flag set (atomic OR)
- If pin_count reaches 0: frame eligible for eviction

#### Function: `allocatePageGlobal()`

```cpp
// Source: src/core/buffer_pool.cpp
Status BufferPool::allocatePageGlobal(
    uint16_t tablespace_id,       // Tablespace for new page
    GPID *gpid_out,               // Output: allocated GPID
    void **buffer,                // Output: page data pointer
    ErrorContext *ctx
);
```

**Preconditions:**
- PageManager can allocate new page in tablespace
- Buffer pool has free frame (or can evict)

**Postconditions:**
- New page allocated via PageManager
- Page pinned in buffer pool
- Page initialized (zeroed)
- `*gpid_out` and `*buffer` set

### Algorithms

#### Algorithm: Clock Sweep Eviction

```
Input:  (none - uses internal state)
Output: evicted_frame_index

1. WHILE true:
2.     frame = frames_[clock_hand_]
3.     
4.     IF frame.pin_count == 0:
5.         IF frame.usage_count > 0:
6.             frame.usage_count--
7.             clock_hand_ = (clock_hand_ + 1) % pool_size_
8.         ELSE:
9.             // Found victim
10.            IF frame.is_dirty:
11.                writePageToDisk(frame.gpid, frame.data)
12.                stats_.flushes++
13.            evicted_gpid = frame.gpid
14.            frame.gpid = INVALID_GPID
15.            frame.is_dirty = false
16.            clock_hand_ = (clock_hand_ + 1) % pool_size_
17.            RETURN clock_hand_ - 1 (modulo)
18.    ELSE:
19.        // Page is pinned, skip
20.        clock_hand_ = (clock_hand_ + 1) % pool_size_
21.    
22.    IF clock_hand_ wrapped around:
23.        stats_.clock_hand_resets++
```

**Complexity:**
- Time: O(1) amortized per eviction
- Space: O(1) additional

#### Algorithm: Pin Page

```
Input:  gpid, strategy
Output: buffer pointer

1. // Check page table (partitioned lock)
2. partition = getPartitionIndex(gpid)
3. ACQUIRE page_table_partitions_[partition].mutex
4. IF gpid IN page_table_partitions_[partition].table:
5.     frame_idx = page_table_partitions_[partition].table[gpid]
6.     frame = frames_[frame_idx]
7.     frame.pin_count++
8.     frame.usage_count = MAX_USAGE_COUNT
9.     RELEASE mutex
10.    stats_.hits++
11.    RETURN frame.data
12. RELEASE mutex

13. // Page not in pool - need to load
14. stats_.misses++

15. // Find victim frame (global lock)
16. ACQUIRE mutex_
17. status = evictPage(evicted_frame, ctx)
18. IF status != OK:
19.     RELEASE mutex_
20.     RETURN status

21. // Set up new frame
22. frame = frames_[evicted_frame]
23. frame.gpid = gpid
24. frame.pin_count = 1
25. frame.usage_count = MAX_USAGE_COUNT
26. frame.is_dirty = false

27. // Add to page table
28. ACQUIRE page_table_partitions_[partition].mutex
29. page_table_partitions_[partition].table[gpid] = evicted_frame
30. RELEASE page_table_partitions_[partition].mutex
31. RELEASE mutex_

32. // Read from disk (outside locks)
33. readPageFromDisk(gpid, frame.data, ctx)
34. 
35. RETURN frame.data
```

#### Algorithm: Background Writer

```
Input:  (runs periodically)
Output: (flushes dirty pages)

backgroundWriterMain():
1. WHILE NOT bgwriter_shutdown_:
2.     WAIT bgwriter_cv_ FOR bgwriter_delay_ms
3.     backgroundWriterFlush()

backgroundWriterFlush():
1. dirty_ratio = calculateDirtyRatio()
2. 
3. IF dirty_ratio < dirty_ratio_low_:
4.     RETURN  // Nothing to do

5. // Determine pages to flush based on ratio
6. IF dirty_ratio >= dirty_ratio_checkpoint_:
7.     pages_to_flush = min(bgwriter_max_pages_, dirty_page_count_)
8.     mode = EMERGENCY
9. ELSE IF dirty_ratio >= dirty_ratio_high_:
10.    pages_to_flush = min(bgwriter_max_pages_, dirty_page_count_ / 2)
11.    mode = AGGRESSIVE
12. ELSE:
13.    pages_to_flush = min(10u, dirty_page_count_ / 4)
14.    mode = NORMAL

15. flushed = 0
16. FOR each frame IN frames_:
17.     IF flushed >= pages_to_flush:
18.         BREAK
19.     IF frame.is_dirty AND frame.pin_count == 0:
20.         writePageToDisk(frame.gpid, frame.data)
21.         frame.is_dirty = false
22.         dirty_page_count_--
23.         flushed++

24. stats_.bgwriter_pages_written += flushed
25. IF flushed >= bgwriter_max_pages_:
26.     stats_.bgwriter_maxwritten++
```

### State Machines

```
Frame State Transitions:
┌───────────┐    pinPage()     ┌───────────┐
│  EMPTY    │ ───────────────► │  PINNED   │
│ (invalid) │                  │ (count=1) │
└───────────┘                  └─────┬─────┘
      ▲                            │
      │         unpinPage()        │ pinPage() (nested)
      └────────────────────────────┤
                                   │
                              ┌────┴────┐
                              │  PINNED  │
                              │(count>1) │
                              └────┬─────┘
                                   │ unpinPage()
                                   │ (count→0)
                                   ▼
                              ┌───────────┐
                              │  UNPINNED │
                              │(eligible  │
                              │for sweep) │
                              └───────────┘
```

| Current State | Event | Action | Next State |
|---------------|-------|--------|------------|
| EMPTY | pinPage (cache miss) | Allocate frame, load page | PINNED |
| EMPTY | pinPage (cache hit) | Impossible (no frame) | - |
| PINNED | pinPage | Increment pin_count | PINNED |
| PINNED | unpinPage | Decrement pin_count | PINNED or UNPINNED |
| UNPINNED | pinPage | Increment pin_count | PINNED |
| UNPINNED | Clock Sweep | Evict if usage_count=0 | EMPTY |

## Invariants

1. **Pin Count Non-Negative**: `pin_count >= 0` always
   - Verification: Atomic increment/decrement with underflow check
   
2. **Dirty Pages Tracked**: `dirty_page_count_` equals count of frames with `is_dirty=true`
   - Verification: Atomically updated on dirty flag transitions
   
3. **Page Table Consistency**: Page table entries point to valid frames containing correct GPID
   - Verification: Validated on every lookup
   
4. **Clock Hand Bounded**: `clock_hand_` always in range `[0, pool_size_)`
   - Verification: Modulo arithmetic on all updates

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `IO_ERROR` | Disk read/write failure | Return error, frame left in undefined state |
| `PAGE_CORRUPT` | Invalid page data read | Log error, may attempt re-read |
| `LOCK_TIMEOUT` | Cannot acquire partition lock | Retry with exponential backoff |

## Performance Considerations

### Clock Sweep vs LRU
- **LRU**: O(1) with list manipulation overhead
- **Clock Sweep**: O(1) with better cache locality (array vs list)
- **Benefit**: ~20% reduction in cache misses for sequential scans

### Partitioned Page Table
- **64 partitions**: Reduces lock contention by ~64x
- **Hash function**: Simple modulo on GPID (already well-distributed)

### Background Writer Tuning
- **Default delay**: 200ms between runs
- **Dirty ratio thresholds**: 25%/50%/75% for progressive flushing
- **Emergency mode**: Triggered at 75% to prevent fsync storms

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_buffer_pool.cpp` | Basic pin/unpin operations |
| `tests/unit/test_buffer_pool_clock_sweep.cpp` | Eviction algorithm |
| `tests/unit/test_buffer_pool_concurrent.cpp` | Thread safety |
| `tests/unit/test_buffer_pool_gpid.cpp` | GPID-based API |

## Related Specifications

- [Buffer I/O](./buffer_io.md) - Read/write and prefetch operations
- [Page Allocation](./page_allocation.md) - Page allocation via PageManager
- [File Layout](./file_layout.md) - On-disk page storage

## Appendix

### Glossary

| Term | Definition |
|------|------------|
| Frame | Slot in buffer pool holding one page |
| GPID | Global Page ID (64-bit: tablespace + page number) |
| Clock Sweep | Page replacement algorithm using usage counter |
| Pin | Reference count preventing eviction |
| Dirty Page | Page modified in memory, needs writeback |

### Statistics

| Statistic | Description |
|-----------|-------------|
| hits | Successful page table lookups |
| misses | Pages not in cache (disk read required) |
| evictions | Pages removed from cache |
| flushes | Dirty pages written to disk |
| clock_sweeps | Full cycles of clock hand |
| bgwriter_pages_written | Pages flushed by background writer |

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | AI Agent |
