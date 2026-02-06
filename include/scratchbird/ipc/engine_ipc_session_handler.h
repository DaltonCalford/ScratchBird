/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0
 */
#pragma once

/**
 * EngineIPCSessionHandler
 * 
 * Full implementation of IPCSessionHandler that connects the IPC server
 * to the ScratchBird execution engine (SBLR).
 * 
 * Features:
 * - Session management with prepared statement cache
 * - Portal (cursor) management for bound statements
 * - Transaction state management
 * - COPY operation support
 * - Query cancellation
 */

#include "scratchbird/ipc/ipc_server.h"
#include "scratchbird/core/database.h"
#include <shared_mutex>

namespace scratchbird {
namespace ipc {

// Forward declaration
struct SessionState;
struct PreparedStatement;
struct Portal;

// Transaction isolation levels
enum class TransactionIsolation {
    READ_UNCOMMITTED,
    READ_COMMITTED,
    REPEATABLE_READ,
    SERIALIZABLE
};

/**
 * EngineIPCSessionHandler - Production implementation
 * 
 * Connects IPC messages to the SBLR executor with:
 * - Prepared statement cache (LRU eviction)
 * - Portal management for cursors
 * - Full transaction support
 * - COPY protocol support
 */
class EngineIPCSessionHandler : public IPCSessionHandler {
public:
    /**
     * Create handler with database reference
     * 
     * @param database The database instance (must outlive this handler)
     */
    explicit EngineIPCSessionHandler(core::Database* database);
    
    ~EngineIPCSessionHandler() override;
    
    // Non-copyable
    EngineIPCSessionHandler(const EngineIPCSessionHandler&) = delete;
    EngineIPCSessionHandler& operator=(const EngineIPCSessionHandler&) = delete;

    // ========================================================================
    // Lifecycle
    // ========================================================================
    
    core::Status onAttach(uint32_t session_id,
                         const IPCStartupPayload& startup,
                         core::ErrorContext* ctx = nullptr) override;
    
    core::Status onDetach(uint32_t session_id,
                         core::ErrorContext* ctx = nullptr) override;
    
    // ========================================================================
    // Query Execution
    // ========================================================================
    
    core::Status onSimpleQuery(uint32_t session_id,
                              const std::string& sql,
                              core::ErrorContext* ctx = nullptr) override;
    
    core::Status onParse(uint32_t session_id,
                        const std::string& stmt_name,
                        const std::string& sql,
                        core::ErrorContext* ctx = nullptr) override;
    
    core::Status onBind(uint32_t session_id,
                       const std::string& portal_name,
                       const std::string& stmt_name,
                       core::ErrorContext* ctx = nullptr) override;
    
    core::Status onExecute(uint32_t session_id,
                          const std::string& portal_name,
                          uint32_t max_rows,
                          core::ErrorContext* ctx = nullptr) override;
    
    core::Status onClose(uint32_t session_id,
                        char type,
                        const std::string& name,
                        core::ErrorContext* ctx = nullptr) override;
    
    core::Status onSync(uint32_t session_id,
                       core::ErrorContext* ctx = nullptr) override;
    
    // ========================================================================
    // Transactions
    // ========================================================================
    
    core::Status onBegin(uint32_t session_id,
                        core::ErrorContext* ctx = nullptr) override;
    
    core::Status onCommit(uint32_t session_id,
                         core::ErrorContext* ctx = nullptr) override;
    
    core::Status onRollback(uint32_t session_id,
                           core::ErrorContext* ctx = nullptr) override;
    
    core::Status onSavepoint(uint32_t session_id,
                            const std::string& name,
                            core::ErrorContext* ctx = nullptr) override;
    
    // ========================================================================
    // COPY Operations
    // ========================================================================
    
    core::Status onCopyInStart(uint32_t session_id,
                              core::ErrorContext* ctx = nullptr) override;
    
    core::Status onCopyData(uint32_t session_id,
                           const uint8_t* data, size_t len,
                           core::ErrorContext* ctx = nullptr) override;
    
    core::Status onCopyDone(uint32_t session_id,
                           core::ErrorContext* ctx = nullptr) override;
    
    core::Status onCopyFail(uint32_t session_id,
                           const std::string& reason,
                           core::ErrorContext* ctx = nullptr) override;
    
    // ========================================================================
    // Cancel and Notifications
    // ========================================================================
    
    core::Status onCancel(uint32_t session_id,
                         core::ErrorContext* ctx = nullptr) override;
    
    core::Status onSubscribe(uint32_t session_id,
                            const std::string& channel,
                            core::ErrorContext* ctx = nullptr) override;
    
    core::Status onUnsubscribe(uint32_t session_id,
                              const std::string& channel,
                              core::ErrorContext* ctx = nullptr) override;
    
    // ========================================================================
    // Response Callbacks
    // ========================================================================
    
    core::Status sendRowDescription(uint32_t session_id,
                                   const std::vector<IPCFieldDesc>& fields) override;
    
    core::Status sendDataRow(uint32_t session_id,
                            const std::vector<std::optional<std::string>>& values) override;
    
    core::Status sendCommandComplete(uint32_t session_id,
                                    const std::string& tag,
                                    uint64_t rows_affected) override;
    
    core::Status sendError(uint32_t session_id,
                          const char* sqlstate,
                          const std::string& message) override;
    
    core::Status sendNotice(uint32_t session_id,
                           const std::string& message) override;
    
    core::Status sendReady(uint32_t session_id,
                          uint32_t server_features) override;
    
    core::Status sendParseComplete(uint32_t session_id) override;
    
    core::Status sendBindComplete(uint32_t session_id) override;
    
    core::Status sendCloseComplete(uint32_t session_id) override;
    
    core::Status sendCopyInRequest(uint32_t session_id) override;
    
    core::Status sendCopyOutResponse(uint32_t session_id) override;
    
    core::Status sendCopyData(uint32_t session_id,
                             const uint8_t* data, size_t len) override;
    
    core::Status sendCopyComplete(uint32_t session_id) override;
    
    core::Status sendTxnComplete(uint32_t session_id) override;
    
    core::Status sendNotification(uint32_t session_id,
                                 const std::string& channel,
                                 const std::string& payload) override;
    
    // ========================================================================
    // Statistics and Management
    // ========================================================================
    
    struct HandlerStats {
        uint32_t active_sessions = 0;
        uint64_t total_sessions = 0;
        uint64_t queries_executed = 0;
        uint64_t prepared_statements = 0;
        uint64_t active_portals = 0;
    };
    
    HandlerStats getStats() const;
    
    /**
     * Clear prepared statement cache for a session
     */
    core::Status clearPreparedStatementCache(uint32_t session_id);
    
    /**
     * Get prepared statement info
     */
    struct StatementInfo {
        std::string name;
        std::string sql_preview;
        uint64_t execution_count;
        double age_seconds;
    };
    std::vector<StatementInfo> getPreparedStatements(uint32_t session_id) const;

private:
    core::Database* database_;
    std::unordered_map<uint32_t, std::unique_ptr<SessionState>> sessions_;
    mutable std::shared_mutex sessions_mutex_;
    
    // Helper methods
    SessionState* getSession(uint32_t session_id);
};

} // namespace ipc
} // namespace scratchbird
