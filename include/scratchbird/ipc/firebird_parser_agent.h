/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0
 */
#pragma once

/**
 * FirebirdParserAgent - Full Wire Protocol Implementation
 * 
 * Implements the Firebird wire protocol (op_connect, op_accept, etc.)
 * using XDR encoding as documented in Firebird source and protocol docs.
 * 
 * Features:
 * - Protocol versions 10-16
 * - Arc4 and ChaCha wire encryption
 * - Authentication (Legacy, SRP, SRP256)
 * - XDR message format
 * - Events (async notifications)
 * - BLOB streaming
 */

#include "scratchbird/ipc/parser_agent.h"
#include <atomic>

namespace scratchbird {
namespace ipc {

/**
 * Client state for Firebird protocol
 */
struct ClientState {
    enum State {
        CONNECTING,
        CONNECTED,
        ATTACHED,
        IN_TRANSACTION,
        DISCONNECTED
    };
    
    int client_fd = -1;
    State state = CONNECTING;
    
    // Protocol info
    uint32_t protocol_version = 0;
    uint32_t accept_version = 0;
    uint32_t last_op = 0;
    
    // Connection info
    std::string database;
    std::string username;
    std::string auth_plugin;
    std::vector<uint8_t> auth_data;
    
    // Handles
    uint32_t handle = 0;
    
    // Wire encryption
    bool wire_encrypted = false;
    std::string encryption_plugin;
    
    // Scramble for auth
    std::vector<uint8_t> scramble;
};

/**
 * Firebird wire protocol parser agent
 * 
 * Full implementation of Firebird wire protocol including:
 * - XDR message encoding/decoding
 * - op_connect/op_accept handshake
 * - Multiple authentication plugins (Legacy, SRP, SRP256)
 * - Wire encryption (Arc4, ChaCha)
 * - BLOB operations
 * - Events/notifications
 */
class FirebirdParserAgent : public EmulatedParserAgent {
public:
    explicit FirebirdParserAgent(const ParserAgentConfig& config);
    ~FirebirdParserAgent() override;
    
    // Non-copyable
    FirebirdParserAgent(const FirebirdParserAgent&) = delete;
    FirebirdParserAgent& operator=(const FirebirdParserAgent&) = delete;

    // ========================================================================
    // Main Handler
    // ========================================================================
    
    /**
     * Handle a Firebird client connection
     */
    core::Status handleClient(int client_fd, core::ErrorContext* ctx) override;
    
    // ========================================================================
    // Protocol Phases
    // ========================================================================
    
    /**
     * Handle op_connect/op_attach
     */
    core::Status handleConnect(ClientState& state, core::ErrorContext* ctx);
    
    /**
     * Send op_accept response
     */
    core::Status sendAccept(ClientState& state, core::ErrorContext* ctx);
    
    /**
     * Send op_reject
     */
    void sendReject(ClientState& state, uint32_t error_code, 
                   const std::string& message);
    
    // ========================================================================
    // Operation Handlers
    // ========================================================================
    
    /**
     * Read and dispatch an operation
     */
    core::Status handleOperation(ClientState& state, core::ErrorContext* ctx);
    
    // Database operations
    core::Status handleAttach(ClientState& state,
                             const std::vector<uint8_t>& packet,
                             core::ErrorContext* ctx);
    
    core::Status handleCreate(ClientState& state,
                             const std::vector<uint8_t>& packet,
                             core::ErrorContext* ctx);
    
    core::Status handleDetach(core::ErrorContext* ctx);
    
    // Statement operations
    core::Status handleCompile(ClientState& state,
                              const std::vector<uint8_t>& packet,
                              core::ErrorContext* ctx);
    
    core::Status handleStart(ClientState& state,
                            const std::vector<uint8_t>& packet,
                            bool want_response,
                            core::ErrorContext* ctx);
    
    core::Status handleReceive(ClientState& state,
                              const std::vector<uint8_t>& packet,
                              core::ErrorContext* ctx);
    
    core::Status handleSend(ClientState& state,
                           const std::vector<uint8_t>& packet,
                           core::ErrorContext* ctx);
    
    core::Status handleUnwind(core::ErrorContext* ctx);
    
    core::Status handleRelease(core::ErrorContext* ctx);
    
    // Transaction operations
    core::Status handleTransaction(ClientState& state,
                                  const std::vector<uint8_t>& packet,
                                  core::ErrorContext* ctx);
    
    core::Status handleCommit(ClientState& state,
                             bool retaining,
                             core::ErrorContext* ctx);
    
    core::Status handleRollback(ClientState& state,
                               bool retaining,
                               core::ErrorContext* ctx);
    
    core::Status handlePrepare(core::ErrorContext* ctx);
    
    // Information operations
    core::Status handleInfo(ClientState& state,
                           const std::vector<uint8_t>& packet,
                           uint32_t op,
                           core::ErrorContext* ctx);
    
    // BLOB operations
    core::Status handleBlobOpen(ClientState& state,
                               const std::vector<uint8_t>& packet,
                               bool create,
                               core::ErrorContext* ctx);
    
    core::Status handleBlobGetSegment(ClientState& state,
                                     const std::vector<uint8_t>& packet,
                                     core::ErrorContext* ctx);
    
    core::Status handleBlobPutSegment(ClientState& state,
                                     const std::vector<uint8_t>& packet,
                                     core::ErrorContext* ctx);
    
    core::Status handleBlobClose(core::ErrorContext* ctx, bool cancel);
    
    // Wire encryption
    core::Status handleCrypt(ClientState& state,
                            const std::vector<uint8_t>& packet,
                            core::ErrorContext* ctx);
    
    // Authentication
    core::Status handleAuthenticate(ClientState& state,
                                   const std::vector<uint8_t>& packet,
                                   core::ErrorContext* ctx);
    
    // ========================================================================
    // Response Sending
    // ========================================================================
    
    /**
     * Send op_response
     */
    void sendResponse(ClientState& state,
                     uint32_t handle,
                     uint32_t status_code,
                     const uint8_t* data,
                     size_t data_len,
                     core::ErrorContext* ctx);
    
    /**
     * Send error response (status vector)
     */
    core::Status sendErrorResponse(ClientState& state,
                                  const std::string& message);
    
    // ========================================================================
    // I/O Helpers
    // ========================================================================
    
    /**
     * Read an XDR packet (4-byte length prefix)
     */
    core::Status readPacket(ClientState& state,
                           std::vector<uint8_t>& packet,
                           core::ErrorContext* ctx);
    
    /**
     * Send an XDR packet
     */
    core::Status sendPacket(ClientState& state,
                           const std::vector<uint8_t>& packet,
                           core::ErrorContext* ctx);
    
    // Inherited methods
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
     * Get message length from XDR header (4 bytes BE)
     */
    size_t readMessageLength(const uint8_t* header, size_t len) override;
    
    // ========================================================================
    // Type Conversion
    // ========================================================================
    
    /**
     * Map Firebird type to ScratchBird DataType
     */
    static core::DataType firebirdTypeToDataType(uint32_t fb_type);
    
    /**
     * Map ScratchBird DataType to Firebird type
     */
    static uint32_t dataTypeToFirebirdType(core::DataType type);
    
    /**
     * Format value for Firebird XDR transport
     */
    static std::vector<uint8_t> formatValueXDR(const core::TypedValue& value,
                                               uint32_t fb_type);
    
    /**
     * Parse value from Firebird XDR transport
     */
    static core::TypedValue parseValueXDR(const std::vector<uint8_t>& data,
                                         uint32_t fb_type);

private:
    // Handle generation
    static uint32_t generateHandle();
};

} // namespace ipc
} // namespace scratchbird
