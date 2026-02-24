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

/**
 * ScratchBird Server Session Management
 *
 * Local Server Architecture - Phase 3
 *
 * This header defines the server-side session management for client connections.
 * Each connected client has a ServerSession that manages:
 * - Authentication state
 * - Database connection context
 * - Transaction state
 * - Query execution
 */

#include <cstdint>
#include <string>
#include <memory>
#include <mutex>
#include <atomic>
#include <vector>
#include <chrono>
#include <optional>
#include <unordered_map>
#include <vector>

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/auth_provider.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/catalog_manager.h"  // For DatabaseTriggerEvent
#include "scratchbird/protocol/wire_protocol.h"
#include "scratchbird/server/ipc_server.h"

namespace scratchbird {

// Forward declarations
namespace sblr {
    class Executor;
    class ResultSet;
    class ExecutionResult;
}

// Query compilation uses the selected parser (ScratchBird or emulated dialects).

namespace server {

// ============================================================================
// Session State
// ============================================================================

/**
 * Session state enumeration
 */
enum class SessionState : uint8_t {
    CREATED = 0,        // Session created, not yet authenticated
    AUTHENTICATING,     // Authentication in progress
    AUTHENTICATED,      // Authenticated, ready for queries
    EXECUTING,          // Query execution in progress
    IN_TRANSACTION,     // Inside an explicit transaction
    CLOSING,            // Session is closing
    CLOSED              // Session closed
};

/**
 * Session statistics
 */
struct SessionStats {
    uint64_t queries_executed = 0;
    uint64_t queries_failed = 0;
    uint64_t rows_returned = 0;
    uint64_t bytes_sent = 0;
    uint64_t bytes_received = 0;
    uint64_t transactions_committed = 0;
    uint64_t transactions_rolled_back = 0;
    std::chrono::steady_clock::time_point created_at;
    std::chrono::steady_clock::time_point last_activity;
};

// ============================================================================
// Server Session
// ============================================================================

/**
 * ServerSession - Manages a single client connection
 *
 * Each client connection gets a ServerSession that handles:
 * - Protocol message processing
 * - Authentication
 * - Query parsing and execution
 * - Transaction management
 * - Result streaming
 */
class ServerSession {
public:
    /**
     * Create a new server session
     *
     * @param connection The IPC connection to the client
     * @param database The database instance
     * @param session_id UUID for this session
     */
    ServerSession(IPCConnection* connection,
                  core::Database* database,
                  const uint8_t session_id[16]);

    ~ServerSession();

    // Disable copy/move
    ServerSession(const ServerSession&) = delete;
    ServerSession& operator=(const ServerSession&) = delete;

    // ========================================================================
    // Session Lifecycle
    // ========================================================================

    /**
     * Run the session main loop
     *
     * Processes messages until the connection is closed or an error occurs.
     * This method blocks until the session ends.
     *
     * @return Status::OK on clean shutdown, error status otherwise
     */
    core::Status run();

    /**
     * Request session shutdown
     *
     * Signals the session to close gracefully. The run() method will
     * return after completing the current operation.
     */
    void requestShutdown();

    /**
     * Check if session is running
     */
    bool isRunning() const { return state_ != SessionState::CLOSED; }

    // ========================================================================
    // Session Information
    // ========================================================================

    /**
     * Get session ID
     */
    const uint8_t* sessionId() const { return session_id_; }
    std::string sessionIdString() const;

    /**
     * Get session state
     */
    SessionState state() const { return state_; }

    /**
     * Get authenticated username
     */
    const std::string& username() const { return username_; }

    /**
     * Get session statistics
     */
    const SessionStats& stats() const { return stats_; }

    /**
     * Get client address/info
     */
    const std::string& clientInfo() const { return client_info_; }


private:
    // ========================================================================
    // Message Handlers
    // ========================================================================

    /**
     * Process a single incoming message
     */
    core::Status processMessage(const protocol::Message& msg, core::ErrorContext* ctx);

    /**
     * Handle CONNECT_REQUEST
     */
    core::Status handleConnect(const protocol::Message& msg, core::ErrorContext* ctx);

    /**
     * Handle AUTH_REQUEST
     */
    core::Status handleAuth(const protocol::Message& msg, core::ErrorContext* ctx);

    /**
     * Handle DISCONNECT
     */
    core::Status handleDisconnect(const protocol::Message& msg, core::ErrorContext* ctx);

    /**
     * Handle QUERY
     */
    core::Status handleQuery(const protocol::Message& msg, core::ErrorContext* ctx);

    /**
     * Handle TRANSACTION (BEGIN/COMMIT/ROLLBACK/SAVEPOINT)
     */
    core::Status handleTransaction(const protocol::Message& msg, core::ErrorContext* ctx);

    /**
     * Handle PING
     */
    core::Status handlePing(const protocol::Message& msg, core::ErrorContext* ctx);

    /**
     * Handle QUERY_CANCEL
     */
    core::Status handleCancel(const protocol::Message& msg, core::ErrorContext* ctx);

    // ========================================================================
    // Query Execution
    // ========================================================================

    /**
     * Execute an SBLR bytecode program and send results
     */
    core::Status executeBytecode(const std::vector<uint8_t>& bytecode,
                                 const std::string& sql,
                                 core::ErrorContext* ctx);

    /**
     * Send a result set to the client
     */
    core::Status sendResultSet(const sblr::ResultSet* results, core::ErrorContext* ctx);

    /**
     * Send an error response
     */
    core::Status sendError(const std::string& message,
                           const std::string& sqlstate = "42000",
                           core::ErrorContext* ctx = nullptr);

    // ========================================================================
    // Authentication
    // ========================================================================

    /**
     * Authenticate with username/password
     *
     * @param username Username to authenticate
     * @param password Password to verify
     * @param user_info Output: user metadata if authenticated
     * @param error_msg_out Output: error message for failures
     * @return AuthResult status
     */
    core::AuthResult authenticate(const std::string& username, const std::string& password,
                                  core::AuthUserInfo& user_info,
                                  std::string& error_msg_out);

    // ========================================================================
    // Database Trigger Firing (Firebird-style)
    // ========================================================================

    /**
     * Fire all database triggers for the specified event
     *
     * Triggers are executed in order of their POSITION value.
     * If any trigger fails, remaining triggers are not executed.
     *
     * @param event The database event that occurred
     * @return true if all triggers executed successfully, false if any failed
     */
    bool fireDatabaseTriggers(core::CatalogManager::DatabaseTriggerEvent event);

    // ========================================================================
    // Internal State
    // ========================================================================

    IPCConnection* connection_;                     // IPC connection (not owned)
    core::Database* database_;                      // Database instance (not owned)
    std::unique_ptr<protocol::ProtocolSession> protocol_session_;

    uint8_t session_id_[16];                        // Session UUID
    std::atomic<SessionState> state_;               // Current state
    std::atomic<bool> shutdown_requested_;          // Shutdown flag

    std::string username_;                          // Authenticated username
    std::string client_info_;                       // Client connection info
    std::string auth_database_context_;             // CONNECT database for auth tuple scoping
    PeerCredentials peer_credentials_{};            // Peer identity for local IPC
    bool peer_credentials_available_ = false;
    core::ID session_id_uuid_{};                    // Catalog session UUID
    core::ID authkey_id_{};                         // AuthKey UUID
    std::optional<core::ScramAuthState> scram_state_;
    struct PendingMfaAuthState {
        bool active = false;
        std::string username;
        protocol::AuthMethod auth_method = protocol::AuthMethod::PASSWORD;
        core::AuthUserInfo user_info;
        core::ID account_id;
        core::ID mfa_policy_id;
        bool allow_recovery_codes = false;
        bool allow_break_glass = false;
        uint32_t step_up_ttl_ms = 0;
        std::string challenge_id;
        std::string challenge_method_name;
        uint32_t attempts = 0;
        uint32_t max_attempts = 0;
        std::vector<uint8_t> challenge_payload;
    };
    PendingMfaAuthState pending_mfa_auth_;
    bool auth_negotiation_ready_ = false;
    std::string auth_negotiation_username_;
    std::vector<protocol::AuthMethod> auth_negotiation_allowed_methods_;
    bool auth_negotiation_has_required_method_ = false;
    protocol::AuthMethod auth_negotiation_required_method_ = protocol::AuthMethod::SCRAM_SHA_256;
    uint8_t auth_negotiation_transport_mask_ = 0;
    std::vector<uint8_t> auth_negotiation_nonce_;
    core::CatalogManager::AuthPeerMode auth_negotiation_peer_mode_ =
        core::CatalogManager::AuthPeerMode::DISABLED;
    bool session_mfa_verified_ = false;
    bool session_mfa_used_recovery_ = false;
    bool session_mfa_used_break_glass_ = false;
    uint64_t session_mfa_verified_time_ms_ = 0;
    uint32_t session_mfa_step_up_ttl_ms_ = 0;

    // Connection context for security and transactions
    std::unique_ptr<core::ConnectionContext> conn_ctx_;

    // Executor for query execution
    std::unique_ptr<sblr::Executor> executor_;

    // Statistics
    SessionStats stats_;

    // NET-M1: Query cancellation support
    // Flag to indicate a query is currently executing
    std::atomic<bool> query_executing_{false};

    // Stream id generator for COPY streaming
    uint64_t next_stream_id_{1};

    // Mutex for thread safety
    mutable std::mutex mutex_;
};

// ============================================================================
// Session Manager
// ============================================================================

/**
 * SessionManager - Manages all active server sessions
 *
 * The SessionManager tracks all client sessions and provides:
 * - Session creation and destruction
 * - Session lookup by ID
 * - Broadcast capabilities
 * - Global shutdown
 */
class SessionManager {
public:
    SessionManager();
    ~SessionManager();

    // Disable copy/move
    SessionManager(const SessionManager&) = delete;
    SessionManager& operator=(const SessionManager&) = delete;

    // ========================================================================
    // Session Management
    // ========================================================================

    /**
     * Create a new session
     *
     * @param connection The client connection
     * @param database The database instance
     * @return Pointer to the new session, or nullptr on failure
     */
    ServerSession* createSession(IPCConnection* connection, core::Database* database);

    /**
     * Remove a session
     *
     * @param session_id The session UUID to remove
     */
    void removeSession(const uint8_t session_id[16]);

    /**
     * Get a session by ID
     *
     * @param session_id The session UUID
     * @return Pointer to the session, or nullptr if not found
     */
    ServerSession* getSession(const uint8_t session_id[16]);

    /**
     * Get all active sessions
     */
    std::vector<ServerSession*> getAllSessions();

    /**
     * Get active session count
     */
    size_t sessionCount() const;

    // ========================================================================
    // Global Operations
    // ========================================================================

    /**
     * Request shutdown of all sessions
     */
    void shutdownAll();

    /**
     * Wait for all sessions to close
     *
     * @param timeout_ms Maximum time to wait (0 = forever)
     * @return true if all sessions closed, false on timeout
     */
    bool waitForShutdown(uint32_t timeout_ms = 0);

    /**
     * Get aggregate statistics
     */
    SessionStats getAggregateStats() const;

private:
    // Session storage (keyed by session ID string)
    std::unordered_map<std::string, std::unique_ptr<ServerSession>> sessions_;

    // Mutex for thread-safe access
    mutable std::mutex mutex_;
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Convert session state to string
 */
const char* sessionStateToString(SessionState state);

}  // namespace server
}  // namespace scratchbird
