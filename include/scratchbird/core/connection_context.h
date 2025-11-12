#pragma once

#include "scratchbird/core/status.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include <cstdint>
#include <memory>
#include <string>
#include <chrono>

namespace scratchbird::core
{
    // Forward declarations
    class Database;
    class TransactionManager;
    class CatalogManager;

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
        SHARED = 0,    // SHARED READ - allows concurrent reads
        PROTECTED = 1, // PROTECTED READ/WRITE - exclusive table access
    };

    // Connection context - per-connection/session state
    // This class manages the always-in-transaction model where every connection
    // always has an active transaction.
    class ConnectionContext
    {
    public:
        // Type alias for UUID-based IDs
        using ID = UuidV7Bytes;

        ConnectionContext(Database *db, uint32_t proc_id);
        ~ConnectionContext();

        // Explicitly delete copy operations (ConnectionContext is non-copyable)
        ConnectionContext(const ConnectionContext &) = delete;
        ConnectionContext &operator=(const ConnectionContext &) = delete;

        // Move operations
        ConnectionContext(ConnectionContext &&) noexcept;
        ConnectionContext &operator=(ConnectionContext &&) noexcept;

        // Thread-local storage access
        // Get the current connection context for this thread
        static ConnectionContext *getCurrent();

        // Set the current connection context for this thread
        static void setCurrent(ConnectionContext *ctx);

        // Convenience method to get current proc_id
        static int32_t getCurrentProcId();

        // Convenience method to get current XID
        static uint64_t getCurrentTransactionId();

        // Initialize connection (start initial transaction)
        Status initialize(ErrorContext *ctx = nullptr);

        // Transaction lifecycle
        // Commit current transaction and start new one atomically
        Status commit(ErrorContext *ctx = nullptr);

        // Rollback current transaction and start new one atomically
        Status rollback(ErrorContext *ctx = nullptr);

        // Start new transaction with specific settings
        // If commit_outstanding is true, commits current transaction first
        // If commit_outstanding is false and settings changed, stages settings for next commit
        Status startTransaction(bool read_only, IsolationLevel isolation_level,
                                bool commit_outstanding, ErrorContext *ctx = nullptr);

        // Transaction state queries
        uint64_t getCurrentXid() const
        {
            return current_xid_;
        }
        uint32_t getProcId() const
        {
            return proc_id_;
        }
        IsolationLevel getIsolationLevel() const
        {
            return isolation_level_;
        }
        bool isReadOnly() const
        {
            return is_read_only_;
        }
        std::chrono::microseconds getTransactionStartTime() const
        {
            return xact_start_time_;
        }

        // Security context queries (Phase 2 - Security System)
        const ID& getCurrentUserId() const { return current_user_id_; }
        const ID& getActiveRoleId() const { return active_role_id_; }
        bool isSuperuser() const { return is_superuser_; }

        // Security context types (Phase 3.1 - SQL Object Permissions)
        enum class SecurityMode : uint8_t
        {
            INVOKER = 0,  // Execute with caller's privileges (default)
            DEFINER = 1   // Execute with owner's privileges
        };

        struct SecurityContext
        {
            ID effective_user_id;      // Who is executing
            ID effective_role_id;      // Active role
            bool is_superuser;         // Superuser flag
            SecurityMode mode;         // DEFINER or INVOKER
            ID object_id;              // Current procedure/function/view ID
        };

        // Security context setters (called during authentication and SET ROLE)
        void setCurrentUser(const ID& user_id, bool is_superuser);
        void setActiveRole(const ID& role_id);
        void clearActiveRole();

        // Security context stack methods (Phase 3.1 - SQL Object Permissions)
        // Push new security context (entering procedure/function/view)
        void pushSecurityContext(const ID& user_id, const ID& role_id,
                                bool is_superuser_flag, SecurityMode mode,
                                const ID& object_id);

        // Pop security context (exiting procedure/function/view)
        void popSecurityContext();

        // Get current effective security context
        SecurityContext getCurrentSecurityContext() const;

        // Check if we're in a DEFINER context
        bool isDefinerContext() const;

        // FIREBIRD MGA: Transaction visibility uses current XID, not snapshots
        // For SNAPSHOT isolation, we simply use the XID at transaction start
        // For READ_COMMITTED, we use the XID at statement start
        // No snapshot structures needed - TIP provides all visibility info

        // Statement XID support (for READ_COMMITTED_READ_CONSISTENCY)
        // Get current statement XID (returns current_xid if no statement-level XID)
        uint64_t getStatementXID() const
        {
            return statement_xid_ != 0 ? statement_xid_ : current_xid_;
        }

        // Create a new statement XID (for READ_COMMITTED_READ_CONSISTENCY)
        // This captures the "current" XID for the statement duration
        void createStatementXID();

        // Clear the statement XID
        void clearStatementXID();

        // Check if termination has been requested (for long transaction monitor)
        // Returns Status::IO_ERROR if termination requested, Status::OK otherwise
        Status checkTerminationRequested(ErrorContext *ctx = nullptr);

        // Connection settings
        void setWaitForLocks(bool wait)
        {
            wait_for_locks_ = wait;
        }
        bool getWaitForLocks() const
        {
            return wait_for_locks_;
        }

        void setLockTimeout(uint32_t timeout_seconds)
        {
            lock_timeout_seconds_ = timeout_seconds;
        }
        uint32_t getLockTimeout() const
        {
            return lock_timeout_seconds_;
        }

        // Table reservation (for SNAPSHOT TABLE STABILITY)
        struct TableReservation
        {
            std::string table_name;
            TableLockMode lock_mode;
            bool for_write;
        };

        Status reserveTables(const std::vector<TableReservation> &reservations,
                             ErrorContext *ctx = nullptr);

    private:
        // Core state
        Database *db_;
        TransactionManager *txn_manager_;
        uint32_t proc_id_;                          // Process ID from ProcArray
        uint64_t current_xid_;                      // Current transaction XID (NEVER 0)
        std::chrono::microseconds xact_start_time_; // Transaction start time

        // Security context (Phase 2 - Security System)
        ID current_user_id_;    // Authenticated user UUID
        ID active_role_id_;     // Active role UUID (from SET ROLE), zero if none
        bool is_superuser_;     // Cached superuser flag for performance

        // Security context stack (Phase 3.1 - SQL Object Permissions)
        // SecurityMode and SecurityContext types defined in public section above
        std::vector<SecurityContext> security_stack_;

        // Transaction settings
        IsolationLevel isolation_level_; // Current isolation level
        bool is_read_only_;              // Is transaction read-only?
        bool wait_for_locks_;            // Wait for locks or fail immediately?
        uint32_t lock_timeout_seconds_;  // Lock timeout (0 = no wait, UINT32_MAX = wait forever)

        // Staged settings (from START TRANSACTION without COMMIT OUTSTANDING)
        bool settings_staged_;                // Are there staged settings?
        IsolationLevel next_isolation_level_; // Staged isolation level
        bool next_is_read_only_;              // Staged read-only flag

        // FIREBIRD MGA: No snapshot structures needed
        // Transaction visibility is determined by current_xid_ and TIP lookups

        // Statement XID for READ_COMMITTED_READ_CONSISTENCY
        // Captures XID at statement start for consistent reads within statement
        // 0 = no statement-level XID (use transaction XID)
        uint64_t statement_xid_;

        // Table reservations for SNAPSHOT TABLE STABILITY
        std::vector<TableReservation> table_reservations_;

        // Subtransaction/Savepoint support (Issue 2.15)
        struct Savepoint
        {
            std::string name;                    // Savepoint name
            uint32_t level;                      // Nesting level
            uint64_t xid;                        // Transaction ID at savepoint creation
            uint32_t command_id;                 // Command ID at savepoint (for future use)

            // FIREBIRD MGA: No snapshot needed - use XID for visibility
            // XID field above is sufficient for rollback

            // Track tuples modified after this savepoint
            // For rollback, we need to mark inserted tuples as aborted
            // and clear xmax on deleted tuples
            std::vector<std::pair<uint32_t, uint16_t>> inserted_tids;  // (page_id, item_id)
            std::vector<std::pair<uint32_t, uint16_t>> deleted_tids;   // (page_id, item_id)
        };

        std::vector<Savepoint> savepoint_stack_;  // Stack of active savepoints
        uint32_t savepoint_level_ = 0;            // Current savepoint nesting level
        uint32_t command_id_ = 0;                 // Current command ID within transaction

        // Thread-local storage
        static thread_local ConnectionContext *current_;

        // Helper methods
        Status beginNewTransaction(ErrorContext *ctx);
        Status endCurrentTransaction(bool commit, ErrorContext *ctx);
        void applyStagedSettings();
        Status createSnapshot(ErrorContext *ctx);

    public:
        // Savepoint operations (Issue 2.15: Subtransaction Support)

        /**
         * Create a new savepoint
         * @param name Savepoint name (must be unique in current transaction)
         * @param ctx Error context
         * @return Status code
         */
        Status createSavepoint(const std::string &name, ErrorContext *ctx = nullptr);

        /**
         * Rollback to a savepoint
         * Undoes all changes made after the named savepoint was created
         * @param name Savepoint name to rollback to
         * @param ctx Error context
         * @return Status code
         */
        Status rollbackToSavepoint(const std::string &name, ErrorContext *ctx = nullptr);

        /**
         * Release a savepoint
         * Removes the savepoint from the stack, keeping all changes
         * @param name Savepoint name to release
         * @param ctx Error context
         * @return Status code
         */
        Status releaseSavepoint(const std::string &name, ErrorContext *ctx = nullptr);

        /**
         * Track a tuple insertion for potential savepoint rollback
         * Called by heap_page.cpp after inserting a tuple
         * @param page_id Page ID where tuple was inserted
         * @param item_id Item ID of inserted tuple
         */
        void trackTupleInsertion(uint32_t page_id, uint16_t item_id);

        /**
         * Track a tuple deletion for potential savepoint rollback
         * Called by heap_page.cpp after marking a tuple deleted
         * @param page_id Page ID where tuple was deleted
         * @param item_id Item ID of deleted tuple
         */
        void trackTupleDeletion(uint32_t page_id, uint16_t item_id);

        /**
         * Increment command ID (for statement-level tracking)
         * @return New command ID
         */
        uint32_t incrementCommandId()
        {
            return ++command_id_;
        }

        /**
         * Get current command ID
         * @return Current command ID
         */
        uint32_t getCommandId() const
        {
            return command_id_;
        }
    };

} // namespace scratchbird::core
