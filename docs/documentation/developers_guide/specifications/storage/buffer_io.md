# Specification: Buffer I/O

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

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/buffer_pool.h:227`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/buffer_pool.cpp`

## Synopsis

This specification defines buffer pool I/O operations including page reads, writes, prefetching, and write-back strategies. These operations form the interface between the buffer pool and the storage layer.

## Scope

### In Scope

- Synchronous page read/write
- Prefetching for sequential access patterns
- Write-back (dirty page flushing)
- Tablespace-aware I/O
- I/O error handling

### Out of Scope

- Asynchronous I/O (future enhancement)
- Direct I/O (O_DIRECT) support
- I/O scheduling algorithms

## Background

Buffer pool I/O uses standard POSIX file operations through the Database layer:

1. **Page Reads**: Synchronous read() calls with offset calculation
2. **Page Writes**: Synchronous write() followed by optional fsync()
3. **Prefetching**: Batched reads for TOAST chunks and sequential scans
4. **Tablespace I/O**: Routed to appropriate .sbts file based on GPID

## Specification

### Data Structures

#### I/O Statistics (BufferPool::StatsSnapshot)

```cpp
// From include/scratchbird/core/buffer_pool.h:279
struct StatsSnapshot {
    uint64_t hits = 0;              // Cache hits
    uint64_t misses = 0;            // Cache misses
    uint64_t evictions = 0;         // Pages evicted
    uint64_t flushes = 0;           // Pages flushed
    uint64_t evictions_clean = 0;   // Clean pages evicted
    uint64_t evictions_dirty = 0;   // Dirty pages evicted (requires write)
    uint64_t clock_sweeps = 0;      // Clock sweep cycles
    uint64_t bgwriter_pages_written = 0;  // Background writer pages
    double dirty_ratio_current = 0.0;     // Current dirty ratio
};
```

### Interface Contracts

#### Function: `readPageFromDisk()`

```cpp
// Source: src/core/buffer_pool.cpp
Status BufferPool::readPageFromDisk(
    GPID gpid,              // Global Page ID to read
    uint8_t *buffer,        // Buffer to receive page data
    ErrorContext *ctx
);
```

**Preconditions:**
- `buffer` is allocated with `page_size` bytes
- `gpid` is valid
- Storage file exists and is accessible

**Postconditions:**
- Page data read from disk into `buffer`
- Checksum validated (if PAGE_FLAG_CHECKSUM_VALID)
- stats_.misses incremented

**Error Handling:**
- `IO_ERROR`: Read failed (disk error, permission)
- `PAGE_CORRUPT`: Checksum mismatch or invalid magic

#### Function: `writePageToDisk()`

```cpp
// Source: src/core/buffer_pool.cpp
Status BufferPool::writePageToDisk(
    GPID gpid,              // Global Page ID to write
    const uint8_t *buffer,  // Page data to write
    ErrorContext *ctx
);
```

**Preconditions:**
- `buffer` contains valid page data
- `gpid` matches page_id in buffer header
- Page header checksum is computed

**Postconditions:**
- Page data written to disk at correct offset
- stats_.flushes incremented

**Algorithm:**
```
1. Calculate file offset: offset = gpid.page_num * page_size_
2. Determine file descriptor from gpid.tablespace_id
3. pwrite(fd, buffer, page_size_, offset)
4. IF write != page_size_: RETURN IO_ERROR
5. stats_.flushes++
6. RETURN OK
```

#### Function: `prefetchPagesGlobal()`

```cpp
// Source: src/core/buffer_pool.cpp:233
Status BufferPool::prefetchPagesGlobal(
    const std::vector<GPID> &gpids,  // Pages to prefetch
    ErrorContext *ctx,
    AccessStrategy strategy
);
```

**Preconditions:**
- `gpids` contains valid page IDs
- Vector size reasonable (< 1000 pages typical)

**Postconditions:**
- Pages loaded into buffer pool (if not already present)
- Pages pinned then immediately unpinned
- Pages remain in cache for subsequent access

**Algorithm:**
```
Input: gpids (vector of GPIDs)
Output: Status

1. filtered_gpids = []
2. FOR each gpid IN gpids:
3.     IF NOT isPageInPool(gpid):
4.         filtered_gpids.push_back(gpid)

5. FOR each gpid IN filtered_gpids:
6.     void *buffer
7.     status = pinPageGlobal(gpid, &buffer, ctx, strategy)
8.     IF status == OK:
9.         unpinPageGlobal(gpid, false, ctx)
10.    // Ignore errors - prefetch is best-effort

11. RETURN OK
```

**Use Cases:**
- TOAST chunk prefetching (all chunks of a large value)
- Index scan prefetching (leaf pages)
- Sequential table scans

#### Function: `flushPageGlobal()`

```cpp
// Source: src/core/buffer_pool.cpp
Status BufferPool::flushPageGlobal(
    GPID gpid,
    ErrorContext *ctx
);
```

**Preconditions:**
- Page is in buffer pool
- Page may be clean or dirty

**Postconditions:**
- If dirty: written to disk, is_dirty cleared
- If clean: no operation

#### Function: `flushAll()`

```cpp
// Source: src/core/buffer_pool.cpp
Status BufferPool::flushAll(ErrorContext *ctx);
```

**Preconditions:**
- Buffer pool initialized

**Postconditions:**
- All dirty pages written to disk
- `dirty_page_count_` becomes 0

**Algorithm:**
```
1. ACQUIRE mutex_
2. dirty_frames = []
3. FOR i FROM 0 TO frames_.size() - 1:
4.     IF frames_[i].is_dirty:
5.         dirty_frames.push_back(i)
6. RELEASE mutex_

7. FOR each frame_idx IN dirty_frames:
8.     frame = frames_[frame_idx]
9.     writePageToDisk(frame.gpid, frame.data, ctx)
10.    frame.is_dirty = false
11.    atomic decrement dirty_page_count_

12. RETURN OK
```

#### Function: `flushTablespace()`

```cpp
// Source: src/core/buffer_pool.cpp:252
Status BufferPool::flushTablespace(
    uint16_t tablespace_id,
    ErrorContext *ctx
);
```

**Preconditions:**
- Tablespace exists and is open
- No active transactions modifying tablespace pages (caller responsibility)

**Postconditions:**
- All dirty pages for tablespace written to disk

**Purpose:**
- Called before tablespace detach
- Ensures data durability across tablespace operations

### Algorithms

#### Algorithm: Page Offset Calculation

```
Input:  gpid, page_size
Output: file_offset

1. file_offset = gpid.page_num * page_size
2. tablespace_id = gpid.tablespace_id

3. IF tablespace_id == 0:
4.     fd = primary_database_fd_
5. ELSE:
6.     fd = tablespace_fds_[tablespace_id]

7. RETURN (fd, file_offset)
```

#### Algorithm: TOAST Prefetch Optimization

```
Input:  toast_pointer, xmin
Output: reconstructed_data

1. chunks_needed = ceil(toast_pointer.total_len / toast_pointer.chunk_size)
2. gpids = []
3. 
4. // Build list of chunk GPIDs from TOAST index
5. FOR seq FROM 0 TO chunks_needed - 1:
6.     gpid = lookupToastChunk(toast_pointer.lob_uuid, seq)
7.     gpids.push_back(gpid)

8. // Prefetch all chunks
9. prefetchPagesGlobal(gpids, ctx, AccessStrategy::Sequential)

10. // Now read chunks (will be cache hits)
11. data = []
12. FOR each gpid IN gpids:
13.     pinPageGlobal(gpid, &buffer, ctx)
14.     copy chunk data from buffer
15.     unpinPageGlobal(gpid, false, ctx)
16.     append to data

17. RETURN decompress(data)
```

## Invariants

1. **Write Atomicity**: Either entire page written or none (pwrite guarantees)
   - Verification: Check return value equals page_size
   
2. **Checksum Before Write**: All pages written have valid checksum
   - Verification: preparePageForWrite() called before writePageToDisk()
   
3. **Offset Alignment**: All I/O at page_size boundaries
   - Verification: Assert offset % page_size == 0

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `IO_ERROR` | pwrite/pread failure | Log errno, return error |
| `PAGE_CORRUPT` | Checksum mismatch on read | Log corruption, may retry |
| `PAGE_NOT_FOUND` | Page beyond file end | Extend file or return error |

## Performance Considerations

### Sequential vs Random I/O
- **Sequential prefetching**: Can achieve 200+ MB/s on SSD
- **Random I/O**: Typically 10-50 MB/s depending on pattern
- **Prefetch benefit**: 4-10x speedup for sequential access

### fsync Strategy
- **Per-page fsync**: Too expensive, causes write amplification
- **Batch fsync**: Flush multiple pages, then single fsync
- **Group commit**: Transactions batch TIP writes with single fsync

### Tablespace I/O
- **Separate files**: Each tablespace in separate .sbts file
- **Independent caching**: Buffer pool caches across tablespaces
- **Parallel I/O**: Different tablespaces can be read concurrently

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_buffer_pool_io.cpp` | Basic read/write |
| `tests/unit/test_buffer_pool_prefetch.cpp` | Prefetching |
| `tests/unit/test_tablespace_io.cpp` | Tablespace I/O |

## Related Specifications

- [Buffer Pool](./buffer_pool.md) - Core buffer pool management
- [File Layout](./file_layout.md) - On-disk file organization
- [TOAST Storage](./toast_storage.md) - Large value prefetching

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | AI Agent |
