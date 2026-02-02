/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#pragma once

#include "scratchbird/core/status.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/monitoring_stats.h"
#include <cstdint>
#include <memory>
#include <string>
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <vector>

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

    enum class ReadCommittedMode : uint8_t
    {
        DEFAULT = 0,
        READ_CONSISTENCY = 1,
        RECORD_VERSION = 2,
        NO_RECORD_VERSION = 3
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

        // Statement tracking for dormant reattach diagnostics.
        // Values intentionally align with CatalogManager dormant enums for direct persistence.
        enum class StatementType : uint8_t
        {
            UNKNOWN = 0,
            DDL = 1,
            DML = 2,
            OTHER = 3
        };

        enum class StatementStatus : uint8_t
        {
            UNKNOWN = 0,
            IN_PROGRESS = 1,
            COMPLETED = 2,
            FAILED = 3
        };

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

        // Prepare current transaction for 2PC and start a new one atomically.
        Status prepareTransaction(const std::string& gid, ErrorContext *ctx = nullptr);

        // End current transaction without starting a new one (disconnect/cleanup path).
        Status shutdownTransaction(ErrorContext *ctx = nullptr);

        // Start new transaction with specific settings
        // If commit_outstanding is true, commits current transaction first
        // If commit_outstanding is false and settings changed, stages settings for next commit
        Status startTransaction(bool read_only, IsolationLevel isolation_level,
                                bool commit_outstanding, ErrorContext *ctx = nullptr);
        Status startTransaction(bool read_only, IsolationLevel isolation_level,
                                ReadCommittedMode read_committed_mode,
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
        bool isBulkWriteMode() const
        {
            return bulk_write_mode_;
        }
        void setBulkWriteMode(bool enabled)
        {
            bulk_write_mode_ = enabled;
        }
        std::chrono::microseconds getTransactionStartTime() const
        {
            return xact_start_time_;
        }

        // Attachment + protocol session identifiers
        const ID& attachmentId() const { return attachment_id_; }
        void setAttachmentId(const ID& id) { attachment_id_ = id; }

        const ID& protocolSessionId() const { return protocol_session_id_; }
        void setProtocolSessionId(const ID& id) { protocol_session_id_ = id; }

        // Session/AuthKey binding (Plan 03)
        const ID& sessionId() const { return session_id_; }
        const ID& authKeyId() const { return authkey_id_; }
        const std::string& emulationMode() const { return emulation_mode_; }
        uint64_t policyEpochGlobal() const { return policy_epoch_global_; }
        uint64_t policyEpochTable() const { return policy_epoch_table_; }
        void setSessionContext(const ID& session_id, const ID& authkey_id,
                               const std::string& emulation_mode,
                               uint64_t policy_epoch_global,
                               uint64_t policy_epoch_table);
        ID effectiveSessionId() const;

        // Security context queries (Phase 2 - Security System)
        const ID& getCurrentUserId() const { return current_user_id_; }
        const ID& getActiveRoleId() const { return active_role_id_; }
        bool isSuperuser() const { return is_superuser_; }

        // WP-5 EXEC-M3: Session user tracking for SET/RESET SESSION AUTHORIZATION
        // Session user is the original authenticated user, never changes during session
        // Current user is the effective user, can be changed by SET SESSION AUTHORIZATION
        const ID& getSessionUserId() const { return session_user_id_; }
        bool isSessionSuperuser() const { return session_is_superuser_; }

        // Schema context queries (Phase 2.1 - Executor Schema Operations)
        const ID& getCurrentSchemaId() const { return current_schema_id_; }
        void setCurrentSchemaId(const ID& schema_id) { current_schema_id_ = schema_id; }

        // Schema navigation support (hierarchical schemas)
        const std::string& current_schema() const { return current_schema_name_; }
        void set_current_schema(const std::string& schema) { current_schema_name_ = schema; }
        const std::vector<std::string>& search_path() const { return search_path_; }
        void set_search_path(const std::vector<std::string>& paths) { search_path_ = paths; }
        const std::string& dialect_tag() const { return dialect_tag_; }
        void set_dialect_tag(const std::string& tag) { dialect_tag_ = tag; }

        // Session variables (user-visible via SHOW)
        void setSessionVariable(const std::string& name, const std::string& value);
        bool getSessionVariable(const std::string& name, std::string& out) const;
        void clearSessionVariable(const std::string& name);
        void clearSessionVariables();
        std::vector<std::pair<std::string, std::string>> listSessionVariables() const;

        // Security context types (Phase 3.1 - SQL Object Permissions)
        enum class RoleSwitchPolicy : uint8_t
        {
            DEFER = 0,   // Stage until next transaction (legacy behavior)
            COMMIT = 1,  // Commit before switching roles
            ROLLBACK = 2,// Rollback before switching roles
            ERROR = 3    // Reject role switch during active transaction
        };

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
            ID session_id;             // Bound session UUID
            ID authkey_id;             // Bound AuthKey UUID
            std::string emulation_mode; // Emulation mode/dialect tag
            uint64_t policy_epoch_global = 0;
            uint64_t policy_epoch_table = 0;
        };

        // Security context setters (called during authentication and SET ROLE)
        void setCurrentUser(const ID& user_id, bool is_superuser);
        void setActiveRole(const ID& role_id);
        void clearActiveRole();
        bool hasStagedSecurityContext() const { return security_context_staged_; }
        void applyStagedSecurityContext();

        void setRoleSwitchPolicy(RoleSwitchPolicy policy);
        RoleSwitchPolicy roleSwitchPolicy() const { return role_switch_policy_; }

        // Security context stack methods (Phase 3.1 - SQL Object Permissions)
        // Push new security context (entering procedure/function/view)
        void pushSecurityContext(const ID& user_id, const ID& role_id,
                                bool is_superuser_flag, SecurityMode mode,
                                const ID& object_id);

        // Pop security context (exiting procedure/function/view)
        void popSecurityContext();

        // Get current effective security context
        SecurityContext getCurrentSecurityContext() const;
        std::vector<SecurityContext> listSecurityContextStack() const;

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

        void stageTransactionSettings(IsolationLevel isolation_level,
                                      bool read_only,
                                      bool wait_for_locks,
                                      uint32_t lock_timeout_seconds,
                                      ReadCommittedMode read_committed_mode);
        void stageDefaultTransactionSettings();

        void setAutocommitMode(bool enabled)
        {
            autocommit_mode_ = enabled;
        }
        bool autocommitMode() const
        {
            return autocommit_mode_;
        }
        void setAutocommitSuspended(bool suspended)
        {
            autocommit_suspended_ = suspended;
        }
        bool autocommitSuspended() const
        {
            return autocommit_suspended_;
        }

        // Statement tracking (used for dormant reattach inspection).
        void beginStatementTracking(const std::string& sql);
        void endStatementTrackingSuccess(int64_t rows_affected);
        void endStatementTrackingFailure(uint32_t error_code, const std::string& sqlstate);
        void updateStatementSourceLocation(int32_t line, int32_t column);

        void recordPageRead();
        void recordPageWrite();
        void recordPageFetch();
        void recordPageMark();

        IOStatsSnapshot snapshotConnectionIoStats() const;
        IOStatsSnapshot snapshotTransactionIoStats() const;
        IOStatsSnapshot snapshotStatementIoStats() const;
        bool statementIoActive() const { return statement_io_active_; }
        uint64_t currentStatementId() const { return statement_id_; }

        void recordTableDmlDelta(const ID& table_id,
                                 uint64_t inserts,
                                 uint64_t updates,
                                 uint64_t deletes,
                                 uint64_t hot_updates = 0,
                                 uint64_t newpage_updates = 0);

        uint64_t lastActivityTime() const { return last_activity_time_; }
        std::string sessionSettingsJson() const;
        const std::string& lastStatementText() const { return last_statement_text_; }
        uint64_t lastStatementHash() const { return last_statement_hash_; }
        StatementType lastStatementType() const { return last_statement_type_; }
        StatementStatus lastStatementStatus() const { return last_statement_status_; }
        uint64_t lastStatementTime() const { return last_statement_time_; }
        int32_t lastStatementLine() const { return last_statement_line_; }
        int32_t lastStatementColumn() const { return last_statement_column_; }
        int64_t lastRowsAffected() const { return last_rows_affected_; }
        uint32_t lastErrorCode() const { return last_error_code_; }
        const std::string& lastSqlstate() const { return last_sqlstate_; }

        // Session settings (Firebird ISQL compatibility)
        void set_sql_dialect(uint8_t dialect)
        {
            sql_dialect_ = dialect;
        }
        uint8_t sql_dialect() const
        {
            return sql_dialect_;
        }

        void set_charset(const std::string& charset)
        {
            charset_ = charset;
        }
        const std::string& charset() const
        {
            return charset_;
        }

        void set_statement_timeout(uint32_t timeout_seconds)
        {
            statement_timeout_seconds_ = timeout_seconds;
            default_statement_timeout_seconds_ = timeout_seconds;
            statement_timeout_local_override_ = false;
        }
        uint32_t statement_timeout() const
        {
            return statement_timeout_seconds_;
        }
        void set_statement_timeout_local(uint32_t timeout_seconds)
        {
            statement_timeout_seconds_ = timeout_seconds;
            statement_timeout_local_override_ = true;
        }
        void reset_statement_timeout_local()
        {
            statement_timeout_seconds_ = default_statement_timeout_seconds_;
            statement_timeout_local_override_ = false;
        }
        uint32_t default_statement_timeout() const
        {
            return default_statement_timeout_seconds_;
        }
        bool statement_timeout_local_override() const
        {
            return statement_timeout_local_override_;
        }

        void setReadCommittedMode(ReadCommittedMode mode)
        {
            read_committed_mode_ = mode;
        }
        ReadCommittedMode getReadCommittedMode() const
        {
            return read_committed_mode_;
        }

        // Table reservation (for SNAPSHOT TABLE STABILITY)
        struct TableReservation
        {
            ID table_id{};
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
        ID current_user_id_;    // Effective user UUID (can be changed by SET SESSION AUTHORIZATION)
        ID active_role_id_;     // Active role UUID (from SET ROLE), zero if none
        bool is_superuser_;     // Cached superuser flag for performance (for current_user)

        // WP-5 EXEC-M3: Session user tracking for SET/RESET SESSION AUTHORIZATION
        ID session_user_id_;         // Original authenticated user UUID (never changes)
        bool session_is_superuser_;  // Original superuser flag (never changes)

        // Schema context (Phase 2.1 - Executor Schema Operations)
        ID current_schema_id_;  // Current schema UUID (default: PUBLIC schema)

        // Schema navigation support (hierarchical schemas)
        std::string current_schema_name_ = "public";  // Current schema name
        std::vector<std::string> search_path_ = {"public", "sys"};  // Schema search path
        std::string dialect_tag_ = "scratchbird";
        mutable std::mutex session_vars_mutex_;
        std::unordered_map<std::string, std::string> session_variables_;

        // Security context stack (Phase 3.1 - SQL Object Permissions)
        // SecurityMode and SecurityContext types defined in public section above
        std::vector<SecurityContext> security_stack_;

        // Transaction settings
        IsolationLevel isolation_level_; // Current isolation level
        ReadCommittedMode read_committed_mode_; // READ COMMITTED variant
        bool is_read_only_;              // Is transaction read-only?
        bool bulk_write_mode_ = false;   // COPY/loader bulk write hint
        bool wait_for_locks_;            // Wait for locks or fail immediately?
        uint32_t lock_timeout_seconds_;  // Lock timeout (0 = no wait, UINT32_MAX = wait forever)
        bool autocommit_mode_ = false;   // Autocommit mode (session-level)
        bool autocommit_suspended_ = false;  // Explicit transaction block active

        RoleSwitchPolicy role_switch_policy_ = RoleSwitchPolicy::ERROR;

        // Attachment/session identifiers (minimal attachment model placeholder)
        ID attachment_id_;
        ID protocol_session_id_;
        ID session_id_;
        ID authkey_id_;
        std::string emulation_mode_ = "native";
        uint64_t policy_epoch_global_ = 0;
        uint64_t policy_epoch_table_ = 0;

        bool security_context_initialized_ = false;
        bool security_context_staged_ = false;
        bool pending_user_change_ = false;
        bool pending_role_change_ = false;
        bool pending_session_change_ = false;
        ID pending_user_id_;
        ID pending_role_id_;
        bool pending_is_superuser_ = false;
        ID pending_session_id_;
        ID pending_authkey_id_;
        std::string pending_emulation_mode_;
        uint64_t pending_policy_epoch_global_ = 0;
        uint64_t pending_policy_epoch_table_ = 0;

        // Session settings (Firebird ISQL compatibility)
        uint8_t sql_dialect_ = 3;             // SQL dialect (1, 2, or 3) - default 3 (modern)
        std::string charset_ = "UTF8";        // Connection character set
        uint32_t statement_timeout_seconds_ = 0;  // Statement timeout (0 = no limit)
        uint32_t default_statement_timeout_seconds_ = 0; // Session default timeout
        bool statement_timeout_local_override_ = false;

        // Last statement tracking (for dormant reattach inspection)
        std::string last_statement_text_;
        uint64_t last_statement_hash_ = 0;
        std::string last_statement_query_type_;
        StatementType last_statement_type_ = StatementType::UNKNOWN;
        StatementStatus last_statement_status_ = StatementStatus::UNKNOWN;
        uint64_t last_statement_time_ = 0;
        int32_t last_statement_line_ = 0;
        int32_t last_statement_column_ = 0;
        int64_t last_rows_affected_ = 0;
        uint32_t last_error_code_ = 0;
        std::string last_sqlstate_;
        uint64_t last_activity_time_ = 0;

        IOStats connection_io_stats_;
        IOStats transaction_io_stats_;
        IOStats statement_io_stats_;
        bool statement_io_active_ = false;
        uint64_t statement_id_ = 0;
        std::unordered_map<ID, TableDmlDelta, IDHash> pending_table_deltas_;

        // Default transaction settings (for AND NO CHAIN resets)
        IsolationLevel default_isolation_level_;
        ReadCommittedMode default_read_committed_mode_;
        bool default_is_read_only_;
        bool default_wait_for_locks_;
        uint32_t default_lock_timeout_seconds_;

        // Staged settings (from START TRANSACTION without COMMIT OUTSTANDING)
        bool settings_staged_;                // Are there staged settings?
        IsolationLevel next_isolation_level_; // Staged isolation level
        ReadCommittedMode next_read_committed_mode_;
        bool next_is_read_only_;              // Staged read-only flag
        bool next_wait_for_locks_;            // Staged wait mode
        uint32_t next_lock_timeout_seconds_;  // Staged lock timeout

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

        // P2-7: Deferred Constraint Support
        /**
         * Structure to track a deferred constraint check
         */
        struct DeferredConstraintCheck
        {
            ID constraint_id;                    // Which constraint to validate
            ID table_id;                         // Table involved
            std::string constraint_name;         // For error messages
            std::vector<std::string> column_values; // Values that may violate
            uint32_t command_id;                 // When this check was deferred
        };

        /**
         * Check if a constraint is currently deferred
         * @param constraint_id Constraint to check
         * @return true if constraint checking is deferred
         */
        bool isConstraintDeferred(const ID& constraint_id) const;

        /**
         * Set constraint deferral state for this transaction
         * @param constraint_id Constraint ID (or empty for ALL)
         * @param deferred true=DEFERRED, false=IMMEDIATE
         */
        void setConstraintDeferred(const ID& constraint_id, bool deferred);

        /**
         * Set all constraints to deferred or immediate
         * @param deferred true=DEFERRED, false=IMMEDIATE
         */
        void setAllConstraintsDeferred(bool deferred);

        /**
         * Add a deferred constraint check to be validated at commit
         * @param check The constraint check to defer
         */
        void addDeferredConstraintCheck(const DeferredConstraintCheck& check);

        /**
         * Validate all deferred constraint checks
         * Called at COMMIT time or when SET CONSTRAINTS ... IMMEDIATE is issued
         * @param ctx Error context
         * @return Status::OK if all constraints pass, error status otherwise
         */
        Status validateDeferredConstraints(ErrorContext* ctx = nullptr);

        /**
         * Clear all deferred constraint checks (on ROLLBACK)
         */
        void clearDeferredConstraints();

        /**
         * Initialize constraint states from catalog (at transaction start)
         * Sets initial deferral state based on INITIALLY DEFERRED/IMMEDIATE
         */
        void initializeConstraintStates();

        // P2-21: Prepared Statement Cache
        /**
         * Structure representing a prepared statement
         */
        struct PreparedStatement
        {
            std::string name;                           // Statement name/handle
            std::string sql_text;                       // Original SQL text
            std::vector<uint8_t> bytecode;              // Compiled SBLR bytecode
            std::vector<uint16_t> param_types;          // Expected parameter types
            size_t param_count;                         // Number of parameters
            std::chrono::steady_clock::time_point created_at;  // When prepared
            std::chrono::steady_clock::time_point last_used;   // Last execution time
            int64_t created_at_micros = 0;              // Wall clock time (micros)
            int64_t last_used_micros = 0;               // Wall clock time (micros)
            uint64_t execution_count;                   // Times executed
        };

        struct PreparedStatementInfo
        {
            std::string name;
            std::string sql_text;
            size_t param_count = 0;
            uint64_t execution_count = 0;
            int64_t created_at_micros = 0;
            int64_t last_used_micros = 0;
            size_t memory_bytes = 0;
        };

        /**
         * Prepare a SQL statement for repeated execution
         * @param name Statement name/handle
         * @param sql_text SQL text to prepare
         * @param bytecode Compiled bytecode
         * @param param_types Parameter type hints
         * @param ctx Error context
         * @return Status code
         */
        Status prepareStatement(const std::string& name, const std::string& sql_text,
                               const std::vector<uint8_t>& bytecode,
                               const std::vector<uint16_t>& param_types,
                               ErrorContext* ctx = nullptr);

        /**
         * Get a prepared statement by name
         * @param name Statement name
         * @return Pointer to PreparedStatement or nullptr if not found
         */
        PreparedStatement* getPreparedStatement(const std::string& name);

        /**
         * Deallocate a prepared statement
         * @param name Statement name (empty string = deallocate ALL)
         * @param ctx Error context
         * @return Status code
         */
        Status deallocatePreparedStatement(const std::string& name, ErrorContext* ctx = nullptr);

        /**
         * Update last_used and execution_count for a statement
         * @param name Statement name
         */
        void recordStatementExecution(const std::string& name);

        /**
         * Get prepared statement cache statistics
         * @param count Output: number of cached statements
         * @param total_bytes Output: approximate memory usage
         */
        void getPreparedStatementStats(size_t& count, size_t& total_bytes) const;

        /**
         * List prepared statements for monitoring.
         */
        void listPreparedStatements(std::vector<PreparedStatementInfo>& out) const;

        /**
         * Set maximum number of cached prepared statements
         * @param max_count Maximum statements (0 = unlimited)
         */
        void setMaxPreparedStatements(size_t max_count) { max_prepared_statements_ = max_count; }

        /**
         * Get maximum number of cached prepared statements
         * @return Current limit
         */
        size_t getMaxPreparedStatements() const { return max_prepared_statements_; }

        // Cleanup temporary tables and views for end-of-session handling.
        Status cleanupTempTablesOnSessionEnd(ErrorContext *ctx);

    private:
        // P2-7: Deferred constraint tracking
        std::unordered_map<ID, bool, IDHash> constraint_deferred_state_; // Per-constraint deferral
        std::vector<DeferredConstraintCheck> deferred_checks_;           // Pending checks
        bool all_constraints_deferred_ = false;                          // Global deferral flag

        // P2-21: Prepared Statement Cache
        std::unordered_map<std::string, PreparedStatement> prepared_statements_;
        size_t max_prepared_statements_ = 100;  // Default limit

        // LRU eviction helper
        void evictOldestPreparedStatement();

        Status cleanupTempTablesOnCommit(ErrorContext *ctx);
    };

} // namespace scratchbird::core
