# ScratchBird Storage Engine - Main Specification
## Master Document for Storage Engine Implementation

## Overview

The ScratchBird storage engine combines the best practices from multiple database systems to provide a robust, efficient, and scalable storage layer. This document serves as the main specification, tying together the buffer pool management and page management components.

## Architecture Overview

```
┌─────────────────────────────────────────────────┐
│              Query Execution Layer               │
└─────────────────────────────────────────────────┘
                        │
                        ▼
┌─────────────────────────────────────────────────┐
│           Storage Engine Interface               │
├─────────────────────────────────────────────────┤
│  • Relation Management                          │
│  • Tuple Operations                             │
│  • Index Operations                             │
│  • Transaction Integration                      │
└─────────────────────────────────────────────────┘
                        │
        ┌───────────────┼───────────────┐
        ▼               ▼               ▼
┌──────────────┐ ┌──────────────┐ ┌──────────────┐
│ Buffer Pool  │ │    Page      │ │   Space      │
│ Management   │ │ Management   │ │ Management   │
├──────────────┤ ├──────────────┤ ├──────────────┤
│ • Multi-pool │ │ • Page layout│ │ • FSM        │
│ • Ring buffer│ │ • Compression│ │ • VM         │
│ • Adaptive   │ │ • Encryption │ │ • TOAST      │
│   hash       │ │ • Checksums  │ │ • Extension  │
│ • Read-ahead │ │ • Multi-size │ │              │
│ • Direct I/O │ │   support    │ │              │
└──────────────┘ └──────────────┘ └──────────────┘
                        │
                        ▼
┌─────────────────────────────────────────────────┐
│              Physical Storage Layer              │
│         (Files, Segments, Tablespaces)          │
└─────────────────────────────────────────────────┘
```

## Component Specifications

### 1. Buffer Pool Management
**Specification**: `STORAGE_ENGINE_BUFFER_POOL.md`

Key features:
- Multiple buffer pools for different workloads
- Ring buffers for sequential access (PostgreSQL-style)
- Adaptive hash index for fast lookups (MySQL InnoDB-style)
- Clock sweep eviction algorithm
- Read-ahead with pattern detection
- Direct I/O and async I/O support

### 2. Page Management
**Specification**: `STORAGE_ENGINE_PAGE_MANAGEMENT.md`

Key features:
- Universal page header for all page types
- Efficient heap page layout with line pointers
- Free space map (FSM) and visibility map (VM)
- Multiple compression algorithms
- Transparent data encryption (TDE)
- TOAST for oversized attributes
- Multi-page-size support (8K-128K)

## 1. Storage Engine Interface

### 1.1 Main Storage Engine Structure

```c
// Main storage engine structure
typedef struct sb_storage_engine {
    // Engine identification
    UUID            se_uuid;               // Storage engine UUID
    char            se_name[64];           // Engine name
    uint32_t        se_version;            // Engine version
    
    // Buffer pool manager
    SBBufferPoolManager* se_buffer_manager;
    
    // Page size context
    PageSizeContext* se_page_sizes;
    
    // Compression context
    CompressionContext* se_compression;
    
    // Encryption context
    EncryptionContext* se_encryption;
    
    // Space management
    SpaceManager*   se_space_manager;
    
    // Relation cache
    RelationCache*  se_rel_cache;
    
    // Statistics
    StorageStats    se_stats;
    
    // Configuration
    StorageConfig   se_config;
    
    // Methods
    StorageInterface* se_interface;
} SBStorageEngine;

// Storage engine interface
typedef struct storage_interface {
    // Initialization
    Status (*init)(SBStorageEngine* engine, StorageConfig* config);
    Status (*shutdown)(SBStorageEngine* engine);
    
    // Relation operations
    RelationId (*create_relation)(SBStorageEngine* engine,
                                 CreateRelationParams* params);
    Status (*drop_relation)(SBStorageEngine* engine,
                          RelationId rel_id);
    Status (*truncate_relation)(SBStorageEngine* engine,
                              RelationId rel_id);
    
    // Tuple operations
    ItemPointer (*insert_tuple)(SBStorageEngine* engine,
                              RelationId rel_id,
                              HeapTuple tuple);
    Status (*delete_tuple)(SBStorageEngine* engine,
                         RelationId rel_id,
                         ItemPointer tid);
    Status (*update_tuple)(SBStorageEngine* engine,
                         RelationId rel_id,
                         ItemPointer old_tid,
                         HeapTuple new_tuple,
                         ItemPointer* new_tid);
    HeapTuple (*fetch_tuple)(SBStorageEngine* engine,
                           RelationId rel_id,
                           ItemPointer tid,
                           Snapshot snapshot);
    
    // Scan operations
    TableScanDesc (*begin_scan)(SBStorageEngine* engine,
                              RelationId rel_id,
                              Snapshot snapshot,
                              ScanKey* keys,
                              int nkeys);
    HeapTuple (*scan_next)(TableScanDesc scan);
    void (*end_scan)(TableScanDesc scan);
    
    // Sweep/GC operations (vacuum alias)
    Status (*vacuum)(SBStorageEngine* engine,
                   RelationId rel_id,
                   VacuumParams* params);
    Status (*analyze)(SBStorageEngine* engine,
                    RelationId rel_id,
                    AnalyzeParams* params);
    
    // Checkpoint operations
    Status (*checkpoint)(SBStorageEngine* engine,
                       CheckpointParams* params);
    Status (*sync_relation)(SBStorageEngine* engine,
                          RelationId rel_id);
} StorageInterface;
```

### 1.2 Relation Management

```c
// Relation descriptor
typedef struct relation_desc {
    // Identification
    UUID            rd_uuid;               // Relation UUID
    Oid             rd_id;                 // Relation OID (compatibility)
    char            rd_name[NAMEDATALEN];  // Relation name
    
    // Storage parameters
    uint32_t        rd_page_size;          // Page size for this relation
    ForkNumber      rd_nforks;             // Number of forks
    
    // Compression and encryption
    CompressionMethod rd_compression;      // Default compression
    EncryptionMethod rd_encryption;        // Encryption method
    
    // TOAST
    UUID            rd_toast_uuid;         // TOAST table UUID
    ToastStrategy   rd_toast_strategy;     // TOAST strategy
    
    // Statistics
    uint64_t        rd_n_tuples;           // Number of tuples
    uint64_t        rd_n_pages;            // Number of pages
    uint64_t        rd_n_dead_tuples;      // Dead tuples
    
    // Access method
    AccessMethod*   rd_am;                 // Access method
    
    // Tuple descriptor
    TupleDesc       rd_att;                // Tuple descriptor
    
    // Indexes
    List*           rd_indexes;            // List of indexes
    
    // Constraints
    List*           rd_constraints;        // List of constraints
    
    // Triggers
    List*           rd_triggers;           // List of triggers
} RelationDesc;

// Create relation
RelationId create_relation(
    SBStorageEngine* engine,
    CreateRelationParams* params)
{
    // Generate UUID for new relation
    UUID rel_uuid = generate_uuid_v7();
    
    // Choose optimal page size
    uint32_t page_size = choose_optimal_page_size(params);
    
    // Create relation descriptor
    RelationDesc* rel = allocate(sizeof(RelationDesc));
    rel->rd_uuid = rel_uuid;
    rel->rd_page_size = page_size;
    rel->rd_compression = params->compression;
    rel->rd_encryption = params->encryption;
    
    // Create physical files
    for (ForkNumber fork = 0; fork < params->nforks; fork++) {
        create_relation_fork(rel, fork);
    }
    
    // Create FSM and VM
    if (params->create_fsm) {
        create_fsm_fork(rel);
    }
    
    if (params->create_vm) {
        create_vm_fork(rel);
    }
    
    // Create TOAST table if needed
    if (needs_toast_table(params->attributes)) {
        rel->rd_toast_uuid = create_toast_table(engine, rel);
    }
    
    // Add to relation cache
    add_to_relation_cache(engine->se_rel_cache, rel);
    
    // Update catalog
    update_catalog_new_relation(rel);
    
    return rel->rd_id;
}
```

## 2. Tablespace Management

### 2.1 Tablespace Structure

```c
// Tablespace descriptor
typedef struct tablespace_desc {
    // Identification
    UUID            ts_uuid;               // Tablespace UUID
    Oid             ts_id;                 // Tablespace OID
    char            ts_name[NAMEDATALEN];  // Tablespace name
    
    // Location
    char            ts_location[MAXPGPATH]; // Directory path
    
    // Configuration
    uint32_t        ts_default_page_size;  // Default page size
    CompressionMethod ts_compression;      // Default compression
    EncryptionMethod ts_encryption;        // Default encryption
    
    // Storage characteristics
    StorageClass    ts_storage_class;      // Storage class
    uint32_t        ts_io_concurrency;     // I/O concurrency
    bool            ts_use_direct_io;      // Use direct I/O
    
    // Quotas
    uint64_t        ts_max_size;           // Maximum size
    uint64_t        ts_current_size;       // Current size
    
    // Statistics
    TablespaceStats ts_stats;
} TablespaceDesc;

// Storage classes for tiered storage
typedef enum storage_class {
    STORAGE_CLASS_NVME,     // NVMe SSD (highest performance)
    STORAGE_CLASS_SSD,      // Standard SSD
    STORAGE_CLASS_HDD,      // Hard disk
    STORAGE_CLASS_ARCHIVE,  // Archive storage
    STORAGE_CLASS_S3,       // Cloud object storage
    STORAGE_CLASS_MEMORY    // In-memory only
} StorageClass;

// Create tablespace
Status create_tablespace(
    SBStorageEngine* engine,
    CreateTablespaceParams* params)
{
    // Validate location
    if (!validate_tablespace_location(params->location)) {
        return STATUS_INVALID_LOCATION;
    }
    
    // Create tablespace descriptor
    TablespaceDesc* ts = allocate(sizeof(TablespaceDesc));
    ts->ts_uuid = generate_uuid_v7();
    strcpy(ts->ts_name, params->name);
    strcpy(ts->ts_location, params->location);
    ts->ts_storage_class = params->storage_class;
    
    // Create directory structure
    create_tablespace_directories(ts);
    
    // Initialize tablespace
    initialize_tablespace(ts);
    
    // Add to catalog
    add_tablespace_to_catalog(ts);
    
    return STATUS_OK;
}
```

## 3. Segment and File Management

### 3.1 Segment Management

```c
// Segment size (1GB by default)
#define SEGMENT_SIZE (1024 * 1024 * 1024)

// Segment descriptor
typedef struct segment_desc {
    // Identification
    RelationId      seg_rel_id;            // Relation ID
    ForkNumber      seg_fork_num;          // Fork number
    uint32_t        seg_number;            // Segment number
    
    // File information
    int             seg_fd;                // File descriptor
    char            seg_path[MAXPGPATH];   // File path
    
    // Size information
    uint64_t        seg_size;              // Current size
    BlockNumber     seg_n_blocks;          // Number of blocks
    
    // Flags
    bool            seg_dirty;             // Has dirty pages
    bool            seg_synced;            // Synced to disk
} SegmentDesc;

// Open segment
SegmentDesc* open_segment(
    RelationDesc* rel,
    ForkNumber fork,
    uint32_t segno,
    int flags)
{
    SegmentDesc* seg = allocate(sizeof(SegmentDesc));
    
    // Build segment path
    build_segment_path(seg->seg_path, rel, fork, segno);
    
    // Open file
    seg->seg_fd = open(seg->seg_path, flags, 0600);
    
    if (seg->seg_fd < 0) {
        if (errno == ENOENT && (flags & O_CREAT)) {
            // Create new segment
            seg->seg_fd = open(seg->seg_path, flags | O_CREAT, 0600);
            
            if (seg->seg_fd < 0) {
                free(seg);
                return NULL;
            }
            
            // Initialize segment
            initialize_segment(seg, rel->rd_page_size);
        } else {
            free(seg);
            return NULL;
        }
    }
    
    // Get segment size
    struct stat st;
    fstat(seg->seg_fd, &st);
    seg->seg_size = st.st_size;
    seg->seg_n_blocks = seg->seg_size / rel->rd_page_size;
    
    seg->seg_rel_id = rel->rd_id;
    seg->seg_fork_num = fork;
    seg->seg_number = segno;
    
    return seg;
}

// Extend segment
BlockNumber extend_segment(
    SegmentDesc* seg,
    RelationDesc* rel,
    uint32_t n_blocks)
{
    uint32_t page_size = rel->rd_page_size;
    BlockNumber first_new_block = seg->seg_n_blocks;
    
    // Allocate and zero new blocks
    char* zero_buffer = allocate_zero(page_size);
    
    for (uint32_t i = 0; i < n_blocks; i++) {
        off_t offset = (first_new_block + i) * page_size;
        
        if (pwrite(seg->seg_fd, zero_buffer, page_size, offset) != page_size) {
            free(zero_buffer);
            return InvalidBlockNumber;
        }
    }
    
    free(zero_buffer);
    
    // Update segment size
    seg->seg_n_blocks += n_blocks;
    seg->seg_size += n_blocks * page_size;
    seg->seg_dirty = true;
    
    return first_new_block;
}
```

## 4. Background Workers

### 4.1 Background Writer

```c
// Background writer context
typedef struct bgwriter_context {
    // Configuration
    uint32_t        bgw_delay_ms;          // Delay between rounds
    uint32_t        bgw_max_pages;         // Max pages per round
    double          bgw_dirty_threshold;   // Dirty page threshold
    
    // State
    bool            bgw_active;            // Writer active
    pthread_t       bgw_thread;            // Writer thread
    
    // Statistics
    uint64_t        bgw_pages_written;     // Total pages written
    uint64_t        bgw_rounds;            // Rounds completed
    uint64_t        bgw_skipped_rounds;    // Rounds skipped
} BGWriterContext;

// Background writer main loop
void* bgwriter_main(void* arg) {
    SBStorageEngine* engine = (SBStorageEngine*)arg;
    BGWriterContext* bgw = engine->se_bgwriter;
    
    while (bgw->bgw_active) {
        // Sleep between rounds
        usleep(bgw->bgw_delay_ms * 1000);
        
        // Check dirty page ratio
        BufferPoolStats stats;
        collect_buffer_pool_stats(engine->se_buffer_manager, &stats);
        
        double dirty_ratio = (double)stats.dirty_pages / 
                           engine->se_buffer_manager->bpm_total_pages;
        
        if (dirty_ratio < bgw->bgw_dirty_threshold) {
            bgw->bgw_skipped_rounds++;
            continue;
        }
        
        // Write dirty pages
        uint32_t pages_written = 0;
        
        for (BufferPool* pool : engine->se_buffer_manager->bpm_pools) {
            pages_written += flush_dirty_pages(pool, 
                                              bgw->bgw_max_pages - pages_written);
            
            if (pages_written >= bgw->bgw_max_pages) {
                break;
            }
        }
        
        // Update statistics
        bgw->bgw_pages_written += pages_written;
        bgw->bgw_rounds++;
    }
    
    return NULL;
}
```

### 4.2 Checkpointer

```c
// Checkpointer context
typedef struct checkpointer_context {
    // Configuration
    uint32_t        ckpt_interval_s;       // Checkpoint interval
    uint32_t        ckpt_completion_target; // Target completion %
    
    // State
    bool            ckpt_active;           // Checkpointer active
    pthread_t       ckpt_thread;           // Checkpointer thread
    CheckpointState ckpt_state;            // Current checkpoint state
    
    // Statistics
    uint64_t        ckpt_count;            // Checkpoints completed
    uint64_t        ckpt_pages_written;    // Pages written
    uint64_t        ckpt_time_total_ms;    // Total time spent
} CheckpointerContext;

// Perform checkpoint
Status perform_checkpoint(
    SBStorageEngine* engine,
    CheckpointParams* params)
{
    CheckpointerContext* ckpt = engine->se_checkpointer;
    
    // Step 1: Mark checkpoint start
    LSN checkpoint_lsn = get_current_lsn();
    ckpt->ckpt_state = CKPT_STATE_STARTING;
    
    // Step 2: Flush all dirty buffers
    uint64_t pages_to_write = count_dirty_pages(engine->se_buffer_manager);
    uint64_t pages_written = 0;
    
    for (BufferPool* pool : engine->se_buffer_manager->bpm_pools) {
        for (uint32_t i = 0; i < pool->bp_n_buffers; i++) {
            BufferDesc* buf = &pool->bp_descriptors[i];
            
            if (buf->buf_is_dirty && buf->buf_lsn <= checkpoint_lsn) {
                flush_buffer(pool, buf);
                pages_written++;
                
                // Spread checkpoint I/O
                if (params->spread_checkpoint) {
                    double progress = (double)pages_written / pages_to_write;
                    if (progress < ckpt->ckpt_completion_target / 100.0) {
                        usleep(calculate_checkpoint_delay(params));
                    }
                }
            }
        }
    }
    
    // Step 3: Write checkpoint record
    write_checkpoint_record(checkpoint_lsn);
    
    // Step 4: Update control file
    update_control_file_checkpoint(checkpoint_lsn);
    
    // Step 5: Clean up optional write-after log (post-gold)
    if (params->remove_old_wal) {
        remove_old_wal_files(checkpoint_lsn);
    }
    
    // Update statistics
    ckpt->ckpt_count++;
    ckpt->ckpt_pages_written += pages_written;
    ckpt->ckpt_state = CKPT_STATE_IDLE;
    
    return STATUS_OK;
}
```

## 5. Performance Optimizations

### 5.1 Prefetching and Readahead

```c
// Prefetch pages for sequential scan
void prefetch_sequential_pages(
    SBStorageEngine* engine,
    RelationDesc* rel,
    BlockNumber start_block,
    uint32_t n_blocks)
{
    BufferPool* pool = select_buffer_pool(engine, BP_SEQUENTIAL);
    ReadAheadContext* ra = pool->bp_readahead;
    
    // Update pattern detection
    ra->ra_pattern = AP_SEQUENTIAL;
    ra->ra_current_size = MIN(n_blocks, ra->ra_max_pages);
    
    // Issue async reads
    for (uint32_t i = 0; i < ra->ra_current_size; i++) {
        BufferTag tag;
        tag.btag_db_uuid = rel->rd_database_uuid;
        tag.btag_table_uuid = rel->rd_uuid;
        tag.btag_fork_num = MAIN_FORKNUM;
        tag.btag_block_num = start_block + i;
        
        // Check if already in buffer
        if (!buffer_in_pool(pool, &tag)) {
            issue_async_read(pool, &tag, ra);
        }
    }
}
```

### 5.2 Bulk Loading Optimization

```c
// Bulk loading context
typedef struct bulk_load_context {
    RelationDesc*   bl_relation;           // Target relation
    BufferPool*     bl_pool;               // Dedicated pool
    uint32_t        bl_n_buffers;          // Number of buffers
    BufferDesc**    bl_buffers;            // Buffer array
    uint32_t        bl_current_buffer;     // Current buffer index
    
    // Statistics
    uint64_t        bl_tuples_loaded;      // Tuples loaded
    uint64_t        bl_pages_written;      // Pages written
} BulkLoadContext;

// Bulk insert tuples
Status bulk_insert_tuples(
    SBStorageEngine* engine,
    RelationDesc* rel,
    HeapTuple* tuples,
    uint32_t n_tuples)
{
    // Create bulk load context
    BulkLoadContext* ctx = create_bulk_load_context(rel);
    
    // Get dedicated buffers
    ctx->bl_pool = select_buffer_pool(engine, BP_BULKWRITE);
    ctx->bl_n_buffers = MIN(n_tuples / TUPLES_PER_PAGE + 1, 1000);
    ctx->bl_buffers = allocate(sizeof(BufferDesc*) * ctx->bl_n_buffers);
    
    // Allocate buffers
    for (uint32_t i = 0; i < ctx->bl_n_buffers; i++) {
        ctx->bl_buffers[i] = get_free_buffer(ctx->bl_pool);
        
        // Initialize as heap page
        HeapPage* page = (HeapPage*)ctx->bl_buffers[i]->buf_data;
        page_init(page, rel->rd_page_size, sizeof(HeapPageSpecial));
    }
    
    // Insert tuples
    for (uint32_t i = 0; i < n_tuples; i++) {
        HeapPage* page = (HeapPage*)
            ctx->bl_buffers[ctx->bl_current_buffer]->buf_data;
        
        OffsetNumber off = heap_page_add_tuple(page, tuples[i],
                                              rel->rd_page_size, true);
        
        if (off == InvalidOffsetNumber) {
            // Current page full - move to next
            ctx->bl_current_buffer++;
            
            if (ctx->bl_current_buffer >= ctx->bl_n_buffers) {
                // Flush all buffers
                flush_bulk_buffers(ctx);
                ctx->bl_current_buffer = 0;
            }
            
            // Retry insert
            page = (HeapPage*)
                ctx->bl_buffers[ctx->bl_current_buffer]->buf_data;
            off = heap_page_add_tuple(page, tuples[i],
                                    rel->rd_page_size, true);
        }
        
        ctx->bl_tuples_loaded++;
    }
    
    // Final flush
    flush_bulk_buffers(ctx);
    
    // Update FSM and VM
    update_fsm_bulk(rel);
    update_vm_bulk(rel);
    
    // Clean up
    free_bulk_load_context(ctx);
    
    return STATUS_OK;
}
```

## 6. Integration Points

### 6.1 Transaction System Integration

```c
// Register dirty page with transaction
void register_dirty_page(
    SBStorageEngine* engine,
    TransactionId xid,
    BufferDesc* buf)
{
    // Mark buffer dirty
    buf->buf_is_dirty = true;
    buf->buf_dirty_xid = xid;
    
    // Update LSN
    buf->buf_lsn = get_current_lsn();
    
    // Add to transaction's dirty list
    add_to_transaction_dirty_list(xid, buf);
    
    // Update statistics
    engine->se_stats.dirty_pages++;
}

// Check tuple visibility
bool tuple_satisfies_snapshot(
    HeapTupleHeader tuple,
    Snapshot snapshot)
{
    TransactionId xmin = tuple->t_xmin;
    TransactionId xmax = tuple->t_xmax;
    
    // Check insert visibility
    if (!TransactionIdIsValid(xmin)) {
        return false;
    }
    
    if (TransactionIdIsCurrentTransactionId(xmin)) {
        // Our own insert
        return true;
    }
    
    if (!TransactionIdIsInSnapshot(xmin, snapshot)) {
        // Insert not visible
        return false;
    }
    
    // Check delete visibility
    if (!TransactionIdIsValid(xmax)) {
        // Not deleted
        return true;
    }
    
    if (TransactionIdIsCurrentTransactionId(xmax)) {
        // Our own delete - not visible
        return false;
    }
    
    if (TransactionIdIsInSnapshot(xmax, snapshot)) {
        // Delete not visible - tuple still visible
        return true;
    }
    
    // Deleted and visible
    return false;
}
```

### 6.2 Optional Write-After Log (Future)

**Scope Note:** ScratchBird MGA does not use write-after log (WAL) for recovery. The write-after log is optional, future work for replication/PITR only.

```c
// Write optional write-after log record for page modification (future)
void log_page_modification(
    BufferDesc* buf,
    WALRecordType type,
    void* data,
    uint32_t data_len)
{
    WALRecord record;
    record.type = type;
    record.xid = GetCurrentTransactionId();
    record.page_tag = buf->buf_tag;
    record.data = data;
    record.data_len = data_len;
    
    // Write to optional write-after log
    LSN lsn = write_wal_record(&record);
    
    // Update page LSN
    SBPageHeader* header = (SBPageHeader*)buf->buf_data;
    header->pd_lsn = lsn;
    buf->buf_lsn = lsn;
}
```

## Implementation Timeline

Alpha scope:

1. Heap storage and basic page management
2. Integration with MGA transactions and buffer pool coordination

Post-alpha scope:

1. Optional write-after log (post-gold, secondary to MGA)
2. Advanced features (compression, encryption, multi-page-size)

## Conclusion

The ScratchBird storage engine provides a comprehensive storage solution that combines:

- **Firebird's** efficient page management and MGA foundation
- **PostgreSQL's** ring buffers and visibility maps
- **MySQL InnoDB's** adaptive hash index and buffer pool management
- **SQL Server's** read-ahead and compression features

The modular design allows for incremental implementation while maintaining high performance and reliability at each stage. The system is designed to handle modern workloads with features like compression, encryption, and multi-page-size support from the ground up.
