/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
// Section 35 invariant: transaction_manager publishes native MGA/TIP lifecycle
// truth used by durability and restart handling. That truth remains distinct
// from redo, undo, or write-ahead recovery folklore.
// Section 37 invariant: transaction_manager defines transaction-visibility and
// concurrency adjacency for metadata and schema operations, but it does not by
// itself prove mature concurrent DDL or global metadata invalidation behavior.

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

    void recordTouchedPageLocal(std::vector<uint32_t> *touched_pages_out, uint32_t page_id)
    {
        if (touched_pages_out == nullptr)
        {
            return;
        }

        if (std::find(touched_pages_out->begin(), touched_pages_out->end(), page_id) ==
            touched_pages_out->end())
        {
            touched_pages_out->push_back(page_id);
        }
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

    auto transactionStateCanConsumeReserve(
        scratchbird::core::TransactionState state) -> bool
    {
        using scratchbird::core::TransactionState;

        switch (state)
        {
            case TransactionState::COMMITTED:
            case TransactionState::ABORTED:
            case TransactionState::PREPARED:
                return true;
            case TransactionState::ACTIVE:
                return false;
        }

        return false;
    }

    auto durabilityModeUsesDurableFence(scratchbird::core::DurabilityMode mode) -> bool
    {
        return mode != scratchbird::core::DurabilityMode::DEVELOPMENT_UNSAFE;
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
        status = writeTipEntry(BOOTSTRAP_XID, TransactionState::COMMITTED, 0, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        status = writeTipEntry(FROZEN_XID, TransactionState::COMMITTED, 0, ctx);
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
        transaction_state_details_.clear();
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
        latest_commit_seqno_ = db_header->latest_commit_seqno;

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

        bool rewrote_commit_sequences = false;
        status = backfillLegacyCommitSequencesLocked(&rewrote_commit_sequences, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        if (rewrote_commit_sequences && startup_summary != nullptr)
        {
            startup_summary->startup_repair = true;
        }

        bool repaired_commit_sequence_metadata = false;
        status = reconcileCommitSequenceMetadataLocked(&repaired_commit_sequence_metadata, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        if (repaired_commit_sequence_metadata && startup_summary != nullptr)
        {
            startup_summary->startup_repair = true;
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

        uint64_t reconciled_oit = oldest_xid_;
        if (!db_->last_shutdown_was_clean() || startup_repair || rewrote_commit_sequences)
        {
            status = findOldestInterestingXidFromInventoryUnlocked(
                oldest_xid_,
                current_next_xid,
                tip_root_page_,
                reconciled_oit,
                ctx);
            if (status != Status::OK)
            {
                return status;
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
            oldest_xid_ != reconciled_oit ||
            oldest_active_xid_ != reconciled_oat ||
            oldest_snapshot_ != reconciled_ost ||
            oldest_snapshot_serial_ != reconciled_snapshot_serial;

        oldest_xid_ = reconciled_oit;
        oldest_active_xid_ = reconciled_oat;
        oldest_snapshot_ = reconciled_ost;
        oldest_snapshot_serial_ = reconciled_snapshot_serial;
        next_snapshot_serial_ = oldest_snapshot_serial_ + 1;

        if (!db_->last_shutdown_was_clean() || startup_repair || rewrote_commit_sequences)
        {
            ++inventory_generation_;
        }

        if (markers_changed || rewrote_commit_sequences || !db_->last_shutdown_was_clean() ||
            startup_repair)
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
                startup_summary->startup_repair || startup_repair || rewrote_commit_sequences;
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

        // AUDIT CONTRACT:
        // Persist ACTIVE state and next_xid advance before publishing the
        // transaction in attachment-visible inventory. ProcArray is only updated
        // after TIP ACTIVE, CLOG IN_PROGRESS, and page-0 next_xid are durably
        // published so restart never sees "visible but not durable" activity.
        // Proof: tests/unit/test_mga_failpoint_replay.cpp and
        // tests/unit/test_transaction_manager.cpp.
        std::vector<uint32_t> publication_pages;

        Status status = writeTipEntry(new_xid,
                                      TransactionState::ACTIVE,
                                      0,
                                      ctx,
                                      &publication_pages);
        if (status != Status::OK)
        {
            return status;
        }

        // Persist IN_PROGRESS in CLOG to avoid "unknown == committed" after restart
        Status clog_status =
            db_->clog()->setStatus(new_xid, ClogStatus::IN_PROGRESS, ctx, &publication_pages);
        if (clog_status != Status::OK)
        {
            // Best-effort mark as aborted in TIP to avoid dangling ACTIVE state
            writeTipEntry(new_xid, TransactionState::ABORTED, 0, nullptr, nullptr);
            return clog_status;
        }

        // Update database header with new next_xid (avoid XID reuse after restart)
        uint64_t current_next_xid_for_header = next_xid_.load(std::memory_order_acquire);
        if (db_ != nullptr)
        {
            db_->update_header_next_xid(current_next_xid_for_header, ctx);
        }

        status = flushTransactionPublicationState(publication_pages, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        status = ProcArrayManager::setTransactionId(proc_id, new_xid, ctx);
        if (status != Status::OK)
        {
            std::vector<uint32_t> cleanup_pages;
            writeTipEntry(new_xid, TransactionState::ABORTED, 0, nullptr, &cleanup_pages);
            db_->clog()->setStatus(new_xid, ClogStatus::ABORTED, nullptr, &cleanup_pages);
            flushTransactionPublicationState(cleanup_pages, nullptr);
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
        uint64_t ignored_commit_seqno = 0;
        return commitTransactionWithSequence(proc_id, xid, ignored_commit_seqno, ctx);
    }

    auto TransactionManager::commitTransactionWithSequence(uint32_t proc_id,
                                                           uint64_t xid,
                                                           uint64_t &commit_seqno_out,
                                                           ErrorContext *ctx) -> Status
    {
        commit_seqno_out = 0;
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

        const DurabilityMode durability_mode = getDurabilityMode();
        const bool require_durable_fence = durabilityModeUsesDurableFence(durability_mode);
        const bool use_group_commit = durability_mode == DurabilityMode::GROUP_COMMIT;

        // AUDIT CONTRACT:
        // A client-visible commit requires terminal TIP/CLOG publication plus the
        // forced-write fence before ACK in safe modes. ProcArray visibility is
        // cleared only after terminal durability is established.
        // Proof: tests/unit/test_mga_failpoint_replay.cpp,
        // tests/unit/test_group_commit.cpp, and
        // tests/unit/test_executor_transaction_payload.cpp.
        Status status = Status::OK;
        if (!use_group_commit && require_durable_fence)
        {
            status = flushTransactionState(ctx);
            if (status != Status::OK)
            {
                SET_ERROR_CONTEXT_VNEXT(ctx, status, "TXN_0216",
                                        "Commit fence flush failed before durable publish");
                return status;
            }
        }

        MgaFailpointManager* failpoints = db_ ? db_->mga_failpoint_manager() : nullptr;
        if (require_durable_fence && !use_group_commit && failpoints != nullptr)
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
        if (use_group_commit)
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

                // Process stragglers outside the lock.
                if (!stragglers.empty())
                {
                    std::vector<TipBatchEntry> straggler_batch;
                    straggler_batch.reserve(stragglers.size());
                    {
                        std::lock_guard<std::mutex> seq_lock(mutex_);
                        for (auto *straggler : stragglers)
                        {
                            TipBatchEntry entry{};
                            entry.xid = straggler->xid;
                            entry.state = straggler->state;
                            if (straggler->state == TransactionState::COMMITTED)
                            {
                                entry.commit_seqno = ++latest_commit_seqno_;
                            }
                            straggler->commit_seqno = entry.commit_seqno;
                            straggler_batch.push_back(entry);
                        }
                    }

                    Status straggler_status = writeTipEntriesBatch(straggler_batch, ctx);
                    if (straggler_status == Status::OK)
                    {
                        for (const auto *straggler : stragglers)
                        {
                            straggler_status = db_->clog()->setStatus(
                                straggler->xid,
                                clogStatusForTransactionState(straggler->state),
                                ctx);
                            if (straggler_status != Status::OK)
                            {
                                break;
                            }
                        }
                    }
                    if (straggler_status == Status::OK)
                    {
                        straggler_status = flushTransactionState(ctx);
                    }

                    // Wake stragglers with result
                    for (size_t i = 0; i < stragglers.size(); ++i)
                    {
                        auto* straggler = stragglers[i];
                        std::lock_guard<std::mutex> lock(straggler->cv_mutex);
                        straggler->commit_seqno =
                            (straggler_status == Status::OK) ? straggler_batch[i].commit_seqno
                                                             : 0;
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

            commit_seqno_out = waiter->commit_seqno;
        }
        else
        {
            uint64_t commit_seqno = 0;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                commit_seqno = ++latest_commit_seqno_;
            }
            status = writeTipEntry(xid, TransactionState::COMMITTED, commit_seqno, ctx);
            if (status == Status::OK)
            {
                status = db_->clog()->setStatus(xid, ClogStatus::COMMITTED, ctx);
            }
            if (status == Status::OK)
            {
                if (require_durable_fence)
                {
                    status = flushTransactionState(ctx);
                }
            }
            commit_seqno_out = commit_seqno;
        }

        if (status != Status::OK)
        {
            commit_seqno_out = 0;
            return status;
        }

        // Clear ProcArray slot after terminal durability is guaranteed.
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

        if (require_durable_fence)
        {
            status = flushTransactionState(ctx);
            if (status != Status::OK)
            {
                return status;
            }
        }
        else
        {
            commits_acknowledged_at_risk_.fetch_add(1, std::memory_order_relaxed);
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
            transaction_state_details_.erase(xid);
            stats_.transactions_committed++;
        }

        if (require_durable_fence && failpoints != nullptr)
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

        // AUDIT CONTRACT:
        // PREPARED publication is catalog-first. The durable prepared record and
        // lock snapshot must exist before TIP/CLOG move to PREPARED so restart can
        // distinguish legitimate limbo from corruption.
        // Proof: tests/unit/test_executor_transaction_payload.cpp and
        // tests/unit/test_mga_failpoint_replay.cpp.
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

        status = writeTipEntry(xid, TransactionState::PREPARED, 0, ctx);
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
            transaction_state_details_.erase(xid);
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

        // AUDIT CONTRACT:
        // COMMIT PREPARED first resolves durable TIP/CLOG truth to COMMITTED, then
        // removes prepared catalog evidence and detached lock-owner state.
        // Proof: tests/unit/test_executor_transaction_payload.cpp.
        uint64_t commit_seqno = 0;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            commit_seqno = ++latest_commit_seqno_;
        }
        status = writeTipEntry(info.txn_id, TransactionState::COMMITTED, commit_seqno, ctx);
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
            transaction_state_details_.erase(info.txn_id);
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

        // AUDIT CONTRACT:
        // ROLLBACK PREPARED first resolves durable TIP/CLOG truth to ABORTED, then
        // deletes prepared catalog evidence and releases detached lock-owner state.
        // Proof: tests/unit/test_executor_transaction_payload.cpp.
        status = writeTipEntry(info.txn_id, TransactionState::ABORTED, 0, ctx);
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
            transaction_state_details_.erase(info.txn_id);
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

                // Process stragglers outside the lock.
                if (!stragglers.empty())
                {
                    std::vector<TipBatchEntry> straggler_batch;
                    straggler_batch.reserve(stragglers.size());
                    for (const auto *straggler : stragglers)
                    {
                        TipBatchEntry entry{};
                        entry.xid = straggler->xid;
                        entry.state = straggler->state;
                        straggler_batch.push_back(entry);
                    }

                    Status straggler_status = writeTipEntriesBatch(straggler_batch, ctx);
                    if (straggler_status == Status::OK)
                    {
                        for (const auto *straggler : stragglers)
                        {
                            straggler_status = db_->clog()->setStatus(
                                straggler->xid,
                                clogStatusForTransactionState(straggler->state),
                                ctx);
                            if (straggler_status != Status::OK)
                            {
                                break;
                            }
                        }
                    }
                    if (straggler_status == Status::OK)
                    {
                        straggler_status = flushTransactionState(ctx);
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
            status = writeTipEntry(xid, TransactionState::ABORTED, 0, ctx);
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
            transaction_state_details_.erase(xid);
            stats_.transactions_aborted++;
        }

        return Status::OK;
    }

    auto TransactionManager::getTransactionState(uint64_t xid, TransactionState &state_out,
                                                 ErrorContext *ctx) -> Status
    {
        TransactionStateResolution resolution{};
        Status status = getTransactionStateDetailed(xid, resolution, ctx);
        if (status == Status::OK)
        {
            state_out = resolution.state;
        }
        return status;
    }

    auto TransactionManager::getTransactionStateDetailed(uint64_t xid,
                                                         TransactionStateResolution &state_out,
                                                         ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);
        state_out = {};

        // Check cache first
        auto it = transaction_cache_.find(xid);
        if (it != transaction_cache_.end())
        {
            state_out.state = it->second;
            state_out.detail = lookupTransactionStateDetailLocked(xid, state_out.state);
            touchCacheEntry(xid); // Mark as recently used
            return Status::OK;
        }

        // XIDs older than the current authoritative inventory window are
        // treated as prehistorical committed history. Anything still inside
        // the inventory window must resolve through TIP.
        if (xid < oldest_xid_)
        {
            state_out.state = TransactionState::COMMITTED;
            state_out.detail = TransactionStateDetail::PREHISTORICAL_COMMITTED;
            addToCacheLRU(xid, TransactionState::COMMITTED);
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
            state_out.state = tip_state;
            state_out.detail = lookupTransactionStateDetailLocked(xid, tip_state);
            return Status::OK;
        }
        if (status != Status::NOT_FOUND)
        {
            return status;
        }

        SET_ERROR_CONTEXT_VNEXT(
            ctx,
            Status::PAGE_CORRUPT,
            "TXN_0215",
            "Authoritative TIP entry missing for in-range transaction state lookup");
        return Status::PAGE_CORRUPT;
    }

    auto TransactionManager::getCommittedTransactionSequence(uint64_t xid,
                                                             uint64_t &commit_seqno_out,
                                                             ErrorContext *ctx) -> Status
    {
        commit_seqno_out = 0;

        if (xid <= FROZEN_XID)
        {
            return Status::OK;
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (xid < oldest_xid_)
            {
                return Status::OK;
            }
        }

        TransactionState state = TransactionState::ACTIVE;
        Status status = getTransactionState(xid, state, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        if (state != TransactionState::COMMITTED)
        {
            SET_ERROR_CONTEXT_VNEXT(ctx, Status::INVALID_ARGUMENT, "TXN_0218",
                                    "Commit sequence lookup requires COMMITTED transaction state");
            return Status::INVALID_ARGUMENT;
        }

        TIPEntry tip_entry{};
        status = findTipEntry(xid, tip_entry, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        if (tip_entry.commit_time == 0)
        {
            SET_ERROR_CONTEXT_VNEXT(ctx, Status::PAGE_CORRUPT, "TXN_0215",
                                    "Committed transaction is missing durable commit sequence");
            return Status::PAGE_CORRUPT;
        }

        commit_seqno_out = tip_entry.commit_time;
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

        // XIDs older than oldest_xid_ are outside the authoritative inventory
        // window and resolve through the prehistorical-committed path in
        // getTransactionState().
        if (xid < oldest_xid_)
        {
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

    auto TransactionManager::findOldestInterestingXidFromInventoryUnlocked(
        uint64_t start_xid,
        uint64_t end_xid_exclusive,
        uint32_t tip_root_page,
        uint64_t &xid_out,
        ErrorContext *ctx) const -> Status
    {
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

    auto TransactionManager::findOldestInterestingXidFromInventory(uint64_t &xid_out,
                                                                   ErrorContext *ctx) const
        -> Status
    {
        uint64_t start_xid = 0;
        uint64_t end_xid_exclusive = 0;
        uint32_t tip_root_page = 0;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            start_xid = oldest_xid_;
            end_xid_exclusive = next_xid_.load(std::memory_order_acquire);
            tip_root_page = tip_root_page_;
        }

        return findOldestInterestingXidFromInventoryUnlocked(
            start_xid,
            end_xid_exclusive,
            tip_root_page,
            xid_out,
            ctx);
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
        db_header->oldest_transaction_id = oldest_xid_;
        db_header->oldest_active_xid = oldest_active_xid_;
        db_header->oldest_snapshot = oldest_snapshot_;
        db_header->inventory_generation = inventory_generation_;
        db_header->oldest_snapshot_serial = oldest_snapshot_serial_;
        db_header->latest_commit_seqno = latest_commit_seqno_;
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
        snapshot_out.snapshot_commit_seqno_high = latest_commit_seqno_;
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

        TransactionStateResolution resolution{};
        Status status = getTransactionStateDetailed(xid, resolution, nullptr);
        if (status != Status::OK)
        {
            decision.reason = VisibilityReason::STATE_LOOKUP_FAILED;
            return decision;
        }

        decision.state = resolution.state;
        decision.detail = resolution.detail;

        if (mode == VisibilityMode::SNAPSHOT && snapshotHasActiveXid(*snapshot, xid))
        {
            decision.reason = VisibilityReason::ACTIVE_IN_SNAPSHOT;
            return decision;
        }

        switch (resolution.state)
        {
            case TransactionState::COMMITTED:
                if (mode == VisibilityMode::SNAPSHOT && xid >= oldest_xid_)
                {
                    uint64_t commit_seqno = 0;
                    Status commit_seq_status =
                        getCommittedTransactionSequence(xid, commit_seqno, nullptr);
                    if (commit_seq_status != Status::OK)
                    {
                        decision.reason = VisibilityReason::STATE_LOOKUP_FAILED;
                        return decision;
                    }
                    if (commit_seqno > snapshot->snapshot_commit_seqno_high)
                    {
                        decision.reason = VisibilityReason::COMMITTED_AFTER_SNAPSHOT;
                        return decision;
                    }
                }
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

    auto TransactionManager::evaluateBootstrapRecordVisibility(uint64_t create_xid,
                                                               uint64_t delete_xid,
                                                               uint64_t reader_xid)
        -> RecordVisibilityDecision
    {
        RecordVisibilityDecision decision{};
        decision.mode = VisibilityMode::READ_CURRENT_TRANSACTION;
        decision.create_decision.mode = VisibilityMode::READ_CURRENT_TRANSACTION;
        decision.delete_decision.mode = VisibilityMode::READ_CURRENT_TRANSACTION;

        if (!isValidXid(create_xid))
        {
            decision.create_decision.reason = VisibilityReason::INVALID_XID;
        }
        else if (create_xid == reader_xid)
        {
            decision.create_visible = true;
            decision.create_decision.visible = true;
            decision.create_decision.reason = VisibilityReason::OWN_TRANSACTION;
        }
        else if (create_xid <= FROZEN_XID)
        {
            decision.create_visible = true;
            decision.create_decision.visible = true;
            decision.create_decision.state = TransactionState::COMMITTED;
            decision.create_decision.reason = VisibilityReason::FROZEN_XID;
        }
        else if (create_xid > reader_xid)
        {
            decision.create_decision.reason = VisibilityReason::FUTURE_XID;
        }
        else
        {
            decision.create_visible = true;
            decision.create_decision.visible = true;
            decision.create_decision.state = TransactionState::COMMITTED;
            decision.create_decision.reason = VisibilityReason::COMMITTED_VISIBLE;
        }

        if (delete_xid == 0)
        {
            decision.delete_decision.state = TransactionState::COMMITTED;
            decision.delete_decision.reason = VisibilityReason::DELETE_NOT_PRESENT;
        }
        else if (!isValidXid(delete_xid))
        {
            decision.delete_decision.reason = VisibilityReason::INVALID_XID;
        }
        else if (delete_xid == reader_xid)
        {
            decision.delete_visible = true;
            decision.delete_decision.visible = true;
            decision.delete_decision.reason = VisibilityReason::OWN_TRANSACTION;
        }
        else if (delete_xid <= FROZEN_XID)
        {
            decision.delete_visible = true;
            decision.delete_decision.visible = true;
            decision.delete_decision.state = TransactionState::COMMITTED;
            decision.delete_decision.reason = VisibilityReason::FROZEN_XID;
        }
        else if (delete_xid > reader_xid)
        {
            decision.delete_decision.reason = VisibilityReason::FUTURE_XID;
        }
        else
        {
            decision.delete_visible = true;
            decision.delete_decision.visible = true;
            decision.delete_decision.state = TransactionState::COMMITTED;
            decision.delete_decision.reason = VisibilityReason::COMMITTED_VISIBLE;
        }

        decision.visible = decision.create_visible && !decision.delete_visible;
        return decision;
    }

    auto TransactionManager::evaluateRuntimeVersionTraversalStep(
        uint64_t create_xid, uint64_t delete_xid, bool has_back_version,
        uint64_t default_reader_xid, const ConnectionContext *conn_ctx)
        -> VersionTraversalDecision
    {
        VersionTraversalDecision decision{};

        if (!isValidXid(create_xid))
        {
            decision.status = has_back_version ? Status::OK : Status::PAGE_CORRUPT;
            decision.action = has_back_version ? VersionTraversalAction::FOLLOW_BACK_VERSION
                                               : VersionTraversalAction::CORRUPT_VERSION;
            decision.reason = VisibilityReason::INVALID_XID;
            return decision;
        }

        decision.record_decision =
            evaluateRuntimeRecordVisibility(create_xid, delete_xid, default_reader_xid, conn_ctx);
        decision.reason = decision.record_decision.create_decision.reason;

        if (decision.record_decision.visible)
        {
            decision.action = VersionTraversalAction::RETURN_VISIBLE;
            return decision;
        }

        if (!decision.record_decision.create_visible && has_back_version)
        {
            decision.action = VersionTraversalAction::FOLLOW_BACK_VERSION;
            return decision;
        }

        decision.action = VersionTraversalAction::TERMINAL_NOT_VISIBLE;
        if (decision.record_decision.create_visible)
        {
            decision.reason = decision.record_decision.delete_decision.reason;
        }
        return decision;
    }

    auto TransactionManager::evaluateBootstrapVersionTraversalStep(uint64_t create_xid,
                                                                   uint64_t delete_xid,
                                                                   bool has_back_version,
                                                                   uint64_t reader_xid)
        -> VersionTraversalDecision
    {
        VersionTraversalDecision decision{};

        if (!isValidXid(create_xid))
        {
            decision.status = has_back_version ? Status::OK : Status::PAGE_CORRUPT;
            decision.action = has_back_version ? VersionTraversalAction::FOLLOW_BACK_VERSION
                                               : VersionTraversalAction::CORRUPT_VERSION;
            decision.reason = VisibilityReason::INVALID_XID;
            return decision;
        }

        decision.record_decision =
            evaluateBootstrapRecordVisibility(create_xid, delete_xid, reader_xid);
        decision.reason = decision.record_decision.create_decision.reason;

        if (decision.record_decision.visible)
        {
            decision.action = VersionTraversalAction::RETURN_VISIBLE;
            return decision;
        }

        if (!decision.record_decision.create_visible && has_back_version)
        {
            decision.action = VersionTraversalAction::FOLLOW_BACK_VERSION;
            return decision;
        }

        decision.action = VersionTraversalAction::TERMINAL_NOT_VISIBLE;
        if (decision.record_decision.create_visible)
        {
            decision.reason = decision.record_decision.delete_decision.reason;
        }
        return decision;
    }

    auto TransactionManager::evaluateRuntimeRecordVisibility(uint64_t create_xid,
                                                             uint64_t delete_xid,
                                                             uint64_t default_reader_xid,
                                                             const ConnectionContext *conn_ctx)
        -> RecordVisibilityDecision
    {
        if (conn_ctx == nullptr)
        {
            conn_ctx = ConnectionContext::getCurrent();
        }

        const auto visibility_context = resolveVisibilityContext(default_reader_xid, conn_ctx);
        if (!visibility_context.valid)
        {
            RecordVisibilityDecision decision{};
            decision.mode = visibility_context.mode;
            decision.create_visible = false;
            decision.delete_visible = false;
            decision.visible = false;
            decision.create_decision.mode = visibility_context.mode;
            decision.create_decision.reason = visibility_context.reason;
            decision.delete_decision.mode = visibility_context.mode;
            decision.delete_decision.reason =
                (delete_xid == 0) ? VisibilityReason::DELETE_NOT_PRESENT
                                  : visibility_context.reason;
            return decision;
        }

        return evaluateRecordVisibility(create_xid,
                                        delete_xid,
                                        visibility_context.reader_xid,
                                        visibility_context.mode,
                                        visibility_context.snapshot);
    }

    auto TransactionManager::evaluateRuntimeTransactionVisibility(
        uint64_t xid, uint64_t default_reader_xid, const ConnectionContext *conn_ctx)
        -> TransactionVisibilityDecision
    {
        if (conn_ctx == nullptr)
        {
            conn_ctx = ConnectionContext::getCurrent();
        }

        const auto visibility_context = resolveVisibilityContext(default_reader_xid, conn_ctx);
        if (!visibility_context.valid)
        {
            TransactionVisibilityDecision decision{};
            decision.mode = visibility_context.mode;
            decision.reason = visibility_context.reason;
            return decision;
        }

        return evaluateTransactionVisibility(xid,
                                             visibility_context.reader_xid,
                                             visibility_context.mode,
                                             visibility_context.snapshot);
    }

    auto TransactionManager::isRuntimeRecordVisible(uint64_t create_xid, uint64_t delete_xid,
                                                    uint64_t default_reader_xid,
                                                    const ConnectionContext *conn_ctx) -> bool
    {
        return evaluateRuntimeRecordVisibility(create_xid,
                                              delete_xid,
                                              default_reader_xid,
                                              conn_ctx)
            .visible;
    }

    auto TransactionManager::isRuntimeTransactionVisible(uint64_t xid,
                                                         uint64_t default_reader_xid,
                                                         const ConnectionContext *conn_ctx) -> bool
    {
        return evaluateRuntimeTransactionVisibility(xid, default_reader_xid, conn_ctx).visible;
    }

    auto TransactionManager::isInventoryRecordVisible(uint64_t create_xid,
                                                      uint64_t delete_xid,
                                                      uint64_t reader_xid) -> bool
    {
        return evaluateRecordVisibility(create_xid,
                                        delete_xid,
                                        reader_xid,
                                        VisibilityMode::READ_CURRENT_TRANSACTION,
                                        nullptr)
            .visible;
    }

    auto TransactionManager::isInventoryTransactionVisible(uint64_t xid,
                                                           uint64_t reader_xid) -> bool
    {
        return evaluateTransactionVisibility(xid,
                                             reader_xid,
                                             VisibilityMode::READ_CURRENT_TRANSACTION,
                                             nullptr)
            .visible;
    }

    auto TransactionManager::resolveVisibilityContext(uint64_t default_reader_xid,
                                                      const ConnectionContext *conn_ctx) const
        -> VisibilityContextSelection
    {
        VisibilityContextSelection context{};
        context.valid = true;
        context.reader_xid = default_reader_xid;

        if (conn_ctx == nullptr)
        {
            return context;
        }

        if (const auto *replay_snapshot = conn_ctx->getForensicReplaySnapshot();
            replay_snapshot != nullptr)
        {
            context.mode = VisibilityMode::SNAPSHOT;
            context.snapshot = replay_snapshot;
            return context;
        }

        switch (conn_ctx->getIsolationLevel())
        {
            case IsolationLevel::READ_COMMITTED:
                context.mode = VisibilityMode::READ_CURRENT_TRANSACTION;
                return context;

            case IsolationLevel::SNAPSHOT:
            case IsolationLevel::SNAPSHOT_TABLE_STABILITY:
                if (const auto *retained_snapshot = conn_ctx->getRetainedTransactionSnapshot();
                    retained_snapshot != nullptr)
                {
                    context.mode = VisibilityMode::SNAPSHOT;
                    context.snapshot = retained_snapshot;
                }
                else
                {
                    context.mode = VisibilityMode::READ_CURRENT_VERSION;
                }
                return context;

            case IsolationLevel::READ_COMMITTED_READ_CONSISTENCY:
                context.reader_xid = conn_ctx->getStatementXID();
                if (conn_ctx->statementTrackingActive())
                {
                    if (const auto *statement_snapshot =
                            conn_ctx->getStatementTransactionSnapshot();
                        statement_snapshot != nullptr)
                    {
                        context.mode = VisibilityMode::SNAPSHOT;
                        context.snapshot = statement_snapshot;
                        return context;
                    }

                    context.valid = false;
                    context.reason = VisibilityReason::MISSING_SNAPSHOT;
                    return context;
                }

                context.mode = VisibilityMode::READ_CURRENT_VERSION;
                return context;
        }

        context.mode = VisibilityMode::READ_CURRENT_TRANSACTION;
        return context;
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

    auto TransactionManager::transactionStateDetailName(TransactionStateDetail detail)
        -> const char *
    {
        switch (detail)
        {
            case TransactionStateDetail::NONE:
                return "NONE";
            case TransactionStateDetail::PREHISTORICAL_COMMITTED:
                return "PREHISTORICAL_COMMITTED";
            case TransactionStateDetail::STARTUP_REPAIRED_ABORTED:
                return "STARTUP_REPAIRED_ABORTED";
            case TransactionStateDetail::STARTUP_REPAIRED_PREPARED:
                return "STARTUP_REPAIRED_PREPARED";
        }

        return "UNKNOWN";
    }

    auto TransactionManager::getBackendXid(uint32_t proc_id) const -> uint64_t
    {
        // Use ProcArray API to safely get backend XID
        uint64_t xid = 0;
        ProcArrayManager::getBackendXid(proc_id, &xid, nullptr);
        return xid;
    }


    auto TransactionManager::allocateTipPage(uint32_t &page_id_out,
                                             ErrorContext *ctx,
                                             bool allow_reserve_consumption) -> Status
    {
        WritebackAttribution attribution{};
        attribution.queue_kind = WritebackQueueKind::FOREGROUND_HELP;
        attribution.policy_domain = WritebackPolicyDomain::TRANSACTION;

        // Allocate a new page for TIP
        Status status =
            page_manager_->allocatePage(page_id_out, ctx, allow_reserve_consumption);
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

        // Publish the bootstrap TIP page through the regular page-write path so
        // transaction inventory allocation participates in the same writeback
        // failure attribution and forced-write fence as every other durable
        // metadata page.
        status = db_->write_page(page_id_out, new_page.get(), ctx, attribution);
        if (status != Status::OK)
        {
            page_manager_->freePage(page_id_out, ctx);
            return status;
        }

        status = db_->sync(ctx, attribution);
        if (status != Status::OK)
        {
            // Do not recycle the page on sync failure. The writeback incident
            // fence now owns recovery of this partially published durable state.
            return status;
        }

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

    auto TransactionManager::writeTipEntry(uint64_t xid,
                                           TransactionState state,
                                           uint64_t commit_seqno,
                                           ErrorContext *ctx,
                                           std::vector<uint32_t> *touched_pages_out) -> Status
    {
        const uint64_t tip_commit_seqno =
            (state == TransactionState::COMMITTED) ? commit_seqno : 0;
        const bool allow_reserve_consumption = transactionStateCanConsumeReserve(state);

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
                        entries[idx].commit_time = tip_commit_seqno;

                        // Update checksum
                        tip_header->page_header.checksum = calculatePageChecksum(
                            reinterpret_cast<uint8_t *>(page_buffer), db_->page_size());

                        buffer_pool_->unpinPage(start_page, true, ctx);
                        recordTouchedPageLocal(touched_pages_out, start_page);
                        if (state == TransactionState::COMMITTED)
                        {
                            void *header_buffer = nullptr;
                            Status header_status = buffer_pool_->pinPage(0, &header_buffer, ctx);
                            if (header_status != Status::OK)
                            {
                                return header_status;
                            }

                            auto *db_header = static_cast<DatabaseHeader *>(header_buffer);
                            db_header->latest_commit_seqno =
                                std::max(db_header->latest_commit_seqno, commit_seqno);
                            db_header->latest_completed_xid =
                                std::max(db_header->latest_completed_xid, xid);
                            db_header->page_header.checksum = calculatePageChecksum(
                                reinterpret_cast<uint8_t *>(db_header), db_->page_size());
                            buffer_pool_->unpinPage(0, true, ctx);
                            recordTouchedPageLocal(touched_pages_out, 0);
                        }
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
                entries[idx].commit_time = tip_commit_seqno;

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
                recordTouchedPageLocal(touched_pages_out, current_page);
                if (state == TransactionState::COMMITTED)
                {
                    void *header_buffer = nullptr;
                    Status header_status = buffer_pool_->pinPage(0, &header_buffer, ctx);
                    if (header_status != Status::OK)
                    {
                        return header_status;
                    }

                    auto *db_header = static_cast<DatabaseHeader *>(header_buffer);
                    db_header->latest_commit_seqno =
                        std::max(db_header->latest_commit_seqno, commit_seqno);
                    db_header->latest_completed_xid =
                        std::max(db_header->latest_completed_xid, xid);
                    db_header->page_header.checksum = calculatePageChecksum(
                        reinterpret_cast<uint8_t *>(db_header), db_->page_size());
                    buffer_pool_->unpinPage(0, true, ctx);
                    recordTouchedPageLocal(touched_pages_out, 0);
                }
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
            status = allocateTipPage(new_page_id, ctx, allow_reserve_consumption);
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
            recordTouchedPageLocal(touched_pages_out, last_page);

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
        entries[idx].commit_time = tip_commit_seqno;

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
        recordTouchedPageLocal(touched_pages_out, last_page);

        if (state == TransactionState::COMMITTED)
        {
            void *header_buffer = nullptr;
            Status header_status = buffer_pool_->pinPage(0, &header_buffer, ctx);
            if (header_status != Status::OK)
            {
                return header_status;
            }

            auto *db_header = static_cast<DatabaseHeader *>(header_buffer);
            db_header->latest_commit_seqno =
                std::max(db_header->latest_commit_seqno, commit_seqno);
            db_header->latest_completed_xid =
                std::max(db_header->latest_completed_xid, xid);
            db_header->page_header.checksum = calculatePageChecksum(
                reinterpret_cast<uint8_t *>(db_header), db_->page_size());
            buffer_pool_->unpinPage(0, true, ctx);
            recordTouchedPageLocal(touched_pages_out, 0);
        }

        return Status::OK;
    }

    auto TransactionManager::writeTipEntriesBatch(const std::vector<TipBatchEntry> &batch,
                                                  ErrorContext *ctx) -> Status
    {
        if (batch.empty())
        {
            return Status::OK;
        }

        // Sort batch by XID to minimize page pin/unpin cycles
        std::vector<TipBatchEntry> sorted_batch = batch;
        std::sort(sorted_batch.begin(), sorted_batch.end(),
                  [](const auto &a, const auto &b) { return a.xid < b.xid; });

        // Process all XIDs in the batch
        for (const auto &entry : sorted_batch)
        {
            // For each XID, update or insert the TIP entry
            // This reuses the existing writeTipEntry logic but without the fsync
            Status status = writeTipEntry(entry.xid, entry.state, entry.commit_seqno, ctx);
            if (status != Status::OK)
            {
                LOG_ERROR(TRANSACTION, "Failed to write TIP entry for XID %lu in batch",
                          entry.xid);
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

        const auto wait_elapsed = std::chrono::steady_clock::now() - start_time;
        const uint64_t wait_us = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(wait_elapsed).count());
        group_commit_wait_time_us_total_.fetch_add(wait_us, std::memory_order_relaxed);

        MgaFailpointManager* failpoints = db_ ? db_->mga_failpoint_manager() : nullptr;

        Status status = flushTransactionState(ctx);
        if (status == Status::OK && failpoints != nullptr)
        {
            for (const auto* waiter : batch)
            {
                MgaFailpointInvocation invocation{};
                invocation.has_txid = true;
                invocation.txid = waiter->xid;
                status = failpoints->trip(
                    MgaFailpointTriggers::kAfterDirtyFlushBeforeTipTerminal,
                    invocation,
                    ctx);
                if (status != Status::OK)
                {
                    break;
                }
            }
        }

        // Build TIP batch and assign durable commit sequence numbers to committed entries.
        std::vector<TipBatchEntry> xid_batch;
        xid_batch.reserve(batch.size());
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto *waiter : batch)
            {
                TipBatchEntry entry{};
                entry.xid = waiter->xid;
                entry.state = waiter->state;
                if (waiter->state == TransactionState::COMMITTED)
                {
                    entry.commit_seqno = ++latest_commit_seqno_;
                }
                waiter->commit_seqno = entry.commit_seqno;
                xid_batch.push_back(entry);
            }
        }

        // Write all TIP entries in batch
        if (status == Status::OK)
        {
            status = writeTipEntriesBatch(xid_batch, ctx);
        }

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
        for (size_t i = 0; i < batch.size(); ++i)
        {
            auto* waiter = batch[i];
            std::lock_guard<std::mutex> lock(waiter->cv_mutex);
            waiter->commit_seqno = (status == Status::OK) ? xid_batch[i].commit_seqno : 0;
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
        // AUDIT CONTRACT:
        // This is the terminal forced-write fence for transaction publication and
        // commit/rollback resolution. It fails closed when writeback incidents are
        // open, flushes dirty buffers, and then delegates to Database::sync() so the
        // fence reaches the primary database plus every registered durable filespace.
        // Proof: tests/unit/test_mga_failpoint_replay.cpp and
        // tests/unit/test_transaction_vnext_contract.cpp.
        if (db_ != nullptr &&
            db_->write_admission_fenced() &&
            !db_->write_admission_enforcement_suspended())
        {
            const Status fenced_status = db_->write_admission_status();
            SET_ERROR_CONTEXT(ctx,
                              fenced_status == Status::OK ? Status::IO_ERROR : fenced_status,
                              "Durable commit fence is blocked by an open writeback incident");
            return fenced_status == Status::OK ? Status::IO_ERROR : fenced_status;
        }

        if (buffer_pool_ != nullptr)
        {
            buffer_pool_->beginCommitFence();
        }
        Status status = buffer_pool_->flushAll(ctx);
        if (status == Status::OK)
        {
            // Firebird-style forced writes require the terminal fence to reach
            // every registered durable filespace before ACK. Database::sync()
            // currently uses a conservative all-filespace fence here.
            WritebackAttribution attribution{};
            attribution.queue_kind = WritebackQueueKind::FOREGROUND_HELP;
            attribution.policy_domain = WritebackPolicyDomain::TRANSACTION;
            status = db_->sync(ctx, attribution);
        }
        if (buffer_pool_ != nullptr)
        {
            buffer_pool_->endCommitFence();
        }
        if (status == Status::OK)
        {
            commits_acknowledged_at_risk_.store(0, std::memory_order_release);
        }
        else
        {
            commit_fence_failures_.fetch_add(1, std::memory_order_relaxed);
        }
        return status;
    }

    auto TransactionManager::flushTransactionPublicationState(const std::vector<uint32_t> &page_ids,
                                                              ErrorContext *ctx) -> Status
    {
        if (db_ != nullptr &&
            db_->write_admission_fenced() &&
            !db_->write_admission_enforcement_suspended())
        {
            const Status fenced_status = db_->write_admission_status();
            SET_ERROR_CONTEXT(ctx,
                              fenced_status == Status::OK ? Status::IO_ERROR : fenced_status,
                              "Transaction publication is blocked by an open writeback incident");
            return fenced_status == Status::OK ? Status::IO_ERROR : fenced_status;
        }

        if (page_manager_ != nullptr)
        {
            Status page_manager_status = page_manager_->flush(ctx);
            if (page_manager_status != Status::OK)
            {
                return page_manager_status;
            }
        }

        if (buffer_pool_ != nullptr)
        {
            Status header_status = buffer_pool_->flushPage(0, ctx);
            if (header_status != Status::OK)
            {
                return header_status;
            }

            for (uint32_t page_id : page_ids)
            {
                if (page_id == 0)
                {
                    continue;
                }

                Status page_status = buffer_pool_->flushPage(page_id, ctx);
                if (page_status != Status::OK)
                {
                    return page_status;
                }
            }
        }

        WritebackAttribution attribution{};
        attribution.queue_kind = WritebackQueueKind::FOREGROUND_HELP;
        attribution.policy_domain = WritebackPolicyDomain::TRANSACTION;
        // Publication ordering uses the same conservative all-filespace forced
        // write fence as terminal transaction durability.
        return db_->sync(ctx, attribution);
    }

    auto TransactionManager::normalizeStartupTipStates(bool clean_shutdown_marker,
                                                       bool *startup_repair_out,
                                                       StartupReconciliationSummary *startup_summary,
                                                       ErrorContext *ctx) -> Status
    {
        // AUDIT CONTRACT:
        // Startup recovery is MGA state reconciliation, not WAL replay. This pass
        // normalizes orphan ACTIVE entries to ABORTED or PREPARED using catalog
        // evidence, rejects PREPARED-without-catalog as corruption, removes stale
        // prepared catalog rows, and durably republishes any repair before admitting
        // new work.
        // Proof: tests/unit/test_mga_failpoint_replay.cpp and
        // tests/unit/test_transaction_vnext_contract.cpp.
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
                    entries[i].commit_time = 0;
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

                    transaction_state_details_[entries[i].xid] =
                        promote_to_prepared
                            ? TransactionStateDetail::STARTUP_REPAIRED_PREPARED
                            : TransactionStateDetail::STARTUP_REPAIRED_ABORTED;

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
        db_header->oldest_transaction_id = oldest_xid_;
        db_header->oldest_active_xid = oldest_active_xid_;
        db_header->oldest_snapshot = oldest_snapshot_;
        db_header->inventory_generation = inventory_generation_;
        db_header->oldest_snapshot_serial = oldest_snapshot_serial_;
        db_header->latest_commit_seqno = latest_commit_seqno_;
        db_header->page_header.checksum = calculatePageChecksum(
            reinterpret_cast<uint8_t *>(db_header), db_->page_size());

        buffer_pool_->unpinPage(0, true, ctx);
        return Status::OK;
    }

    auto TransactionManager::backfillLegacyCommitSequencesLocked(bool *rewrote_tip_out,
                                                                 ErrorContext *ctx) -> Status
    {
        if (rewrote_tip_out != nullptr)
        {
            *rewrote_tip_out = false;
        }

        if (latest_commit_seqno_ != 0 || tip_root_page_ == 0)
        {
            return Status::OK;
        }

        struct LegacyCommittedEntry
        {
            uint64_t xid = 0;
            uint64_t legacy_commit_marker = 0;
        };

        std::vector<LegacyCommittedEntry> committed_entries;
        std::unordered_map<uint64_t, uint64_t> assigned_seqnos;

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
                                        "Invalid TIP page during commit-sequence backfill");
                return Status::PAGE_CORRUPT;
            }

            auto *entries = reinterpret_cast<TIPEntry *>(
                reinterpret_cast<uint8_t *>(page_buffer) + sizeof(TIPPageHeader));
            for (uint32_t i = 0; i < tip_header->num_transactions; ++i)
            {
                TransactionState state = TransactionState::ACTIVE;
                if (!decodeTipState(entries[i].state, state))
                {
                    buffer_pool_->unpinPage(current_page, false, ctx);
                    SET_ERROR_CONTEXT_VNEXT(ctx, Status::PAGE_CORRUPT, "TXN_0215",
                                            "Invalid TIP state during commit-sequence backfill");
                    return Status::PAGE_CORRUPT;
                }

                if (state == TransactionState::COMMITTED && entries[i].xid > FROZEN_XID)
                {
                    committed_entries.push_back({entries[i].xid, entries[i].commit_time});
                }
            }

            const uint32_t page_to_unpin = current_page;
            current_page = tip_header->next_tip_page;
            buffer_pool_->unpinPage(page_to_unpin, false, ctx);
        }

        if (committed_entries.empty())
        {
            return Status::OK;
        }

        std::stable_sort(committed_entries.begin(), committed_entries.end(),
                         [](const auto &lhs, const auto &rhs) {
                             if (lhs.legacy_commit_marker != rhs.legacy_commit_marker)
                             {
                                 return lhs.legacy_commit_marker < rhs.legacy_commit_marker;
                             }
                             return lhs.xid < rhs.xid;
                         });

        uint64_t next_commit_seqno = 1;
        for (const auto &entry : committed_entries)
        {
            assigned_seqnos[entry.xid] = next_commit_seqno++;
        }

        bool any_page_mutation = false;
        uint64_t latest_completed_xid = 0;
        current_page = tip_root_page_;
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
                                        "Invalid TIP page during commit-sequence rewrite");
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
                                            "Invalid TIP state during commit-sequence rewrite");
                    return Status::PAGE_CORRUPT;
                }

                if (state != TransactionState::COMMITTED || entries[i].xid <= FROZEN_XID)
                {
                    continue;
                }

                const auto seq_it = assigned_seqnos.find(entries[i].xid);
                if (seq_it == assigned_seqnos.end())
                {
                    buffer_pool_->unpinPage(current_page, false, ctx);
                    SET_ERROR_CONTEXT_VNEXT(ctx, Status::PAGE_CORRUPT, "TXN_0215",
                                            "Commit-sequence rewrite lost committed xid mapping");
                    return Status::PAGE_CORRUPT;
                }

                latest_completed_xid = std::max(latest_completed_xid, entries[i].xid);
                if (entries[i].commit_time != seq_it->second)
                {
                    entries[i].commit_time = seq_it->second;
                    page_mutation = true;
                    any_page_mutation = true;
                }
            }

            if (page_mutation)
            {
                tip_header->page_header.checksum = calculatePageChecksum(
                    reinterpret_cast<uint8_t *>(page_buffer), db_->page_size());
            }

            const uint32_t page_to_unpin = current_page;
            current_page = tip_header->next_tip_page;
            buffer_pool_->unpinPage(page_to_unpin, page_mutation, ctx);
        }

        latest_commit_seqno_ = next_commit_seqno - 1;

        void *header_buffer = nullptr;
        Status header_status = buffer_pool_->pinPage(0, &header_buffer, ctx);
        if (header_status != Status::OK)
        {
            return header_status;
        }

        auto *db_header = static_cast<DatabaseHeader *>(header_buffer);
        db_header->latest_commit_seqno = latest_commit_seqno_;
        db_header->latest_completed_xid =
            std::max(db_header->latest_completed_xid, latest_completed_xid);
        db_header->page_header.checksum = calculatePageChecksum(
            reinterpret_cast<uint8_t *>(db_header), db_->page_size());
        buffer_pool_->unpinPage(0, true, ctx);

        if (rewrote_tip_out != nullptr)
        {
            *rewrote_tip_out = true;
        }

        return flushTransactionState(ctx);
    }

    auto TransactionManager::reconcileCommitSequenceMetadataLocked(bool *startup_repair_out,
                                                                   ErrorContext *ctx) -> Status
    {
        if (startup_repair_out != nullptr)
        {
            *startup_repair_out = false;
        }

        if (tip_root_page_ == 0)
        {
            return Status::OK;
        }

        uint64_t max_commit_seqno = 0;
        std::unordered_map<uint64_t, uint64_t> seq_owner_xids;

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
                                        "Invalid TIP page during commit-sequence reconciliation");
                return Status::PAGE_CORRUPT;
            }

            auto *entries = reinterpret_cast<TIPEntry *>(
                reinterpret_cast<uint8_t *>(page_buffer) + sizeof(TIPPageHeader));
            for (uint32_t i = 0; i < tip_header->num_transactions; ++i)
            {
                TransactionState state = TransactionState::ACTIVE;
                if (!decodeTipState(entries[i].state, state))
                {
                    buffer_pool_->unpinPage(current_page, false, ctx);
                    SET_ERROR_CONTEXT_VNEXT(ctx, Status::PAGE_CORRUPT, "TXN_0215",
                                            "Invalid TIP state during commit-sequence reconciliation");
                    return Status::PAGE_CORRUPT;
                }

                if (state != TransactionState::COMMITTED || entries[i].xid <= FROZEN_XID)
                {
                    continue;
                }

                if (entries[i].commit_time == 0)
                {
                    buffer_pool_->unpinPage(current_page, false, ctx);
                    SET_ERROR_CONTEXT_VNEXT(
                        ctx,
                        Status::PAGE_CORRUPT,
                        "TXN_0215",
                        "Committed TIP entry missing durable commit sequence during startup reconciliation");
                    return Status::PAGE_CORRUPT;
                }

                const auto [seq_it, inserted] =
                    seq_owner_xids.emplace(entries[i].commit_time, entries[i].xid);
                if (!inserted && seq_it->second != entries[i].xid)
                {
                    buffer_pool_->unpinPage(current_page, false, ctx);
                    SET_ERROR_CONTEXT_VNEXT(
                        ctx,
                        Status::PAGE_CORRUPT,
                        "TXN_0215",
                        "Duplicate durable commit sequence detected during startup reconciliation");
                    return Status::PAGE_CORRUPT;
                }

                max_commit_seqno = std::max(max_commit_seqno, entries[i].commit_time);
            }

            const uint32_t page_to_unpin = current_page;
            current_page = tip_header->next_tip_page;
            buffer_pool_->unpinPage(page_to_unpin, false, ctx);
        }

        if (max_commit_seqno <= latest_commit_seqno_)
        {
            return Status::OK;
        }

        latest_commit_seqno_ = max_commit_seqno;

        void *header_buffer = nullptr;
        Status header_status = buffer_pool_->pinPage(0, &header_buffer, ctx);
        if (header_status != Status::OK)
        {
            return header_status;
        }

        auto *db_header = static_cast<DatabaseHeader *>(header_buffer);
        db_header->latest_commit_seqno = latest_commit_seqno_;
        db_header->page_header.checksum = calculatePageChecksum(
            reinterpret_cast<uint8_t *>(db_header), db_->page_size());
        buffer_pool_->unpinPage(0, true, ctx);

        if (startup_repair_out != nullptr)
        {
            *startup_repair_out = true;
        }

        return flushTransactionState(ctx);
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

    auto TransactionManager::lookupTransactionStateDetailLocked(uint64_t xid,
                                                                TransactionState state) const
        -> TransactionStateDetail
    {
        if (state == TransactionState::COMMITTED && xid < oldest_xid_)
        {
            return TransactionStateDetail::PREHISTORICAL_COMMITTED;
        }

        auto it = transaction_state_details_.find(xid);
        if (it != transaction_state_details_.end())
        {
            return it->second;
        }

        return TransactionStateDetail::NONE;
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
