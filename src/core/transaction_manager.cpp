#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/error_context.h"
#include <chrono>
#include <cstring>
#include <new>
#include <unistd.h>

namespace scratchbird {
namespace core {

TransactionManager::TransactionManager(Database* db)
    : db_(db),
      buffer_pool_(db->buffer_pool()),
      page_manager_(db->page_manager()) {}

TransactionManager::~TransactionManager() {}

Status TransactionManager::initialize(ErrorContext* ctx) {
    // Note: This is called from load() which already holds the lock
    // Don't lock again to avoid deadlock
    
    // fprintf(stderr, "TransactionManager::initialize() called\n");
    
    // Allocate the first TIP page
    // fprintf(stderr, "About to allocate TIP page\n");
    Status status = allocate_tip_page(tip_root_page_, ctx);
    if (status != Status::Ok) {
        // fprintf(stderr, "Failed to allocate TIP page: %d\n", static_cast<int>(status));
        return status;
    }
    // fprintf(stderr, "Allocated TIP page: %u\n", tip_root_page_);
    
    // Update database header with TIP root page
    void* header_buffer;
    status = buffer_pool_->pin_page(0, &header_buffer, ctx);
    if (status != Status::Ok) {
        return status;
    }
    
    DatabaseHeader* db_header = static_cast<DatabaseHeader*>(header_buffer);
    db_header->tip_root_page = tip_root_page_;
    
    // Mark header page as dirty
    buffer_pool_->unpin_page(0, true, ctx);
    
    // Initialize special transactions
    transaction_cache_[BOOTSTRAP_XID] = TransactionState::COMMITTED;
    transaction_cache_[FROZEN_XID] = TransactionState::COMMITTED;
    
    // Write bootstrap transaction to TIP
    status = write_tip_entry(BOOTSTRAP_XID, TransactionState::COMMITTED, ctx);
    if (status != Status::Ok) {
        return status;
    }
    
    status = write_tip_entry(FROZEN_XID, TransactionState::COMMITTED, ctx);
    if (status != Status::Ok) {
        return status;
    }
    
    // Sync to ensure TIP is persisted
    return db_->sync(ctx);
}

Status TransactionManager::load(ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(mutex_);
    

    
    // Read database header to get TIP root page
    void* header_buffer;
    // fprintf(stderr, "About to pin page 0 for database header\n");
    Status status = buffer_pool_->pin_page(0, &header_buffer, ctx);
    if (status != Status::Ok) {
        SET_ERROR_CONTEXT(ctx, status, "Failed to read database header");
        return status;
    }
    // fprintf(stderr, "Successfully pinned page 0\n");
    
    DatabaseHeader* db_header = static_cast<DatabaseHeader*>(header_buffer);
    tip_root_page_ = db_header->tip_root_page;
    // fprintf(stderr, "TIP root page from header: %u\n", tip_root_page_);
    
    // Get next transaction ID from header
    next_xid_ = db_header->next_transaction_id;
    
    // Ensure XIDs start after reserved values
    if (next_xid_ <= FROZEN_XID) {
        next_xid_ = FROZEN_XID + 1;
    }
    
    // Save total_pages before unpinning
    uint32_t total_pages = db_header->total_pages;
    
    buffer_pool_->unpin_page(0, false, ctx);
    
    // If no TIP pages allocated yet, initialize
    if (tip_root_page_ == 0) {
        // fprintf(stderr, "No TIP pages allocated, calling initialize()\n");
        return initialize(ctx);
    }
    
    // Check if TIP page is within file bounds
    if (tip_root_page_ >= total_pages) {
        SET_ERROR_CONTEXT(ctx, Status::PageCorrupt, 
                         "TIP root page beyond file bounds");
        return Status::PageCorrupt;
    }
    
    // Load the TIP page
    void* page_buffer;
    status = buffer_pool_->pin_page(tip_root_page_, &page_buffer, ctx);
    if (status != Status::Ok) {
        // TIP page should exist if tip_root_page_ is non-zero
        SET_ERROR_CONTEXT(ctx, status, "Failed to load TIP page");
        return status;
    }
    
    // Validate TIP page
    TIPPageHeader* tip_header = static_cast<TIPPageHeader*>(page_buffer);
    if (tip_header->page_header.page_type != PAGE_TYPE_TRANSACTION_MAP) {
        buffer_pool_->unpin_page(tip_root_page_, false, ctx);
        SET_ERROR_CONTEXT(ctx, Status::PageCorrupt, 
                         "Invalid page type for TIP page");
        return Status::PageCorrupt;
    }
    
    // Load transaction states into cache
    TIPEntry* entries = reinterpret_cast<TIPEntry*>(
        reinterpret_cast<uint8_t*>(page_buffer) + sizeof(TIPPageHeader));
    
    for (uint32_t i = 0; i < tip_header->num_transactions; i++) {
        transaction_cache_[entries[i].xid] = 
            static_cast<TransactionState>(entries[i].state);
        
        // Track highest XID (in case it's higher than header's next_xid)
        if (entries[i].xid >= next_xid_) {
            next_xid_ = entries[i].xid + 1;
        }
    }
    
    buffer_pool_->unpin_page(tip_root_page_, false, ctx);
    
    return Status::Ok;
}

Status TransactionManager::begin_transaction(uint64_t& xid_out, ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // For single connection, only one active transaction allowed
    if (active_xid_ != 0) {
        SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, 
                         "Transaction already active");
        return Status::InvalidArgument;
    }
    
    // Allocate new XID
    uint64_t new_xid = next_xid_++;
    
    // Prevent wraparound to reserved XIDs
    if (next_xid_ <= FROZEN_XID) {
        next_xid_ = FROZEN_XID + 1;
    }
    
    // Record transaction as active
    transaction_cache_[new_xid] = TransactionState::ACTIVE;
    active_xid_ = new_xid;
    
    // Write to TIP
    Status status = write_tip_entry(new_xid, TransactionState::ACTIVE, ctx);
    if (status != Status::Ok) {
        // Rollback on failure
        transaction_cache_.erase(new_xid);
        active_xid_ = 0;
        return status;
    }
    
    // Update database header with new next_xid periodically (every 100 XIDs)
    if (next_xid_ % 100 == 0) {
        void* header_buffer;
        status = buffer_pool_->pin_page(0, &header_buffer, ctx);
        if (status == Status::Ok) {
            DatabaseHeader* db_header = static_cast<DatabaseHeader*>(header_buffer);
            db_header->next_transaction_id = next_xid_;
            buffer_pool_->unpin_page(0, true, ctx);
        }
        // Ignore errors - this is just an optimization
    }
    
    xid_out = new_xid;
    return Status::Ok;
}

Status TransactionManager::commit_transaction(uint64_t xid, ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Verify this is the active transaction
    if (xid != active_xid_) {
        SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, 
                         "Transaction not active");
        return Status::InvalidArgument;
    }
    
    // Update state
    transaction_cache_[xid] = TransactionState::COMMITTED;
    active_xid_ = 0;
    
    // Write to TIP
    Status status = write_tip_entry(xid, TransactionState::COMMITTED, ctx);
    if (status != Status::Ok) {
        // Try to rollback on failure
        transaction_cache_[xid] = TransactionState::ABORTED;
        return status;
    }
    
    // Ensure durability
    return db_->sync(ctx);
}

Status TransactionManager::rollback_transaction(uint64_t xid, ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Verify this is the active transaction
    if (xid != active_xid_) {
        SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, 
                         "Transaction not active");
        return Status::InvalidArgument;
    }
    
    // Update state
    transaction_cache_[xid] = TransactionState::ABORTED;
    active_xid_ = 0;
    
    // Write to TIP
    Status status = write_tip_entry(xid, TransactionState::ABORTED, ctx);
    if (status != Status::Ok) {
        return status;
    }
    
    // Sync to ensure rollback is recorded
    return db_->sync(ctx);
}

Status TransactionManager::get_transaction_state(uint64_t xid, 
                                                TransactionState& state_out,
                                                ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check cache first
    auto it = transaction_cache_.find(xid);
    if (it != transaction_cache_.end()) {
        state_out = it->second;
        return Status::Ok;
    }
    
    // Not in cache, check TIP pages
    TIPEntry entry;
    Status status = find_tip_entry(xid, entry, ctx);
    if (status == Status::NotFound) {
        // Transaction not found, assume it's too old and committed
        state_out = TransactionState::COMMITTED;
        transaction_cache_[xid] = TransactionState::COMMITTED;
        return Status::Ok;
    }
    
    if (status != Status::Ok) {
        return status;
    }
    
    state_out = static_cast<TransactionState>(entry.state);
    transaction_cache_[xid] = state_out;
    
    return Status::Ok;
}

bool TransactionManager::is_transaction_visible(uint64_t xid, uint64_t snapshot_xid) {
    // Simple visibility rules for single connection:
    // - Transaction sees its own changes
    // - Transaction sees all committed changes with XID < snapshot_xid
    // - Transaction does not see aborted changes
    // - Transaction does not see active changes from other transactions
    
    if (xid == snapshot_xid) {
        return true;  // See own changes
    }
    
    if (xid >= snapshot_xid) {
        return false;  // Future transaction
    }
    
    TransactionState state;
    if (get_transaction_state(xid, state, nullptr) != Status::Ok) {
        // Error getting state, assume not visible
        return false;
    }
    
    return state == TransactionState::COMMITTED;
}

Status TransactionManager::get_snapshot(Snapshot& snapshot_out, ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    snapshot_out.xmin = FROZEN_XID + 1;  // Oldest possible active XID
    snapshot_out.xmax = next_xid_;
    snapshot_out.active_xids.clear();
    
    // For single connection, only one active transaction
    if (active_xid_ != 0) {
        snapshot_out.active_xids.push_back(active_xid_);
        snapshot_out.xmin = active_xid_;
    }
    
    return Status::Ok;
}

Status TransactionManager::allocate_tip_page(uint32_t& page_id_out, ErrorContext* ctx) {
    // Allocate a new page for TIP
    // fprintf(stderr, "allocate_tip_page: calling page_manager_->allocate_page\n");
    Status status = page_manager_->allocate_page(page_id_out, ctx);
    if (status != Status::Ok) {
        // fprintf(stderr, "allocate_tip_page: allocate_page failed: %d\n", static_cast<int>(status));
        return status;
    }
    // fprintf(stderr, "allocate_tip_page: allocated page %u\n", page_id_out);
    
    // The newly allocated page needs to be written to disk first
    // Create a buffer for the new page
    uint8_t* new_page = new uint8_t[db_->page_size()];
    memset(new_page, 0, db_->page_size());
    
    // Initialize the page header
    PageHeader* ph = reinterpret_cast<PageHeader*>(new_page);
    ph->magic = kMagicSBRD;
    ph->version = 1;
    ph->page_type = PAGE_TYPE_TRANSACTION_MAP;
    ph->page_size = db_->page_size();
    ph->page_id = page_id_out;
    ph->checksum = 0;  // Will be set later
    
    // Calculate checksum before writing
    ph->checksum = calculate_page_checksum(new_page, db_->page_size());
    
    // Write the page to disk at the correct offset
    off_t offset = static_cast<off_t>(page_id_out) * db_->page_size();
    if (lseek(db_->fd(), offset, SEEK_SET) < 0 ||
        write(db_->fd(), new_page, db_->page_size()) != static_cast<ssize_t>(db_->page_size())) {
        delete[] new_page;
        page_manager_->free_page(page_id_out, ctx);
        SET_ERROR_CONTEXT(ctx, Status::IoError, "Failed to write TIP page");
        return Status::IoError;
    }
    
    // Sync to ensure page is on disk before BufferPool reads it
    fsync(db_->fd());
    
    delete[] new_page;
    
    // Now pin and initialize the page properly
    void* page_buffer;
    status = buffer_pool_->pin_page(page_id_out, &page_buffer, ctx);
    if (status != Status::Ok) {
        page_manager_->free_page(page_id_out, ctx);
        return status;
    }
    
    // Initialize TIP page header
    TIPPageHeader* tip_header = static_cast<TIPPageHeader*>(page_buffer);
    memset(tip_header, 0, sizeof(TIPPageHeader));
    
    tip_header->page_header.magic = kMagicSBRD;
    tip_header->page_header.version = 1;
    tip_header->page_header.page_type = PAGE_TYPE_TRANSACTION_MAP;
    tip_header->page_header.page_size = db_->page_size();
    tip_header->page_header.page_id = page_id_out;
    
    tip_header->min_xid = 0;
    tip_header->max_xid = 0;
    tip_header->num_transactions = 0;
    tip_header->next_tip_page = 0;
    
    // Calculate and set checksum
    tip_header->page_header.checksum = calculate_page_checksum(
        reinterpret_cast<uint8_t*>(page_buffer), db_->page_size());
    
    buffer_pool_->unpin_page(page_id_out, true, ctx);
    
    return Status::Ok;
}

Status TransactionManager::write_tip_entry(uint64_t xid, TransactionState state,
                                          ErrorContext* ctx) {
    // For simplicity, we'll append to the current TIP page
    // In production, we'd handle page overflow and chaining
    
    void* page_buffer;
    Status status = buffer_pool_->pin_page(tip_root_page_, &page_buffer, ctx);
    if (status != Status::Ok) {
        return status;
    }
    
    TIPPageHeader* tip_header = static_cast<TIPPageHeader*>(page_buffer);
    
    // Check if there's space
    if (tip_header->num_transactions >= TIP_ENTRIES_PER_PAGE) {
        buffer_pool_->unpin_page(tip_root_page_, false, ctx);
        SET_ERROR_CONTEXT(ctx, Status::PageFull, "TIP page full");
        return Status::PageFull;
    }
    
    // Find or add entry
    TIPEntry* entries = reinterpret_cast<TIPEntry*>(
        reinterpret_cast<uint8_t*>(page_buffer) + sizeof(TIPPageHeader));
    
    bool found = false;
    for (uint32_t i = 0; i < tip_header->num_transactions; i++) {
        if (entries[i].xid == xid) {
            // Update existing entry
            entries[i].state = static_cast<uint8_t>(state);
            entries[i].commit_time = (state != TransactionState::ACTIVE) ?
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count() : 0;
            found = true;
            break;
        }
    }
    
    if (!found) {
        // Add new entry
        uint32_t idx = tip_header->num_transactions++;
        entries[idx].xid = xid;
        entries[idx].state = static_cast<uint8_t>(state);
        entries[idx].flags = 0;
        entries[idx].reserved = 0;
        entries[idx].commit_time = (state != TransactionState::ACTIVE) ?
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count() : 0;
        
        // Update min/max XIDs
        if (tip_header->min_xid == 0 || xid < tip_header->min_xid) {
            tip_header->min_xid = xid;
        }
        if (xid > tip_header->max_xid) {
            tip_header->max_xid = xid;
        }
    }
    
    // Update checksum
    tip_header->page_header.checksum = calculate_page_checksum(
        reinterpret_cast<uint8_t*>(page_buffer), db_->page_size());
    
    buffer_pool_->unpin_page(tip_root_page_, true, ctx);
    
    return Status::Ok;
}

Status TransactionManager::find_tip_entry(uint64_t xid, TIPEntry& entry_out,
                                         ErrorContext* ctx) {
    // Search TIP pages for the transaction
    uint32_t current_page = tip_root_page_;
    
    while (current_page != 0) {
        void* page_buffer;
        Status status = buffer_pool_->pin_page(current_page, &page_buffer, ctx);
        if (status != Status::Ok) {
            return status;
        }
        
        TIPPageHeader* tip_header = static_cast<TIPPageHeader*>(page_buffer);
        
        // Check if XID could be in this page
        if (xid >= tip_header->min_xid && xid <= tip_header->max_xid) {
            TIPEntry* entries = reinterpret_cast<TIPEntry*>(
                reinterpret_cast<uint8_t*>(page_buffer) + sizeof(TIPPageHeader));
            
            for (uint32_t i = 0; i < tip_header->num_transactions; i++) {
                if (entries[i].xid == xid) {
                    entry_out = entries[i];
                    buffer_pool_->unpin_page(current_page, false, ctx);
                    return Status::Ok;
                }
            }
        }
        
        current_page = tip_header->next_tip_page;
        buffer_pool_->unpin_page(current_page, false, ctx);
    }
    
    return Status::NotFound;
}

Status TransactionManager::flush_transaction_state(ErrorContext* ctx) {
    // Ensure all transaction state is persisted
    return db_->sync(ctx);
}

} // namespace core
} // namespace scratchbird