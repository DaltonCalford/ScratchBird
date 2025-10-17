#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/clog.h"
#include "scratchbird/core/sweep_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/logger.h"
#include "scratchbird/core/config.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <new>
#include <thread>
#include <unistd.h>
#include <unordered_set>

namespace scratchbird::core
{

    // Snapshot cleanup implementation
    void TransactionManager::Snapshot::cleanup()
    {
        if (buffer_pool != nullptr)
        {
            for (uint32_t page_id : pinned_pages)
            {
                buffer_pool->unpinPage(page_id, false, nullptr);
            }
            pinned_pages.clear();
            buffer_pool = nullptr;
        }
    }

    TransactionManager::Snapshot::~Snapshot()
    {
        cleanup();
    }

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

        // Allocate the first TIP page
        Status status = allocateTipPage(tip_root_page_, ctx);
        if (status != Status::OK)
        {
            return status;
        }

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

        // Initialize special transactions (with LRU tracking)
        addToCacheLRU(BOOTSTRAP_XID, TransactionState::COMMITTED);
        addToCacheLRU(FROZEN_XID, TransactionState::COMMITTED);

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
        next_xid_.store(db_header->next_transaction_id, std::memory_order_release);
        oldest_xid_ = db_header->oldest_transaction_id;
        oldest_active_xid_ = db_header->oldest_active_xid;
        oldest_snapshot_ = db_header->oldest_snapshot;

        // DATABASE HEADER VALIDATION: Validate next_xid is sane
        uint64_t current_next_xid = next_xid_.load(std::memory_order_acquire);
        if (current_next_xid > MAX_SAFE_XID)
        {
            buffer_pool_->unpinPage(0, false, ctx);
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "Database next_xid approaching wraparound: next_xid=%lu, max_safe=%lu",
                     current_next_xid, MAX_SAFE_XID);
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, msg);
            // WARNING: Database needs immediate VACUUM to prevent wraparound
            // For now, allow loading but transactions will be blocked
        }

        // Validate next_xid is not corrupted
        if (current_next_xid == UINT64_MAX)
        {
            buffer_pool_->unpinPage(0, false, ctx);
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                              "Database next_xid is UINT64_MAX - corrupted database");
            return Status::PAGE_CORRUPT;
        }

        // Ensure next_xid is at least beyond reserved XIDs
        if (current_next_xid <= FROZEN_XID)
        {
            next_xid_.store(FROZEN_XID + 1, std::memory_order_release);
            current_next_xid = FROZEN_XID + 1;
        }

        // Validate oldest_xid is sane
        if (oldest_xid_ == 0)
        {
            // Not set - initialize to safe default
            oldest_xid_ = FROZEN_XID + 1;
        }
        else if (oldest_xid_ <= FROZEN_XID)
        {
            // Invalid - reset to safe default
            oldest_xid_ = FROZEN_XID + 1;
        }
        else if (oldest_xid_ > current_next_xid)
        {
            // Corrupted - oldest_xid should never exceed next_xid
            char msg[256];
            snprintf(msg, sizeof(msg), "Database oldest_xid > next_xid: oldest=%lu, next=%lu",
                     oldest_xid_, current_next_xid);
            buffer_pool_->unpinPage(0, false, ctx);
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, msg);
            return Status::PAGE_CORRUPT;
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
        auto *entries = reinterpret_cast<TIPEntry *>(reinterpret_cast<uint8_t *>(page_buffer) +
                                                     sizeof(TIPPageHeader));

        for (uint32_t i = 0; i < tip_header->num_transactions; i++)
        {
            addToCacheLRU(entries[i].xid, static_cast<TransactionState>(entries[i].state));

            // Track highest XID (in case it's higher than header's next_xid)
            uint64_t current_xid = entries[i].xid;
            uint64_t expected_next = next_xid_.load(std::memory_order_acquire);
            while (current_xid >= expected_next)
            {
                // Atomically update next_xid_ if current_xid is still >= expected_next
                if (next_xid_.compare_exchange_weak(expected_next, current_xid + 1,
                                                     std::memory_order_acq_rel))
                {
                    break; // Successfully updated
                }
                // If CAS failed, expected_next was updated with current value, retry
            }
        }

        buffer_pool_->unpinPage(page_id, false, ctx);

        return Status::OK;
    }

    auto TransactionManager::beginTransaction(uint32_t proc_id, uint64_t &xid_out,
                                              ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // No longer check for active_xid_ - allow multiple active transactions

        // WRAPAROUND PROTECTION: Check if approaching UINT64_MAX
        uint64_t current_next = next_xid_.load(std::memory_order_acquire);
        if (current_next > MAX_SAFE_XID)
        {
            // Critical: Database is approaching XID wraparound
            // VACUUM must be run to freeze old tuples before continuing
            SET_ERROR_CONTEXT(
                ctx, Status::PAGE_FULL,
                "XID wraparound imminent - VACUUM required to freeze old transactions");
            return Status::PAGE_FULL;
        }

        // Allocate new XID (check for overflow BEFORE increment)
        // Note: Using .load() to read atomic value for comparison
        if (next_xid_.load(std::memory_order_acquire) == UINT64_MAX)
        {
            // Catastrophic: Wraparound occurred
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "XID overflow - database is corrupted");
            return Status::PAGE_CORRUPT;
        }

        // CRITICAL FIX (Issue 1.2): Use atomic fetch_add for thread-safe XID allocation
        // This prevents race conditions where multiple threads could get the same XID
        uint64_t new_xid = next_xid_.fetch_add(1, std::memory_order_seq_cst);

        // Prevent wraparound to reserved XIDs (should never happen with above checks)
        uint64_t check_next = next_xid_.load(std::memory_order_acquire);
        if (check_next <= FROZEN_XID)
        {
            next_xid_.store(FROZEN_XID + 1, std::memory_order_release);
        }

        // Record transaction as active (with LRU tracking)
        addToCacheLRU(new_xid, TransactionState::ACTIVE);

        // Track statistics
        stats_.transactions_started++;

        // Register in ProcArray
        Status status = ProcArrayManager::setTransactionId(proc_id, new_xid, ctx);
        if (status != Status::OK)
        {
            // Rollback on failure
            removeFromCacheLRU(new_xid);
            return status;
        }

        // Write to TIP
        status = writeTipEntry(new_xid, TransactionState::ACTIVE, ctx);
        if (status != Status::OK)
        {
            // Rollback on failure
            removeFromCacheLRU(new_xid);
            ProcArrayManager::clearTransactionId(proc_id, ctx);
            return status;
        }

        // Update database header with new next_xid periodically (every 100 XIDs)
        uint64_t current_next_xid_for_header = next_xid_.load(std::memory_order_acquire);
        if ((current_next_xid_for_header % config::DEFAULT_HEADER_UPDATE_FREQUENCY) == 0)
        {
            void *header_buffer;
            status = buffer_pool_->pinPage(0, &header_buffer, ctx);
            if (status == Status::OK)
            {
                auto *db_header = static_cast<DatabaseHeader *>(header_buffer);
                db_header->next_transaction_id = current_next_xid_for_header;
                buffer_pool_->unpinPage(0, true, ctx);
            }
            // Ignore errors - this is just an optimization
        }

        xid_out = new_xid;
        return Status::OK;
    }

    auto TransactionManager::commitTransaction(uint32_t proc_id, uint64_t xid, ErrorContext *ctx)
        -> Status
    {
        Status status;

        // Perform pre-commit work within mutex
        {
            std::lock_guard<std::mutex> lock(mutex_);

            // Update cache state
            auto cache_it = transaction_cache_.find(xid);
            if (cache_it != transaction_cache_.end())
            {
                cache_it->second = TransactionState::COMMITTED;
                touchCacheEntry(xid);
            }
            else
            {
                addToCacheLRU(xid, TransactionState::COMMITTED);
            }

            // Write to CLOG (commit log)
            status = db_->clog()->setStatus(xid, ClogStatus::COMMITTED, ctx);
            if (status != Status::OK)
            {
                // Rollback cache on CLOG failure
                auto it = transaction_cache_.find(xid);
                if (it != transaction_cache_.end())
                {
                    it->second = TransactionState::ABORTED;
                    touchCacheEntry(xid);
                }
                return status;
            }

            // Track statistics
            stats_.transactions_committed++;
        }
        // Mutex released - don't hold during I/O!

        // GROUP COMMIT OPTIMIZATION (Issue 2.19)
        if (group_commit_enabled_.load(std::memory_order_acquire))
        {
            // Create waiter for this commit
            CommitWaiter waiter(xid, TransactionState::COMMITTED);

            bool is_leader = false;

            // Try to become leader
            {
                std::lock_guard<std::mutex> lock(group_commit_mutex_);

                if (!group_commit_in_progress_)
                {
                    // Become leader
                    is_leader = true;
                    group_commit_in_progress_ = true;
                }
                else
                {
                    // Join queue as follower
                    commit_queue_.push_back(&waiter);
                }
            }

            if (is_leader)
            {
                // Perform group commit as leader
                status = performGroupCommit(&waiter, ctx);

                // Mark group commit complete
                {
                    std::lock_guard<std::mutex> lock(group_commit_mutex_);
                    group_commit_in_progress_ = false;
                }
            }
            else
            {
                // Wait for leader to complete
                std::unique_lock<std::mutex> lock(waiter.cv_mutex);
                waiter.cv.wait(lock, [&waiter] { return waiter.completed; });
                status = waiter.result;
            }
        }
        else
        {
            // Fallback: Traditional individual commit (for testing/debugging)
            status = writeTipEntry(xid, TransactionState::COMMITTED, ctx);
            if (status != Status::OK)
            {
                LOG_WARNING(TRANSACTION, "Failed to update TIP entry for committed XID %lu", xid);
            }
            status = db_->sync(ctx);
        }

        // Clear ProcArray slot after durability guaranteed (Issue 1.14)
        Status clear_status = ProcArrayManager::clearTransactionId(proc_id, ctx);
        if (clear_status != Status::OK)
        {
            LOG_WARNING(TRANSACTION, "Failed to clear ProcArray slot for committed XID %lu", xid);
        }

        // Check sweep trigger (non-blocking)
        if (status == Status::OK && db_->sweep_manager())
        {
            db_->sweep_manager()->checkSweepTrigger(ctx);
        }

        return status;
    }

    auto TransactionManager::rollbackTransaction(uint32_t proc_id, uint64_t xid, ErrorContext *ctx)
        -> Status
    {
        Status status;

        // Perform pre-rollback work within mutex
        {
            std::lock_guard<std::mutex> lock(mutex_);

            // Update cache state
            auto cache_it = transaction_cache_.find(xid);
            if (cache_it != transaction_cache_.end())
            {
                cache_it->second = TransactionState::ABORTED;
                touchCacheEntry(xid);
            }
            else
            {
                addToCacheLRU(xid, TransactionState::ABORTED);
            }

            // Write to CLOG (commit log)
            status = db_->clog()->setStatus(xid, ClogStatus::ABORTED, ctx);
            if (status != Status::OK)
            {
                // Rollback cache on CLOG failure
                auto it = transaction_cache_.find(xid);
                if (it != transaction_cache_.end())
                {
                    it->second = TransactionState::ACTIVE;
                    touchCacheEntry(xid);
                }
                return status;
            }

            // Track statistics
            stats_.transactions_aborted++;
        }
        // Mutex released - don't hold during I/O!

        // GROUP COMMIT OPTIMIZATION (Issue 2.19) - Applied to rollbacks for consistency
        if (group_commit_enabled_.load(std::memory_order_acquire))
        {
            // Create waiter for this rollback
            CommitWaiter waiter(xid, TransactionState::ABORTED);

            bool is_leader = false;

            // Try to become leader
            {
                std::lock_guard<std::mutex> lock(group_commit_mutex_);

                if (!group_commit_in_progress_)
                {
                    // Become leader
                    is_leader = true;
                    group_commit_in_progress_ = true;
                }
                else
                {
                    // Join queue as follower
                    commit_queue_.push_back(&waiter);
                }
            }

            if (is_leader)
            {
                // Perform group commit as leader (handles both commits and rollbacks)
                status = performGroupCommit(&waiter, ctx);

                // Mark group commit complete
                {
                    std::lock_guard<std::mutex> lock(group_commit_mutex_);
                    group_commit_in_progress_ = false;
                }
            }
            else
            {
                // Wait for leader to complete
                std::unique_lock<std::mutex> lock(waiter.cv_mutex);
                waiter.cv.wait(lock, [&waiter] { return waiter.completed; });
                status = waiter.result;
            }
        }
        else
        {
            // Fallback: Traditional individual rollback (for testing/debugging)
            status = writeTipEntry(xid, TransactionState::ABORTED, ctx);
            if (status != Status::OK)
            {
                LOG_WARNING(TRANSACTION, "Failed to update TIP entry for aborted XID %lu", xid);
            }
            status = db_->sync(ctx);
        }

        // Clear ProcArray slot after durability guaranteed (Issue 1.14)
        Status clear_status = ProcArrayManager::clearTransactionId(proc_id, ctx);
        if (clear_status != Status::OK)
        {
            LOG_WARNING(TRANSACTION, "Failed to clear ProcArray slot for aborted XID %lu", xid);
        }

        return status;
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
            touchCacheEntry(xid); // Mark as recently used
            return Status::OK;
        }

        // Not in cache, check CLOG
        ClogStatus clog_status;
        Status status = db_->clog()->getStatus(xid, &clog_status, ctx);
        if (status == Status::NOT_FOUND)
        {
            // Transaction not found, assume it's too old and committed
            state_out = TransactionState::COMMITTED;
            addToCacheLRU(xid, TransactionState::COMMITTED);
            return Status::OK;
        }

        if (status != Status::OK)
        {
            return status;
        }

        // Convert CLOG status to TransactionState
        switch (clog_status)
        {
            case ClogStatus::IN_PROGRESS:
                state_out = TransactionState::ACTIVE;
                break;
            case ClogStatus::COMMITTED:
                state_out = TransactionState::COMMITTED;
                break;
            case ClogStatus::ABORTED:
                state_out = TransactionState::ABORTED;
                break;
            case ClogStatus::SUB_COMMITTED:
                // For now, treat sub-committed as committed
                state_out = TransactionState::COMMITTED;
                break;
        }
        transaction_cache_[xid] = state_out;

        return Status::OK;
    }

    auto TransactionManager::isValidXid(uint64_t xid) -> bool
    {
        // INVALID_XID (0) is never valid for tuple headers
        if (xid == INVALID_XID)
        {
            return false;
        }

        // All other XIDs are structurally valid
        // (BOOTSTRAP_XID, FROZEN_XID, and user XIDs)
        return true;
    }

    auto TransactionManager::isXidInRange(uint64_t xid) const -> bool
    {
        // Check structural validity first
        if (!isValidXid(xid))
        {
            return false;
        }

        // Reserved XIDs are always in range
        if (xid <= FROZEN_XID)
        {
            return true;
        }

        std::lock_guard<std::mutex> lock(mutex_);

        // User XIDs must be less than next_xid (no future transactions)
        uint64_t current_next_xid = next_xid_.load(std::memory_order_acquire);
        if (xid >= current_next_xid)
        {
            return false; // Future XID - invalid!
        }

        // Check if XID is too old (has been vacuumed)
        // XIDs older than oldest_xid_ should have been frozen by VACUUM
        if (xid < oldest_xid_)
        {
            // Old XID that should have been frozen
            // CORRUPTION LOGGING: This indicates the tuple wasn't frozen by VACUUM
            LOG_WARNING(
                VACUUM,
                "XID %lu is older than oldest_xid %lu - tuple should have been frozen by VACUUM",
                xid, oldest_xid_);
            // CRITICAL FIX (Issue 2.9): Reject old unfrozen XIDs for data integrity
            // This enforces proper VACUUM discipline and protects wraparound mechanisms
            return false;
        }

        // XID is in valid range
        return true;
    }

    auto TransactionManager::setOldestXid(uint64_t xid, ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Validate new oldest XID is sane
        uint64_t current_next_xid = next_xid_.load(std::memory_order_acquire);
        if (xid > current_next_xid)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Cannot set oldest_xid beyond next_xid");
            return Status::INVALID_ARGUMENT;
        }

        if (xid <= FROZEN_XID)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "oldest_xid must be > FROZEN_XID");
            return Status::INVALID_ARGUMENT;
        }

        oldest_xid_ = xid;

        // Update database header with new oldest XID
        void *header_buffer;
        Status status = buffer_pool_->pinPage(0, &header_buffer, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        auto *db_header = static_cast<DatabaseHeader *>(header_buffer);
        db_header->oldest_transaction_id = oldest_xid_;

        buffer_pool_->unpinPage(0, true, ctx);

        return Status::OK;
    }

    auto TransactionManager::updateTransactionMarkers(ErrorContext *ctx) -> Status
    {
        // CRITICAL FIX (CRITICAL-3): Lock ordering documentation
        // This method follows correct lock hierarchy: mutex_ → ProcArray::array_lock
        // 1. Acquire mutex_ (protects transaction markers)
        // 2. Then acquire ProcArray::array_lock (protects process control blocks)
        // This ordering MUST be maintained to prevent deadlock!
        std::lock_guard<std::mutex> lock(mutex_);

        // Get ProcArray instance to scan active transactions
        ProcArray *proc_array = ProcArrayManager::getInstance();
        if (!proc_array)
        {
            // ProcArray not initialized - this is okay during early startup
            return Status::OK;
        }

        // LOCK ORDERING: mutex_ already held, now acquire ProcArray::array_lock (read lock)
        // This is CORRECT order: mutex_ → ProcArray::array_lock
        pthread_rwlock_rdlock(&proc_array->array_lock);

        // Compute OAT (Oldest Active Transaction) and OST (Oldest Snapshot Transaction)
        uint64_t current_next_xid = next_xid_.load(std::memory_order_acquire);
        uint64_t new_oat = current_next_xid; // Start with next_xid (will be reduced)
        uint64_t new_ost = current_next_xid; // Start with next_xid (will be reduced)
        bool has_active = false;
        bool has_snapshot = false;

        // Scan all process control blocks
        ProcessControlBlock *pcbs = reinterpret_cast<ProcessControlBlock *>(
            reinterpret_cast<uint8_t *>(proc_array) + sizeof(ProcArray));

        for (uint32_t i = 0; i < proc_array->max_backends; ++i)
        {
            ProcessControlBlock *pcb = &pcbs[i];

            if (!pcb->is_active || pcb->xid == 0)
            {
                continue; // Skip inactive or non-transactional backends
            }

            // OPTIMIZATION: Exclude read-only transactions from OAT calculation
            // Read-only transactions don't create tuple versions, so they don't prevent VACUUM
            // This allows VACUUM to be more aggressive when there are only read-only analytics
            // queries
            if (!pcb->is_read_only)
            {
                // Update OAT - minimum of all active WRITE transactions
                if (pcb->xid < new_oat)
                {
                    new_oat = pcb->xid;
                    has_active = true;
                }
            }

            // Update OST - minimum of active SNAPSHOT transaction XIDs (regardless of read-only
            // status) OST must include read-only transactions for correct MVCC visibility
            if (pcb->is_snapshot_txn && pcb->xid < new_ost)
            {
                new_ost = pcb->xid;
                has_snapshot = true;
            }
        }

        pthread_rwlock_unlock(&proc_array->array_lock);

        // If no active transactions, set OAT to 0
        if (!has_active)
        {
            new_oat = 0;
        }

        // If no snapshot transactions, set OST to 0
        if (!has_snapshot)
        {
            new_ost = 0;
        }

        // Update in-memory markers
        oldest_active_xid_ = new_oat;
        oldest_snapshot_ = new_ost;

        // Update database header with new markers
        void *header_buffer;
        Status status = buffer_pool_->pinPage(0, &header_buffer, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        auto *db_header = static_cast<DatabaseHeader *>(header_buffer);
        db_header->oldest_active_xid = oldest_active_xid_;
        db_header->oldest_snapshot = oldest_snapshot_;

        buffer_pool_->unpinPage(0, true, ctx);

        return Status::OK;
    }

    auto TransactionManager::isTransactionVisible(uint64_t xid, uint64_t snapshot_xid) -> bool
    {
        // VALIDATE XID FIRST - Critical security check
        if (!isXidInRange(xid))
        {
            // Invalid XID - treat as invisible
            // This protects against corrupted tuple headers
            // ISSUE 3.4 FIX: Rate limit logging in hot path to prevent log spam
            // Only log first occurrence per invalid XID to avoid performance degradation
            static thread_local std::unordered_set<uint64_t> logged_invalid_xids;
            if (logged_invalid_xids.find(xid) == logged_invalid_xids.end())
            {
                uint64_t current_next = next_xid_.load(std::memory_order_acquire);
                LOG_WARNING(TRANSACTION,
                          "Invalid XID %lu in visibility check (next_xid=%lu, oldest_xid=%lu) - "
                          "further occurrences suppressed", xid,
                          current_next, oldest_xid_);
                logged_invalid_xids.insert(xid);

                // Limit set size to prevent unbounded memory growth
                if (logged_invalid_xids.size() > 1000)
                {
                    logged_invalid_xids.clear();
                }
            }
            return false;
        }

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

    auto TransactionManager::isSnapshotVisible(uint64_t xid, const Snapshot *snapshot) -> bool
    {
        // Null snapshot check
        if (snapshot == nullptr)
        {
            LOG_WARNING(TRANSACTION, "isSnapshotVisible called with null snapshot for XID %lu",
                        xid);
            return false;
        }

        // 1. Validate XID - protect against corrupted tuple headers
        if (!isXidInRange(xid))
        {
            // ISSUE 3.4 FIX: Rate limit logging in hot path to prevent log spam
            // Only log first occurrence per invalid XID to avoid performance degradation
            static thread_local std::unordered_set<uint64_t> logged_invalid_snapshot_xids;
            if (logged_invalid_snapshot_xids.find(xid) == logged_invalid_snapshot_xids.end())
            {
                LOG_WARNING(TRANSACTION, "Invalid XID %lu in snapshot visibility check - "
                          "further occurrences suppressed", xid);
                logged_invalid_snapshot_xids.insert(xid);

                // Limit set size to prevent unbounded memory growth
                if (logged_invalid_snapshot_xids.size() > 1000)
                {
                    logged_invalid_snapshot_xids.clear();
                }
            }
            return false;
        }

        // 2. Future transaction (started after snapshot was taken) - INVISIBLE
        //    Any XID >= xmax did not exist when snapshot was created
        if (xid >= snapshot->xmax)
        {
            return false;
        }

        // 3. Frozen tuples are always visible
        //    FROZEN_XID = 2, used for tuples that survived VACUUM
        if (xid <= FROZEN_XID)
        {
            return true;
        }

        // 4. Transaction was active at snapshot time - INVISIBLE
        //    Binary search in sorted active_xids array (O(log N))
        //    Note: active_xids is sorted by getSnapshot()
        if (std::binary_search(snapshot->active_xids.begin(), snapshot->active_xids.end(), xid))
        {
            return false; // Transaction was in-progress at snapshot time
        }

        // 5. Old transaction (started before oldest active transaction)
        //    These must be committed to be visible
        if (xid < snapshot->xmin)
        {
            TransactionState state;
            if (getTransactionState(xid, state, nullptr) != Status::OK)
            {
                // Error getting state - for safety, assume visible if old
                return true;
            }
            return state == TransactionState::COMMITTED;
        }

        // 6. Transaction started after xmin but before xmax,
        //    and was NOT in the active list at snapshot time
        //    This means it must have committed BEFORE the snapshot
        //    Verify it's actually committed
        TransactionState state;
        if (getTransactionState(xid, state, nullptr) != Status::OK)
        {
            // Error getting state - assume not visible for safety
            return false;
        }

        return state == TransactionState::COMMITTED;
    }

    auto TransactionManager::getBackendXid(uint32_t proc_id) const -> uint64_t
    {
        // Use ProcArray API to safely get backend XID
        uint64_t xid = 0;
        ProcArrayManager::getBackendXid(proc_id, &xid, nullptr);
        return xid;
    }

    auto TransactionManager::getSnapshot(Snapshot &snapshot_out, ErrorContext *ctx) -> Status
    {
        // CRITICAL FIX (CRITICAL-3): Lock ordering documentation
        // This method follows correct lock hierarchy: mutex_ → ProcArray::array_lock
        // 1. Acquire mutex_ (protects transaction state)
        // 2. Then acquire ProcArray::array_lock (protects process control blocks)
        // This ordering MUST be maintained to prevent deadlock!
        std::lock_guard<std::mutex> lock(mutex_);

        snapshot_out.xmax = next_xid_.load(std::memory_order_acquire);
        snapshot_out.active_xids.clear();

        // Get active transactions from ProcArray
        // NOTE: getActiveTransactions() internally acquires ProcArray::array_lock
        // This is safe because we're following the correct lock order: mutex_ → array_lock
        uint64_t oldest_xmin = 0;
        Status status =
            ProcArrayManager::getActiveTransactions(&snapshot_out.active_xids, &oldest_xmin, ctx);
        if (status != Status::OK)
        {
            // Fallback to simple snapshot if ProcArray not available
            snapshot_out.xmin = FROZEN_XID + 1;
            return Status::OK;
        }

        // OPTIMIZATION: For read-only transactions, filter out other read-only transactions
        // from the active_xids list. Read-only transactions don't create write conflicts,
        // so they don't need to be tracked for visibility purposes.
        // This reduces memory usage and improves snapshot visibility check performance.
        ConnectionContext *current_ctx = ConnectionContext::getCurrent();
        if (current_ctx && current_ctx->isReadOnly())
        {
            // Get ProcArray to check read-only status of active transactions
            ProcArray *proc_array = ProcArrayManager::getInstance();
            if (proc_array)
            {
                // LOCK ORDERING: mutex_ already held, now acquire ProcArray::array_lock (read lock)
                // This is CORRECT order: mutex_ → ProcArray::array_lock
                // NOTE: This is a second acquisition of array_lock (first was in getActiveTransactions)
                // but that's safe because rdlocks are reentrant for the same thread
                pthread_rwlock_rdlock(&proc_array->array_lock);

                ProcessControlBlock *pcbs = reinterpret_cast<ProcessControlBlock *>(
                    reinterpret_cast<uint8_t *>(proc_array) + sizeof(ProcArray));

                // Filter active_xids to only include write transactions
                std::vector<uint64_t> filtered_xids;
                filtered_xids.reserve(snapshot_out.active_xids.size());

                for (uint64_t active_xid : snapshot_out.active_xids)
                {
                    // Find this XID in proc array
                    bool is_write_txn = true; // Assume write if not found
                    for (uint32_t i = 0; i < proc_array->max_backends; ++i)
                    {
                        if (pcbs[i].is_active && pcbs[i].xid == active_xid)
                        {
                            is_write_txn = !pcbs[i].is_read_only;
                            break;
                        }
                    }

                    // Only include write transactions
                    if (is_write_txn)
                    {
                        filtered_xids.push_back(active_xid);
                    }
                }

                pthread_rwlock_unlock(&proc_array->array_lock);

                // Track statistics
                size_t original_size = snapshot_out.active_xids.size();
                size_t filtered_size = filtered_xids.size();
                uint64_t xids_filtered = original_size - filtered_size;

                snapshot_out.active_xids = std::move(filtered_xids);

                stats_.readonly_snapshots++;
                stats_.readonly_snapshot_xids_filtered += xids_filtered;

                LOG_DEBUG(TRANSACTION,
                          "Read-only snapshot optimization: filtered %zu active XIDs down to %zu "
                          "write XIDs",
                          original_size, filtered_size);
            }
        }

        snapshot_out.xmin = (oldest_xmin != 0) ? oldest_xmin : FROZEN_XID + 1;

        // Sort active_xids for efficient binary search in isSnapshotVisible()
        std::sort(snapshot_out.active_xids.begin(), snapshot_out.active_xids.end());

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
            LOG_WARNING(TRANSACTION, "Failed to flush page manager after TIP page allocation");
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

    auto TransactionManager::writeTipEntry(uint64_t xid, TransactionState state, ErrorContext *ctx)
        -> Status
    {
        // ===========================================================================================
        // ISSUE 3.1: OPTIMIZE TIP PAGE SCAN
        // ===========================================================================================
        //
        // OPTIMIZATION 1: Check transaction_cache_ first
        // If XID is already in cache, we know it exists in TIP (likely)
        // This avoids the O(N) TIP page scan for cache hits
        //
        // OPTIMIZATION 2: Use TIP location cache
        // Maps XID -> TIP page ID to avoid scanning entire TIP chain
        // Cache is populated when we find/create an entry
        //
        // Performance impact:
        // - Before: O(N) scan through all TIP pages and entries (worst case: thousands of pages)
        // - After: O(1) cache lookup + single page pin (best case: cache hit)
        // - Expected speedup: 10-100x for transactions with multiple state updates
        //
        // See: docs/audit/ISSUE_3_1_STATUS.md for complete analysis
        // ===========================================================================================

        // OPTIMIZATION 1: Check transaction_cache_ first (quick O(1) check)
        // If XID is in cache, it's likely already in TIP, so try TIP location cache
        auto cache_it = transaction_cache_.find(xid);
        bool in_cache = (cache_it != transaction_cache_.end());

        // OPTIMIZATION 2: Check TIP location cache for known page
        uint32_t start_page = tip_root_page_;
        auto tip_cache_it = tip_location_cache_.find(xid);
        if (tip_cache_it != tip_location_cache_.end())
        {
            // We know which page this XID is on - start there
            start_page = tip_cache_it->second;

            // Try to update the entry on the cached page first (fast path)
            void *page_buffer;
            Status status = buffer_pool_->pinPage(start_page, &page_buffer, ctx);
            if (status == Status::OK)
            {
                auto *tip_header = static_cast<TIPPageHeader *>(page_buffer);

                // Verify XID is in range for this page (cache could be stale)
                if (xid >= tip_header->min_xid && xid <= tip_header->max_xid)
                {
                    auto *entries = reinterpret_cast<TIPEntry *>(
                        reinterpret_cast<uint8_t *>(page_buffer) + sizeof(TIPPageHeader));

                    // Search for XID in this page
                    for (uint32_t i = 0; i < tip_header->num_transactions; i++)
                    {
                        if (entries[i].xid == xid)
                        {
                            // FAST PATH: Found entry on cached page - update it
                            entries[i].state = static_cast<uint8_t>(state);
                            entries[i].commit_time =
                                (state != TransactionState::ACTIVE)
                                    ? std::chrono::duration_cast<std::chrono::microseconds>(
                                          std::chrono::system_clock::now().time_since_epoch())
                                          .count()
                                    : 0;

                            // Update checksum
                            tip_header->page_header.checksum = calculatePageChecksum(
                                reinterpret_cast<uint8_t *>(page_buffer), db_->page_size());

                            buffer_pool_->unpinPage(start_page, true, ctx);
                            return Status::OK;
                        }
                    }
                }

                // XID not found on cached page - cache is stale, fall through to full scan
                buffer_pool_->unpinPage(start_page, false, ctx);
                tip_location_cache_.erase(xid); // Remove stale entry
            }
        }

        // SLOW PATH: XID not in TIP location cache or cache was stale
        // Perform full scan of TIP chain to find existing entry
        uint32_t current_page = tip_root_page_;
        uint32_t last_page = tip_root_page_;

        while (current_page != 0)
        {
            void *page_buffer;
            Status status = buffer_pool_->pinPage(current_page, &page_buffer, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *tip_header = static_cast<TIPPageHeader *>(page_buffer);

            // Check if this XID already exists in this page (for updates)
            auto *entries = reinterpret_cast<TIPEntry *>(reinterpret_cast<uint8_t *>(page_buffer) +
                                                         sizeof(TIPPageHeader));

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

                    // Update checksum
                    tip_header->page_header.checksum = calculatePageChecksum(
                        reinterpret_cast<uint8_t *>(page_buffer), db_->page_size());

                    // Cache this page location for future updates (OPTIMIZATION)
                    if (tip_location_cache_.size() < MAX_TIP_LOCATION_CACHE_SIZE)
                    {
                        tip_location_cache_[xid] = current_page;
                    }

                    buffer_pool_->unpinPage(current_page, true, ctx);
                    return Status::OK;
                }
            }

            last_page = current_page;
            current_page = tip_header->next_tip_page;
            buffer_pool_->unpinPage(last_page, false, ctx);
        }

        // XID not found - need to add new entry to the last page
        // Re-pin the last page
        void *page_buffer;
        Status status = buffer_pool_->pinPage(last_page, &page_buffer, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        auto *tip_header = static_cast<TIPPageHeader *>(page_buffer);

        // Check if there's space on the last page
        if (tip_header->num_transactions >= getTipEntriesPerPage())
        {
            // Page is full - need to allocate a new page and chain it
            uint32_t new_page_id;
            status = allocateTipPage(new_page_id, ctx);
            if (status != Status::OK)
            {
                buffer_pool_->unpinPage(last_page, false, ctx);
                SET_ERROR_CONTEXT(ctx, status, "Failed to allocate new TIP page for chaining");
                return status;
            }

            // Update the last page's next pointer
            tip_header->next_tip_page = new_page_id;
            tip_header->page_header.checksum =
                calculatePageChecksum(reinterpret_cast<uint8_t *>(page_buffer), db_->page_size());
            buffer_pool_->unpinPage(last_page, true, ctx);

            // Now use the new page
            last_page = new_page_id;
            status = buffer_pool_->pinPage(last_page, &page_buffer, ctx);
            if (status != Status::OK)
            {
                return status;
            }
            tip_header = static_cast<TIPPageHeader *>(page_buffer);
        }

        // Add new entry (we already checked it doesn't exist in the chain)
        auto *entries = reinterpret_cast<TIPEntry *>(reinterpret_cast<uint8_t *>(page_buffer) +
                                                     sizeof(TIPPageHeader));

        uint32_t idx = tip_header->num_transactions++;
        entries[idx].xid = xid;
        entries[idx].state = static_cast<uint8_t>(state);
        entries[idx].flags = 0;
        entries[idx].reserved = 0;
        entries[idx].commit_time = (state != TransactionState::ACTIVE)
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

        // Update checksum
        tip_header->page_header.checksum =
            calculatePageChecksum(reinterpret_cast<uint8_t *>(page_buffer), db_->page_size());

        // Cache this page location for future updates (OPTIMIZATION)
        if (tip_location_cache_.size() < MAX_TIP_LOCATION_CACHE_SIZE)
        {
            tip_location_cache_[xid] = last_page;
        }

        buffer_pool_->unpinPage(last_page, true, ctx);

        return Status::OK;
    }

    auto TransactionManager::writeTipEntriesBatch(
        const std::vector<std::pair<uint64_t, TransactionState>> &batch, ErrorContext *ctx) -> Status
    {
        if (batch.empty())
        {
            return Status::OK;
        }

        // Sort batch by XID to minimize page pin/unpin cycles
        std::vector<std::pair<uint64_t, TransactionState>> sorted_batch = batch;
        std::sort(sorted_batch.begin(), sorted_batch.end(),
                  [](const auto &a, const auto &b) { return a.first < b.first; });

        // Process all XIDs in the batch
        for (const auto &[xid, state] : sorted_batch)
        {
            // For each XID, update or insert the TIP entry
            // This reuses the existing writeTipEntry logic but without the fsync
            Status status = writeTipEntry(xid, state, ctx);
            if (status != Status::OK)
            {
                LOG_ERROR(TRANSACTION, "Failed to write TIP entry for XID %lu in batch", xid);
                return status;
            }
        }

        // All TIP entries written successfully
        return Status::OK;
    }

    auto TransactionManager::performGroupCommit(CommitWaiter *leader_waiter, ErrorContext *ctx)
        -> Status
    {
        // Leader function: collect batch, write TIDs, fsync, wake waiters
        std::vector<CommitWaiter *> batch;
        batch.push_back(leader_waiter);

        // Collect batch of waiting commits (with timeout)
        auto start_time = std::chrono::steady_clock::now();
        auto deadline = start_time + std::chrono::microseconds(group_commit_timeout_us_);

        while (std::chrono::steady_clock::now() < deadline &&
               batch.size() < group_commit_batch_size_)
        {
            {
                std::lock_guard<std::mutex> lock(group_commit_mutex_);

                // Collect all waiting commits from queue
                while (!commit_queue_.empty() && batch.size() < group_commit_batch_size_)
                {
                    batch.push_back(commit_queue_.back());
                    commit_queue_.pop_back();
                }
            }

            // If we have a good batch size, break early
            if (batch.size() >= group_commit_batch_size_ / 2)
            {
                break;
            }

            // If queue is empty and we're past minimum wait time, break
            if (commit_queue_.empty() &&
                std::chrono::steady_clock::now() - start_time >
                    std::chrono::microseconds(group_commit_timeout_us_ / 4))
            {
                break;
            }

            // Sleep briefly (1ms) to allow more commits to arrive
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        // Final queue drain
        {
            std::lock_guard<std::mutex> lock(group_commit_mutex_);
            while (!commit_queue_.empty() && batch.size() < group_commit_batch_size_)
            {
                batch.push_back(commit_queue_.back());
                commit_queue_.pop_back();
            }
        }

        // Build TID batch for TIP writes
        std::vector<std::pair<uint64_t, TransactionState>> xid_batch;
        xid_batch.reserve(batch.size());
        for (auto *waiter : batch)
        {
            xid_batch.push_back({waiter->xid, waiter->state});
        }

        // Write all TIP entries in batch
        Status status = writeTipEntriesBatch(xid_batch, ctx);

        // Single fsync for entire batch (the key optimization!)
        if (status == Status::OK)
        {
            status = db_->sync(ctx);
        }

        // Wake all waiters with result
        for (auto *waiter : batch)
        {
            std::lock_guard<std::mutex> lock(waiter->cv_mutex);
            waiter->result = status;
            waiter->completed = true;
            waiter->cv.notify_one();
        }

        // Update statistics
        group_commits_performed_.fetch_add(1, std::memory_order_relaxed);
        group_commit_total_xids_.fetch_add(batch.size(), std::memory_order_relaxed);

        LOG_DEBUG(TRANSACTION, "Group commit completed: %zu XIDs in batch, status=%d", batch.size(),
                  static_cast<int>(status));

        return status;
    }

    auto TransactionManager::findTipEntry(uint64_t xid, TIPEntry &entry_out, ErrorContext *ctx)
        -> Status
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

    void TransactionManager::touchCacheEntry(uint64_t xid) const
    {
        // Move entry to front of LRU list (most recently used)
        // Assumes mutex_ is already held by caller

        auto lru_it = cache_lru_map_.find(xid);
        if (lru_it == cache_lru_map_.end())
        {
            return; // Not in cache
        }

        // Remove from current position
        cache_lru_list_.erase(lru_it->second);

        // Add to front
        cache_lru_list_.push_front(xid);

        // Update map
        cache_lru_map_[xid] = cache_lru_list_.begin();
    }

    void TransactionManager::evictOldestCacheEntry() const
    {
        // Remove least recently used entry (back of list)
        // Assumes mutex_ is already held by caller

        if (cache_lru_list_.empty())
        {
            return;
        }

        uint64_t oldest_xid = cache_lru_list_.back();

        // Remove from all structures
        cache_lru_list_.pop_back();
        cache_lru_map_.erase(oldest_xid);
        transaction_cache_.erase(oldest_xid);
    }

    void TransactionManager::addToCacheLRU(uint64_t xid, TransactionState state) const
    {
        // Add entry with LRU tracking
        // Assumes mutex_ is already held by caller

        // Check if cache is full
        if (transaction_cache_.size() >= MAX_CACHE_SIZE)
        {
            evictOldestCacheEntry();
        }

        // Add to cache
        transaction_cache_[xid] = state;

        // Add to front of LRU list
        cache_lru_list_.push_front(xid);
        cache_lru_map_[xid] = cache_lru_list_.begin();
    }

    void TransactionManager::removeFromCacheLRU(uint64_t xid) const
    {
        // Remove entry with LRU cleanup
        // Assumes mutex_ is already held by caller

        auto lru_it = cache_lru_map_.find(xid);
        if (lru_it != cache_lru_map_.end())
        {
            cache_lru_list_.erase(lru_it->second);
            cache_lru_map_.erase(lru_it);
        }

        transaction_cache_.erase(xid);
    }

} // namespace scratchbird::core
