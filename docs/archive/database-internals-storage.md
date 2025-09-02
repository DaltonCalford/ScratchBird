# Database Internals: Storage Management

## Table of Contents
1. [FirebirdSQL Storage Management](#firebirdsql-storage-management)
2. [PostgreSQL Storage Management](#postgresql-storage-management)
3. [MySQL/MariaDB Storage Management](#mysqlmariadb-storage-management)
4. [Microsoft SQL Server Storage Management](#microsoft-sql-server-storage-management)

---

# FirebirdSQL Storage Management

## Buffer Pool Algorithms

### Firebird Cache Manager
```c
// Buffer control block
typedef struct bcb {
    struct bcb*     bcb_next;           // Next in chain
    struct bcb*     bcb_prev;           // Previous in chain
    struct bcb*     bcb_lru_next;       // Next in LRU
    struct bcb*     bcb_lru_prev;       // Previous in LRU
    struct bcb*     bcb_dirty_next;     // Next in dirty list
    struct bcb*     bcb_dirty_prev;     // Previous in dirty list
    PageNumber      bcb_page_number;    // Page number
    Database*       bcb_database;       // Database
    struct bdb*     bcb_buffer;         // Buffer descriptor
    ULONG           bcb_flags;          // Flags
    SLONG           bcb_use_count;      // Reference count
    SLONG           bcb_scan_count;     // Scan count for clock algorithm
    ThreadId        bcb_exclusive;      // Exclusive lock holder
    Semaphore       bcb_semaphore;      // Page semaphore
    ULONG           bcb_hash_slot;      // Hash slot
    TraNumber       bcb_transaction;    // Last transaction to modify
    ULONG           bcb_generation;     // Page generation
} BCB;

// Buffer pool structure
typedef struct buffer_pool {
    BCB**           bpl_hash_table;     // Hash table
    ULONG           bpl_hash_slots;     // Number of hash slots
    BCB*            bpl_lru_head;       // LRU list head
    BCB*            bpl_lru_tail;       // LRU list tail
    BCB*            bpl_dirty_head;     // Dirty list head
    BCB*            bpl_dirty_tail;     // Dirty list tail
    ULONG           bpl_count;          // Number of buffers
    ULONG           bpl_free;           // Free buffers
    ULONG           bpl_min_free;       // Minimum free buffers
    ULONG           bpl_target_free;    // Target free buffers
    Mutex           bpl_mutex;          // Pool mutex
    Event           bpl_writer_event;   // Writer event
    Thread          bpl_writer_thread;  // Writer thread
    Statistics      bpl_stats;          // Statistics
} BufferPool;

// Fetch page into cache
BCB* CCH_fetch_page(
    Database*       dbb,
    PageNumber      page_number,
    LockType        lock_type,
    PageType        page_type)
{
    BufferPool* pool = dbb->dbb_buffer_pool;
    BCB* bcb;
    ULONG hash_slot;
    
    // Calculate hash slot
    hash_slot = page_number % pool->bpl_hash_slots;
    
    // Lock hash chain
    MutexLockGuard guard(pool->bpl_mutex);
    
    // Search in hash table
    for (bcb = pool->bpl_hash_table[hash_slot]; bcb; bcb = bcb->bcb_next) {
        if (bcb->bcb_page_number == page_number &&
            bcb->bcb_database == dbb) {
            // Found in cache
            break;
        }
    }
    
    if (bcb) {
        // Page in cache - update LRU
        if (bcb != pool->bpl_lru_head) {
            // Remove from current position
            if (bcb->bcb_lru_prev) {
                bcb->bcb_lru_prev->bcb_lru_next = bcb->bcb_lru_next;
            }
            if (bcb->bcb_lru_next) {
                bcb->bcb_lru_next->bcb_lru_prev = bcb->bcb_lru_prev;
            }
            if (bcb == pool->bpl_lru_tail) {
                pool->bpl_lru_tail = bcb->bcb_lru_prev;
            }
            
            // Move to head
            bcb->bcb_lru_prev = NULL;
            bcb->bcb_lru_next = pool->bpl_lru_head;
            pool->bpl_lru_head->bcb_lru_prev = bcb;
            pool->bpl_lru_head = bcb;
        }
        
        // Increment use count
        bcb->bcb_use_count++;
        
        // Wait for exclusive lock if needed
        if (lock_type == LCK_write && bcb->bcb_exclusive != Thread::getId()) {
            guard.release();
            bcb->bcb_semaphore.wait();
            guard.acquire();
        }
        
        // Update statistics
        pool->bpl_stats.cache_hits++;
        
    } else {
        // Page not in cache - need to read
        pool->bpl_stats.cache_misses++;
        
        // Get free buffer
        bcb = CCH_get_free_buffer(pool);
        
        if (!bcb) {
            // No free buffers - evict one
            bcb = CCH_evict_page(pool);
        }
        
        // Initialize BCB
        bcb->bcb_page_number = page_number;
        bcb->bcb_database = dbb;
        bcb->bcb_use_count = 1;
        bcb->bcb_flags = 0;
        
        // Add to hash table
        bcb->bcb_hash_slot = hash_slot;
        bcb->bcb_next = pool->bpl_hash_table[hash_slot];
        if (bcb->bcb_next) {
            bcb->bcb_next->bcb_prev = bcb;
        }
        pool->bpl_hash_table[hash_slot] = bcb;
        
        // Add to LRU head
        bcb->bcb_lru_prev = NULL;
        bcb->bcb_lru_next = pool->bpl_lru_head;
        if (pool->bpl_lru_head) {
            pool->bpl_lru_head->bcb_lru_prev = bcb;
        }
        pool->bpl_lru_head = bcb;
        if (!pool->bpl_lru_tail) {
            pool->bpl_lru_tail = bcb;
        }
        
        // Read page from disk
        guard.release();
        PIO_read_page(dbb, page_number, bcb->bcb_buffer->bdb_page);
        guard.acquire();
    }
    
    // Set lock type
    if (lock_type == LCK_write) {
        bcb->bcb_exclusive = Thread::getId();
        bcb->bcb_flags |= BCB_exclusive;
    }
    
    return bcb;
}

// Evict page using clock algorithm
BCB* CCH_evict_page(BufferPool* pool)
{
    BCB* bcb;
    BCB* victim = NULL;
    ULONG passes = 0;
    
    // Clock algorithm with second chance
    while (!victim && passes < 2) {
        for (bcb = pool->bpl_lru_tail; bcb; bcb = bcb->bcb_lru_prev) {
            // Skip if in use
            if (bcb->bcb_use_count > 0) {
                continue;
            }
            
            // Skip if dirty (prefer clean pages)
            if (passes == 0 && (bcb->bcb_flags & BCB_dirty)) {
                continue;
            }
            
            // Check scan count (second chance)
            if (bcb->bcb_scan_count > 0) {
                bcb->bcb_scan_count--;
                continue;
            }
            
            // Found victim
            victim = bcb;
            break;
        }
        passes++;
    }
    
    if (!victim) {
        // Emergency: pick LRU page regardless of state
        victim = pool->bpl_lru_tail;
    }
    
    // Write if dirty
    if (victim->bcb_flags & BCB_dirty) {
        CCH_write_page(victim);
    }
    
    // Remove from hash table
    if (victim->bcb_prev) {
        victim->bcb_prev->bcb_next = victim->bcb_next;
    } else {
        pool->bpl_hash_table[victim->bcb_hash_slot] = victim->bcb_next;
    }
    if (victim->bcb_next) {
        victim->bcb_next->bcb_prev = victim->bcb_prev;
    }
    
    // Remove from LRU
    if (victim->bcb_lru_prev) {
        victim->bcb_lru_prev->bcb_lru_next = victim->bcb_lru_next;
    } else {
        pool->bpl_lru_head = victim->bcb_lru_next;
    }
    if (victim->bcb_lru_next) {
        victim->bcb_lru_next->bcb_lru_prev = victim->bcb_lru_prev;
    } else {
        pool->bpl_lru_tail = victim->bcb_lru_prev;
    }
    
    // Clear BCB
    victim->bcb_page_number = 0;
    victim->bcb_database = NULL;
    victim->bcb_flags = 0;
    
    return victim;
}

// Background writer thread
void CCH_writer_thread(BufferPool* pool)
{
    while (!pool->bpl_shutdown) {
        // Wait for signal or timeout
        pool->bpl_writer_event.wait(WRITER_TIMEOUT);
        
        // Write dirty pages
        MutexLockGuard guard(pool->bpl_mutex);
        
        BCB* bcb = pool->bpl_dirty_head;
        ULONG written = 0;
        
        while (bcb && written < MAX_WRITE_BATCH) {
            BCB* next = bcb->bcb_dirty_next;
            
            if (bcb->bcb_use_count == 0) {
                guard.release();
                CCH_write_page(bcb);
                guard.acquire();
                
                // Remove from dirty list
                if (bcb->bcb_dirty_prev) {
                    bcb->bcb_dirty_prev->bcb_dirty_next = bcb->bcb_dirty_next;
                } else {
                    pool->bpl_dirty_head = bcb->bcb_dirty_next;
                }
                if (bcb->bcb_dirty_next) {
                    bcb->bcb_dirty_next->bcb_dirty_prev = bcb->bcb_dirty_prev;
                } else {
                    pool->bpl_dirty_tail = bcb->bcb_dirty_prev;
                }
                
                bcb->bcb_flags &= ~BCB_dirty;
                written++;
            }
            
            bcb = next;
        }
        
        // Update statistics
        pool->bpl_stats.pages_written += written;
    }
}
```

## Free Space Management

### Firebird Page Inventory Pages (PIP)
```c
// Page Inventory Page structure
typedef struct pip_page {
    PageHeader      pip_header;         // Page header
    ULONG           pip_min;            // Minimum page number
    ULONG           pip_max;            // Maximum page number
    ULONG           pip_used;           // Used pages
    UCHAR           pip_bits[1];        // Bit array for pages
} PIPPage;

// Data Page header with free space info
typedef struct data_page {
    PageHeader      dpg_header;         // Page header
    USHORT          dpg_count;          // Number of records
    USHORT          dpg_free_space;     // Free space on page
    USHORT          dpg_min_space;      // Minimum contiguous space
    struct dpg_repeat {
        USHORT      dpg_offset;         // Offset to record
        USHORT      dpg_length;         // Record length
    } dpg_rpt[1];                       // Record pointer table
} DataPage;

// Find page with enough free space
PageNumber DPM_find_space(
    Database*       dbb,
    ULONG           space_needed,
    PageType        page_type)
{
    PageNumber pip_page = FIRST_PIP_PAGE;
    
    while (pip_page) {
        // Fetch PIP page
        WIN pip_window;
        PIPPage* pip = (PIPPage*) CCH_fetch(dbb, &pip_window, pip_page);
        
        // Scan bits for free pages
        ULONG page_base = pip->pip_min;
        ULONG bits_per_page = 2;  // 2 bits per page: free/used/full
        
        for (ULONG i = 0; i < (pip->pip_max - pip->pip_min); i++) {
            ULONG byte_offset = (i * bits_per_page) / 8;
            ULONG bit_offset = (i * bits_per_page) % 8;
            UCHAR bits = pip->pip_bits[byte_offset];
            UCHAR page_bits = (bits >> bit_offset) & 0x03;
            
            if (page_bits == PIP_FREE || page_bits == PIP_USED) {
                // Page has free space - check it
                PageNumber page_number = page_base + i;
                
                WIN page_window;
                DataPage* page = (DataPage*) CCH_fetch(dbb, &page_window, 
                                                       page_number);
                
                if (page->dpg_free_space >= space_needed) {
                    // Found suitable page
                    CCH_release(dbb, &page_window);
                    CCH_release(dbb, &pip_window);
                    return page_number;
                }
                
                CCH_release(dbb, &page_window);
            }
        }
        
        // Move to next PIP page
        pip_page = pip->pip_header.pag_next;
        CCH_release(dbb, &pip_window);
    }
    
    // No suitable page found - allocate new
    return DPM_allocate_page(dbb, page_type);
}

// Update PIP when page space changes
void DPM_update_pip(
    Database*       dbb,
    PageNumber      page_number,
    PageState       new_state)
{
    // Calculate PIP page for this data page
    ULONG pip_sequence = page_number / PAGES_PER_PIP;
    PageNumber pip_page = FIRST_PIP_PAGE + pip_sequence;
    
    // Fetch PIP page
    WIN pip_window;
    PIPPage* pip = (PIPPage*) CCH_fetch(dbb, &pip_window, pip_page);
    
    // Calculate bit position
    ULONG page_offset = page_number - pip->pip_min;
    ULONG byte_offset = (page_offset * 2) / 8;
    ULONG bit_offset = (page_offset * 2) % 8;
    
    // Update bits
    UCHAR* byte_ptr = &pip->pip_bits[byte_offset];
    UCHAR old_bits = (*byte_ptr >> bit_offset) & 0x03;
    
    // Clear old bits
    *byte_ptr &= ~(0x03 << bit_offset);
    
    // Set new bits
    UCHAR new_bits;
    switch (new_state) {
        case PAGE_FREE:
            new_bits = PIP_FREE;
            pip->pip_used--;
            break;
        case PAGE_USED:
            new_bits = PIP_USED;
            if (old_bits == PIP_FREE) {
                pip->pip_used++;
            }
            break;
        case PAGE_FULL:
            new_bits = PIP_FULL;
            if (old_bits == PIP_FREE) {
                pip->pip_used++;
            }
            break;
    }
    
    *byte_ptr |= (new_bits << bit_offset);
    
    // Mark PIP page as modified
    CCH_mark(dbb, &pip_window);
    CCH_release(dbb, &pip_window);
}

// Garbage collection for free space
void DPM_garbage_collect_page(
    Database*       dbb,
    DataPage*       page)
{
    USHORT slot_count = page->dpg_count;
    USHORT new_count = 0;
    UCHAR* page_end = (UCHAR*)page + dbb->dbb_page_size;
    UCHAR* data_start = page_end;
    
    // Temporary record table
    struct {
        USHORT offset;
        USHORT length;
        UCHAR* data;
    } records[MAX_RECORDS_PER_PAGE];
    
    // Collect valid records
    for (USHORT i = 0; i < slot_count; i++) {
        if (page->dpg_rpt[i].dpg_length > 0) {
            records[new_count].offset = page->dpg_rpt[i].dpg_offset;
            records[new_count].length = page->dpg_rpt[i].dpg_length;
            records[new_count].data = (UCHAR*)page + page->dpg_rpt[i].dpg_offset;
            new_count++;
        }
    }
    
    // Compact records at end of page
    for (USHORT i = 0; i < new_count; i++) {
        data_start -= records[i].length;
        memmove(data_start, records[i].data, records[i].length);
        page->dpg_rpt[i].dpg_offset = data_start - (UCHAR*)page;
        page->dpg_rpt[i].dpg_length = records[i].length;
    }
    
    // Update page header
    page->dpg_count = new_count;
    page->dpg_free_space = data_start - 
                          ((UCHAR*)&page->dpg_rpt[new_count]);
    page->dpg_min_space = page->dpg_free_space;
    
    // Clear unused slots
    for (USHORT i = new_count; i < slot_count; i++) {
        page->dpg_rpt[i].dpg_offset = 0;
        page->dpg_rpt[i].dpg_length = 0;
    }
}
```

## BLOB/CLOB Handling

### Firebird BLOB Management
```c
// BLOB page structure
typedef struct blob_page {
    PageHeader      blp_header;         // Page header
    SLONG           blp_lead_page;      // First page of BLOB
    SLONG           blp_sequence;       // Sequence within BLOB
    USHORT          blp_length;         // Data length on page
    USHORT          blp_pad;            // Padding
    UCHAR           blp_data[1];        // BLOB data
} BlobPage;

// BLOB control structure
typedef struct blob_control {
    ISC_QUAD        blb_blob_id;        // BLOB ID
    Database*       blb_database;       // Database
    Transaction*    blb_transaction;    // Transaction
    Relation*       blb_relation;       // Relation
    USHORT          blb_level;          // BLOB level (0, 1, or 2)
    ULONG           blb_max_segment;    // Maximum segment size
    ULONG           blb_count;          // Total segments
    ULONG           blb_length;         // Total length
    ULONG           blb_lead_page;      // First page
    ULONG           blb_max_sequence;   // Maximum sequence
    ULONG           blb_current_page;   // Current page
    ULONG           blb_position;       // Current position
    BlobPage*       blb_page;           // Current page buffer
    WIN             blb_window;         // Current window
} BlobControl;

// Create new BLOB
ISC_QUAD BLB_create(
    Database*       dbb,
    Transaction*    transaction,
    Relation*       relation)
{
    BlobControl* blob = FB_NEW BlobControl();
    
    // Generate BLOB ID
    blob->blb_blob_id.gds_quad_high = transaction->tra_number;
    blob->blb_blob_id.gds_quad_low = DPM_gen_id(dbb, BLOB_ID_GENERATOR, 1);
    
    blob->blb_database = dbb;
    blob->blb_transaction = transaction;
    blob->blb_relation = relation;
    blob->blb_level = 0;
    blob->blb_max_segment = relation->rel_max_segment;
    
    // Allocate first page
    blob->blb_lead_page = DPM_allocate_page(dbb, pag_blob);
    blob->blb_current_page = blob->blb_lead_page;
    
    // Initialize first page
    blob->blb_page = (BlobPage*) CCH_fetch(dbb, &blob->blb_window, 
                                          blob->blb_lead_page);
    blob->blb_page->blp_header.pag_type = pag_blob;
    blob->blb_page->blp_lead_page = blob->blb_lead_page;
    blob->blb_page->blp_sequence = 0;
    blob->blb_page->blp_length = 0;
    
    CCH_mark(dbb, &blob->blb_window);
    
    return blob->blb_blob_id;
}

// Write BLOB segment
void BLB_put_segment(
    BlobControl*    blob,
    UCHAR*          buffer,
    USHORT          length)
{
    while (length > 0) {
        // Calculate space on current page
        USHORT page_space = blob->blb_database->dbb_page_size - 
                          sizeof(BlobPage) - blob->blb_page->blp_length;
        
        if (page_space == 0) {
            // Current page full - allocate new
            ULONG new_page = DPM_allocate_page(blob->blb_database, pag_blob);
            
            // Link pages
            blob->blb_page->blp_header.pag_next = new_page;
            CCH_release(blob->blb_database, &blob->blb_window);
            
            // Move to new page
            blob->blb_current_page = new_page;
            blob->blb_page = (BlobPage*) CCH_fetch(blob->blb_database, 
                                                  &blob->blb_window, new_page);
            
            blob->blb_page->blp_header.pag_type = pag_blob;
            blob->blb_page->blp_lead_page = blob->blb_lead_page;
            blob->blb_page->blp_sequence = ++blob->blb_max_sequence;
            blob->blb_page->blp_length = 0;
            
            page_space = blob->blb_database->dbb_page_size - sizeof(BlobPage);
        }
        
        // Copy data to page
        USHORT copy_length = MIN(length, page_space);
        memcpy(blob->blb_page->blp_data + blob->blb_page->blp_length,
               buffer, copy_length);
        
        blob->blb_page->blp_length += copy_length;
        blob->blb_length += copy_length;
        
        buffer += copy_length;
        length -= copy_length;
        
        CCH_mark(blob->blb_database, &blob->blb_window);
    }
}

// Read BLOB segment
USHORT BLB_get_segment(
    BlobControl*    blob,
    UCHAR*          buffer,
    USHORT          buffer_length)
{
    USHORT bytes_read = 0;
    
    while (buffer_length > 0 && blob->blb_position < blob->blb_length) {
        // Check if we need to fetch a page
        if (!blob->blb_page || 
            blob->blb_position >= blob->blb_page->blp_length) {
            
            // Move to next page
            if (blob->blb_page) {
                ULONG next_page = blob->blb_page->blp_header.pag_next;
                CCH_release(blob->blb_database, &blob->blb_window);
                
                if (!next_page) {
                    break;  // End of BLOB
                }
                
                blob->blb_current_page = next_page;
            }
            
            blob->blb_page = (BlobPage*) CCH_fetch(blob->blb_database,
                                                  &blob->blb_window,
                                                  blob->blb_current_page);
            blob->blb_position = 0;
        }
        
        // Copy data from page
        USHORT page_offset = blob->blb_position % 
                            (blob->blb_database->dbb_page_size - sizeof(BlobPage));
        USHORT available = blob->blb_page->blp_length - page_offset;
        USHORT copy_length = MIN(buffer_length, available);
        
        memcpy(buffer, blob->blb_page->blp_data + page_offset, copy_length);
        
        buffer += copy_length;
        buffer_length -= copy_length;
        bytes_read += copy_length;
        blob->blb_position += copy_length;
    }
    
    return bytes_read;
}

// BLOB garbage collection
void BLB_garbage_collect(
    Database*       dbb,
    ISC_QUAD        blob_id)
{
    // Start from lead page
    ULONG page_number = blob_id.gds_quad_low & 0x3FFFFFFF;
    
    while (page_number) {
        WIN window;
        BlobPage* page = (BlobPage*) CCH_fetch(dbb, &window, page_number);
        ULONG next_page = page->blp_header.pag_next;
        
        // Mark page as free
        DPM_release_page(dbb, page_number);
        
        CCH_release(dbb, &window);
        page_number = next_page;
    }
}
```

---

# PostgreSQL Storage Management

## Buffer Pool Algorithms

### PostgreSQL Buffer Manager
```c
// Buffer descriptor
typedef struct BufferDesc {
    BufferTag   tag;            // Buffer tag (identifies page)
    int         buf_id;         // Buffer ID (0 based)
    
    /* state is a combined variable holding several flags */
    pg_atomic_uint32 state;
    
    int         wait_backend_pid;   // PID of backend waiting on lock
    int         freeNext;           // Link in freelist
    LWLock      content_lock;       // Lock to access page content
    
    /* Below fields are protected by buffer header lock */
    int         usage_count;        // Clock sweep usage count
    int         refcount;           // Current reference count
    TransactionId recent_xmin;     // Recent XID horizon
} BufferDesc;

// Buffer tag identifying a page
typedef struct buftag {
    RelFileNode rnode;          // Physical relation identifier
    ForkNumber  forkNum;        // Fork number
    BlockNumber blockNum;       // Block number
} BufferTag;

// Buffer pool shared state
typedef struct BufferStrategyControl {
    SpinLock    buffer_strategy_lock;
    
    int         nextVictimBuffer;   // Next buffer to consider evicting
    int         firstFreeBuffer;    // First free buffer
    int         lastFreeBuffer;     // Last free buffer
    
    uint32      completePasses;     // Complete passes over buffer pool
    pg_atomic_uint32 numBufferAllocs; // Total buffer allocations
    
    int         bgwprocno;          // Background writer process number
} BufferStrategyControl;

// Get buffer for page
Buffer
ReadBuffer_common(SMgrRelation smgr, char relpersistence, ForkNumber forkNum,
                 BlockNumber blockNum, ReadBufferMode mode,
                 BufferAccessStrategy strategy, bool *hit)
{
    BufferDesc *bufHdr;
    Block       bufBlock;
    bool        found;
    bool        isExtend;
    bool        isLocalBuf = SmgrIsTemp(smgr);
    
    *hit = false;
    
    /* Make sure we will have room to remember the buffer pin */
    ResourceOwnerEnlargeBuffers(CurrentResourceOwner);
    
    isExtend = (blockNum == P_NEW);
    
    INIT_BUFFERTAG(newTag, smgr->smgr_rnode.node, forkNum, blockNum);
    
    /* Determine hash key and partition lock for it */
    newHash = BufTableHashCode(&newTag);
    newPartitionLock = BufMappingPartitionLock(newHash);
    
    /* Check if block is already in buffer pool */
    LWLockAcquire(newPartitionLock, LW_SHARED);
    buf_id = BufTableLookup(&newTag, newHash);
    
    if (buf_id >= 0) {
        /* Found in buffer pool */
        bufHdr = GetBufferDescriptor(buf_id);
        
        valid = PinBuffer(bufHdr, strategy);
        
        LWLockRelease(newPartitionLock);
        
        if (!valid) {
            /* Buffer was not valid - need to read */
            if (mode == RBM_ZERO_AND_LOCK || mode == RBM_ZERO_AND_CLEANUP_LOCK) {
                /* Just zero page */
                MemSet((char *) bufBlock, 0, BLCKSZ);
            } else {
                /* Read page from disk */
                smgrread(smgr, forkNum, blockNum, (char *) bufBlock);
            }
        }
        
        *hit = true;
        return BufferDescriptorGetBuffer(bufHdr);
    }
    
    /* Not in buffer pool - need to load */
    LWLockRelease(newPartitionLock);
    
    /* Get victim buffer using clock sweep */
    bufHdr = StrategyGetBuffer(strategy, &buf_state);
    
    /* If buffer is dirty, write it out */
    if (buf_state & BM_DIRTY) {
        FlushBuffer(bufHdr, NULL);
    }
    
    /* Now insert the new buffer into the hash table */
    LWLockAcquire(newPartitionLock, LW_EXCLUSIVE);
    
    buf_id = BufTableInsert(&newTag, newHash, bufHdr->buf_id);
    
    if (buf_id >= 0) {
        /* Someone else loaded it while we waited */
        UnpinBuffer(bufHdr, true);
        
        bufHdr = GetBufferDescriptor(buf_id);
        PinBuffer(bufHdr, strategy);
        
        LWLockRelease(newPartitionLock);
        
        *hit = true;
        return BufferDescriptorGetBuffer(bufHdr);
    }
    
    /* Initialize buffer tag */
    bufHdr->tag = newTag;
    
    LWLockRelease(newPartitionLock);
    
    /* Read page from disk */
    smgrread(smgr, forkNum, blockNum, (char *) BufferGetBlock(buffer));
    
    return BufferDescriptorGetBuffer(bufHdr);
}

// Clock sweep algorithm for victim selection
BufferDesc *
StrategyGetBuffer(BufferAccessStrategy strategy, uint32 *buf_state)
{
    BufferDesc *buf;
    int         bgwprocno;
    int         numCandidates;
    int         trycounter;
    uint32      local_buf_state;
    
    SpinLockAcquire(&StrategyControl->buffer_strategy_lock);
    
    bgwprocno = StrategyControl->bgwprocno;
    
    /* Loop until we find a victim buffer */
    for (;;) {
        /* Select next candidate buffer */
        buf = GetBufferDescriptor(StrategyControl->nextVictimBuffer);
        
        if (++StrategyControl->nextVictimBuffer >= NBuffers) {
            StrategyControl->nextVictimBuffer = 0;
            StrategyControl->completePasses++;
        }
        
        /* Check refcount and usage count */
        local_buf_state = LockBufHdr(buf);
        
        if (BUF_STATE_GET_REFCOUNT(local_buf_state) == 0) {
            if (BUF_STATE_GET_USAGECOUNT(local_buf_state) != 0) {
                /* Decrement usage count */
                local_buf_state -= BUF_USAGECOUNT_ONE;
                
                trycounter = NBuffers;
            } else {
                /* Found victim */
                buf->flags |= BM_TAG_VALID;
                *buf_state = local_buf_state;
                
                SpinLockRelease(&StrategyControl->buffer_strategy_lock);
                
                return buf;
            }
        }
        
        UnlockBufHdr(buf, local_buf_state);
    }
}

// Background writer for dirty buffers
void
BackgroundWriterMain(void)
{
    sigjmp_buf  local_sigjmp_buf;
    MemoryContext bgwriter_context;
    bool        prev_hibernate;
    WritebackContext wb_context;
    
    /* Initialize writeback context */
    WritebackContextInit(&wb_context, &bgwriter_flush_after);
    
    /* Main loop */
    for (;;) {
        bool        can_hibernate;
        int         rc;
        
        /* Do one cycle of dirty-buffer writing */
        can_hibernate = BgBufferSync(&wb_context);
        
        /* Check for signals, etc */
        HandleMainLoopInterrupts();
        
        /* Sleep until next cycle */
        if (can_hibernate) {
            /* Scale back activity */
            pg_usleep(bgwriter_delay * 1000L);
        } else {
            /* Stay busy */
            pg_usleep(bgwriter_delay * 1000L / 10);
        }
    }
}

// Scan and write dirty buffers
bool
BgBufferSync(WritebackContext *wb_context)
{
    int         strategy_buf_id;
    uint32      num_to_scan;
    uint32      num_written;
    int         reusable_buffers;
    
    /* Determine how many buffers to scan */
    num_to_scan = bgwriter_lru_maxpages;
    
    /* Initialize counters */
    num_written = 0;
    reusable_buffers = 0;
    
    /* Start scanning from current clock position */
    strategy_buf_id = StrategySyncStart(NULL, NULL);
    
    /* Scan buffers */
    while (num_to_scan > 0 && reusable_buffers < bgwriter_lru_maxpages) {
        BufferDesc *bufHdr;
        uint32      buf_state;
        
        bufHdr = GetBufferDescriptor(strategy_buf_id);
        
        /* Check if buffer needs writing */
        buf_state = LockBufHdr(bufHdr);
        
        if (BUF_STATE_GET_REFCOUNT(buf_state) == 0 &&
            BUF_STATE_GET_USAGECOUNT(buf_state) == 0) {
            
            reusable_buffers++;
            
            if (buf_state & BM_DIRTY) {
                /* Write dirty buffer */
                PinBuffer_Locked(bufHdr);
                
                UnlockBufHdr(bufHdr, buf_state);
                
                FlushBuffer(bufHdr, NULL);
                ScheduleBufferTagForWriteback(wb_context, &bufHdr->tag);
                
                UnpinBuffer(bufHdr, true);
                
                num_written++;
            } else {
                UnlockBufHdr(bufHdr, buf_state);
            }
        } else {
            UnlockBufHdr(bufHdr, buf_state);
        }
        
        /* Move to next buffer */
        if (++strategy_buf_id >= NBuffers) {
            strategy_buf_id = 0;
        }
        
        num_to_scan--;
    }
    
    /* Issue writeback requests */
    IssuePendingWritebacks(wb_context);
    
    /* Return true if we found enough reusable buffers */
    return (reusable_buffers >= bgwriter_lru_maxpages / 2);
}
```

## Free Space Management

### PostgreSQL Free Space Map (FSM)
```c
// FSM page structure
typedef struct FSMPage {
    uint8       fp_nodes[BLCKSZ - MAXALIGN(SizeOfPageHeaderData)];
} FSMPage;

// FSM tree structure (max-heap)
#define NodesPerPage (BLCKSZ - MAXALIGN(SizeOfPageHeaderData))
#define NonLeafNodesPerPage (BLCKSZ / 2 - 1)
#define LeafNodesPerPage (NodesPerPage - NonLeafNodesPerPage)

// Get amount of free space on page
Size
GetRecordedFreeSpace(Relation rel, BlockNumber heapBlk)
{
    FSMAddress  addr;
    uint16      slot;
    Buffer      buf;
    uint8       cat;
    
    /* Get FSM address for heap block */
    addr = fsm_get_location(heapBlk, &slot);
    
    buf = fsm_readbuf(rel, addr, false);
    if (!BufferIsValid(buf))
        return 0;
    
    cat = fsm_get_avail(BufferGetPage(buf), slot);
    ReleaseBuffer(buf);
    
    return fsm_space_cat_to_avail(cat);
}

// Update free space info
void
RecordPageWithFreeSpace(Relation rel, BlockNumber heapBlk, Size spaceAvail)
{
    FSMAddress  addr;
    uint16      slot;
    Buffer      buf;
    Page        page;
    uint8       newCat;
    
    /* Get FSM address for heap block */
    addr = fsm_get_location(heapBlk, &slot);
    
    buf = fsm_readbuf(rel, addr, true);
    page = BufferGetPage(buf);
    
    newCat = fsm_space_avail_to_cat(spaceAvail);
    
    if (fsm_set_avail(page, slot, newCat)) {
        MarkBufferDirtyHint(buf, false);
    }
    
    ReleaseBuffer(buf);
    
    /* Update upper levels of FSM tree */
    fsm_update_recursive(rel, addr, newCat);
}

// Search for page with enough free space
BlockNumber
GetPageWithFreeSpace(Relation rel, Size spaceNeeded)
{
    uint8       min_cat = fsm_space_needed_to_cat(spaceNeeded);
    
    return fsm_search(rel, min_cat);
}

// Search FSM tree for suitable page
static BlockNumber
fsm_search(Relation rel, uint8 min_cat)
{
    FSMAddress  addr = FSM_ROOT_ADDRESS;
    int         slot;
    Buffer      buf;
    uint8       max_avail = 0;
    
    /* Start from root */
    for (;;) {
        int         child;
        
        buf = fsm_readbuf(rel, addr, false);
        if (!BufferIsValid(buf))
            return InvalidBlockNumber;
        
        /* Search for slot with enough space */
        slot = fsm_search_avail(BufferGetPage(buf), min_cat,
                               (addr.level == 0), false);
        
        if (slot == -1) {
            ReleaseBuffer(buf);
            return InvalidBlockNumber;
        }
        
        /* If leaf level, return the heap block number */
        if (addr.level == 0) {
            BlockNumber blkno = fsm_get_heap_blk(addr, slot);
            ReleaseBuffer(buf);
            return blkno;
        }
        
        /* Descend to child */
        child = fsm_get_child(slot);
        addr = fsm_get_child_address(addr, child);
        
        ReleaseBuffer(buf);
    }
}
```

## TOAST/LOB Handling

### PostgreSQL TOAST (The Oversized-Attribute Storage Technique)
```c
// TOAST pointer structure
typedef struct toast_pointer {
    struct varlena  header;         // Varlena header
    Oid             va_toastrelid;  // OID of TOAST table
    Oid             va_valueid;     // OID of value in TOAST table
    uint32          va_extsize;     // External size
    uint32          va_rawsize;     // Original data size
} toast_pointer;

// TOAST strategies
typedef enum ToastCompressionId {
    TOAST_PGLZ_COMPRESSION_ID = 0,
    TOAST_LZ4_COMPRESSION_ID = 1,
    TOAST_INVALID_COMPRESSION_ID = 2
} ToastCompressionId;

// TOAST a tuple
HeapTuple
toast_insert_or_update(Relation rel, HeapTuple newtup, HeapTuple oldtup,
                      int options)
{
    HeapTuple   result_tuple;
    TupleDesc   tupleDesc;
    int         numAttrs;
    
    Size        maxDataLen;
    Size        hoff;
    
    bool        toast_isnull[MaxHeapAttributeNumber];
    bool        toast_oldisnull[MaxHeapAttributeNumber];
    Datum       toast_values[MaxHeapAttributeNumber];
    Datum       toast_oldvalues[MaxHeapAttributeNumber];
    ToastAttrInfo toast_attr[MaxHeapAttributeNumber];
    
    /* Calculate maximum data length */
    maxDataLen = TOAST_TUPLE_TARGET - hoff;
    
    /* Analyze attributes */
    for (i = 0; i < numAttrs; i++) {
        Form_pg_attribute att = TupleDescAttr(tupleDesc, i);
        
        if (att->attlen == -1) {
            /* Varlena attribute - may need toasting */
            toast_attr[i].tai_size = VARSIZE_ANY(DatumGetPointer(toast_values[i]));
            toast_attr[i].tai_attno = i;
            
            if (toast_attr[i].tai_size > TOAST_TUPLE_THRESHOLD) {
                /* Definitely needs toasting */
                need_toast = true;
            }
        }
    }
    
    /* Compress and/or externalize attributes */
    while (heap_compute_data_size(tupleDesc, toast_values, toast_isnull) > maxDataLen) {
        int         biggest_attno = -1;
        Size        biggest_size = 0;
        
        /* Find biggest attribute to toast */
        for (i = 0; i < numAttrs; i++) {
            if (toast_attr[i].tai_size > biggest_size) {
                biggest_attno = i;
                biggest_size = toast_attr[i].tai_size;
            }
        }
        
        if (biggest_attno < 0)
            break;
        
        /* Toast the selected attribute */
        toast_values[biggest_attno] = 
            toast_save_datum(rel, toast_values[biggest_attno], 
                           toast_oldvalues[biggest_attno], options);
        
        /* Update size */
        toast_attr[biggest_attno].tai_size = 
            VARSIZE(DatumGetPointer(toast_values[biggest_attno]));
    }
    
    /* Build new tuple */
    result_tuple = heap_form_tuple(tupleDesc, toast_values, toast_isnull);
    
    return result_tuple;
}

// Save datum to TOAST table
static Datum
toast_save_datum(Relation rel, Datum value, Datum oldvalue, int options)
{
    Relation    toastrel;
    Relation    *toastidxs;
    HeapTuple   toasttup;
    
    Pointer     data_p;
    int32       data_len;
    int32       chunk_size;
    int         num_chunks;
    int         chunk_no;
    
    /* Get TOAST relation */
    toastrel = table_open(rel->rd_rel->reltoastrelid, RowExclusiveLock);
    toastidxs = RelationGetIndexList(toastrel);
    
    /* Compress if beneficial */
    if (VARATT_CAN_MAKE_SHORT(DatumGetPointer(value))) {
        /* Try compression */
        data_p = toast_compress_datum(value, &data_len);
        
        if (data_len < VARSIZE(DatumGetPointer(value)) * 0.75) {
            /* Compression beneficial */
            value = PointerGetDatum(data_p);
        }
    }
    
    /* Calculate chunks */
    data_len = VARSIZE(DatumGetPointer(value)) - VARHDRSZ;
    chunk_size = TOAST_MAX_CHUNK_SIZE;
    num_chunks = (data_len + chunk_size - 1) / chunk_size;
    
    /* Insert chunks */
    for (chunk_no = 0; chunk_no < num_chunks; chunk_no++) {
        int         chunk_len;
        
        /* Calculate chunk length */
        if (chunk_no < num_chunks - 1) {
            chunk_len = chunk_size;
        } else {
            chunk_len = data_len - (chunk_no * chunk_size);
        }
        
        /* Build chunk tuple */
        Datum       values[3];
        bool        nulls[3];
        
        values[0] = ObjectIdGetDatum(toast_pointer.va_valueid);
        values[1] = Int32GetDatum(chunk_no);
        values[2] = PointerGetDatum(
            data_p + chunk_no * chunk_size, chunk_len);
        
        memset(nulls, false, sizeof(nulls));
        
        toasttup = heap_form_tuple(toastrel->rd_att, values, nulls);
        
        /* Insert chunk */
        heap_insert(toastrel, toasttup, GetCurrentCommandId(true), 0, NULL);
        
        /* Update index */
        for (int i = 0; i < toastrel->rd_rel->relnatts; i++) {
            index_insert(toastidxs[i], values, nulls,
                        &(toasttup->t_self), toastrel,
                        UNIQUE_CHECK_NO, false, NULL);
        }
    }
    
    /* Build TOAST pointer */
    toast_pointer.va_toastrelid = rel->rd_rel->reltoastrelid;
    toast_pointer.va_valueid = GetNewOidWithIndex(toastrel, 
                                                 RelationGetRelid(toastidxs[0]),
                                                 (AttrNumber) 1);
    toast_pointer.va_extsize = data_len;
    toast_pointer.va_rawsize = VARSIZE(DatumGetPointer(value));
    
    table_close(toastrel, RowExclusiveLock);
    
    return PointerGetDatum(&toast_pointer);
}
```

---

# MySQL/MariaDB Storage Management

## Buffer Pool Algorithms

### InnoDB Buffer Pool
```c
// Buffer pool structure
typedef struct buf_pool_t {
    ulint           size;               // Size in pages
    ulint           curr_size;          // Current size in pages
    hash_table_t*   page_hash;          // Hash table of pages
    UT_LIST_BASE_NODE_T(buf_page_t) free; // Free list
    UT_LIST_BASE_NODE_T(buf_page_t) LRU;  // LRU list
    UT_LIST_BASE_NODE_T(buf_page_t) flush_list; // Flush list
    
    buf_page_t*     watch;              // Sentinel for watch
    ulint           n_pend_reads;       // Pending reads
    ulint           n_flush[BUF_FLUSH_N_TYPES]; // Pending flushes
    
    /* LRU list statistics */
    ulint           LRU_old_ratio;      // Ratio of old blocks
    ulint           LRU_old_len;        // Length of old blocks
    buf_page_t*     LRU_old;            // Pointer to old list
    
    /* Buddy allocator for compressed pages */
    UT_LIST_BASE_NODE_T(buf_buddy_free_t) zip_free[BUF_BUDDY_SIZES_MAX];
} buf_pool_t;

// Get buffer block
buf_block_t*
buf_page_get_gen(
    const page_id_t page_id,
    const page_size_t& page_size,
    ulint           rw_latch,
    buf_block_t*    guess,
    Page_fetch      fetch_mode,
    mtr_t*          mtr)
{
    buf_block_t*    block;
    buf_page_t*     bpage;
    buf_pool_t*     buf_pool = buf_pool_get(page_id);
    
    /* Check guess */
    if (guess && buf_block_is_accessed(guess, page_id)) {
        block = guess;
        goto got_block;
    }
    
    /* Look up in page hash */
lookup:
    bpage = buf_page_hash_get_low(buf_pool, page_id);
    
    if (bpage == NULL) {
        /* Page not in buffer pool - read it */
        block = buf_read_page(page_id, page_size);
        
        if (block == NULL) {
            return NULL;
        }
        
        goto got_block;
    }
    
    block = reinterpret_cast<buf_block_t*>(bpage);
    
got_block:
    /* Fix block in buffer pool */
    buf_block_fix(block);
    
    /* Acquire latch */
    if (rw_latch == RW_S_LATCH) {
        rw_lock_s_lock(&block->lock);
    } else {
        rw_lock_x_lock(&block->lock);
    }
    
    /* Make block young in LRU */
    if (fetch_mode != Page_fetch::NO_LATCH) {
        buf_page_make_young_if_needed(&block->page);
    }
    
    return block;
}

// LRU replacement algorithm
buf_block_t*
buf_LRU_get_free_block(buf_pool_t* buf_pool)
{
    buf_block_t*    block = NULL;
    ulint           n_iterations = 0;
    ulint           flush_failures = 0;
    
    mutex_enter(&buf_pool->LRU_list_mutex);
    
loop:
    /* Try to get from free list */
    block = buf_LRU_get_free_only(buf_pool);
    
    if (block != NULL) {
        mutex_exit(&buf_pool->LRU_list_mutex);
        return block;
    }
    
    /* Scan LRU list for victim */
    ulint   scanned = 0;
    
    for (buf_page_t* bpage = UT_LIST_GET_LAST(buf_pool->LRU);
         bpage != NULL && scanned < BUF_LRU_SEARCH_SCAN_THRESHOLD;
         bpage = UT_LIST_GET_PREV(LRU, bpage), scanned++) {
        
        if (buf_page_can_be_evicted(bpage)) {
            /* Found victim */
            block = reinterpret_cast<buf_block_t*>(bpage);
            
            if (buf_page_get_state(bpage) == BUF_BLOCK_FILE_PAGE) {
                /* Remove from page hash */
                buf_LRU_block_remove_hashed(bpage, true);
            }
            
            /* Remove from LRU */
            buf_LRU_block_free_non_file_page(block);
            
            mutex_exit(&buf_pool->LRU_list_mutex);
            return block;
        }
        
        if (buf_page_get_state(bpage) == BUF_BLOCK_FILE_PAGE
            && bpage->oldest_modification > 0) {
            /* Page is dirty - flush it */
            buf_flush_page(buf_pool, bpage, BUF_FLUSH_LRU);
            flush_failures++;
        }
    }
    
    /* No victim found - wait and retry */
    mutex_exit(&buf_pool->LRU_list_mutex);
    
    if (n_iterations > 20 && flush_failures > 0) {
        /* Flush dirty pages synchronously */
        buf_flush_LRU_list(buf_pool);
    }
    
    os_thread_sleep(10000);  /* 10ms */
    
    n_iterations++;
    
    mutex_enter(&buf_pool->LRU_list_mutex);
    goto loop;
}

// Adaptive hash index integration
void
buf_page_make_young_if_needed(buf_page_t* bpage)
{
    buf_pool_t* buf_pool = buf_pool_from_bpage(bpage);
    
    mutex_enter(&buf_pool->LRU_list_mutex);
    
    if (buf_page_peek_if_too_old(bpage)) {
        /* Move to young end of LRU */
        buf_LRU_make_block_young(bpage);
    }
    
    /* Update access time */
    if (bpage->access_time == 0) {
        buf_page_set_accessed(bpage);
    }
    
    mutex_exit(&buf_pool->LRU_list_mutex);
}

// Flush dirty pages
ulint
buf_flush_LRU_list(buf_pool_t* buf_pool)
{
    ulint   count = 0;
    ulint   scanned = 0;
    
    mutex_enter(&buf_pool->LRU_list_mutex);
    
    for (buf_page_t* bpage = UT_LIST_GET_LAST(buf_pool->LRU);
         bpage != NULL && count < BUF_FLUSH_LRU_MIN_LEN;
         bpage = UT_LIST_GET_PREV(LRU, bpage)) {
        
        if (buf_flush_ready_for_flush(bpage, BUF_FLUSH_LRU)) {
            /* Flush page */
            mutex_exit(&buf_pool->LRU_list_mutex);
            
            buf_flush_page(buf_pool, bpage, BUF_FLUSH_LRU);
            count++;
            
            mutex_enter(&buf_pool->LRU_list_mutex);
        }
        
        scanned++;
        
        if (scanned > BUF_LRU_SEARCH_SCAN_THRESHOLD) {
            break;
        }
    }
    
    mutex_exit(&buf_pool->LRU_list_mutex);
    
    return count;
}
```

## Free Space Management

### InnoDB Free Space Management
```c
// File segment inode
typedef struct fseg_inode_t {
    ulint       fseg_id;            // Segment ID
    ulint       fseg_not_full_n_used; // Used pages in not-full list
    
    /* List of free extents */
    fil_addr_t  fseg_free;
    
    /* List of partially free extents */
    fil_addr_t  fseg_not_full;
    
    /* List of full extents */
    fil_addr_t  fseg_full;
    
    ulint       fseg_magic_n;       // Magic number
    
    /* Fragment array (32 pages) */
    ulint       fseg_frag_arr[FSEG_FRAG_ARR_N_SLOTS];
} fseg_inode_t;

// Allocate page from segment
buf_block_t*
fseg_alloc_free_page_general(
    fseg_header_t*  seg_header,
    ulint           hint,
    byte            direction,
    ibool           has_done_reservation,
    mtr_t*          mtr)
{
    fil_addr_t  inode_addr;
    fseg_inode_t* inode;
    space_id_t  space_id;
    page_no_t   page_no;
    buf_block_t* block;
    
    space_id = page_get_space_id(page_align(seg_header));
    
    /* Get segment inode */
    inode = fseg_inode_get(seg_header, &inode_addr, mtr);
    
    /* Try to allocate from hint page vicinity */
    if (hint != 0) {
        page_no = fseg_alloc_free_page_low(
            inode, hint, direction, mtr);
        
        if (page_no != FIL_NULL) {
            goto allocated;
        }
    }
    
    /* Try to allocate from segment free list */
    if (flst_get_len(inode + FSEG_FREE) > 0) {
        /* Get extent from free list */
        fil_addr_t  node_addr = flst_get_first(inode + FSEG_FREE);
        xdes_t*     descr = xdes_lst_get_descriptor(
            space_id, node_addr, mtr);
        
        page_no = xdes_get_offset(descr) + 
                 xdes_find_free_bit(descr, 0);
        
        xdes_set_bit(descr, XDES_FREE_BIT, page_no % FSP_EXTENT_SIZE, FALSE);
        
        goto allocated;
    }
    
    /* Try to allocate from partially full extent */
    if (flst_get_len(inode + FSEG_NOT_FULL) > 0) {
        fil_addr_t  node_addr = flst_get_first(inode + FSEG_NOT_FULL);
        xdes_t*     descr = xdes_lst_get_descriptor(
            space_id, node_addr, mtr);
        
        page_no = xdes_get_offset(descr) + 
                 xdes_find_free_bit(descr, 0);
        
        xdes_set_bit(descr, XDES_FREE_BIT, page_no % FSP_EXTENT_SIZE, FALSE);
        
        /* Check if extent is now full */
        if (xdes_is_full(descr)) {
            /* Move to full list */
            flst_remove(inode + FSEG_NOT_FULL, descr + XDES_FLST_NODE, mtr);
            flst_add_last(inode + FSEG_FULL, descr + XDES_FLST_NODE, mtr);
        }
        
        goto allocated;
    }
    
    /* Allocate new extent */
    if (fsp_alloc_free_extent(space_id, hint, mtr) != NULL) {
        /* Retry allocation */
        return fseg_alloc_free_page_general(
            seg_header, hint, direction, has_done_reservation, mtr);
    }
    
    /* No space available */
    return NULL;
    
allocated:
    /* Initialize page */
    block = buf_page_create(page_id_t(space_id, page_no), 
                          univ_page_size, mtr);
    
    return block;
}

// Free page to segment
void
fseg_free_page(
    fseg_header_t*  seg_header,
    space_id_t      space_id,
    page_no_t       page_no,
    mtr_t*          mtr)
{
    fseg_inode_t*   inode;
    xdes_t*         descr;
    
    /* Get segment inode */
    inode = fseg_inode_get(seg_header, NULL, mtr);
    
    /* Get extent descriptor */
    descr = xdes_get_descriptor(space_id, page_no, mtr);
    
    /* Mark page as free in extent */
    xdes_set_bit(descr, XDES_FREE_BIT, page_no % FSP_EXTENT_SIZE, TRUE);
    
    /* Check if extent should be moved between lists */
    if (xdes_is_free(descr)) {
        /* Extent is now completely free */
        flst_remove(inode + FSEG_NOT_FULL, descr + XDES_FLST_NODE, mtr);
        flst_add_last(inode + FSEG_FREE, descr + XDES_FLST_NODE, mtr);
    } else if (xdes_get_n_used(descr) == FSP_EXTENT_SIZE - 1) {
        /* Extent was full, now has one free page */
        flst_remove(inode + FSEG_FULL, descr + XDES_FLST_NODE, mtr);
        flst_add_last(inode + FSEG_NOT_FULL, descr + XDES_FLST_NODE, mtr);
    }
}
```

---

# Microsoft SQL Server Storage Management

## Buffer Pool Algorithms

### SQL Server Buffer Manager
```c
// Buffer descriptor
typedef struct BUF {
    struct BUF     *hashNext;       // Next in hash chain
    struct BUF     *hashPrev;       // Previous in hash chain
    struct BUF     *lruNext;        // Next in LRU
    struct BUF     *lruPrev;        // Previous in LRU
    
    DBID            dbid;           // Database ID
    FILEID          fileid;         // File ID
    PAGEID          pageid;         // Page ID
    
    BYTE           *page;           // Page data
    
    LONG            pinCount;       // Pin count
    LONG            referenceCount; // Reference count
    
    DWORD           status;         // Status flags
    DWORD           waiters;        // Number of waiters
    
    HANDLE          event;          // Wait event
    
    LSN             pageLSN;        // Page LSN
    LSN             recoveryLSN;    // Recovery LSN
} BUF;

// Buffer pool
typedef struct BUFFER_POOL {
    BUF            *buffers;        // Array of buffers
    ULONG           bufferCount;    // Number of buffers
    
    BUF           **hashTable;      // Hash table
    ULONG           hashSize;       // Hash table size
    
    BUF            *lruHead;        // LRU head
    BUF            *lruTail;        // LRU tail
    BUF            *freeList;       // Free list
    
    CRITICAL_SECTION lruLock;      // LRU lock
    CRITICAL_SECTION hashLock;     // Hash lock
    
    HANDLE          lazyWriter;     // Lazy writer thread
    HANDLE          checkpoint;     // Checkpoint thread
    
    /* Statistics */
    ULONGLONG       pageReads;      // Page reads
    ULONGLONG       pageWrites;     // Page writes
    ULONGLONG       cacheHits;      // Cache hits
    ULONGLONG       cacheMisses;    // Cache misses
} BUFFER_POOL;

// Get page from buffer pool
BUF*
GetPage(DBID dbid, FILEID fileid, PAGEID pageid, DWORD flags)
{
    BUFFER_POOL    *pool = GetBufferPool();
    BUF            *buf;
    ULONG           hash;
    
    /* Calculate hash */
    hash = HashPage(dbid, fileid, pageid) % pool->hashSize;
    
    /* Look up in hash table */
    EnterCriticalSection(&pool->hashLock);
    
    for (buf = pool->hashTable[hash]; buf; buf = buf->hashNext) {
        if (buf->dbid == dbid && 
            buf->fileid == fileid && 
            buf->pageid == pageid) {
            
            /* Found in cache */
            InterlockedIncrement(&buf->pinCount);
            
            LeaveCriticalSection(&pool->hashLock);
            
            /* Update LRU */
            UpdateLRU(pool, buf);
            
            /* Update statistics */
            InterlockedIncrement64(&pool->cacheHits);
            
            return buf;
        }
    }
    
    LeaveCriticalSection(&pool->hashLock);
    
    /* Not in cache - need to read */
    InterlockedIncrement64(&pool->cacheMisses);
    
    /* Get free buffer */
    buf = GetFreeBuffer(pool);
    
    if (!buf) {
        /* No free buffers - evict one */
        buf = EvictPage(pool);
    }
    
    /* Initialize buffer */
    buf->dbid = dbid;
    buf->fileid = fileid;
    buf->pageid = pageid;
    buf->pinCount = 1;
    buf->status = 0;
    
    /* Add to hash table */
    EnterCriticalSection(&pool->hashLock);
    
    buf->hashNext = pool->hashTable[hash];
    if (buf->hashNext) {
        buf->hashNext->hashPrev = buf;
    }
    pool->hashTable[hash] = buf;
    
    LeaveCriticalSection(&pool->hashLock);
    
    /* Read page from disk */
    ReadPageFromDisk(buf);
    
    /* Add to LRU */
    AddToLRU(pool, buf);
    
    return buf;
}

// Evict page using clock algorithm
BUF*
EvictPage(BUFFER_POOL *pool)
{
    BUF            *buf;
    BUF            *victim = NULL;
    ULONG           passes = 0;
    
    EnterCriticalSection(&pool->lruLock);
    
    /* Clock algorithm */
    while (!victim && passes < 2) {
        for (buf = pool->lruTail; buf; buf = buf->lruPrev) {
            /* Skip pinned pages */
            if (buf->pinCount > 0) {
                continue;
            }
            
            /* Check reference count */
            if (buf->referenceCount > 0) {
                /* Give second chance */
                InterlockedDecrement(&buf->referenceCount);
                continue;
            }
            
            /* Found victim */
            victim = buf;
            break;
        }
        
        passes++;
    }
    
    if (!victim) {
        /* Emergency - pick any unpinned page */
        for (buf = pool->lruTail; buf; buf = buf->lruPrev) {
            if (buf->pinCount == 0) {
                victim = buf;
                break;
            }
        }
    }
    
    if (victim) {
        /* Remove from LRU */
        RemoveFromLRU(pool, victim);
        
        /* Write if dirty */
        if (victim->status & BUF_DIRTY) {
            WritePageToDisk(victim);
        }
        
        /* Remove from hash table */
        RemoveFromHash(pool, victim);
    }
    
    LeaveCriticalSection(&pool->lruLock);
    
    return victim;
}

// Lazy writer thread
DWORD WINAPI
LazyWriterThread(LPVOID param)
{
    BUFFER_POOL    *pool = (BUFFER_POOL *)param;
    BUF            *buf;
    ULONG           written;
    
    while (!pool->shutdown) {
        /* Sleep for interval */
        Sleep(LAZY_WRITER_INTERVAL);
        
        written = 0;
        
        EnterCriticalSection(&pool->lruLock);
        
        /* Scan LRU for dirty pages */
        for (buf = pool->lruTail; 
             buf && written < LAZY_WRITER_BATCH_SIZE; 
             buf = buf->lruPrev) {
            
            if ((buf->status & BUF_DIRTY) && buf->pinCount == 0) {
                /* Write dirty page */
                LeaveCriticalSection(&pool->lruLock);
                
                WritePageToDisk(buf);
                InterlockedAnd(&buf->status, ~BUF_DIRTY);
                written++;
                
                EnterCriticalSection(&pool->lruLock);
            }
        }
        
        LeaveCriticalSection(&pool->lruLock);
        
        /* Update statistics */
        InterlockedAdd64(&pool->pageWrites, written);
    }
    
    return 0;
}

// Checkpoint thread
DWORD WINAPI
CheckpointThread(LPVOID param)
{
    BUFFER_POOL    *pool = (BUFFER_POOL *)param;
    BUF            *buf;
    LSN             checkpointLSN;
    
    while (!pool->shutdown) {
        /* Wait for checkpoint interval */
        Sleep(CHECKPOINT_INTERVAL);
        
        /* Get checkpoint LSN */
        checkpointLSN = GetCurrentLSN();
        
        /* Write all dirty pages */
        EnterCriticalSection(&pool->lruLock);
        
        for (buf = pool->lruHead; buf; buf = buf->lruNext) {
            if ((buf->status & BUF_DIRTY) && 
                buf->pageLSN <= checkpointLSN) {
                
                LeaveCriticalSection(&pool->lruLock);
                
                WritePageToDisk(buf);
                InterlockedAnd(&buf->status, ~BUF_DIRTY);
                
                EnterCriticalSection(&pool->lruLock);
            }
        }
        
        LeaveCriticalSection(&pool->lruLock);
        
        /* Write checkpoint record */
        WriteCheckpointRecord(checkpointLSN);
    }
    
    return 0;
}
```

## Free Space Management

### SQL Server GAM/SGAM/PFS Pages
```c
// Global Allocation Map (GAM) - 1 bit per extent
typedef struct GAM_PAGE {
    PAGE_HEADER     header;
    BYTE            bits[8088];     // 64,000 extents per GAM page
} GAM_PAGE;

// Shared Global Allocation Map (SGAM) - 1 bit per mixed extent
typedef struct SGAM_PAGE {
    PAGE_HEADER     header;
    BYTE            bits[8088];     // 64,000 extents per SGAM page
} SGAM_PAGE;

// Page Free Space (PFS) - 1 byte per page
typedef struct PFS_PAGE {
    PAGE_HEADER     header;
    BYTE            bytes[8088];    // 8,088 pages per PFS page
} PFS_PAGE;

// PFS byte structure
#define PFS_IS_ALLOCATED    0x01    // Page is allocated
#define PFS_IS_MIXED_EXT    0x02    // Page is in mixed extent
#define PFS_IS_IAM_PAGE     0x04    // Page is IAM page
#define PFS_HAS_GHOST       0x08    // Page has ghost records
#define PFS_FULLNESS_MASK   0xE0    // Fullness level (3 bits)

// Allocate extent
EXTENT_ID
AllocateExtent(DATABASE *db, FILEID fileid, BOOL mixed)
{
    GAM_PAGE       *gam;
    SGAM_PAGE      *sgam;
    EXTENT_ID       extent;
    PAGEID          gamPageId;
    ULONG           bitOffset;
    
    /* Calculate GAM page */
    gamPageId = (fileid * GAM_INTERVAL);
    
    /* Lock GAM page */
    gam = (GAM_PAGE *)GetPage(db->dbid, fileid, gamPageId, PAGE_EXCLUSIVE);
    
    if (mixed) {
        /* Allocate mixed extent - also need SGAM */
        sgam = (SGAM_PAGE *)GetPage(db->dbid, fileid, 
                                   gamPageId + 1, PAGE_EXCLUSIVE);
    }
    
    /* Find free extent */
    for (bitOffset = 0; bitOffset < GAM_BITS_PER_PAGE; bitOffset++) {
        ULONG byteOffset = bitOffset / 8;
        ULONG bitMask = 1 << (bitOffset % 8);
        
        if (gam->bits[byteOffset] & bitMask) {
            /* Extent is free */
            
            if (mixed) {
                /* Check if it can be mixed */
                if (!(sgam->bits[byteOffset] & bitMask)) {
                    continue;  /* Already mixed with free space */
                }
                
                /* Mark as mixed */
                sgam->bits[byteOffset] &= ~bitMask;
            }
            
            /* Mark as allocated */
            gam->bits[byteOffset] &= ~bitMask;
            
            extent = (fileid * EXTENTS_PER_FILE) + 
                    (gamPageId / GAM_INTERVAL * GAM_BITS_PER_PAGE) + 
                    bitOffset;
            
            /* Mark pages dirty */
            MarkPageDirty((BUF *)gam);
            if (mixed) {
                MarkPageDirty((BUF *)sgam);
            }
            
            /* Release pages */
            ReleasePage((BUF *)gam);
            if (mixed) {
                ReleasePage((BUF *)sgam);
            }
            
            return extent;
        }
    }
    
    /* No free extents */
    ReleasePage((BUF *)gam);
    if (mixed) {
        ReleasePage((BUF *)sgam);
    }
    
    /* Grow file */
    GrowFile(db, fileid);
    
    /* Retry */
    return AllocateExtent(db, fileid, mixed);
}

// Update PFS for page
void
UpdatePFS(DATABASE *db, PAGEID pageId, BYTE pfsFlags)
{
    PFS_PAGE       *pfs;
    PAGEID          pfsPageId;
    ULONG           pageOffset;
    
    /* Calculate PFS page */
    pfsPageId = (pageId / PFS_PAGES_PER_INTERVAL) * PFS_PAGES_PER_INTERVAL;
    pageOffset = pageId % PFS_PAGES_PER_INTERVAL;
    
    /* Get PFS page */
    pfs = (PFS_PAGE *)GetPage(db->dbid, 
                             FILEID_FROM_PAGEID(pageId),
                             pfsPageId, 
                             PAGE_EXCLUSIVE);
    
    /* Update PFS byte */
    pfs->bytes[pageOffset] = pfsFlags;
    
    /* Mark dirty and release */
    MarkPageDirty((BUF *)pfs);
    ReleasePage((BUF *)pfs);
}

// Find page with free space
PAGEID
FindPageWithSpace(DATABASE *db, FILEID fileid, ULONG spaceNeeded)
{
    PFS_PAGE       *pfs;
    PAGEID          pfsPageId;
    PAGEID          pageId;
    ULONG           i;
    BYTE            requiredFullness;
    
    /* Calculate required fullness level */
    if (spaceNeeded > 6144) {
        requiredFullness = 0x00;  /* 0-50% full */
    } else if (spaceNeeded > 4096) {
        requiredFullness = 0x20;  /* 51-80% full */
    } else if (spaceNeeded > 2048) {
        requiredFullness = 0x40;  /* 81-95% full */
    } else {
        requiredFullness = 0x60;  /* 96-100% full */
    }
    
    /* Scan PFS pages */
    for (pfsPageId = 0; 
         pfsPageId < GetFileSize(db, fileid); 
         pfsPageId += PFS_PAGES_PER_INTERVAL) {
        
        pfs = (PFS_PAGE *)GetPage(db->dbid, fileid, pfsPageId, PAGE_SHARED);
        
        /* Scan PFS bytes */
        for (i = 1; i < PFS_PAGES_PER_INTERVAL && 
             i < GetFileSize(db, fileid) - pfsPageId; i++) {
            
            BYTE pfsByte = pfs->bytes[i];
            
            /* Check if allocated and has enough space */
            if ((pfsByte & PFS_IS_ALLOCATED) &&
                (pfsByte & PFS_FULLNESS_MASK) <= requiredFullness) {
                
                pageId = pfsPageId + i;
                ReleasePage((BUF *)pfs);
                return pageId;
            }
        }
        
        ReleasePage((BUF *)pfs);
    }
    
    /* No suitable page found */
    return INVALID_PAGEID;
}
```