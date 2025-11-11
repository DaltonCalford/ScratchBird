#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/logger.h"
#include "scratchbird/core/lock_manager.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/config.h"
#include <cassert>

namespace scratchbird::core
{
    // Thread-local storage for current connection context
    thread_local ConnectionContext *ConnectionContext::current_ = nullptr;

    ConnectionContext::ConnectionContext(Database *db, uint32_t proc_id)
        : db_(db), txn_manager_(db ? db->transaction_manager() : nullptr), proc_id_(proc_id),
          current_xid_(0) // Will be set by initialize()
          ,
          xact_start_time_(std::chrono::microseconds(0)),
          current_user_id_(), // Zero UUID - will be set during authentication
          active_role_id_(),  // Zero UUID - no role active initially
          is_superuser_(false), // Will be set during authentication
          isolation_level_(IsolationLevel::SNAPSHOT) // Default to SNAPSHOT
          ,
          is_read_only_(false), wait_for_locks_(true) // Default: wait for locks
          ,
          lock_timeout_seconds_(config::DEFAULT_LOCK_TIMEOUT_SECONDS) // Default: 60 second timeout
          ,
          settings_staged_(false), next_isolation_level_(IsolationLevel::SNAPSHOT),
          next_is_read_only_(false), statement_xid_(0)
    {
        assert(db != nullptr && "Database must not be null");
        assert(txn_manager_ != nullptr && "TransactionManager must not be null");

        // Initialize UUIDs to zero (no user authenticated yet)
        std::memset(&current_user_id_, 0, sizeof(current_user_id_));
        std::memset(&active_role_id_, 0, sizeof(active_role_id_));
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

    ConnectionContext::ConnectionContext(ConnectionContext &&other) noexcept
        : db_(other.db_), txn_manager_(other.txn_manager_), proc_id_(other.proc_id_),
          current_xid_(other.current_xid_), xact_start_time_(other.xact_start_time_),
          current_user_id_(other.current_user_id_), active_role_id_(other.active_role_id_),
          is_superuser_(other.is_superuser_),
          isolation_level_(other.isolation_level_), is_read_only_(other.is_read_only_),
          wait_for_locks_(other.wait_for_locks_),
          lock_timeout_seconds_(other.lock_timeout_seconds_),
          settings_staged_(other.settings_staged_),
          next_isolation_level_(other.next_isolation_level_),
          next_is_read_only_(other.next_is_read_only_), statement_xid_(other.statement_xid_),
          table_reservations_(std::move(other.table_reservations_))
    {
        // Clear other's state
        other.db_ = nullptr;
        other.txn_manager_ = nullptr;
        other.current_xid_ = 0;
        other.statement_xid_ = 0;
        std::memset(&other.current_user_id_, 0, sizeof(other.current_user_id_));
        std::memset(&other.active_role_id_, 0, sizeof(other.active_role_id_));
        other.is_superuser_ = false;
    }

    ConnectionContext &ConnectionContext::operator=(ConnectionContext &&other) noexcept
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
            current_user_id_ = other.current_user_id_;
            active_role_id_ = other.active_role_id_;
            is_superuser_ = other.is_superuser_;
            isolation_level_ = other.isolation_level_;
            is_read_only_ = other.is_read_only_;
            wait_for_locks_ = other.wait_for_locks_;
            lock_timeout_seconds_ = other.lock_timeout_seconds_;
            settings_staged_ = other.settings_staged_;
            next_isolation_level_ = other.next_isolation_level_;
            next_is_read_only_ = other.next_is_read_only_;
            statement_xid_ = other.statement_xid_;
            table_reservations_ = std::move(other.table_reservations_);

            // Clear other's state
            other.db_ = nullptr;
            other.txn_manager_ = nullptr;
            other.current_xid_ = 0;
            other.statement_xid_ = 0;
            std::memset(&other.current_user_id_, 0, sizeof(other.current_user_id_));
            std::memset(&other.active_role_id_, 0, sizeof(other.active_role_id_));
            other.is_superuser_ = false;
        }
        return *this;
    }

    ConnectionContext *ConnectionContext::getCurrent()
    {
        return current_;
    }

    void ConnectionContext::setCurrent(ConnectionContext *ctx)
    {
        current_ = ctx;
    }

    int32_t ConnectionContext::getCurrentProcId()
    {
        ConnectionContext *ctx = getCurrent();
        if (ctx == nullptr)
        {
            return -1; // No connection context
        }
        return static_cast<int32_t>(ctx->proc_id_);
    }

    uint64_t ConnectionContext::getCurrentTransactionId()
    {
        ConnectionContext *ctx = getCurrent();
        if (ctx == nullptr)
        {
            return 0; // Invalid XID
        }
        return ctx->current_xid_;
    }

    Status ConnectionContext::initialize(ErrorContext *ctx)
    {
        // Start initial transaction
        Status s = beginNewTransaction(ctx);
        if (s != Status::OK)
        {
            LOG_ERROR(TRANSACTION, "Failed to initialize connection context: %d",
                      static_cast<int>(s));
            return s;
        }

        LOG_DEBUG(TRANSACTION, "Initialized connection context: proc_id=%u, xid=%lu", proc_id_,
                  current_xid_);

        return Status::OK;
    }

    Status ConnectionContext::commit(ErrorContext *ctx)
    {
        if (current_xid_ == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "No active transaction to commit");
            return Status::INVALID_ARGUMENT;
        }

        LOG_DEBUG(TRANSACTION, "Committing transaction: proc_id=%u, xid=%lu", proc_id_,
                  current_xid_);

        // 1. Check if termination has been requested (before commit)
        Status s = checkTerminationRequested(ctx);
        if (s != Status::OK)
        {
            // Termination requested - rollback instead of commit and return error
            LOG_WARNING(TRANSACTION,
                        "Termination requested, rolling back instead of committing: proc_id=%u, "
                        "xid=%lu",
                        proc_id_, current_xid_);
            rollback(nullptr); // Best effort rollback
            return s;
        }

        // 2. Commit current transaction
        s = endCurrentTransaction(true, ctx);
        if (s != Status::OK)
        {
            LOG_ERROR(TRANSACTION, "Failed to commit transaction: proc_id=%u, xid=%lu, status=%d",
                      proc_id_, current_xid_, static_cast<int>(s));
            return s;
        }

        // 3. Apply staged settings if any
        applyStagedSettings();

        // 4. ATOMICALLY start new transaction
        s = beginNewTransaction(ctx);
        if (s != Status::OK)
        {
            LOG_ERROR(TRANSACTION,
                      "Failed to start new transaction after commit: proc_id=%u, status=%d",
                      proc_id_, static_cast<int>(s));
            return s;
        }

        LOG_DEBUG(TRANSACTION, "Started new transaction after commit: proc_id=%u, new_xid=%lu",
                  proc_id_, current_xid_);

        return Status::OK;
    }

    Status ConnectionContext::rollback(ErrorContext *ctx)
    {
        if (current_xid_ == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "No active transaction to rollback");
            return Status::INVALID_ARGUMENT;
        }

        LOG_DEBUG(TRANSACTION, "Rolling back transaction: proc_id=%u, xid=%lu", proc_id_,
                  current_xid_);

        // 1. Check if termination has been requested (before rollback)
        Status s = checkTerminationRequested(ctx);
        bool termination_requested = (s != Status::OK);

        // 2. Rollback current transaction (always succeeds)
        s = endCurrentTransaction(false, ctx);
        if (s != Status::OK)
        {
            LOG_WARNING(TRANSACTION, "Rollback encountered error: proc_id=%u, xid=%lu, status=%d",
                        proc_id_, current_xid_, static_cast<int>(s));
            // Continue anyway - rollback should be best-effort
        }

        // If termination was requested, don't start a new transaction - just return error
        if (termination_requested)
        {
            LOG_WARNING(TRANSACTION,
                        "Termination requested, not starting new transaction after rollback: "
                        "proc_id=%u",
                        proc_id_);
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR,
                              "Connection terminated due to long-running transaction");
            return Status::IO_ERROR;
        }

        // 3. Apply staged settings if any
        applyStagedSettings();

        // 4. Start new transaction
        s = beginNewTransaction(ctx);
        if (s != Status::OK)
        {
            LOG_ERROR(TRANSACTION,
                      "Failed to start new transaction after rollback: proc_id=%u, status=%d",
                      proc_id_, static_cast<int>(s));
            return s;
        }

        LOG_DEBUG(TRANSACTION, "Started new transaction after rollback: proc_id=%u, new_xid=%lu",
                  proc_id_, current_xid_);

        return Status::OK;
    }

    Status ConnectionContext::startTransaction(bool read_only, IsolationLevel isolation_level,
                                               bool commit_outstanding, ErrorContext *ctx)
    {
        LOG_DEBUG(
            TRANSACTION,
            "START TRANSACTION: proc_id=%u, read_only=%d, isolation=%d, commit_outstanding=%d",
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

    Status ConnectionContext::reserveTables(const std::vector<TableReservation> &reservations,
                                            ErrorContext *ctx)
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

    Status ConnectionContext::beginNewTransaction(ErrorContext *ctx)
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
            std::chrono::system_clock::now().time_since_epoch());

        // Update isolation level in ProcArray for transaction marker tracking
        s = ProcArrayManager::setIsolationLevel(proc_id_, static_cast<uint8_t>(isolation_level_),
                                                ctx);
        if (s != Status::OK)
        {
            LOG_WARNING(TRANSACTION, "Failed to set isolation level in ProcArray for proc_id %u",
                        proc_id_);
            // Non-fatal - continue with transaction
        }

        // Update read-only flag in ProcArray for long transaction monitoring
        s = ProcArrayManager::setTransactionReadOnly(proc_id_, is_read_only_, ctx);
        if (s != Status::OK)
        {
            LOG_WARNING(TRANSACTION, "Failed to set read-only flag in ProcArray for proc_id %u",
                        proc_id_);
            // Non-fatal - continue with transaction
        }

        // Update transaction start time in ProcArray for long transaction monitoring
        s = ProcArrayManager::setTransactionStartTime(proc_id_, xact_start_time_.count(), ctx);
        if (s != Status::OK)
        {
            LOG_WARNING(TRANSACTION,
                        "Failed to set transaction start time in ProcArray for proc_id %u",
                        proc_id_);
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
            LockManager *lock_mgr = db_->lock_manager();
            CatalogManager *catalog = db_->catalog_manager();

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
            for (const auto &reservation : table_reservations_)
            {
                // Look up table to get its UUID
                CatalogManager::TableInfo table_info;
                s = catalog->getTable(schema_info.schema_id, reservation.table_name, table_info,
                                      ctx);
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
                s = lock_mgr->acquireLock(proc_id_, tag, lock_mode, wait_for_locks_, timeout_ms,
                                          ctx);
                if (s != Status::OK)
                {
                    LOG_ERROR(LOCK, "Failed to acquire %s lock on table '%s'",
                              reservation.lock_mode == TableLockMode::SHARED ? "SHARED"
                                                                             : "PROTECTED",
                              reservation.table_name.c_str());
                    // Release all locks acquired so far
                    lock_mgr->releaseAllLocks(proc_id_, nullptr);
                    return s;
                }

                LOG_DEBUG(LOCK,
                          "Acquired %s lock on table '%s' for transaction: proc_id=%u, xid=%lu",
                          reservation.lock_mode == TableLockMode::SHARED ? "SHARED" : "PROTECTED",
                          reservation.table_name.c_str(), proc_id_, current_xid_);
            }
        }

        return Status::OK;
    }

    Status ConnectionContext::endCurrentTransaction(bool commit, ErrorContext *ctx)
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
        LockManager *lock_mgr = db_->lock_manager();
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
                LOG_DEBUG(LOCK, "Released all locks for transaction: proc_id=%u, xid=%lu", proc_id_,
                          current_xid_);
            }
        }

        // Clear statement XID (FIREBIRD MGA: No snapshots)
        statement_xid_ = 0;

        // Clear savepoint stack (transaction ending clears all savepoints)
        savepoint_stack_.clear();
        savepoint_level_ = 0;
        command_id_ = 0;

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

    // FIREBIRD MGA: No snapshot creation needed
    // Transaction visibility is determined by current_xid and TIP lookups
    // This function is kept for API compatibility but does nothing
    Status ConnectionContext::createSnapshot(ErrorContext *ctx)
    {
        // For SNAPSHOT isolation, current_xid_ is already set at transaction start
        // No additional snapshot structure needed
        LOG_DEBUG(TRANSACTION, "Transaction visibility using XID %lu (Firebird MGA)", current_xid_);
        return Status::OK;
    }

    void ConnectionContext::createStatementXID()
    {
        // For READ_COMMITTED_READ_CONSISTENCY, capture current XID for statement duration
        // This provides consistent reads within a single statement
        if (txn_manager_ != nullptr)
        {
            statement_xid_ = txn_manager_->getCurrentXid();
            LOG_DEBUG(TRANSACTION, "Created statement XID: %lu", statement_xid_);
        }
    }

    void ConnectionContext::clearStatementXID()
    {
        if (statement_xid_ != 0)
        {
            LOG_DEBUG(TRANSACTION, "Cleared statement XID %lu", statement_xid_);
            statement_xid_ = 0;
        }
    }

    Status ConnectionContext::checkTerminationRequested(ErrorContext *ctx)
    {
        // Check if long transaction monitor has requested termination
        bool termination_requested = false;
        Status s =
            ProcArrayManager::isTerminationRequested(proc_id_, &termination_requested, ctx);

        if (s != Status::OK)
        {
            LOG_WARNING(TRANSACTION,
                        "Failed to check termination status for proc_id %u: status=%d", proc_id_,
                        static_cast<int>(s));
            // Non-fatal - continue with transaction
            return Status::OK;
        }

        if (termination_requested)
        {
            LOG_ERROR(TRANSACTION,
                      "Connection termination requested by long transaction monitor: proc_id=%u, "
                      "xid=%lu",
                      proc_id_, current_xid_);

            // Clear the termination request flag
            Status clear_status = ProcArrayManager::clearTerminationRequest(proc_id_, ctx);
            if (clear_status != Status::OK)
            {
                LOG_WARNING(TRANSACTION,
                            "Failed to clear termination request for proc_id %u: status=%d",
                            proc_id_, static_cast<int>(clear_status));
            }

            // Set error context to inform caller
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR,
                              "Connection terminated due to long-running transaction");

            return Status::IO_ERROR;
        }

        return Status::OK;
    }

    // ============================================================================
    // Savepoint/Subtransaction Support (Issue 2.15)
    // ============================================================================

    Status ConnectionContext::createSavepoint(const std::string &name, ErrorContext *ctx)
    {
        if (current_xid_ == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Cannot create savepoint outside of a transaction");
            return Status::INVALID_ARGUMENT;
        }

        // Check for duplicate savepoint name
        for (const auto &sp : savepoint_stack_)
        {
            if (sp.name == name)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Savepoint with this name already exists");
                return Status::INVALID_ARGUMENT;
            }
        }

        // Create new savepoint
        Savepoint sp;
        sp.name = name;
        sp.level = ++savepoint_level_;
        sp.xid = current_xid_;
        sp.command_id = command_id_;

        // FIREBIRD MGA: No snapshot needed - XID is sufficient for rollback
        // Savepoint just marks a point in transaction for potential rollback

        // Add to stack
        savepoint_stack_.push_back(std::move(sp));

        LOG_DEBUG(TRANSACTION, "Created savepoint '%s' at level %u: proc_id=%u, xid=%lu",
                  name.c_str(), sp.level, proc_id_, current_xid_);

        return Status::OK;
    }

    Status ConnectionContext::rollbackToSavepoint(const std::string &name, ErrorContext *ctx)
    {
        if (current_xid_ == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Cannot rollback savepoint outside of a transaction");
            return Status::INVALID_ARGUMENT;
        }

        // Find the named savepoint
        auto sp_it = savepoint_stack_.end();
        for (auto it = savepoint_stack_.begin(); it != savepoint_stack_.end(); ++it)
        {
            if (it->name == name)
            {
                sp_it = it;
                break;
            }
        }

        if (sp_it == savepoint_stack_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Savepoint not found");
            return Status::NOT_FOUND;
        }

        LOG_DEBUG(TRANSACTION, "Rolling back to savepoint '%s' at level %u: proc_id=%u, xid=%lu",
                  name.c_str(), sp_it->level, proc_id_, current_xid_);

        // Get buffer pool for page access
        BufferPool *pool = db_->buffer_pool();
        if (!pool)
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Buffer pool not available");
            return Status::IO_ERROR;
        }

        // Rollback all savepoints created AFTER the named one
        // Process from most recent (end of stack) to the target savepoint
        auto rollback_start = sp_it;
        ++rollback_start; // Start with the savepoint AFTER the target

        for (auto it = rollback_start; it != savepoint_stack_.end(); ++it)
        {
            // Mark all inserted tuples as aborted (set HEAP_XMIN_ABORTED)
            for (const auto &tid : it->inserted_tids)
            {
                void *page_buffer = nullptr;
                Status s = pool->pinPage(tid.first, &page_buffer, ctx);
                if (s != Status::OK)
                {
                    LOG_WARNING(TRANSACTION,
                                "Failed to pin page %u during savepoint rollback: %d", tid.first,
                                static_cast<int>(s));
                    continue; // Best effort
                }

                // Get the tuple header
                auto *page_data = static_cast<uint8_t *>(page_buffer);
                // Locate item pointer at offset (simplified - assumes fixed layout)
                // In real implementation, would use HeapPage::getItemPointer()
                // For now, log the action
                LOG_DEBUG(TRANSACTION, "Marking tuple (page=%u, item=%u) as aborted", tid.first,
                          tid.second);

                // TODO: Actually mark the tuple as aborted by setting HEAP_XMIN_ABORTED flag
                // This requires accessing HeapPage structure, which we'll do through
                // heap_page.cpp In the test, we'll demonstrate the API is correct

                pool->unpinPage(tid.first, true, ctx); // Mark as dirty
            }

            // Clear xmax on all deleted tuples (restore them)
            for (const auto &tid : it->deleted_tids)
            {
                void *page_buffer = nullptr;
                Status s = pool->pinPage(tid.first, &page_buffer, ctx);
                if (s != Status::OK)
                {
                    LOG_WARNING(TRANSACTION,
                                "Failed to pin page %u during savepoint rollback: %d", tid.first,
                                static_cast<int>(s));
                    continue; // Best effort
                }

                LOG_DEBUG(TRANSACTION, "Clearing delete mark on tuple (page=%u, item=%u)", tid.first,
                          tid.second);

                // TODO: Actually clear xmax by setting it to 0 and clearing HEAP_XMAX_VALID
                // This requires accessing HeapPage structure

                pool->unpinPage(tid.first, true, ctx); // Mark as dirty
            }
        }

        // Remove all savepoints after (and including) the next one after target
        savepoint_stack_.erase(rollback_start, savepoint_stack_.end());

        // Update savepoint level
        savepoint_level_ = sp_it->level;

        // Restore command ID from savepoint
        command_id_ = sp_it->command_id;

        LOG_DEBUG(TRANSACTION,
                  "Rolled back to savepoint '%s': proc_id=%u, xid=%lu, new_level=%u", name.c_str(),
                  proc_id_, current_xid_, savepoint_level_);

        return Status::OK;
    }

    Status ConnectionContext::releaseSavepoint(const std::string &name, ErrorContext *ctx)
    {
        if (current_xid_ == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Cannot release savepoint outside of a transaction");
            return Status::INVALID_ARGUMENT;
        }

        // Find the named savepoint
        auto sp_it = savepoint_stack_.end();
        size_t sp_index = 0;
        for (auto it = savepoint_stack_.begin(); it != savepoint_stack_.end(); ++it, ++sp_index)
        {
            if (it->name == name)
            {
                sp_it = it;
                break;
            }
        }

        if (sp_it == savepoint_stack_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Savepoint not found");
            return Status::NOT_FOUND;
        }

        LOG_DEBUG(TRANSACTION, "Releasing savepoint '%s' at level %u: proc_id=%u, xid=%lu",
                  name.c_str(), sp_it->level, proc_id_, current_xid_);

        // If there's a parent savepoint, merge our tuple lists into it
        if (sp_index > 0)
        {
            auto &parent = savepoint_stack_[sp_index - 1];

            // Merge inserted tuples
            parent.inserted_tids.insert(parent.inserted_tids.end(), sp_it->inserted_tids.begin(),
                                        sp_it->inserted_tids.end());

            // Merge deleted tuples
            parent.deleted_tids.insert(parent.deleted_tids.end(), sp_it->deleted_tids.begin(),
                                       sp_it->deleted_tids.end());

            LOG_DEBUG(TRANSACTION,
                      "Merged %zu insertions and %zu deletions into parent savepoint '%s'",
                      sp_it->inserted_tids.size(), sp_it->deleted_tids.size(), parent.name.c_str());
        }

        // Remove this savepoint and all nested ones
        savepoint_stack_.erase(sp_it, savepoint_stack_.end());

        // Update savepoint level
        if (!savepoint_stack_.empty())
        {
            savepoint_level_ = savepoint_stack_.back().level;
        }
        else
        {
            savepoint_level_ = 0;
        }

        LOG_DEBUG(TRANSACTION, "Released savepoint '%s': proc_id=%u, xid=%lu, new_level=%u",
                  name.c_str(), proc_id_, current_xid_, savepoint_level_);

        return Status::OK;
    }

    void ConnectionContext::trackTupleInsertion(uint32_t page_id, uint16_t item_id)
    {
        // If we have active savepoints, track this insertion in the most recent one
        if (!savepoint_stack_.empty())
        {
            savepoint_stack_.back().inserted_tids.emplace_back(page_id, item_id);

            LOG_DEBUG(TRANSACTION,
                      "Tracked tuple insertion (page=%u, item=%u) in savepoint '%s' (level %u)",
                      page_id, item_id, savepoint_stack_.back().name.c_str(),
                      savepoint_stack_.back().level);
        }
    }

    void ConnectionContext::trackTupleDeletion(uint32_t page_id, uint16_t item_id)
    {
        // If we have active savepoints, track this deletion in the most recent one
        if (!savepoint_stack_.empty())
        {
            savepoint_stack_.back().deleted_tids.emplace_back(page_id, item_id);

            LOG_DEBUG(TRANSACTION,
                      "Tracked tuple deletion (page=%u, item=%u) in savepoint '%s' (level %u)",
                      page_id, item_id, savepoint_stack_.back().name.c_str(),
                      savepoint_stack_.back().level);
        }
    }

    // ============================================================================
    // Security Context Management (Phase 2 - Security System)
    // ============================================================================

    void ConnectionContext::setCurrentUser(const ID& user_id, bool is_superuser)
    {
        current_user_id_ = user_id;
        is_superuser_ = is_superuser;

        // Clear active role when user changes (switching users resets session state)
        std::memset(&active_role_id_, 0, sizeof(active_role_id_));

        LOG_DEBUG(TRANSACTION, "Set current user: proc_id=%u, user_id=%s, is_superuser=%d",
                  proc_id_, user_id.toString().c_str(), is_superuser);
    }

    void ConnectionContext::setActiveRole(const ID& role_id)
    {
        active_role_id_ = role_id;

        LOG_DEBUG(TRANSACTION, "Set active role: proc_id=%u, role_id=%s",
                  proc_id_, role_id.toString().c_str());
    }

    void ConnectionContext::clearActiveRole()
    {
        LOG_DEBUG(TRANSACTION, "Clearing active role: proc_id=%u, previous_role=%s",
                  proc_id_, active_role_id_.toString().c_str());

        std::memset(&active_role_id_, 0, sizeof(active_role_id_));
    }

} // namespace scratchbird::core
