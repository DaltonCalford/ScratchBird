#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/debug.h"
#include "scratchbird/core/logger.h"
#include "scratchbird/core/tablespace.h"
#include "scratchbird/core/catalog_manager.h"
#include <cstring>
#include <algorithm>
#include <chrono>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

namespace scratchbird::core
{

    PageManager::PageManager(Database *db, uint32_t page_size)
        : db_(db), page_size_(page_size), total_pages_(0), free_pages_(0), dirty_(false)
    {
    }

    PageManager::~PageManager()
    {
        // Flush if dirty
        // MEDIUM-5 FIX: Log flush errors in destructor to help diagnose data loss on shutdown
        if (dirty_)
        {
            ErrorContext ctx;
            Status status = flush(&ctx);
            if (status != Status::OK)
            {
                // Can't throw in destructor, but we can log the critical error
                // This helps diagnose data loss issues during shutdown
                LOG_ERROR(STORAGE,
                          "PageManager destructor: CRITICAL - Failed to flush FSM! Status=%d. "
                          "Free space map changes may be lost. Error: %s",
                          static_cast<int>(status),
                          ctx.message.empty() ? "Unknown error" : ctx.message.c_str());

                // If we have valid database pointer, attempt emergency sync
                // This is a last-ditch effort to minimize data loss
                if (db_ != nullptr)
                {
                    ErrorContext sync_ctx;
                    Status sync_status = db_->sync(&sync_ctx);
                    if (sync_status != Status::OK)
                    {
                        LOG_ERROR(STORAGE,
                                  "PageManager destructor: Emergency sync also failed! Status=%d",
                                  static_cast<int>(sync_status));
                    }
                }
            }
        }
    }

    auto PageManager::initialize(ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // For a new database, we start with header (0), catalog (1), and FSM (2)
        total_pages_ = 3;
        free_pages_ = 0;

        // Calculate bitmap size needed
        // Each page needs 1 bit, round up to nearest byte
        size_t bitmap_bytes = (total_pages_ + 7) / 8;
        // EXCEPTION SAFETY (ERROR-CRITICAL-2 Priority 2): Protect bitmap allocation
        try
        {
            bitmap_.resize(bitmap_bytes, 0);
        }
        catch (const std::bad_alloc &)
        {
            SET_ERROR_CONTEXT(ctx, Status::OOM,
                              "Out of memory allocating page bitmap during initialization");
            return Status::OOM;
        }

        // Mark first 3 pages as allocated
        setBit(0, true); // Header
        setBit(1, true); // System catalog
        setBit(2, true); // FSM itself

        // Write FSM page
        return flush(ctx);
    }

    auto PageManager::load(ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // Allocate buffer for FSM page
        auto buffer = std::make_unique<uint8_t[]>(page_size_);
        if (!buffer)
        {
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate buffer for FSM page");
            return Status::OOM;
        }

        // Read FSM page
        Status status = db_->read_page(FSM_PAGE_ID, buffer.get(), ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Parse FSM page
        auto *fsm = reinterpret_cast<FSMPage *>(buffer.get());

        // Validate page type
        if (fsm->header.page_type != PAGE_TYPE_FREE_SPACE_MAP)
        {
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Invalid FSM page type");
            return Status::PAGE_CORRUPT;
        }

        // Validate FSM metadata consistency
        if (fsm->total_pages == 0 || fsm->total_pages > (1ULL << 32) / page_size_)
        {
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Invalid FSM total_pages");
            return Status::PAGE_CORRUPT;
        }

        if (fsm->free_pages > fsm->total_pages)
        {
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Invalid FSM free_pages count");
            return Status::PAGE_CORRUPT;
        }

        total_pages_ = fsm->total_pages;
        free_pages_ = fsm->free_pages;

        // Load bitmap
        size_t bitmap_bytes = (total_pages_ + 7) / 8;
        // EXCEPTION SAFETY (ERROR-CRITICAL-2 Priority 2): Protect bitmap allocation in load
        try
        {
            bitmap_.resize(bitmap_bytes);
        }
        catch (const std::bad_alloc &)
        {
            SET_ERROR_CONTEXT(ctx, Status::OOM,
                              "Out of memory allocating page bitmap during FSM load");
            return Status::OOM;
        }
        memcpy(bitmap_.data(), fsm->bitmap, bitmap_bytes);

        // Update database header to ensure it matches FSM
        Status update_status = db_->update_header_total_pages(total_pages_, ctx);
        if (update_status != Status::OK)
        {
            // Log but continue - FSM is authoritative
            LOG_WARNING(STORAGE, "Failed to sync header total_pages with FSM: status=%d",
                        static_cast<int>(update_status));
        }

        // Validate bitmap consistency - count allocated pages
        uint32_t allocated_count = 0;
        for (uint32_t i = 0; i < total_pages_; i++)
        {
            if (getBit(i))
            {
                allocated_count++;
            }
        }

        uint32_t expected_allocated = total_pages_ - free_pages_;
        if (allocated_count != expected_allocated)
        {
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                              "FSM bitmap inconsistent with free_pages count");
            return Status::PAGE_CORRUPT;
        }

        dirty_ = false;
        return Status::OK;
    }

    auto PageManager::allocatePage(uint32_t &page_id, ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        DEBUG_LOG_PM("allocate_page: total=" << total_pages_ << " free=" << free_pages_);

        // Find a free page
        uint32_t free_page = findFreePage();
        if (free_page == total_pages_)
        {
            // No free pages, need to extend file
            DEBUG_LOG_PM("No free pages, extending file");
            Status status = extendFile(1, ctx);
            if (status != Status::OK)
            {
                return status;
            }
            free_page = findFreePage();
        }

        // Mark page as allocated
        setBit(free_page, true);
        free_pages_--;
        dirty_ = true;

        page_id = free_page;
        DEBUG_LOG_PM("Allocated page " << page_id << ", free pages now: " << free_pages_);

        // Eager FSM flush: Flush periodically to avoid data loss on crash
        // Flush every 100 allocations to balance safety and performance
        // Note: Counter is protected by mutex_ which is already held
        if (++alloc_counter_ >= 100)
        {
            alloc_counter_ = 0;
            Status flush_status = flush(ctx);
            if (flush_status != Status::OK)
            {
                // Log warning but don't fail the allocation
                LOG_WARN(STORAGE, "Failed to flush FSM after allocation: " << flush_status);
            }
        }

        return Status::OK;
    }

    auto PageManager::freePage(uint32_t page_id, ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Validate page_id
        if (page_id >= total_pages_)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid page_id");
            return Status::INVALID_ARGUMENT;
        }

        // Don't allow freeing system pages
        if (page_id <= 2)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Cannot free system pages");
            return Status::INVALID_ARGUMENT;
        }

        // Check if already free
        if (!getBit(page_id))
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Page is already free");
            return Status::INVALID_ARGUMENT;
        }

        // Mark as free
        setBit(page_id, false);
        free_pages_++;
        dirty_ = true;

        // Eager FSM flush: Flush periodically to avoid data loss on crash
        // Flush every 100 frees to balance safety and performance
        // Note: Counter is protected by mutex_ which is already held
        if (++free_counter_ >= 100)
        {
            free_counter_ = 0;
            Status flush_status = flush(ctx);
            if (flush_status != Status::OK)
            {
                // Log warning but don't fail the free operation
                LOG_WARN(STORAGE, "Failed to flush FSM after free: " << flush_status);
            }
        }

        return Status::OK;
    }

    auto PageManager::isAllocated(uint32_t page_id) const -> bool
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (page_id >= total_pages_)
        {
            return false;
        }

        return getBit(page_id);
    }

    auto PageManager::extendFile(uint32_t num_pages, ErrorContext *ctx) -> Status
    {
        // Allocate buffer for new pages
        auto buffer = std::make_unique<uint8_t[]>(page_size_);
        if (!buffer)
        {
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate buffer for page extension");
            return Status::OOM;
        }

        // Write empty pages to extend file
        for (uint32_t i = 0; i < num_pages; i++)
        {
            // ISSUE 3.5 FIX: Only zero the data portion, not the header
            // Initialize page header first (will be overwritten anyway)
            auto *header = reinterpret_cast<PageHeader *>(buffer.get());
            header->magic = K_MAGIC_SBRD;
            header->version = 1;
            header->page_type = PAGE_TYPE_HEAP; // Default to heap page
            header->page_size = page_size_;
            header->page_id = total_pages_ + i;
            header->flags = 0;
            memcpy(header->database_uuid, db_->uuid().bytes.data(), 16);
            header->generation = 1;
            header->free_space = page_size_ - sizeof(PageHeader);
            header->item_count = 0;
            header->free_offset = sizeof(PageHeader);
            header->special_size = 0;

            // Zero only the data portion after the header
            // This avoids wasting CPU cycles zeroing bytes that are immediately overwritten
            memset(buffer.get() + sizeof(PageHeader), 0, page_size_ - sizeof(PageHeader));

            // Write page
            Status status = db_->write_page(total_pages_ + i, buffer.get(), ctx);
            if (status != Status::OK)
            {
                return status;
            }
        }

        // Update bitmap with overflow protection
        size_t old_size = bitmap_.size();

        // Check for overflow BEFORE performing addition (Issue 1.7 fix)
        // Ensure: total_pages_ + num_pages <= SIZE_MAX
        if (num_pages > SIZE_MAX - total_pages_)
        {
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Database extension would exceed addressable space.");
            return Status::OOM;
        }

        size_t new_total = total_pages_ + num_pages;  // Safe: overflow checked above

        // Check that bitmap calculation won't overflow: new_total + 7 <= SIZE_MAX
        // Equivalently: new_total <= SIZE_MAX - 7
        if (new_total > (SIZE_MAX - 7))
        {
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Database size exceeds addressable space.");
            return Status::OOM;
        }

        size_t new_bitmap_bytes = (new_total + 7) / 8;  // Safe: overflow checked above

        // EXCEPTION SAFETY (ERROR-CRITICAL-2 Priority 2): Protect bitmap expansion
        if (new_bitmap_bytes > old_size)
        {
            try
            {
                bitmap_.resize(new_bitmap_bytes, 0);
            }
            catch (const std::bad_alloc &)
            {
                SET_ERROR_CONTEXT(ctx, Status::OOM,
                                  "Out of memory expanding page bitmap during file extension");
                return Status::OOM;
            }
        }

        // New pages are free by default
        total_pages_ = new_total;
        free_pages_ += num_pages;
        dirty_ = true;

        // Update database header with new total pages
        Status update_status = db_->update_header_total_pages(total_pages_, ctx);
        if (update_status != Status::OK)
        {
            // Log but don't fail - the pages are allocated
            LOG_WARNING(STORAGE,
                        "Failed to update header total_pages after file extension: status=%d",
                        static_cast<int>(update_status));
        }

        return Status::OK;
    }

    auto PageManager::flush(ErrorContext *ctx) -> Status
    {
        if (!dirty_)
        {
            return Status::OK;
        }

        std::lock_guard<std::mutex> lock(mutex_);

        auto buffer = std::make_unique<uint8_t[]>(page_size_);
        if (!buffer)
        {
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate buffer for FSM flush");
            return Status::OOM;
        }

        buildFsmPageBuffer(buffer.get());

        Status status = db_->write_page(FSM_PAGE_ID, buffer.get(), ctx);
        if (status != Status::OK)
        {
            return status;
        }

        status = db_->sync(ctx);
        if (status == Status::OK)
        {
            dirty_ = false;
        }

        return status;
    }

    void PageManager::buildFsmPageBuffer(uint8_t *buffer)
    {
        memset(buffer, 0, page_size_);

        // Build FSM page
        auto *fsm = reinterpret_cast<FSMPage *>(buffer);

        // Initialize page header
        fsm->header.magic = K_MAGIC_SBRD;
        fsm->header.version = 1;
        fsm->header.page_type = PAGE_TYPE_FREE_SPACE_MAP;
        fsm->header.page_size = page_size_;
        fsm->header.page_id = FSM_PAGE_ID;
        fsm->header.flags = 0;
        memcpy(fsm->header.database_uuid, db_->uuid().bytes.data(), 16);
        fsm->header.generation++;

        // FSM metadata
        fsm->total_pages = total_pages_;
        fsm->free_pages = free_pages_;
        fsm->next_fsm_page = 0; // No chaining yet

        // Copy bitmap
        size_t bitmap_bytes = (total_pages_ + 7) / 8;
        memcpy(fsm->bitmap, bitmap_.data(), bitmap_bytes);

        // Update header fields
        fsm->header.free_space =
            page_size_ - sizeof(PageHeader) - sizeof(uint32_t) * 3 - bitmap_bytes;
        fsm->header.item_count = 1; // One logical item (the bitmap)
        fsm->header.free_offset = sizeof(PageHeader) + sizeof(uint32_t) * 3 + bitmap_bytes;
        fsm->header.special_size = 0;

        // Calculate checksum for FSM page
        fsm->header.checksum = calculatePageChecksum(buffer, page_size_);
    }

    void PageManager::setBit(uint32_t page_id, bool allocated)
    {
        uint32_t byte_index = page_id / 8;
        uint32_t bit_index = page_id % 8;

        if (allocated)
        {
            bitmap_[byte_index] |= (1 << bit_index);
        }
        else
        {
            bitmap_[byte_index] &= ~(1 << bit_index);
        }
    }

    auto PageManager::getBit(uint32_t page_id) const -> bool
    {
        uint32_t byte_index = page_id / 8;
        uint32_t bit_index = page_id % 8;

        return (bitmap_[byte_index] & (1 << bit_index)) != 0;
    }

    auto PageManager::findFreePage() const -> uint32_t
    {
        for (uint32_t i = 0; i < total_pages_; i++)
        {
            if (!getBit(i))
            {
                return i;
            }
        }
        return total_pages_; // No free page found
    }

    auto PageManager::reconstructFromPages(ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        LOG_INFO(STORAGE, "FSM reconstruction: Scanning %u pages...", total_pages_);

        // Reset bitmap and counters
        free_pages_ = 0;
        for (size_t i = 0; i < bitmap_.size(); i++)
        {
            bitmap_[i] = 0;
        }

        // Mark system pages as allocated (always)
        setBit(0, true);  // Header page
        setBit(1, true);  // System catalog
        setBit(2, true);  // FSM itself

        // Scan all pages to determine actual allocation state
        auto buffer = std::make_unique<uint8_t[]>(page_size_);
        if (!buffer)
        {
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate buffer for FSM reconstruction");
            return Status::OOM;
        }

        uint32_t allocated_count = 3;  // System pages
        uint32_t empty_pages = 0;
        uint32_t corrupt_pages = 0;

        for (uint32_t page_id = 3; page_id < total_pages_; page_id++)
        {
            Status status = db_->read_page(page_id, buffer.get(), ctx);

            if (status == Status::IO_ERROR)
            {
                // Page doesn't exist yet (file not extended to this point)
                // Mark as free
                setBit(page_id, false);
                free_pages_++;
                empty_pages++;
                continue;
            }

            if (status != Status::OK)
            {
                // Read error - mark as allocated (conservative approach)
                // Better to waste space than to double-allocate
                setBit(page_id, true);
                corrupt_pages++;
                LOG_WARNING(STORAGE, "FSM reconstruction: page %u read error, marking allocated",
                            page_id);
                continue;
            }

            // Check if page is initialized (has valid header)
            auto *header = reinterpret_cast<PageHeader *>(buffer.get());

            // Page is allocated if:
            // 1. Has correct magic number
            // 2. page_id matches (prevents corruption detection)
            // 3. page_size matches (prevents format mismatch)
            if (header->magic == K_MAGIC_SBRD &&
                header->page_id == page_id &&
                header->page_size == page_size_)
            {
                // Page is initialized and allocated
                // Even if the transaction that allocated it was aborted,
                // the page contains data and should not be reused until GC
                setBit(page_id, true);
                allocated_count++;
            }
            else
            {
                // Page is uninitialized or corrupt - mark as free
                // This allows reuse of pages that were never properly initialized
                setBit(page_id, false);
                free_pages_++;
                empty_pages++;
            }
        }

        LOG_INFO(STORAGE,
                 "FSM reconstruction complete: %u allocated, %u free, %u empty, %u corrupt",
                 allocated_count, free_pages_, empty_pages, corrupt_pages);

        // Mark FSM as dirty so it gets flushed with the corrected state
        dirty_ = true;

        return Status::OK;
    }

    // ============================================================================
    // GPID-based API (Phase 1, Task 1.2.2)
    // ============================================================================

    Status PageManager::allocatePageInTablespace(uint16_t tablespace_id, GPID *gpid_out,
                                                ErrorContext *ctx)
    {
        if (gpid_out == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "gpid_out cannot be null");
            return Status::INVALID_ARGUMENT;
        }

        // Handle primary tablespace (0) separately - uses legacy allocatePage()
        if (tablespace_id == PRIMARY_TABLESPACE_ID)
        {
            uint32_t page_id = 0;
            Status status = allocatePage(page_id, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            *gpid_out = makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(page_id));
            return Status::OK;
        }

        // === PHASE 3, TASK 3.1.2: Custom Tablespace Allocation with Autoextend ===

        // Acquire extension mutex to prevent concurrent extensions
        // This mutex is held during the entire allocation attempt to ensure atomicity
        std::lock_guard<std::mutex> extend_lock(tablespace_extend_mutex_);

        // Try to allocate a page from the tablespace
        uint64_t allocated_page_number = 0;
        bool allocation_succeeded = false;

        {
            std::lock_guard<std::mutex> fsm_lock(tablespace_fsm_mutex_);
            auto it = tablespace_fsms_.find(tablespace_id);
            if (it == tablespace_fsms_.end())
            {
                SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                                 ("Tablespace " + std::to_string(tablespace_id) +
                                  " not found (not open)").c_str());
                return Status::NOT_FOUND;
            }

            TablespaceFSM &ts_fsm = it->second;

            // Check if there are free pages available
            if (ts_fsm.free_pages > 0)
            {
                // Find a free page in the bitmap
                for (uint32_t page_num = 0; page_num < ts_fsm.total_pages; page_num++)
                {
                    uint32_t byte_index = page_num / 8;
                    uint32_t bit_index = page_num % 8;

                    // Check if page is free (bit = 0)
                    if ((ts_fsm.bitmap[byte_index] & (1 << bit_index)) == 0)
                    {
                        // Mark page as allocated (set bit to 1)
                        ts_fsm.bitmap[byte_index] |= (1 << bit_index);
                        ts_fsm.free_pages--;
                        ts_fsm.dirty = true;

                        allocated_page_number = page_num;
                        allocation_succeeded = true;

                        LOG_INFO(STORAGE,
                                "Allocated page %lu in tablespace %u (free_pages=%u)",
                                static_cast<unsigned long>(allocated_page_number),
                                tablespace_id, ts_fsm.free_pages);
                        break;
                    }
                }

                if (!allocation_succeeded)
                {
                    // This should never happen if free_pages > 0
                    LOG_ERROR(STORAGE,
                             "Tablespace %u reports free_pages=%u but no free page found in bitmap!",
                             tablespace_id, ts_fsm.free_pages);
                    SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                                     ("FSM corruption in tablespace " + std::to_string(tablespace_id) +
                                      ": free_pages > 0 but no free page in bitmap").c_str());
                    return Status::PAGE_CORRUPT;
                }
            }
        }

        // If allocation succeeded, we're done
        if (allocation_succeeded)
        {
            *gpid_out = makeGPID(tablespace_id, allocated_page_number);
            return Status::OK;
        }

        // === No free pages available - try to extend tablespace ===

        LOG_INFO(STORAGE,
                "Tablespace %u has no free pages, attempting autoextend",
                tablespace_id);

        // Try to extend the tablespace
        Status extend_status = extendTablespace(tablespace_id, ctx);
        if (extend_status != Status::OK)
        {
            // Extension failed - return the error from extendTablespace
            // (could be PAGE_FULL if at MAXSIZE, INVALID_ARGUMENT if autoextend disabled, etc.)
            LOG_WARNING(STORAGE,
                       "Failed to extend tablespace %u: status=%d",
                       tablespace_id, static_cast<int>(extend_status));
            return extend_status;
        }

        LOG_INFO(STORAGE,
                "Successfully extended tablespace %u, retrying allocation",
                tablespace_id);

        // === Retry allocation after successful extension ===

        {
            std::lock_guard<std::mutex> fsm_lock(tablespace_fsm_mutex_);
            auto it = tablespace_fsms_.find(tablespace_id);
            if (it == tablespace_fsms_.end())
            {
                SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                                 ("Tablespace " + std::to_string(tablespace_id) +
                                  " disappeared after extension").c_str());
                return Status::NOT_FOUND;
            }

            TablespaceFSM &ts_fsm = it->second;

            // After extension, there should definitely be free pages
            if (ts_fsm.free_pages == 0)
            {
                LOG_ERROR(STORAGE,
                         "Tablespace %u extension succeeded but free_pages still 0!",
                         tablespace_id);
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                                 ("Tablespace " + std::to_string(tablespace_id) +
                                  " extension succeeded but no free pages available").c_str());
                return Status::PAGE_CORRUPT;
            }

            // Find a free page (should succeed immediately)
            for (uint32_t page_num = 0; page_num < ts_fsm.total_pages; page_num++)
            {
                uint32_t byte_index = page_num / 8;
                uint32_t bit_index = page_num % 8;

                // Check if page is free (bit = 0)
                if ((ts_fsm.bitmap[byte_index] & (1 << bit_index)) == 0)
                {
                    // Mark page as allocated (set bit to 1)
                    ts_fsm.bitmap[byte_index] |= (1 << bit_index);
                    ts_fsm.free_pages--;
                    ts_fsm.dirty = true;

                    allocated_page_number = page_num;
                    allocation_succeeded = true;

                    LOG_INFO(STORAGE,
                            "Allocated page %lu in tablespace %u after extension (free_pages=%u)",
                            static_cast<unsigned long>(allocated_page_number),
                            tablespace_id, ts_fsm.free_pages);
                    break;
                }
            }
        }

        if (!allocation_succeeded)
        {
            LOG_ERROR(STORAGE,
                     "Tablespace %u extension succeeded but still cannot allocate page!",
                     tablespace_id);
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                             ("Tablespace " + std::to_string(tablespace_id) +
                              " extension succeeded but allocation still failed").c_str());
            return Status::PAGE_CORRUPT;
        }

        *gpid_out = makeGPID(tablespace_id, allocated_page_number);
        return Status::OK;
    }

    Status PageManager::freePageGlobal(GPID gpid, ErrorContext *ctx)
    {
        // Extract components
        uint16_t tablespace_id = getTablespaceID(gpid);
        uint64_t page_number = getPageNumber(gpid);

        // Handle primary tablespace (0) separately
        if (tablespace_id == PRIMARY_TABLESPACE_ID)
        {
            // Validate page_number fits in uint32_t for primary file
            if (page_number > UINT32_MAX)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                "Page number exceeds uint32_t range for primary file");
                return Status::INVALID_ARGUMENT;
            }

            // Free page in primary file using existing logic
            return freePage(static_cast<uint32_t>(page_number), ctx);
        }

        // === Custom tablespace freeing ===

        std::lock_guard<std::mutex> lock(tablespace_fsm_mutex_);

        // Find the tablespace FSM
        auto it = tablespace_fsms_.find(tablespace_id);
        if (it == tablespace_fsms_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                             ("Tablespace " + std::to_string(tablespace_id) +
                              " not found (not open)").c_str());
            return Status::NOT_FOUND;
        }

        TablespaceFSM &ts_fsm = it->second;

        // Validate page number is within tablespace bounds
        if (page_number >= ts_fsm.total_pages)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                             ("Page number " + std::to_string(page_number) +
                              " exceeds tablespace " + std::to_string(tablespace_id) +
                              " bounds (total_pages=" + std::to_string(ts_fsm.total_pages) + ")").c_str());
            return Status::INVALID_ARGUMENT;
        }

        // Check if page 0 or 1 (reserved for metadata)
        if (page_number <= 1)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                             "Cannot free reserved pages 0-1 in tablespace");
            return Status::INVALID_ARGUMENT;
        }

        // Calculate bitmap position
        uint32_t page_num = static_cast<uint32_t>(page_number);
        uint32_t byte_index = page_num / 8;
        uint32_t bit_index = page_num % 8;

        // Check if page is already free
        if ((ts_fsm.bitmap[byte_index] & (1 << bit_index)) == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                             ("Page " + std::to_string(page_number) +
                              " in tablespace " + std::to_string(tablespace_id) +
                              " is already free").c_str());
            return Status::INVALID_ARGUMENT;
        }

        // Mark page as free (clear bit)
        ts_fsm.bitmap[byte_index] &= ~(1 << bit_index);
        ts_fsm.free_pages++;
        ts_fsm.dirty = true;

        LOG_INFO(STORAGE,
                "Freed page %lu in tablespace %u (free_pages=%u)",
                static_cast<unsigned long>(page_number),
                tablespace_id, ts_fsm.free_pages);

        // Eager FSM flush: Flush periodically to avoid data loss on crash
        // Reuse the free_counter_ from primary FSM for custom tablespaces too
        if (++free_counter_ >= 100)
        {
            free_counter_ = 0;
            // Note: Flushing custom tablespace FSM would require a new method
            // For now, just mark as dirty and it will be flushed on close
            LOG_DEBUG(STORAGE, "Custom tablespace FSM flush deferred (flush on close)");
        }

        return Status::OK;
    }

    bool PageManager::isAllocatedGlobal(GPID gpid) const
    {
        // Extract components
        uint16_t tablespace_id = getTablespaceID(gpid);
        uint64_t page_number = getPageNumber(gpid);

        // Handle primary tablespace (0) separately
        if (tablespace_id == PRIMARY_TABLESPACE_ID)
        {
            // Validate page_number fits in uint32_t for primary file
            if (page_number > UINT32_MAX)
            {
                return false;
            }

            // Check allocation in primary file using existing logic
            return isAllocated(static_cast<uint32_t>(page_number));
        }

        // === Custom tablespace check ===

        std::lock_guard<std::mutex> lock(tablespace_fsm_mutex_);

        // Find the tablespace FSM
        auto it = tablespace_fsms_.find(tablespace_id);
        if (it == tablespace_fsms_.end())
        {
            // Tablespace not found/open, treat as not allocated
            return false;
        }

        const TablespaceFSM &ts_fsm = it->second;

        // Check if page number is within bounds
        if (page_number >= ts_fsm.total_pages)
        {
            return false;
        }

        // Check bitmap
        uint32_t page_num = static_cast<uint32_t>(page_number);
        uint32_t byte_index = page_num / 8;
        uint32_t bit_index = page_num % 8;

        // Allocated if bit is set (1)
        return (ts_fsm.bitmap[byte_index] & (1 << bit_index)) != 0;
    }

    // ========================================================================
    // PHASE 1, TASK 1.3.1: Tablespace File Management - Create Tablespace
    // ========================================================================

    Status PageManager::createTablespace(uint16_t tablespace_id, const std::string &name,
                                        const std::string &path, const TablespaceConfig &config,
                                        ErrorContext *ctx)
    {
        // Step 1: Validate inputs
        if (tablespace_id == PRIMARY_TABLESPACE_ID)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                             "Cannot create tablespace 0 (reserved for primary database file)");
            return Status::INVALID_ARGUMENT;
        }

        if (tablespace_id == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                             "Invalid tablespace ID: 0 is reserved");
            return Status::INVALID_ARGUMENT;
        }

        if (name.empty())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                             "Tablespace name cannot be empty");
            return Status::INVALID_ARGUMENT;
        }

        if (name.length() >= 32)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                             ("Tablespace name too long: max 31 chars, got " +
                              std::to_string(name.length())).c_str());
            return Status::INVALID_ARGUMENT;
        }

        if (path.empty())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                             "Tablespace path cannot be empty");
            return Status::INVALID_ARGUMENT;
        }

        // Step 2: Check if file already exists
        struct stat st;
        if (stat(path.c_str(), &st) == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::FILE_EXISTS,
                             ("Tablespace file already exists: " + path).c_str());
            return Status::FILE_EXISTS;
        }

        // Step 3: Create .sbts file with exclusive create
        int fd = ::open(path.c_str(), O_RDWR | O_CREAT | O_EXCL, 0644);
        if (fd < 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR,
                             ("Failed to create tablespace file: " + path + " (errno=" +
                              std::to_string(errno) + ", " + std::string(strerror(errno)) + ")").c_str());
            return Status::IO_ERROR;
        }

        // Step 4: Initialize TablespaceHeader (page 0)
        auto header_buffer = std::make_unique<uint8_t[]>(page_size_);
        memset(header_buffer.get(), 0, page_size_);
        auto *header = reinterpret_cast<TablespaceHeader *>(header_buffer.get());

        // Initialize PageHeader portion
        header->page_header.magic = K_MAGIC_SBRD;
        header->page_header.version = 1;
        header->page_header.page_type = PAGE_TYPE_DATABASE_HEADER; // Tablespace header uses same type
        header->page_header.page_size = page_size_;
        header->page_header.page_id = 0;
        header->page_header.flags = 0;
        header->page_header.lsn = 0;
        const ID &db_uuid = db_->uuid();
        memcpy(header->page_header.database_uuid, db_uuid.bytes.data(), 16);
        header->page_header.generation = 1;
        header->page_header.free_space = 0; // Header page has no free space
        header->page_header.item_count = 0;
        header->page_header.free_offset = 0;
        header->page_header.special_size = 0;

        // Initialize TablespaceHeader fields
        strncpy(header->tablespace_name, name.c_str(), sizeof(header->tablespace_name) - 1);
        header->tablespace_name[sizeof(header->tablespace_name) - 1] = '\0'; // Ensure null termination

        header->tablespace_uuid = generateUuidV7();
        header->database_uuid = db_uuid;

        header->tablespace_id = tablespace_id;
        header->page_size = page_size_;

        // Get current time in microseconds
        auto now = std::chrono::system_clock::now();
        auto micros = std::chrono::duration_cast<std::chrono::microseconds>(
            now.time_since_epoch()).count();
        header->creation_time = static_cast<uint64_t>(micros);
        header->last_checkpoint = static_cast<uint64_t>(micros);

        header->autoextend_enabled = config.autoextend_enabled ? 1 : 0;
        header->autoextend_size_mb = config.autoextend_size_mb;
        header->max_size_mb = config.max_size_mb;

        // Calculate initial pages: header (0) + FSM (1) + preallocated pages
        uint64_t initial_total_pages = 2 + config.prealloc_pages;
        header->total_pages = initial_total_pages;
        header->free_pages = config.prealloc_pages; // FSM and header are allocated
        header->next_page_number = 2; // Next page after header and FSM
        header->fsm_root_page = 1; // FSM is always page 1

        // Initialize transaction info (will be synced with database during checkpoint)
        // For new tablespace, start with 0 - these will be updated by transaction manager
        header->oldest_transaction_id = 0;
        header->latest_completed_xid = 0;

        // Step 5: Write TablespaceHeader to page 0
        ssize_t bytes_written = ::pwrite(fd, header_buffer.get(), page_size_, 0);
        if (bytes_written != static_cast<ssize_t>(page_size_))
        {
            ::close(fd);
            ::unlink(path.c_str()); // Clean up partial file
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR,
                             ("Failed to write tablespace header (wrote " +
                              std::to_string(bytes_written) + " of " +
                              std::to_string(page_size_) + " bytes)").c_str());
            return Status::IO_ERROR;
        }

        // Step 6: Initialize tablespace FSM (page 1)
        auto fsm_buffer = std::make_unique<uint8_t[]>(page_size_);
        memset(fsm_buffer.get(), 0, page_size_);
        auto *fsm_header = reinterpret_cast<PageHeader *>(fsm_buffer.get());

        fsm_header->magic = K_MAGIC_SBRD;
        fsm_header->version = 1;
        fsm_header->page_type = PAGE_TYPE_FREE_SPACE_MAP;
        fsm_header->page_size = page_size_;
        fsm_header->page_id = 1;
        fsm_header->flags = 0;
        fsm_header->lsn = 0;
        memcpy(fsm_header->database_uuid, db_uuid.bytes.data(), 16);
        fsm_header->generation = 1;

        // Initialize FSM data structure
        struct FSMData
        {
            uint32_t total_pages;
            uint32_t free_pages;
            uint32_t next_fsm_page;
            uint8_t bitmap[1]; // First byte of bitmap
        };
        auto *fsm_data = reinterpret_cast<FSMData *>(fsm_buffer.get() + sizeof(PageHeader));

        fsm_data->total_pages = static_cast<uint32_t>(initial_total_pages);
        fsm_data->free_pages = static_cast<uint32_t>(config.prealloc_pages);
        fsm_data->next_fsm_page = 0; // No next FSM page

        // Mark pages 0 and 1 as allocated (header and FSM)
        fsm_data->bitmap[0] = 0x03; // First 2 bits set (pages 0,1 allocated)

        // If preallocating pages, mark them as free in bitmap
        // Pages 2 onwards are free (bits starting from bit 2)
        // We already set bitmap[0] = 0x03, so pages 2-7 in first byte are already 0 (free)
        // This is correct

        bytes_written = ::pwrite(fd, fsm_buffer.get(), page_size_, page_size_);
        if (bytes_written != static_cast<ssize_t>(page_size_))
        {
            ::close(fd);
            ::unlink(path.c_str()); // Clean up partial file
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR,
                             ("Failed to write tablespace FSM (wrote " +
                              std::to_string(bytes_written) + " of " +
                              std::to_string(page_size_) + " bytes)").c_str());
            return Status::IO_ERROR;
        }

        // Step 7: Sync initial pages (header + FSM) to disk before proceeding
        if (::fsync(fd) != 0)
        {
            ::close(fd);
            ::unlink(path.c_str()); // Clean up partial file
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR,
                             ("Failed to sync tablespace header/FSM to disk (errno=" +
                              std::to_string(errno) + ", " + std::string(strerror(errno)) + ")").c_str());
            return Status::IO_ERROR;
        }

        // Step 8: Register file descriptor in Database
        // (Required before calling preallocatePages which needs getTablespaceFd to work)
        Status status = db_->registerTablespaceFile(tablespace_id, fd, ctx);
        if (status != Status::OK)
        {
            ::close(fd);
            ::unlink(path.c_str()); // Clean up file
            SET_ERROR_CONTEXT(ctx, status,
                             ("Failed to register tablespace " + std::to_string(tablespace_id) +
                              " file descriptor").c_str());
            return status;
        }

        // Step 9: Create in-memory FSM for this tablespace
        // (Required before calling preallocatePages which updates the FSM)
        {
            std::lock_guard<std::mutex> lock(tablespace_fsm_mutex_);

            TablespaceFSM ts_fsm;
            ts_fsm.total_pages = 2; // Header (0) + FSM (1)
            ts_fsm.free_pages = 0;  // No free pages yet (will be added by preallocatePages)
            ts_fsm.dirty = false;

            // Initialize bitmap with pages 0 and 1 allocated
            ts_fsm.bitmap.resize(1, 0x03); // First byte: bits 0,1 set (pages 0,1 allocated)

            tablespace_fsms_[tablespace_id] = std::move(ts_fsm);

            LOG_INFO(STORAGE,
                    "Created in-memory FSM for tablespace %u: total_pages=2, free_pages=0",
                    tablespace_id);
        }

        // Step 10: Preallocate pages if requested (Phase 3 Task 3.2.2)
        // Use the optimized preallocatePages() method with fallocate support
        if (config.prealloc_pages > 0)
        {
            LOG_INFO(STORAGE, "Preallocating %u pages for tablespace %u using optimized method",
                    config.prealloc_pages, tablespace_id);

            status = preallocatePages(tablespace_id, config.prealloc_pages, ctx);
            if (status != Status::OK)
            {
                // Preallocation failed - cleanup
                db_->unregisterTablespaceFile(tablespace_id);
                ::close(fd);
                ::unlink(path.c_str()); // Clean up partial file

                // Remove in-memory FSM
                {
                    std::lock_guard<std::mutex> lock(tablespace_fsm_mutex_);
                    tablespace_fsms_.erase(tablespace_id);
                }

                SET_ERROR_CONTEXT(ctx, status,
                                 ("Failed to preallocate " + std::to_string(config.prealloc_pages) +
                                  " pages for tablespace " + std::to_string(tablespace_id)).c_str());
                return status;
            }

            LOG_INFO(STORAGE, "Successfully preallocated %u pages for tablespace %u",
                    config.prealloc_pages, tablespace_id);
        }

        LOG_INFO(STORAGE, "Successfully created tablespace %u: %s (name='%s', total_pages=%lu, free_pages=%lu, prealloc=%u)",
                tablespace_id, path.c_str(), name.c_str(),
                static_cast<unsigned long>(initial_total_pages),
                static_cast<unsigned long>(config.prealloc_pages),
                config.prealloc_pages);

        return Status::OK;
    }

    // ========================================================================
    // PHASE 1, TASK 1.3.2: Tablespace File Management - Open Tablespace
    // ========================================================================

    Status PageManager::openTablespace(uint16_t tablespace_id, const std::string &path,
                                      ErrorContext *ctx)
    {
        // Step 1: Validate inputs
        if (tablespace_id == PRIMARY_TABLESPACE_ID)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                             "Cannot open tablespace 0 (primary database file)");
            return Status::INVALID_ARGUMENT;
        }

        if (path.empty())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                             "Tablespace path cannot be empty");
            return Status::INVALID_ARGUMENT;
        }

        // Step 2: Open .sbts file
        int fd = ::open(path.c_str(), O_RDWR);
        if (fd < 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR,
                             ("Failed to open tablespace file: " + path + " (errno=" +
                              std::to_string(errno) + ", " + std::string(strerror(errno)) + ")").c_str());
            return Status::IO_ERROR;
        }

        // Step 3: Read TablespaceHeader (page 0)
        auto header_buffer = std::make_unique<uint8_t[]>(page_size_);
        ssize_t bytes_read = ::pread(fd, header_buffer.get(), page_size_, 0);
        if (bytes_read != static_cast<ssize_t>(page_size_))
        {
            ::close(fd);
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR,
                             ("Failed to read tablespace header from " + path +
                              " (read " + std::to_string(bytes_read) + " of " +
                              std::to_string(page_size_) + " bytes)").c_str());
            return Status::IO_ERROR;
        }

        auto *header = reinterpret_cast<TablespaceHeader *>(header_buffer.get());

        // Step 4: Validate PageHeader magic
        if (header->page_header.magic != K_MAGIC_SBRD)
        {
            ::close(fd);
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                             ("Invalid tablespace file: bad magic number (expected 0x" +
                              std::to_string(K_MAGIC_SBRD) + ", got 0x" +
                              std::to_string(header->page_header.magic) + ")").c_str());
            return Status::PAGE_CORRUPT;
        }

        // Step 5: Validate page_size matches
        if (header->page_size != page_size_)
        {
            ::close(fd);
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                             ("Tablespace page_size mismatch: expected " +
                              std::to_string(page_size_) + ", got " +
                              std::to_string(header->page_size) +
                              ". Cannot open tablespace with different page size.").c_str());
            return Status::INVALID_ARGUMENT;
        }

        // Step 6: Validate tablespace_id matches
        if (header->tablespace_id != tablespace_id)
        {
            ::close(fd);
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                             ("Tablespace ID mismatch: expected " +
                              std::to_string(tablespace_id) + ", got " +
                              std::to_string(header->tablespace_id)).c_str());
            return Status::INVALID_ARGUMENT;
        }

        // Step 7: Check database_uuid matches (warn if mismatch, don't fail)
        // Get database UUID from Database instance
        const ID &db_uuid = db_->uuid();
        bool uuid_mismatch = (header->database_uuid != db_uuid);

        if (uuid_mismatch)
        {
            LOG_WARNING(STORAGE,
                    "Tablespace %u (%s) has different database_uuid than current database. "
                    "This may indicate the tablespace was created for a different database. "
                    "Proceeding with caution.",
                    tablespace_id, path.c_str());
            // Don't fail - allow cross-database tablespace attachment for data migration
        }

        // Step 8: Log tablespace information
        LOG_INFO(STORAGE, "Opening tablespace %u: %s (name='%s', total_pages=%lu, free_pages=%lu)",
                tablespace_id, path.c_str(), header->tablespace_name,
                static_cast<unsigned long>(header->total_pages),
                static_cast<unsigned long>(header->free_pages));

        // Step 9: Load tablespace FSM (page 1)
        auto fsm_buffer = std::make_unique<uint8_t[]>(page_size_);
        ssize_t fsm_bytes_read = ::pread(fd, fsm_buffer.get(), page_size_, page_size_);
        if (fsm_bytes_read != static_cast<ssize_t>(page_size_))
        {
            ::close(fd);
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR,
                             ("Failed to read tablespace FSM from " + path +
                              " (read " + std::to_string(fsm_bytes_read) + " of " +
                              std::to_string(page_size_) + " bytes)").c_str());
            return Status::IO_ERROR;
        }

        // Parse FSM page
        auto *fsm_header = reinterpret_cast<PageHeader *>(fsm_buffer.get());
        if (fsm_header->page_type != PAGE_TYPE_FREE_SPACE_MAP)
        {
            ::close(fd);
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                             ("Invalid FSM page type: expected PAGE_TYPE_FREE_SPACE_MAP, got " +
                              std::to_string(fsm_header->page_type)).c_str());
            return Status::PAGE_CORRUPT;
        }

        // Extract FSM data
        struct FSMData
        {
            uint32_t total_pages;
            uint32_t free_pages;
            uint32_t next_fsm_page;
            uint8_t bitmap[1]; // Variable length
        };
        auto *fsm_data = reinterpret_cast<FSMData *>(fsm_buffer.get() + sizeof(PageHeader));

        // Create in-memory FSM for this tablespace
        TablespaceFSM ts_fsm;
        ts_fsm.total_pages = fsm_data->total_pages;
        ts_fsm.free_pages = fsm_data->free_pages;

        // Copy bitmap (calculate size needed)
        size_t bitmap_bytes = (fsm_data->total_pages + 7) / 8; // Round up to nearest byte
        ts_fsm.bitmap.resize(bitmap_bytes);
        std::memcpy(ts_fsm.bitmap.data(), fsm_data->bitmap, bitmap_bytes);
        ts_fsm.dirty = false;

        // Store FSM in map (thread-safe)
        {
            std::lock_guard<std::mutex> lock(tablespace_fsm_mutex_);
            tablespace_fsms_[tablespace_id] = std::move(ts_fsm);
        }

        LOG_INFO(STORAGE, "Loaded FSM for tablespace %u: total_pages=%u, free_pages=%u",
                tablespace_id, fsm_data->total_pages, fsm_data->free_pages);

        // Step 10: Register file descriptor in Database
        Status status = db_->registerTablespaceFile(tablespace_id, fd, ctx);
        if (status != Status::OK)
        {
            ::close(fd);
            SET_ERROR_CONTEXT(ctx, status,
                             ("Failed to register tablespace " + std::to_string(tablespace_id) +
                              " file descriptor").c_str());
            return status;
        }

        LOG_INFO(STORAGE, "Successfully opened tablespace %u from %s", tablespace_id, path.c_str());
        return Status::OK;
    }

    // ========================================================================
    // PHASE 1, TASK 1.3.3: Tablespace File Management - Close Tablespace
    // ========================================================================

    Status PageManager::closeTablespace(uint16_t tablespace_id, ErrorContext *ctx)
    {
        // Step 1: Validate inputs
        if (tablespace_id == PRIMARY_TABLESPACE_ID)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                             "Cannot close tablespace 0 (primary database file)");
            return Status::INVALID_ARGUMENT;
        }

        if (tablespace_id == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                             "Invalid tablespace ID: 0 is reserved");
            return Status::INVALID_ARGUMENT;
        }

        // Step 2: Get file descriptor before unregistering
        int fd = db_->getTablespaceFd(tablespace_id);
        if (fd < 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                             ("Tablespace " + std::to_string(tablespace_id) +
                              " not found (not open)").c_str());
            return Status::NOT_FOUND;
        }

        // Step 3: Flush dirty FSM pages for this tablespace
        bool fsm_was_dirty = false;
        {
            std::lock_guard<std::mutex> lock(tablespace_fsm_mutex_);
            auto it = tablespace_fsms_.find(tablespace_id);
            if (it != tablespace_fsms_.end() && it->second.dirty)
            {
                fsm_was_dirty = true;
                TablespaceFSM &ts_fsm = it->second;

                // Build FSM page buffer
                auto fsm_buffer = std::make_unique<uint8_t[]>(page_size_);
                memset(fsm_buffer.get(), 0, page_size_);

                // Initialize PageHeader
                auto *fsm_header = reinterpret_cast<PageHeader *>(fsm_buffer.get());
                fsm_header->magic = K_MAGIC_SBRD;
                fsm_header->version = 1;
                fsm_header->page_type = PAGE_TYPE_FREE_SPACE_MAP;
                fsm_header->page_size = page_size_;
                fsm_header->page_id = 1; // FSM is always page 1
                fsm_header->flags = 0;
                fsm_header->lsn = 0;
                const ID &db_uuid = db_->uuid();
                memcpy(fsm_header->database_uuid, db_uuid.bytes.data(), 16);
                fsm_header->generation = 1;

                // Write FSM data
                struct FSMData
                {
                    uint32_t total_pages;
                    uint32_t free_pages;
                    uint32_t next_fsm_page;
                    uint8_t bitmap[1];
                };
                auto *fsm_data = reinterpret_cast<FSMData *>(fsm_buffer.get() + sizeof(PageHeader));
                fsm_data->total_pages = ts_fsm.total_pages;
                fsm_data->free_pages = ts_fsm.free_pages;
                fsm_data->next_fsm_page = 0;

                // Copy bitmap
                size_t bitmap_bytes = ts_fsm.bitmap.size();
                std::memcpy(fsm_data->bitmap, ts_fsm.bitmap.data(), bitmap_bytes);

                // Write FSM to page 1 of tablespace
                ssize_t bytes_written = ::pwrite(fd, fsm_buffer.get(), page_size_, page_size_);
                if (bytes_written != static_cast<ssize_t>(page_size_))
                {
                    LOG_WARNING(STORAGE,
                            "Failed to flush FSM for tablespace %u (wrote %ld of %u bytes). "
                            "Continuing with close operation.",
                            tablespace_id, bytes_written, page_size_);
                }
                else
                {
                    ts_fsm.dirty = false;
                    LOG_INFO(STORAGE, "Flushed FSM for tablespace %u (total_pages=%u, free_pages=%u)",
                            tablespace_id, ts_fsm.total_pages, ts_fsm.free_pages);
                }
            }
        }

        if (!fsm_was_dirty)
        {
            LOG_INFO(STORAGE, "Tablespace %u: FSM not dirty, skipping flush", tablespace_id);
        }

        // Step 4: Sync tablespace file to disk
        if (::fsync(fd) != 0)
        {
            // Log warning but don't fail - we'll still try to close
            LOG_WARNING(STORAGE,
                    "Failed to sync tablespace %u to disk before closing (errno=%d, %s). "
                    "Continuing with close operation.",
                    tablespace_id, errno, strerror(errno));
            // Don't return error - continue with unregister
        }

        // Step 5: Unregister file descriptor from Database (this also closes it)
        Status status = db_->unregisterTablespaceFile(tablespace_id, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status,
                             ("Failed to unregister tablespace " + std::to_string(tablespace_id) +
                              " file descriptor").c_str());
            return status;
        }

        // Step 6: Remove FSM from in-memory map
        {
            std::lock_guard<std::mutex> lock(tablespace_fsm_mutex_);
            tablespace_fsms_.erase(tablespace_id);
        }

        LOG_INFO(STORAGE, "Successfully closed tablespace %u", tablespace_id);
        return Status::OK;
    }

    // ========================================================================
    // PHASE 3, TASK 3.1.1: Tablespace Auto-Extension
    // ========================================================================

    Status PageManager::extendTablespace(uint16_t tablespace_id, ErrorContext *ctx)
    {
        // Step 1: Validate inputs
        if (tablespace_id == PRIMARY_TABLESPACE_ID)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                             "Cannot extend tablespace 0 (use extendFile for primary database)");
            return Status::INVALID_ARGUMENT;
        }

        if (tablespace_id == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                             "Invalid tablespace ID: 0 is reserved");
            return Status::INVALID_ARGUMENT;
        }

        // Step 2: Get file descriptor for this tablespace
        int fd = db_->getTablespaceFd(tablespace_id);
        if (fd < 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                             ("Tablespace " + std::to_string(tablespace_id) +
                              " not found (not open)").c_str());
            return Status::NOT_FOUND;
        }

        // Step 3: Read TablespaceHeader to get autoextend config and current state
        auto header_buffer = std::make_unique<uint8_t[]>(page_size_);
        ssize_t bytes_read = ::pread(fd, header_buffer.get(), page_size_, 0);
        if (bytes_read != static_cast<ssize_t>(page_size_))
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR,
                             ("Failed to read tablespace header for tablespace " +
                              std::to_string(tablespace_id) + " (read " +
                              std::to_string(bytes_read) + " of " +
                              std::to_string(page_size_) + " bytes)").c_str());
            return Status::IO_ERROR;
        }

        auto *header = reinterpret_cast<TablespaceHeader *>(header_buffer.get());

        // Step 4: Check if autoextend is enabled
        if (!header->autoextend_enabled)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                             ("Tablespace " + std::to_string(tablespace_id) +
                              " has autoextend disabled").c_str());
            return Status::INVALID_ARGUMENT;
        }

        // Step 5: Calculate extension size from autoextend_size_mb
        // Convert MB to pages: (size_mb * 1024 * 1024) / page_size
        uint64_t autoextend_bytes = static_cast<uint64_t>(header->autoextend_size_mb) * 1024 * 1024;
        uint64_t pages_to_add = autoextend_bytes / page_size_;

        if (pages_to_add == 0)
        {
            // Ensure at least 1 page is added
            pages_to_add = 1;
            LOG_WARNING(STORAGE,
                       "Tablespace %u: autoextend_size_mb=%u too small, extending by 1 page",
                       tablespace_id, header->autoextend_size_mb);
        }

        // Step 6: Check MAXSIZE limit before extending
        uint64_t current_total_pages = header->total_pages;
        uint64_t new_total_pages = current_total_pages + pages_to_add;

        if (header->max_size_mb > 0)
        {
            // Calculate max pages from max_size_mb
            uint64_t max_bytes = static_cast<uint64_t>(header->max_size_mb) * 1024 * 1024;
            uint64_t max_pages = max_bytes / page_size_;

            if (new_total_pages > max_pages)
            {
                // Would exceed MAXSIZE - try to extend only to max
                if (current_total_pages >= max_pages)
                {
                    // Track failed extension (Phase 3 Task 3.1.5)
                    {
                        std::lock_guard<std::mutex> lock(tablespace_fsm_mutex_);
                        tablespace_metrics_[tablespace_id].failed_extension_count++;
                    }

                    SET_ERROR_CONTEXT(ctx, Status::PAGE_FULL,
                                     ("Tablespace " + std::to_string(tablespace_id) +
                                      " has reached MAXSIZE (" + std::to_string(header->max_size_mb) +
                                      " MB, " + std::to_string(max_pages) + " pages)").c_str());
                    return Status::PAGE_FULL;
                }

                // Extend only to max_pages
                pages_to_add = max_pages - current_total_pages;
                new_total_pages = max_pages;

                LOG_INFO(STORAGE,
                        "Tablespace %u: limiting extension to MAXSIZE (adding %lu pages to reach %lu pages)",
                        tablespace_id,
                        static_cast<unsigned long>(pages_to_add),
                        static_cast<unsigned long>(new_total_pages));
            }

            // Check if we're approaching MAXSIZE (90% full) after this extension
            // Phase 3 Task 3.1.5: Log WARNING when approaching MAXSIZE
            if (new_total_pages > 0 && max_pages > 0)
            {
                double usage_percent = (static_cast<double>(new_total_pages) / max_pages) * 100.0;
                if (usage_percent >= 90.0)
                {
                    uint64_t remaining_pages = max_pages - new_total_pages;
                    uint64_t remaining_mb = (remaining_pages * page_size_) / (1024 * 1024);

                    LOG_WARNING(STORAGE,
                               "Tablespace %u is approaching MAXSIZE: %.1f%% full "
                               "(%lu / %lu pages, %lu MB remaining)",
                               tablespace_id, usage_percent,
                               static_cast<unsigned long>(new_total_pages),
                               static_cast<unsigned long>(max_pages),
                               static_cast<unsigned long>(remaining_mb));
                }
            }
        }

        // Step 7: Use ftruncate() to grow the file
        off_t new_file_size = static_cast<off_t>(new_total_pages) * page_size_;
        if (ftruncate(fd, new_file_size) != 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR,
                             ("Failed to extend tablespace " + std::to_string(tablespace_id) +
                              " to " + std::to_string(new_total_pages) + " pages (errno=" +
                              std::to_string(errno) + ", " + std::string(strerror(errno)) + ")").c_str());
            return Status::IO_ERROR;
        }

        LOG_INFO(STORAGE,
                "Extended tablespace %u from %lu to %lu pages (+%lu pages, +%lu MB)",
                tablespace_id,
                static_cast<unsigned long>(current_total_pages),
                static_cast<unsigned long>(new_total_pages),
                static_cast<unsigned long>(pages_to_add),
                static_cast<unsigned long>((pages_to_add * page_size_) / (1024 * 1024)));

        // Step 8: Update in-memory FSM to reflect new pages
        {
            std::lock_guard<std::mutex> lock(tablespace_fsm_mutex_);
            auto it = tablespace_fsms_.find(tablespace_id);
            if (it == tablespace_fsms_.end())
            {
                SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                                 ("FSM not found for tablespace " + std::to_string(tablespace_id)).c_str());
                return Status::NOT_FOUND;
            }

            TablespaceFSM &ts_fsm = it->second;

            // Resize bitmap to accommodate new pages
            size_t old_bitmap_size = ts_fsm.bitmap.size();
            size_t new_bitmap_size = (new_total_pages + 7) / 8; // Round up to nearest byte

            if (new_bitmap_size > old_bitmap_size)
            {
                // Expand bitmap, new bits are 0 (free) by default
                ts_fsm.bitmap.resize(new_bitmap_size, 0);
            }

            // Update FSM counters
            ts_fsm.total_pages = static_cast<uint32_t>(new_total_pages);
            ts_fsm.free_pages += static_cast<uint32_t>(pages_to_add);
            ts_fsm.dirty = true;

            LOG_INFO(STORAGE,
                    "Updated FSM for tablespace %u: total_pages=%u, free_pages=%u",
                    tablespace_id, ts_fsm.total_pages, ts_fsm.free_pages);
        }

        // Step 9: Update TablespaceHeader.total_pages and free_pages
        header->total_pages = new_total_pages;
        header->free_pages += pages_to_add;

        // Write updated header back to page 0
        ssize_t bytes_written = ::pwrite(fd, header_buffer.get(), page_size_, 0);
        if (bytes_written != static_cast<ssize_t>(page_size_))
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR,
                             ("Failed to update tablespace header for tablespace " +
                              std::to_string(tablespace_id) + " (wrote " +
                              std::to_string(bytes_written) + " of " +
                              std::to_string(page_size_) + " bytes)").c_str());
            return Status::IO_ERROR;
        }

        LOG_INFO(STORAGE,
                "Successfully extended tablespace %u: total_pages=%lu, free_pages=%lu",
                tablespace_id,
                static_cast<unsigned long>(new_total_pages),
                static_cast<unsigned long>(header->free_pages));

        // Step 10: Update pg_tablespace statistics (Phase 3 Task 3.1.4)
        // Calculate sizes in MB
        uint64_t total_size_mb = (new_total_pages * page_size_) / (1024 * 1024);
        uint64_t free_size_mb = (header->free_pages * page_size_) / (1024 * 1024);

        // Get current time for last_extended_time
        auto now = std::chrono::system_clock::now();
        auto micros = std::chrono::duration_cast<std::chrono::microseconds>(
            now.time_since_epoch()).count();
        uint64_t last_extended_time = static_cast<uint64_t>(micros);

        // Update catalog statistics
        Status catalog_status = db_->catalog_manager()->updateTablespaceStats(
            tablespace_id, total_size_mb, free_size_mb, last_extended_time, ctx);

        if (catalog_status != Status::OK)
        {
            // Log warning but don't fail the extension - it succeeded on disk
            LOG_WARNING(STORAGE,
                       "Tablespace %u extended successfully but failed to update catalog statistics: status=%d",
                       tablespace_id, static_cast<int>(catalog_status));
            // Don't return error - the extension itself succeeded
        }

        // Step 11: Update extension metrics (Phase 3 Task 3.1.5)
        {
            // Note: Using tablespace_fsm_mutex_ to protect metrics map (shared lock)
            // already acquired above, so we're thread-safe here

            TablespaceMetrics &metrics = tablespace_metrics_[tablespace_id];

            metrics.extension_count++;
            metrics.total_pages_added += pages_to_add;
            metrics.last_extension_time = last_extended_time;

            // Set first_extension_time if this is the first extension
            if (metrics.extension_count == 1)
            {
                metrics.first_extension_time = last_extended_time;
            }

            LOG_INFO(STORAGE,
                    "Updated metrics for tablespace %u: extension_count=%lu, total_pages_added=%lu",
                    tablespace_id,
                    static_cast<unsigned long>(metrics.extension_count),
                    static_cast<unsigned long>(metrics.total_pages_added));
        }

        return Status::OK;
    }

    // === PHASE 3, TASK 3.2.1: Preallocate Pages ===

    Status PageManager::preallocatePages(uint16_t tablespace_id, uint32_t num_pages, ErrorContext *ctx)
    {
        // Step 1: Validate inputs
        if (tablespace_id == PRIMARY_TABLESPACE_ID)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                             "Cannot preallocate pages for tablespace 0 (use extendFile for primary database)");
            return Status::INVALID_ARGUMENT;
        }

        if (num_pages == 0)
        {
            // Nothing to do
            LOG_INFO(STORAGE, "Preallocation requested 0 pages for tablespace %u, skipping", tablespace_id);
            return Status::OK;
        }

        LOG_INFO(STORAGE, "Preallocating %u pages for tablespace %u", num_pages, tablespace_id);

        // Step 2: Get file descriptor for this tablespace
        int fd = db_->getTablespaceFd(tablespace_id);
        if (fd < 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                             ("Tablespace " + std::to_string(tablespace_id) +
                              " not found (not open)").c_str());
            return Status::NOT_FOUND;
        }

        // Step 3: Read TablespaceHeader to get current state
        auto header_buffer = std::make_unique<uint8_t[]>(page_size_);
        ssize_t bytes_read = ::pread(fd, header_buffer.get(), page_size_, 0);
        if (bytes_read != static_cast<ssize_t>(page_size_))
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR,
                             ("Failed to read tablespace header for tablespace " +
                              std::to_string(tablespace_id) + " (read " +
                              std::to_string(bytes_read) + " of " +
                              std::to_string(page_size_) + " bytes)").c_str());
            return Status::IO_ERROR;
        }

        auto *header = reinterpret_cast<TablespaceHeader *>(header_buffer.get());

        // Step 4: Calculate new file size
        uint64_t current_total_pages = header->total_pages;
        uint64_t new_total_pages = current_total_pages + num_pages;

        // Check MAXSIZE limit if set
        if (header->max_size_mb > 0)
        {
            uint64_t max_bytes = static_cast<uint64_t>(header->max_size_mb) * 1024 * 1024;
            uint64_t max_pages = max_bytes / page_size_;

            if (new_total_pages > max_pages)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                 ("Preallocation would exceed MAXSIZE for tablespace " +
                                  std::to_string(tablespace_id) + " (requested " +
                                  std::to_string(new_total_pages) + " pages, max " +
                                  std::to_string(max_pages) + " pages)").c_str());
                return Status::INVALID_ARGUMENT;
            }
        }

        off_t new_file_size = static_cast<off_t>(new_total_pages) * page_size_;
        off_t current_file_size = static_cast<off_t>(current_total_pages) * page_size_;

        LOG_INFO(STORAGE,
                "Preallocating tablespace %u: current_size=%lu bytes, new_size=%lu bytes, adding=%u pages",
                tablespace_id,
                static_cast<unsigned long>(current_file_size),
                static_cast<unsigned long>(new_file_size),
                num_pages);

        // Step 5: Try to use fallocate() for efficient allocation (Linux only)
        bool allocation_successful = false;

#if defined(__linux__)
        // Try posix_fallocate first (guarantees space reservation)
        int result = ::posix_fallocate(fd, current_file_size, new_file_size - current_file_size);
        if (result == 0)
        {
            allocation_successful = true;
            LOG_INFO(STORAGE,
                    "Tablespace %u: used posix_fallocate() to preallocate %u pages (%lu MB)",
                    tablespace_id, num_pages,
                    static_cast<unsigned long>((num_pages * page_size_) / (1024 * 1024)));
        }
        else
        {
            LOG_WARNING(STORAGE,
                       "Tablespace %u: posix_fallocate() failed (errno=%d), falling back to manual zeroing",
                       tablespace_id, result);
        }
#endif

        // Step 6: Fallback - manually write zeroed pages in batches
        if (!allocation_successful)
        {
            // First, extend the file with ftruncate
            if (ftruncate(fd, new_file_size) != 0)
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR,
                                 ("Failed to extend tablespace " + std::to_string(tablespace_id) +
                                  " to " + std::to_string(new_file_size) + " bytes (errno=" +
                                  std::to_string(errno) + ")").c_str());
                return Status::IO_ERROR;
            }

            // Now write zeros in 10MB batches to ensure space is actually allocated
            // (ftruncate creates sparse files on many filesystems)
            const size_t BATCH_SIZE = 10 * 1024 * 1024; // 10 MB
            auto zero_buffer = std::make_unique<uint8_t[]>(BATCH_SIZE);
            std::memset(zero_buffer.get(), 0, BATCH_SIZE);

            off_t offset = current_file_size;
            off_t remaining = new_file_size - current_file_size;
            uint32_t batches_written = 0;

            LOG_INFO(STORAGE,
                    "Tablespace %u: writing zeros in 10MB batches (total %lu MB)",
                    tablespace_id,
                    static_cast<unsigned long>(remaining / (1024 * 1024)));

            while (remaining > 0)
            {
                size_t to_write = (remaining > static_cast<off_t>(BATCH_SIZE)) ? BATCH_SIZE : static_cast<size_t>(remaining);

                ssize_t bytes_written = ::pwrite(fd, zero_buffer.get(), to_write, offset);
                if (bytes_written != static_cast<ssize_t>(to_write))
                {
                    SET_ERROR_CONTEXT(ctx, Status::IO_ERROR,
                                     ("Failed to write zeros during preallocation for tablespace " +
                                      std::to_string(tablespace_id) + " at offset " +
                                      std::to_string(offset) + " (wrote " +
                                      std::to_string(bytes_written) + " of " +
                                      std::to_string(to_write) + " bytes)").c_str());
                    return Status::IO_ERROR;
                }

                offset += to_write;
                remaining -= to_write;
                batches_written++;

                // Log progress every 100MB
                if (batches_written % 10 == 0)
                {
                    uint64_t mb_written = (batches_written * BATCH_SIZE) / (1024 * 1024);
                    LOG_INFO(STORAGE,
                            "Tablespace %u: preallocated %lu MB so far...",
                            tablespace_id, static_cast<unsigned long>(mb_written));
                }
            }

            LOG_INFO(STORAGE,
                    "Tablespace %u: manual zeroing completed, wrote %u batches",
                    tablespace_id, batches_written);
        }

        // Step 7: Update FSM bitmap to mark new pages as free
        {
            std::lock_guard<std::mutex> lock(tablespace_fsm_mutex_);

            auto ts_fsm_it = tablespace_fsms_.find(tablespace_id);
            if (ts_fsm_it == tablespace_fsms_.end())
            {
                SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                                 ("Tablespace FSM not found for tablespace " +
                                  std::to_string(tablespace_id)).c_str());
                return Status::NOT_FOUND;
            }

            TablespaceFSM &ts_fsm = ts_fsm_it->second;

            // Resize bitmap to accommodate new pages
            uint32_t old_bitmap_bytes = (ts_fsm.total_pages + 7) / 8;
            uint32_t new_bitmap_bytes = (new_total_pages + 7) / 8;

            if (new_bitmap_bytes > old_bitmap_bytes)
            {
                ts_fsm.bitmap.resize(new_bitmap_bytes, 0x00); // Initialize new bytes as free (0)
            }

            // Update FSM counters
            ts_fsm.total_pages = static_cast<uint32_t>(new_total_pages);
            ts_fsm.free_pages += num_pages;
            ts_fsm.dirty = true;

            LOG_INFO(STORAGE,
                    "Updated FSM for tablespace %u after preallocation: total_pages=%u, free_pages=%u",
                    tablespace_id, ts_fsm.total_pages, ts_fsm.free_pages);
        }

        // Step 8: Update TablespaceHeader.total_pages and free_pages
        header->total_pages = new_total_pages;
        header->free_pages += num_pages;

        // Write updated header back to page 0
        ssize_t bytes_written = ::pwrite(fd, header_buffer.get(), page_size_, 0);
        if (bytes_written != static_cast<ssize_t>(page_size_))
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR,
                             ("Failed to update tablespace header after preallocation for tablespace " +
                              std::to_string(tablespace_id) + " (wrote " +
                              std::to_string(bytes_written) + " of " +
                              std::to_string(page_size_) + " bytes)").c_str());
            return Status::IO_ERROR;
        }

        // Step 9: Sync changes to disk
        if (::fsync(fd) != 0)
        {
            LOG_WARNING(STORAGE,
                       "Failed to sync tablespace %u after preallocation (errno=%d)",
                       tablespace_id, errno);
            // Don't fail - the data is written, just not guaranteed to be on disk yet
        }

        LOG_INFO(STORAGE,
                "Successfully preallocated %u pages for tablespace %u: total_pages=%lu, free_pages=%lu",
                num_pages, tablespace_id,
                static_cast<unsigned long>(new_total_pages),
                static_cast<unsigned long>(header->free_pages));

        return Status::OK;
    }

    // === PHASE 3, TASK 3.1.5: Get Tablespace Metrics ===

    bool PageManager::getTablespaceMetrics(uint16_t tablespace_id, TablespaceMetrics *metrics_out) const
    {
        if (metrics_out == nullptr)
        {
            return false;
        }

        std::lock_guard<std::mutex> lock(tablespace_fsm_mutex_);

        auto it = tablespace_metrics_.find(tablespace_id);
        if (it == tablespace_metrics_.end())
        {
            // No metrics for this tablespace (never extended)
            return false;
        }

        // Copy metrics to output
        *metrics_out = it->second;
        return true;
    }

    // === PHASE 5, TASK 5.1.1: Page Enumeration ===

    Status PageManager::getAllocatedPages(uint16_t tablespace_id,
                                         std::vector<GPID> &pages_out,
                                         ErrorContext *ctx)
    {
        // Clear output vector
        pages_out.clear();

        // Handle primary tablespace (0) separately
        if (tablespace_id == PRIMARY_TABLESPACE_ID)
        {
            std::lock_guard<std::mutex> lock(mutex_);

            // Scan bitmap for primary tablespace
            uint32_t total = total_pages_;
            for (uint32_t page_id = 0; page_id < total; page_id++)
            {
                if (getBit(page_id))
                {
                    // Page is allocated
                    GPID gpid = makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(page_id));
                    pages_out.push_back(gpid);
                }
            }

            LOG_DEBUG(STORAGE, "Enumerated %zu allocated pages in primary tablespace",
                     pages_out.size());
            return Status::OK;
        }

        // Handle custom tablespace (1-65535)
        std::lock_guard<std::mutex> lock(tablespace_fsm_mutex_);

        // Find FSM for this tablespace
        auto fsm_it = tablespace_fsms_.find(tablespace_id);
        if (fsm_it == tablespace_fsms_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                            "Tablespace not found or not loaded");
            return Status::NOT_FOUND;
        }

        const TablespaceFSM &fsm = fsm_it->second;

        // Scan bitmap
        uint32_t total = fsm.total_pages;
        const std::vector<uint8_t> &bitmap = fsm.bitmap;

        for (uint32_t page_num = 0; page_num < total; page_num++)
        {
            // Check bit in bitmap
            uint32_t byte_index = page_num / 8;
            uint32_t bit_index = page_num % 8;

            if (byte_index >= bitmap.size())
            {
                break; // Shouldn't happen, but be safe
            }

            bool allocated = (bitmap[byte_index] & (1 << bit_index)) != 0;
            if (allocated)
            {
                // Page is allocated
                GPID gpid = makeGPID(tablespace_id, static_cast<uint64_t>(page_num));
                pages_out.push_back(gpid);
            }
        }

        LOG_DEBUG(STORAGE, "Enumerated %zu allocated pages in tablespace %u",
                 pages_out.size(), tablespace_id);

        return Status::OK;
    }

} // namespace scratchbird::core
