#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/logger.h"
#include "scratchbird/core/lock_manager.h"
#include "scratchbird/core/catalog_manager.h"
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
        , statement_snapshot_(std::move(other.statement_snapshot_))
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
            statement_snapshot_ = std::move(other.statement_snapshot_);
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
        // Store the reservations
        // Actual lock acquisition happens in beginNewTransaction() for SNAPSHOT TABLE STABILITY
        table_reservations_ = reservations;

        LOG_DEBUG(LOCK, "Reserved %zu tables for transaction: proc_id=%u, xid=%lu",
                 reservations.size(), proc_id_, current_xid_);

        // Note: Table locks will be acquired when the next transaction starts with
        // SNAPSHOT TABLE STABILITY isolation level. See beginNewTransaction().

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

        // Update isolation level in ProcArray for transaction marker tracking
        s = ProcArrayManager::setIsolationLevel(proc_id_, static_cast<uint8_t>(isolation_level_), ctx);
        if (s != Status::OK)
        {
            LOG_WARNING(TRANSACTION, "Failed to set isolation level in ProcArray for proc_id %u", proc_id_);
            // Non-fatal - continue with transaction
        }

        // Update read-only flag in ProcArray for long transaction monitoring
        s = ProcArrayManager::setTransactionReadOnly(proc_id_, is_read_only_, ctx);
        if (s != Status::OK)
        {
            LOG_WARNING(TRANSACTION, "Failed to set read-only flag in ProcArray for proc_id %u", proc_id_);
            // Non-fatal - continue with transaction
        }

        // Update transaction start time in ProcArray for long transaction monitoring
        s = ProcArrayManager::setTransactionStartTime(proc_id_, xact_start_time_.count(), ctx);
        if (s != Status::OK)
        {
            LOG_WARNING(TRANSACTION, "Failed to set transaction start time in ProcArray for proc_id %u", proc_id_);
            // Non-fatal - continue with transaction
        }

        // Update transaction markers (OAT, OST) after starting new transaction
        s = txn_manager_->updateTransactionMarkers(ctx);
        if (s != Status::OK)
        {
            LOG_WARNING(TRANSACTION, "Failed to update transaction markers");
            // Non-fatal - continue with transaction
        }

        // Create snapshot if using SNAPSHOT or SNAPSHOT_TABLE_STABILITY isolation
        if (isolation_level_ == IsolationLevel::SNAPSHOT ||
            isolation_level_ == IsolationLevel::SNAPSHOT_TABLE_STABILITY)
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
            LockManager* lock_mgr = db_->lock_manager();
            CatalogManager* catalog = db_->catalog_manager();

            if (!lock_mgr || !catalog)
            {
                LOG_ERROR(LOCK, "LockManager or CatalogManager not available");
                return Status::IO_ERROR;
            }

            // First, get the default schema ID (use "[sys]" for now)
            CatalogManager::SchemaInfo schema_info;
            s = catalog->getSchema("[sys]", schema_info, ctx);
            if (s != Status::OK)
            {
                LOG_ERROR(LOCK, "Failed to get [sys] schema for table locking");
                return s;
            }

            // Acquire locks for each reserved table
            for (const auto& reservation : table_reservations_)
            {
                // Look up table to get its UUID
                CatalogManager::TableInfo table_info;
                s = catalog->getTable(schema_info.schema_id, reservation.table_name, table_info, ctx);
                if (s != Status::OK)
                {
                    LOG_ERROR(LOCK, "Failed to find table '%s' for locking",
                             reservation.table_name.c_str());
                    // Release all locks acquired so far
                    lock_mgr->releaseAllLocks(proc_id_, nullptr);
                    return s;
                }

                // Convert TableLockMode to LockMode
                LockMode lock_mode;
                if (reservation.lock_mode == TableLockMode::SHARED)
                {
                    // SHARED allows concurrent reads
                    lock_mode = LockMode::LOCK_SHARE;
                }
                else // PROTECTED
                {
                    // PROTECTED gives exclusive access
                    lock_mode = LockMode::LOCK_ACCESS_EXCLUSIVE;
                }

                // Create lock tag for table
                LockTag tag;
                tag.target_type = LockTarget::LOCK_TARGET_TABLE;
                tag.object_uuid = table_info.table_id;
                tag.page_num = 0;
                tag.offset_num = 0;
                tag.padding = 0;

                // Acquire the lock
                uint32_t timeout_ms = lock_timeout_seconds_ * 1000;
                s = lock_mgr->acquireLock(proc_id_, tag, lock_mode, wait_for_locks_, timeout_ms, ctx);
                if (s != Status::OK)
                {
                    LOG_ERROR(LOCK, "Failed to acquire %s lock on table '%s'",
                             reservation.lock_mode == TableLockMode::SHARED ? "SHARED" : "PROTECTED",
                             reservation.table_name.c_str());
                    // Release all locks acquired so far
                    lock_mgr->releaseAllLocks(proc_id_, nullptr);
                    return s;
                }

                LOG_DEBUG(LOCK, "Acquired %s lock on table '%s' for transaction: proc_id=%u, xid=%lu",
                         reservation.lock_mode == TableLockMode::SHARED ? "SHARED" : "PROTECTED",
                         reservation.table_name.c_str(), proc_id_, current_xid_);
            }
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

        // Release all locks held by this transaction
        LockManager* lock_mgr = db_->lock_manager();
        if (lock_mgr)
        {
            Status lock_status = lock_mgr->releaseAllLocks(proc_id_, ctx);
            if (lock_status != Status::OK)
            {
                LOG_WARNING(LOCK, "Failed to release locks after %s: proc_id=%u, xid=%lu",
                           commit ? "commit" : "rollback", proc_id_, current_xid_);
                // Non-fatal - continue with transaction cleanup
            }
            else
            {
                LOG_DEBUG(LOCK, "Released all locks for transaction: proc_id=%u, xid=%lu",
                         proc_id_, current_xid_);
            }
        }

        // Clear snapshots if any
        snapshot_.reset();
        statement_snapshot_.reset();

        // Clear transaction state (will be reset by beginNewTransaction)
        current_xid_ = 0;
        xact_start_time_ = std::chrono::microseconds(0);

        // Update transaction markers (OAT, OST) after ending transaction
        Status marker_status = txn_manager_->updateTransactionMarkers(ctx);
        if (marker_status != Status::OK)
        {
            LOG_WARNING(TRANSACTION, "Failed to update transaction markers after %s",
                       commit ? "commit" : "rollback");
            // Non-fatal - markers will be updated on next transaction
        }

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

    Status ConnectionContext::createStatementSnapshot(ErrorContext* ctx)
    {
        auto snapshot = std::make_unique<TransactionManager::Snapshot>();

        Status s = txn_manager_->getSnapshot(*snapshot, ctx);
        if (s != Status::OK)
        {
            return s;
        }

        statement_snapshot_ = std::move(snapshot);

        LOG_DEBUG(TRANSACTION, "Created statement snapshot: xmin=%lu, xmax=%lu, active_xids=%zu",
                 statement_snapshot_->xmin, statement_snapshot_->xmax, statement_snapshot_->active_xids.size());

        return Status::OK;
    }

    void ConnectionContext::clearStatementSnapshot()
    {
        if (statement_snapshot_)
        {
            LOG_DEBUG(TRANSACTION, "Cleared statement snapshot");
            statement_snapshot_.reset();
        }
    }

} // namespace scratchbird::core
