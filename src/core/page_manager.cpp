#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/debug.h"
#include <cstring>
#include <algorithm>

namespace scratchbird {
namespace core {

PageManager::PageManager(Database* db, uint32_t page_size)
    : db_(db), page_size_(page_size), total_pages_(0), free_pages_(0), dirty_(false) {
}

PageManager::~PageManager() {
    // Flush if dirty
    if (dirty_) {
        flush();
    }
}

Status PageManager::initialize(ErrorContext* ctx) {
    // For a new database, we start with header (0), catalog (1), and FSM (2)
    total_pages_ = 3;
    free_pages_ = 0;
    
    // Calculate bitmap size needed
    // Each page needs 1 bit, round up to nearest byte
    size_t bitmap_bytes = (total_pages_ + 7) / 8;
    bitmap_.resize(bitmap_bytes, 0);
    
    // Mark first 3 pages as allocated
    set_bit(0, true);  // Header
    set_bit(1, true);  // System catalog
    set_bit(2, true);  // FSM itself
    
    // Write FSM page
    return flush(ctx);
}

Status PageManager::load(ErrorContext* ctx) {
    // Allocate buffer for FSM page
    uint8_t* buffer = new(std::nothrow) uint8_t[page_size_];
    if (!buffer) {
        SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate buffer for FSM page");
        return Status::OOM;
    }
    
    // Read FSM page
    Status status = db_->read_page(FSM_PAGE_ID, buffer, ctx);
    if (status != Status::Ok) {
        delete[] buffer;
        return status;
    }
    
    // Parse FSM page
    FSMPage* fsm = reinterpret_cast<FSMPage*>(buffer);
    
    // Validate page type
    if (fsm->header.page_type != PAGE_TYPE_FREE_SPACE_MAP) {
        delete[] buffer;
        SET_ERROR_CONTEXT(ctx, Status::PageCorrupt, "Invalid FSM page type");
        return Status::PageCorrupt;
    }
    
    // Validate FSM metadata consistency
    if (fsm->total_pages == 0 || fsm->total_pages > (1ULL << 32) / page_size_) {
        delete[] buffer;
        SET_ERROR_CONTEXT(ctx, Status::PageCorrupt, "Invalid FSM total_pages");
        return Status::PageCorrupt;
    }
    
    if (fsm->free_pages > fsm->total_pages) {
        delete[] buffer;
        SET_ERROR_CONTEXT(ctx, Status::PageCorrupt, "Invalid FSM free_pages count");
        return Status::PageCorrupt;
    }
    
    total_pages_ = fsm->total_pages;
    free_pages_ = fsm->free_pages;
    
    // Load bitmap
    size_t bitmap_bytes = (total_pages_ + 7) / 8;
    bitmap_.resize(bitmap_bytes);
    memcpy(bitmap_.data(), fsm->bitmap, bitmap_bytes);
    
    // Validate bitmap consistency - count allocated pages
    uint32_t allocated_count = 0;
    for (uint32_t i = 0; i < total_pages_; i++) {
        if (get_bit(i)) {
            allocated_count++;
        }
    }
    
    uint32_t expected_allocated = total_pages_ - free_pages_;
    if (allocated_count != expected_allocated) {
        delete[] buffer;
        SET_ERROR_CONTEXT(ctx, Status::PageCorrupt, 
            "FSM bitmap inconsistent with free_pages count");
        return Status::PageCorrupt;
    }
    
    delete[] buffer;
    dirty_ = false;
    return Status::Ok;
}

Status PageManager::allocate_page(uint32_t& page_id, ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    DEBUG_LOG_PM("allocate_page: total=" << total_pages_ << " free=" << free_pages_);
    
    // Find a free page
    uint32_t free_page = find_free_page();
    if (free_page == total_pages_) {
        // No free pages, need to extend file
        DEBUG_LOG_PM("No free pages, extending file");
        Status status = extend_file(1, ctx);
        if (status != Status::Ok) {
            return status;
        }
        free_page = find_free_page();
    }
    
    // Mark page as allocated
    set_bit(free_page, true);
    free_pages_--;
    dirty_ = true;
    
    page_id = free_page;
    DEBUG_LOG_PM("Allocated page " << page_id << ", free pages now: " << free_pages_);
    return Status::Ok;
}

Status PageManager::free_page(uint32_t page_id, ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Validate page_id
    if (page_id >= total_pages_) {
        SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, "Invalid page_id");
        return Status::InvalidArgument;
    }
    
    // Don't allow freeing system pages
    if (page_id <= 2) {
        SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, "Cannot free system pages");
        return Status::InvalidArgument;
    }
    
    // Check if already free
    if (!get_bit(page_id)) {
        SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, "Page is already free");
        return Status::InvalidArgument;
    }
    
    // Mark as free
    set_bit(page_id, false);
    free_pages_++;
    dirty_ = true;
    
    return Status::Ok;
}

bool PageManager::is_allocated(uint32_t page_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (page_id >= total_pages_) {
        return false;
    }
    
    return get_bit(page_id);
}

Status PageManager::extend_file(uint32_t num_pages, ErrorContext* ctx) {
    // Allocate buffer for new pages
    uint8_t* buffer = new(std::nothrow) uint8_t[page_size_];
    if (!buffer) {
        SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate buffer for page extension");
        return Status::OOM;
    }
    
    // Write empty pages to extend file
    for (uint32_t i = 0; i < num_pages; i++) {
        memset(buffer, 0, page_size_);
        
        // Initialize page header
        PageHeader* header = reinterpret_cast<PageHeader*>(buffer);
        header->magic = kMagicSBRD;
        header->version = 1;
        header->page_type = PAGE_TYPE_HEAP;  // Default to heap page
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
        Status status = db_->write_page(total_pages_ + i, buffer, ctx);
        if (status != Status::Ok) {
            delete[] buffer;
            return status;
        }
    }
    
    delete[] buffer;
    
    // Update bitmap
    size_t old_size = bitmap_.size();
    size_t new_total = total_pages_ + num_pages;
    size_t new_bitmap_bytes = (new_total + 7) / 8;
    
    if (new_bitmap_bytes > old_size) {
        bitmap_.resize(new_bitmap_bytes, 0);
    }
    
    // New pages are free by default
    total_pages_ = new_total;
    free_pages_ += num_pages;
    dirty_ = true;
    
    return Status::Ok;
}

Status PageManager::flush(ErrorContext* ctx) {
    if (!dirty_) {
        return Status::Ok;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Allocate buffer for FSM page
    uint8_t* buffer = new(std::nothrow) uint8_t[page_size_];
    if (!buffer) {
        SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate buffer for FSM flush");
        return Status::OOM;
    }
    
    memset(buffer, 0, page_size_);
    
    // Build FSM page
    FSMPage* fsm = reinterpret_cast<FSMPage*>(buffer);
    
    // Initialize page header
    fsm->header.magic = kMagicSBRD;
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
    fsm->next_fsm_page = 0;  // No chaining yet
    
    // Copy bitmap
    size_t bitmap_bytes = (total_pages_ + 7) / 8;
    memcpy(fsm->bitmap, bitmap_.data(), bitmap_bytes);
    
    // Update header fields
    fsm->header.free_space = page_size_ - sizeof(PageHeader) - sizeof(uint32_t) * 3 - bitmap_bytes;
    fsm->header.item_count = 1;  // One logical item (the bitmap)
    fsm->header.free_offset = sizeof(PageHeader) + sizeof(uint32_t) * 3 + bitmap_bytes;
    fsm->header.special_size = 0;
    
    // Write FSM page
    Status status = db_->write_page(FSM_PAGE_ID, buffer, ctx);
    
    delete[] buffer;
    
    if (status == Status::Ok) {
        // Sync to ensure FSM durability
        status = db_->sync(ctx);
        if (status == Status::Ok) {
            dirty_ = false;
        }
    }
    
    return status;
}

void PageManager::set_bit(uint32_t page_id, bool allocated) {
    uint32_t byte_index = page_id / 8;
    uint32_t bit_index = page_id % 8;
    
    if (allocated) {
        bitmap_[byte_index] |= (1 << bit_index);
    } else {
        bitmap_[byte_index] &= ~(1 << bit_index);
    }
}

bool PageManager::get_bit(uint32_t page_id) const {
    uint32_t byte_index = page_id / 8;
    uint32_t bit_index = page_id % 8;
    
    return (bitmap_[byte_index] & (1 << bit_index)) != 0;
}

uint32_t PageManager::find_free_page() const {
    for (uint32_t i = 0; i < total_pages_; i++) {
        if (!get_bit(i)) {
            return i;
        }
    }
    return total_pages_;  // No free page found
}

} // namespace core
} // namespace scratchbird