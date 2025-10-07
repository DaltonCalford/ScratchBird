#pragma once

#include "scratchbird/core/status.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/error_context.h"
#include <cstdint>
#include <memory>
#include <string>
#include <chrono>

namespace scratchbird::core
{
    // Forward declarations
    class Database;
    class TransactionManager;

    // Isolation levels supported by ScratchBird
    enum class IsolationLevel : uint8_t
    {
        // Read Committed - each statement sees latest committed data
        READ_COMMITTED = 0,

        // Read Committed Read Consistency - statement-level snapshot (Firebird 4.0+)
        READ_COMMITTED_READ_CONSISTENCY = 1,

        // Snapshot - point-in-time snapshot at transaction start (default)
        SNAPSHOT = 2,

        // Snapshot Table Stability - table-level locking for consistency
        SNAPSHOT_TABLE_STABILITY = 3
    };

    // Transaction lock mode for table reservation (Firebird-style)
    enum class TableLockMode : uint8_t
    {
        SHARED = 0,     // SHARED READ - allows concurrent reads
        PROTECTED = 1,  // PROTECTED READ/WRITE - exclusive table access
    };

    // Connection context - per-connection/session state
    // This class manages the always-in-transaction model where every connection
    // always has an active transaction.
    class ConnectionContext
    {
    public:
        ConnectionContext(Database* db, uint32_t proc_id);
        ~ConnectionContext();

        // Explicitly delete copy operations (ConnectionContext is non-copyable)
        ConnectionContext(const ConnectionContext&) = delete;
        ConnectionContext& operator=(const ConnectionContext&) = delete;

        // Move operations
        ConnectionContext(ConnectionContext&&) noexcept;
        ConnectionContext& operator=(ConnectionContext&&) noexcept;

        // Thread-local storage access
        // Get the current connection context for this thread
        static ConnectionContext* getCurrent();

        // Set the current connection context for this thread
        static void setCurrent(ConnectionContext* ctx);

        // Convenience method to get current proc_id
        static int32_t getCurrentProcId();

        // Convenience method to get current XID
        static uint64_t getCurrentTransactionId();

        // Initialize connection (start initial transaction)
        Status initialize(ErrorContext* ctx = nullptr);

        // Transaction lifecycle
        // Commit current transaction and start new one atomically
        Status commit(ErrorContext* ctx = nullptr);

        // Rollback current transaction and start new one atomically
        Status rollback(ErrorContext* ctx = nullptr);

        // Start new transaction with specific settings
        // If commit_outstanding is true, commits current transaction first
        // If commit_outstanding is false and settings changed, stages settings for next commit
        Status startTransaction(bool read_only, IsolationLevel isolation_level,
                              bool commit_outstanding, ErrorContext* ctx = nullptr);

        // Transaction state queries
        uint64_t getCurrentXid() const { return current_xid_; }
        uint32_t getProcId() const { return proc_id_; }
        IsolationLevel getIsolationLevel() const { return isolation_level_; }
        bool isReadOnly() const { return is_read_only_; }
        std::chrono::microseconds getTransactionStartTime() const { return xact_start_time_; }

        // Get current snapshot (for SNAPSHOT isolation)
        const TransactionManager::Snapshot* getSnapshot() const { return snapshot_.get(); }

        // Connection settings
        void setWaitForLocks(bool wait) { wait_for_locks_ = wait; }
        bool getWaitForLocks() const { return wait_for_locks_; }

        void setLockTimeout(uint32_t timeout_seconds) { lock_timeout_seconds_ = timeout_seconds; }
        uint32_t getLockTimeout() const { return lock_timeout_seconds_; }

        // Table reservation (for SNAPSHOT TABLE STABILITY)
        struct TableReservation
        {
            std::string table_name;
            TableLockMode lock_mode;
            bool for_write;
        };

        Status reserveTables(const std::vector<TableReservation>& reservations,
                           ErrorContext* ctx = nullptr);

    private:
        // Core state
        Database* db_;
        TransactionManager* txn_manager_;
        uint32_t proc_id_;                      // Process ID from ProcArray
        uint64_t current_xid_;                  // Current transaction XID (NEVER 0)
        std::chrono::microseconds xact_start_time_;  // Transaction start time

        // Transaction settings
        IsolationLevel isolation_level_;        // Current isolation level
        bool is_read_only_;                     // Is transaction read-only?
        bool wait_for_locks_;                   // Wait for locks or fail immediately?
        uint32_t lock_timeout_seconds_;         // Lock timeout (0 = no wait, UINT32_MAX = wait forever)

        // Staged settings (from START TRANSACTION without COMMIT OUTSTANDING)
        bool settings_staged_;                  // Are there staged settings?
        IsolationLevel next_isolation_level_;   // Staged isolation level
        bool next_is_read_only_;                // Staged read-only flag

        // Snapshot for SNAPSHOT isolation
        std::unique_ptr<TransactionManager::Snapshot> snapshot_;

        // Table reservations for SNAPSHOT TABLE STABILITY
        std::vector<TableReservation> table_reservations_;

        // Thread-local storage
        static thread_local ConnectionContext* current_;

        // Helper methods
        Status beginNewTransaction(ErrorContext* ctx);
        Status endCurrentTransaction(bool commit, ErrorContext* ctx);
        void applyStagedSettings();
        Status createSnapshot(ErrorContext* ctx);
    };

} // namespace scratchbird::core
