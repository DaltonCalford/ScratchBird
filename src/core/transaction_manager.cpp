/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/clog.h"
#include "scratchbird/core/sweep_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/logger.h"
#include "scratchbird/core/mga_failpoint_manager.h"
#include "scratchbird/core/config.h"
#include "scratchbird/core/portable_file_io.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <new>
#include <thread>
#include <unordered_set>

namespace {
    uint64_t nowMicros() {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count());
    }

    auto lockModeNameLocal(scratchbird::core::LockMode mode) -> const char*
    {
        using scratchbird::core::LockMode;
        switch (mode)
        {
            case LockMode::LOCK_ACCESS_SHARE:
                return "LOCK_ACCESS_SHARE";
            case LockMode::LOCK_ROW_SHARE:
                return "LOCK_ROW_SHARE";
            case LockMode::LOCK_ROW_EXCLUSIVE:
                return "LOCK_ROW_EXCLUSIVE";
            case LockMode::LOCK_SHARE_UPDATE_EXCLUSIVE:
                return "LOCK_SHARE_UPDATE_EXCLUSIVE";
            case LockMode::LOCK_SHARE:
                return "LOCK_SHARE";
            case LockMode::LOCK_SHARE_ROW_EXCLUSIVE:
                return "LOCK_SHARE_ROW_EXCLUSIVE";
            case LockMode::LOCK_EXCLUSIVE:
                return "LOCK_EXCLUSIVE";
            case LockMode::LOCK_ACCESS_EXCLUSIVE:
                return "LOCK_ACCESS_EXCLUSIVE";
        }
        return "LOCK_UNKNOWN";
    }

    auto formatLockResourceIdLocal(const scratchbird::core::LockTag& tag) -> std::string
    {
        return std::to_string(static_cast<uint8_t>(tag.target_type)) + ":" +
               tag.object_uuid.toString() + ":" +
               std::to_string(tag.page_num) + ":" +
               std::to_string(tag.offset_num);
    }

    scratchbird::core::ClogStatus clogStatusForTransactionState(
        scratchbird::core::TransactionState state)
    {
        using scratchbird::core::ClogStatus;
        using scratchbird::core::TransactionState;

        switch (state)
        {
            case TransactionState::ACTIVE:
                return ClogStatus::IN_PROGRESS;
            case TransactionState::COMMITTED:
                return ClogStatus::COMMITTED;
            case TransactionState::ABORTED:
                return ClogStatus::ABORTED;
            case TransactionState::PREPARED:
                return ClogStatus::PREPARED;
        }

        return ClogStatus::IN_PROGRESS;
    }
}

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
        db_header->page_header.checksum =
            calculatePageChecksum(reinterpret_cast<uint8_t *>(db_header), db_->page_size());

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

    auto TransactionManager::load(StartupReconciliationSummary *startup_summary,
                                  ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_pins_.clear();
        if (startup_summary != nullptr)
        {
            *startup_summary = {};
            startup_summary->clean_shutdown_marker = db_->last_shutdown_was_clean();
        }

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
        inventory_generation_ = db_header->inventory_generation == 0
            ? 1
            : db_header->inventory_generation;
        oldest_snapshot_serial_ = db_header->oldest_snapshot_serial;
        next_snapshot_serial_ = oldest_snapshot_serial_ + 1;

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

        if (oldest_active_xid_ == 0 || oldest_active_xid_ > current_next_xid)
        {
            oldest_active_xid_ = current_next_xid;
        }

        if (oldest_snapshot_ == 0 || oldest_snapshot_ > current_next_xid)
        {
            oldest_snapshot_ = current_next_xid;
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

        status = loadTipPage(tip_root_page_, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        CatalogManager *catalog = db_->catalog_manager();
        if (catalog)
        {
            std::vector<CatalogManager::PreparedTransactionInfo> prepared;
            Status list_status = catalog->listPreparedTransactions(prepared, ctx);
            if (list_status == Status::OK)
            {
                for (const auto &entry : prepared)
                {
                    prepared_xids_.insert(entry.txn_id);
                    auto cache_it = transaction_cache_.find(entry.txn_id);
                    if (cache_it != transaction_cache_.end())
                    {
                        cache_it->second = TransactionState::PREPARED;
                        touchCacheEntry(entry.txn_id);
                    }
                    else
                    {
                        addToCacheLRU(entry.txn_id, TransactionState::PREPARED);
                    }
                }
            }
            else if (list_status != Status::NOT_FOUND)
            {
                LOG_WARNING(TRANSACTION, "Failed to load prepared transactions: %d",
                            static_cast<int>(list_status));
            }
        }

        MgaFailpointManager* failpoints = db_ ? db_->mga_failpoint_manager() : nullptr;
        if (failpoints != nullptr)
        {
            status = failpoints->trip(
                MgaFailpointTriggers::kAfterTipLoadBeforeActiveNormalization,
                {},
                ctx);
            if (status != Status::OK)
            {
                return status;
            }
        }

        bool startup_repair = false;
        status = normalizeStartupTipStates(
            db_->last_shutdown_was_clean(), &startup_repair, startup_summary, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        if (db_->clog() != nullptr)
        {
            uint64_t synchronized_count = 0;
            status = synchronizeStartupClogStateLocked(&synchronized_count, ctx);
            if (status != Status::OK)
            {
                return status;
            }
            if (startup_summary != nullptr)
            {
                startup_summary->clog_states_synchronized = synchronized_count;
                startup_summary->startup_repair =
                    startup_summary->startup_repair || synchronized_count > 0;
            }
        }

        uint64_t reconciled_oat = current_next_xid;
        if (!prepared_xids_.empty())
        {
            uint64_t prepared_min = UINT64_MAX;
            for (const auto &prepared_xid : prepared_xids_)
            {
                prepared_min = std::min(prepared_min, prepared_xid);
            }
            reconciled_oat = (prepared_min == UINT64_MAX) ? current_next_xid : prepared_min;
        }

        const uint64_t reconciled_ost = current_next_xid;
        const uint64_t reconciled_snapshot_serial = 0;
        const bool markers_changed =
            oldest_active_xid_ != reconciled_oat ||
            oldest_snapshot_ != reconciled_ost ||
            oldest_snapshot_serial_ != reconciled_snapshot_serial;

        oldest_active_xid_ = reconciled_oat;
        oldest_snapshot_ = reconciled_ost;
        oldest_snapshot_serial_ = reconciled_snapshot_serial;
        next_snapshot_serial_ = oldest_snapshot_serial_ + 1;

        if (!db_->last_shutdown_was_clean() || startup_repair)
        {
            ++inventory_generation_;
        }

        if (markers_changed || !db_->last_shutdown_was_clean() || startup_repair)
        {
            status = persistTransactionMarkersLocked(ctx);
            if (status != Status::OK)
            {
                return status;
            }
        }

        if (startup_summary != nullptr)
        {
            startup_summary->startup_repair =
                startup_summary->startup_repair || startup_repair;
        }

        status = restorePreparedLockOwners(ctx);
        if (status != Status::OK)
        {
            return status;
        }

        return Status::OK;
    }

    auto TransactionManager::restorePreparedLockOwners(ErrorContext *ctx) -> Status
    {
        CatalogManager *catalog = db_ ? db_->catalog_manager() : nullptr;
        LockManager *lock_mgr = db_ ? db_->lock_manager() : nullptr;
        if (catalog == nullptr || lock_mgr == nullptr)
        {
            return Status::OK;
        }

        std::vector<CatalogManager::PreparedTransactionInfo> prepared;
        Status status = catalog->listPreparedTransactions(prepared, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND)
        {
            return status;
        }

        for (auto &info : prepared)
        {
            uint32_t detached_proc_id = 0;
            status = ProcArrayManager::reserveDetachedPreparedOwner(&detached_proc_id, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            std::vector<CatalogManager::PreparedTransactionLockInfo> persisted_locks;
            status = catalog->listPreparedTransactionLocks(info.prepared_id, persisted_locks, ctx);
            if (status != Status::OK && status != Status::NOT_FOUND)
            {
                ErrorContext cleanup_ctx;
                ProcArrayManager::releaseDetachedPreparedOwner(detached_proc_id, &cleanup_ctx);
                return status;
            }

            if (info.lock_count != persisted_locks.size())
            {
                ErrorContext cleanup_ctx;
                ProcArrayManager::releaseDetachedPreparedOwner(detached_proc_id, &cleanup_ctx);
                SET_ERROR_CONTEXT_VNEXT(
                    ctx,
                    Status::PAGE_CORRUPT,
                    "TXN_0218",
                    "Prepared transaction lock snapshot count mismatch during startup restore");
                return Status::PAGE_CORRUPT;
            }

            for (const auto &persisted_lock : persisted_locks)
            {
                if (persisted_lock.target_type > static_cast<uint8_t>(LockTarget::LOCK_TARGET_TUPLE) ||
                    persisted_lock.mode < static_cast<uint8_t>(LockMode::LOCK_ACCESS_SHARE) ||
                    persisted_lock.mode > static_cast<uint8_t>(LockMode::LOCK_ACCESS_EXCLUSIVE))
                {
                    ErrorContext cleanup_ctx;
                    lock_mgr->releaseAllLocks(detached_proc_id, &cleanup_ctx);
                    ProcArrayManager::releaseDetachedPreparedOwner(detached_proc_id, &cleanup_ctx);
                    SET_ERROR_CONTEXT_VNEXT(
                        ctx,
                        Status::PAGE_CORRUPT,
                        "TXN_0218",
                        "Prepared transaction lock snapshot contains invalid target or mode");
                    return Status::PAGE_CORRUPT;
                }

                LockTag tag{};
                tag.target_type = static_cast<LockTarget>(persisted_lock.target_type);
                tag.object_uuid = persisted_lock.object_uuid;
                tag.page_num = persisted_lock.page_num;
                tag.offset_num = persisted_lock.offset_num;
                tag.padding = 0;

                status = lock_mgr->acquireLock(detached_proc_id,
                                               tag,
                                               static_cast<LockMode>(persisted_lock.mode),
                                               false,
                                               0,
                                               ctx);
                if (status != Status::OK)
                {
                    ErrorContext cleanup_ctx;
                    lock_mgr->releaseAllLocks(detached_proc_id, &cleanup_ctx);
                    ProcArrayManager::releaseDetachedPreparedOwner(detached_proc_id, &cleanup_ctx);
                    return status;
                }
            }

            info.lock_owner_proc_id = detached_proc_id;
            status = catalog->updatePreparedTransaction(info, ctx);
            if (status != Status::OK)
            {
                ErrorContext cleanup_ctx;
                lock_mgr->releaseAllLocks(detached_proc_id, &cleanup_ctx);
                ProcArrayManager::releaseDetachedPreparedOwner(detached_proc_id, &cleanup_ctx);
                return status;
            }
        }

        return Status::OK;
    }

    auto TransactionManager::loadTipPage(uint32_t page_id, ErrorContext *ctx) -> Status
    {
        // Load the TIP page
        void *page_buffer;
        Status status = buffer_pool_->pinPage(page_id, &page_buffer, ctx);
        if (status != Status::OK)
        {
            // TIP page should exist if tip_root_page_ is non-zero
            SET_ERROR_CONTEXT_VNEXT(ctx, status, "TXN_0215", "Failed to load TIP page");
            return status;
        }

        // Validate TIP page
        auto *tip_header = static_cast<TIPPageHeader *>(page_buffer);
        if (tip_header->page_header.page_type != PAGE_TYPE_TRANSACTION_MAP)
        {
            buffer_pool_->unpinPage(page_id, false, ctx);
            SET_ERROR_CONTEXT_VNEXT(ctx, Status::PAGE_CORRUPT, "TXN_0215",
                                    "Invalid page type for TIP page");
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

        // P1-2: Age-based wraparound protection (check OIT age first)
        Status age_check = checkXIDWraparound(ctx);
        if (age_check == Status::PAGE_CORRUPT)
        {
            // Hard block - age >= 2 billion
            return Status::PAGE_CORRUPT;
        }
        // Note: PAGE_FULL from age check is a warning but not blocking
        // We'll continue with transaction but have logged the critical state

        // WRAPAROUND PROTECTION: Check if approaching UINT64_MAX (absolute limit)
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
        // HIGH-5 FIX: Use memory_order_acq_rel instead of seq_cst for better performance
        // Acquire-release semantics are sufficient for XID allocation - we don't need
        // full sequential consistency. This provides the same correctness guarantees
        // (atomic increment + happens-before relationships) with lower overhead.
        uint64_t new_xid = next_xid_.fetch_add(1, std::memory_order_acq_rel);

        // Prevent wraparound to reserved XIDs (should never happen with above checks)
        uint64_t check_next = next_xid_.load(std::memory_order_acquire);
        if (check_next <= FROZEN_XID)
        {
            next_xid_.store(FROZEN_XID + 1, std::memory_order_release);
        }

        MgaFailpointManager* failpoints = db_ ? db_->mga_failpoint_manager() : nullptr;
        if (failpoints != nullptr)
        {
            MgaFailpointInvocation invocation{};
            invocation.has_txid = true;
            invocation.txid = new_xid;
            Status failpoint_status = failpoints->trip(
                MgaFailpointTriggers::kAfterTxidAllocationBeforeActive,
                invocation,
                ctx);
            if (failpoint_status != Status::OK)
            {
                return failpoint_status;
            }
        }

        // Persist ACTIVE state and next_xid advance before publishing the
        // transaction in attachment-visible inventory.
        Status status = writeTipEntry(new_xid, TransactionState::ACTIVE, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Persist IN_PROGRESS in CLOG to avoid "unknown == committed" after restart
        Status clog_status = db_->clog()->setStatus(new_xid, ClogStatus::IN_PROGRESS, ctx);
        if (clog_status != Status::OK)
        {
            // Best-effort mark as aborted in TIP to avoid dangling ACTIVE state
            writeTipEntry(new_xid, TransactionState::ABORTED, nullptr);
            return clog_status;
        }

        // Update database header with new next_xid (avoid XID reuse after restart)
        uint64_t current_next_xid_for_header = next_xid_.load(std::memory_order_acquire);
        if (db_ != nullptr)
        {
            db_->update_header_next_xid(current_next_xid_for_header, ctx);
        }

        status = flushTransactionState(ctx);
        if (status != Status::OK)
        {
            return status;
        }

        status = ProcArrayManager::setTransactionId(proc_id, new_xid, ctx);
        if (status != Status::OK)
        {
            writeTipEntry(new_xid, TransactionState::ABORTED, nullptr);
            db_->clog()->setStatus(new_xid, ClogStatus::ABORTED, nullptr);
            flushTransactionState(nullptr);
            return status;
        }

        addToCacheLRU(new_xid, TransactionState::ACTIVE);
        stats_.transactions_started++;

        xid_out = new_xid;
        return Status::OK;
    }

    auto TransactionManager::commitTransaction(uint32_t proc_id, uint64_t xid, ErrorContext *ctx)
        -> Status
    {
        TransactionState current_state = TransactionState::ACTIVE;
        Status state_status = getTransactionState(xid, current_state, ctx);
        if (state_status != Status::OK)
        {
            return state_status;
        }
        if (current_state != TransactionState::ACTIVE)
        {
            SET_ERROR_CONTEXT_VNEXT(ctx, Status::INVALID_ARGUMENT, "TXN_0201",
                                    "Commit requires ACTIVE transaction state");
            return Status::INVALID_ARGUMENT;
        }

        TIPEntry tip_entry{};
        Status tip_status = findTipEntry(xid, tip_entry, ctx);
        if (tip_status != Status::OK)
        {
            return tip_status;
        }

        TransactionState tip_state = TransactionState::ACTIVE;
        if (!decodeTipState(tip_entry.state, tip_state) || tip_state != TransactionState::ACTIVE)
        {
            SET_ERROR_CONTEXT_VNEXT(ctx, Status::INVALID_ARGUMENT, "TXN_0201",
                                    "Commit requires TIP ACTIVE state");
            return Status::INVALID_ARGUMENT;
        }

        Status status = flushTransactionState(ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT_VNEXT(ctx, status, "TXN_0216",
                                    "Commit fence flush failed before durable publish");
            return status;
        }

        MgaFailpointManager* failpoints = db_ ? db_->mga_failpoint_manager() : nullptr;
        if (failpoints != nullptr)
        {
            MgaFailpointInvocation invocation{};
            invocation.has_txid = true;
            invocation.txid = xid;
            status = failpoints->trip(
                MgaFailpointTriggers::kAfterDirtyFlushBeforeTipTerminal,
                invocation,
                ctx);
            if (status != Status::OK)
            {
                return status;
            }
        }

        // GROUP COMMIT OPTIMIZATION (Issue 2.19)
        if (group_commit_enabled_.load(std::memory_order_acquire))
        {
            // Thread-owned waiter: lifetime is tied to this transaction call.
            // Queue stores raw pointers protected by group_commit_mutex_.
            auto waiter = std::make_unique<CommitWaiter>(xid, TransactionState::COMMITTED);

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
                    commit_queue_.push_back(waiter.get());
                }
            }

            if (is_leader)
            {
                // Perform group commit as leader
                status = performGroupCommit(waiter.get(), ctx);

                // Mark group commit complete and handle any stragglers
                // There's a race window where followers can join the queue after
                // performGroupCommit's final drain but before we set group_commit_in_progress_ = false.
                // We must process those stragglers to avoid deadlock.
                std::vector<CommitWaiter*> stragglers;
                {
                    std::lock_guard<std::mutex> lock(group_commit_mutex_);
                    // Collect any stragglers that arrived after final drain
                    while (!commit_queue_.empty())
                    {
                        stragglers.push_back(commit_queue_.back());
                        commit_queue_.pop_back();
                    }
                    group_commit_in_progress_ = false;
                }

                // Process stragglers outside the lock
                // Stragglers have CLOG entries but NOT TIP entries - we must write TIP for them
                if (!stragglers.empty())
                {
                    // Build batch for TIP writes
                    std::vector<std::pair<uint64_t, TransactionState>> straggler_batch;
                    straggler_batch.reserve(stragglers.size());
                    for (const auto* straggler : stragglers)
                    {
                        straggler_batch.push_back({straggler->xid, straggler->state});
                    }

                    // Write TIP entries for stragglers
                    Status straggler_status = writeTipEntriesBatch(straggler_batch, ctx);
                    if (straggler_status == Status::OK)
                    {
                        straggler_status = db_->sync(ctx);
                    }

                    // Wake stragglers with result
                    for (auto* straggler : stragglers)
                    {
                        std::lock_guard<std::mutex> lock(straggler->cv_mutex);
                        straggler->result = straggler_status;
                        straggler->completed = true;
                        straggler->cv.notify_one();
                    }
                    // Update statistics to count stragglers
                    group_commit_total_xids_.fetch_add(stragglers.size(), std::memory_order_relaxed);
                }
            }
            else
            {
                // Wait for leader to complete
                std::unique_lock<std::mutex> lock(waiter->cv_mutex);
                waiter->cv.wait(lock, [&waiter] { return waiter->completed; });
                status = waiter->result;
            }
        }
        else
        {
            status = writeTipEntry(xid, TransactionState::COMMITTED, ctx);
            if (status == Status::OK)
            {
                status = db_->clog()->setStatus(xid, ClogStatus::COMMITTED, ctx);
            }
            if (status == Status::OK)
            {
                status = flushTransactionState(ctx);
            }
        }

        if (status != Status::OK)
        {
            return status;
        }

        // Clear ProcArray slot after durability guaranteed (Issue 1.14)
        Status clear_status = ProcArrayManager::clearTransactionId(proc_id, ctx);
        if (clear_status != Status::OK)
        {
            LOG_WARNING(TRANSACTION, "Failed to clear ProcArray slot for committed XID %lu", xid);
        }

        Status marker_status = updateTransactionMarkers(ctx);
        if (marker_status != Status::OK)
        {
            return marker_status;
        }

        status = flushTransactionState(ctx);
        if (status != Status::OK)
        {
            return status;
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
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
            stats_.transactions_committed++;
        }

        if (failpoints != nullptr)
        {
            MgaFailpointInvocation invocation{};
            invocation.has_txid = true;
            invocation.txid = xid;
            status = failpoints->trip(
                MgaFailpointTriggers::kAfterTipTerminalBeforeClientAck,
                invocation,
                ctx);
            if (status != Status::OK)
            {
                return status;
            }
        }

        // Check sweep trigger (non-blocking)
        if (status == Status::OK && db_->sweep_manager())
        {
            db_->sweep_manager()->checkSweepTrigger(ctx);
        }

        return status;
    }

    auto TransactionManager::prepareTransaction(uint32_t proc_id, uint64_t xid,
                                                const std::string& gid,
                                                const ID& owner_id,
                                                ErrorContext *ctx) -> Status
    {
        if (xid == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Cannot prepare transaction with XID 0");
            return Status::INVALID_ARGUMENT;
        }

        if (gid.empty())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Prepared transaction GID is required");
            return Status::INVALID_ARGUMENT;
        }

        TransactionState state = TransactionState::ACTIVE;
        Status status = getTransactionState(xid, state, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        if (state != TransactionState::ACTIVE)
        {
            SET_ERROR_CONTEXT_VNEXT(ctx, Status::INVALID_ARGUMENT, "TXN_0210",
                                    "Transaction is not active; cannot prepare");
            return Status::INVALID_ARGUMENT;
        }

        TIPEntry tip_entry{};
        status = findTipEntry(xid, tip_entry, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        TransactionState tip_state = TransactionState::ACTIVE;
        if (!decodeTipState(tip_entry.state, tip_state) || tip_state != TransactionState::ACTIVE)
        {
            SET_ERROR_CONTEXT_VNEXT(ctx, Status::INVALID_ARGUMENT, "TXN_0204",
                                    "Prepare requires TIP ACTIVE state");
            return Status::INVALID_ARGUMENT;
        }

        CatalogManager *catalog = db_->catalog_manager();
        LockManager *lock_mgr = db_ ? db_->lock_manager() : nullptr;
        if (!catalog)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Catalog manager not available");
            return Status::INVALID_ARGUMENT;
        }

        std::vector<CatalogManager::PreparedTransactionLockInfo> prepared_locks;
        if (lock_mgr != nullptr)
        {
            std::vector<LockSnapshot> locks;
            status = lock_mgr->listLocks(locks);
            if (status != Status::OK)
            {
                SET_ERROR_CONTEXT(ctx, status, "Failed to snapshot prepared transaction locks");
                return status;
            }

            for (const auto &lock : locks)
            {
                if (lock.proc_id != proc_id || !lock.granted)
                {
                    continue;
                }

                CatalogManager::PreparedTransactionLockInfo persisted_lock;
                persisted_lock.object_uuid = lock.tag.object_uuid;
                persisted_lock.page_num = lock.tag.page_num;
                persisted_lock.request_time = lock.request_time;
                persisted_lock.offset_num = lock.tag.offset_num;
                persisted_lock.target_type = static_cast<uint8_t>(lock.tag.target_type);
                persisted_lock.mode = static_cast<uint8_t>(lock.mode);
                persisted_lock.granted = lock.granted;
                persisted_lock.is_valid = true;
                prepared_locks.push_back(persisted_lock);
            }
        }

        CatalogManager::PreparedTransactionInfo info;
        info.txn_id = xid;
        info.gid = gid;
        info.owner_id = owner_id;
        info.database_id = db_->uuid();
        info.lock_owner_proc_id = proc_id;
        info.lock_count = static_cast<uint32_t>(prepared_locks.size());
        info.prepared_time = nowMicros();
        info.is_valid = true;

        status = catalog->createPreparedTransaction(info, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        status = catalog->createPreparedTransactionLocks(info.prepared_id, prepared_locks, ctx);
        if (status != Status::OK)
        {
            ErrorContext cleanup_ctx;
            catalog->deletePreparedTransaction(info.gid, &cleanup_ctx);
            catalog->deletePreparedTransactionLocks(info.prepared_id, &cleanup_ctx);
            return status;
        }

        status = flushTransactionState(ctx);
        if (status != Status::OK)
        {
            return status;
        }

        MgaFailpointManager* failpoints = db_ ? db_->mga_failpoint_manager() : nullptr;
        if (failpoints != nullptr)
        {
            MgaFailpointInvocation invocation{};
            invocation.has_txid = true;
            invocation.txid = xid;
            status = failpoints->trip(
                MgaFailpointTriggers::kBetweenPreparedRecordAndTipPrepared,
                invocation,
                ctx);
            if (status != Status::OK)
            {
                return status;
            }
        }

        status = writeTipEntry(xid, TransactionState::PREPARED, ctx);
        if (status == Status::OK)
        {
            status = db_->clog()->setStatus(xid, ClogStatus::PREPARED, ctx);
        }
        if (status == Status::OK)
        {
            status = flushTransactionState(ctx);
        }
        if (status != Status::OK)
        {
            ErrorContext cleanup_ctx;
            catalog->deletePreparedTransaction(gid, &cleanup_ctx);
            catalog->deletePreparedTransactionLocks(info.prepared_id, &cleanup_ctx);
            return status;
        }

        Status clear_status = ProcArrayManager::clearTransactionId(proc_id, ctx);
        if (clear_status != Status::OK)
        {
            LOG_WARNING(TRANSACTION, "Failed to clear ProcArray slot for prepared XID %lu", xid);
        }

        status = updateTransactionMarkers(ctx);
        if (status != Status::OK)
        {
            return status;
        }
        status = flushTransactionState(ctx);
        if (status != Status::OK)
        {
            return status;
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            prepared_xids_.insert(xid);
            auto cache_it = transaction_cache_.find(xid);
            if (cache_it != transaction_cache_.end())
            {
                cache_it->second = TransactionState::PREPARED;
                touchCacheEntry(xid);
            }
            else
            {
                addToCacheLRU(xid, TransactionState::PREPARED);
            }
        }

        return Status::OK;
    }

    auto TransactionManager::commitPreparedTransaction(const std::string& gid,
                                                       ErrorContext *ctx) -> Status
    {
        if (gid.empty())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Prepared transaction GID is required");
            return Status::INVALID_ARGUMENT;
        }

        CatalogManager *catalog = db_->catalog_manager();
        if (!catalog)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Catalog manager not available");
            return Status::INVALID_ARGUMENT;
        }

        CatalogManager::PreparedTransactionInfo info;
        Status status = catalog->getPreparedTransactionByGid(gid, info, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        TransactionState state = TransactionState::ACTIVE;
        status = getTransactionState(info.txn_id, state, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        if (state != TransactionState::PREPARED)
        {
            SET_ERROR_CONTEXT_VNEXT(ctx, Status::INVALID_ARGUMENT, "TXN_0212",
                                    "Prepared transaction is not in PREPARED state");
            return Status::INVALID_ARGUMENT;
        }

        TIPEntry tip_entry{};
        status = findTipEntry(info.txn_id, tip_entry, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        TransactionState tip_state = TransactionState::ACTIVE;
        if (!decodeTipState(tip_entry.state, tip_state) || tip_state != TransactionState::PREPARED)
        {
            SET_ERROR_CONTEXT_VNEXT(ctx, Status::INVALID_ARGUMENT, "TXN_0204",
                                    "Commit prepared requires TIP PREPARED state");
            return Status::INVALID_ARGUMENT;
        }

        status = writeTipEntry(info.txn_id, TransactionState::COMMITTED, ctx);
        if (status == Status::OK)
        {
            status = db_->clog()->setStatus(info.txn_id, ClogStatus::COMMITTED, ctx);
        }
        if (status == Status::OK)
        {
            status = flushTransactionState(ctx);
        }
        if (status != Status::OK)
        {
            return status;
        }

        LockManager *lock_mgr = db_ ? db_->lock_manager() : nullptr;
        Status delete_status = catalog->deletePreparedTransaction(gid, ctx);
        if (delete_status != Status::OK)
        {
            LOG_WARNING(TRANSACTION, "Failed to delete prepared transaction record: %s",
                        gid.c_str());
        }
        Status lock_snapshot_delete_status =
            catalog->deletePreparedTransactionLocks(info.prepared_id, ctx);
        if (lock_snapshot_delete_status != Status::OK &&
            lock_snapshot_delete_status != Status::NOT_FOUND)
        {
            LOG_WARNING(TRANSACTION,
                        "Failed to delete prepared transaction lock snapshots: gid=%s",
                        gid.c_str());
        }

        Status lock_status = Status::OK;
        if (lock_mgr)
        {
            lock_status = lock_mgr->releaseAllLocks(info.lock_owner_proc_id, ctx);
            if (lock_status != Status::OK)
            {
                LOG_WARNING(LOCK,
                            "Failed to release prepared transaction locks after commit: gid=%s owner_proc_id=%u",
                            gid.c_str(),
                            info.lock_owner_proc_id);
            }
        }

        Status owner_release_status = ProcArrayManager::releaseDetachedPreparedOwner(
            info.lock_owner_proc_id, ctx);
        if (owner_release_status != Status::OK)
        {
            LOG_WARNING(TRANSACTION,
                        "Failed to release detached prepared owner slot after commit: gid=%s owner_proc_id=%u",
                        gid.c_str(),
                        info.lock_owner_proc_id);
        }

        if (status != Status::OK)
        {
            return status;
        }

        status = updateTransactionMarkers(ctx);
        if (status != Status::OK)
        {
            return status;
        }
        status = flushTransactionState(ctx);
        if (status != Status::OK)
        {
            return status;
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            prepared_xids_.erase(info.txn_id);
            auto cache_it = transaction_cache_.find(info.txn_id);
            if (cache_it != transaction_cache_.end())
            {
                cache_it->second = TransactionState::COMMITTED;
                touchCacheEntry(info.txn_id);
            }
            else
            {
                addToCacheLRU(info.txn_id, TransactionState::COMMITTED);
            }
            stats_.transactions_committed++;
        }

        if (delete_status != Status::OK)
        {
            return delete_status;
        }
        if (lock_status != Status::OK)
        {
            return lock_status;
        }
        if (owner_release_status != Status::OK &&
            owner_release_status != Status::INVALID_ARGUMENT)
        {
            return owner_release_status;
        }

        return Status::OK;
    }

    auto TransactionManager::rollbackPreparedTransaction(const std::string& gid,
                                                         ErrorContext *ctx) -> Status
    {
        if (gid.empty())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Prepared transaction GID is required");
            return Status::INVALID_ARGUMENT;
        }

        CatalogManager *catalog = db_->catalog_manager();
        if (!catalog)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Catalog manager not available");
            return Status::INVALID_ARGUMENT;
        }

        CatalogManager::PreparedTransactionInfo info;
        Status status = catalog->getPreparedTransactionByGid(gid, info, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        TransactionState state = TransactionState::ACTIVE;
        status = getTransactionState(info.txn_id, state, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        if (state != TransactionState::PREPARED)
        {
            SET_ERROR_CONTEXT_VNEXT(ctx, Status::INVALID_ARGUMENT, "TXN_0212",
                                    "Prepared transaction is not in PREPARED state");
            return Status::INVALID_ARGUMENT;
        }

        TIPEntry tip_entry{};
        status = findTipEntry(info.txn_id, tip_entry, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        TransactionState tip_state = TransactionState::ACTIVE;
        if (!decodeTipState(tip_entry.state, tip_state) || tip_state != TransactionState::PREPARED)
        {
            SET_ERROR_CONTEXT_VNEXT(ctx, Status::INVALID_ARGUMENT, "TXN_0204",
                                    "Rollback prepared requires TIP PREPARED state");
            return Status::INVALID_ARGUMENT;
        }

        status = writeTipEntry(info.txn_id, TransactionState::ABORTED, ctx);
        if (status == Status::OK)
        {
            status = db_->clog()->setStatus(info.txn_id, ClogStatus::ABORTED, ctx);
        }
        if (status == Status::OK)
        {
            status = flushTransactionState(ctx);
        }
        if (status != Status::OK)
        {
            return status;
        }

        LockManager *lock_mgr = db_ ? db_->lock_manager() : nullptr;
        Status delete_status = catalog->deletePreparedTransaction(gid, ctx);
        if (delete_status != Status::OK)
        {
            LOG_WARNING(TRANSACTION, "Failed to delete prepared transaction record: %s",
                        gid.c_str());
        }
        Status lock_snapshot_delete_status =
            catalog->deletePreparedTransactionLocks(info.prepared_id, ctx);
        if (lock_snapshot_delete_status != Status::OK &&
            lock_snapshot_delete_status != Status::NOT_FOUND)
        {
            LOG_WARNING(TRANSACTION,
                        "Failed to delete prepared transaction lock snapshots: gid=%s",
                        gid.c_str());
        }

        Status lock_status = Status::OK;
        if (lock_mgr)
        {
            lock_status = lock_mgr->releaseAllLocks(info.lock_owner_proc_id, ctx);
            if (lock_status != Status::OK)
            {
                LOG_WARNING(LOCK,
                            "Failed to release prepared transaction locks after rollback: gid=%s owner_proc_id=%u",
                            gid.c_str(),
                            info.lock_owner_proc_id);
            }
        }

        Status owner_release_status = ProcArrayManager::releaseDetachedPreparedOwner(
            info.lock_owner_proc_id, ctx);
        if (owner_release_status != Status::OK)
        {
            LOG_WARNING(TRANSACTION,
                        "Failed to release detached prepared owner slot after rollback: gid=%s owner_proc_id=%u",
                        gid.c_str(),
                        info.lock_owner_proc_id);
        }

        if (status != Status::OK)
        {
            return status;
        }

        status = updateTransactionMarkers(ctx);
        if (status != Status::OK)
        {
            return status;
        }
        status = flushTransactionState(ctx);
        if (status != Status::OK)
        {
            return status;
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            prepared_xids_.erase(info.txn_id);
            auto cache_it = transaction_cache_.find(info.txn_id);
            if (cache_it != transaction_cache_.end())
            {
                cache_it->second = TransactionState::ABORTED;
                touchCacheEntry(info.txn_id);
            }
            else
            {
                addToCacheLRU(info.txn_id, TransactionState::ABORTED);
            }
            stats_.transactions_aborted++;
        }

        if (delete_status != Status::OK)
        {
            return delete_status;
        }
        if (lock_status != Status::OK)
        {
            return lock_status;
        }
        if (owner_release_status != Status::OK &&
            owner_release_status != Status::INVALID_ARGUMENT)
        {
            return owner_release_status;
        }

        return Status::OK;
    }

    auto TransactionManager::rollbackTransaction(uint32_t proc_id, uint64_t xid, ErrorContext *ctx)
        -> Status
    {
        TransactionState current_state = TransactionState::ACTIVE;
        Status state_status = getTransactionState(xid, current_state, ctx);
        if (state_status != Status::OK)
        {
            return state_status;
        }
        if (current_state != TransactionState::ACTIVE)
        {
            SET_ERROR_CONTEXT_VNEXT(ctx, Status::INVALID_ARGUMENT, "TXN_0201",
                                    "Rollback requires ACTIVE transaction state");
            return Status::INVALID_ARGUMENT;
        }

        TIPEntry tip_entry{};
        Status tip_status = findTipEntry(xid, tip_entry, ctx);
        if (tip_status != Status::OK)
        {
            return tip_status;
        }

        TransactionState tip_state = TransactionState::ACTIVE;
        if (!decodeTipState(tip_entry.state, tip_state) || tip_state != TransactionState::ACTIVE)
        {
            SET_ERROR_CONTEXT_VNEXT(ctx, Status::INVALID_ARGUMENT, "TXN_0201",
                                    "Rollback requires TIP ACTIVE state");
            return Status::INVALID_ARGUMENT;
        }

        Status status;

        // GROUP COMMIT OPTIMIZATION (Issue 2.19) - Applied to rollbacks for consistency
        if (group_commit_enabled_.load(std::memory_order_acquire))
        {
            // Thread-owned waiter: lifetime is tied to this transaction call.
            // Queue stores raw pointers protected by group_commit_mutex_.
            auto waiter = std::make_unique<CommitWaiter>(xid, TransactionState::ABORTED);

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
                    commit_queue_.push_back(waiter.get());
                }
            }

            if (is_leader)
            {
                // Perform group commit as leader (handles both commits and rollbacks)
                status = performGroupCommit(waiter.get(), ctx);

                // Mark group commit complete and handle any stragglers
                // There's a race window where followers can join the queue after
                // performGroupCommit's final drain but before we set group_commit_in_progress_ = false.
                // We must process those stragglers to avoid deadlock.
                std::vector<CommitWaiter*> stragglers;
                {
                    std::lock_guard<std::mutex> lock(group_commit_mutex_);
                    // Collect any stragglers that arrived after final drain
                    while (!commit_queue_.empty())
                    {
                        stragglers.push_back(commit_queue_.back());
                        commit_queue_.pop_back();
                    }
                    group_commit_in_progress_ = false;
                }

                // Process stragglers outside the lock
                // Stragglers have CLOG entries but NOT TIP entries - we must write TIP for them
                if (!stragglers.empty())
                {
                    // Build batch for TIP writes
                    std::vector<std::pair<uint64_t, TransactionState>> straggler_batch;
                    straggler_batch.reserve(stragglers.size());
                    for (const auto* straggler : stragglers)
                    {
                        straggler_batch.push_back({straggler->xid, straggler->state});
                    }

                    // Write TIP entries for stragglers
                    Status straggler_status = writeTipEntriesBatch(straggler_batch, ctx);
                    if (straggler_status == Status::OK)
                    {
                        straggler_status = db_->sync(ctx);
                    }

                    // Wake stragglers with result
                    for (auto* straggler : stragglers)
                    {
                        std::lock_guard<std::mutex> lock(straggler->cv_mutex);
                        straggler->result = straggler_status;
                        straggler->completed = true;
                        straggler->cv.notify_one();
                    }
                    // Update statistics to count stragglers
                    group_commit_total_xids_.fetch_add(stragglers.size(), std::memory_order_relaxed);
                }
            }
            else
            {
                // Wait for leader to complete
                std::unique_lock<std::mutex> lock(waiter->cv_mutex);
                waiter->cv.wait(lock, [&waiter] { return waiter->completed; });
                status = waiter->result;
            }
        }
        else
        {
            status = writeTipEntry(xid, TransactionState::ABORTED, ctx);
            if (status == Status::OK)
            {
                status = db_->clog()->setStatus(xid, ClogStatus::ABORTED, ctx);
            }
            if (status == Status::OK)
            {
                status = flushTransactionState(ctx);
            }
        }

        if (status != Status::OK)
        {
            return status;
        }

        // Clear ProcArray slot after durability guaranteed (Issue 1.14)
        Status clear_status = ProcArrayManager::clearTransactionId(proc_id, ctx);
        if (clear_status != Status::OK)
        {
            LOG_WARNING(TRANSACTION, "Failed to clear ProcArray slot for aborted XID %lu", xid);
        }

        status = updateTransactionMarkers(ctx);
        if (status != Status::OK)
        {
            return status;
        }
        status = flushTransactionState(ctx);
        if (status != Status::OK)
        {
            return status;
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
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
            stats_.transactions_aborted++;
        }

        return Status::OK;
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

        // TIP is authoritative transaction truth.
        TIPEntry tip_entry{};
        Status status = findTipEntry(xid, tip_entry, ctx);
        if (status == Status::OK)
        {
            TransactionState tip_state = TransactionState::ACTIVE;
            if (!decodeTipState(tip_entry.state, tip_state))
            {
                SET_ERROR_CONTEXT_VNEXT(ctx, Status::PAGE_CORRUPT, "TXN_0215",
                                        "Invalid TIP state during transaction lookup");
                return Status::PAGE_CORRUPT;
            }
            addToCacheLRU(xid, tip_state);
            state_out = tip_state;
            return Status::OK;
        }
        if (status != Status::NOT_FOUND)
        {
            return status;
        }

        // TIP entry absent: fall back to CLOG only as a compatibility accelerator.
        ClogStatus clog_status;
        status = db_->clog()->getStatus(xid, &clog_status, ctx);
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
            case ClogStatus::PREPARED:
                state_out = TransactionState::PREPARED;
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

        if (xid == config::DEFAULT_INITIAL_XID)
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
            // Allow visibility checks to proceed; treat as in-range to avoid false invisibility
            return true;
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
        db_header->page_header.checksum =
            calculatePageChecksum(reinterpret_cast<uint8_t *>(db_header), db_->page_size());

        buffer_pool_->unpinPage(0, true, ctx);

        return Status::OK;
    }

    auto TransactionManager::findOldestInterestingXidFromInventory(uint64_t &xid_out,
                                                                   ErrorContext *ctx) const
        -> Status
    {
        (void)ctx;

        uint64_t start_xid = 0;
        uint64_t end_xid_exclusive = 0;
        uint32_t tip_root_page = 0;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            start_xid = oldest_xid_;
            end_xid_exclusive = next_xid_.load(std::memory_order_acquire);
            tip_root_page = tip_root_page_;
        }

        if (start_xid <= FROZEN_XID)
        {
            start_xid = FROZEN_XID + 1;
        }

        if (end_xid_exclusive <= start_xid)
        {
            xid_out = end_xid_exclusive;
            return Status::OK;
        }

        uint64_t expected_xid = start_xid;
        uint32_t current_page = tip_root_page;

        while (current_page != 0 && expected_xid < end_xid_exclusive)
        {
            void *page_buffer = nullptr;
            ErrorContext local_ctx;
            Status status = buffer_pool_->pinPage(current_page, &page_buffer, &local_ctx);
            if (status != Status::OK)
            {
                LOG_WARNING(
                    TRANSACTION,
                    "TIP horizon walk could not pin page %u while resolving xid=%lu; holding OIT conservatively (%s)",
                    current_page,
                    expected_xid,
                    local_ctx.message.c_str());
                xid_out = expected_xid;
                return Status::OK;
            }

            auto *tip_header = static_cast<TIPPageHeader *>(page_buffer);
            if (tip_header->page_header.page_type != PAGE_TYPE_TRANSACTION_MAP)
            {
                buffer_pool_->unpinPage(current_page, false, nullptr);
                LOG_WARNING(TRANSACTION,
                            "TIP horizon walk encountered non-TIP page %u while resolving xid=%lu; holding OIT conservatively",
                            current_page,
                            expected_xid);
                xid_out = expected_xid;
                return Status::OK;
            }

            const uint32_t next_page = tip_header->next_tip_page;
            auto *entries = reinterpret_cast<TIPEntry *>(reinterpret_cast<uint8_t *>(page_buffer) +
                                                         sizeof(TIPPageHeader));

            if (tip_header->num_transactions == 0)
            {
                buffer_pool_->unpinPage(current_page, false, nullptr);
                current_page = next_page;
                continue;
            }

            if (tip_header->max_xid < expected_xid)
            {
                buffer_pool_->unpinPage(current_page, false, nullptr);
                current_page = next_page;
                continue;
            }

            if (tip_header->min_xid != 0 && expected_xid < tip_header->min_xid)
            {
                buffer_pool_->unpinPage(current_page, false, nullptr);
                LOG_WARNING(TRANSACTION,
                            "TIP horizon walk found uncovered XID gap before page %u (expected=%lu min=%lu); holding OIT conservatively",
                            current_page,
                            expected_xid,
                            tip_header->min_xid);
                xid_out = expected_xid;
                return Status::OK;
            }

            for (uint32_t i = 0; i < tip_header->num_transactions; ++i)
            {
                const uint64_t entry_xid = entries[i].xid;
                if (entry_xid < expected_xid)
                {
                    continue;
                }

                if (entry_xid >= end_xid_exclusive)
                {
                    buffer_pool_->unpinPage(current_page, false, nullptr);
                    xid_out = end_xid_exclusive;
                    return Status::OK;
                }

                if (entry_xid > expected_xid)
                {
                    buffer_pool_->unpinPage(current_page, false, nullptr);
                    LOG_WARNING(TRANSACTION,
                                "TIP horizon walk found missing inventory coverage at xid=%lu before page %u entry xid=%lu; holding OIT conservatively",
                                expected_xid,
                                current_page,
                                entry_xid);
                    xid_out = expected_xid;
                    return Status::OK;
                }

                TransactionState tip_state = TransactionState::ACTIVE;
                if (!decodeTipState(entries[i].state, tip_state))
                {
                    buffer_pool_->unpinPage(current_page, false, nullptr);
                    LOG_WARNING(TRANSACTION,
                                "TIP horizon walk found invalid state byte for xid=%lu on page %u; holding OIT conservatively",
                                entry_xid,
                                current_page);
                    xid_out = expected_xid;
                    return Status::OK;
                }

                if (tip_state != TransactionState::COMMITTED &&
                    tip_state != TransactionState::ABORTED)
                {
                    buffer_pool_->unpinPage(current_page, false, nullptr);
                    xid_out = entry_xid;
                    return Status::OK;
                }

                expected_xid = entry_xid + 1;
            }

            buffer_pool_->unpinPage(current_page, false, nullptr);
            current_page = next_page;
        }

        xid_out = (expected_xid < end_xid_exclusive) ? expected_xid : end_xid_exclusive;
        return Status::OK;
    }

    auto TransactionManager::captureReclaimHorizons(ReclaimHorizonSnapshot &snapshot_out,
                                                    ErrorContext *ctx) const -> Status
    {
        (void)ctx;

        std::lock_guard<std::mutex> lock(mutex_);

        snapshot_out.oldest_interesting_xid = oldest_xid_;
        snapshot_out.oldest_active_xid = oldest_active_xid_;
        snapshot_out.oldest_snapshot_xid = oldest_snapshot_;
        snapshot_out.inventory_generation = inventory_generation_;

        const uint64_t next_xid = next_xid_.load(std::memory_order_acquire);
        snapshot_out.current_xid =
            (next_xid <= config::DEFAULT_INITIAL_XID) ? config::DEFAULT_INITIAL_XID : (next_xid - 1);

        auto fallbackToCurrentXid = [&snapshot_out](uint64_t xid) -> uint64_t {
            if (xid != 0)
            {
                return xid;
            }
            if (snapshot_out.current_xid != 0)
            {
                return snapshot_out.current_xid;
            }
            return UINT64_MAX;
        };

        snapshot_out.heap_reclaim_horizon =
            fallbackToCurrentXid(snapshot_out.oldest_snapshot_xid);

        if (snapshot_out.oldest_active_xid != 0 && snapshot_out.oldest_snapshot_xid != 0)
        {
            snapshot_out.toast_reclaim_horizon =
                std::min(snapshot_out.oldest_active_xid, snapshot_out.oldest_snapshot_xid);
        }
        else if (snapshot_out.oldest_active_xid != 0)
        {
            snapshot_out.toast_reclaim_horizon = snapshot_out.oldest_active_xid;
        }
        else
        {
            snapshot_out.toast_reclaim_horizon =
                fallbackToCurrentXid(snapshot_out.oldest_snapshot_xid);
        }

        return Status::OK;
    }

    auto TransactionManager::checkXIDWraparound(ErrorContext *ctx) -> Status
    {
        // P1-2: Age-based XID wraparound prevention (Firebird MGA style)
        // Calculate transaction age: distance between next XID and oldest interesting transaction
        // LOCKING: Caller must hold mutex_

        uint64_t next_xid = next_xid_.load(std::memory_order_acquire);
        uint64_t oit = oldest_xid_;

        // Calculate age (wraparound-safe subtraction)
        uint64_t age = next_xid - oit;

        // WARNING threshold: 1 billion XIDs
        // This is informational - database can continue
        if (age > 1'000'000'000ULL && age <= 1'800'000'000ULL)
        {
            LOG_WARNING(VACUUM,
                        "XID age is %lu (OIT=%lu, NEXT=%lu) - consider running VACUUM to advance OIT",
                        age, oit, next_xid);
            // Continue - not blocking
        }

        // CRITICAL threshold: 1.8 billion XIDs (90% of 2 billion)
        // Force emergency sweep to advance OIT
        else if (age > 1'800'000'000ULL && age < 2'000'000'000ULL)
        {
            LOG_ERROR(VACUUM,
                      "XID age is %lu (OIT=%lu, NEXT=%lu) - XID wraparound imminent! Forcing "
                      "emergency sweep...",
                      age, oit, next_xid);

            // Try to trigger emergency sweep
            // Note: We don't block here, but we strongly recommend sweep
            if (db_ && db_->sweep_manager())
            {
                // Check if sweep is already running
                if (!db_->sweep_manager()->isSweepInProgress())
                {
                    LOG_INFO(VACUUM, "Triggering emergency background sweep to advance OIT");
                    // Execute background sweep (doesn't reclaim space, just advances OIT)
                    // This should be fast
                    Status sweep_status = db_->sweep_manager()->executeSweep(false, ctx);
                    if (sweep_status == Status::OK)
                    {
                        LOG_INFO(VACUUM, "Emergency sweep completed successfully");
                    }
                    else
                    {
                        LOG_ERROR(VACUUM,
                                  "Emergency sweep failed with status %d - manual VACUUM required",
                                  static_cast<int>(sweep_status));
                    }
                }
                else
                {
                    LOG_INFO(VACUUM, "Sweep already in progress");
                }
            }

            // Return PAGE_FULL to signal critical condition (but allow transaction to proceed)
            SET_ERROR_CONTEXT(ctx, Status::PAGE_FULL,
                              "XID wraparound critical - emergency sweep triggered, OIT must advance");
            return Status::PAGE_FULL;
        }

        // PREVENT threshold: 2 billion XIDs
        // Absolutely block new transactions until OIT advances
        else if (age >= 2'000'000'000ULL)
        {
            LOG_ERROR(VACUUM,
                      "XID age is %lu (OIT=%lu, NEXT=%lu) - DATABASE LOCKED! VACUUM must complete "
                      "before new transactions allowed",
                      age, oit, next_xid);

            SET_ERROR_CONTEXT(
                ctx, Status::PAGE_CORRUPT,
                "XID wraparound blocked - age >= 2 billion. VACUUM must complete to advance OIT");
            return Status::PAGE_CORRUPT;
        }

        // Safe - age is acceptable
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
        uint64_t new_oat = current_next_xid;
        uint64_t new_ost = current_next_xid;
        uint64_t new_oldest_snapshot_serial = 0;
        uint64_t prepared_oat = current_next_xid;
        bool has_prepared = false;

        for (const auto &prepared_xid : prepared_xids_)
        {
            if (prepared_xid != 0 && prepared_xid < prepared_oat)
            {
                prepared_oat = prepared_xid;
                has_prepared = true;
            }
        }

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

            if (pcb->xid < new_oat)
            {
                new_oat = pcb->xid;
            }

            if (pcb->backend_xmin != 0 && pcb->backend_xmin < new_ost)
            {
                new_ost = pcb->backend_xmin;
            }
        }

        pthread_rwlock_unlock(&proc_array->array_lock);

        if (has_prepared)
        {
            if (prepared_oat < new_oat)
            {
                new_oat = prepared_oat;
            }
        }

        for (const auto &entry : snapshot_pins_)
        {
            const SnapshotPin &pin = entry.second;
            if (pin.xmin != 0 && pin.xmin < new_ost)
            {
                new_ost = pin.xmin;
            }
            if (pin.snapshot_serial != 0 &&
                (new_oldest_snapshot_serial == 0 || pin.snapshot_serial < new_oldest_snapshot_serial))
            {
                new_oldest_snapshot_serial = pin.snapshot_serial;
            }
        }

        // Update in-memory markers
        oldest_active_xid_ = new_oat;
        oldest_snapshot_ = new_ost;
        oldest_snapshot_serial_ = new_oldest_snapshot_serial;

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
        db_header->inventory_generation = inventory_generation_;
        db_header->oldest_snapshot_serial = oldest_snapshot_serial_;
        db_header->page_header.checksum =
            calculatePageChecksum(reinterpret_cast<uint8_t *>(db_header), db_->page_size());

        buffer_pool_->unpinPage(0, true, ctx);

        return Status::OK;
    }

    auto TransactionManager::isTransactionVisible(uint64_t xid, uint64_t current_xid) -> bool
    {
        return evaluateTransactionVisibility(
                   xid, current_xid, VisibilityMode::READ_CURRENT_TRANSACTION, nullptr)
            .visible;
    }

    auto TransactionManager::captureSnapshot(TransactionSnapshot &snapshot_out, ErrorContext *ctx)
        -> Status
    {
        snapshot_out.active_txid_set.clear();
        snapshot_out.snapshot_txid_low = 0;
        snapshot_out.snapshot_serial = 0;
        snapshot_out.capture_time = nowMicros();

        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_out.snapshot_txid_high = next_xid_.load(std::memory_order_acquire);
        snapshot_out.snapshot_commit_seqno_high = 0;
        snapshot_out.snapshot_serial = next_snapshot_serial_++;

        ProcArray *proc_array = ProcArrayManager::getInstance();
        if (proc_array)
        {
            auto snapshotIncludesPublishedXid = [&](uint64_t xid) -> bool
            {
                TIPEntry tip_entry{};
                Status tip_status = findTipEntry(xid, tip_entry, nullptr);
                if (tip_status != Status::OK)
                {
                    return false;
                }

                TransactionState tip_state = TransactionState::ACTIVE;
                if (!decodeTipState(tip_entry.state, tip_state))
                {
                    return false;
                }

                if (tip_state == TransactionState::ACTIVE ||
                    tip_state == TransactionState::PREPARED)
                {
                    auto cache_it = transaction_cache_.find(xid);
                    if (cache_it != transaction_cache_.end())
                    {
                        cache_it->second = tip_state;
                        touchCacheEntry(xid);
                    }
                    else
                    {
                        addToCacheLRU(xid, tip_state);
                    }
                    return true;
                }

                auto cache_it = transaction_cache_.find(xid);
                if (cache_it != transaction_cache_.end())
                {
                    cache_it->second = tip_state;
                    touchCacheEntry(xid);
                }
                else
                {
                    addToCacheLRU(xid, tip_state);
                }
                return false;
            };

            pthread_rwlock_rdlock(&proc_array->array_lock);
            auto *pcbs = reinterpret_cast<ProcessControlBlock *>(
                reinterpret_cast<uint8_t *>(proc_array) + sizeof(ProcArray));
            for (uint32_t i = 0; i < proc_array->max_backends; ++i)
            {
                const ProcessControlBlock &pcb = pcbs[i];
                if (!pcb.is_active || pcb.xid == 0 || pcb.xid >= snapshot_out.snapshot_txid_high)
                {
                    continue;
                }
                if (!snapshotIncludesPublishedXid(pcb.xid))
                {
                    continue;
                }
                snapshot_out.active_txid_set.push_back(pcb.xid);
            }
            pthread_rwlock_unlock(&proc_array->array_lock);
        }

        for (const auto &prepared_xid : prepared_xids_)
        {
            if (prepared_xid != 0 && prepared_xid < snapshot_out.snapshot_txid_high)
            {
                snapshot_out.active_txid_set.push_back(prepared_xid);
            }
        }

        std::sort(snapshot_out.active_txid_set.begin(), snapshot_out.active_txid_set.end());
        snapshot_out.active_txid_set.erase(
            std::unique(snapshot_out.active_txid_set.begin(), snapshot_out.active_txid_set.end()),
            snapshot_out.active_txid_set.end());

        snapshot_out.snapshot_txid_low = snapshot_out.snapshot_txid_high;
        if (!snapshot_out.active_txid_set.empty())
        {
            snapshot_out.snapshot_txid_low = snapshot_out.active_txid_set.front();
        }

        return Status::OK;
    }

    auto TransactionManager::registerSnapshotPin(uint32_t proc_id, uint64_t owner_xid,
                                                 const TransactionSnapshot &snapshot,
                                                 ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ProcArray *proc_array = ProcArrayManager::getInstance();
        if (!proc_array)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "ProcArray not initialized");
            return Status::INVALID_ARGUMENT;
        }

        auto *pcbs = reinterpret_cast<ProcessControlBlock *>(
            reinterpret_cast<uint8_t *>(proc_array) + sizeof(ProcArray));
        if (proc_id >= proc_array->max_backends)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid backend slot");
            return Status::INVALID_ARGUMENT;
        }

        pthread_rwlock_wrlock(&proc_array->array_lock);
        ProcessControlBlock *pcb = &pcbs[proc_id];
        if (!pcb->is_active)
        {
            pthread_rwlock_unlock(&proc_array->array_lock);
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Inactive backend slot");
            return Status::INVALID_ARGUMENT;
        }
        pcb->backend_xmin = snapshot.snapshot_txid_low;
        pthread_rwlock_unlock(&proc_array->array_lock);

        snapshot_pins_[proc_id] = SnapshotPin{
            owner_xid,
            snapshot.snapshot_txid_low,
            snapshot.snapshot_txid_high,
            snapshot.snapshot_serial,
        };
        return Status::OK;
    }

    auto TransactionManager::clearSnapshotPin(uint32_t proc_id, ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ProcArray *proc_array = ProcArrayManager::getInstance();
        if (proc_array && proc_id < proc_array->max_backends)
        {
            auto *pcbs = reinterpret_cast<ProcessControlBlock *>(
                reinterpret_cast<uint8_t *>(proc_array) + sizeof(ProcArray));
            pthread_rwlock_wrlock(&proc_array->array_lock);
            if (pcbs[proc_id].is_active)
            {
                pcbs[proc_id].backend_xmin = 0;
            }
            pthread_rwlock_unlock(&proc_array->array_lock);
        }
        else if (proc_array == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "ProcArray not initialized");
            return Status::INVALID_ARGUMENT;
        }
        snapshot_pins_.erase(proc_id);
        return Status::OK;
    }

    auto TransactionManager::isCreateVisibleInSnapshot(uint64_t create_xid, uint64_t reader_xid,
                                                       const TransactionSnapshot &snapshot)
        -> bool
    {
        return evaluateTransactionVisibility(
                   create_xid, reader_xid, VisibilityMode::SNAPSHOT, &snapshot)
            .visible;
    }

    auto TransactionManager::isRecordVersionVisibleInSnapshot(uint64_t create_xid,
                                                              uint64_t delete_xid,
                                                              uint64_t reader_xid,
                                                              const TransactionSnapshot &snapshot)
        -> bool
    {
        return evaluateRecordVisibility(
                   create_xid, delete_xid, reader_xid, VisibilityMode::SNAPSHOT, &snapshot)
            .visible;
    }

    auto TransactionManager::isVersionVisible(uint64_t version_xid, uint64_t reader_xid) -> bool
    {
        return evaluateTransactionVisibility(
                   version_xid, reader_xid, VisibilityMode::READ_CURRENT_VERSION, nullptr)
            .visible;
    }

    auto TransactionManager::evaluateTransactionVisibility(uint64_t xid, uint64_t reader_xid,
                                                           VisibilityMode mode,
                                                           const TransactionSnapshot *snapshot)
        -> TransactionVisibilityDecision
    {
        TransactionVisibilityDecision decision{};
        decision.mode = mode;

        if (mode == VisibilityMode::SNAPSHOT && snapshot == nullptr)
        {
            decision.reason = VisibilityReason::MISSING_SNAPSHOT;
            return decision;
        }

        if (!isXidInRange(xid))
        {
            decision.reason = VisibilityReason::INVALID_XID;
            return decision;
        }

        if (xid == reader_xid)
        {
            decision.visible = true;
            decision.reason = VisibilityReason::OWN_TRANSACTION;
            return decision;
        }

        if (xid <= FROZEN_XID)
        {
            decision.visible = true;
            decision.state = TransactionState::COMMITTED;
            decision.reason = VisibilityReason::FROZEN_XID;
            return decision;
        }

        if (mode == VisibilityMode::SNAPSHOT)
        {
            if (xid >= snapshot->snapshot_txid_high)
            {
                decision.reason = VisibilityReason::ABOVE_SNAPSHOT_HIGH;
                return decision;
            }
        }
        else if (xid > reader_xid)
        {
            decision.reason = VisibilityReason::FUTURE_XID;
            return decision;
        }

        TransactionState state = TransactionState::ACTIVE;
        Status status = getTransactionState(xid, state, nullptr);
        if (status != Status::OK)
        {
            if (mode == VisibilityMode::READ_CURRENT_TRANSACTION && xid < reader_xid)
            {
                decision.visible = true;
                decision.state = TransactionState::COMMITTED;
                decision.reason = VisibilityReason::STATE_LOOKUP_ASSUMED_COMMITTED;
                return decision;
            }

            decision.reason = VisibilityReason::STATE_LOOKUP_FAILED;
            return decision;
        }

        decision.state = state;

        if (mode == VisibilityMode::SNAPSHOT && snapshotHasActiveXid(*snapshot, xid))
        {
            decision.reason = VisibilityReason::ACTIVE_IN_SNAPSHOT;
            return decision;
        }

        switch (state)
        {
            case TransactionState::COMMITTED:
                decision.visible = true;
                decision.reason = VisibilityReason::COMMITTED_VISIBLE;
                break;

            case TransactionState::ACTIVE:
                decision.reason = VisibilityReason::ACTIVE_INVISIBLE;
                break;

            case TransactionState::ABORTED:
                decision.reason = VisibilityReason::ABORTED_INVISIBLE;
                break;

            case TransactionState::PREPARED:
                decision.reason = VisibilityReason::PREPARED_INVISIBLE;
                break;
        }

        return decision;
    }

    auto TransactionManager::evaluateRecordVisibility(uint64_t create_xid, uint64_t delete_xid,
                                                      uint64_t reader_xid, VisibilityMode mode,
                                                      const TransactionSnapshot *snapshot)
        -> RecordVisibilityDecision
    {
        RecordVisibilityDecision decision{};
        decision.mode = mode;
        decision.create_decision =
            evaluateTransactionVisibility(create_xid, reader_xid, mode, snapshot);
        decision.create_visible = decision.create_decision.visible;

        if (delete_xid == 0)
        {
            decision.delete_decision.mode = mode;
            decision.delete_decision.state = TransactionState::COMMITTED;
            decision.delete_decision.reason = VisibilityReason::DELETE_NOT_PRESENT;
            decision.delete_visible = false;
        }
        else
        {
            decision.delete_decision =
                evaluateTransactionVisibility(delete_xid, reader_xid, mode, snapshot);
            decision.delete_visible = decision.delete_decision.visible;
        }

        decision.visible = decision.create_visible && !decision.delete_visible;
        return decision;
    }

    auto TransactionManager::evaluateReadConsistencyRestart(
        Status conflict_status, uint64_t reader_xid, bool statement_scope_active,
        bool forensic_replay_active, const TransactionSnapshot *statement_snapshot,
        const LockTag& resource_tag, LockMode requested_mode, uint32_t blocker_proc_id,
        LockMode blocker_mode) const -> StatementRestartDecision
    {
        StatementRestartDecision decision{};
        decision.source_status = conflict_status;
        decision.reader_xid = reader_xid;
        decision.resource_tag = resource_tag;
        decision.requested_mode = requested_mode;
        decision.blocker_proc_id = blocker_proc_id;
        decision.blocker_mode = blocker_mode;
        if (statement_snapshot != nullptr)
        {
            decision.statement_snapshot_serial = statement_snapshot->snapshot_serial;
        }

        if (forensic_replay_active)
        {
            decision.reason = StatementRestartReason::FORENSIC_REPLAY_ACTIVE;
            return decision;
        }

        if (!statement_scope_active)
        {
            decision.reason = StatementRestartReason::INACTIVE_STATEMENT_SCOPE;
            return decision;
        }

        if (statement_snapshot == nullptr)
        {
            decision.reason = StatementRestartReason::MISSING_STATEMENT_SNAPSHOT;
            return decision;
        }

        switch (conflict_status)
        {
            case Status::LOCK_CONFLICT:
                decision.restart_required = true;
                decision.retry_eligible = true;
                decision.reason = StatementRestartReason::TUPLE_WRITE_CONFLICT;
                break;

            case Status::LOCK_TIMEOUT:
                decision.restart_required = true;
                decision.retry_eligible = true;
                decision.reason = StatementRestartReason::LOCK_TIMEOUT;
                break;

            case Status::DEADLOCK:
                decision.restart_required = true;
                decision.retry_eligible = true;
                decision.reason = StatementRestartReason::DEADLOCK_DETECTED;
                break;

            default:
                decision.reason = StatementRestartReason::UNSUPPORTED_CONFLICT_STATUS;
                break;
        }

        return decision;
    }

    auto TransactionManager::statementRestartReasonName(StatementRestartReason reason)
        -> const char *
    {
        switch (reason)
        {
            case StatementRestartReason::NONE:
                return "none";
            case StatementRestartReason::TUPLE_WRITE_CONFLICT:
                return "tuple_write_conflict";
            case StatementRestartReason::LOCK_TIMEOUT:
                return "lock_timeout";
            case StatementRestartReason::DEADLOCK_DETECTED:
                return "deadlock_detected";
            case StatementRestartReason::MISSING_STATEMENT_SNAPSHOT:
                return "missing_statement_snapshot";
            case StatementRestartReason::INACTIVE_STATEMENT_SCOPE:
                return "inactive_statement_scope";
            case StatementRestartReason::FORENSIC_REPLAY_ACTIVE:
                return "forensic_replay_active";
            case StatementRestartReason::UNSUPPORTED_CONFLICT_STATUS:
                return "unsupported_conflict_status";
        }

        return "unknown";
    }

    auto TransactionManager::formatStatementRestartMessage(
        const StatementRestartDecision& decision) -> std::string
    {
        std::string message = "READ_CONSISTENCY_RESTART_REQUIRED: reason=";
        message += statementRestartReasonName(decision.reason);
        message += " blocker_proc_id=" + std::to_string(decision.blocker_proc_id);
        message += " requested_mode=" +
            std::string(lockModeNameLocal(decision.requested_mode));
        message += " blocker_mode=" + std::string(lockModeNameLocal(decision.blocker_mode));
        message += " resource=" + formatLockResourceIdLocal(decision.resource_tag);
        message += " snapshot_serial=" + std::to_string(decision.statement_snapshot_serial);
        message += " retry_eligible=";
        message += decision.retry_eligible ? "true" : "false";
        return message;
    }

    auto TransactionManager::getBackendXid(uint32_t proc_id) const -> uint64_t
    {
        // Use ProcArray API to safely get backend XID
        uint64_t xid = 0;
        ProcArrayManager::getBackendXid(proc_id, &xid, nullptr);
        return xid;
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
        if (platform::seekFd(db_->fd(), offset, SEEK_SET) < 0 ||
            write(db_->fd(), new_page.get(), db_->page_size()) !=
                static_cast<ssize_t>(db_->page_size()))
        {
            page_manager_->freePage(page_id_out, ctx);
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to write TIP page");
            return Status::IO_ERROR;
        }

        // Sync to ensure page is on disk before BufferPool reads it
        platform::syncFd(db_->fd());

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
        tip_header->page_header.generation = 1;
        tip_header->page_header.checksum = 0;
        tip_header->page_header.flags = 0;
        tip_header->page_header.lsn = 0;
        pageSetLower(tip_header->page_header, sizeof(TIPPageHeader));
        pageSetUpper(tip_header->page_header, db_->page_size());
        pageSetSpecial(tip_header->page_header, db_->page_size());

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
        // TIP mutations are serialized to avoid concurrent page updates across
        // commit/rollback/job paths that can touch the same TIP chain.
        std::lock_guard<std::mutex> tip_guard(tip_io_mutex_);

        // ===========================================================================================
        // ISSUE 3.1: OPTIMIZE TIP PAGE SCAN
        // ===========================================================================================
        //
        // Use TIP location cache:
        // Maps XID -> TIP page ID to avoid scanning entire TIP chain.
        // Cache is populated when we find/create an entry.
        //
        // Performance impact:
        // - Before: O(N) scan through all TIP pages and entries (worst case: thousands of pages)
        // - After: O(1) cache lookup + single page pin (best case: cache hit)
        // - Expected speedup: 10-100x for transactions with multiple state updates
        //
        // See: docs/audit/ISSUE_3_1_STATUS.md for complete analysis
        // ===========================================================================================

        // OPTIMIZATION: Check TIP location cache for known page (best-effort)
        uint32_t start_page = tip_root_page_;
        bool has_cached_page = false;
        {
            std::lock_guard<std::mutex> lock(tip_cache_mutex_);
            auto tip_cache_it = tip_location_cache_.find(xid);
            if (tip_cache_it != tip_location_cache_.end())
            {
                start_page = tip_cache_it->second;
                has_cached_page = true;
            }
        }
        if (has_cached_page)
        {
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

                    // P1-7: Use binary search instead of linear search (O(log N) vs O(N))
                    int32_t idx = binarySearchTIPEntries(entries, tip_header->num_transactions, xid);
                    if (idx >= 0)
                    {
                        // FAST PATH: Found entry on cached page - update it
                        entries[idx].state = static_cast<uint8_t>(state);
                        entries[idx].commit_time =
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

                // XID not found on cached page - cache is stale, fall through to full scan
                buffer_pool_->unpinPage(start_page, false, ctx);
                {
                    std::lock_guard<std::mutex> lock(tip_cache_mutex_);
                    tip_location_cache_.erase(xid);
                }
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

            // P1-7: Use binary search instead of linear search (O(log N) vs O(N))
            int32_t idx = binarySearchTIPEntries(entries, tip_header->num_transactions, xid);
            if (idx >= 0)
            {
                // Update existing entry
                entries[idx].state = static_cast<uint8_t>(state);
                entries[idx].commit_time =
                    (state != TransactionState::ACTIVE)
                        ? std::chrono::duration_cast<std::chrono::microseconds>(
                              std::chrono::system_clock::now().time_since_epoch())
                              .count()
                        : 0;

                // Update checksum
                tip_header->page_header.checksum = calculatePageChecksum(
                    reinterpret_cast<uint8_t *>(page_buffer), db_->page_size());

                // Cache this page location for future updates (OPTIMIZATION)
                {
                    std::lock_guard<std::mutex> lock(tip_cache_mutex_);
                    if (tip_location_cache_.size() < MAX_TIP_LOCATION_CACHE_SIZE)
                    {
                        tip_location_cache_[xid] = current_page;
                    }
                }

                buffer_pool_->unpinPage(current_page, true, ctx);
                return Status::OK;
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
        {
            std::lock_guard<std::mutex> lock(tip_cache_mutex_);
            if (tip_location_cache_.size() < MAX_TIP_LOCATION_CACHE_SIZE)
            {
                tip_location_cache_[xid] = last_page;
            }
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

    auto TransactionManager::performGroupCommit(CommitWaiter* leader_waiter,
                                                ErrorContext *ctx)
        -> Status
    {
        // Leader function: collect batch, write TIDs, fsync, wake waiters
        std::vector<CommitWaiter*> batch;
        batch.push_back(leader_waiter);

        // Collect batch of waiting commits (with timeout)
        auto start_time = std::chrono::steady_clock::now();
        auto deadline = start_time + std::chrono::microseconds(group_commit_timeout_us_);

        while (std::chrono::steady_clock::now() < deadline &&
               batch.size() < group_commit_batch_size_)
        {
            bool queue_empty_snapshot = false;
            {
                std::lock_guard<std::mutex> lock(group_commit_mutex_);

                // Collect all waiting commits from queue
                while (!commit_queue_.empty() && batch.size() < group_commit_batch_size_)
                {
                    batch.push_back(commit_queue_.back());
                    commit_queue_.pop_back();
                }
                queue_empty_snapshot = commit_queue_.empty();
            }

            // If we have a good batch size, break early
            if (batch.size() >= group_commit_batch_size_ / 2)
            {
                break;
            }

            // If queue is empty and we're past minimum wait time, break
            if (queue_empty_snapshot &&
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
        for (const auto* waiter : batch)
        {
            xid_batch.push_back({waiter->xid, waiter->state});
        }

        // Write all TIP entries in batch
        Status status = writeTipEntriesBatch(xid_batch, ctx);

        if (status == Status::OK)
        {
            for (const auto* waiter : batch)
            {
                status = db_->clog()->setStatus(waiter->xid,
                                                clogStatusForTransactionState(waiter->state),
                                                ctx);
                if (status != Status::OK)
                {
                    break;
                }
            }
        }

        // Single durable fence for the entire terminal-state batch.
        if (status == Status::OK)
        {
            status = flushTransactionState(ctx);
        }

        // Wake all waiters with result
        for (auto* waiter : batch)
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
        if (buffer_pool_ != nullptr)
        {
            buffer_pool_->beginCommitFence();
        }
        Status status = buffer_pool_->flushAll(ctx);
        if (status == Status::OK)
        {
            status = db_->sync(ctx);
        }
        if (buffer_pool_ != nullptr)
        {
            buffer_pool_->endCommitFence();
        }
        return status;
    }

    auto TransactionManager::normalizeStartupTipStates(bool clean_shutdown_marker,
                                                       bool *startup_repair_out,
                                                       StartupReconciliationSummary *startup_summary,
                                                       ErrorContext *ctx) -> Status
    {
        if (startup_repair_out)
        {
            *startup_repair_out = false;
        }

        std::unordered_map<uint64_t, std::vector<std::string>> prepared_gids_by_xid;
        CatalogManager *catalog = db_->catalog_manager();
        if (catalog)
        {
            std::vector<CatalogManager::PreparedTransactionInfo> prepared;
            Status prepared_status = catalog->listPreparedTransactions(prepared, ctx);
            if (prepared_status == Status::OK)
            {
                for (const auto &entry : prepared)
                {
                    prepared_gids_by_xid[entry.txn_id].push_back(entry.gid);
                }
            }
            else if (prepared_status != Status::NOT_FOUND)
            {
                return prepared_status;
            }
        }

        std::lock_guard<std::mutex> tip_guard(tip_io_mutex_);

        uint32_t current_page = tip_root_page_;
        bool any_mutation = false;
        const uint64_t next_xid = next_xid_.load(std::memory_order_acquire);
        std::vector<std::pair<uint64_t, std::string>> stale_prepared_records;

        while (current_page != 0)
        {
            void *page_buffer = nullptr;
            Status status = buffer_pool_->pinPage(current_page, &page_buffer, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *tip_header = static_cast<TIPPageHeader *>(page_buffer);
            if (tip_header->page_header.page_type != PAGE_TYPE_TRANSACTION_MAP)
            {
                buffer_pool_->unpinPage(current_page, false, ctx);
                SET_ERROR_CONTEXT_VNEXT(ctx, Status::PAGE_CORRUPT, "TXN_0215",
                                        "Invalid TIP page during startup normalization");
                return Status::PAGE_CORRUPT;
            }

            bool page_mutation = false;
            auto *entries = reinterpret_cast<TIPEntry *>(
                reinterpret_cast<uint8_t *>(page_buffer) + sizeof(TIPPageHeader));

            for (uint32_t i = 0; i < tip_header->num_transactions; ++i)
            {
                TransactionState state = TransactionState::ACTIVE;
                if (!decodeTipState(entries[i].state, state))
                {
                    buffer_pool_->unpinPage(current_page, false, ctx);
                    SET_ERROR_CONTEXT_VNEXT(ctx, Status::PAGE_CORRUPT, "TXN_0215",
                                            "Invalid TIP state byte during startup normalization");
                    return Status::PAGE_CORRUPT;
                }

                const bool has_prepared_record =
                    prepared_gids_by_xid.find(entries[i].xid) != prepared_gids_by_xid.end();

                if (state == TransactionState::ACTIVE && entries[i].xid < next_xid)
                {
                    const bool promote_to_prepared = has_prepared_record;
                    entries[i].state = static_cast<uint8_t>(
                        promote_to_prepared ? TransactionState::PREPARED
                                            : TransactionState::ABORTED);
                    entries[i].commit_time = nowMicros();
                    page_mutation = true;
                    any_mutation = true;
                    if (startup_repair_out)
                    {
                        *startup_repair_out = true;
                    }
                    if (startup_summary)
                    {
                        startup_summary->startup_repair = true;
                        if (promote_to_prepared)
                        {
                            ++startup_summary->tip_active_to_prepared;
                        }
                        else
                        {
                            ++startup_summary->tip_active_to_aborted;
                        }
                    }

                    if (clean_shutdown_marker)
                    {
                        LOG_WARNING(TRANSACTION,
                                    "Clean-shutdown marker contradicted TIP ACTIVE state for xid=%lu; treating startup as repaired recovery",
                                    entries[i].xid);
                    }

                    auto cache_it = transaction_cache_.find(entries[i].xid);
                    if (cache_it != transaction_cache_.end())
                    {
                        cache_it->second =
                            has_prepared_record ? TransactionState::PREPARED
                                                : TransactionState::ABORTED;
                        touchCacheEntry(entries[i].xid);
                    }
                    else
                    {
                        addToCacheLRU(entries[i].xid,
                                      has_prepared_record ? TransactionState::PREPARED
                                                          : TransactionState::ABORTED);
                    }
                }
                else if (state == TransactionState::PREPARED)
                {
                    if (!has_prepared_record)
                    {
                        if (startup_summary)
                        {
                            ++startup_summary->prepared_tip_without_catalog;
                        }
                        buffer_pool_->unpinPage(current_page, false, ctx);
                        SET_ERROR_CONTEXT_VNEXT(ctx, Status::PAGE_CORRUPT, "TXN_0217",
                                                "TX_LIMBO_FENCE_MISMATCH: PREPARED TIP state has no matching prepared transaction record");
                        return Status::PAGE_CORRUPT;
                    }
                }
                else if (has_prepared_record)
                {
                    for (const auto &gid : prepared_gids_by_xid[entries[i].xid])
                    {
                        stale_prepared_records.emplace_back(entries[i].xid, gid);
                    }
                }
            }

            if (page_mutation)
            {
                tip_header->page_header.checksum = calculatePageChecksum(
                    reinterpret_cast<uint8_t *>(page_buffer), db_->page_size());
            }

            uint32_t page_to_unpin = current_page;
            current_page = tip_header->next_tip_page;
            buffer_pool_->unpinPage(page_to_unpin, page_mutation, ctx);
        }

        if (!stale_prepared_records.empty() && catalog)
        {
            for (const auto &[xid, gid] : stale_prepared_records)
            {
                ErrorContext cleanup_ctx;
                Status cleanup_status = catalog->deletePreparedTransaction(gid, &cleanup_ctx);
                if (cleanup_status != Status::OK && cleanup_status != Status::NOT_FOUND)
                {
                    if (ctx && !cleanup_ctx.message.empty())
                    {
                        ctx->set(cleanup_status,
                                 cleanup_ctx.message.c_str(),
                                 __FILE__,
                                 __LINE__,
                                 __func__);
                    }
                    return cleanup_status;
                }
                prepared_xids_.erase(xid);
                if (startup_repair_out)
                {
                    *startup_repair_out = true;
                }
                if (startup_summary)
                {
                    startup_summary->startup_repair = true;
                    ++startup_summary->stale_prepared_records_removed;
                }
            }
        }

        if (any_mutation)
        {
            return db_->sync(ctx);
        }
        return Status::OK;
    }

    auto TransactionManager::synchronizeStartupClogStateLocked(
        uint64_t *synchronized_count_out,
        ErrorContext *ctx) -> Status
    {
        if (synchronized_count_out != nullptr)
        {
            *synchronized_count_out = 0;
        }

        if (db_ == nullptr || db_->clog() == nullptr || tip_root_page_ == 0)
        {
            return Status::OK;
        }

        std::lock_guard<std::mutex> tip_guard(tip_io_mutex_);
        uint32_t current_page = tip_root_page_;

        while (current_page != 0)
        {
            void *page_buffer = nullptr;
            Status status = buffer_pool_->pinPage(current_page, &page_buffer, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *tip_header = static_cast<TIPPageHeader *>(page_buffer);
            if (tip_header->page_header.page_type != PAGE_TYPE_TRANSACTION_MAP)
            {
                buffer_pool_->unpinPage(current_page, false, ctx);
                SET_ERROR_CONTEXT_VNEXT(ctx, Status::PAGE_CORRUPT, "TXN_0215",
                                        "Invalid TIP page during startup CLOG synchronization");
                return Status::PAGE_CORRUPT;
            }

            auto *entries = reinterpret_cast<TIPEntry *>(
                reinterpret_cast<uint8_t *>(page_buffer) + sizeof(TIPPageHeader));
            for (uint32_t i = 0; i < tip_header->num_transactions; ++i)
            {
                TransactionState tip_state = TransactionState::ACTIVE;
                if (!decodeTipState(entries[i].state, tip_state))
                {
                    buffer_pool_->unpinPage(current_page, false, ctx);
                    SET_ERROR_CONTEXT_VNEXT(ctx, Status::PAGE_CORRUPT, "TXN_0215",
                                            "Invalid TIP state during startup CLOG synchronization");
                    return Status::PAGE_CORRUPT;
                }

                ClogStatus expected = clogStatusForTransactionState(tip_state);
                ClogStatus existing = ClogStatus::IN_PROGRESS;
                ErrorContext clog_ctx;
                Status clog_status = db_->clog()->getStatus(entries[i].xid, &existing, &clog_ctx);
                if (clog_status == Status::OK && existing == expected)
                {
                    continue;
                }
                if (clog_status != Status::OK && clog_status != Status::NOT_FOUND)
                {
                    buffer_pool_->unpinPage(current_page, false, ctx);
                    if (ctx && !clog_ctx.message.empty())
                    {
                        ctx->set(clog_status,
                                 clog_ctx.message.c_str(),
                                 __FILE__,
                                 __LINE__,
                                 __func__);
                    }
                    return clog_status;
                }

                ErrorContext set_ctx;
                clog_status = db_->clog()->setStatus(entries[i].xid, expected, &set_ctx);
                if (clog_status != Status::OK)
                {
                    buffer_pool_->unpinPage(current_page, false, ctx);
                    if (ctx && !set_ctx.message.empty())
                    {
                        ctx->set(clog_status,
                                 set_ctx.message.c_str(),
                                 __FILE__,
                                 __LINE__,
                                 __func__);
                    }
                    return clog_status;
                }

                if (synchronized_count_out != nullptr)
                {
                    ++(*synchronized_count_out);
                }
            }

            uint32_t page_to_unpin = current_page;
            current_page = tip_header->next_tip_page;
            buffer_pool_->unpinPage(page_to_unpin, false, ctx);
        }

        return Status::OK;
    }

    auto TransactionManager::persistTransactionMarkersLocked(ErrorContext *ctx) -> Status
    {
        void *header_buffer = nullptr;
        Status status = buffer_pool_->pinPage(0, &header_buffer, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        auto *db_header = static_cast<DatabaseHeader *>(header_buffer);
        db_header->oldest_active_xid = oldest_active_xid_;
        db_header->oldest_snapshot = oldest_snapshot_;
        db_header->inventory_generation = inventory_generation_;
        db_header->oldest_snapshot_serial = oldest_snapshot_serial_;
        db_header->page_header.checksum = calculatePageChecksum(
            reinterpret_cast<uint8_t *>(db_header), db_->page_size());

        buffer_pool_->unpinPage(0, true, ctx);
        return Status::OK;
    }

    auto TransactionManager::decodeTipState(uint8_t tip_state, TransactionState &state_out) -> bool
    {
        switch (tip_state)
        {
            case static_cast<uint8_t>(TransactionState::ACTIVE):
                state_out = TransactionState::ACTIVE;
                return true;
            case static_cast<uint8_t>(TransactionState::COMMITTED):
                state_out = TransactionState::COMMITTED;
                return true;
            case static_cast<uint8_t>(TransactionState::ABORTED):
                state_out = TransactionState::ABORTED;
                return true;
            case static_cast<uint8_t>(TransactionState::PREPARED):
                state_out = TransactionState::PREPARED;
                return true;
            default:
                return false;
        }
    }

    auto TransactionManager::snapshotHasActiveXid(const TransactionSnapshot &snapshot,
                                                  uint64_t xid) -> bool
    {
        return std::binary_search(snapshot.active_txid_set.begin(),
                                  snapshot.active_txid_set.end(), xid);
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
        //
        // MEDIUM-7 FIX: Check-then-act pattern here is SAFE
        // The check (transaction_cache_.size() >= MAX_CACHE_SIZE) and act (insert)
        // execute atomically within mutex_ critical section. All callers hold mutex_,
        // preventing concurrent modification. Slight cache overflow is impossible
        // because only one thread can execute this code at a time.

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

    int32_t TransactionManager::binarySearchTIPEntries(const TIPEntry *entries, uint32_t count,
                                                        uint64_t xid)
    {
        // P1-7: Binary search for XID in sorted TIP entries array
        // Performance: O(log N) vs O(N) for linear search
        // For 32K entries: ~15 comparisons vs ~16K average comparisons (1000x speedup)

        if (count == 0)
        {
            return -1;
        }

        int32_t left = 0;
        int32_t right = static_cast<int32_t>(count) - 1;

        while (left <= right)
        {
            int32_t mid = left + (right - left) / 2;
            uint64_t mid_xid = entries[mid].xid;

            if (mid_xid == xid)
            {
                // Found it!
                return mid;
            }
            else if (mid_xid < xid)
            {
                // Search upper half
                left = mid + 1;
            }
            else
            {
                // Search lower half
                right = mid - 1;
            }
        }

        // Not found
        return -1;
    }

} // namespace scratchbird::core
