# FirebirdSQL Multi-Generational Architecture (MGA) Technical Reference

## Table of Contents
1. [MGA Overview](#mga-overview)
2. [Transaction ID Generation](#transaction-id-generation)
3. [Version Chain Management](#version-chain-management)
4. [Garbage Collection Algorithm](#garbage-collection-algorithm)
5. [TIP Page Format](#tip-page-format)
6. [Implementation Details](#implementation-details)

---

## MGA Overview

Multi-Generational Architecture (MGA) is FirebirdSQL's implementation of MVCC (Multi-Version Concurrency Control) that provides:
- Non-blocking reads
- Consistent snapshots
- ACID compliance without read locks
- Multiple versions of each record stored on data pages

### Core Concepts
- **Record Version**: Each modification creates a new version
- **Transaction ID**: Monotonically increasing identifier for each transaction
- **Back Version**: Previous version of a record linked via pointer
- **Transaction Inventory Page (TIP)**: Bitmap tracking transaction states
- **Oldest Interesting Transaction (OIT)**: Oldest transaction that may affect visibility
- **Oldest Active Transaction (OAT)**: Oldest transaction still running

---

## Transaction ID Generation

### Transaction ID Structure

```c
// Transaction ID is a 32-bit unsigned integer (Classic/SuperClassic)
// or 48-bit in Firebird 4.0+ for extended transaction space
typedef ULONG TraNumber;      // Classic transaction number
typedef UINT64 TraNumber64;   // Extended transaction number (FB 4.0+)

// Transaction ID generation structure
typedef struct transaction_id_block {
    TraNumber   tib_next;           // Next transaction ID to assign
    TraNumber   tib_oldest;         // Oldest interesting transaction
    TraNumber   tib_oldest_active;  // Oldest active transaction
    TraNumber   tib_oldest_snapshot; // Oldest snapshot transaction
    TraNumber   tib_next_attachment; // Next attachment ID
    SLONG       tib_sweep_interval;  // Automatic sweep interval
    USHORT      tib_database_flags;  // Database state flags
} TIB;

// Transaction ID allocation
TraNumber allocate_transaction_id(Database* dbb) {
    // Lock the header page for exclusive access
    WIN window;
    HeaderPage* header = (HeaderPage*) CCH_FETCH(dbb, &window, LCK_write);
    
    // Get next transaction ID
    TraNumber trans_id = header->hdr_next_transaction++;
    
    // Check for wraparound (32-bit systems)
    if (header->hdr_next_transaction >= MAX_TRA_NUMBER) {
        // Handle transaction ID exhaustion
        if (!can_sweep_now(dbb)) {
            ERR_post(isc_tra_num_exc, 0);
        }
        // Force sweep to reclaim transaction IDs
        start_sweep(dbb, header->hdr_oldest_transaction);
    }
    
    // Update TIP pages if needed
    if (trans_id % TRANS_PER_TIP == 0) {
        extend_tip(dbb, trans_id);
    }
    
    CCH_RELEASE(dbb, &window);
    return trans_id;
}

// Constants
#define MAX_TRA_NUMBER      0x7FFFFFFF  // Maximum 31-bit transaction ID
#define MAX_TRA_NUMBER_64   0x0000FFFFFFFFFFFF  // Maximum 48-bit (FB 4.0+)
#define TRANS_PER_TIP       (BITS_PER_LONG * 4)  // Transactions per TIP entry
#define BITS_PER_LONG       32
```

### Transaction ID Assignment Process

```c
// Transaction start sequence
typedef struct transaction {
    TraNumber       tra_number;          // This transaction's ID
    TraNumber       tra_top;              // Highest transaction at start
    TraNumber       tra_oldest;          // Oldest interesting at start
    TraNumber       tra_oldest_active;   // OAT at start
    UCHAR           tra_state;           // Current state
    USHORT          tra_flags;           // Transaction flags
    Lock*           tra_lock;            // Transaction lock
    TxPageCache*    tra_tip_cache;       // Cached TIP pages
    RecordBitmap*   tra_save_point;      // Savepoint data
    struct transaction* tra_next;        // Next in list
    struct transaction* tra_prev;        // Previous in list
    Attachment*     tra_attachment;      // Owning attachment
    jrd_req*        tra_requests;        // Active requests
    DeferredWork*   tra_deferred_work;   // Deferred work items
    SINT64          tra_oldest_active_snapshot; // For read consistency
} Transaction;

Transaction* TRA_start(Database* dbb, Attachment* attachment, 
                      USHORT flags) {
    Transaction* trans = FB_NEW Transaction();
    
    // Acquire transaction ID
    trans->tra_number = allocate_transaction_id(dbb);
    
    // Snapshot current state
    trans->tra_top = dbb->dbb_next_transaction - 1;
    trans->tra_oldest = dbb->dbb_oldest_transaction;
    trans->tra_oldest_active = dbb->dbb_oldest_active;
    
    // Set initial state
    trans->tra_state = tra_active;
    trans->tra_flags = flags;
    
    // Create transaction lock
    trans->tra_lock = create_transaction_lock(dbb, trans->tra_number);
    
    // Cache TIP state for this transaction
    cache_tip_state(trans, dbb);
    
    // Add to active transaction list
    add_to_active_list(dbb, trans);
    
    // Update TIP to mark as active
    TIP_set_state(dbb, trans->tra_number, tra_active);
    
    return trans;
}
```

### Transaction State Machine

```c
// Transaction states in TIP
#define tra_active      0   // Transaction is active
#define tra_limbo       1   // Transaction in limbo (2PC)
#define tra_dead        2   // Transaction rolled back
#define tra_committed   3   // Transaction committed

// State transitions
void TRA_commit(Transaction* trans, bool retain_context) {
    Database* dbb = trans->tra_database;
    
    // Phase 1: Prepare
    if (trans->tra_flags & TRA_prepared) {
        // Already prepared (2PC)
    } else {
        // Flush all dirty pages
        flush_transaction_pages(trans);
        
        // Write commit record to WAL
        write_commit_record(trans);
    }
    
    // Phase 2: Update TIP
    TIP_set_state(dbb, trans->tra_number, tra_committed);
    
    // Phase 3: Release resources
    if (!retain_context) {
        release_transaction_locks(trans);
        remove_from_active_list(dbb, trans);
        
        // Update OAT if necessary
        if (trans->tra_number == dbb->dbb_oldest_active) {
            update_oldest_active(dbb);
        }
    }
    
    // Phase 4: Trigger garbage collection if needed
    if (should_garbage_collect(dbb, trans)) {
        schedule_garbage_collection(dbb);
    }
}

void TRA_rollback(Transaction* trans, bool retain_context) {
    Database* dbb = trans->tra_database;
    
    // Mark transaction as dead
    TIP_set_state(dbb, trans->tra_number, tra_dead);
    
    // Undo changes (for savepoints)
    if (trans->tra_save_point) {
        undo_savepoint_changes(trans);
    }
    
    // Release resources
    if (!retain_context) {
        release_transaction_locks(trans);
        remove_from_active_list(dbb, trans);
    }
}
```

---

## Version Chain Management

### Record Version Structure

```c
// Record header format (on-disk)
typedef struct rhd {
    TraNumber   rhd_transaction;   // Transaction that created version
    USHORT      rhd_b_page;        // Back pointer page number
    USHORT      rhd_b_line;        // Back pointer line number
    USHORT      rhd_flags;         // Record flags
    UCHAR       rhd_format;        // Record format version
    UCHAR       rhd_data[1];       // Record data follows
} RHD;

// Record header flags
#define rhd_deleted     0x01    // Record is deleted
#define rhd_chain       0x02    // Record is part of version chain
#define rhd_fragment    0x04    // Record is fragmented
#define rhd_incomplete  0x08    // Record is incomplete
#define rhd_blob        0x10    // Record contains blob
#define rhd_delta       0x20    // Record is delta-compressed
#define rhd_large       0x40    // Record is large
#define rhd_damaged     0x80    // Record is damaged

// Extended record header for versioning
typedef struct rhdf {
    TraNumber   rhdf_transaction;  // Transaction ID
    USHORT      rhdf_b_page;      // Back version page
    USHORT      rhdf_b_line;      // Back version slot
    USHORT      rhdf_flags;       // Flags
    UCHAR       rhdf_format;      // Format number
    TraNumber   rhdf_update_trans; // Last update transaction
    ULONG       rhdf_back_tid;    // Back version TID
    UCHAR       rhdf_data[1];     // Actual data
} RHDF;
```

### Version Chain Creation

```c
// Creating a new record version
RecordNumber DPM_store(Transaction* trans, Record* record, 
                       RecordNumber* back_record) {
    Database* dbb = trans->tra_database;
    
    // Allocate space for new version
    DataPage* page = find_space_for_record(dbb, record->rec_length);
    
    // Build record header
    RHD* header = (RHD*) page->dpg_rpt[slot].dpg_offset;
    header->rhd_transaction = trans->tra_number;
    
    // Link to previous version if updating
    if (back_record) {
        header->rhd_b_page = back_record->rec_page;
        header->rhd_b_line = back_record->rec_slot;
        header->rhd_flags |= rhd_chain;
        
        // Mark old version as having a newer version
        mark_record_as_versioned(dbb, back_record);
    }
    
    // Copy record data
    memcpy(header->rhd_data, record->rec_data, record->rec_length);
    
    // Update page header
    page->dpg_count++;
    page->dpg_header.pag_generation++;
    
    // Mark page as dirty
    CCH_MARK(dbb, page, trans->tra_number);
    
    return create_record_number(page->dpg_number, slot);
}

// Updating an existing record
RecordNumber DPM_update(Transaction* trans, Record* old_record, 
                        Record* new_record) {
    Database* dbb = trans->tra_database;
    
    // Check if we can update in place
    if (can_update_in_place(trans, old_record)) {
        // Delta compression for minor changes
        if (use_delta_compression(old_record, new_record)) {
            return store_delta_record(trans, old_record, new_record);
        }
    }
    
    // Create new version with back pointer
    RecordNumber old_num = old_record->rec_number;
    RecordNumber new_num = DPM_store(trans, new_record, &old_num);
    
    // Update indexes if needed
    update_index_entries(trans, old_num, new_num);
    
    return new_num;
}
```

### Version Visibility Rules

```c
// Determine if a record version is visible to a transaction
typedef enum {
    vis_visible,      // Version is visible
    vis_invisible,    // Version is not visible
    vis_deleted,      // Version is deleted
    vis_unknown       // Need to check TIP
} VisibilityState;

VisibilityState check_visibility(Transaction* reader, RHD* version) {
    TraNumber version_trans = version->rhd_transaction;
    TraNumber reader_trans = reader->tra_number;
    
    // Rule 1: Own changes are visible
    if (version_trans == reader_trans) {
        return (version->rhd_flags & rhd_deleted) ? 
               vis_deleted : vis_visible;
    }
    
    // Rule 2: Changes after reader started are invisible
    if (version_trans > reader->tra_top) {
        return vis_invisible;
    }
    
    // Rule 3: Check transaction state from TIP cache
    UCHAR state = get_transaction_state(reader, version_trans);
    
    switch (state) {
        case tra_committed:
            // Committed before we started - visible
            if (version_trans < reader->tra_oldest_active) {
                return (version->rhd_flags & rhd_deleted) ? 
                       vis_deleted : vis_visible;
            }
            // Need to check commit order
            return check_commit_order(reader, version_trans);
            
        case tra_active:
            // Still active - invisible
            return vis_invisible;
            
        case tra_dead:
            // Rolled back - invisible
            return vis_invisible;
            
        case tra_limbo:
            // In limbo - wait or skip based on settings
            return handle_limbo_transaction(reader, version_trans);
    }
    
    return vis_unknown;
}

// Walk version chain to find visible version
Record* VIO_chase_record_version(Transaction* trans, RecordNumber rec_num) {
    Database* dbb = trans->tra_database;
    RHD* version = fetch_record(dbb, rec_num);
    
    while (version) {
        VisibilityState vis = check_visibility(trans, version);
        
        if (vis == vis_visible) {
            // Found visible version
            return materialize_record(version);
        }
        
        if (vis == vis_deleted) {
            // Record was deleted in a visible transaction
            return NULL;
        }
        
        // Check if there's a back version
        if (!(version->rhd_flags & rhd_chain)) {
            // No more versions
            return NULL;
        }
        
        // Follow back pointer
        rec_num.rec_page = version->rhd_b_page;
        rec_num.rec_slot = version->rhd_b_line;
        version = fetch_record(dbb, rec_num);
    }
    
    return NULL;
}
```

### Delta Compression

```c
// Delta record structure for minor updates
typedef struct delta_record {
    USHORT  dlt_offset;     // Offset of first change
    USHORT  dlt_length;     // Length of changed data
    UCHAR   dlt_data[1];    // New data for changed portion
} DeltaRecord;

// Create delta-compressed version
RecordNumber store_delta_record(Transaction* trans, 
                               Record* old_record, 
                               Record* new_record) {
    // Find differences
    USHORT offset, length;
    find_record_differences(old_record, new_record, &offset, &length);
    
    // Create delta record
    USHORT delta_size = sizeof(DeltaRecord) + length;
    DeltaRecord* delta = (DeltaRecord*) allocate(delta_size);
    
    delta->dlt_offset = offset;
    delta->dlt_length = length;
    memcpy(delta->dlt_data, new_record->rec_data + offset, length);
    
    // Store with special flag
    RHD* header = store_record_header(trans, delta_size + RHD_SIZE);
    header->rhd_flags |= rhd_delta;
    header->rhd_b_page = old_record->rec_page;
    header->rhd_b_line = old_record->rec_slot;
    
    memcpy(header->rhd_data, delta, delta_size);
    
    return header_to_record_number(header);
}

// Apply delta to reconstruct record
Record* apply_delta(Record* base_record, DeltaRecord* delta) {
    Record* new_record = copy_record(base_record);
    
    // Apply delta changes
    memcpy(new_record->rec_data + delta->dlt_offset,
           delta->dlt_data,
           delta->dlt_length);
    
    return new_record;
}
```

---

## Garbage Collection Algorithm

### Garbage Collection Overview

```c
// Garbage collection modes
typedef enum {
    GC_BACKGROUND,     // Background garbage collector thread
    GC_COOPERATIVE,    // Cooperative (classic)
    GC_COMBINED,       // Combined (SuperServer)
    GC_SWEEP,         // Database sweep
    GC_NONE           // No garbage collection
} GCMode;

// Garbage collection state
typedef struct gc_state {
    TraNumber   gc_oit;            // Oldest interesting transaction
    TraNumber   gc_oat;            // Oldest active transaction  
    TraNumber   gc_ost;            // Oldest snapshot transaction
    ULONG       gc_pages_collected; // Pages garbage collected
    ULONG       gc_records_removed; // Back versions removed
    ULONG       gc_records_updated; // Records updated in place
    ULONG       gc_fragments_removed; // Fragments removed
    bool        gc_active;         // GC currently running
    ThreadId    gc_thread;         // GC thread ID
} GCState;
```

### Cooperative Garbage Collection

```c
// Cooperative GC - readers clean up old versions
void garbage_collect_record(Transaction* trans, RecordNumber rec_num) {
    Database* dbb = trans->tra_database;
    
    // Get exclusive access to record
    RecordLock* lock = lock_record(dbb, rec_num, LCK_write);
    
    RHD* version = fetch_record(dbb, rec_num);
    RHD* previous = NULL;
    
    while (version) {
        TraNumber ver_trans = version->rhd_transaction;
        
        // Can we garbage collect this version?
        if (can_garbage_collect(trans, ver_trans)) {
            if (previous) {
                // Remove intermediate version
                remove_back_version(dbb, previous, version);
            } else {
                // This is the primary version
                if (version->rhd_flags & rhd_chain) {
                    // Promote next version to primary
                    promote_back_version(dbb, version);
                } else if (version->rhd_flags & rhd_deleted) {
                    // Remove deleted stub
                    remove_record_stub(dbb, rec_num);
                }
            }
            
            // Update statistics
            dbb->dbb_gc_state.gc_records_removed++;
        }
        
        // Move to next back version
        if (version->rhd_flags & rhd_chain) {
            RecordNumber back_num;
            back_num.rec_page = version->rhd_b_page;
            back_num.rec_slot = version->rhd_b_line;
            
            previous = version;
            version = fetch_record(dbb, back_num);
        } else {
            break;
        }
    }
    
    unlock_record(lock);
}

// Determine if version can be garbage collected
bool can_garbage_collect(Transaction* trans, TraNumber ver_trans) {
    Database* dbb = trans->tra_database;
    
    // Version must be committed or dead
    UCHAR state = TIP_get_state(dbb, ver_trans);
    if (state != tra_committed && state != tra_dead) {
        return false;
    }
    
    // No active transaction should depend on this version
    if (ver_trans >= dbb->dbb_oldest_active) {
        // Check if any active transaction might need this
        return !has_active_dependency(dbb, ver_trans);
    }
    
    // Safe to collect - older than all active transactions
    return true;
}
```

### Background Garbage Collection Thread

```c
// Background GC thread main loop
void garbage_collector_thread(Database* dbb) {
    GCState* gc = &dbb->dbb_gc_state;
    gc->gc_active = true;
    gc->gc_thread = Thread::getId();
    
    while (!dbb->dbb_shutdown) {
        // Wait for work or timeout
        gc_event_wait(dbb, GC_INTERVAL);
        
        // Check if GC is needed
        if (!should_run_gc(dbb)) {
            continue;
        }
        
        // Get current OIT/OAT
        TraNumber oit = get_oldest_interesting(dbb);
        TraNumber oat = get_oldest_active(dbb);
        
        // Scan data pages
        for (ULONG page_num = FIRST_DATA_PAGE; 
             page_num < dbb->dbb_page_count; 
             page_num++) {
            
            // Check for shutdown
            if (dbb->dbb_shutdown) break;
            
            // Skip non-data pages
            if (!is_data_page(dbb, page_num)) continue;
            
            // Process page
            garbage_collect_page(dbb, page_num, oit, oat);
            
            // Yield periodically
            if (page_num % GC_YIELD_INTERVAL == 0) {
                Thread::yield();
            }
        }
        
        // Update statistics
        update_gc_statistics(dbb, gc);
    }
    
    gc->gc_active = false;
}

// Garbage collect a single page
void garbage_collect_page(Database* dbb, ULONG page_num, 
                          TraNumber oit, TraNumber oat) {
    // Fetch page with read lock
    WIN window;
    DataPage* page = (DataPage*) CCH_FETCH(dbb, &window, LCK_read);
    
    bool modified = false;
    
    // Check each record slot
    for (USHORT slot = 0; slot < page->dpg_count; slot++) {
        if (page->dpg_rpt[slot].dpg_length == 0) {
            continue;  // Empty slot
        }
        
        RHD* record = (RHD*) ((UCHAR*)page + page->dpg_rpt[slot].dpg_offset);
        
        // Check if record can be garbage collected
        if (should_gc_record(record, oit, oat)) {
            // Need write lock to modify
            if (!modified) {
                CCH_RELEASE(dbb, &window);
                page = (DataPage*) CCH_FETCH(dbb, &window, LCK_write);
                modified = true;
            }
            
            // Garbage collect this record
            gc_record_on_page(page, slot, record);
        }
    }
    
    if (modified) {
        CCH_MARK(dbb, &window);
    }
    
    CCH_RELEASE(dbb, &window);
}
```

### Database Sweep

```c
// Database sweep - aggressive garbage collection
typedef struct sweep_control {
    TraNumber   swp_oit;           // OIT at sweep start
    TraNumber   swp_oat;           // OAT at sweep start
    TraNumber   swp_current;       // Current transaction being swept
    ULONG       swp_page_current;  // Current page
    ULONG       swp_page_count;    // Total pages
    ULONG       swp_records_swept; // Records processed
    bool        swp_active;        // Sweep active
    time_t      swp_started;       // Sweep start time
} SweepControl;

void sweep_database(Database* dbb) {
    SweepControl* sweep = &dbb->dbb_sweep;
    
    // Initialize sweep
    sweep->swp_oit = get_oldest_interesting(dbb);
    sweep->swp_oat = get_oldest_active(dbb);
    sweep->swp_active = true;
    sweep->swp_started = time(NULL);
    
    // Phase 1: Clean up old transactions
    sweep_transactions(dbb, sweep);
    
    // Phase 2: Sweep all data pages
    for (ULONG page = FIRST_DATA_PAGE; 
         page < dbb->dbb_page_count; 
         page++) {
        
        sweep_page(dbb, sweep, page);
        sweep->swp_page_current = page;
        
        // Check for shutdown
        if (dbb->dbb_shutdown_requested) {
            break;
        }
    }
    
    // Phase 3: Update OIT
    update_oldest_interesting(dbb, sweep->swp_oat);
    
    // Phase 4: Clean up TIP pages
    compress_tip_pages(dbb);
    
    sweep->swp_active = false;
    
    // Log sweep completion
    log_sweep_complete(dbb, sweep);
}

// Sweep individual page
void sweep_page(Database* dbb, SweepControl* sweep, ULONG page_num) {
    WIN window;
    DataPage* page = (DataPage*) CCH_FETCH(dbb, &window, LCK_write);
    
    for (USHORT slot = 0; slot < page->dpg_count; slot++) {
        if (page->dpg_rpt[slot].dpg_length == 0) continue;
        
        RHD* record = get_record(page, slot);
        
        // Remove all unnecessary back versions
        sweep_record_versions(dbb, sweep, record);
        
        // Check for deleted stubs
        if (record->rhd_flags & rhd_deleted) {
            TraNumber del_trans = record->rhd_transaction;
            
            // If delete is committed and old enough
            if (del_trans < sweep->swp_oit) {
                // Remove the stub
                remove_record(page, slot);
                sweep->swp_records_swept++;
            }
        }
    }
    
    CCH_MARK(dbb, &window);
    CCH_RELEASE(dbb, &window);
}

// Automatic sweep trigger
bool should_trigger_sweep(Database* dbb) {
    TraNumber oit = get_oldest_interesting(dbb);
    TraNumber oat = get_oldest_active(dbb);
    TraNumber next = get_next_transaction(dbb);
    
    // Check sweep interval
    SLONG sweep_interval = dbb->dbb_sweep_interval;
    if (sweep_interval == 0) {
        return false;  // Sweep disabled
    }
    
    // Trigger if gap exceeds threshold
    if ((oat - oit) > sweep_interval) {
        return true;
    }
    
    // Also check for transaction ID wraparound risk
    if (next > (MAX_TRA_NUMBER - SWEEP_SAFETY_MARGIN)) {
        return true;
    }
    
    return false;
}
```

---

## TIP Page Format

### TIP Page Structure

```c
// Transaction Inventory Page layout
typedef struct tip_page {
    PageHeader  tip_header;         // Standard page header
    TraNumber   tip_min;            // Minimum transaction on page
    TraNumber   tip_max;            // Maximum transaction on page
    ULONG       tip_next;           // Next TIP page number
    UCHAR       tip_transactions[1]; // Transaction state array
} TipPage;

// Page header structure
typedef struct page_header {
    UCHAR       pag_type;           // Page type (pag_transactions)
    UCHAR       pag_flags;          // Page flags
    USHORT      pag_checksum;       // Page checksum
    ULONG       pag_generation;     // Page generation/SCN
    ULONG       pag_sequence;       // Sequence for WAL
    ULONG       pag_pageno;         // This page number
} PageHeader;

// TIP page constants
#define TIP_CACHE_SIZE      256     // TIP cache entries
#define TRANS_PER_TIP       (BITS_PER_LONG * 4)
#define TRANS_PER_BYTE      4       // 4 transactions per byte (2 bits each)
#define TIP_SHIFT           2       // Shift for 2-bit entries
#define TIP_MASK            0x03    // Mask for 2-bit state

// Calculate TIP page for transaction
#define TIP_PAGE_NUMBER(trans)     ((trans) / TRANS_PER_TIP)
#define TIP_PAGE_OFFSET(trans)     (((trans) % TRANS_PER_TIP) / TRANS_PER_BYTE)
#define TIP_BYTE_SHIFT(trans)      ((((trans) % TRANS_PER_BYTE) * 2))
```

### TIP State Management

```c
// Get transaction state from TIP
UCHAR TIP_get_state(Database* dbb, TraNumber trans) {
    // Check cache first
    TipCache* cache = dbb->dbb_tip_cache;
    UCHAR state = check_tip_cache(cache, trans);
    if (state != tra_unknown) {
        return state;
    }
    
    // Calculate TIP page location
    ULONG tip_page = TIP_PAGE_NUMBER(trans);
    ULONG tip_offset = TIP_PAGE_OFFSET(trans);
    UCHAR tip_shift = TIP_BYTE_SHIFT(trans);
    
    // Fetch TIP page
    WIN window;
    TipPage* page = (TipPage*) CCH_FETCH(dbb, &window, LCK_read);
    
    // Extract 2-bit state
    UCHAR byte = page->tip_transactions[tip_offset];
    state = (byte >> tip_shift) & TIP_MASK;
    
    CCH_RELEASE(dbb, &window);
    
    // Update cache
    update_tip_cache(cache, trans, state);
    
    return state;
}

// Set transaction state in TIP
void TIP_set_state(Database* dbb, TraNumber trans, UCHAR state) {
    // Calculate location
    ULONG tip_page = TIP_PAGE_NUMBER(trans);
    ULONG tip_offset = TIP_PAGE_OFFSET(trans);
    UCHAR tip_shift = TIP_BYTE_SHIFT(trans);
    
    // Fetch TIP page for write
    WIN window;
    TipPage* page = (TipPage*) CCH_FETCH(dbb, &window, LCK_write);
    
    // Update 2-bit state
    UCHAR* byte_ptr = &page->tip_transactions[tip_offset];
    UCHAR old_byte = *byte_ptr;
    UCHAR new_byte = (old_byte & ~(TIP_MASK << tip_shift)) | 
                     ((state & TIP_MASK) << tip_shift);
    *byte_ptr = new_byte;
    
    // Update page bounds
    if (trans < page->tip_min || page->tip_min == 0) {
        page->tip_min = trans;
    }
    if (trans > page->tip_max) {
        page->tip_max = trans;
    }
    
    // Mark page as modified
    CCH_MARK(dbb, &window);
    CCH_RELEASE(dbb, &window);
    
    // Update all caches
    broadcast_tip_update(dbb, trans, state);
}

// Bulk TIP update for commit/rollback
void TIP_update_batch(Database* dbb, TraNumber* trans_array, 
                      UCHAR* state_array, ULONG count) {
    // Sort by TIP page for efficiency
    sort_by_tip_page(trans_array, state_array, count);
    
    ULONG current_page = 0;
    TipPage* page = NULL;
    WIN window;
    
    for (ULONG i = 0; i < count; i++) {
        TraNumber trans = trans_array[i];
        UCHAR state = state_array[i];
        
        ULONG tip_page = TIP_PAGE_NUMBER(trans);
        
        // Fetch new page if needed
        if (tip_page != current_page) {
            if (page) {
                CCH_MARK(dbb, &window);
                CCH_RELEASE(dbb, &window);
            }
            page = (TipPage*) CCH_FETCH(dbb, &window, LCK_write);
            current_page = tip_page;
        }
        
        // Update state
        ULONG tip_offset = TIP_PAGE_OFFSET(trans);
        UCHAR tip_shift = TIP_BYTE_SHIFT(trans);
        
        UCHAR* byte_ptr = &page->tip_transactions[tip_offset];
        *byte_ptr = (*byte_ptr & ~(TIP_MASK << tip_shift)) | 
                   ((state & TIP_MASK) << tip_shift);
    }
    
    if (page) {
        CCH_MARK(dbb, &window);
        CCH_RELEASE(dbb, &window);
    }
}
```

### TIP Cache Implementation

```c
// TIP cache for fast transaction state lookup
typedef struct tip_cache {
    TraNumber   cache_base;         // Base transaction number
    TraNumber   cache_top;          // Top transaction number
    ULONG       cache_generation;   // Cache generation
    UCHAR*      cache_state;        // Cached states
    SpinLock    cache_lock;         // Cache lock
} TipCache;

// Transaction-local TIP cache
typedef struct tx_tip_cache {
    TraNumber   min_trans;          // Minimum cached transaction
    TraNumber   max_trans;          // Maximum cached transaction
    ULONG       num_pages;          // Number of cached pages
    struct {
        ULONG   page_num;          // TIP page number
        UCHAR   states[TRANS_PER_TIP/TRANS_PER_BYTE]; // States
    } pages[TIP_CACHE_SIZE];
} TxTipCache;

// Initialize transaction TIP cache
void cache_tip_state(Transaction* trans, Database* dbb) {
    TxTipCache* cache = FB_NEW TxTipCache();
    trans->tra_tip_cache = cache;
    
    // Cache relevant TIP pages
    TraNumber min_interesting = trans->tra_oldest;
    TraNumber max_interesting = trans->tra_top;
    
    ULONG min_page = TIP_PAGE_NUMBER(min_interesting);
    ULONG max_page = TIP_PAGE_NUMBER(max_interesting);
    
    cache->min_trans = min_interesting;
    cache->max_trans = max_interesting;
    cache->num_pages = 0;
    
    // Load TIP pages into cache
    for (ULONG page_num = min_page; 
         page_num <= max_page && cache->num_pages < TIP_CACHE_SIZE; 
         page_num++) {
        
        load_tip_page_to_cache(dbb, cache, page_num);
    }
}

// Load TIP page into cache
void load_tip_page_to_cache(Database* dbb, TxTipCache* cache, 
                            ULONG page_num) {
    WIN window;
    TipPage* page = (TipPage*) CCH_FETCH(dbb, &window, LCK_read);
    
    ULONG cache_idx = cache->num_pages++;
    cache->pages[cache_idx].page_num = page_num;
    
    // Copy transaction states
    ULONG bytes_to_copy = TRANS_PER_TIP / TRANS_PER_BYTE;
    memcpy(cache->pages[cache_idx].states, 
           page->tip_transactions, 
           bytes_to_copy);
    
    CCH_RELEASE(dbb, &window);
}

// Fast cached state lookup
UCHAR get_cached_state(Transaction* trans, TraNumber target) {
    TxTipCache* cache = trans->tra_tip_cache;
    
    // Check bounds
    if (target < cache->min_trans || target > cache->max_trans) {
        return tra_unknown;
    }
    
    // Find in cache
    ULONG page_num = TIP_PAGE_NUMBER(target);
    
    for (ULONG i = 0; i < cache->num_pages; i++) {
        if (cache->pages[i].page_num == page_num) {
            ULONG offset = TIP_PAGE_OFFSET(target);
            UCHAR shift = TIP_BYTE_SHIFT(target);
            UCHAR byte = cache->pages[i].states[offset];
            return (byte >> shift) & TIP_MASK;
        }
    }
    
    return tra_unknown;
}
```

### TIP Compression and Extension

```c
// Compress TIP pages to reclaim space
void compress_tip_pages(Database* dbb) {
    // Find oldest transaction still needed
    TraNumber oldest_needed = get_oldest_transaction_needed(dbb);
    
    // Calculate first TIP page to keep
    ULONG first_keep_page = TIP_PAGE_NUMBER(oldest_needed);
    
    // Release TIP pages before oldest needed
    for (ULONG page = FIRST_TIP_PAGE; page < first_keep_page; page++) {
        release_tip_page(dbb, page);
    }
    
    // Compact remaining TIP pages
    compact_tip_chain(dbb, first_keep_page);
}

// Extend TIP for new transactions
void extend_tip(Database* dbb, TraNumber trans) {
    ULONG new_tip_page = TIP_PAGE_NUMBER(trans);
    
    // Allocate new TIP page
    WIN window;
    TipPage* page = (TipPage*) CCH_FAKE(dbb, &window, new_tip_page);
    
    // Initialize page
    page->tip_header.pag_type = pag_transactions;
    page->tip_header.pag_flags = 0;
    page->tip_min = trans;
    page->tip_max = trans;
    page->tip_next = 0;
    
    // Initialize all transactions as active
    ULONG bytes = TRANS_PER_TIP / TRANS_PER_BYTE;
    memset(page->tip_transactions, 0, bytes);
    
    // Link to previous TIP page
    if (new_tip_page > FIRST_TIP_PAGE) {
        link_tip_page(dbb, new_tip_page - 1, new_tip_page);
    }
    
    CCH_MARK_MUST_WRITE(dbb, &window);
    CCH_RELEASE(dbb, &window);
}

// TIP page statistics
typedef struct tip_stats {
    ULONG   total_pages;        // Total TIP pages
    ULONG   active_pages;       // Active TIP pages
    ULONG   compressed_pages;   // Compressed pages
    TraNumber oldest_trans;     // Oldest transaction in TIP
    TraNumber newest_trans;     // Newest transaction in TIP
    ULONG   active_count;       // Active transactions
    ULONG   committed_count;    // Committed transactions
    ULONG   rolled_back_count;  // Rolled back transactions
    ULONG   limbo_count;        // Limbo transactions
} TipStats;

// Gather TIP statistics
void get_tip_statistics(Database* dbb, TipStats* stats) {
    memset(stats, 0, sizeof(TipStats));
    
    ULONG page_num = FIRST_TIP_PAGE;
    
    while (page_num) {
        WIN window;
        TipPage* page = (TipPage*) CCH_FETCH(dbb, &window, LCK_read);
        
        stats->total_pages++;
        
        if (page->tip_min > 0) {
            if (stats->oldest_trans == 0 || page->tip_min < stats->oldest_trans) {
                stats->oldest_trans = page->tip_min;
            }
            if (page->tip_max > stats->newest_trans) {
                stats->newest_trans = page->tip_max;
            }
            
            // Count transaction states
            count_tip_states(page, stats);
        }
        
        page_num = page->tip_next;
        CCH_RELEASE(dbb, &window);
    }
}
```

---

## Implementation Details

### Lock Management for MGA

```c
// Transaction lock for write consistency
typedef struct transaction_lock {
    Lock        lock_header;        // Standard lock header
    TraNumber   lock_trans_id;      // Transaction ID
    USHORT      lock_type;          // Lock type
    USHORT      lock_state;         // Current state
} TransactionLock;

// Record lock for update conflicts
typedef struct record_lock {
    Lock        lock_header;
    RecordNumber lock_record;       // Record being locked
    TraNumber   lock_owner;         // Owning transaction
    USHORT      lock_type;          // Shared/Exclusive
} RecordLock;

// Lock types
#define LCK_none        0
#define LCK_shared      1   // Shared/read lock
#define LCK_protected   2   // Protected read
#define LCK_exclusive   3   // Exclusive/write lock

// Acquire record lock for update
RecordLock* acquire_record_lock(Transaction* trans, RecordNumber rec) {
    RecordLock* lock = create_record_lock(rec);
    lock->lock_owner = trans->tra_number;
    lock->lock_type = LCK_exclusive;
    
    // Try to acquire lock
    if (!LCK_lock(lock, LCK_exclusive, LCK_WAIT)) {
        // Update conflict
        handle_update_conflict(trans, rec);
    }
    
    return lock;
}
```

### Write-Ahead Logging for MGA

```c
// WAL record for version creation
typedef struct wal_version_record {
    UCHAR       wal_type;           // WAL_VERSION
    TraNumber   wal_trans_id;       // Transaction ID
    RecordNumber wal_record;        // Record number
    RecordNumber wal_back_version;  // Back version pointer
    USHORT      wal_length;         // Data length
    UCHAR       wal_data[1];        // Record data
} WalVersionRecord;

// Log new version creation
void log_version_creation(Transaction* trans, RecordNumber new_rec,
                         RecordNumber* back_rec, Record* data) {
    WalVersionRecord* wal = allocate_wal_record(sizeof(WalVersionRecord) + 
                                               data->rec_length);
    
    wal->wal_type = WAL_VERSION;
    wal->wal_trans_id = trans->tra_number;
    wal->wal_record = new_rec;
    
    if (back_rec) {
        wal->wal_back_version = *back_rec;
    } else {
        wal->wal_back_version.rec_page = 0;
        wal->wal_back_version.rec_slot = 0;
    }
    
    wal->wal_length = data->rec_length;
    memcpy(wal->wal_data, data->rec_data, data->rec_length);
    
    write_wal_record(trans, wal);
}
```

### Performance Optimizations

```c
// Optimistic concurrency control
typedef struct occ_state {
    TraNumber   read_timestamp;     // Read phase timestamp
    TraNumber   validation_timestamp; // Validation timestamp
    RecordSet*  read_set;          // Records read
    RecordSet*  write_set;         // Records written
} OCCState;

// Validate transaction for optimistic CC
bool validate_transaction(Transaction* trans) {
    OCCState* occ = trans->tra_occ_state;
    
    // Check read set validity
    for (Record* rec = occ->read_set->first; rec; rec = rec->next) {
        // Check if record was modified after we read it
        if (was_modified_after(rec, occ->read_timestamp)) {
            return false;  // Validation failed
        }
    }
    
    // Check write set conflicts
    for (Record* rec = occ->write_set->first; rec; rec = rec->next) {
        // Check for concurrent updates
        if (has_concurrent_update(rec, occ->read_timestamp)) {
            return false;  // Write conflict
        }
    }
    
    return true;  // Validation successful
}

// Read-committed isolation optimization
typedef struct read_committed_snapshot {
    TraNumber   snapshot_number;    // Snapshot transaction number
    TipCache*   snapshot_tip;       // TIP state at snapshot
    TraNumber   snapshot_oldest;    // Oldest at snapshot
} ReadCommittedSnapshot;

// Refresh snapshot for read-committed
void refresh_read_committed_snapshot(Transaction* trans) {
    ReadCommittedSnapshot* snapshot = trans->tra_rc_snapshot;
    
    // Get new snapshot number
    snapshot->snapshot_number = get_snapshot_number(trans->tra_database);
    
    // Refresh TIP cache
    refresh_tip_cache(snapshot->snapshot_tip);
    
    // Update oldest transaction
    snapshot->snapshot_oldest = get_oldest_active(trans->tra_database);
}
```

### Memory Management

```c
// Version cache for hot records
typedef struct version_cache {
    ULONG       cache_size;         // Cache size in bytes
    ULONG       cache_used;         // Used bytes
    HashTable*  cache_records;      // Cached record versions
    LRUList*    cache_lru;          // LRU list for eviction
} VersionCache;

// Cache record version
void cache_record_version(Database* dbb, RecordNumber rec_num, 
                         Record* record, TraNumber trans_id) {
    VersionCache* cache = dbb->dbb_version_cache;
    
    // Check cache size
    if (cache->cache_used + record->rec_length > cache->cache_size) {
        // Evict LRU entries
        evict_lru_versions(cache, record->rec_length);
    }
    
    // Add to cache
    CachedVersion* cached = FB_NEW CachedVersion();
    cached->cv_record = rec_num;
    cached->cv_transaction = trans_id;
    cached->cv_data = copy_record(record);
    
    hash_insert(cache->cache_records, rec_num, cached);
    lru_add_head(cache->cache_lru, cached);
    
    cache->cache_used += record->rec_length;
}

// Retrieve from cache
Record* get_cached_version(Database* dbb, RecordNumber rec_num, 
                          TraNumber trans_id) {
    VersionCache* cache = dbb->dbb_version_cache;
    
    CachedVersion* cached = hash_lookup(cache->cache_records, rec_num);
    
    while (cached) {
        if (cached->cv_transaction <= trans_id) {
            // Move to head of LRU
            lru_move_to_head(cache->cache_lru, cached);
            return copy_record(cached->cv_data);
        }
        cached = cached->cv_next;
    }
    
    return NULL;  // Not in cache
}
```

### Monitoring and Statistics

```c
// MGA statistics
typedef struct mga_stats {
    // Transaction statistics
    SINT64  total_transactions;     // Total transactions started
    SINT64  active_transactions;    // Currently active
    SINT64  committed_transactions; // Total committed
    SINT64  rolled_back_transactions; // Total rolled back
    SINT64  limbo_transactions;     // Currently in limbo
    
    // Version statistics  
    SINT64  versions_created;       // Total versions created
    SINT64  versions_gc_cooperative; // Versions GC'd cooperatively
    SINT64  versions_gc_background; // Versions GC'd by background
    SINT64  versions_gc_sweep;      // Versions GC'd by sweep
    
    // Conflict statistics
    SINT64  update_conflicts;       // Update conflicts detected
    SINT64  deadlocks;             // Deadlocks detected
    SINT64  lock_timeouts;         // Lock timeout errors
    
    // Performance metrics
    SINT64  version_reads;          // Version chain reads
    SINT64  avg_version_length;     // Average version chain length
    SINT64  max_version_length;     // Maximum version chain length
    
    // TIP statistics
    SINT64  tip_pages;             // Number of TIP pages
    SINT64  tip_cache_hits;        // TIP cache hits
    SINT64  tip_cache_misses;      // TIP cache misses
    
    // Garbage collection
    SINT64  gc_cycles;             // GC cycles completed
    SINT64  sweep_cycles;          // Sweep cycles completed
    time_t  last_sweep;            // Last sweep time
    
} MGAStats;

// Collect MGA statistics
void collect_mga_statistics(Database* dbb, MGAStats* stats) {
    // Transaction statistics
    stats->total_transactions = dbb->dbb_next_transaction;
    stats->active_transactions = count_active_transactions(dbb);
    stats->committed_transactions = dbb->dbb_stats.committed_count;
    stats->rolled_back_transactions = dbb->dbb_stats.rollback_count;
    
    // Scan TIP for limbo transactions
    stats->limbo_transactions = count_limbo_transactions(dbb);
    
    // Version chain statistics
    calculate_version_statistics(dbb, stats);
    
    // GC statistics
    stats->versions_gc_cooperative = dbb->dbb_gc_state.gc_records_removed;
    stats->versions_gc_background = dbb->dbb_gc_bg_removed;
    stats->versions_gc_sweep = dbb->dbb_sweep.swp_records_swept;
    
    // TIP statistics
    get_tip_statistics(dbb, &stats->tip_stats);
}
```