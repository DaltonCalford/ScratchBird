/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0
 */
#pragma once

/**
 * PostgreSQLParserAgent - Full Wire Protocol Implementation
 * 
 * Implements the complete PostgreSQL frontend/backend protocol (3.0)
 * as documented in https://www.postgresql.org/docs/current/protocol.html
 * 
 * Features:
 * - Full protocol 3.0 support
 * - SSL request handling
 * - Multiple authentication methods (trust, password, MD5, SASL/SCRAM-SHA-256)
 * - Simple query protocol
 * - Extended query protocol (Parse, Bind, Execute, Close, Describe, Sync)
 * - COPY protocol (COPY FROM/TO)
 * - Error and notice message formatting
 * - Parameter status messages
 * - Prepared statement and portal management
 */

#include "scratchbird/ipc/parser_agent.h"
#include "scratchbird/core/types.h"
#include <unordered_map>

namespace scratchbird {
namespace ipc {

/**
 * Client state for PostgreSQL protocol
 */
struct PGClientState {
    enum State {
        STARTUP,
        AUTHENTICATING,
        IDLE,
        IN_TRANSACTION,
        IN_FAILED_TRANSACTION,
        COPY_IN,
        COPY_OUT,
        TERMINATED
    };
    
    int client_fd = -1;
    State state = STARTUP;
    char transaction_status = 'I';  // I=idle, T=in transaction, E=failed transaction
    
    // Connection info
    std::string username;
    std::string database;
    std::unordered_map<std::string, std::string> params;
    
    // Backend key data (for cancel)
    uint32_t process_id = 0;
    uint32_t secret_key = 0;
    
    // Client tracking for IPC
    uint32_t client_id = 0;
    
    // Request tracking
    uint32_t request_id = 0;
    
    // Prepared statements
    struct PreparedStatementInfo {
        std::string name;
        std::string sql;
        std::vector<uint32_t> param_types;
        bool valid = false;
    };
    std::unordered_map<std::string, PreparedStatementInfo> prepared_stmts;
    
    // Portals (cursors)
    struct PortalInfo {
        std::string name;
        std::string stmt_name;
        std::vector<std::vector<uint8_t>> params;
        std::vector<uint16_t> result_formats;
        bool is_open = false;
    };
    std::unordered_map<std::string, PortalInfo> portals;
    
    // COPY state
    bool in_copy_mode = false;
    bool copy_is_in = false;  // true=COPY FROM, false=COPY TO
    uint32_t copy_chunk_id = 0;
};

/**
 * PostgreSQL wire protocol parser agent
 * 
 * Full implementation of PostgreSQL protocol 3.0 including:
 * - Startup and SSL negotiation
 * - Authentication (trust, password, MD5, SCRAM-SHA-256)
 * - Simple and extended query protocols
 * - COPY protocol
 * - Asynchronous notifications (prepared for LISTEN/NOTIFY)
 */
class PostgreSQLParserAgent : public EmulatedParserAgent {
public:
    explicit PostgreSQLParserAgent(const ParserAgentConfig& config);
    ~PostgreSQLParserAgent() override;
    
    // Non-copyable
    PostgreSQLParserAgent(const PostgreSQLParserAgent&) = delete;
    PostgreSQLParserAgent& operator=(const PostgreSQLParserAgent&) = delete;

    // ========================================================================
    // Main Handler
    // ========================================================================
    
    /**
     * Handle a PostgreSQL client connection
     * 
     * This is the main entry point that manages the full client lifecycle:
     * 1. Handle startup/SSL negotiation
     * 2. Authenticate
     * 3. Process messages until termination
     */
    core::Status handleClient(int client_fd, core::ErrorContext* ctx) override;
    
    // ========================================================================
    // Protocol Phases
    // ========================================================================
    
    /**
     * Handle startup phase (version negotiation, SSL, parameters)
     */
    core::Status handleStartupPhase(PGClientState& state, core::ErrorContext* ctx);
    
    /**
     * Perform authentication based on configured method
     */
    core::Status authenticate(PGClientState& state, core::ErrorContext* ctx);
    
    /**
     * Handle SASL/SCRAM-SHA-256 authentication
     */
    core::Status handleSASLAuth(PGClientState& state, core::ErrorContext* ctx);
    
    /**
     * Handle cancel request
     */
    core::Status handleCancelRequest(PGClientState& state, 
                                    const std::vector<uint8_t>& msg,
                                    core::ErrorContext* ctx);
    
    // ========================================================================
    // Message Handlers
    // ========================================================================
    
    /**
     * Read and dispatch a single message
     */
    core::Status handleMessage(PGClientState& state, core::ErrorContext* ctx);
    
    // Simple query (Q)
    core::Status handleQueryMessage(PGClientState& state,
                                   const std::vector<uint8_t>& msg,
                                   core::ErrorContext* ctx);
    
    // Extended query protocol
    core::Status handleParseMessage(PGClientState& state,
                                   const std::vector<uint8_t>& msg,
                                   core::ErrorContext* ctx);
    
    core::Status handleBindMessage(PGClientState& state,
                                  const std::vector<uint8_t>& msg,
                                  core::ErrorContext* ctx);
    
    core::Status handleExecuteMessage(PGClientState& state,
                                     const std::vector<uint8_t>& msg,
                                     core::ErrorContext* ctx);
    
    core::Status handleCloseMessage(PGClientState& state,
                                   const std::vector<uint8_t>& msg,
                                   core::ErrorContext* ctx);
    
    core::Status handleDescribeMessage(PGClientState& state,
                                      const std::vector<uint8_t>& msg,
                                      core::ErrorContext* ctx);
    
    core::Status handleSyncMessage(PGClientState& state,
                                  core::ErrorContext* ctx);
    
    // COPY protocol
    core::Status handleCopyDataMessage(PGClientState& state,
                                      const std::vector<uint8_t>& msg,
                                      core::ErrorContext* ctx);
    
    core::Status handleCopyDoneMessage(PGClientState& state,
                                      core::ErrorContext* ctx);
    
    core::Status handleCopyFailMessage(PGClientState& state,
                                      const std::vector<uint8_t>& msg,
                                      core::ErrorContext* ctx);
    
    // ========================================================================
    // Message Sending
    // ========================================================================
    
    // Authentication messages
    void sendAuthenticationOk(PGClientState& state);
    void sendAuthenticationCleartext(PGClientState& state);
    void sendAuthenticationMD5(PGClientState& state, const std::string& salt);
    void sendAuthenticationSASL(PGClientState& state);
    void sendAuthenticationSASLContinue(PGClientState& state, const std::string& data);
    void sendAuthenticationSASLFinal(PGClientState& state, const std::string& data);
    
    // Session messages
    void sendBackendKeyData(PGClientState& state);
    void sendReadyForQuery(PGClientState& state);
    void sendParameterStatus(PGClientState& state, 
                            const std::string& name,
                            const std::string& value);
    
    // Result messages
    void sendRowDescription(PGClientState& state,
                           const std::vector<IPCFieldDesc>& fields);
    void sendDataRow(PGClientState& state,
                    const std::vector<std::optional<std::string>>& values);
    void sendCommandComplete(PGClientState& state, const std::string& tag);
    void sendEmptyQueryResponse(PGClientState& state);
    void sendPortalSuspended(PGClientState& state);
    void sendNoData(PGClientState& state);
    
    // Extended query protocol responses
    void sendParseComplete(PGClientState& state);
    void sendBindComplete(PGClientState& state);
    void sendCloseComplete(PGClientState& state);
    void sendParameterDescription(PGClientState& state,
                                  const std::vector<uint32_t>& type_oids);
    
    // Error and notice
    core::Status sendErrorResponse(PGClientState& state,
                                  const std::string& sqlstate,
                                  const std::string& message);
    void sendErrorResponse(PGClientState& state,
                          const std::string& severity,
                          const std::string& sqlstate,
                          const std::string& message,
                          const std::string& detail = "",
                          const std::string& hint = "");
    void sendNoticeResponse(PGClientState& state,
                           const std::string& severity,
                           const std::string& message);
    
    // IPC message translation
    core::Status translateAndSendResponse(PGClientState& state,
                                         const IPCMessage& ipc_response,
                                         core::ErrorContext* ctx);
    
    // COPY protocol
    void sendCopyInResponse(PGClientState& state,
                           uint8_t format,
                           const std::vector<uint16_t>& column_formats);
    void sendCopyOutResponse(PGClientState& state,
                            uint8_t format,
                            const std::vector<uint16_t>& column_formats);
    void sendCopyData(PGClientState& state, const std::vector<uint8_t>& data);
    void sendCopyDone(PGClientState& state);
    
    // Notification
    void sendNotification(PGClientState& state,
                         const std::string& channel,
                         const std::string& payload,
                         uint32_t pid);
    
    // ========================================================================
    // I/O Helpers
    // ========================================================================
    
    /**
     * Read a message with type byte (normal protocol messages)
     */
    core::Status readMessageWithType(int fd,
                                    std::vector<uint8_t>& message,
                                    core::ErrorContext* ctx);
    
    /**
     * Read startup/cancel message (no type byte)
     */
    core::Status readFullMessage(int fd,
                                std::vector<uint8_t>& message,
                                core::ErrorContext* ctx) override;
    
    /**
     * Write message to client
     */
    core::Status writeMessage(int fd,
                             const std::vector<uint8_t>& message,
                             core::ErrorContext* ctx) override;
    
    // ========================================================================
    // Translation Methods
    // ========================================================================
    
    core::Status translateStartupToIPC(const std::vector<uint8_t>& startup,
                                      IPCMessage& ipc_msg,
                                      core::ErrorContext* ctx) override;
    
    core::Status translateIPCToResponse(const IPCMessage& ipc_msg,
                                       std::vector<uint8_t>& response,
                                       core::ErrorContext* ctx) override;
    
    // ========================================================================
    // Utility Methods
    // ========================================================================
    
    /**
     * Map IPC SQLSTATE to PostgreSQL wire format
     */
    std::string mapSQLStateToProtocol(const char* sqlstate) override;
    
    /**
     * Map PostgreSQL protocol error to SQLSTATE
     */
    void mapProtocolErrorToSQLState(const std::vector<uint8_t>& error,
                                   char* sqlstate_out) override;
    
    /**
     * Get message length from header
     */
    size_t readMessageLength(const uint8_t* header, size_t len) override;
    
    /**
     * Map PostgreSQL message type to IPC type
     */
    IPCMessageType mapClientToIPC(uint8_t msg_type) override;
    
    /**
     * Map IPC message type to PostgreSQL type
     */
    uint8_t mapIPCToClient(IPCMessageType msg_type) override;
    
    // ========================================================================
    // Type Conversion
    // ========================================================================
    
    /**
     * Convert PostgreSQL OID to DataType
     */
    static core::DataType oidToDataType(uint32_t oid);
    
    /**
     * Convert DataType to PostgreSQL OID
     */
    static uint32_t dataTypeToOid(core::DataType type);
    
    /**
     * Format value for PostgreSQL text format
     */
    static std::string formatValueText(const core::TypedValue& value);
    
    /**
     * Format value for PostgreSQL binary format
     */
    static std::vector<uint8_t> formatValueBinary(const core::TypedValue& value);
    
    /**
     * Parse value from PostgreSQL text format
     */
    static core::TypedValue parseValueText(const std::string& text, core::DataType type);
    
    /**
     * Parse value from PostgreSQL binary format
     */
    static core::TypedValue parseValueBinary(const std::vector<uint8_t>& data, 
                                             core::DataType type);

private:
    // Protocol version
    static constexpr int PROTOCOL_VERSION = 196608;  // 3.0
    static constexpr int SSL_REQUEST_CODE = 80877103;
    static constexpr int CANCEL_REQUEST_CODE = 80877102;
};

} // namespace ipc
} // namespace scratchbird
