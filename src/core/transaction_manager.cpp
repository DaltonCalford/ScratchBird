#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/error_context.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <new>
#include <unistd.h>


    namespace scratchbird::core
    {

        TransactionManager::TransactionManager(Database *db)
            : db_(db), buffer_pool_(db->buffer_pool()), page_manager_(db->page_manager())
        {
        }

        TransactionManager::~TransactionManager() = default;

        auto TransactionManager::getTipEntriesPerPage() const -> uint32_t
        {
            return (db_->page_size() - sizeof(TIPPageHeader)) / sizeof(TIPEntry);
        }

        auto TransactionManager::initialize(ErrorContext *ctx) -> Status
        {
            // Note: This is called from load() which already holds the lock
            // Don't lock again to avoid deadlock

            // fprintf(stderr, "TransactionManager::initialize() called\n");

            // Allocate the first TIP page
            // fprintf(stderr, "About to allocate TIP page\n");
            Status status = allocateTipPage(tip_root_page_, ctx);
            if (status != Status::OK)
            {
                // fprintf(stderr, "Failed to allocate TIP page: %d\n", static_cast<int>(status));
                return status;
            }
            // fprintf(stderr, "Allocated TIP page: %u\n", tip_root_page_);

            // Update database header with TIP root page
            void *header_buffer;
            status = buffer_pool_->pinPage(0, &header_buffer, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *db_header = static_cast<DatabaseHeader *>(header_buffer);
            db_header->tip_root_page = tip_root_page_;

            // Mark header page as dirty
            buffer_pool_->unpinPage(0, true, ctx);

            // Initialize special transactions
            transaction_cache_[BOOTSTRAP_XID] = TransactionState::COMMITTED;
            transaction_cache_[FROZEN_XID] = TransactionState::COMMITTED;

            // Write bootstrap transaction to TIP
            status = writeTipEntry(BOOTSTRAP_XID, TransactionState::COMMITTED, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            status = writeTipEntry(FROZEN_XID, TransactionState::COMMITTED, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            // Sync to ensure TIP is persisted
            return db_->sync(ctx);
        }

        auto TransactionManager::load(ErrorContext *ctx) -> Status
        {
            std::lock_guard<std::mutex> lock(mutex_);

            // Read database header to get TIP root page
            void *header_buffer;
            Status status = buffer_pool_->pinPage(0, &header_buffer, ctx);
            if (status != Status::OK)
            {
                SET_ERROR_CONTEXT(ctx, status, "Failed to read database header");
                return status;
            }

            auto *db_header = static_cast<DatabaseHeader *>(header_buffer);
            tip_root_page_ = db_header->tip_root_page;
            next_xid_ = db_header->next_transaction_id;

            if (next_xid_ <= FROZEN_XID)
            {
                next_xid_ = FROZEN_XID + 1;
            }

            buffer_pool_->unpinPage(0, false, ctx);

            if (tip_root_page_ == 0)
            {
                return initialize(ctx);
            }

            uint32_t total_pages = page_manager_->totalPages();
            if (tip_root_page_ >= total_pages)
            {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "TIP root page beyond file bounds: tip_root_page=%u, total_pages=%u",
                         tip_root_page_, total_pages);
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, msg);
                return Status::PAGE_CORRUPT;
            }

            return loadTipPage(tip_root_page_, ctx);
        }

        auto TransactionManager::loadTipPage(uint32_t page_id, ErrorContext *ctx) -> Status
        {
            // Load the TIP page
            void *page_buffer;
            Status status = buffer_pool_->pinPage(page_id, &page_buffer, ctx);
            if (status != Status::OK)
            {
                // TIP page should exist if tip_root_page_ is non-zero
                SET_ERROR_CONTEXT(ctx, status, "Failed to load TIP page");
                return status;
            }

            // Validate TIP page
            auto *tip_header = static_cast<TIPPageHeader *>(page_buffer);
            if (tip_header->page_header.page_type != PAGE_TYPE_TRANSACTION_MAP)
            {
                buffer_pool_->unpinPage(page_id, false, ctx);
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Invalid page type for TIP page");
                return Status::PAGE_CORRUPT;
            }

            // Load transaction states into cache
            auto *entries = reinterpret_cast<TIPEntry *>(
                reinterpret_cast<uint8_t *>(page_buffer) + sizeof(TIPPageHeader));

            for (uint32_t i = 0; i < tip_header->num_transactions; i++)
            {
                transaction_cache_[entries[i].xid] =
                    static_cast<TransactionState>(entries[i].state);

                // Track highest XID (in case it's higher than header's next_xid)
                if (entries[i].xid >= next_xid_)
                {
                    next_xid_ = entries[i].xid + 1;
                }
            }

            buffer_pool_->unpinPage(page_id, false, ctx);

            return Status::OK;
        }

        auto TransactionManager::beginTransaction(uint32_t proc_id, uint64_t &xid_out, ErrorContext *ctx) -> Status
        {
            std::lock_guard<std::mutex> lock(mutex_);

            // No longer check for active_xid_ - allow multiple active transactions

            // Allocate new XID
            uint64_t new_xid = next_xid_++;

            // Prevent wraparound to reserved XIDs
            if (next_xid_ <= FROZEN_XID)
            {
                next_xid_ = FROZEN_XID + 1;
            }

            // Record transaction as active
            transaction_cache_[new_xid] = TransactionState::ACTIVE;

            // Register in ProcArray
            Status status = ProcArrayManager::setTransactionId(proc_id, new_xid, ctx);
            if (status != Status::OK)
            {
                // Rollback on failure
                transaction_cache_.erase(new_xid);
                return status;
            }

            // Write to TIP
            status = writeTipEntry(new_xid, TransactionState::ACTIVE, ctx);
            if (status != Status::OK)
            {
                // Rollback on failure
                transaction_cache_.erase(new_xid);
                ProcArrayManager::clearTransactionId(proc_id, ctx);
                return status;
            }

            // Update database header with new next_xid periodically (every 100 XIDs)
            if (next_xid_ % 100 == 0)
            {
                void *header_buffer;
                status = buffer_pool_->pinPage(0, &header_buffer, ctx);
                if (status == Status::OK)
                {
                    auto *db_header = static_cast<DatabaseHeader *>(header_buffer);
                    db_header->next_transaction_id = next_xid_;
                    buffer_pool_->unpinPage(0, true, ctx);
                }
                // Ignore errors - this is just an optimization
            }

            xid_out = new_xid;
            return Status::OK;
        }

        auto TransactionManager::commitTransaction(uint32_t proc_id, uint64_t xid, ErrorContext *ctx) -> Status
        {
            std::lock_guard<std::mutex> lock(mutex_);

            // No longer verify against single active_xid_

            // Update state
            transaction_cache_[xid] = TransactionState::COMMITTED;

            // Clear from ProcArray
            Status status = ProcArrayManager::clearTransactionId(proc_id, ctx);
            if (status != Status::OK)
            {
                // Continue anyway - state update is more critical
            }

            // Write to TIP
            status = writeTipEntry(xid, TransactionState::COMMITTED, ctx);
            if (status != Status::OK)
            {
                // Try to rollback on failure
                transaction_cache_[xid] = TransactionState::ABORTED;
                return status;
            }

            // Ensure durability
            return db_->sync(ctx);
        }

        auto TransactionManager::rollbackTransaction(uint32_t proc_id, uint64_t xid, ErrorContext *ctx) -> Status
        {
            std::lock_guard<std::mutex> lock(mutex_);

            // No longer verify against single active_xid_

            // Update state
            transaction_cache_[xid] = TransactionState::ABORTED;

            // Clear from ProcArray
            Status status = ProcArrayManager::clearTransactionId(proc_id, ctx);
            if (status != Status::OK)
            {
                // Continue anyway - state update is more critical
            }

            // Write to TIP
            status = writeTipEntry(xid, TransactionState::ABORTED, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            // Sync to ensure rollback is recorded
            return db_->sync(ctx);
        }

        auto TransactionManager::getTransactionState(uint64_t xid, TransactionState &state_out,
                                                         ErrorContext *ctx) -> Status
        {
            std::lock_guard<std::mutex> lock(mutex_);

            // Check cache first
            auto it = transaction_cache_.find(xid);
            if (it != transaction_cache_.end())
            {
                state_out = it->second;
                return Status::OK;
            }

            // Not in cache, check TIP pages
            TIPEntry entry;
            Status status = findTipEntry(xid, entry, ctx);
            if (status == Status::NOT_FOUND)
            {
                // Transaction not found, assume it's too old and committed
                state_out = TransactionState::COMMITTED;
                transaction_cache_[xid] = TransactionState::COMMITTED;
                return Status::OK;
            }

            if (status != Status::OK)
            {
                return status;
            }

            state_out = static_cast<TransactionState>(entry.state);
            transaction_cache_[xid] = state_out;

            return Status::OK;
        }

        auto TransactionManager::isTransactionVisible(uint64_t xid, uint64_t snapshot_xid) -> bool
        {
            // Simple visibility rules for single connection:
            // - Transaction sees its own changes
            // - Transaction sees all committed changes with XID < snapshot_xid
            // - Transaction does not see aborted changes
            // - Transaction does not see active changes from other transactions

            if (xid == snapshot_xid)
            {
                return true; // See own changes
            }

            if (xid > snapshot_xid)
            {
                return false; // Future transaction
            }

            // Frozen tuples are always visible
            if (xid <= FROZEN_XID)
            {
                return true;
            }

            TransactionState state;
            if (getTransactionState(xid, state, nullptr) != Status::OK)
            {
                // Error getting state, for old transactions assume committed
                if (xid < snapshot_xid)
                {
                    return true; // Old transaction, assume committed
                }
                return false;
            }

            return state == TransactionState::COMMITTED;
        }

        auto TransactionManager::getBackendXid(uint32_t proc_id) const -> uint64_t
        {
            // Get XID from ProcArray
            ProcessControlBlock* pcb = ProcArrayManager::getInstance() ?
                reinterpret_cast<ProcessControlBlock*>(
                    reinterpret_cast<uint8_t*>(ProcArrayManager::getInstance()) +
                    sizeof(ProcArray)) + proc_id : nullptr;

            if (!pcb || !pcb->is_active) {
                return 0;
            }

            return pcb->xid;
        }

        auto TransactionManager::getSnapshot(Snapshot &snapshot_out, ErrorContext *ctx) -> Status
        {
            std::lock_guard<std::mutex> lock(mutex_);

            snapshot_out.xmax = next_xid_;
            snapshot_out.active_xids.clear();

            // Get active transactions from ProcArray
            uint64_t oldest_xmin = 0;
            Status status = ProcArrayManager::getActiveTransactions(&snapshot_out.active_xids,
                                                                     &oldest_xmin, ctx);
            if (status != Status::OK)
            {
                // Fallback to simple snapshot if ProcArray not available
                snapshot_out.xmin = FROZEN_XID + 1;
                return Status::OK;
            }

            snapshot_out.xmin = (oldest_xmin != 0) ? oldest_xmin : FROZEN_XID + 1;

            return Status::OK;
        }

        auto TransactionManager::allocateTipPage(uint32_t &page_id_out, ErrorContext *ctx) -> Status
        {
            // Allocate a new page for TIP
            Status status = page_manager_->allocatePage(page_id_out, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            // The newly allocated page needs to be written to disk first
            // Create a buffer for the new page
            auto new_page = std::make_unique<uint8_t[]>(db_->page_size());
            if (!new_page)
            {
                page_manager_->freePage(page_id_out, ctx);
                SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate buffer for TIP page");
                return Status::OOM;
            }
            memset(new_page.get(), 0, db_->page_size());

            // Initialize the page header
            auto *ph = reinterpret_cast<PageHeader *>(new_page.get());
            ph->magic = K_MAGIC_SBRD;
            ph->version = 1;
            ph->page_type = PAGE_TYPE_TRANSACTION_MAP;
            ph->page_size = db_->page_size();
            ph->page_id = page_id_out;
            ph->checksum = 0; // Will be set later

            // Calculate checksum before writing
            ph->checksum = calculatePageChecksum(new_page.get(), db_->page_size());

            // Write the page to disk at the correct offset
            off_t offset = static_cast<off_t>(page_id_out) * db_->page_size();
            if (lseek(db_->fd(), offset, SEEK_SET) < 0 ||
                write(db_->fd(), new_page.get(), db_->page_size()) !=
                    static_cast<ssize_t>(db_->page_size()))
            {
                page_manager_->freePage(page_id_out, ctx);
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to write TIP page");
                return Status::IO_ERROR;
            }

            // Sync to ensure page is on disk before BufferPool reads it
            fsync(db_->fd());

            // Flush page manager to ensure FSM is updated with new total_pages
            status = page_manager_->flush(ctx);
            if (status != Status::OK)
            {
                // Log but don't fail - the page is allocated
                // fprintf(stderr, "Warning: Failed to flush page manager after TIP allocation\n");
            }

            // Now pin and initialize the page properly
            void *page_buffer;
            status = buffer_pool_->pinPage(page_id_out, &page_buffer, ctx);
            if (status != Status::OK)
            {
                page_manager_->freePage(page_id_out, ctx);
                return status;
            }

            // Initialize TIP page header
            auto *tip_header = static_cast<TIPPageHeader *>(page_buffer);
            memset(tip_header, 0, sizeof(TIPPageHeader));

            tip_header->page_header.magic = K_MAGIC_SBRD;
            tip_header->page_header.version = 1;
            tip_header->page_header.page_type = PAGE_TYPE_TRANSACTION_MAP;
            tip_header->page_header.page_size = db_->page_size();
            tip_header->page_header.page_id = page_id_out;

            tip_header->min_xid = 0;
            tip_header->max_xid = 0;
            tip_header->num_transactions = 0;
            tip_header->next_tip_page = 0;

            // Calculate and set checksum
            tip_header->page_header.checksum =
                calculatePageChecksum(reinterpret_cast<uint8_t *>(page_buffer), db_->page_size());

            buffer_pool_->unpinPage(page_id_out, true, ctx);

            return Status::OK;
        }

        auto TransactionManager::writeTipEntry(uint64_t xid, TransactionState state,
                                                   ErrorContext *ctx) -> Status
        {
            // For simplicity, we'll append to the current TIP page
            // In production, we'd handle page overflow and chaining

            void *page_buffer;
            Status status = buffer_pool_->pinPage(tip_root_page_, &page_buffer, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *tip_header = static_cast<TIPPageHeader *>(page_buffer);

            // Check if there's space
            if (tip_header->num_transactions >= getTipEntriesPerPage())
            {
                buffer_pool_->unpinPage(tip_root_page_, false, ctx);
                SET_ERROR_CONTEXT(ctx, Status::PAGE_FULL, "TIP page full");
                return Status::PAGE_FULL;
            }

            // Find or add entry
            auto *entries = reinterpret_cast<TIPEntry *>(
                reinterpret_cast<uint8_t *>(page_buffer) + sizeof(TIPPageHeader));

            bool found = false;
            for (uint32_t i = 0; i < tip_header->num_transactions; i++)
            {
                if (entries[i].xid == xid)
                {
                    // Update existing entry
                    entries[i].state = static_cast<uint8_t>(state);
                    entries[i].commit_time =
                        (state != TransactionState::ACTIVE)
                            ? std::chrono::duration_cast<std::chrono::microseconds>(
                                  std::chrono::system_clock::now().time_since_epoch())
                                  .count()
                            : 0;
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                // Add new entry
                uint32_t idx = tip_header->num_transactions++;
                entries[idx].xid = xid;
                entries[idx].state = static_cast<uint8_t>(state);
                entries[idx].flags = 0;
                entries[idx].reserved = 0;
                entries[idx].commit_time =
                    (state != TransactionState::ACTIVE)
                        ? std::chrono::duration_cast<std::chrono::microseconds>(
                              std::chrono::system_clock::now().time_since_epoch())
                              .count()
                        : 0;

                // Update min/max XIDs
                if (tip_header->min_xid == 0 || xid < tip_header->min_xid)
                {
                    tip_header->min_xid = xid;
                }
                tip_header->max_xid = std::max(xid, tip_header->max_xid);
            }

            // Update checksum
            tip_header->page_header.checksum =
                calculatePageChecksum(reinterpret_cast<uint8_t *>(page_buffer), db_->page_size());

            buffer_pool_->unpinPage(tip_root_page_, true, ctx);

            return Status::OK;
        }

        auto TransactionManager::findTipEntry(uint64_t xid, TIPEntry &entry_out,
                                                  ErrorContext *ctx) -> Status
        {
            // Search TIP pages for the transaction
            uint32_t current_page = tip_root_page_;

            while (current_page != 0)
            {
                void *page_buffer;
                Status status = buffer_pool_->pinPage(current_page, &page_buffer, ctx);
                if (status != Status::OK)
                {
                    return status;
                }

                auto *tip_header = static_cast<TIPPageHeader *>(page_buffer);

                // Check if XID could be in this page
                if (xid >= tip_header->min_xid && xid <= tip_header->max_xid)
                {
                    auto *entries = reinterpret_cast<TIPEntry *>(
                        reinterpret_cast<uint8_t *>(page_buffer) + sizeof(TIPPageHeader));

                    for (uint32_t i = 0; i < tip_header->num_transactions; i++)
                    {
                        if (entries[i].xid == xid)
                        {
                            entry_out = entries[i];
                            buffer_pool_->unpinPage(current_page, false, ctx);
                            return Status::OK;
                        }
                    }
                }

                uint32_t page_to_unpin = current_page;
                current_page = tip_header->next_tip_page;
                buffer_pool_->unpinPage(page_to_unpin, false, ctx);
            }

            return Status::NOT_FOUND;
        }

        auto TransactionManager::flushTransactionState(ErrorContext *ctx) -> Status
        {
            // Ensure all transaction state is persisted
            return db_->sync(ctx);
        }

    } // namespace scratchbird::core
