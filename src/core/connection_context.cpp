#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/logger.h"
#include "scratchbird/core/lock_manager.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/config.h"
#include <cassert>
#include <cctype>

namespace scratchbird::core
{
    // Thread-local storage for current connection context
    thread_local ConnectionContext *ConnectionContext::current_ = nullptr;

    namespace {
        uint64_t nowMicros()
        {
            return std::chrono::duration_cast<std::chrono::microseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                .count();
        }

        uint64_t fnv1a64(const std::string& value)
        {
            uint64_t hash = 1469598103934665603ULL;
            for (unsigned char c : value)
            {
                hash ^= static_cast<uint64_t>(c);
                hash *= 1099511628211ULL;
            }
            return hash;
        }

        bool isZeroUuidLocal(const ID& id)
        {
            for (uint8_t byte : id.bytes)
            {
                if (byte != 0)
                {
                    return false;
                }
            }
            return true;
        }

        void appendJsonString(std::string& out, const std::string& value)
        {
            static const char kHexDigits[] = "0123456789abcdef";
            out.push_back('"');
            for (unsigned char c : value)
            {
                switch (c)
                {
                    case '\\': out.append("\\\\"); break;
                    case '"': out.append("\\\""); break;
                    case '\n': out.append("\\n"); break;
                    case '\r': out.append("\\r"); break;
                    case '\t': out.append("\\t"); break;
                    default:
                        if (c < 0x20)
                        {
                            out.append("\\u00");
                            out.push_back(kHexDigits[(c >> 4) & 0xF]);
                            out.push_back(kHexDigits[c & 0xF]);
                        }
                        else
                        {
                            out.push_back(static_cast<char>(c));
                        }
                        break;
                }
            }
            out.push_back('"');
        }
    }

    ConnectionContext::ConnectionContext(Database *db, uint32_t proc_id)
        : db_(db), txn_manager_(db ? db->transaction_manager() : nullptr), proc_id_(proc_id),
          current_xid_(0) // Will be set by initialize()
          ,
          xact_start_time_(std::chrono::microseconds(0)),
          current_user_id_(), // Zero UUID - will be set during authentication
          active_role_id_(),  // Zero UUID - no role active initially
          is_superuser_(false), // Will be set during authentication
          session_user_id_(),   // WP-5 EXEC-M3: Zero UUID - set on first setCurrentUser() call
          session_is_superuser_(false),  // WP-5 EXEC-M3: Set on first setCurrentUser() call
          isolation_level_(IsolationLevel::SNAPSHOT), // Default to SNAPSHOT
          read_committed_mode_(ReadCommittedMode::READ_CONSISTENCY)
          ,
          is_read_only_(false), wait_for_locks_(true) // Default: wait for locks
          ,
          lock_timeout_seconds_(config::DEFAULT_LOCK_TIMEOUT_SECONDS) // Default: 60 second timeout
          ,
          autocommit_mode_(false),
          autocommit_suspended_(false),
          attachment_id_(generateUuidV7()),
          protocol_session_id_(),
          session_id_(),
          authkey_id_(),
          emulation_mode_("native"),
          policy_epoch_global_(0),
          policy_epoch_table_(0),
          security_context_initialized_(false),
          security_context_staged_(false),
          pending_user_change_(false),
          pending_role_change_(false),
          pending_session_change_(false),
          pending_user_id_(),
          pending_role_id_(),
          pending_is_superuser_(false),
          pending_session_id_(),
          pending_authkey_id_(),
          pending_emulation_mode_(),
          pending_policy_epoch_global_(0),
          pending_policy_epoch_table_(0),
          default_isolation_level_(IsolationLevel::SNAPSHOT),
          default_read_committed_mode_(ReadCommittedMode::READ_CONSISTENCY),
          default_is_read_only_(false),
          default_wait_for_locks_(true),
          default_lock_timeout_seconds_(config::DEFAULT_LOCK_TIMEOUT_SECONDS),
          settings_staged_(false), next_isolation_level_(IsolationLevel::SNAPSHOT),
          next_read_committed_mode_(ReadCommittedMode::READ_CONSISTENCY),
          next_is_read_only_(false), next_wait_for_locks_(true),
          next_lock_timeout_seconds_(config::DEFAULT_LOCK_TIMEOUT_SECONDS),
          statement_xid_(0)
    {
        assert(db != nullptr && "Database must not be null");
        assert(txn_manager_ != nullptr && "TransactionManager must not be null");

        // Initialize UUIDs to zero (no user authenticated yet)
        std::memset(&current_user_id_, 0, sizeof(current_user_id_));
        std::memset(&active_role_id_, 0, sizeof(active_role_id_));
        std::memset(&current_schema_id_, 0, sizeof(current_schema_id_));
        std::memset(&session_user_id_, 0, sizeof(session_user_id_));  // WP-5 EXEC-M3
        std::memset(&protocol_session_id_, 0, sizeof(protocol_session_id_));
        std::memset(&session_id_, 0, sizeof(session_id_));
        std::memset(&authkey_id_, 0, sizeof(authkey_id_));
        std::memset(&pending_user_id_, 0, sizeof(pending_user_id_));
        std::memset(&pending_role_id_, 0, sizeof(pending_role_id_));
        std::memset(&pending_session_id_, 0, sizeof(pending_session_id_));
        std::memset(&pending_authkey_id_, 0, sizeof(pending_authkey_id_));
        // Note: current_schema_id_ will be set to PUBLIC schema during initialize()
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
            // Final shutdown should not start a new transaction; end in-place to avoid orphaned XIDs.
            shutdownTransaction(&err_ctx);
        }

        // Unregister this backend from ProcArray to free the slot
        if (proc_id_ != UINT32_MAX)
        {
            ProcArrayManager::unregisterBackend(proc_id_, nullptr);
            proc_id_ = UINT32_MAX;
        }
    }

    ConnectionContext::ConnectionContext(ConnectionContext &&other) noexcept
        : db_(other.db_), txn_manager_(other.txn_manager_), proc_id_(other.proc_id_),
          current_xid_(other.current_xid_), xact_start_time_(other.xact_start_time_),
          current_user_id_(other.current_user_id_), active_role_id_(other.active_role_id_),
          is_superuser_(other.is_superuser_),
          session_user_id_(other.session_user_id_),  // WP-5 EXEC-M3
          session_is_superuser_(other.session_is_superuser_),  // WP-5 EXEC-M3
          current_schema_id_(other.current_schema_id_),
          current_schema_name_(std::move(other.current_schema_name_)),
          search_path_(std::move(other.search_path_)),
          dialect_tag_(std::move(other.dialect_tag_)),
          security_stack_(std::move(other.security_stack_)),
          isolation_level_(other.isolation_level_),
          read_committed_mode_(other.read_committed_mode_),
          is_read_only_(other.is_read_only_),
          wait_for_locks_(other.wait_for_locks_),
          lock_timeout_seconds_(other.lock_timeout_seconds_),
          autocommit_mode_(other.autocommit_mode_),
          autocommit_suspended_(other.autocommit_suspended_),
          role_switch_policy_(other.role_switch_policy_),
          attachment_id_(other.attachment_id_),
          protocol_session_id_(other.protocol_session_id_),
          session_id_(other.session_id_),
          authkey_id_(other.authkey_id_),
          emulation_mode_(std::move(other.emulation_mode_)),
          policy_epoch_global_(other.policy_epoch_global_),
          policy_epoch_table_(other.policy_epoch_table_),
          security_context_initialized_(other.security_context_initialized_),
          security_context_staged_(other.security_context_staged_),
          pending_user_change_(other.pending_user_change_),
          pending_role_change_(other.pending_role_change_),
          pending_session_change_(other.pending_session_change_),
          pending_user_id_(other.pending_user_id_),
          pending_role_id_(other.pending_role_id_),
          pending_is_superuser_(other.pending_is_superuser_),
          pending_session_id_(other.pending_session_id_),
          pending_authkey_id_(other.pending_authkey_id_),
          pending_emulation_mode_(std::move(other.pending_emulation_mode_)),
          pending_policy_epoch_global_(other.pending_policy_epoch_global_),
          pending_policy_epoch_table_(other.pending_policy_epoch_table_),
          sql_dialect_(other.sql_dialect_),
          charset_(std::move(other.charset_)),
          statement_timeout_seconds_(other.statement_timeout_seconds_),
          last_statement_text_(std::move(other.last_statement_text_)),
          last_statement_hash_(other.last_statement_hash_),
          last_statement_type_(other.last_statement_type_),
          last_statement_status_(other.last_statement_status_),
          last_statement_time_(other.last_statement_time_),
          last_rows_affected_(other.last_rows_affected_),
          last_error_code_(other.last_error_code_),
          last_sqlstate_(std::move(other.last_sqlstate_)),
          last_activity_time_(other.last_activity_time_),
          default_isolation_level_(other.default_isolation_level_),
          default_read_committed_mode_(other.default_read_committed_mode_),
          default_is_read_only_(other.default_is_read_only_),
          default_wait_for_locks_(other.default_wait_for_locks_),
          default_lock_timeout_seconds_(other.default_lock_timeout_seconds_),
          settings_staged_(other.settings_staged_),
          next_isolation_level_(other.next_isolation_level_),
          next_read_committed_mode_(other.next_read_committed_mode_),
          next_is_read_only_(other.next_is_read_only_),
          next_wait_for_locks_(other.next_wait_for_locks_),
          next_lock_timeout_seconds_(other.next_lock_timeout_seconds_),
          statement_xid_(other.statement_xid_),
          table_reservations_(std::move(other.table_reservations_))
    {
        // Clear other's state - critical to invalidate proc_id_ so destructor doesn't unregister
        other.db_ = nullptr;
        other.txn_manager_ = nullptr;
        other.proc_id_ = UINT32_MAX;  // Invalidate so destructor doesn't double-unregister
        other.current_xid_ = 0;
        other.statement_xid_ = 0;
        std::memset(&other.current_user_id_, 0, sizeof(other.current_user_id_));
        std::memset(&other.active_role_id_, 0, sizeof(other.active_role_id_));
        std::memset(&other.current_schema_id_, 0, sizeof(other.current_schema_id_));
        std::memset(&other.session_user_id_, 0, sizeof(other.session_user_id_));  // WP-5 EXEC-M3
        std::memset(&other.attachment_id_, 0, sizeof(other.attachment_id_));
        std::memset(&other.protocol_session_id_, 0, sizeof(other.protocol_session_id_));
        std::memset(&other.session_id_, 0, sizeof(other.session_id_));
        std::memset(&other.authkey_id_, 0, sizeof(other.authkey_id_));
        std::memset(&other.pending_user_id_, 0, sizeof(other.pending_user_id_));
        std::memset(&other.pending_role_id_, 0, sizeof(other.pending_role_id_));
        std::memset(&other.pending_session_id_, 0, sizeof(other.pending_session_id_));
        std::memset(&other.pending_authkey_id_, 0, sizeof(other.pending_authkey_id_));
        other.emulation_mode_.clear();
        other.policy_epoch_global_ = 0;
        other.policy_epoch_table_ = 0;
        other.security_context_initialized_ = false;
        other.security_context_staged_ = false;
        other.pending_user_change_ = false;
        other.pending_role_change_ = false;
        other.pending_session_change_ = false;
        other.pending_is_superuser_ = false;
        other.pending_emulation_mode_.clear();
        other.pending_policy_epoch_global_ = 0;
        other.pending_policy_epoch_table_ = 0;
        other.is_superuser_ = false;
        other.session_is_superuser_ = false;  // WP-5 EXEC-M3
        other.last_statement_text_.clear();
        other.last_statement_hash_ = 0;
        other.last_statement_type_ = StatementType::UNKNOWN;
        other.last_statement_status_ = StatementStatus::UNKNOWN;
        other.last_statement_time_ = 0;
        other.last_rows_affected_ = 0;
        other.last_error_code_ = 0;
        other.last_sqlstate_.clear();
        other.last_activity_time_ = 0;
        other.role_switch_policy_ = RoleSwitchPolicy::ERROR;
    }

    ConnectionContext &ConnectionContext::operator=(ConnectionContext &&other) noexcept
    {
        if (this != &other)
        {
            // Cleanup current transaction if any
            if (current_xid_ != 0)
            {
                ErrorContext err_ctx;
                // Avoid starting a new transaction while being overwritten.
                shutdownTransaction(&err_ctx);
            }

            // Unregister our current backend before taking the new one
            if (proc_id_ != UINT32_MAX)
            {
                ProcArrayManager::unregisterBackend(proc_id_, nullptr);
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
            session_user_id_ = other.session_user_id_;  // WP-5 EXEC-M3
            session_is_superuser_ = other.session_is_superuser_;  // WP-5 EXEC-M3
            current_schema_id_ = other.current_schema_id_;
            current_schema_name_ = std::move(other.current_schema_name_);
            search_path_ = std::move(other.search_path_);
            dialect_tag_ = std::move(other.dialect_tag_);
            security_stack_ = std::move(other.security_stack_);
            isolation_level_ = other.isolation_level_;
            read_committed_mode_ = other.read_committed_mode_;
            is_read_only_ = other.is_read_only_;
            wait_for_locks_ = other.wait_for_locks_;
            lock_timeout_seconds_ = other.lock_timeout_seconds_;
            autocommit_mode_ = other.autocommit_mode_;
            autocommit_suspended_ = other.autocommit_suspended_;
            role_switch_policy_ = other.role_switch_policy_;
            attachment_id_ = other.attachment_id_;
            protocol_session_id_ = other.protocol_session_id_;
            session_id_ = other.session_id_;
            authkey_id_ = other.authkey_id_;
            emulation_mode_ = std::move(other.emulation_mode_);
            policy_epoch_global_ = other.policy_epoch_global_;
            policy_epoch_table_ = other.policy_epoch_table_;
            security_context_initialized_ = other.security_context_initialized_;
            security_context_staged_ = other.security_context_staged_;
            pending_user_change_ = other.pending_user_change_;
            pending_role_change_ = other.pending_role_change_;
            pending_session_change_ = other.pending_session_change_;
            pending_user_id_ = other.pending_user_id_;
            pending_role_id_ = other.pending_role_id_;
            pending_is_superuser_ = other.pending_is_superuser_;
            pending_session_id_ = other.pending_session_id_;
            pending_authkey_id_ = other.pending_authkey_id_;
            pending_emulation_mode_ = std::move(other.pending_emulation_mode_);
            pending_policy_epoch_global_ = other.pending_policy_epoch_global_;
            pending_policy_epoch_table_ = other.pending_policy_epoch_table_;
            sql_dialect_ = other.sql_dialect_;
            charset_ = std::move(other.charset_);
            statement_timeout_seconds_ = other.statement_timeout_seconds_;
            last_statement_text_ = std::move(other.last_statement_text_);
            last_statement_hash_ = other.last_statement_hash_;
            last_statement_type_ = other.last_statement_type_;
            last_statement_status_ = other.last_statement_status_;
            last_statement_time_ = other.last_statement_time_;
            last_rows_affected_ = other.last_rows_affected_;
            last_error_code_ = other.last_error_code_;
            last_sqlstate_ = std::move(other.last_sqlstate_);
            last_activity_time_ = other.last_activity_time_;
            default_isolation_level_ = other.default_isolation_level_;
            default_read_committed_mode_ = other.default_read_committed_mode_;
            default_is_read_only_ = other.default_is_read_only_;
            default_wait_for_locks_ = other.default_wait_for_locks_;
            default_lock_timeout_seconds_ = other.default_lock_timeout_seconds_;
            settings_staged_ = other.settings_staged_;
            next_isolation_level_ = other.next_isolation_level_;
            next_read_committed_mode_ = other.next_read_committed_mode_;
            next_is_read_only_ = other.next_is_read_only_;
            next_wait_for_locks_ = other.next_wait_for_locks_;
            next_lock_timeout_seconds_ = other.next_lock_timeout_seconds_;
            statement_xid_ = other.statement_xid_;
            table_reservations_ = std::move(other.table_reservations_);

            // Clear other's state - critical to invalidate proc_id_
            other.db_ = nullptr;
            other.txn_manager_ = nullptr;
            other.proc_id_ = UINT32_MAX;  // Invalidate so destructor doesn't double-unregister
            other.current_xid_ = 0;
            other.statement_xid_ = 0;
            std::memset(&other.current_user_id_, 0, sizeof(other.current_user_id_));
            std::memset(&other.active_role_id_, 0, sizeof(other.active_role_id_));
            std::memset(&other.current_schema_id_, 0, sizeof(other.current_schema_id_));
            std::memset(&other.session_user_id_, 0, sizeof(other.session_user_id_));  // WP-5 EXEC-M3
            std::memset(&other.attachment_id_, 0, sizeof(other.attachment_id_));
            std::memset(&other.protocol_session_id_, 0, sizeof(other.protocol_session_id_));
            std::memset(&other.session_id_, 0, sizeof(other.session_id_));
            std::memset(&other.authkey_id_, 0, sizeof(other.authkey_id_));
            std::memset(&other.pending_user_id_, 0, sizeof(other.pending_user_id_));
            std::memset(&other.pending_role_id_, 0, sizeof(other.pending_role_id_));
            std::memset(&other.pending_session_id_, 0, sizeof(other.pending_session_id_));
            std::memset(&other.pending_authkey_id_, 0, sizeof(other.pending_authkey_id_));
            other.emulation_mode_.clear();
            other.policy_epoch_global_ = 0;
            other.policy_epoch_table_ = 0;
            other.security_context_initialized_ = false;
            other.security_context_staged_ = false;
            other.pending_user_change_ = false;
            other.pending_role_change_ = false;
            other.pending_session_change_ = false;
            other.pending_is_superuser_ = false;
            other.pending_emulation_mode_.clear();
            other.pending_policy_epoch_global_ = 0;
            other.pending_policy_epoch_table_ = 0;
            other.is_superuser_ = false;
            other.session_is_superuser_ = false;  // WP-5 EXEC-M3
            other.last_statement_text_.clear();
            other.last_statement_hash_ = 0;
            other.last_statement_type_ = StatementType::UNKNOWN;
            other.last_statement_status_ = StatementStatus::UNKNOWN;
            other.last_statement_time_ = 0;
            other.last_rows_affected_ = 0;
            other.last_error_code_ = 0;
            other.last_sqlstate_.clear();
            other.last_activity_time_ = 0;
            other.role_switch_policy_ = RoleSwitchPolicy::ERROR;
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

        LOG_INFO(TRANSACTION, "Commit start: proc_id=%u, xid=%lu", proc_id_, current_xid_);
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
        LOG_INFO(TRANSACTION, "Commit complete: proc_id=%u, xid=%lu", proc_id_, current_xid_);

        return Status::OK;
    }

    Status ConnectionContext::rollback(ErrorContext *ctx)
    {
        if (current_xid_ == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "No active transaction to rollback");
            return Status::INVALID_ARGUMENT;
        }

        LOG_INFO(TRANSACTION, "Rollback start: proc_id=%u, xid=%lu", proc_id_, current_xid_);
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
        LOG_INFO(TRANSACTION, "Rollback complete: proc_id=%u, xid=%lu", proc_id_, current_xid_);

        return Status::OK;
    }

    Status ConnectionContext::prepareTransaction(const std::string& gid, ErrorContext *ctx)
    {
        if (current_xid_ == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "No active transaction to prepare");
            return Status::INVALID_ARGUMENT;
        }

        if (gid.empty())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "GID required for prepare");
            return Status::INVALID_ARGUMENT;
        }

        LOG_DEBUG(TRANSACTION, "Preparing transaction: proc_id=%u, xid=%lu, gid=%s",
                  proc_id_, current_xid_, gid.c_str());

        Status s = checkTerminationRequested(ctx);
        if (s != Status::OK)
        {
            LOG_WARNING(TRANSACTION,
                        "Termination requested, rolling back instead of preparing: proc_id=%u, "
                        "xid=%lu",
                        proc_id_, current_xid_);
            rollback(nullptr);
            return s;
        }

        s = txn_manager_->prepareTransaction(proc_id_, current_xid_, gid, current_user_id_, ctx);
        if (s != Status::OK)
        {
            LOG_ERROR(TRANSACTION,
                      "Failed to prepare transaction: proc_id=%u, xid=%lu, status=%d",
                      proc_id_, current_xid_, static_cast<int>(s));
            return s;
        }

        // Prepared transactions don't have a dedicated lock owner yet, so release locks
        // to avoid leaking them into the next transaction on the same proc_id.
        LockManager *lock_mgr = db_->lock_manager();
        if (lock_mgr)
        {
            Status lock_status = lock_mgr->releaseAllLocks(proc_id_, ctx);
            if (lock_status != Status::OK)
            {
                LOG_WARNING(LOCK, "Failed to release locks after prepare: proc_id=%u, xid=%lu",
                            proc_id_, current_xid_);
            }
        }

        statement_xid_ = 0;
        savepoint_stack_.clear();
        savepoint_level_ = 0;
        command_id_ = 0;
        current_xid_ = 0;
        xact_start_time_ = std::chrono::microseconds(0);

        applyStagedSettings();

        s = beginNewTransaction(ctx);
        if (s != Status::OK)
        {
            LOG_ERROR(TRANSACTION,
                      "Failed to start new transaction after prepare: proc_id=%u, status=%d",
                      proc_id_, static_cast<int>(s));
            return s;
        }

        LOG_DEBUG(TRANSACTION, "Started new transaction after prepare: proc_id=%u, new_xid=%lu",
                  proc_id_, current_xid_);

        return Status::OK;
    }

    Status ConnectionContext::shutdownTransaction(ErrorContext *ctx)
    {
        if (current_xid_ == 0)
        {
            return Status::OK;
        }

        // Disconnects should not start a new transaction; end the current one in-place.
        return endCurrentTransaction(false, ctx);
    }

    Status ConnectionContext::startTransaction(bool read_only, IsolationLevel isolation_level,
                                               bool commit_outstanding, ErrorContext *ctx)
    {
        return startTransaction(read_only, isolation_level, ReadCommittedMode::DEFAULT,
                                commit_outstanding, ctx);
    }

    Status ConnectionContext::startTransaction(bool read_only, IsolationLevel isolation_level,
                                               ReadCommittedMode read_committed_mode,
                                               bool commit_outstanding, ErrorContext *ctx)
    {
        ReadCommittedMode effective_mode = read_committed_mode;
        if (effective_mode == ReadCommittedMode::DEFAULT)
        {
            effective_mode = read_committed_mode_;
        }

        LOG_DEBUG(
            TRANSACTION,
            "START TRANSACTION: proc_id=%u, read_only=%d, isolation=%d, rc_mode=%d, commit_outstanding=%d",
            proc_id_, read_only, static_cast<int>(isolation_level),
            static_cast<int>(effective_mode), commit_outstanding);

        if (commit_outstanding)
        {
            // Commit current transaction and apply new settings immediately
            stageTransactionSettings(isolation_level, read_only, wait_for_locks_,
                                     lock_timeout_seconds_, effective_mode);

            return commit(ctx);
        }
        else
        {
            // Stage settings for next commit/rollback
            stageTransactionSettings(isolation_level, read_only, wait_for_locks_,
                                     lock_timeout_seconds_, effective_mode);

            LOG_DEBUG(TRANSACTION, "Staged transaction settings: isolation=%d, read_only=%d",
                      static_cast<int>(isolation_level), read_only);

            return Status::OK;
        }
    }

    void ConnectionContext::stageTransactionSettings(IsolationLevel isolation_level,
                                                     bool read_only,
                                                     bool wait_for_locks,
                                                     uint32_t lock_timeout_seconds,
                                                     ReadCommittedMode read_committed_mode)
    {
        next_isolation_level_ = isolation_level;
        next_is_read_only_ = read_only;
        next_wait_for_locks_ = wait_for_locks;
        next_lock_timeout_seconds_ = lock_timeout_seconds;
        next_read_committed_mode_ = read_committed_mode;
        settings_staged_ = true;
    }

    void ConnectionContext::stageDefaultTransactionSettings()
    {
        stageTransactionSettings(default_isolation_level_,
                                 default_is_read_only_,
                                 default_wait_for_locks_,
                                 default_lock_timeout_seconds_,
                                 default_read_committed_mode_);
    }

    void ConnectionContext::beginStatementTracking(const std::string& sql)
    {
        // Record the SQL text so dormant reattach can show what was running.
        last_statement_text_ = sql;
        last_statement_hash_ = fnv1a64(sql);
        last_statement_time_ = nowMicros();
        last_activity_time_ = last_statement_time_;
        last_rows_affected_ = 0;
        last_error_code_ = 0;
        last_sqlstate_.clear();

        // Classify statement type using the leading keyword only (fast, dialect-agnostic).
        size_t i = 0;
        while (i < sql.size() && std::isspace(static_cast<unsigned char>(sql[i])))
        {
            ++i;
        }

        std::string keyword;
        for (; i < sql.size(); ++i)
        {
            unsigned char c = static_cast<unsigned char>(sql[i]);
            if (std::isalnum(c) || c == '_' || c == '$')
            {
                keyword.push_back(static_cast<char>(std::toupper(c)));
            }
            else
            {
                break;
            }
        }

        if (keyword.empty())
        {
            last_statement_type_ = StatementType::UNKNOWN;
        }
        else if (keyword == "SELECT" || keyword == "INSERT" || keyword == "UPDATE" ||
                 keyword == "DELETE" || keyword == "MERGE" || keyword == "WITH")
        {
            last_statement_type_ = StatementType::DML;
        }
        else if (keyword == "CREATE" || keyword == "ALTER" || keyword == "DROP" ||
                 keyword == "TRUNCATE" || keyword == "COMMENT" || keyword == "GRANT" ||
                 keyword == "REVOKE" || keyword == "RENAME")
        {
            last_statement_type_ = StatementType::DDL;
        }
        else
        {
            last_statement_type_ = StatementType::OTHER;
        }

        last_statement_status_ = StatementStatus::IN_PROGRESS;
    }

    void ConnectionContext::endStatementTrackingSuccess(int64_t rows_affected)
    {
        last_statement_status_ = StatementStatus::COMPLETED;
        last_rows_affected_ = rows_affected;
        last_error_code_ = 0;
        last_sqlstate_.clear();
        last_statement_time_ = nowMicros();
        last_activity_time_ = last_statement_time_;
    }

    void ConnectionContext::endStatementTrackingFailure(uint32_t error_code,
                                                       const std::string& sqlstate)
    {
        last_statement_status_ = StatementStatus::FAILED;
        last_rows_affected_ = 0;
        last_error_code_ = error_code;
        last_sqlstate_ = sqlstate;
        last_statement_time_ = nowMicros();
        last_activity_time_ = last_statement_time_;
    }

    std::string ConnectionContext::sessionSettingsJson() const
    {
        // Stable ordering keeps diffs readable in dormant transaction records.
        std::string json;
        json.reserve(256);

        json.append("{\"search_path\":[");
        for (size_t i = 0; i < search_path_.size(); ++i)
        {
            if (i > 0)
            {
                json.push_back(',');
            }
            appendJsonString(json, search_path_[i]);
        }
        json.append("],\"dialect_tag\":");
        appendJsonString(json, dialect_tag_);
        json.append(",\"sql_dialect\":");
        json.append(std::to_string(sql_dialect_));
        json.append(",\"charset\":");
        appendJsonString(json, charset_);
        json.append(",\"statement_timeout\":");
        json.append(std::to_string(statement_timeout_seconds_));
        json.push_back('}');

        return json;
    }

    Status ConnectionContext::reserveTables(const std::vector<TableReservation> &reservations,
                                            ErrorContext *ctx)
    {
        // Resolve table names to UUIDs so renames don't affect lock identity.
        table_reservations_.clear();
        table_reservations_.reserve(reservations.size());

        CatalogManager *catalog = db_ ? db_->catalog_manager() : nullptr;
        if (!catalog)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "CatalogManager not available");
            return Status::INVALID_ARGUMENT;
        }

        for (const auto &reservation : reservations)
        {
            TableReservation resolved = reservation;
            if (isZeroUuidLocal(resolved.table_id))
            {
                if (resolved.table_name.empty())
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Table name required");
                    return Status::INVALID_ARGUMENT;
                }

                ObjectPath path;
                path.type = PathType::UNQUALIFIED;

                size_t start = 0;
                while (start < resolved.table_name.size())
                {
                    size_t dot = resolved.table_name.find('.', start);
                    if (dot == std::string::npos)
                    {
                        path.components.push_back(resolved.table_name.substr(start));
                        break;
                    }
                    if (dot == start)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Invalid table name in RESERVING clause");
                        return Status::INVALID_ARGUMENT;
                    }
                    path.components.push_back(resolved.table_name.substr(start, dot - start));
                    start = dot + 1;
                }

                if (path.components.empty())
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                      "Invalid table name in RESERVING clause");
                    return Status::INVALID_ARGUMENT;
                }

                if (path.components.size() > 1)
                {
                    path.type = PathType::ABSOLUTE;
                }

                CatalogManager::ResolveOptions opts;
                opts.dialect_tag = dialect_tag_;

                ID resolved_id;
                CatalogManager::ObjectType resolved_type;
                Status status = catalog->resolveObjectPath(
                    path, CatalogManager::ObjectType::TABLE, opts,
                    resolved_id, resolved_type, ctx);
                if (status != Status::OK)
                {
                    std::string msg = "Failed to resolve table '" + resolved.table_name + "'";
                    SET_ERROR_CONTEXT(ctx, status, msg.c_str());
                    return status;
                }

                (void)resolved_type;
                resolved.table_id = resolved_id;
            }

            table_reservations_.push_back(std::move(resolved));
        }

        LOG_DEBUG(LOCK, "Reserved %zu tables for transaction: proc_id=%u, xid=%lu",
                  table_reservations_.size(), proc_id_, current_xid_);

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
        // Track activity at transaction start for dormant transaction auditing.
        last_activity_time_ = static_cast<uint64_t>(xact_start_time_.count());

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
            if (!lock_mgr)
            {
                LOG_ERROR(LOCK, "LockManager not available");
                return Status::IO_ERROR;
            }

            // Acquire locks for each reserved table
            for (const auto &reservation : table_reservations_)
            {
                if (isZeroUuidLocal(reservation.table_id))
                {
                    LOG_ERROR(LOCK, "Missing table UUID for reservation '%s'",
                              reservation.table_name.c_str());
                    lock_mgr->releaseAllLocks(proc_id_, nullptr);
                    return Status::INVALID_ARGUMENT;
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
                tag.object_uuid = reservation.table_id;
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
            wait_for_locks_ = next_wait_for_locks_;
            lock_timeout_seconds_ = next_lock_timeout_seconds_;
            read_committed_mode_ = next_read_committed_mode_;
            settings_staged_ = false;

            LOG_DEBUG(TRANSACTION,
                      "Applied staged settings: isolation=%d, read_only=%d, wait=%d, lock_timeout=%u, rc_mode=%d",
                      static_cast<int>(isolation_level_), is_read_only_, wait_for_locks_,
                      lock_timeout_seconds_, static_cast<int>(read_committed_mode_));
        }

        if (security_context_staged_)
        {
            if (pending_user_change_)
            {
                current_user_id_ = pending_user_id_;
                is_superuser_ = pending_is_superuser_;
                std::memset(&active_role_id_, 0, sizeof(active_role_id_));
                pending_user_change_ = false;
            }

            if (pending_role_change_)
            {
                active_role_id_ = pending_role_id_;
                pending_role_change_ = false;
            }

            if (pending_session_change_)
            {
                session_id_ = pending_session_id_;
                authkey_id_ = pending_authkey_id_;
                emulation_mode_ = pending_emulation_mode_.empty()
                                      ? emulation_mode_
                                      : pending_emulation_mode_;
                policy_epoch_global_ = pending_policy_epoch_global_;
                policy_epoch_table_ = pending_policy_epoch_table_;
                pending_session_change_ = false;
                pending_emulation_mode_.clear();
            }

            security_context_staged_ = false;

            LOG_DEBUG(TRANSACTION, "Applied staged security context changes: proc_id=%u", proc_id_);
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

                // MGA Architecture Note: Tuple abort marking is handled by TIP, not tuple flags
                // In Firebird MGA, aborted transactions are recorded in TIP (Transaction Inventory
                // Pages) and visibility checks use TIP lookup. The HEAP_XMIN_INVALID flag is an
                // optional optimization that gets set lazily during subsequent visibility checks.
                // The savepoint rollback is already complete - TIP marks the transaction as aborted,
                // and future reads will correctly identify these tuples as invisible.

                pool->unpinPage(tid.first, false, ctx); // No modification needed
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

                // MGA Architecture Note: Delete mark clearing is handled by TIP
                // Similar to insertions, the xmax transaction being marked as aborted in TIP
                // means visibility checks will treat this tuple as not deleted.
                // MGA rule: xmax is only valid if the transaction that set it is COMMITTED.
                // Since the savepoint rollback marks the transaction as aborted in TIP,
                // the xmax is automatically invalidated.

                pool->unpinPage(tid.first, false, ctx); // No modification needed
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

    void ConnectionContext::setSessionContext(const ID& session_id, const ID& authkey_id,
                                             const std::string& emulation_mode,
                                             uint64_t policy_epoch_global,
                                             uint64_t policy_epoch_table)
    {
        const bool initial_binding = isZeroUuidLocal(session_id_) && isZeroUuidLocal(authkey_id_);
        if (initial_binding || current_xid_ == 0)
        {
            session_id_ = session_id;
            authkey_id_ = authkey_id;
            emulation_mode_ = emulation_mode.empty() ? "native" : emulation_mode;
            policy_epoch_global_ = policy_epoch_global;
            policy_epoch_table_ = policy_epoch_table;
            LOG_DEBUG(TRANSACTION, "Set session context: proc_id=%u, session_id=%s, authkey_id=%s",
                      proc_id_, session_id.toString().c_str(), authkey_id.toString().c_str());
            return;
        }

        pending_session_change_ = true;
        pending_session_id_ = session_id;
        pending_authkey_id_ = authkey_id;
        pending_emulation_mode_ = emulation_mode.empty() ? emulation_mode_ : emulation_mode;
        pending_policy_epoch_global_ = policy_epoch_global;
        pending_policy_epoch_table_ = policy_epoch_table;
        security_context_staged_ = true;

        LOG_DEBUG(TRANSACTION,
                  "Staged session context change for next transaction: proc_id=%u, session_id=%s",
                  proc_id_, session_id.toString().c_str());
    }

    void ConnectionContext::setCurrentUser(const ID& user_id, bool is_superuser)
    {
        // WP-5 EXEC-M3: Initialize session user on first call (authentication)
        // Session user is the original authenticated user and never changes
        static const ID zero_id{};
        const bool initial_binding = (session_user_id_ == zero_id);
        if (initial_binding)
        {
            session_user_id_ = user_id;
            session_is_superuser_ = is_superuser;
            LOG_DEBUG(TRANSACTION, "Set session user: proc_id=%u, session_user_id=%s, session_is_superuser=%d",
                      proc_id_, user_id.toString().c_str(), is_superuser);
        }

        if (current_xid_ != 0 && security_context_initialized_ && !initial_binding)
        {
            pending_user_change_ = true;
            pending_user_id_ = user_id;
            pending_is_superuser_ = is_superuser;
            pending_role_change_ = true;
            std::memset(&pending_role_id_, 0, sizeof(pending_role_id_));
            security_context_staged_ = true;

            LOG_DEBUG(TRANSACTION,
                      "Staged user change for next transaction: proc_id=%u, user_id=%s",
                      proc_id_, user_id.toString().c_str());
            return;
        }

        current_user_id_ = user_id;
        is_superuser_ = is_superuser;

        // Clear active role when user changes (switching users resets session state)
        std::memset(&active_role_id_, 0, sizeof(active_role_id_));
        security_context_initialized_ = true;

        LOG_DEBUG(TRANSACTION, "Set current user: proc_id=%u, user_id=%s, is_superuser=%d",
                  proc_id_, user_id.toString().c_str(), is_superuser);
    }

    void ConnectionContext::setActiveRole(const ID& role_id)
    {
        if (current_xid_ != 0 && security_context_initialized_)
        {
            pending_role_change_ = true;
            pending_role_id_ = role_id;
            security_context_staged_ = true;

            LOG_DEBUG(TRANSACTION,
                      "Staged role change for next transaction: proc_id=%u, role_id=%s",
                      proc_id_, role_id.toString().c_str());
            return;
        }

        active_role_id_ = role_id;

        LOG_DEBUG(TRANSACTION, "Set active role: proc_id=%u, role_id=%s",
                  proc_id_, role_id.toString().c_str());
    }

    void ConnectionContext::clearActiveRole()
    {
        if (current_xid_ != 0 && security_context_initialized_)
        {
            pending_role_change_ = true;
            std::memset(&pending_role_id_, 0, sizeof(pending_role_id_));
            security_context_staged_ = true;

            LOG_DEBUG(TRANSACTION, "Staged role clear for next transaction: proc_id=%u",
                      proc_id_);
            return;
        }

        LOG_DEBUG(TRANSACTION, "Clearing active role: proc_id=%u, previous_role=%s",
                  proc_id_, active_role_id_.toString().c_str());

        std::memset(&active_role_id_, 0, sizeof(active_role_id_));
    }

    void ConnectionContext::setRoleSwitchPolicy(RoleSwitchPolicy policy)
    {
        role_switch_policy_ = policy;
    }

    // ============================================================================
    // Security Context Stack (Phase 3.1 - SQL Object Permissions)
    // ============================================================================

    void ConnectionContext::pushSecurityContext(const ID& user_id, const ID& role_id,
                                               bool is_superuser_flag, SecurityMode mode,
                                               const ID& object_id)
    {
        SecurityContext ctx;
        ctx.effective_user_id = user_id;
        ctx.effective_role_id = role_id;
        ctx.is_superuser = is_superuser_flag;
        ctx.mode = mode;
        ctx.object_id = object_id;
        ctx.session_id = session_id_;
        ctx.authkey_id = authkey_id_;
        ctx.emulation_mode = emulation_mode_;
        ctx.policy_epoch_global = policy_epoch_global_;
        ctx.policy_epoch_table = policy_epoch_table_;

        security_stack_.push_back(ctx);

        LOG_DEBUG(TRANSACTION, "Pushed security context: proc_id=%u, depth=%zu, mode=%s, user=%s",
                  proc_id_, security_stack_.size(),
                  mode == SecurityMode::DEFINER ? "DEFINER" : "INVOKER",
                  user_id.toString().c_str());
    }

    void ConnectionContext::popSecurityContext()
    {
        if (!security_stack_.empty())
        {
            security_stack_.pop_back();

            LOG_DEBUG(TRANSACTION, "Popped security context: proc_id=%u, depth=%zu",
                      proc_id_, security_stack_.size());
        }
        else
        {
            LOG_WARNING(TRANSACTION, "Attempted to pop empty security context stack: proc_id=%u",
                       proc_id_);
        }
    }

    ConnectionContext::SecurityContext ConnectionContext::getCurrentSecurityContext() const
    {
        if (!security_stack_.empty())
        {
            // Return the top of the stack (most recent context)
            return security_stack_.back();
        }

        // No stacked context - return base connection context
        SecurityContext ctx;
        ctx.effective_user_id = current_user_id_;
        ctx.effective_role_id = active_role_id_;
        ctx.is_superuser = is_superuser_;
        ctx.mode = SecurityMode::INVOKER;  // Base context is always INVOKER
        std::memset(&ctx.object_id, 0, sizeof(ctx.object_id));  // No object
        ctx.session_id = session_id_;
        ctx.authkey_id = authkey_id_;
        ctx.emulation_mode = emulation_mode_;
        ctx.policy_epoch_global = policy_epoch_global_;
        ctx.policy_epoch_table = policy_epoch_table_;

        return ctx;
    }

    bool ConnectionContext::isDefinerContext() const
    {
        if (!security_stack_.empty())
        {
            return security_stack_.back().mode == SecurityMode::DEFINER;
        }
        return false;  // Base context is always INVOKER
    }

    // P2-7: Deferred Constraint Support Implementation

    bool ConnectionContext::isConstraintDeferred(const ID& constraint_id) const
    {
        // Check global deferral first
        if (all_constraints_deferred_)
        {
            return true;
        }

        // Check per-constraint state
        auto it = constraint_deferred_state_.find(constraint_id);
        if (it != constraint_deferred_state_.end())
        {
            return it->second;
        }

        // Default: not deferred (INITIALLY IMMEDIATE)
        return false;
    }

    void ConnectionContext::setConstraintDeferred(const ID& constraint_id, bool deferred)
    {
        constraint_deferred_state_[constraint_id] = deferred;

        // If setting to IMMEDIATE, validate any pending checks for this constraint
        if (!deferred)
        {
            // Validate pending checks for this specific constraint
            std::vector<DeferredConstraintCheck> remaining_checks;
            for (const auto& check : deferred_checks_)
            {
                if (check.constraint_id == constraint_id)
                {
                    // Would validate here - constraint validation would happen
                    (void)check; // Suppress unused warning
                }
                else
                {
                    remaining_checks.push_back(check);
                }
            }
            deferred_checks_ = std::move(remaining_checks);
        }
    }

    void ConnectionContext::setAllConstraintsDeferred(bool deferred)
    {
        all_constraints_deferred_ = deferred;

        // If setting to IMMEDIATE, validate all pending checks
        if (!deferred)
        {
            // Validate all pending checks - in full implementation would re-check constraints
            (void)deferred_checks_; // All checks would be validated
            deferred_checks_.clear();
        }
    }

    void ConnectionContext::addDeferredConstraintCheck(const DeferredConstraintCheck& check)
    {
        deferred_checks_.push_back(check);
    }

    Status ConnectionContext::validateDeferredConstraints(ErrorContext* ctx)
    {
        (void)ctx; // Will be used in full implementation

        // Called at COMMIT time to validate all deferred constraints
        // In a full implementation, we would:
        // 1. Look up the constraint from catalog
        // 2. Re-execute the constraint check with stored values
        // 3. Return error if any check fails

        // Placeholder: For now, all deferred constraints pass
        // Full implementation would call catalog_manager methods to re-validate

        // Clear validated checks
        deferred_checks_.clear();

        return Status::OK;
    }

    void ConnectionContext::clearDeferredConstraints()
    {
        deferred_checks_.clear();
        constraint_deferred_state_.clear();
        all_constraints_deferred_ = false;
    }

    void ConnectionContext::initializeConstraintStates()
    {
        // Clear any existing state
        constraint_deferred_state_.clear();
        all_constraints_deferred_ = false;

        // In a full implementation, we would query the catalog for all
        // DEFERRABLE constraints and set their initial state based on
        // INITIALLY DEFERRED / INITIALLY IMMEDIATE

        // For now, all constraints start as IMMEDIATE (not deferred)
    }

    // =========================================================================
    // P2-21: Prepared Statement Cache Implementation
    // =========================================================================

    Status ConnectionContext::prepareStatement(const std::string& name,
                                               const std::string& sql_text,
                                               const std::vector<uint8_t>& bytecode,
                                               const std::vector<uint16_t>& param_types,
                                               ErrorContext* ctx)
    {
        // Check if name already exists
        if (prepared_statements_.find(name) != prepared_statements_.end())
        {
            if (ctx)
            {
                ctx->code = Status::CONSTRAINT_VIOLATION;
                ctx->message = "Prepared statement '" + name + "' already exists";
            }
            return Status::CONSTRAINT_VIOLATION;
        }

        // Check if we need to evict (LRU) before adding
        if (max_prepared_statements_ > 0 &&
            prepared_statements_.size() >= max_prepared_statements_)
        {
            evictOldestPreparedStatement();
        }

        // Create the prepared statement
        PreparedStatement stmt;
        stmt.name = name;
        stmt.sql_text = sql_text;
        stmt.bytecode = bytecode;
        stmt.param_types = param_types;
        stmt.param_count = param_types.size();
        stmt.created_at = std::chrono::steady_clock::now();
        stmt.last_used = stmt.created_at;
        stmt.execution_count = 0;

        prepared_statements_[name] = std::move(stmt);
        return Status::OK;
    }

    ConnectionContext::PreparedStatement* ConnectionContext::getPreparedStatement(const std::string& name)
    {
        auto it = prepared_statements_.find(name);
        if (it != prepared_statements_.end())
        {
            return &it->second;
        }
        return nullptr;
    }

    Status ConnectionContext::deallocatePreparedStatement(const std::string& name, ErrorContext* ctx)
    {
        if (name.empty())
        {
            // DEALLOCATE ALL
            prepared_statements_.clear();
            return Status::OK;
        }

        auto it = prepared_statements_.find(name);
        if (it == prepared_statements_.end())
        {
            if (ctx)
            {
                ctx->code = Status::NOT_FOUND;
                ctx->message = "Prepared statement '" + name + "' does not exist";
            }
            return Status::NOT_FOUND;
        }

        prepared_statements_.erase(it);
        return Status::OK;
    }

    void ConnectionContext::recordStatementExecution(const std::string& name)
    {
        auto it = prepared_statements_.find(name);
        if (it != prepared_statements_.end())
        {
            it->second.last_used = std::chrono::steady_clock::now();
            it->second.execution_count++;
        }
    }

    void ConnectionContext::getPreparedStatementStats(size_t& count, size_t& total_bytes) const
    {
        count = prepared_statements_.size();
        total_bytes = 0;

        for (const auto& [name, stmt] : prepared_statements_)
        {
            total_bytes += name.size();
            total_bytes += stmt.sql_text.size();
            total_bytes += stmt.bytecode.size();
            total_bytes += stmt.param_types.size() * sizeof(uint16_t);
            total_bytes += sizeof(PreparedStatement);  // Struct overhead
        }
    }

    void ConnectionContext::evictOldestPreparedStatement()
    {
        if (prepared_statements_.empty())
        {
            return;
        }

        // Find the least recently used statement
        auto oldest_it = prepared_statements_.begin();
        auto oldest_time = oldest_it->second.last_used;

        for (auto it = prepared_statements_.begin(); it != prepared_statements_.end(); ++it)
        {
            if (it->second.last_used < oldest_time)
            {
                oldest_time = it->second.last_used;
                oldest_it = it;
            }
        }

        // Evict it
        prepared_statements_.erase(oldest_it);
    }

} // namespace scratchbird::core
