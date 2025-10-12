#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/debug.h"
#include "scratchbird/core/logger.h"
#include <cstring>
#include <algorithm>

namespace scratchbird::core
{

    PageManager::PageManager(Database *db, uint32_t page_size)
        : db_(db), page_size_(page_size), total_pages_(0), free_pages_(0), dirty_(false)
    {
    }

    PageManager::~PageManager()
    {
        // Flush if dirty
        if (dirty_)
        {
            ErrorContext ctx;
            flush(&ctx);
            // Can't do much if flush fails in destructor
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
        bitmap_.resize(bitmap_bytes, 0);

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
        bitmap_.resize(bitmap_bytes);
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
            memset(buffer.get(), 0, page_size_);

            // Initialize page header
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

            // Write page
            Status status = db_->write_page(total_pages_ + i, buffer.get(), ctx);
            if (status != Status::OK)
            {
                return status;
            }
        }

        // Update bitmap
        size_t old_size = bitmap_.size();
        size_t new_total = total_pages_ + num_pages;

        if (new_total > (SIZE_MAX - 7))
        {
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Database size exceeds addressable space.");
            return Status::OOM;
        }

        size_t new_bitmap_bytes = (new_total + 7) / 8;

        if (new_bitmap_bytes > old_size)
        {
            bitmap_.resize(new_bitmap_bytes, 0);
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

} // namespace scratchbird::core
