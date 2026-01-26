# ScratchBird Storage Engine - Buffer Pool Management
## Part 1 of Storage Engine Specification

---

## IMPLEMENTATION STATUS: PARTIAL (ALPHA TARGET)

**Current Alpha Implementation (code-truth):**
- Fixed-size buffer pool (configurable); GPID-based page IDs
- Partitioned page table with per-partition locks
- Clock-sweep eviction with LRU fallback and dirty-page preference
- Adaptive background writer with dirty-ratio thresholds
- Dirty page counter + basic hit/miss/eviction/flush stats
- Buffer pool telemetry wiring (hits/misses/reads/writes + dirty/pages gauges)
- Page pin/unpin + per-page content mutex

**Not Yet Implemented (Alpha target in this spec):**
- Scan-resistant ring buffers (sequential scan / sweep-GC / bulk write)
- Read-ahead policy for large scans
- Young/old or midpoint insertion to prevent scan pollution
- Multiple buffer pools by workload or tablespace
- Adaptive hash index in the buffer pool

**Canonical cache architecture:** `docs/specifications/core/CACHE_AND_BUFFER_ARCHITECTURE.md`

This specification describes the **Alpha target** buffer-pool design for a single node.

See `include/scratchbird/core/buffer_pool.h` and `src/core/buffer_pool.cpp` for current implementation details.

---

## Overview

ScratchBird's buffer pool management combines an implemented clock-sweep core with planned PostgreSQL-style ring buffers, MySQL InnoDB midpoint insertion/adaptive hash concepts, Firebird-style page management efficiency, and SQL Server-style read-ahead. The Alpha target includes scan-resistance and read-ahead while keeping the design compatible with future multi-pool layouts.

## 1. Buffer Pool Architecture

**Note:** The structures in this section define the target architecture. The current Alpha implementation is in `include/scratchbird/core/buffer_pool.h` and `src/core/buffer_pool.cpp`.

### 1.1 Core Buffer Pool Structure

```c
// Master buffer pool manager
typedef struct sb_buffer_pool_manager {
    // Pool identification
    UUID            bpm_uuid;              // Buffer pool manager UUID
    
    // Multiple buffer pools for different workloads
    BufferPool**    bpm_pools;             // Array of pools
    uint16_t        bpm_pool_count;        // Number of pools
    
    // Global configuration
    uint64_t        bpm_total_memory;      // Total memory allocated
    uint64_t        bpm_total_pages;       // Total pages across all pools
    
    // Page size support (8K, 16K, 32K, 64K, 128K)
    uint32_t        bpm_page_sizes[5];     // Supported page sizes
    uint16_t        bpm_default_page_size_idx; // Default page size index
    
    // Global statistics
    BufferPoolStats bpm_global_stats;      // Aggregated statistics
    
    // Background workers
    pthread_t       bpm_writer_thread;     // Background writer
    pthread_t       bpm_checkpointer_thread; // Checkpointer
    pthread_t       bpm_stats_collector;   // Statistics collector
    
    // Synchronization
    pthread_mutex_t bpm_global_mutex;      // Global mutex
} SBBufferPoolManager;

// Individual buffer pool
typedef struct sb_buffer_pool {
    // Pool identity
    UUID            bp_uuid;               // Pool UUID
    char            bp_name[64];           // Pool name
    BufferPoolType  bp_type;               // Pool type
    
    // Memory allocation
    uint64_t        bp_size_bytes;         // Total size in bytes
    uint32_t        bp_page_size;          // Page size for this pool
    uint64_t        bp_n_buffers;          // Number of buffers
    
    // Buffer descriptors
    BufferDesc*     bp_descriptors;        // Buffer descriptor array
    char*           bp_buffer_memory;      // Actual buffer memory
    
    // Hash table for fast lookup
    BufferHashTable* bp_hash_table;        // Hash table
    uint32_t        bp_hash_size;          // Hash table size
    
    // Free list management
    BufferDesc*     bp_free_list_head;     // Head of free list
    uint32_t        bp_free_count;         // Number of free buffers
    pthread_mutex_t bp_free_list_mutex;    // Free list mutex
    
    // LRU/Clock management
    ClockSweep*     bp_clock_sweep;        // Clock sweep for eviction
    
    // Ring buffers (PostgreSQL-style)
    RingBuffer*     bp_ring_sequential;    // Sequential scan ring
    RingBuffer*     bp_ring_vacuum;        // Sweep/GC operations ring
    RingBuffer*     bp_ring_bulkwrite;     // Bulk write ring
    
    // Adaptive hash index (MySQL-style)
    AdaptiveHashIndex* bp_adaptive_hash;   // Fast lookups
    
    // Young/Old lists (MySQL-style)
    BufferDesc*     bp_young_list_head;    // Young (hot) pages
    BufferDesc*     bp_young_list_tail;
    uint64_t        bp_young_size;         // Young list size
    BufferDesc*     bp_old_list_head;      // Old (cold) pages
    BufferDesc*     bp_old_list_tail;
    uint64_t        bp_old_size;           // Old list size
    double          bp_young_old_ratio;    // Target ratio
    
    // Read-ahead (SQL Server-style)
    ReadAheadContext* bp_readahead;        // Read-ahead context
    
    // Direct I/O support
    bool            bp_use_direct_io;      // Use O_DIRECT
    AIOContext*     bp_aio_context;        // Async I/O context
    
    // Statistics
    BufferPoolStats bp_stats;              // Pool statistics
    
    // Synchronization
    pthread_rwlock_t bp_mapping_lock;      // Protects hash table
    pthread_mutex_t bp_clock_sweep_mutex;  // Protects clock sweep
} SBBufferPool;

// Buffer pool types
typedef enum buffer_pool_type {
    BP_DEFAULT,         // Default mixed workload
    BP_TRANSACTIONAL,   // OLTP workload
    BP_ANALYTICAL,      // OLAP workload
    BP_SEQUENTIAL,      // Sequential access
    BP_RANDOM,          // Random access
    BP_TEMPORARY,       // Temporary tables
    BP_LOG              // Reserved for optional write-after log (not used in MGA)
} BufferPoolType;
```

### 1.2 Buffer Descriptor

```c
// Buffer descriptor (metadata for each buffer)
typedef struct buffer_desc {
    // Buffer identification
    BufferTag       buf_tag;               // Page identifier
    uint32_t        buf_id;                // Buffer slot number
    
    // Buffer state
    BufferState     buf_state;             // Current state
    uint32_t        buf_flags;             // Buffer flags
    
    // Reference counting
    int32_t         buf_refcount;          // Pin count
    uint32_t        buf_usage_count;       // Usage count for eviction
    
    // LRU/Clock management
    struct buffer_desc* buf_lru_next;      // Next in LRU
    struct buffer_desc* buf_lru_prev;      // Previous in LRU
    
    // Content lock
    LWLock          buf_content_lock;      // Protects page content
    
    // I/O in progress
    bool            buf_io_in_progress;    // I/O operation active
    pthread_cond_t  buf_io_cv;             // Condition variable for I/O
    
    // Dirty tracking
    bool            buf_is_dirty;          // Buffer modified
    TransactionId   buf_dirty_xid;         // Transaction that dirtied
    LSN             buf_lsn;               // LSN of last change
    
    // Checksum
    uint32_t        buf_checksum;          // Page checksum
    
    // Access tracking
    time_t          buf_last_access;       // Last access time
    uint64_t        buf_access_count;      // Total accesses
    
    // Compression
    bool            buf_compressed;        // Page is compressed
    uint32_t        buf_compressed_size;   // Compressed size
    
    // Encryption
    bool            buf_encrypted;         // Page is encrypted
    uint8_t         buf_key_id;            // Encryption key ID
} BufferDesc;

// Buffer tag (identifies a page)
typedef struct buffer_tag {
    UUID            btag_db_uuid;          // Database UUID
    UUID            btag_table_uuid;       // Table UUID
    ForkNumber      btag_fork_num;         // Fork number (main, fsm, vm)
    BlockNumber     btag_block_num;        // Block number
} BufferTag;

// Buffer state
typedef enum buffer_state {
    BUF_STATE_EMPTY,        // Buffer is empty
    BUF_STATE_LOADING,      // Being loaded from disk
    BUF_STATE_VALID,        // Contains valid data
    BUF_STATE_DIRTY,        // Modified, needs write
    BUF_STATE_WRITING,      // Being written to disk
    BUF_STATE_ERROR         // I/O error occurred
} BufferState;

// Buffer flags
#define BUF_FLAG_HAS_SNAPSHOT   0x0001  // Has snapshot for MVCC
#define BUF_FLAG_PERMANENT      0x0002  // Permanent relation
#define BUF_FLAG_JUST_DIRTIED   0x0004  // Recently dirtied
#define BUF_FLAG_CHECKPOINT     0x0008  // Needs checkpoint
#define BUF_FLAG_PREFETCHED     0x0010  // Was prefetched
#define BUF_FLAG_WILL_NEED      0x0020  // Will be needed soon
```

### 1.3 Ring Buffer Implementation

```c
// Ring buffer for specific access patterns (PostgreSQL-style)
typedef struct ring_buffer {
    // Ring configuration
    uint32_t        ring_size;             // Size of ring
    BufferPoolType  ring_type;             // Type of operations
    
    // Ring buffers
    BufferDesc**    ring_buffers;          // Array of buffer descriptors
    uint32_t        ring_head;             // Current position
    uint32_t        ring_count;            // Buffers in ring
    
    // Statistics
    uint64_t        ring_hits;             // Hits within ring
    uint64_t        ring_misses;           // Misses requiring eviction
    
    // Synchronization
    pthread_mutex_t ring_mutex;            // Ring mutex
} RingBuffer;

// Create ring buffer for sequential scan
RingBuffer* create_ring_buffer(
    BufferPool* pool,
    uint32_t size,
    BufferPoolType type)
{
    RingBuffer* ring = allocate(sizeof(RingBuffer));
    
    ring->ring_size = MIN(size, pool->bp_n_buffers / 8);  // Max 1/8 of pool
    ring->ring_type = type;
    ring->ring_buffers = allocate(sizeof(BufferDesc*) * ring->ring_size);
    ring->ring_head = 0;
    ring->ring_count = 0;
    
    pthread_mutex_init(&ring->ring_mutex, NULL);
    
    return ring;
}

// Get buffer from ring
BufferDesc* ring_get_buffer(
    RingBuffer* ring,
    BufferTag* tag,
    BufferPool* pool)
{
    pthread_mutex_lock(&ring->ring_mutex);
    
    // Check if already in ring
    for (uint32_t i = 0; i < ring->ring_count; i++) {
        BufferDesc* buf = ring->ring_buffers[i];
        if (buffer_tag_equals(&buf->buf_tag, tag)) {
            ring->ring_hits++;
            pthread_mutex_unlock(&ring->ring_mutex);
            return buf;
        }
    }
    
    ring->ring_misses++;
    
    // Not in ring - need to allocate or evict
    BufferDesc* buf;
    
    if (ring->ring_count < ring->ring_size) {
        // Ring not full - get from free list
        buf = get_free_buffer(pool);
        ring->ring_buffers[ring->ring_count++] = buf;
    } else {
        // Ring full - reuse oldest
        buf = ring->ring_buffers[ring->ring_head];
        ring->ring_head = (ring->ring_head + 1) % ring->ring_size;
        
        // Evict if necessary
        if (buf->buf_is_dirty) {
            flush_buffer(pool, buf);
        }
    }
    
    pthread_mutex_unlock(&ring->ring_mutex);
    return buf;
}
```

## 2. Buffer Acquisition and Management

### 2.1 Buffer Fetch Operations

```c
// Fetch a page into buffer pool
BufferDesc* buffer_fetch_page(
    SBBufferPool* pool,
    BufferTag* tag,
    BufferAccessMode mode,
    BufferAccessStrategy strategy)
{
    // Step 1: Fast path - check hash table
    uint32_t hash = buffer_tag_hash(tag);
    BufferDesc* buf = hash_table_lookup(pool->bp_hash_table, tag, hash);
    
    if (buf != NULL) {
        // Found in cache
        return pin_buffer(pool, buf, mode);
    }
    
    // Step 2: Choose acquisition strategy
    switch (strategy) {
        case BAS_NORMAL:
            buf = get_buffer_normal(pool, tag);
            break;
            
        case BAS_SEQUENTIAL:
            buf = ring_get_buffer(pool->bp_ring_sequential, tag, pool);
            break;
            
        case BAS_VACUUM: // Sweep/GC access strategy (PostgreSQL alias)
            buf = ring_get_buffer(pool->bp_ring_vacuum, tag, pool);
            break;
            
        case BAS_BULKWRITE:
            buf = ring_get_buffer(pool->bp_ring_bulkwrite, tag, pool);
            break;
    }
    
    // Step 3: Load page from disk
    if (buf->buf_state == BUF_STATE_EMPTY) {
        load_buffer_from_disk(pool, buf, tag);
    }
    
    // Step 4: Pin and return
    return pin_buffer(pool, buf, mode);
}

// Pin buffer (increment reference count)
BufferDesc* pin_buffer(
    SBBufferPool* pool,
    BufferDesc* buf,
    BufferAccessMode mode)
{
    // Atomic increment of refcount
    int32_t old_refcount = atomic_fetch_add(&buf->buf_refcount, 1);
    
    if (old_refcount == 0) {
        // First pin - remove from free list if present
        remove_from_free_list(pool, buf);
    }
    
    // Update usage count for eviction algorithm
    buf->buf_usage_count = MIN(buf->buf_usage_count + 1, BM_MAX_USAGE_COUNT);
    
    // Update access tracking
    buf->buf_last_access = time(NULL);
    buf->buf_access_count++;
    
    // Acquire appropriate lock
    if (mode == BUFFER_ACCESS_EXCLUSIVE) {
        LWLockAcquire(&buf->buf_content_lock, LW_EXCLUSIVE);
    } else {
        LWLockAcquire(&buf->buf_content_lock, LW_SHARED);
    }
    
    // Update statistics
    pool->bp_stats.pin_count++;
    
    return buf;
}

// Unpin buffer
void unpin_buffer(
    SBBufferPool* pool,
    BufferDesc* buf)
{
    // Release content lock
    LWLockRelease(&buf->buf_content_lock);
    
    // Atomic decrement of refcount
    int32_t new_refcount = atomic_fetch_sub(&buf->buf_refcount, 1) - 1;
    
    if (new_refcount == 0) {
        // No longer pinned - candidate for eviction
        if (buf->buf_state == BUF_STATE_VALID && !buf->buf_is_dirty) {
            add_to_free_list(pool, buf);
        }
    } else if (new_refcount < 0) {
        // Error - negative refcount
        panic("Buffer refcount went negative");
    }
    
    // Update statistics
    pool->bp_stats.unpin_count++;
}
```

### 2.2 Buffer Eviction (Clock Sweep)

```c
// Clock sweep algorithm for buffer eviction
typedef struct clock_sweep {
    uint32_t        cs_hand;               // Clock hand position
    uint32_t        cs_passes;             // Passes made
    uint32_t        cs_evictions;          // Total evictions
    
    // Tuning parameters
    uint32_t        cs_max_passes;         // Max passes before force evict
    uint32_t        cs_usage_decrement;    // Usage count decrement
} ClockSweep;

// Find victim buffer using clock sweep
BufferDesc* clock_sweep_get_victim(
    SBBufferPool* pool)
{
    ClockSweep* cs = pool->bp_clock_sweep;
    uint32_t n_buffers = pool->bp_n_buffers;
    uint32_t passes = 0;
    
    pthread_mutex_lock(&pool->bp_clock_sweep_mutex);
    
    while (passes < cs->cs_max_passes) {
        BufferDesc* buf = &pool->bp_descriptors[cs->cs_hand];
        
        // Move clock hand
        cs->cs_hand = (cs->cs_hand + 1) % n_buffers;
        
        // Skip if pinned
        if (buf->buf_refcount > 0) {
            continue;
        }
        
        // Check usage count
        if (buf->buf_usage_count == 0) {
            // Found victim
            cs->cs_evictions++;
            pthread_mutex_unlock(&pool->bp_clock_sweep_mutex);
            
            // Flush if dirty
            if (buf->buf_is_dirty) {
                flush_buffer(pool, buf);
            }
            
            return buf;
        }
        
        // Decrement usage count
        buf->buf_usage_count -= cs->cs_usage_decrement;
        
        // Track passes
        if (cs->cs_hand == 0) {
            passes++;
        }
    }
    
    pthread_mutex_unlock(&pool->bp_clock_sweep_mutex);
    
    // Emergency: force evict least recently used unpinned buffer
    return force_evict_lru_buffer(pool);
}
```

## 3. Adaptive Hash Index (MySQL InnoDB-style)

### 3.1 Adaptive Hash Structure

```c
// Adaptive hash index for fast buffer lookups
typedef struct adaptive_hash_index {
    // Hash table
    AHIBucket**     ahi_buckets;           // Hash buckets
    uint32_t        ahi_n_buckets;         // Number of buckets
    uint32_t        ahi_n_entries;         // Total entries
    
    // Monitoring
    uint64_t        ahi_searches;          // Total searches
    uint64_t        ahi_hits;              // Hash hits
    uint64_t        ahi_misses;            // Hash misses
    double          ahi_hit_rate;          // Hit rate
    
    // Adaptation
    bool            ahi_enabled;           // Index enabled
    uint32_t        ahi_threshold;         // Access threshold
    uint64_t        ahi_build_time_us;     // Time to build
    
    // Memory management
    uint64_t        ahi_memory_used;       // Memory used
    uint64_t        ahi_memory_limit;      // Memory limit
    
    // Synchronization
    pthread_rwlock_t* ahi_bucket_locks;    // Per-bucket locks
} AdaptiveHashIndex;

// Adaptive hash bucket
typedef struct ahi_bucket {
    AHIEntry*       entries;                // Entry list
    uint32_t        n_entries;             // Entries in bucket
    pthread_rwlock_t lock;                  // Bucket lock
} AHIBucket;

// Adaptive hash entry
typedef struct ahi_entry {
    // Key (partial page content)
    uint64_t        key_hash;              // Hash of key columns
    uint16_t        key_offset;            // Offset in page
    uint16_t        key_length;            // Length of key
    
    // Value
    BufferDesc*     buffer;                // Buffer descriptor
    PageOffset      tuple_offset;          // Tuple offset in page
    
    // Statistics
    uint32_t        access_count;          // Access count
    time_t          last_access;           // Last access time
    
    // Chain
    struct ahi_entry* next;                // Next in bucket
} AHIEntry;

// Build adaptive hash index
void build_adaptive_hash(
    AdaptiveHashIndex* ahi,
    BufferPool* pool)
{
    uint64_t start_time = get_time_us();
    
    // Scan frequently accessed buffers
    for (uint32_t i = 0; i < pool->bp_n_buffers; i++) {
        BufferDesc* buf = &pool->bp_descriptors[i];
        
        // Only index frequently accessed pages
        if (buf->buf_access_count < ahi->ahi_threshold) {
            continue;
        }
        
        // Build hash entries for page
        build_page_hash_entries(ahi, buf);
    }
    
    ahi->ahi_build_time_us = get_time_us() - start_time;
    ahi->ahi_enabled = true;
}

// Lookup in adaptive hash
BufferDesc* adaptive_hash_lookup(
    AdaptiveHashIndex* ahi,
    uint64_t key_hash,
    uint16_t offset,
    uint16_t length)
{
    if (!ahi->ahi_enabled) {
        return NULL;
    }
    
    uint32_t bucket_idx = key_hash % ahi->ahi_n_buckets;
    AHIBucket* bucket = ahi->ahi_buckets[bucket_idx];
    
    pthread_rwlock_rdlock(&bucket->lock);
    
    AHIEntry* entry = bucket->entries;
    while (entry != NULL) {
        if (entry->key_hash == key_hash &&
            entry->key_offset == offset &&
            entry->key_length == length) {
            // Found
            entry->access_count++;
            entry->last_access = time(NULL);
            ahi->ahi_hits++;
            
            BufferDesc* buf = entry->buffer;
            pthread_rwlock_unlock(&bucket->lock);
            return buf;
        }
        entry = entry->next;
    }
    
    ahi->ahi_misses++;
    pthread_rwlock_unlock(&bucket->lock);
    return NULL;
}
```

## 4. Read-Ahead and Prefetching

### 4.1 Read-Ahead Context

```c
// Read-ahead context for predictive fetching
typedef struct readahead_context {
    // Configuration
    uint32_t        ra_max_pages;          // Maximum read-ahead pages
    uint32_t        ra_min_pages;          // Minimum read-ahead pages
    double          ra_growth_factor;      // Growth factor
    
    // Current state
    uint32_t        ra_current_size;       // Current read-ahead size
    BlockNumber     ra_last_block;         // Last block accessed
    BlockNumber     ra_next_block;         // Next expected block
    
    // Pattern detection
    AccessPattern   ra_pattern;            // Detected pattern
    uint32_t        ra_sequential_count;   // Sequential access count
    uint32_t        ra_random_count;       // Random access count
    
    // Performance tracking
    uint64_t        ra_hits;               // Successful predictions
    uint64_t        ra_misses;             // Failed predictions
    double          ra_hit_rate;           // Hit rate
    
    // Async I/O
    AIORequest*     ra_pending_requests;   // Pending I/O requests
    uint32_t        ra_n_pending;          // Number of pending
} ReadAheadContext;

// Access patterns
typedef enum access_pattern {
    AP_UNKNOWN,         // Unknown pattern
    AP_SEQUENTIAL,      // Sequential access
    AP_RANDOM,          // Random access
    AP_STRIDED,         // Strided access
    AP_REVERSE          // Reverse sequential
} AccessPattern;

// Perform read-ahead
void perform_readahead(
    SBBufferPool* pool,
    BufferTag* current_tag,
    ReadAheadContext* ra)
{
    BlockNumber current_block = current_tag->btag_block_num;
    
    // Update pattern detection
    if (current_block == ra->ra_next_block) {
        // Sequential access
        ra->ra_sequential_count++;
        ra->ra_hits++;
        
        if (ra->ra_sequential_count > 3) {
            ra->ra_pattern = AP_SEQUENTIAL;
            
            // Increase read-ahead size
            ra->ra_current_size = MIN(
                ra->ra_current_size * ra->ra_growth_factor,
                ra->ra_max_pages);
        }
    } else if (current_block == ra->ra_last_block - 1) {
        // Reverse sequential
        ra->ra_pattern = AP_REVERSE;
    } else {
        // Random access
        ra->ra_random_count++;
        ra->ra_misses++;
        
        if (ra->ra_random_count > 5) {
            ra->ra_pattern = AP_RANDOM;
            ra->ra_current_size = ra->ra_min_pages;
        }
    }
    
    // Issue read-ahead based on pattern
    switch (ra->ra_pattern) {
        case AP_SEQUENTIAL:
            readahead_sequential(pool, current_tag, ra);
            break;
            
        case AP_REVERSE:
            readahead_reverse(pool, current_tag, ra);
            break;
            
        case AP_STRIDED:
            readahead_strided(pool, current_tag, ra);
            break;
            
        case AP_RANDOM:
            // No read-ahead for random access
            break;
    }
    
    // Update state
    ra->ra_last_block = current_block;
    ra->ra_next_block = current_block + 1;
}

// Sequential read-ahead
void readahead_sequential(
    SBBufferPool* pool,
    BufferTag* current_tag,
    ReadAheadContext* ra)
{
    BufferTag prefetch_tag = *current_tag;
    
    for (uint32_t i = 1; i <= ra->ra_current_size; i++) {
        prefetch_tag.btag_block_num = current_tag->btag_block_num + i;
        
        // Check if already in buffer
        if (!buffer_in_pool(pool, &prefetch_tag)) {
            // Issue async read
            issue_async_read(pool, &prefetch_tag, ra);
        }
    }
}
```

## 5. Direct I/O and Async I/O

### 5.1 Direct I/O Support

```c
// Direct I/O context for bypassing OS cache
typedef struct direct_io_context {
    // Configuration
    bool            dio_enabled;           // Direct I/O enabled
    uint32_t        dio_alignment;         // Alignment requirement
    
    // Aligned buffers
    void*           dio_aligned_buffer;    // Aligned buffer for I/O
    size_t          dio_buffer_size;       // Buffer size
    
    // File descriptors with O_DIRECT
    int*            dio_fds;               // File descriptors
    uint32_t        dio_n_fds;             // Number of FDs
    
    // Statistics
    uint64_t        dio_reads;             // Direct reads
    uint64_t        dio_writes;            // Direct writes
    uint64_t        dio_bytes_read;        // Bytes read
    uint64_t        dio_bytes_written;     // Bytes written
} DirectIOContext;

// Perform direct I/O read
ssize_t direct_io_read(
    DirectIOContext* dio,
    int fd,
    void* buffer,
    size_t size,
    off_t offset)
{
    // Ensure alignment
    if ((uintptr_t)buffer % dio->dio_alignment != 0 ||
        size % dio->dio_alignment != 0 ||
        offset % dio->dio_alignment != 0) {
        // Use aligned buffer
        return aligned_pread(dio, fd, buffer, size, offset);
    }
    
    // Direct read
    ssize_t bytes = pread(fd, buffer, size, offset);
    
    if (bytes > 0) {
        dio->dio_reads++;
        dio->dio_bytes_read += bytes;
    }
    
    return bytes;
}
```

### 5.2 Async I/O Implementation

```c
// Async I/O context using io_uring (Linux) or IOCP (Windows)
typedef struct aio_context {
    // I/O submission queue
    #ifdef __linux__
    struct io_uring ring;                  // io_uring instance
    #elif _WIN32
    HANDLE          iocp;                  // I/O completion port
    #endif
    
    // Request tracking
    AIORequest*     aio_requests;          // Request array
    uint32_t        aio_max_requests;      // Maximum requests
    uint32_t        aio_pending_count;     // Pending requests
    
    // Completion handling
    pthread_t       aio_completion_thread; // Completion thread
    CompletionCallback aio_callback;       // Completion callback
    
    // Statistics
    uint64_t        aio_submitted;         // Requests submitted
    uint64_t        aio_completed;         // Requests completed
    uint64_t        aio_failed;            // Failed requests
} AIOContext;

// Async I/O request
typedef struct aio_request {
    // Request identification
    uint64_t        req_id;                // Request ID
    AIOOperation    req_op;                // Operation type
    
    // Buffer information
    BufferDesc*     req_buffer;            // Buffer descriptor
    void*           req_data;              // Data buffer
    size_t          req_size;              // Size
    off_t           req_offset;            // File offset
    
    // File descriptor
    int             req_fd;                // File descriptor
    
    // Completion
    bool            req_completed;         // Completed flag
    ssize_t         req_result;            // Result
    int             req_error;             // Error code
    
    // Callback
    void (*req_callback)(struct aio_request*); // Completion callback
    void*           req_user_data;         // User data
} AIORequest;

// Submit async read
void submit_async_read(
    AIOContext* aio,
    BufferDesc* buf,
    int fd,
    off_t offset)
{
    #ifdef __linux__
    // Using io_uring
    struct io_uring_sqe* sqe = io_uring_get_sqe(&aio->ring);
    
    AIORequest* req = allocate_aio_request(aio);
    req->req_buffer = buf;
    req->req_fd = fd;
    req->req_offset = offset;
    req->req_size = buf->buf_tag.btag_block_size;
    
    io_uring_prep_read(sqe, fd, buf->buf_data, 
                      req->req_size, offset);
    io_uring_sqe_set_data(sqe, req);
    
    io_uring_submit(&aio->ring);
    
    #elif _WIN32
    // Using Windows IOCP
    OVERLAPPED* overlapped = allocate(sizeof(OVERLAPPED));
    overlapped->Offset = offset & 0xFFFFFFFF;
    overlapped->OffsetHigh = offset >> 32;
    
    ReadFileEx(fd, buf->buf_data, req->req_size,
              overlapped, aio_completion_routine);
    #endif
    
    aio->aio_submitted++;
    aio->aio_pending_count++;
}

// Process async I/O completions
void* aio_completion_thread(void* arg) {
    AIOContext* aio = (AIOContext*)arg;
    
    #ifdef __linux__
    struct io_uring_cqe* cqe;
    
    while (!aio->shutdown) {
        int ret = io_uring_wait_cqe(&aio->ring, &cqe);
        
        if (ret == 0) {
            AIORequest* req = io_uring_cqe_get_data(cqe);
            req->req_result = cqe->res;
            req->req_completed = true;
            
            if (req->req_callback) {
                req->req_callback(req);
            }
            
            io_uring_cqe_seen(&aio->ring, cqe);
            
            aio->aio_completed++;
            aio->aio_pending_count--;
        }
    }
    #endif
    
    return NULL;
}
```

## 6. Buffer Pool Statistics and Monitoring

### 6.1 Statistics Collection

```c
// Buffer pool statistics
typedef struct buffer_pool_stats {
    // Hit rates
    uint64_t        hits;                  // Buffer hits
    uint64_t        misses;                // Buffer misses
    double          hit_rate;              // Hit rate percentage
    
    // I/O statistics
    uint64_t        reads;                 // Physical reads
    uint64_t        writes;                // Physical writes
    uint64_t        bytes_read;            // Bytes read
    uint64_t        bytes_written;         // Bytes written
    
    // Eviction statistics
    uint64_t        evictions;             // Pages evicted
    uint64_t        eviction_scans;        // Scans for eviction
    double          avg_eviction_scans;    // Average scans per eviction
    
    // Dirty page statistics
    uint64_t        dirty_pages;           // Current dirty pages
    uint64_t        pages_flushed;         // Pages flushed
    uint64_t        checkpoint_writes;     // Checkpoint writes
    uint64_t        background_writes;     // Background writer writes
    
    // Pin statistics
    uint64_t        pin_count;             // Total pins
    uint64_t        unpin_count;           // Total unpins
    uint64_t        pin_waits;             // Waits for pin
    
    // Read-ahead statistics
    uint64_t        readahead_pages;       // Pages read ahead
    uint64_t        readahead_hits;        // Read-ahead hits
    double          readahead_hit_rate;    // Read-ahead hit rate
    
    // Timing statistics
    uint64_t        total_read_time_us;    // Total read time
    uint64_t        total_write_time_us;   // Total write time
    double          avg_read_time_us;      // Average read time
    double          avg_write_time_us;     // Average write time
} BufferPoolStats;

// Collect buffer pool statistics
void collect_buffer_pool_stats(
    SBBufferPool* pool,
    BufferPoolStats* stats)
{
    memset(stats, 0, sizeof(BufferPoolStats));
    
    // Calculate hit rate
    uint64_t total_requests = pool->bp_stats.hits + pool->bp_stats.misses;
    if (total_requests > 0) {
        stats->hit_rate = (double)pool->bp_stats.hits / total_requests * 100.0;
    }
    
    // Count dirty pages
    for (uint32_t i = 0; i < pool->bp_n_buffers; i++) {
        BufferDesc* buf = &pool->bp_descriptors[i];
        if (buf->buf_is_dirty) {
            stats->dirty_pages++;
        }
    }
    
    // Calculate averages
    if (pool->bp_stats.reads > 0) {
        stats->avg_read_time_us = 
            pool->bp_stats.total_read_time_us / pool->bp_stats.reads;
    }
    
    if (pool->bp_stats.writes > 0) {
        stats->avg_write_time_us = 
            pool->bp_stats.total_write_time_us / pool->bp_stats.writes;
    }
    
    // Copy other statistics
    memcpy(stats, &pool->bp_stats, sizeof(BufferPoolStats));
}
```

### 6.2 Telemetry Wiring (Alpha)

The buffer pool emits metrics through the telemetry registry to support
`scratchbird_buffer_pool_*` counters and gauges. SQL monitoring views are
defined separately in the cache/buffer architecture spec.

**Code references:**
- `src/core/buffer_pool.cpp:190-330`
- `src/core/buffer_pool.cpp:1040-1100`

## 7. Integration Points

### 7.1 Integration with Storage Manager

```c
// Interface between buffer pool and storage
typedef struct buffer_storage_interface {
    // Read page from storage
    Status (*read_page)(StorageManager* storage,
                       BufferTag* tag,
                       void* buffer);
    
    // Write page to storage
    Status (*write_page)(StorageManager* storage,
                        BufferTag* tag,
                        void* buffer);
    
    // Extend relation
    BlockNumber (*extend_relation)(StorageManager* storage,
                                  UUID relation_uuid,
                                  uint32_t n_blocks);
    
    // Truncate relation
    Status (*truncate_relation)(StorageManager* storage,
                               UUID relation_uuid,
                               BlockNumber new_size);
} BufferStorageInterface;
```

### 7.2 Integration with Transaction Manager

```c
// Interface with transaction system
typedef struct buffer_transaction_interface {
    // Check visibility
    bool (*tuple_visible)(TransactionId xmin,
                         TransactionId xmax,
                         Snapshot* snapshot);
    
    // Get transaction status
    TransactionStatus (*get_xact_status)(TransactionId xid);
    
    // Register dirty page
    void (*register_dirty_page)(TransactionId xid,
                               BufferTag* tag,
                               LSN lsn);
} BufferTransactionInterface;
```

## Implementation Notes

This buffer pool implementation provides:

1. **Multiple buffer pools** for different workload types
2. **Ring buffers** for sequential access patterns (PostgreSQL-style)
3. **Adaptive hash index** for fast lookups (MySQL InnoDB-style)
4. **Clock sweep algorithm** for efficient eviction
5. **Read-ahead** with pattern detection (SQL Server-style)
6. **Direct I/O** and **async I/O** support for high performance
7. **Comprehensive statistics** for monitoring and tuning

The system is designed to handle multiple page sizes (8K-128K) and integrate seamlessly with the MGA transaction system described in the main specification.
