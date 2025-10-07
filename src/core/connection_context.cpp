#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/logger.h"
#include <cassert>

namespace scratchbird::core
{
    // Thread-local storage for current connection context
    thread_local ConnectionContext* ConnectionContext::current_ = nullptr;

    ConnectionContext::ConnectionContext(Database* db, uint32_t proc_id)
        : db_(db)
        , txn_manager_(db ? db->transaction_manager() : nullptr)
        , proc_id_(proc_id)
        , current_xid_(0)  // Will be set by initialize()
        , xact_start_time_(std::chrono::microseconds(0))
        , isolation_level_(IsolationLevel::SNAPSHOT)  // Default to SNAPSHOT
        , is_read_only_(false)
        , wait_for_locks_(true)  // Default: wait for locks
        , lock_timeout_seconds_(60)  // Default: 60 second timeout
        , settings_staged_(false)
        , next_isolation_level_(IsolationLevel::SNAPSHOT)
        , next_is_read_only_(false)
        , snapshot_(nullptr)
    {
        assert(db != nullptr && "Database must not be null");
        assert(txn_manager_ != nullptr && "TransactionManager must not be null");
    }

    ConnectionContext::~ConnectionContext()
    {
        // If we're still the current context, clear it
        if (current_ == this)
        {
            current_ = nullptr;
        }

        // Rollback any outstanding transaction
        if (current_xid_ != 0)
        {
            ErrorContext err_ctx;
            rollback(&err_ctx);
        }
    }

    ConnectionContext::ConnectionContext(ConnectionContext&& other) noexcept
        : db_(other.db_)
        , txn_manager_(other.txn_manager_)
        , proc_id_(other.proc_id_)
        , current_xid_(other.current_xid_)
        , xact_start_time_(other.xact_start_time_)
        , isolation_level_(other.isolation_level_)
        , is_read_only_(other.is_read_only_)
        , wait_for_locks_(other.wait_for_locks_)
        , lock_timeout_seconds_(other.lock_timeout_seconds_)
        , settings_staged_(other.settings_staged_)
        , next_isolation_level_(other.next_isolation_level_)
        , next_is_read_only_(other.next_is_read_only_)
        , snapshot_(std::move(other.snapshot_))
        , table_reservations_(std::move(other.table_reservations_))
    {
        // Clear other's state
        other.db_ = nullptr;
        other.txn_manager_ = nullptr;
        other.current_xid_ = 0;
    }

    ConnectionContext& ConnectionContext::operator=(ConnectionContext&& other) noexcept
    {
        if (this != &other)
        {
            // Cleanup current transaction if any
            if (current_xid_ != 0)
            {
                ErrorContext err_ctx;
                rollback(&err_ctx);
            }

            // Move state
            db_ = other.db_;
            txn_manager_ = other.txn_manager_;
            proc_id_ = other.proc_id_;
            current_xid_ = other.current_xid_;
            xact_start_time_ = other.xact_start_time_;
            isolation_level_ = other.isolation_level_;
            is_read_only_ = other.is_read_only_;
            wait_for_locks_ = other.wait_for_locks_;
            lock_timeout_seconds_ = other.lock_timeout_seconds_;
            settings_staged_ = other.settings_staged_;
            next_isolation_level_ = other.next_isolation_level_;
            next_is_read_only_ = other.next_is_read_only_;
            snapshot_ = std::move(other.snapshot_);
            table_reservations_ = std::move(other.table_reservations_);

            // Clear other's state
            other.db_ = nullptr;
            other.txn_manager_ = nullptr;
            other.current_xid_ = 0;
        }
        return *this;
    }

    ConnectionContext* ConnectionContext::getCurrent()
    {
        return current_;
    }

    void ConnectionContext::setCurrent(ConnectionContext* ctx)
    {
        current_ = ctx;
    }

    int32_t ConnectionContext::getCurrentProcId()
    {
        ConnectionContext* ctx = getCurrent();
        if (ctx == nullptr)
        {
            return -1;  // No connection context
        }
        return static_cast<int32_t>(ctx->proc_id_);
    }

    uint64_t ConnectionContext::getCurrentTransactionId()
    {
        ConnectionContext* ctx = getCurrent();
        if (ctx == nullptr)
        {
            return 0;  // Invalid XID
        }
        return ctx->current_xid_;
    }

    Status ConnectionContext::initialize(ErrorContext* ctx)
    {
        // Start initial transaction
        Status s = beginNewTransaction(ctx);
        if (s != Status::OK)
        {
            LOG_ERROR(TRANSACTION, "Failed to initialize connection context: %d", static_cast<int>(s));
            return s;
        }

        LOG_DEBUG(TRANSACTION, "Initialized connection context: proc_id=%u, xid=%lu",
                 proc_id_, current_xid_);

        return Status::OK;
    }

    Status ConnectionContext::commit(ErrorContext* ctx)
    {
        if (current_xid_ == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "No active transaction to commit");
            return Status::INVALID_ARGUMENT;
        }

        LOG_DEBUG(TRANSACTION, "Committing transaction: proc_id=%u, xid=%lu", proc_id_, current_xid_);

        // 1. Commit current transaction
        Status s = endCurrentTransaction(true, ctx);
        if (s != Status::OK)
        {
            LOG_ERROR(TRANSACTION, "Failed to commit transaction: proc_id=%u, xid=%lu, status=%d",
                     proc_id_, current_xid_, static_cast<int>(s));
            return s;
        }

        // 2. Apply staged settings if any
        applyStagedSettings();

        // 3. ATOMICALLY start new transaction
        s = beginNewTransaction(ctx);
        if (s != Status::OK)
        {
            LOG_ERROR(TRANSACTION, "Failed to start new transaction after commit: proc_id=%u, status=%d",
                     proc_id_, static_cast<int>(s));
            return s;
        }

        LOG_DEBUG(TRANSACTION, "Started new transaction after commit: proc_id=%u, new_xid=%lu",
                 proc_id_, current_xid_);

        return Status::OK;
    }

    Status ConnectionContext::rollback(ErrorContext* ctx)
    {
        if (current_xid_ == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "No active transaction to rollback");
            return Status::INVALID_ARGUMENT;
        }

        LOG_DEBUG(TRANSACTION, "Rolling back transaction: proc_id=%u, xid=%lu", proc_id_, current_xid_);

        // 1. Rollback current transaction (always succeeds)
        Status s = endCurrentTransaction(false, ctx);
        if (s != Status::OK)
        {
            LOG_WARNING(TRANSACTION, "Rollback encountered error: proc_id=%u, xid=%lu, status=%d",
                       proc_id_, current_xid_, static_cast<int>(s));
            // Continue anyway - rollback should be best-effort
        }

        // 2. Apply staged settings if any
        applyStagedSettings();

        // 3. Start new transaction
        s = beginNewTransaction(ctx);
        if (s != Status::OK)
        {
            LOG_ERROR(TRANSACTION, "Failed to start new transaction after rollback: proc_id=%u, status=%d",
                     proc_id_, static_cast<int>(s));
            return s;
        }

        LOG_DEBUG(TRANSACTION, "Started new transaction after rollback: proc_id=%u, new_xid=%lu",
                 proc_id_, current_xid_);

        return Status::OK;
    }

    Status ConnectionContext::startTransaction(bool read_only, IsolationLevel isolation_level,
                                              bool commit_outstanding, ErrorContext* ctx)
    {
        LOG_DEBUG(TRANSACTION, "START TRANSACTION: proc_id=%u, read_only=%d, isolation=%d, commit_outstanding=%d",
                 proc_id_, read_only, static_cast<int>(isolation_level), commit_outstanding);

        if (commit_outstanding)
        {
            // Commit current transaction and apply new settings immediately
            next_isolation_level_ = isolation_level;
            next_is_read_only_ = read_only;
            settings_staged_ = true;

            return commit(ctx);
        }
        else
        {
            // Stage settings for next commit/rollback
            next_isolation_level_ = isolation_level;
            next_is_read_only_ = read_only;
            settings_staged_ = true;

            LOG_DEBUG(TRANSACTION, "Staged transaction settings: isolation=%d, read_only=%d",
                     static_cast<int>(isolation_level), read_only);

            return Status::OK;
        }
    }

    Status ConnectionContext::reserveTables(const std::vector<TableReservation>& reservations,
                                          ErrorContext* ctx)
    {
        // For now, just store the reservations
        // Actual lock acquisition would happen in the lock manager
        table_reservations_ = reservations;

        LOG_DEBUG(LOCK, "Reserved %zu tables for transaction: proc_id=%u, xid=%lu",
                 reservations.size(), proc_id_, current_xid_);

        // TODO: Implement actual table locking when SNAPSHOT TABLE STABILITY is used

        return Status::OK;
    }

    Status ConnectionContext::beginNewTransaction(ErrorContext* ctx)
    {
        // Allocate new XID
        uint64_t new_xid = 0;
        Status s = txn_manager_->beginTransaction(proc_id_, new_xid, ctx);
        if (s != Status::OK)
        {
            return s;
        }

        // Update context
        current_xid_ = new_xid;
        xact_start_time_ = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        );

        // Create snapshot if using SNAPSHOT isolation
        if (isolation_level_ == IsolationLevel::SNAPSHOT)
        {
            s = createSnapshot(ctx);
            if (s != Status::OK)
            {
                // Failed to create snapshot - rollback the transaction
                txn_manager_->rollbackTransaction(proc_id_, current_xid_, nullptr);
                current_xid_ = 0;
                return s;
            }
        }

        // Acquire table locks if using SNAPSHOT TABLE STABILITY
        if (isolation_level_ == IsolationLevel::SNAPSHOT_TABLE_STABILITY &&
            !table_reservations_.empty())
        {
            // TODO: Acquire table locks via LockManager
            LOG_DEBUG(LOCK, "TODO: Acquire table locks for %zu reserved tables",
                     table_reservations_.size());
        }

        return Status::OK;
    }

    Status ConnectionContext::endCurrentTransaction(bool commit, ErrorContext* ctx)
    {
        Status s;

        if (commit)
        {
            s = txn_manager_->commitTransaction(proc_id_, current_xid_, ctx);
        }
        else
        {
            s = txn_manager_->rollbackTransaction(proc_id_, current_xid_, ctx);
        }

        // Clear snapshot if any
        snapshot_.reset();

        // Clear transaction state (will be reset by beginNewTransaction)
        current_xid_ = 0;
        xact_start_time_ = std::chrono::microseconds(0);

        return s;
    }

    void ConnectionContext::applyStagedSettings()
    {
        if (settings_staged_)
        {
            isolation_level_ = next_isolation_level_;
            is_read_only_ = next_is_read_only_;
            settings_staged_ = false;

            LOG_DEBUG(TRANSACTION, "Applied staged settings: isolation=%d, read_only=%d",
                     static_cast<int>(isolation_level_), is_read_only_);
        }
    }

    Status ConnectionContext::createSnapshot(ErrorContext* ctx)
    {
        auto snapshot = std::make_unique<TransactionManager::Snapshot>();

        Status s = txn_manager_->getSnapshot(*snapshot, ctx);
        if (s != Status::OK)
        {
            return s;
        }

        snapshot_ = std::move(snapshot);

        LOG_DEBUG(TRANSACTION, "Created snapshot: xmin=%lu, xmax=%lu, active_xids=%zu",
                 snapshot_->xmin, snapshot_->xmax, snapshot_->active_xids.size());

        return Status::OK;
    }

} // namespace scratchbird::core
