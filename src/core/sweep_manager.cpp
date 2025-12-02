#include "scratchbird/core/sweep_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/garbage_collector.h"
#include "scratchbird/core/logger.h"
#include "scratchbird/core/config.h"
#include <chrono>
#include <thread>

namespace scratchbird::core
{
    SweepManager::SweepManager(Database *db) : db_(db), txn_manager_(nullptr), buffer_pool_(nullptr)
    {
    }

    SweepManager::~SweepManager()
    {
        // Wait for any ongoing sweep to complete
        while (sweep_in_progress_.load(std::memory_order_acquire))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    Status SweepManager::initialize(ErrorContext *ctx)
    {
        if (!db_)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database is null");
            return Status::INVALID_ARGUMENT;
        }

        txn_manager_ = db_->transaction_manager();
        buffer_pool_ = db_->buffer_pool();

        if (!txn_manager_ || !buffer_pool_)
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR,
                              "TransactionManager or BufferPool not available");
            return Status::IO_ERROR;
        }

        LOG_INFO(VACUUM, "SweepManager initialized");
        return Status::OK;
    }

    bool SweepManager::checkSweepTrigger(ErrorContext *ctx)
    {
        // Don't trigger if sweep is already in progress
        if (sweep_in_progress_.load(std::memory_order_acquire))
        {
            return false;
        }

        // Get current transaction markers
        uint64_t oit = txn_manager_->getOldestXid();
        uint64_t ost = txn_manager_->getOldestSnapshot();

        // No sweep needed if no snapshot transactions
        if (ost == 0)
        {
            return false;
        }

        // Calculate transaction gap
        uint64_t gap = (ost > oit) ? (ost - oit) : 0;

        // Phase 4 Enhancement: Read sweep_interval from config
        // For now, use hardcoded default of 20000 (safe default value)
        uint32_t sweep_interval = config::DEFAULT_SWEEP_INTERVAL;

        // Trigger sweep if gap exceeds threshold
        if (gap > sweep_interval)
        {
            LOG_INFO(VACUUM, "Sweep trigger condition met: gap=%lu, interval=%u, oit=%lu, ost=%lu",
                     gap, sweep_interval, oit, ost);

            // Trigger background sweep (non-blocking)
            Status s = executeSweep(false, ctx);
            if (s != Status::OK)
            {
                LOG_ERROR(VACUUM, "Failed to trigger sweep: %d", static_cast<int>(s));
                return false;
            }

            return true;
        }

        return false;
    }

    Status SweepManager::executeSweep(bool foreground, ErrorContext *ctx)
    {
        // Check if sweep is already running
        bool expected = false;
        if (!sweep_in_progress_.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
        {
            LOG_WARNING(VACUUM, "Sweep already in progress, skipping");
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Sweep already in progress");
            return Status::IO_ERROR;
        }

        LOG_INFO(VACUUM, "Starting sweep: mode=%s", foreground ? "foreground" : "background");

        auto start_time = std::chrono::steady_clock::now();
        uint64_t oit_before = txn_manager_->getOldestXid();

        // 1. Scan TIP pages to find new OIT
        uint64_t new_oit = findFirstUncommittedTransaction(ctx);

        if (new_oit == 0 || new_oit == oit_before)
        {
            // No change needed, but still update statistics
            LOG_INFO(VACUUM, "Sweep completed: OIT unchanged (oit=%lu)", oit_before);

            auto end_time = std::chrono::steady_clock::now();
            uint64_t duration_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time)
                    .count();
            updateStatistics(oit_before, oit_before, duration_ms);

            sweep_in_progress_.store(false, std::memory_order_release);
            return Status::OK;
        }

        // 2. Update OIT in database header
        Status s = txn_manager_->setOldestXid(new_oit, ctx);
        if (s != Status::OK)
        {
            LOG_ERROR(VACUUM, "Failed to update OIT: %d", static_cast<int>(s));
            sweep_in_progress_.store(false, std::memory_order_release);
            return s;
        }

        // 3. Optional: Remove old tuple versions (if foreground)
        if (foreground)
        {
            s = reclaimSpace(new_oit, ctx);
            if (s != Status::OK)
            {
                LOG_WARNING(VACUUM, "Space reclamation failed: %d", static_cast<int>(s));
                // Non-fatal - OIT is already advanced
            }
        }

        // 4. Update statistics
        auto end_time = std::chrono::steady_clock::now();
        uint64_t duration_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

        updateStatistics(oit_before, new_oit, duration_ms);

        LOG_INFO(VACUUM, "Sweep completed: old_oit=%lu, new_oit=%lu, duration=%lums", oit_before,
                 new_oit, duration_ms);

        // 5. Notify garbage collector that OIT has advanced
        // This allows GC to identify more garbage tuples for removal
        if (db_->garbage_collector() != nullptr)
        {
            db_->garbage_collector()->notifySweepComplete(oit_before, new_oit);
        }

        sweep_in_progress_.store(false, std::memory_order_release);
        return Status::OK;
    }

    uint64_t SweepManager::findFirstUncommittedTransaction(ErrorContext *ctx) const
    {
        uint64_t current_oit = txn_manager_->getOldestXid();
        uint64_t current_xmax = txn_manager_->getCurrentXid();

        LOG_DEBUG(VACUUM, "Scanning transactions: oit=%lu, xmax=%lu, range=%lu", current_oit,
                  current_xmax, current_xmax - current_oit);

        // Scan from current OIT to current XMAX
        // This is a simplified implementation that uses the transaction manager's
        // getTransactionState() method. A production implementation should scan
        // TIP pages directly for better performance.

        for (uint64_t xid = current_oit; xid < current_xmax; xid++)
        {
            // Skip special XIDs (INVALID_XID=0, BOOTSTRAP_XID=1, FROZEN_XID=2)
            constexpr uint64_t FROZEN_XID = 2;
            if (xid <= FROZEN_XID)
            {
                continue;
            }

            TransactionState state;
            Status s = txn_manager_->getTransactionState(xid, state, ctx);
            if (s != Status::OK)
            {
                // If we can't read state, assume it's still active (conservative)
                LOG_DEBUG(VACUUM, "Failed to read transaction state for xid=%lu, assuming active",
                          xid);
                return xid;
            }

            // First transaction that's not committed/aborted is new OIT
            if (state != TransactionState::COMMITTED && state != TransactionState::ABORTED)
            {
                LOG_DEBUG(VACUUM, "Found first uncommitted transaction: xid=%lu, state=%d", xid,
                          static_cast<int>(state));
                return xid;
            }
        }

        // All transactions are committed/aborted - OIT can advance to XMAX
        LOG_DEBUG(VACUUM, "All transactions committed/aborted, advancing OIT to XMAX=%lu",
                  current_xmax);
        return current_xmax;
    }

    Status SweepManager::reclaimSpace(uint64_t new_oit, ErrorContext *ctx)
    {
        // Space reclamation (foreground sweep):
        // 1. Scan all data pages
        // 2. For each tuple with xmax < new_oit, remove old versions
        // 3. Update forward pointers
        // 4. Compact pages if needed
        // 5. Update indexes

        // Phase 4 Enhancement: Implement space reclamation in future iteration
        // For now, rely on cooperative/background GC for space reclamation

        LOG_INFO(VACUUM, "Space reclamation not yet implemented (new_oit=%lu)", new_oit);
        return Status::OK;
    }

    void SweepManager::updateStatistics(uint64_t oit_before, uint64_t oit_after,
                                        uint64_t duration_ms)
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);

        stats_.sweep_count++;
        stats_.last_sweep_time = std::chrono::duration_cast<std::chrono::microseconds>(
                                     std::chrono::system_clock::now().time_since_epoch())
                                     .count();
        stats_.last_sweep_duration_ms = duration_ms;
        stats_.last_oit_before = oit_before;
        stats_.last_oit_after = oit_after;
        stats_.total_transactions_swept += (oit_after - oit_before);
        stats_.sweep_in_progress = false;

        LOG_DEBUG(VACUUM, "Statistics updated: count=%lu, transactions_swept=%lu",
                  stats_.sweep_count, stats_.total_transactions_swept);
    }

    SweepStatistics SweepManager::getStatistics() const
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        return stats_;
    }

} // namespace scratchbird::core
