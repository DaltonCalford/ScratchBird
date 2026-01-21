# ScratchBird Memory Management Specification

**Version:** 2.0
**Last Updated:** 2026-01-07
**Status:** ✅ Complete

**WAL Scope:** ScratchBird does not use write-after log (WAL) for recovery in Alpha; any WAL support is optional post-gold (replication/PITR).
Any WAL references in this document describe an optional post-gold stream for
replication/PITR only.
**Table Footnote:** In comparison tables below, ScratchBird WAL references are optional post-gold (replication/PITR).

---

## Table of Contents

1. [Overview](#1-overview)
2. [Memory Architecture](#2-memory-architecture)
3. [Memory Contexts](#3-memory-contexts)
4. [Buffer Pool Management](#4-buffer-pool-management)
5. [Shared Memory](#5-shared-memory)
6. [Memory Allocators](#6-memory-allocators)
7. [Cache Management](#7-cache-management)
8. [Memory Limits and Quotas](#8-memory-limits-and-quotas)
9. [Security Considerations](#9-security-considerations)
10. [Performance Optimization](#10-performance-optimization)
11. [Monitoring and Observability](#11-monitoring-and-observability)
12. [Debugging and Leak Detection](#12-debugging-and-leak-detection)
13. [Best Practices](#13-best-practices)
14. [Testing Requirements](#14-testing-requirements)

---

## 1. Overview

### 1.1. Purpose

This document specifies memory management in ScratchBird Database Engine, including:

- Memory allocation and deallocation strategies
- Buffer pool management for page caching
- Shared memory architecture for inter-process communication
- Memory contexts for lifetime management
- Cache management for metadata and query plans
- Memory limits, quotas, and security

### 1.2. Design Principles

ScratchBird's memory management is designed around these principles:

1. **Explicit Lifetimes** - Memory contexts make allocation lifetimes explicit
2. **Zero-Copy Where Possible** - Minimize memory copies in hot paths
3. **NUMA-Aware** - Optimize for multi-socket systems
4. **Thread-Safe** - Concurrent access without contention
5. **Secure by Default** - Zero sensitive memory, encrypt when needed
6. **Observable** - Rich metrics for monitoring and debugging

### 1.3. Key Components

```
ScratchBird Memory Architecture:
┌──────────────────────────────────────────────────────┐
│ Application Layer                                    │
│   - SQL Executor                                     │
│   - Query Optimizer                                  │
│   - Parser                                           │
└─────────────────┬────────────────────────────────────┘
                  │
┌─────────────────▼────────────────────────────────────┐
│ Memory Contexts                                      │
│   - Global, Database, Transaction, Temp              │
└─────────────────┬────────────────────────────────────┘
                  │
┌─────────────────▼────────────────────────────────────┐
│ Caches                                               │
│   - Catalog Cache                                    │
│   - Query Plan Cache                                 │
│   - Domain Validation Cache                          │
└─────────────────┬────────────────────────────────────┘
                  │
┌─────────────────▼────────────────────────────────────┐
│ Buffer Pool                                          │
│   - Page Cache (LRU/Clock)                           │
│   - Dirty Page Tracking                              │
│   - Prefetching                                      │
└─────────────────┬────────────────────────────────────┘
                  │
┌─────────────────▼────────────────────────────────────┐
│ Memory Allocators                                    │
│   - System malloc/free                               │
│   - jemalloc (optional)                              │
│   - Memory Pools                                     │
└─────────────────┬────────────────────────────────────┘
                  │
┌─────────────────▼────────────────────────────────────┐
│ Shared Memory (Cluster Mode)                         │
│   - Cluster Configuration                            │
│   - Lock Manager                                     │
│   - Statistics                                       │
└──────────────────────────────────────────────────────┘
```

### 1.4. Memory Regions

| Region | Lifetime | Thread Safety | Use Case |
|--------|----------|---------------|----------|
| **Global** | Process | Thread-safe | Configuration, read-only data |
| **Database** | Database connection | Thread-safe | Catalog cache, schemas |
| **Transaction** | Transaction | Thread-local | Per-transaction buffers |
| **Temporary** | Operation | Thread-local | Scratch space, sort buffers |
| **Shared** | Process | Thread-safe | IPC, cluster coordination |
| **Buffer Pool** | Process | Thread-safe | Page cache |

---

## 2. Memory Architecture

### 2.1. Memory Layout

**64-bit Address Space Layout:**

```
┌─────────────────────────────────────────────────┐
│ 0x0000000000000000 - 0x00007FFFFFFFFFFF         │
│ User Space (128 TB)                             │
│                                                 │
│ ┌─────────────────────────────────────────────┐ │
│ │ Stack (grows down)                          │ │
│ └─────────────────────────────────────────────┘ │
│                                                 │
│ ┌─────────────────────────────────────────────┐ │
│ │ Shared Libraries (.so)                      │ │
│ └─────────────────────────────────────────────┘ │
│                                                 │
│ ┌─────────────────────────────────────────────┐ │
│ │ Heap (malloc, grows up)                     │ │
│ │   - Memory Contexts                         │ │
│ │   - Caches                                  │ │
│ │   - Working Memory                          │ │
│ └─────────────────────────────────────────────┘ │
│                                                 │
│ ┌─────────────────────────────────────────────┐ │
│ │ Buffer Pool (mmap, large pages)             │ │
│ │   - Page Cache (configurable size)          │ │
│ │   - Pinned Pages                            │ │
│ └─────────────────────────────────────────────┘ │
│                                                 │
│ ┌─────────────────────────────────────────────┐ │
│ │ Shared Memory (POSIX shm, cluster mode)     │ │
│ │   - Cluster Config                          │ │
│ │   - Lock Tables                             │ │
│ │   - Statistics                              │ │
│ └─────────────────────────────────────────────┘ │
│                                                 │
│ ┌─────────────────────────────────────────────┐ │
│ │ Text Segment (.text)                        │ │
│ │   - Executable Code                         │ │
│ └─────────────────────────────────────────────┘ │
│                                                 │
│ ┌─────────────────────────────────────────────┐ │
│ │ Data Segment (.data, .bss)                  │ │
│ │   - Global Variables                        │ │
│ └─────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────┘
```

### 2.2. Memory Subsystems

**Core Subsystems:**

1. **Heap Allocator** - System malloc/free for general allocations
2. **Buffer Pool** - Page cache using mmap with huge pages
3. **Memory Contexts** - Hierarchical lifetime-based allocators
4. **Shared Memory** - POSIX shared memory for cluster coordination
5. **Caches** - LRU/CLOCK caches for metadata
6. **Memory Pools** - Fixed-size allocators for hot paths

### 2.3. MGA-Specific Memory Requirements

**ScratchBird uses Firebird MGA, not PostgreSQL write-after log (WAL, optional post-gold):**

- **No write-after log (WAL, optional post-gold) Buffers** - MGA doesn't require write-after log (WAL, optional post-gold) for recovery
- **Multi-Version Storage** - Pages contain multiple tuple versions
- **Transaction Marker Caching** - OIT, OAT, OST, NEXT in memory
- **Sweep Metadata** - Garbage collection state tracking

**Memory Implications:**

```cpp
// MGA transaction state (cached in memory)
struct TransactionState {
    uint64_t oit;        // Oldest Interesting Transaction
    uint64_t oat;        // Oldest Active Transaction
    uint64_t ost;        // Oldest Snapshot Transaction
    uint64_t next;       // Next Transaction ID

    // Transaction commit bitmap (in memory)
    std::vector<uint8_t> commit_bitmap;  // 1 bit per transaction
};
```

**Comparison with PostgreSQL:**

| Aspect | ScratchBird (MGA) | PostgreSQL (write-after log (WAL)) |
|--------|-------------------|------------------|
| **Write-after log (WAL) Buffers** | Not needed | 16 MB default |
| **Version Storage** | In-page (xmin/xmax) | TOAST + HOT |
| **Commit Log** | Commit bitmap | CLOG files |
| **Recovery** | No write-after log (WAL) replay | Write-after log (WAL) replay |
| **Memory Overhead** | Lower (no write-after log (WAL)) | Higher (write-after log (WAL) + buffers) |

---

## 3. Memory Contexts

### 3.1. Overview

**Memory Contexts** provide hierarchical, lifetime-based memory management.

**Benefits:**

- Automatic cleanup on scope exit
- Clear ownership semantics
- Prevents leaks in error paths
- Bulk deallocation (fast)

### 3.2. Context Types

```cpp
namespace scratchbird::memory {

enum class ContextType {
    GLOBAL,          // Process lifetime
    DATABASE,        // Per-database connection
    TRANSACTION,     // Per-transaction
    TEMPORARY,       // Per-operation (reset frequently)
    QUERY,           // Per-query execution
    SORT,            // Sort operation
    AGGREGATE,       // Aggregate operation
    CACHE            // Cache entry lifetime
};

} // namespace scratchbird::memory
```

### 3.3. Context API

```cpp
class MemoryContext {
public:
    // Create child context
    MemoryContext* create_child(
        ContextType type,
        const std::string& name,
        size_t initial_size = 8192
    );

    // Allocate memory
    void* alloc(size_t size);
    void* alloc_zero(size_t size);
    void* realloc(void* ptr, size_t new_size);

    // Free individual allocation
    void free(void* ptr);

    // Reset context (free all allocations)
    void reset();

    // Destroy context (must be empty or reset first)
    void destroy();

    // Statistics
    size_t total_allocated() const;
    size_t total_freed() const;
    size_t current_usage() const;
    size_t peak_usage() const;

private:
    ContextType type_;
    std::string name_;
    MemoryContext* parent_;
    std::vector<MemoryContext*> children_;

    // Allocation tracking
    size_t total_allocated_;
    size_t total_freed_;
    size_t peak_usage_;
};
```

### 3.4. Context Hierarchy

```
GlobalContext (process lifetime)
    │
    ├── DatabaseContext (per-database)
    │       │
    │       ├── TransactionContext (per-transaction)
    │       │       │
    │       │       ├── QueryContext (per-query)
    │       │       │       │
    │       │       │       ├── SortContext
    │       │       │       └── AggregateContext
    │       │       │
    │       │       └── TempContext (scratch space)
    │       │
    │       └── CacheContext (catalog cache entries)
    │
    └── SharedContext (shared memory, cluster mode)
```

### 3.5. Context Usage Example

```cpp
// Transaction context example
void execute_transaction(Database* db, const std::string& sql) {
    // Create transaction context
    MemoryContext* txn_ctx = db->context()->create_child(
        ContextType::TRANSACTION,
        "txn_" + std::to_string(current_tx_id())
    );

    try {
        // Parse query (allocates in txn_ctx)
        Query* query = parse_sql(sql, txn_ctx);

        // Create query context
        MemoryContext* query_ctx = txn_ctx->create_child(
            ContextType::QUERY,
            "query"
        );

        // Execute query (allocates in query_ctx)
        ResultSet* result = execute_query(query, query_ctx);

        // Commit transaction
        commit_transaction();

        // Cleanup query context
        query_ctx->destroy();

    } catch (const std::exception& e) {
        rollback_transaction();
        throw;

    } finally {
        // Cleanup transaction context (automatic)
        txn_ctx->destroy();
    }
}
```

### 3.6. Ownership Rules

**Rule 1: Creator Owns**

Whoever allocates memory is responsible for freeing it.

```cpp
// Caller owns returned memory
char* create_string(MemoryContext* ctx, const char* input) {
    char* str = (char*)ctx->alloc(strlen(input) + 1);
    strcpy(str, input);
    return str;  // Caller must free or context must be reset/destroyed
}
```

**Rule 2: Transfer Explicitly**

Use naming conventions to indicate ownership transfer.

```cpp
// Function takes ownership (will free)
void consume_buffer(MemoryContext* ctx, char* buffer) {
    // Use buffer...
    ctx->free(buffer);  // Function owns and frees
}

// Function borrows (will NOT free)
const char* get_name(void) {
    static char name[] = "ScratchBird";
    return name;  // Caller must NOT free
}
```

**Rule 3: Const Never Transfers**

Const parameters never transfer ownership.

```cpp
void process_data(const char* input);  // Does not take ownership
```

**Rule 4: RAII for C++**

Use RAII wrappers for automatic cleanup.

```cpp
class ContextGuard {
public:
    explicit ContextGuard(MemoryContext* parent, ContextType type)
        : ctx_(parent->create_child(type, "temp")) {}

    ~ContextGuard() { ctx_->destroy(); }

    MemoryContext* context() { return ctx_; }

private:
    MemoryContext* ctx_;
};

// Usage
void some_operation(MemoryContext* parent) {
    ContextGuard guard(parent, ContextType::TEMPORARY);

    // Allocate in temporary context
    void* buffer = guard.context()->alloc(1024);

    // ... use buffer ...

    // Automatic cleanup on scope exit
}
```

### 3.7. Thread Safety

**Global and Database contexts:**

- Must be thread-safe (use mutexes or per-thread arenas)
- Multiple threads may allocate concurrently

**Transaction and Temporary contexts:**

- Thread-local (no locking required)
- Each thread has its own contexts

```cpp
class MemoryContext {
    // Global/Database contexts use mutex
    std::mutex mutex_;  // Only for GLOBAL and DATABASE types

    void* alloc(size_t size) {
        if (type_ == ContextType::GLOBAL || type_ == ContextType::DATABASE) {
            std::lock_guard<std::mutex> lock(mutex_);
            return alloc_impl(size);
        } else {
            return alloc_impl(size);  // Thread-local, no lock
        }
    }
};
```

---

## 4. Buffer Pool Management

### 4.1. Overview

The **Buffer Pool** is a page cache that minimizes disk I/O by keeping frequently accessed pages in memory.

**Key Features:**

- Configurable size (default: 25% of system RAM)
- Multiple replacement policies (LRU, CLOCK, LRU-2)
- Dirty page tracking and asynchronous flushing
- Page pinning for active operations
- Prefetching and readahead
- Huge page support (2 MB pages)

### 4.2. Buffer Pool Architecture

```
Buffer Pool (e.g., 4 GB):
┌────────────────────────────────────────────────────┐
│ Buffer Descriptors (metadata)                      │
│   - Page ID, LSN, dirty bit, pin count, etc.      │
├────────────────────────────────────────────────────┤
│ Buffer Frames (page data)                          │
│   ┌──────────┬──────────┬──────────┬──────────┐   │
│   │ Page 0   │ Page 1   │ Page 2   │ Page 3   │   │
│   ├──────────┼──────────┼──────────┼──────────┤   │
│   │ 8 KB     │ 8 KB     │ 8 KB     │ 8 KB     │   │
│   └──────────┴──────────┴──────────┴──────────┘   │
│   │          │          │          │              │
│   │  ...     │  ...     │  ...     │  ...         │
└────────────────────────────────────────────────────┘
```

**Buffer Descriptor:**

```cpp
struct BufferDescriptor {
    PageID page_id;           // Page identifier
    uint64_t lsn;             // Last modification LSN
    uint32_t pin_count;       // Number of active references
    bool dirty;               // Modified since last flush
    bool valid;               // Contains valid data

    // Replacement policy metadata
    uint64_t last_access;     // Timestamp of last access (LRU)
    uint32_t access_count;    // Number of accesses (LRU-2)
    bool reference_bit;       // Reference bit (CLOCK)

    // Synchronization
    std::shared_mutex latch;  // Page latch (shared/exclusive)
};
```

### 4.3. Page Replacement Policies

#### LRU (Least Recently Used)

**Algorithm:**

```cpp
BufferFrame* evict_lru() {
    BufferDescriptor* victim = nullptr;
    uint64_t oldest = UINT64_MAX;

    for (auto& desc : buffer_descriptors_) {
        if (desc.pin_count == 0 && desc.last_access < oldest) {
            oldest = desc.last_access;
            victim = &desc;
        }
    }

    return evict_page(victim);
}
```

**Pros:** Simple, good for temporal locality
**Cons:** Sequential scans can pollute cache

#### CLOCK (Second-Chance)

**Algorithm:**

```cpp
BufferFrame* evict_clock() {
    while (true) {
        BufferDescriptor* desc = &buffer_descriptors_[clock_hand_];

        if (desc->pin_count == 0) {
            if (desc->reference_bit) {
                desc->reference_bit = false;  // Give second chance
            } else {
                return evict_page(desc);
            }
        }

        clock_hand_ = (clock_hand_ + 1) % buffer_pool_size_;
    }
}
```

**Pros:** Low overhead, approximates LRU
**Cons:** Less precise than LRU

#### LRU-2 (Two-Level LRU)

**Algorithm:**

```cpp
BufferFrame* evict_lru2() {
    // Prefer evicting pages accessed only once
    for (auto& desc : buffer_descriptors_) {
        if (desc.pin_count == 0 && desc.access_count == 1) {
            return evict_page(&desc);
        }
    }

    // Fallback to standard LRU
    return evict_lru();
}
```

**Pros:** Resists sequential scan pollution
**Cons:** Higher overhead

### 4.4. Buffer Pool Operations

#### Pin Page

```cpp
BufferFrame* pin_page(PageID page_id, AccessMode mode) {
    // 1. Check if page is in buffer pool
    BufferDescriptor* desc = lookup_page(page_id);

    if (desc != nullptr) {
        // Page is in buffer pool (cache hit)
        desc->pin_count++;
        desc->last_access = current_timestamp();
        desc->reference_bit = true;

        if (mode == AccessMode::EXCLUSIVE) {
            desc->latch.lock();
        } else {
            desc->latch.lock_shared();
        }

        return &buffer_frames_[desc - buffer_descriptors_];
    }

    // 2. Page not in buffer pool (cache miss)
    //    Find victim page to evict
    desc = find_victim_page();

    if (desc == nullptr) {
        throw BufferPoolFullException("All pages are pinned");
    }

    // 3. Evict victim page (flush if dirty)
    if (desc->dirty) {
        flush_page(desc);
    }

    // 4. Load new page from disk
    read_page_from_disk(page_id, desc);

    // 5. Pin page
    desc->page_id = page_id;
    desc->pin_count = 1;
    desc->dirty = false;
    desc->valid = true;
    desc->last_access = current_timestamp();
    desc->reference_bit = true;

    if (mode == AccessMode::EXCLUSIVE) {
        desc->latch.lock();
    } else {
        desc->latch.lock_shared();
    }

    return &buffer_frames_[desc - buffer_descriptors_];
}
```

#### Unpin Page

```cpp
void unpin_page(BufferFrame* frame, bool dirty) {
    BufferDescriptor* desc = get_descriptor(frame);

    if (dirty) {
        desc->dirty = true;
    }

    desc->latch.unlock();  // or unlock_shared
    desc->pin_count--;
}
```

#### Flush Dirty Pages

```cpp
void flush_dirty_pages() {
    for (auto& desc : buffer_descriptors_) {
        if (desc.dirty && desc.pin_count == 0) {
            flush_page(&desc);
        }
    }
}

void flush_page(BufferDescriptor* desc) {
    // Write page to disk
    write_page_to_disk(desc->page_id, &buffer_frames_[desc - buffer_descriptors_]);

    // Mark as clean
    desc->dirty = false;
}
```

### 4.5. Prefetching and Readahead

**Sequential Prefetching:**

```cpp
void prefetch_sequential(PageID start_page, size_t count) {
    std::vector<PageID> pages_to_prefetch;

    for (size_t i = 0; i < count; ++i) {
        PageID page_id = start_page + i;

        // Only prefetch if not already in buffer pool
        if (!is_page_cached(page_id)) {
            pages_to_prefetch.push_back(page_id);
        }
    }

    // Submit async I/O requests
    for (PageID page_id : pages_to_prefetch) {
        async_read_page(page_id);
    }
}
```

**Random Prefetching:**

```cpp
void prefetch_random(const std::vector<PageID>& pages) {
    // Sort by physical location on disk
    std::vector<PageID> sorted_pages = pages;
    std::sort(sorted_pages.begin(), sorted_pages.end());

    // Submit async I/O requests in disk order
    for (PageID page_id : sorted_pages) {
        if (!is_page_cached(page_id)) {
            async_read_page(page_id);
        }
    }
}
```

### 4.6. Buffer Pool Tuning

**Configuration Parameters:**

```ini
# Buffer pool size (bytes, or percentage of RAM)
buffer_pool_size = 4GB          # Absolute size
buffer_pool_size_pct = 25       # 25% of system RAM

# Replacement policy
buffer_replacement_policy = clock  # lru, clock, lru2

# Dirty page flushing
dirty_page_flush_interval_ms = 5000   # Flush every 5 seconds
dirty_page_max_pct = 80               # Flush when 80% dirty

# Prefetching
enable_prefetch = true
prefetch_window = 16            # Prefetch 16 pages ahead

# Huge pages (Linux)
use_huge_pages = true           # Use 2 MB huge pages
```

**Monitoring:**

```cpp
struct BufferPoolStats {
    size_t total_pages;
    size_t free_pages;
    size_t dirty_pages;
    size_t pinned_pages;

    uint64_t cache_hits;
    uint64_t cache_misses;
    double hit_rate;

    uint64_t evictions;
    uint64_t flushes;

    size_t memory_usage_bytes;
};
```

---

## 5. Shared Memory

### 5.1. Overview

**Shared Memory** enables inter-process communication (IPC) in cluster mode.

**Use Cases:**

- Cluster configuration and coordination
- Distributed lock manager
- Shared statistics and monitoring
- Transaction coordination (two-phase commit)

### 5.2. POSIX Shared Memory

**Creation:**

```cpp
int create_shared_memory(const char* name, size_t size) {
    // Create shared memory object
    int fd = shm_open(name, O_CREAT | O_RDWR, 0600);
    if (fd == -1) {
        throw std::runtime_error("Failed to create shared memory");
    }

    // Set size
    if (ftruncate(fd, size) == -1) {
        close(fd);
        throw std::runtime_error("Failed to set shared memory size");
    }

    return fd;
}

void* map_shared_memory(int fd, size_t size) {
    void* addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        throw std::runtime_error("Failed to map shared memory");
    }

    return addr;
}
```

**Usage:**

```cpp
// Process 1: Create and initialize
int fd = create_shared_memory("/scratchbird_cluster", 1024 * 1024);
void* shm = map_shared_memory(fd, 1024 * 1024);

ClusterConfig* config = (ClusterConfig*)shm;
config->node_count = 3;
config->shard_count = 16;

// Process 2: Attach
int fd = shm_open("/scratchbird_cluster", O_RDWR, 0600);
void* shm = mmap(NULL, 1024 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

ClusterConfig* config = (ClusterConfig*)shm;
printf("Node count: %d\n", config->node_count);
```

### 5.3. Shared Memory Layout

```
Shared Memory Region (1 MB example):
┌────────────────────────────────────────────────────┐
│ Header (64 bytes)                                  │
│   - Magic number, version, size                   │
├────────────────────────────────────────────────────┤
│ Cluster Configuration (256 bytes)                  │
│   - Node list, shard mapping, etc.                │
├────────────────────────────────────────────────────┤
│ Lock Manager (64 KB)                               │
│   - Distributed locks, wait queues                │
├────────────────────────────────────────────────────┤
│ Statistics (128 KB)                                │
│   - Per-node, per-shard stats                     │
├────────────────────────────────────────────────────┤
│ Transaction Coordinator (64 KB)                    │
│   - 2PC state, prepared transactions              │
├────────────────────────────────────────────────────┤
│ Free Space                                         │
│   (for future use)                                 │
└────────────────────────────────────────────────────┘
```

### 5.4. Synchronization

**Shared Memory Mutex:**

```cpp
// Use pthread mutex with PTHREAD_PROCESS_SHARED attribute
struct SharedData {
    pthread_mutex_t mutex;
    int counter;
};

void init_shared_mutex(SharedData* data) {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);

    pthread_mutex_init(&data->mutex, &attr);
    data->counter = 0;

    pthread_mutexattr_destroy(&attr);
}

void increment_counter(SharedData* data) {
    pthread_mutex_lock(&data->mutex);
    data->counter++;
    pthread_mutex_unlock(&data->mutex);
}
```

**Read-Write Lock:**

```cpp
struct SharedCounter {
    pthread_rwlock_t lock;
    int value;
};

void init_shared_rwlock(SharedCounter* counter) {
    pthread_rwlockattr_t attr;
    pthread_rwlockattr_init(&attr);
    pthread_rwlockattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);

    pthread_rwlock_init(&counter->lock, &attr);
    counter->value = 0;

    pthread_rwlockattr_destroy(&attr);
}

int read_counter(SharedCounter* counter) {
    pthread_rwlock_rdlock(&counter->lock);
    int value = counter->value;
    pthread_rwlock_unlock(&counter->lock);
    return value;
}

void write_counter(SharedCounter* counter, int new_value) {
    pthread_rwlock_wrlock(&counter->lock);
    counter->value = new_value;
    pthread_rwlock_unlock(&counter->lock);
}
```

---

## 6. Memory Allocators

### 6.1. System Allocator (malloc/free)

**Default allocator:** glibc malloc (ptmalloc2)

**Characteristics:**

- Thread-safe (per-thread arenas)
- General-purpose
- Moderate performance
- Good for mixed workloads

**Configuration:**

```cpp
// Tune malloc behavior via mallopt()
mallopt(M_MMAP_THRESHOLD, 128 * 1024);   // Use mmap for allocations > 128 KB
mallopt(M_TRIM_THRESHOLD, 256 * 1024);   // Trim free memory when > 256 KB
```

### 6.2. jemalloc (Optional)

**High-performance allocator:**

- Better multithreaded performance
- Lower fragmentation
- Rich statistics

**Usage:**

```bash
# Link with jemalloc
LD_PRELOAD=/usr/lib/libjemalloc.so.2 ./scratchbird-server
```

**Configuration:**

```bash
# jemalloc tuning
export MALLOC_CONF="narenas:4,tcache:false,dirty_decay_ms:5000"
```

### 6.3. Memory Pools

**Fixed-size allocator for hot paths:**

```cpp
template<size_t BlockSize, size_t PoolSize>
class MemoryPool {
public:
    MemoryPool() {
        pool_ = (uint8_t*)malloc(BlockSize * PoolSize);
        for (size_t i = 0; i < PoolSize; ++i) {
            free_list_.push_back(&pool_[i * BlockSize]);
        }
    }

    ~MemoryPool() {
        free(pool_);
    }

    void* allocate() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (free_list_.empty()) {
            return malloc(BlockSize);  // Fallback to heap
        }

        void* ptr = free_list_.back();
        free_list_.pop_back();
        return ptr;
    }

    void deallocate(void* ptr) {
        std::lock_guard<std::mutex> lock(mutex_);

        // Check if ptr belongs to pool
        if (ptr >= pool_ && ptr < pool_ + (BlockSize * PoolSize)) {
            free_list_.push_back(ptr);
        } else {
            free(ptr);  // From heap
        }
    }

private:
    uint8_t* pool_;
    std::vector<void*> free_list_;
    std::mutex mutex_;
};

// Usage: Pool for 64-byte blocks
MemoryPool<64, 1024> small_block_pool;

void* ptr = small_block_pool.allocate();
// ... use ptr ...
small_block_pool.deallocate(ptr);
```

### 6.4. Stack Allocator

**Fast allocator for temporary allocations:**

```cpp
class StackAllocator {
public:
    explicit StackAllocator(size_t size)
        : buffer_((uint8_t*)malloc(size)), size_(size), offset_(0) {}

    ~StackAllocator() { free(buffer_); }

    void* allocate(size_t n) {
        // Align to 8 bytes
        n = (n + 7) & ~7;

        if (offset_ + n > size_) {
            throw std::bad_alloc();
        }

        void* ptr = buffer_ + offset_;
        offset_ += n;
        return ptr;
    }

    void reset() {
        offset_ = 0;  // Fast reset
    }

private:
    uint8_t* buffer_;
    size_t size_;
    size_t offset_;
};

// Usage
StackAllocator temp_alloc(64 * 1024);  // 64 KB

void process_query() {
    void* buf1 = temp_alloc.allocate(1024);
    void* buf2 = temp_alloc.allocate(2048);

    // ... use buffers ...

    temp_alloc.reset();  // Fast cleanup
}
```

---

## 7. Cache Management

### 7.1. Catalog Cache

**Caches database metadata:**

- Tables, indexes, domains
- Security policies (RLS/CLS)
- Sequences, views, procedures

**LRU Cache Implementation:**

```cpp
template<typename Key, typename Value>
class LRUCache {
public:
    explicit LRUCache(size_t capacity) : capacity_(capacity) {}

    bool get(const Key& key, Value& value) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = cache_.find(key);
        if (it == cache_.end()) {
            return false;  // Cache miss
        }

        // Move to front (most recently used)
        lru_list_.splice(lru_list_.begin(), lru_list_, it->second.lru_iter);

        value = it->second.value;
        return true;  // Cache hit
    }

    void put(const Key& key, const Value& value) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = cache_.find(key);
        if (it != cache_.end()) {
            // Update existing entry
            it->second.value = value;
            lru_list_.splice(lru_list_.begin(), lru_list_, it->second.lru_iter);
            return;
        }

        // Evict if at capacity
        if (cache_.size() >= capacity_) {
            evict_lru();
        }

        // Insert new entry
        lru_list_.push_front(key);
        cache_[key] = {value, lru_list_.begin()};
    }

    void evict(const Key& key) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = cache_.find(key);
        if (it != cache_.end()) {
            lru_list_.erase(it->second.lru_iter);
            cache_.erase(it);
        }
    }

private:
    void evict_lru() {
        if (lru_list_.empty()) return;

        Key key = lru_list_.back();
        lru_list_.pop_back();
        cache_.erase(key);
    }

    struct Entry {
        Value value;
        std::list<Key>::iterator lru_iter;
    };

    size_t capacity_;
    std::unordered_map<Key, Entry> cache_;
    std::list<Key> lru_list_;
    std::mutex mutex_;
};

// Usage
LRUCache<TableOID, TableMetadata> catalog_cache(1000);

TableMetadata metadata;
if (catalog_cache.get(table_oid, metadata)) {
    // Cache hit
} else {
    // Cache miss - load from disk
    metadata = load_table_metadata(table_oid);
    catalog_cache.put(table_oid, metadata);
}
```

### 7.2. Query Plan Cache

**Caches compiled query plans:**

```cpp
struct QueryPlanCacheKey {
    std::string sql;
    uint64_t database_oid;

    bool operator==(const QueryPlanCacheKey& other) const {
        return sql == other.sql && database_oid == other.database_oid;
    }
};

struct QueryPlanCacheKeyHash {
    size_t operator()(const QueryPlanCacheKey& key) const {
        return std::hash<std::string>()(key.sql) ^ std::hash<uint64_t>()(key.database_oid);
    }
};

LRUCache<QueryPlanCacheKey, QueryPlan> plan_cache(500);
```

### 7.3. Domain Validation Cache

**Caches compiled domain validation rules:**

```cpp
LRUCache<DomainOID, CompiledDomainValidation> domain_cache(100);

bool validate_domain_value(DomainOID domain_oid, const TypedValue& value) {
    CompiledDomainValidation validation;

    if (!domain_cache.get(domain_oid, validation)) {
        // Compile and cache
        validation = compile_domain_validation(domain_oid);
        domain_cache.put(domain_oid, validation);
    }

    return validation.validate(value);
}
```

---

## 8. Memory Limits and Quotas

### 8.1. Global Memory Limits

```ini
# Maximum total memory usage
max_memory = 8GB

# Per-connection memory limit
max_connection_memory = 256MB

# Per-query memory limit
max_query_memory = 1GB

# Sort buffer size
sort_buffer_size = 64MB

# Aggregate buffer size
aggregate_buffer_size = 128MB
```

### 8.2. Memory Quota Enforcement

```cpp
class MemoryQuota {
public:
    explicit MemoryQuota(size_t limit) : limit_(limit), used_(0) {}

    bool try_allocate(size_t n) {
        size_t current = used_.load();
        while (current + n <= limit_) {
            if (used_.compare_exchange_weak(current, current + n)) {
                return true;  // Success
            }
        }
        return false;  // Quota exceeded
    }

    void deallocate(size_t n) {
        used_ -= n;
    }

    size_t used() const { return used_.load(); }
    size_t limit() const { return limit_; }

private:
    size_t limit_;
    std::atomic<size_t> used_;
};

// Global quota
MemoryQuota global_quota(8ULL * 1024 * 1024 * 1024);  // 8 GB

// Per-connection quota
void* allocate_with_quota(size_t n, MemoryQuota* quota) {
    if (!quota->try_allocate(n)) {
        throw OutOfMemoryException("Quota exceeded");
    }

    void* ptr = malloc(n);
    if (!ptr) {
        quota->deallocate(n);
        throw OutOfMemoryException("malloc failed");
    }

    return ptr;
}
```

---

## 9. Security Considerations

### 9.1. Memory Zeroing

**Zero sensitive data before freeing:**

```cpp
void* secure_alloc(size_t n) {
    return calloc(1, n);  // Zero-initialized
}

void secure_free(void* ptr, size_t n) {
    if (ptr) {
        explicit_bzero(ptr, n);  // Zero memory
        free(ptr);
    }
}
```

### 9.2. Memory Encryption

**Encrypt sensitive pages in buffer pool:**

```cpp
void encrypt_page(BufferFrame* frame, const uint8_t* key) {
    uint8_t iv[16];
    RAND_bytes(iv, sizeof(iv));

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, key, iv);

    int len;
    EVP_EncryptUpdate(ctx, frame->data, &len, frame->data, PAGE_SIZE);
    EVP_EncryptFinal_ex(ctx, frame->data + len, &len);

    EVP_CIPHER_CTX_free(ctx);
}
```

### 9.3. Stack Protection

**Enable stack canaries:**

```bash
# Compile with stack protection
gcc -fstack-protector-strong ...
```

### 9.4. ASLR (Address Space Layout Randomization)

**Enabled by default on modern Linux:**

```bash
# Check ASLR status
cat /proc/sys/kernel/randomize_va_space
# 2 = full randomization
```

---

## 10. Performance Optimization

### 10.1. NUMA Awareness

**Allocate memory on local NUMA node:**

```cpp
#include <numa.h>

void* numa_alloc_local(size_t size) {
    int node = numa_node_of_cpu(sched_getcpu());
    return numa_alloc_onnode(size, node);
}
```

### 10.2. Huge Pages

**Use 2 MB huge pages for buffer pool:**

```cpp
void* allocate_huge_pages(size_t size) {
    void* ptr = mmap(NULL, size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);

    if (ptr == MAP_FAILED) {
        // Fallback to regular pages
        return mmap(NULL, size, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    }

    return ptr;
}
```

**Configure huge pages:**

```bash
# Reserve 1024 huge pages (2 GB)
echo 1024 > /proc/sys/vm/nr_hugepages
```

### 10.3. Memory Alignment

**Align allocations to cache lines (64 bytes):**

```cpp
void* aligned_alloc_64(size_t size) {
    void* ptr;
    if (posix_memalign(&ptr, 64, size) != 0) {
        return nullptr;
    }
    return ptr;
}
```

---

## 11. Monitoring and Observability

### 11.1. Memory Metrics

```cpp
struct MemoryMetrics {
    // Global
    size_t total_allocated;
    size_t total_freed;
    size_t current_usage;
    size_t peak_usage;

    // Buffer pool
    size_t buffer_pool_size;
    size_t buffer_pool_used;
    uint64_t cache_hits;
    uint64_t cache_misses;
    double hit_rate;

    // Shared memory
    size_t shared_memory_size;
    size_t shared_memory_used;

    // Per-context
    std::map<std::string, size_t> context_usage;
};
```

### 11.2. OpenTelemetry Integration

```cpp
void record_memory_metrics(const MemoryMetrics& metrics) {
    auto meter = opentelemetry::metrics::Provider::GetMeterProvider()
                    ->GetMeter("scratchbird.memory");

    meter->CreateGauge("memory_usage_bytes")->Record(metrics.current_usage);
    meter->CreateGauge("buffer_pool_hit_rate")->Record(metrics.hit_rate);
    meter->CreateCounter("cache_hits_total")->Add(metrics.cache_hits);
}
```

---

## 12. Debugging and Leak Detection

### 12.1. AddressSanitizer (ASan)

**Enable in CMake:**

```cmake
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
```

**Run tests:**

```bash
ASAN_OPTIONS=detect_leaks=1 ./scratchbird_tests
```

### 12.2. Valgrind

```bash
valgrind --leak-check=full --show-leak-kinds=all ./scratchbird-server
```

### 12.3. Debug Allocator

```cpp
#ifdef DEBUG_MEMORY
void* debug_malloc(size_t n, const char* file, int line) {
    void* ptr = malloc(n);
    LOG_DEBUG("malloc({}) = {} at {}:{}", n, ptr, file, line);
    return ptr;
}

void debug_free(void* ptr, const char* file, int line) {
    LOG_DEBUG("free({}) at {}:{}", ptr, file, line);
    free(ptr);
}

#define malloc(n) debug_malloc(n, __FILE__, __LINE__)
#define free(ptr) debug_free(ptr, __FILE__, __LINE__)
#endif
```

---

## 13. Best Practices

### 13.1. Allocation Best Practices

1. **Use memory contexts** for automatic lifetime management
2. **Allocate in appropriate context** (global, database, transaction, temp)
3. **Reset temp contexts frequently** to avoid leaks
4. **Avoid global allocations** in hot paths
5. **Use memory pools** for fixed-size frequent allocations

### 13.2. Buffer Pool Best Practices

1. **Size buffer pool appropriately** (25-50% of RAM)
2. **Monitor hit rate** (target > 95%)
3. **Use prefetching** for sequential scans
4. **Pin pages sparingly** (avoid buffer pool starvation)
5. **Flush dirty pages asynchronously** (background writer)

### 13.3. Performance Best Practices

1. **Use huge pages** for large allocations (buffer pool)
2. **Enable NUMA awareness** on multi-socket systems
3. **Align to cache lines** (64 bytes) for frequently accessed structures
4. **Minimize allocations** in hot paths
5. **Use stack allocations** for small temporary buffers

### 13.4. Security Best Practices

1. **Zero sensitive memory** before freeing
2. **Encrypt pages** containing sensitive data
3. **Use secure allocators** (e.g., LibreSSL's OPENSSL_cleanse)
4. **Enable stack protection** (-fstack-protector-strong)
5. **Validate input sizes** to prevent overflows

---

## 14. Testing Requirements

### 14.1. Unit Tests

**Required Coverage:**

- Memory context allocation/deallocation
- Buffer pool page pinning/unpinning
- Cache hit/miss behavior
- Memory quota enforcement
- Out-of-memory handling

**Example Test:**

```cpp
TEST(MemoryContextTest, TransactionContextCleanup) {
    MemoryContext* global = create_global_context();
    MemoryContext* txn = global->create_child(ContextType::TRANSACTION, "test_txn");

    // Allocate in transaction context
    void* buf1 = txn->alloc(1024);
    void* buf2 = txn->alloc(2048);

    EXPECT_EQ(txn->current_usage(), 1024 + 2048);

    // Destroy context (should free all allocations)
    txn->destroy();

    EXPECT_EQ(global->current_usage(), 0);
}
```

### 14.2. Integration Tests

**Required Scenarios:**

1. **Concurrent buffer pool access** - Multiple threads pin/unpin pages
2. **Memory quota exhaustion** - Allocate until quota exceeded
3. **Shared memory IPC** - Multiple processes communicate
4. **Cache eviction** - Fill cache and verify LRU behavior

### 14.3. Leak Detection Tests

**Run all tests with ASan/Valgrind:**

```bash
# ASan
ASAN_OPTIONS=detect_leaks=1 ctest --test-dir build

# Valgrind
ctest --test-dir build -T memcheck
```

### 14.4. Performance Tests

**Benchmarks:**

- Buffer pool hit rate under various workloads
- Allocation/deallocation throughput
- Cache lookup latency
- Memory quota enforcement overhead

---

## Appendix A: Memory Configuration Reference

```ini
# Buffer Pool
buffer_pool_size = 4GB
buffer_replacement_policy = clock
dirty_page_flush_interval_ms = 5000
use_huge_pages = true

# Memory Limits
max_memory = 8GB
max_connection_memory = 256MB
max_query_memory = 1GB
sort_buffer_size = 64MB
aggregate_buffer_size = 128MB

# Shared Memory (Cluster Mode)
shared_memory_size = 1GB

# Allocator
use_jemalloc = false
enable_numa = true

# Monitoring
enable_memory_tracking = true
memory_metrics_interval_sec = 60
```

---

## Appendix B: Glossary

| Term | Definition |
|------|------------|
| **Buffer Pool** | Page cache that stores database pages in memory |
| **Memory Context** | Lifetime-based allocator for automatic cleanup |
| **LRU** | Least Recently Used (cache eviction policy) |
| **CLOCK** | Second-chance cache eviction policy |
| **Page Pinning** | Preventing a page from being evicted |
| **Dirty Page** | Modified page not yet flushed to disk |
| **NUMA** | Non-Uniform Memory Access |
| **Huge Pages** | Large memory pages (2 MB or 1 GB) |
| **Shared Memory** | Memory shared between processes |
| **ASLR** | Address Space Layout Randomization |

---

## Appendix C: References

- [Firebird MGA Architecture](../../MGA_RULES.md)
- [PostgreSQL Buffer Management](https://www.postgresql.org/docs/current/storage.html)
- [jemalloc Documentation](http://jemalloc.net/)
- [Linux Huge Pages](https://www.kernel.org/doc/Documentation/vm/hugetlbpage.txt)
- [NUMA Best Practices](https://www.kernel.org/doc/html/latest/vm/numa.html)

---

**Document Status:** ✅ Complete
**Last Review:** 2026-01-07
**Next Review:** 2026-04-07 (Quarterly)
**Owner:** ScratchBird Core Team
