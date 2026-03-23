/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0
 */
#pragma once

/**
 * IPC Server v1.1
 * 
 * Section E2: Engine IPC Server
 * 
 * Server-side IPC infrastructure that handles connections from parser agents.
 * Manages session lifecycle, message routing, and query execution.
 */

#include "scratchbird/ipc/ipc_contract_v1_1.h"

#include <atomic>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <queue>
#include <functional>
#include <condition_variable>

namespace scratchbird {
namespace ipc {

// Forward declarations
class IPCSession;
class IPCWorker;
class IPCServerTest;  // For testing

// ============================================================================
// Session Configuration
// ============================================================================

struct IPCServerConfig {
    std::string endpoint;           // Socket/pipe path or host:port
    uint32_t max_sessions = 100;    // Maximum concurrent sessions
    uint32_t max_workers = 4;       // Worker threads
    uint32_t request_timeout_ms = 30000;  // Request timeout
    uint32_t idle_timeout_ms = 300000;    // Idle session timeout
    uint32_t heartbeat_interval_ms = 30000;  // Heartbeat interval
    bool enable_compression = false;
    bool enable_encryption = false;
    
    IPCServerConfig();
};

// ============================================================================
// Session State
// ============================================================================

enum class SessionState {
    INITIALIZING,   // Session created, waiting for STARTUP
    NEGOTIATING,    // Feature negotiation in progress
    ACTIVE,         // Session ready for commands
    EXECUTING,      // Query currently executing
    COPY_IN,        // COPY FROM in progress
    COPY_OUT,       // COPY TO in progress
    CLOSING,        // Session closing
    CLOSED          // Session closed
};

// ============================================================================
// Session Statistics
// ============================================================================

struct SessionStats {
    uint64_t messages_received = 0;
    uint64_t messages_sent = 0;
    uint64_t bytes_received = 0;
    uint64_t bytes_sent = 0;
    uint64_t queries_executed = 0;
    uint64_t transactions_started = 0;
    uint64_t errors = 0;
    uint64_t start_time_ms = 0;
    uint64_t last_activity_ms = 0;
};

// ============================================================================
// Query Context
// ============================================================================

struct QueryContext {
    uint32_t request_id = 0;
    std::string sql;
    std::string stmt_name;
    std::string portal_name;
    uint32_t max_rows = 0;
    bool cancelled = false;
    std::chrono::steady_clock::time_point start_time;
};

// ============================================================================
// Session Handler Interface
// ============================================================================

class IPCSessionHandler {
public:
    virtual ~IPCSessionHandler() = default;
    
    // Lifecycle
    virtual core::Status onAttach(uint32_t session_id,
                                 const IPCStartupPayload& startup,
                                 core::ErrorContext* ctx = nullptr) = 0;
    virtual core::Status onDetach(uint32_t session_id,
                                 core::ErrorContext* ctx = nullptr) = 0;
    
    // Query execution
    virtual core::Status onSimpleQuery(uint32_t session_id,
                                      const std::string& sql,
                                      core::ErrorContext* ctx = nullptr) = 0;
    virtual core::Status onCompiledQuery(uint32_t session_id,
                                        const std::vector<uint8_t>& bytecode,
                                        const std::string& original_sql,
                                        core::ErrorContext* ctx = nullptr) = 0;
    virtual core::Status onParse(uint32_t session_id,
                                const std::string& stmt_name,
                                const std::string& sql,
                                core::ErrorContext* ctx = nullptr) = 0;
    virtual core::Status onCompiledParse(uint32_t session_id,
                                        const std::string& stmt_name,
                                        const std::vector<uint8_t>& bytecode,
                                        const std::string& original_sql,
                                        core::ErrorContext* ctx = nullptr) = 0;
    virtual core::Status onBind(uint32_t session_id,
                               const std::string& portal_name,
                               const std::string& stmt_name,
                               const std::vector<std::optional<std::string>>& params,
                               const std::vector<bool>& param_nulls,
                               core::ErrorContext* ctx = nullptr) = 0;
    virtual core::Status onExecute(uint32_t session_id,
                                  const std::string& portal_name,
                                  uint32_t max_rows,
                                  core::ErrorContext* ctx = nullptr) = 0;
    virtual core::Status onClose(uint32_t session_id,
                                char type,
                                const std::string& name,
                                core::ErrorContext* ctx = nullptr) = 0;
    virtual core::Status onSync(uint32_t session_id,
                               core::ErrorContext* ctx = nullptr) = 0;
    
    // Transactions
    virtual core::Status onBegin(uint32_t session_id,
                                core::ErrorContext* ctx = nullptr) = 0;
    virtual core::Status onCommit(uint32_t session_id,
                                 core::ErrorContext* ctx = nullptr) = 0;
    virtual core::Status onRollback(uint32_t session_id,
                                   core::ErrorContext* ctx = nullptr) = 0;
    virtual core::Status onSavepoint(uint32_t session_id,
                                    const std::string& name,
                                    core::ErrorContext* ctx = nullptr) = 0;
    
    // COPY operations
    virtual core::Status onCopyInStart(uint32_t session_id,
                                      core::ErrorContext* ctx = nullptr) = 0;
    virtual core::Status onCopyData(uint32_t session_id,
                                   const uint8_t* data, size_t len,
                                   core::ErrorContext* ctx = nullptr) = 0;
    virtual core::Status onCopyDone(uint32_t session_id,
                                   core::ErrorContext* ctx = nullptr) = 0;
    virtual core::Status onCopyFail(uint32_t session_id,
                                   const std::string& reason,
                                   core::ErrorContext* ctx = nullptr) = 0;
    
    // Cancel
    virtual core::Status onCancel(uint32_t session_id,
                                 core::ErrorContext* ctx = nullptr) = 0;
    
    // Notifications
    virtual core::Status onSubscribe(uint32_t session_id,
                                    const std::string& channel,
                                    core::ErrorContext* ctx = nullptr) = 0;
    virtual core::Status onUnsubscribe(uint32_t session_id,
                                      const std::string& channel,
                                      core::ErrorContext* ctx = nullptr) = 0;
    
    // Response callbacks (called by handler to send responses)
    virtual core::Status sendRowDescription(uint32_t session_id,
                                           const std::vector<IPCFieldDesc>& fields) = 0;
    virtual core::Status sendDataRow(uint32_t session_id,
                                    const std::vector<std::optional<std::string>>& values) = 0;
    virtual core::Status sendCommandComplete(uint32_t session_id,
                                            const std::string& tag,
                                            uint64_t rows_affected) = 0;
    virtual core::Status sendError(uint32_t session_id,
                                  const char* sqlstate,
                                  const std::string& message) = 0;
    virtual core::Status sendNotice(uint32_t session_id,
                                   const std::string& message) = 0;
    virtual core::Status sendReady(uint32_t session_id,
                                  uint32_t server_features) = 0;
    virtual core::Status sendParseComplete(uint32_t session_id) = 0;
    virtual core::Status sendBindComplete(uint32_t session_id) = 0;
    virtual core::Status sendCloseComplete(uint32_t session_id) = 0;
    virtual core::Status sendCopyInRequest(uint32_t session_id) = 0;
    virtual core::Status sendCopyOutResponse(uint32_t session_id) = 0;
    virtual core::Status sendCopyData(uint32_t session_id,
                                     const uint8_t* data, size_t len) = 0;
    virtual core::Status sendCopyComplete(uint32_t session_id) = 0;
    virtual core::Status sendTxnComplete(uint32_t session_id) = 0;
    virtual core::Status sendNotification(uint32_t session_id,
                                         const std::string& channel,
                                         const std::string& payload) = 0;
    virtual core::Status drainOutboundMessages(uint32_t session_id,
                                              std::vector<IPCMessage>& messages,
                                              core::ErrorContext* ctx = nullptr) = 0;
};

// ============================================================================
// IPC Session
// ============================================================================

class IPCSession {
    friend class IPCServer;
    
public:
    IPCSession(uint32_t id, std::unique_ptr<IPCChannel> channel,
               IPCSessionHandler* handler);
    ~IPCSession();
    
    // Lifecycle
    core::Status start(core::ErrorContext* ctx = nullptr);
    core::Status stop(core::ErrorContext* ctx = nullptr);
    bool isActive() const {
        return state_ != SessionState::CLOSING &&
               state_ != SessionState::CLOSED;
    }
    
    // Message handling
    core::Status handleMessage(const IPCMessage& msg, core::ErrorContext* ctx);
    core::Status sendMessage(const IPCMessage& msg, core::ErrorContext* ctx);
    
    // State
    SessionState getState() const { return state_; }
    void setState(SessionState state) { state_ = state; }
    uint32_t getId() const { return id_; }
    SessionStats getStats() const;
    void updateActivity();
    bool isIdle() const;
    
    // Query context
    void setQueryContext(const QueryContext& ctx) { query_ctx_ = ctx; }
    QueryContext getQueryContext() const { return query_ctx_; }
    void clearQueryContext();
    bool isQueryCancelled() const { return query_ctx_.cancelled; }
    void cancelQuery() { query_ctx_.cancelled = true; }
    
    static uint64_t getCurrentTimeMs();

    // Shutdown
    void shutdown();

protected:
    IPCSessionHandler* getHandler() const { return handler_; }
    IPCChannel* getChannel() const { return channel_.get(); }
    std::mutex& getIOMutex() { return io_mutex_; }

private:
    uint32_t id_;
    std::unique_ptr<IPCChannel> channel_;
    IPCSessionHandler* handler_;
    std::atomic<SessionState> state_{SessionState::INITIALIZING};
    SessionStats stats_;
    QueryContext query_ctx_;
    
    std::mutex mutex_;
    std::mutex io_mutex_;  // Protects channel I/O (thread safety)
    uint64_t last_activity_ms_ = 0;
    
    // Flow control state (for COPY operations)
    struct FlowControlState {
        int32_t credits = 0;
        uint32_t buffer_avail = 0;
        std::chrono::steady_clock::time_point last_update;
        std::mutex mutex;
        std::condition_variable cv;
    } flow_control_;
    
    // Handlers for each message type
    core::Status flushOutboundMessages(core::ErrorContext* ctx);
    core::Status handleStartup(const IPCMessage& msg, core::ErrorContext* ctx);
    core::Status handleFeatureNegotiate(const IPCMessage& msg, core::ErrorContext* ctx);
    core::Status handleSimpleQuery(const IPCMessage& msg, core::ErrorContext* ctx);
    core::Status handleCompiledQuery(const IPCMessage& msg, core::ErrorContext* ctx);
    core::Status handleParse(const IPCMessage& msg, core::ErrorContext* ctx);
    core::Status handleCompiledParse(const IPCMessage& msg, core::ErrorContext* ctx);
    core::Status handleBind(const IPCMessage& msg, core::ErrorContext* ctx);
    core::Status handleExecute(const IPCMessage& msg, core::ErrorContext* ctx);
    core::Status handleClose(const IPCMessage& msg, core::ErrorContext* ctx);
    core::Status handleSync(const IPCMessage& msg, core::ErrorContext* ctx);
    core::Status handleTxnBegin(const IPCMessage& msg, core::ErrorContext* ctx);
    core::Status handleTxnCommit(const IPCMessage& msg, core::ErrorContext* ctx);
    core::Status handleTxnRollback(const IPCMessage& msg, core::ErrorContext* ctx);
    core::Status handleSavepoint(const IPCMessage& msg, core::ErrorContext* ctx);
    core::Status handleCopyData(const IPCMessage& msg, core::ErrorContext* ctx);
    core::Status handleCopyDone(const IPCMessage& msg, core::ErrorContext* ctx);
    core::Status handleCopyFail(const IPCMessage& msg, core::ErrorContext* ctx);
    core::Status handleCancelRequest(const IPCMessage& msg, core::ErrorContext* ctx);
    core::Status handlePing(const IPCMessage& msg, core::ErrorContext* ctx);
    core::Status handleTerminate(const IPCMessage& msg, core::ErrorContext* ctx);
    core::Status handleStreamControl(const IPCMessage& msg, core::ErrorContext* ctx);
};

// ============================================================================
// IPC Server
// ============================================================================

class IPCServer {
    friend class IPCWorker;
    friend class IPCServerTest;
public:
    IPCServer(const IPCServerConfig& config, std::unique_ptr<IPCSessionHandler> handler);
    ~IPCServer();
    
    // Lifecycle
    core::Status start(core::ErrorContext* ctx = nullptr);
    core::Status stop(core::ErrorContext* ctx = nullptr);
    bool isRunning() const { return running_; }
    
    // Session management
    core::Status broadcast(const IPCMessage& msg, core::ErrorContext* ctx = nullptr);
    core::Status sendTo(uint32_t session_id, const IPCMessage& msg,
                       core::ErrorContext* ctx = nullptr);
    core::Status closeSession(uint32_t session_id, core::ErrorContext* ctx = nullptr);
    
    // Statistics
    struct ServerStats {
        uint32_t active_sessions = 0;
        uint32_t total_sessions = 0;
        uint64_t messages_received = 0;
        uint64_t messages_sent = 0;
        uint64_t bytes_received = 0;
        uint64_t bytes_sent = 0;
    };
    ServerStats getStats() const;
    
    // Configuration
    const IPCServerConfig& getConfig() const { return config_; }

private:
    IPCServerConfig config_;
    std::unique_ptr<IPCSessionHandler> handler_;
    std::atomic<bool> running_{false};
    
    // Sessions (shared_ptr for safe access from reader and worker threads)
    std::unordered_map<uint32_t, std::shared_ptr<IPCSession>> sessions_;
    mutable std::shared_mutex sessions_mutex_;
    std::atomic<uint32_t> next_session_id_{1};
    
    // Worker threads
    std::vector<std::unique_ptr<IPCWorker>> workers_;
    std::vector<std::thread> worker_threads_;
    
    // Accept thread
    std::thread accept_thread_;
    int listen_fd_ = -1;
    
    // Message queue for workers
    struct QueuedMessage {
        uint32_t session_id;
        IPCMessage message;
    };
    std::queue<QueuedMessage> message_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::atomic<bool> shutdown_{false};
    
    // Statistics
    mutable std::mutex stats_mutex_;
    ServerStats stats_;
    
    // Internal methods
    core::Status setupListener(core::ErrorContext* ctx);
    void acceptLoop();
    void workerLoop(IPCWorker* worker);
    void cleanupIdleSessions();
    uint32_t createSession(std::unique_ptr<IPCChannel> channel);
    void destroySession(uint32_t session_id);
    
    // Session reader loop (populates message_queue_)
    void sessionReadLoop(uint32_t session_id);
    void enqueueMessage(uint32_t session_id, IPCMessage&& msg);
    
    // Get session by ID (thread-safe)
    std::shared_ptr<IPCSession> getSession(uint32_t session_id);
};

// ============================================================================
// IPC Worker
// ============================================================================

class IPCWorker {
public:
    IPCWorker(uint32_t id, IPCServer* server);
    ~IPCWorker();
    
    void start();
    void stop();
    bool isRunning() const { return running_; }
    uint32_t getId() const { return id_; }
    
private:
    uint32_t id_;
    IPCServer* server_;
    std::atomic<bool> running_{false};
    std::thread thread_;
};

} // namespace ipc
} // namespace scratchbird
