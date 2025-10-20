#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/debug.h"
#include "scratchbird/core/logger.h"
#include "scratchbird/core/tablespace.h"
#include <cstring>
#include <algorithm>
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

        // Phase 1: Only tablespace 0 (primary file) supported
        // Future: Support custom tablespaces (Task 1.3)
        if (tablespace_id != PRIMARY_TABLESPACE_ID)
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
                            "Custom tablespaces not yet implemented (Phase 1, Task 1.3)");
            return Status::NOT_IMPLEMENTED;
        }

        // Allocate page in primary file using existing logic
        uint32_t page_id = 0;
        Status status = allocatePage(page_id, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Convert page_id to GPID (tablespace 0)
        *gpid_out = makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(page_id));

        return Status::OK;
    }

    Status PageManager::freePageGlobal(GPID gpid, ErrorContext *ctx)
    {
        // Extract components
        uint16_t tablespace_id = getTablespaceID(gpid);
        uint64_t page_number = getPageNumber(gpid);

        // Phase 1: Only tablespace 0 (primary file) supported
        if (tablespace_id != PRIMARY_TABLESPACE_ID)
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
                            "Custom tablespaces not yet implemented (Phase 1, Task 1.3)");
            return Status::NOT_IMPLEMENTED;
        }

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

    bool PageManager::isAllocatedGlobal(GPID gpid) const
    {
        // Extract components
        uint16_t tablespace_id = getTablespaceID(gpid);
        uint64_t page_number = getPageNumber(gpid);

        // Phase 1: Only tablespace 0 (primary file) supported
        if (tablespace_id != PRIMARY_TABLESPACE_ID)
        {
            // Custom tablespace not yet supported, treat as not allocated
            return false;
        }

        // Validate page_number fits in uint32_t for primary file
        if (page_number > UINT32_MAX)
        {
            return false;
        }

        // Check allocation in primary file using existing logic
        return isAllocated(static_cast<uint32_t>(page_number));
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
        // TODO PHASE 1, TASK 1.3.5: Implement tablespace-specific FSM loading
        // For now, skip FSM loading (will be implemented in Task 1.3.5)
        LOG_INFO(STORAGE, "Tablespace %u: FSM loading deferred to Task 1.3.5 (fsm_root_page=%lu)",
                tablespace_id, static_cast<unsigned long>(header->fsm_root_page));

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

} // namespace scratchbird::core
