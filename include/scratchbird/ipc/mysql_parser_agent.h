/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0
 */
#pragma once

/**
 * MySQLParserAgent - Full Wire Protocol Implementation
 * 
 * Implements the MySQL client/server protocol as documented in
 * https://dev.mysql.com/doc/dev/mysql-server/latest/page_protocol_basics.html
 * 
 * Features:
 * - Protocol version 10
 * - Capability negotiation (CLIENT_PROTOCOL_41, CLIENT_DEPRECATE_EOF, etc.)
 * - Authentication (mysql_native_password, caching_sha2_password)
 * - Text protocol (COM_QUERY)
 * - Binary protocol (COM_STMT_PREPARE/EXECUTE)
 * - Multiple result sets
 * - Prepared statements with parameter binding
 * - SSL/TLS support (prepared)
 */

#include "scratchbird/ipc/parser_agent.h"
#include "scratchbird/core/types.h"
#include <unordered_map>
#include <atomic>

namespace scratchbird {
namespace ipc {

/**
 * Client state for MySQL protocol
 */
struct MySQLClientState {
    enum State {
        HANDSHAKE,
        AUTHENTICATING,
        READY,
        IN_TRANSACTION,
        TERMINATED
    };
    
    int client_fd = -1;
    State state = HANDSHAKE;
    uint8_t seq = 0;
    
    // Connection info
    uint32_t connection_id = 0;
    std::string username;
    std::string database;
    std::string auth_plugin;
    std::vector<uint8_t> auth_response;
    std::vector<uint8_t> scramble;
    uint8_t charset = 0;
    
    // Capabilities
    uint32_t capabilities = 0;
    uint16_t status_flags = 0;
    
    // Prepared statements
    struct PreparedStatement {
        uint32_t id = 0;
        std::string sql;
        uint16_t param_count = 0;
        uint16_t column_count = 0;
    };
    std::unordered_map<uint32_t, PreparedStatement> prepared_stmts;
    uint32_t stmt_counter = 0;
};

/**
 * MySQL wire protocol parser agent
 * 
 * Full implementation of MySQL protocol 4.1+ including:
 * - Handshake V10
 * - Capability negotiation
 * - Multiple authentication plugins
 * - Text and binary protocols
 * - Prepared statements
 * - Multiple result sets
 */
class MySQLParserAgent : public EmulatedParserAgent {
public:
    explicit MySQLParserAgent(const ParserAgentConfig& config);
    ~MySQLParserAgent() override;
    
    // Non-copyable
    MySQLParserAgent(const MySQLParserAgent&) = delete;
    MySQLParserAgent& operator=(const MySQLParserAgent&) = delete;

    // ========================================================================
    // Main Handler
    // ========================================================================
    
    /**
     * Handle a MySQL client connection
     */
    core::Status handleClient(int client_fd, core::ErrorContext* ctx) override;
    
    // ========================================================================
    // Protocol Phases
    // ========================================================================
    
    /**
     * Send handshake V10 packet
     */
    core::Status sendHandshakeV10(MySQLClientState& state, core::ErrorContext* ctx);
    
    /**
     * Read and parse handshake response
     */
    core::Status readHandshakeResponse(MySQLClientState& state, core::ErrorContext* ctx);
    
    /**
     * Authenticate client
     */
    core::Status authenticate(MySQLClientState& state, core::ErrorContext* ctx);
    
    // Authentication methods
    core::Status authenticateNativePassword(MySQLClientState& state);
    core::Status authenticateCachingSha2Password(MySQLClientState& state);
    core::Status authenticateSha256Password(MySQLClientState& state);
    
    // ========================================================================
    // Command Handlers
    // ========================================================================
    
    /**
     * Read and dispatch a command
     */
    core::Status handleCommand(MySQLClientState& state, core::ErrorContext* ctx);
    
    // Database commands
    core::Status handleInitDB(MySQLClientState& state,
                             const std::vector<uint8_t>& packet,
                             core::ErrorContext* ctx);
    
    // Query commands
    core::Status handleQuery(MySQLClientState& state,
                            const std::vector<uint8_t>& packet,
                            core::ErrorContext* ctx);
    
    core::Status handleFieldList(MySQLClientState& state,
                                const std::vector<uint8_t>& packet,
                                core::ErrorContext* ctx);
    
    // Prepared statement commands
    core::Status handleStmtPrepare(MySQLClientState& state,
                                  const std::vector<uint8_t>& packet,
                                  core::ErrorContext* ctx);
    
    core::Status handleStmtExecute(MySQLClientState& state,
                                  const std::vector<uint8_t>& packet,
                                  core::ErrorContext* ctx);
    
    core::Status handleStmtClose(MySQLClientState& state,
                                const std::vector<uint8_t>& packet,
                                core::ErrorContext* ctx);
    
    core::Status handleStmtReset(MySQLClientState& state,
                                const std::vector<uint8_t>& packet,
                                core::ErrorContext* ctx);
    
    core::Status handleStmtFetch(MySQLClientState& state,
                                const std::vector<uint8_t>& packet,
                                core::ErrorContext* ctx);
    
    // Session commands
    core::Status handleSetOption(MySQLClientState& state,
                                const std::vector<uint8_t>& packet,
                                core::ErrorContext* ctx);
    
    core::Status handleResetConnection(MySQLClientState& state,
                                      core::ErrorContext* ctx);
    
    // ========================================================================
    // Packet Sending
    // ========================================================================
    
    /**
     * Send OK packet
     */
    void sendOKPacket(MySQLClientState& state,
                     uint64_t affected_rows,
                     uint64_t last_insert_id,
                     uint16_t status_flags,
                     uint16_t warnings,
                     const std::string& info);
    
    /**
     * Send ERROR packet
     */
    void sendErrorPacket(MySQLClientState& state,
                        uint16_t error_code,
                        const std::string& sqlstate,
                        const std::string& message);
    
    /**
     * Send EOF packet (or OK if CLIENT_DEPRECATE_EOF)
     */
    void sendEOFPacket(MySQLClientState& state,
                      uint16_t warnings,
                      uint16_t status_flags);
    
    /**
     * Send result set
     */
    void sendResultSet(MySQLClientState& state,
                      const std::vector<IPCFieldDesc>& fields,
                      const std::vector<std::vector<std::optional<std::string>>>& rows);
    
    /**
     * Send column definition packet
     */
    void sendColumnDefinition(MySQLClientState& state,
                             const IPCFieldDesc& field);
    
    // ========================================================================
    // I/O Helpers
    // ========================================================================
    
    /**
     * Read a MySQL packet (handles 3-byte length + 1-byte seq)
     */
    core::Status readPacket(MySQLClientState& state,
                           std::vector<uint8_t>& packet,
                           core::ErrorContext* ctx);
    
    /**
     * Send a MySQL packet
     */
    core::Status sendPacket(MySQLClientState& state,
                           const std::vector<uint8_t>& payload,
                           core::ErrorContext* ctx);
    
    // Inherited from EmulatedParserAgent
    core::Status readFullMessage(int fd,
                                std::vector<uint8_t>& message,
                                core::ErrorContext* ctx) override;
    
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
    
    IPCMessageType mapClientToIPC(uint8_t msg_type) override;
    uint8_t mapIPCToClient(IPCMessageType msg_type) override;
    std::string mapSQLStateToProtocol(const char* sqlstate) override;
    void mapProtocolErrorToSQLState(const std::vector<uint8_t>& error,
                                   char* sqlstate_out) override;
    
    /**
     * Get message length from header (3 bytes LE)
     */
    size_t readMessageLength(const uint8_t* header, size_t len) override;
    
    // ========================================================================
    // Type Conversion
    // ========================================================================
    
    /**
     * Map ScratchBird DataType to MySQL field type
     */
    static uint8_t mapDataTypeToMySQL(core::DataType type);
    
    /**
     * Map MySQL field type to ScratchBird DataType
     */
    static core::DataType mapMySQLToDataType(uint8_t mysql_type);
    
    /**
     * Format value for MySQL binary protocol
     */
    static std::vector<uint8_t> formatValueBinary(const core::TypedValue& value,
                                                  uint8_t mysql_type);
    
    /**
     * Parse value from MySQL binary protocol
     */
    static core::TypedValue parseValueBinary(const std::vector<uint8_t>& data,
                                            uint8_t mysql_type);

private:
    // Utility methods
    static uint32_t generateConnectionId();
    static void generateScramble(uint8_t* out, size_t len);
};

} // namespace ipc
} // namespace scratchbird
