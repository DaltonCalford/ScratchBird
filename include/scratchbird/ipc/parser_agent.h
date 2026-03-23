/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0
 */
#pragma once

/**
 * Parser Agent v1.1
 *
 * Section E3: Parser Agents
 *
 * Parser agents are the protocol-facing SQL front-door in the deployed
 * multi-protocol topology. They accept engine-specific client connections,
 * apply protocol/auth policy, map the emulated schema tree to the emulated
 * engine's root model, and compile SQL text into SBLR before handing work to
 * the engine IPC layer.
 *
 * The engine runtime can still be built with in-process parser/compiler
 * surfaces for local tools and tests, but that is not the primary external
 * protocol architecture.
 */

#include "scratchbird/ipc/ipc_contract_v1_1.h"
#include "scratchbird/protocol/wire_protocol.h"

#include <atomic>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace scratchbird {
namespace ipc {

// Forward declarations
class IPCChannel;

// ============================================================================
// Parser Agent Configuration
// ============================================================================

struct ParserAgentConfig {
    std::string name;                    // Agent name
    std::string protocol;                // "native", "postgresql", "mysql", "firebird"
    std::string listen_endpoint;         // Socket endpoint to listen on
    std::string ipc_endpoint;            // IPC server endpoint to connect to
    uint32_t max_connections = 100;      // Max concurrent connections
    uint32_t io_threads = 4;             // I/O thread count
    bool enable_ssl = false;             // SSL/TLS support
    
    // Protocol-specific options
    std::unordered_map<std::string, std::string> options;
};

// ============================================================================
// Client Connection State
// ============================================================================

struct ClientConnection {
    uint32_t client_id = 0;
    int socket_fd = -1;
    std::string protocol_version;
    std::string client_encoding = "UTF8";
    std::string database;
    std::string user;
    uint32_t session_id = 0;             // IPC session ID
    bool authenticated = false;
    bool ssl_enabled = false;
    uint64_t connect_time_ms = 0;
    uint64_t last_activity_ms = 0;
    
    // Transaction state
    bool in_transaction = false;
    std::string transaction_status = "I"; // I=idle, T=in transaction, E=error
    
    // Statement/portal tracking
    std::unordered_map<std::string, uint32_t> prepared_statements;
    std::unordered_map<std::string, std::string> portals;
    
    // IPC channel for this client
    std::unique_ptr<IPCChannel> ipc_channel;
};

// ============================================================================
// Parser Agent Base Class
// ============================================================================

class ParserAgent {
public:
    ParserAgent(const ParserAgentConfig& config);
    virtual ~ParserAgent();

    // Lifecycle
    virtual core::Status start(core::ErrorContext* ctx = nullptr);
    virtual core::Status stop(core::ErrorContext* ctx = nullptr);
    bool isRunning() const { return running_; }
    
    // Statistics
    struct Stats {
        uint64_t connections_accepted = 0;
        uint64_t connections_closed = 0;
        uint64_t messages_received = 0;
        uint64_t messages_sent = 0;
        uint64_t bytes_received = 0;
        uint64_t bytes_sent = 0;
        uint32_t active_connections = 0;
    };
    Stats getStats() const;

    // Single accepted-client entrypoint for handoff workers.
    core::Status runAcceptedClient(int client_fd, core::ErrorContext* ctx = nullptr);

protected:
    ParserAgentConfig config_;
    std::atomic<bool> running_{false};
    
    // Connection management
    std::unordered_map<uint32_t, std::unique_ptr<ClientConnection>> connections_;
    mutable std::shared_mutex connections_mutex_;
    std::atomic<uint32_t> next_client_id_{1};
    
    // Listener
    int listen_fd_ = -1;
    std::thread accept_thread_;
    
    // I/O threads
    std::vector<std::thread> io_threads_;
    
    // Statistics
    mutable std::mutex stats_mutex_;
    Stats stats_;
    
    // IPC channel pool
    std::vector<std::unique_ptr<IPCChannel>> ipc_channels_;
    std::mutex ipc_pool_mutex_;
    
    // Internal methods
    virtual core::Status setupListener(core::ErrorContext* ctx);
    void acceptLoop();
    void ioLoop();
    
    // Client handling (protocol-specific)
    virtual core::Status handleClient(int client_fd, core::ErrorContext* ctx) = 0;
    virtual void disconnectClient(uint32_t client_id);
    
    // IPC communication
    virtual core::Status sendToEngine(uint32_t client_id, const IPCMessage& msg,
                                     core::ErrorContext* ctx);
    virtual core::Status sendCompiledQueryToEngine(uint32_t client_id,
                                                   uint32_t request_id,
                                                   const std::vector<uint8_t>& bytecode,
                                                   const std::string& original_sql,
                                                   core::ErrorContext* ctx);
    virtual core::Status sendCompiledParseToEngine(uint32_t client_id,
                                                   uint32_t request_id,
                                                   const std::string& stmt_name,
                                                   const std::vector<uint8_t>& bytecode,
                                                   const std::string& original_sql,
                                                   core::ErrorContext* ctx);
    virtual core::Status receiveFromEngine(uint32_t client_id, IPCMessage& msg,
                                          core::ErrorContext* ctx,
                                          uint32_t timeout_ms = 30000);
    
    // Utility
    std::unique_ptr<IPCChannel> acquireIPCChannel();
    void releaseIPCChannel(std::unique_ptr<IPCChannel> channel);
    uint64_t getCurrentTimeMs() const;
    void updateStats(const std::function<void(Stats&)>& updater);
};

// ============================================================================
// Native SB Parser Agent
// ============================================================================

class NativeSBParserAgent : public ParserAgent {
public:
    explicit NativeSBParserAgent(const ParserAgentConfig& config);
    ~NativeSBParserAgent() override;

protected:
    // Protocol-specific client handling
    core::Status handleClient(int client_fd, core::ErrorContext* ctx) override;
    
    // SBWP message handlers
    core::Status handleStartup(ClientConnection& client, core::ErrorContext* ctx);
    core::Status handleSSLRequest(ClientConnection& client, core::ErrorContext* ctx);
    core::Status handleAuth(ClientConnection& client, const std::string& method,
                           const std::vector<uint8_t>& data, core::ErrorContext* ctx);
    core::Status handleQuery(ClientConnection& client, const std::string& sql,
                            core::ErrorContext* ctx);
    core::Status handleParse(ClientConnection& client, const std::string& stmt_name,
                            const std::string& sql, core::ErrorContext* ctx);
    core::Status handleBind(ClientConnection& client, const std::string& portal_name,
                           const std::string& stmt_name, core::ErrorContext* ctx);
    core::Status handleExecute(ClientConnection& client, const std::string& portal_name,
                              uint32_t max_rows, core::ErrorContext* ctx);
    core::Status handleClose(ClientConnection& client, char type,
                            const std::string& name, core::ErrorContext* ctx);
    core::Status handleSync(ClientConnection& client, core::ErrorContext* ctx);
    core::Status handleTerminate(ClientConnection& client, core::ErrorContext* ctx);
    
    // Response builders
    core::Status sendReady(ClientConnection& client, uint32_t features);
    core::Status sendRowDescription(ClientConnection& client,
                                   const std::vector<IPCFieldDesc>& fields);
    core::Status sendDataRow(ClientConnection& client,
                            const std::vector<std::optional<std::string>>& values);
    core::Status sendCommandComplete(ClientConnection& client,
                                    const std::string& tag);
    core::Status sendError(ClientConnection& client,
                          const char* sqlstate,
                          const std::string& message);
    core::Status sendNotice(ClientConnection& client, const std::string& message);
    
    // Extended query responses
    core::Status sendParseComplete(ClientConnection& client);
    core::Status sendBindComplete(ClientConnection& client);
    core::Status sendCloseComplete(ClientConnection& client);
    
    // Response forwarding
    core::Status forwardResponseToClient(ClientConnection& client,
                                        const IPCMessage& response,
                                        core::ErrorContext* ctx);
};

// ============================================================================
// Emulated Parser Agent (PostgreSQL, MySQL, Firebird)
// ============================================================================

class EmulatedParserAgent : public ParserAgent {
public:
    EmulatedParserAgent(const ParserAgentConfig& config, 
                       const std::string& target_protocol);
    ~EmulatedParserAgent() override;

protected:
    std::string target_protocol_;
    
    // Protocol-specific message framing
    virtual size_t readMessageLength(const uint8_t* header, size_t len) = 0;
    virtual core::Status readFullMessage(int fd, std::vector<uint8_t>& message,
                                        core::ErrorContext* ctx) = 0;
    virtual core::Status writeMessage(int fd, const std::vector<uint8_t>& message,
                                     core::ErrorContext* ctx) = 0;
    
    // Protocol mapping
    virtual IPCMessageType mapClientToIPC(uint8_t msg_type) = 0;
    virtual uint8_t mapIPCToClient(IPCMessageType msg_type) = 0;
    virtual core::Status translateStartupToIPC(const std::vector<uint8_t>& startup,
                                              IPCMessage& ipc_msg,
                                              core::ErrorContext* ctx) = 0;
    virtual core::Status translateIPCToResponse(const IPCMessage& ipc_msg,
                                               std::vector<uint8_t>& response,
                                               core::ErrorContext* ctx) = 0;
    
    // Error mapping
    virtual std::string mapSQLStateToProtocol(const char* sqlstate) = 0;
    virtual void mapProtocolErrorToSQLState(const std::vector<uint8_t>& error,
                                           char* sqlstate_out) = 0;
};

// ============================================================================
// Error Mapping Utilities
// ============================================================================

class IPCErrorMapper {
public:
    // SQLSTATE to protocol-specific error codes
    static std::string sqlStateToPostgreSQL(const char* sqlstate);
    static uint16_t sqlStateToMySQLErrorCode(const char* sqlstate);
    static uint32_t sqlStateToFirebirdErrorCode(const char* sqlstate);
    
    // Protocol errors to SQLSTATE
    static void postgreSQLErrorToSQLState(uint8_t* pg_error, char* sqlstate_out);
    static void mySQLErrorToSQLState(uint16_t mysql_errno, char* sqlstate_out);
    static void firebirdErrorToSQLState(uint32_t fb_code, char* sqlstate_out);
    
    // Build protocol-specific error messages
    static std::vector<uint8_t> buildPostgreSQLError(const char* sqlstate,
                                                     const std::string& message,
                                                     const std::string& detail);
    static std::vector<uint8_t> buildMySQLError(uint16_t error_code,
                                               const std::string& sqlstate,
                                               const std::string& message);
    static std::vector<uint8_t> buildFirebirdError(uint32_t error_code,
                                                  const std::string& message);
};

} // namespace ipc
} // namespace scratchbird
