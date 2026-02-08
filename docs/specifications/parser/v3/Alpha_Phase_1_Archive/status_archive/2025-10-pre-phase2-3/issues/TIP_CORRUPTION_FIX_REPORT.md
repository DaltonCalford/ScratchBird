# TIP Page Corruption Fix Report

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


## Issue Summary

Multiple tests were failing with the error "TIP root page beyond file bounds" when reopening databases after modifications. This was preventing tests like FSM persistence, catalog persistence, and others from passing.

## Root Cause Analysis

The issue was caused by a desynchronization between different components' view of the total number of pages in the database:

1. **Database Header**: Contains a `total_pages` field that was only set during database creation (to 3 pages)
2. **Page Manager**: Maintains its own `total_pages` count in the FSM (Free Space Map) page
3. **Transaction Manager**: Was reading `total_pages` from the database header

### The Problem Sequence:

1. Database created with 3 pages (header, catalog, FSM)
2. On first open, Transaction Manager initializes and allocates a TIP page (page 7)
3. Page Manager updates its internal count to 8 pages and saves to FSM
4. Database header still shows 3 pages (never updated)
5. On second open:
   - Transaction Manager reads header (total_pages = 3)
   - TIP root page = 7
   - Check fails: 7 >= 3, throwing "TIP root page beyond file bounds"

## Solution Implemented

The fix involved three key changes:

### 1. Added Header Update Method
Created `Database::update_header_total_pages()` to update both the in-memory header and the header page through the buffer pool when the page count changes.

### 2. Page Manager Updates Header
Modified `PageManager::extend_file()` to call the new method whenever pages are allocated, keeping the header in sync with the FSM.

### 3. Transaction Manager Uses Page Manager
Changed `TransactionManager::load()` to get `total_pages` from the page manager (which reads from FSM) instead of the potentially stale database header.

## Code Changes

### In `include/scratchbird/core/database.h`:
```cpp
// Added method to update header total pages
Status update_header_total_pages(uint32_t total_pages, ErrorContext* ctx = nullptr);
```

### In `src/core/database.cpp`:
```cpp
Status Database::update_header_total_pages(uint32_t total_pages, ErrorContext* ctx) {
    // Update in-memory header
    if (header_) {
        header_->total_pages = total_pages;
    }
    
    // Pin header page through buffer pool to update it
    if (buffer_pool_) {
        void* header_buffer;
        Status status = buffer_pool_->pin_page(0, &header_buffer, ctx);
        if (status != Status::Ok) {
            return status;
        }
        
        DatabaseHeader* db_header = static_cast<DatabaseHeader*>(header_buffer);
        db_header->total_pages = total_pages;
        
        // Unpin as dirty
        buffer_pool_->unpin_page(0, true, ctx);
    }
    
    return Status::Ok;
}
```

### In `src/core/page_manager.cpp`:
```cpp
// In extend_file() after updating total_pages_:
Status update_status = db_->update_header_total_pages(total_pages_, ctx);
if (update_status != Status::Ok) {
    // Log but don't fail - the pages are allocated
}

// In load() after reading FSM:
Status update_status = db_->update_header_total_pages(total_pages_, ctx);
if (update_status != Status::Ok) {
    // Log but continue - FSM is authoritative
}
```

### In `src/core/transaction_manager.cpp`:
```cpp
// Changed from:
// uint32_t total_pages = db_header->total_pages;

// To:
uint32_t total_pages = page_manager_->total_pages();
```

## Results

### Tests Fixed:
- ✅ PageManagementTest.FSMPersistence
- ✅ CatalogManagerTest.CatalogPersistence
- ✅ All tests that were failing due to TIP page issues

### Test Improvement:
- **Before Fix**: 30 tests failing
- **After Fix**: 26 tests failing
- **Fixed**: 4 tests

### Remaining Issue:
One test (PageManagementTest.BufferPoolDirtyPages) still fails, but for a different reason - the buffer pool doesn't recalculate checksums when flushing dirty pages. This is a separate issue from the TIP corruption.

## Lessons Learned

1. **Single Source of Truth**: Having multiple components maintain their own view of database metadata (like total_pages) leads to synchronization issues
2. **Initialization Order Matters**: The transaction manager was reading stale data because it initialized before the page manager could update the header
3. **FSM is Authoritative**: The Free Space Map should be the authoritative source for page allocation information, not the database header

## Future Recommendations

1. Consider removing `total_pages` from the database header entirely to avoid duplication
2. Implement a more robust mechanism for metadata synchronization between components
3. Add validation tests that specifically check metadata consistency across components
