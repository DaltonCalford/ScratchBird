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
#include "scratchbird/core/types.h"
#include <atomic>
#include <deque>
#include <optional>
#include <unordered_map>

namespace scratchbird {
namespace ipc {

struct FBMessageFieldDesc {
    uint8_t type_opcode = 0;
    int8_t scale = 0;
    uint16_t length = 0;
    uint16_t subtype = 0;
    bool not_nullable = false;
    uint32_t sql_type_override = 0;
};

struct FBSqldaVarDesc {
    std::string field_name;
    std::string relation_schema;
    std::string relation_name;
    std::string owner_name;
    std::string relation_alias;
    std::string alias_name;
};

struct FBProjectionBinding {
    uint8_t message_number = 0;
    uint16_t value_index = 0;
    std::optional<uint16_t> null_index;
};

struct FBInputBinding {
    uint8_t message_number = 0;
    uint16_t value_index = 0;
    std::optional<uint16_t> null_index;
};

struct FBCompiledRequestState {
    std::string stmt_name;
    std::unordered_map<uint8_t, std::vector<FBMessageFieldDesc>> message_fields;
    std::unordered_map<uint8_t, std::vector<FBProjectionBinding>> projection_bindings;
    std::vector<FBInputBinding> input_bindings;
    std::vector<std::optional<std::string>> bound_params;
    std::vector<bool> bound_param_nulls;
    bool input_values_ready = false;
    std::deque<std::vector<std::optional<std::string>>> pending_rows;
    bool execution_complete = false;
    bool portal_active = false;
    bool statement_prepared = false;
};

struct FBDsqlStatementState {
    std::string stmt_name;
    std::string cursor_name;
    std::string sql_text;
    std::vector<uint8_t> compiled_bytecode;
    std::unordered_map<uint8_t, std::vector<FBMessageFieldDesc>> input_message_fields;
    std::unordered_map<uint8_t, std::vector<FBMessageFieldDesc>> output_message_fields;
    std::vector<FBSqldaVarDesc> input_sqlda_fields;
    std::vector<FBSqldaVarDesc> output_sqlda_fields;
    std::vector<std::string> output_field_names;
    std::vector<std::optional<std::string>> bound_params;
    std::vector<bool> bound_param_nulls;
    std::deque<std::vector<std::optional<std::string>>> pending_rows;
    uint64_t select_count = 0;
    uint64_t insert_count = 0;
    uint64_t update_count = 0;
    uint64_t delete_count = 0;
    int64_t current_fetch_index = -1;
    bool execution_complete = false;
    bool portal_active = false;
    bool statement_prepared = false;
    bool engine_statement_prepared = false;
};

struct FBTransactionState {
    uint32_t transaction_id = 0;
    uint32_t oldest_interesting = 0;
    uint32_t oldest_snapshot = 0;
    uint32_t oldest_active = 0;
    uint8_t isolation_mode = 2;
    uint8_t read_committed_mode = 0;
    bool read_only = false;
    uint32_t lock_timeout = 0;
    uint64_t snapshot_number = 0;
    bool prepared = false;
    std::string prepare_description;
    std::string database_path;
};

/**
 * Client state for Firebird protocol
 */
struct FBClientState {
    enum State {
        CONNECTING,
        CONNECTED,
        ATTACHED,
        IN_TRANSACTION,
        DISCONNECTED
    };
    
    int client_fd = -1;
    uint32_t client_id = 0;
    uint32_t session_id = 0;
    State state = CONNECTING;
    
    // Protocol info
    uint32_t protocol_version = 0;
    uint32_t accept_version = 0;
    uint32_t accept_type = 0;
    uint32_t last_op = 0;
    
    // Connection info
    std::string database;
    std::string emulated_schema_root;
    std::string username;
    std::string auth_plugin;
    std::vector<uint8_t> auth_data;
    
    // Handles
    uint32_t handle = 0;
    uint32_t attachment_id = 0;
    
    // Wire encryption
    bool wire_encrypted = false;
    std::string encryption_plugin;
    
    // Scramble for auth
    std::vector<uint8_t> scramble;
    std::unordered_map<uint32_t, FBCompiledRequestState> compiled_requests;
    std::unordered_map<uint32_t, FBDsqlStatementState> dsql_statements;
    std::unordered_map<uint32_t, FBTransactionState> transactions;
    bool pending_catalog_refresh = false;
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
    core::Status handleConnect(FBClientState& state, core::ErrorContext* ctx);
    
    /**
     * Send op_accept response
     */
    core::Status sendAccept(FBClientState& state, core::ErrorContext* ctx);
    
    /**
     * Send op_reject
     */
    void sendReject(FBClientState& state, uint32_t error_code, 
                   const std::string& message);
    
    // ========================================================================
    // Operation Handlers
    // ========================================================================
    
    /**
     * Read and dispatch an operation
     */
    core::Status handleOperation(FBClientState& state, core::ErrorContext* ctx);
    
    // Database operations
    core::Status handleAttach(FBClientState& state,
                             const std::vector<uint8_t>& packet,
                             core::ErrorContext* ctx);
    
    core::Status handleCreate(FBClientState& state,
                             const std::vector<uint8_t>& packet,
                             core::ErrorContext* ctx);
    
    core::Status handleDetach(FBClientState& state,
                             const std::vector<uint8_t>& packet,
                             core::ErrorContext* ctx);
    
    // Statement operations
    core::Status handleCompile(FBClientState& state,
                              const std::vector<uint8_t>& packet,
                              core::ErrorContext* ctx);

    core::Status handleAllocateStatement(FBClientState& state,
                                        const std::vector<uint8_t>& packet,
                                        core::ErrorContext* ctx);

    core::Status handlePrepareStatement(FBClientState& state,
                                       const std::vector<uint8_t>& packet,
                                       core::ErrorContext* ctx);

    core::Status handleExecImmediate(FBClientState& state,
                                    const std::vector<uint8_t>& packet,
                                    bool want_sql_response,
                                    core::ErrorContext* ctx);

    core::Status handleExecuteStatement(FBClientState& state,
                                       const std::vector<uint8_t>& packet,
                                       bool want_sql_response,
                                       core::ErrorContext* ctx);

    core::Status handleFetchStatement(FBClientState& state,
                                     const std::vector<uint8_t>& packet,
                                     bool scroll,
                                     core::ErrorContext* ctx);

    core::Status handleFreeStatement(FBClientState& state,
                                    const std::vector<uint8_t>& packet,
                                    core::ErrorContext* ctx);

    core::Status handleSetCursor(FBClientState& state,
                                const std::vector<uint8_t>& packet,
                                core::ErrorContext* ctx);
    
    core::Status handleStart(FBClientState& state,
                            const std::vector<uint8_t>& packet,
                            bool want_response,
                            core::ErrorContext* ctx);

    core::Status handleStartWithSend(FBClientState& state,
                                    const std::vector<uint8_t>& packet,
                                    bool want_response,
                                    core::ErrorContext* ctx);
    
    core::Status handleReceive(FBClientState& state,
                              const std::vector<uint8_t>& packet,
                              core::ErrorContext* ctx);
    
    core::Status handleSend(FBClientState& state,
                           const std::vector<uint8_t>& packet,
                           core::ErrorContext* ctx);
    
    core::Status handleUnwind(FBClientState& state,
                             const std::vector<uint8_t>& packet,
                             core::ErrorContext* ctx);

    core::Status handleRelease(FBClientState& state,
                              const std::vector<uint8_t>& packet,
                              core::ErrorContext* ctx);
    
    // Transaction operations
    core::Status handleTransaction(FBClientState& state,
                                  const std::vector<uint8_t>& packet,
                                  core::ErrorContext* ctx);
    
    core::Status handleCommit(FBClientState& state,
                             const std::vector<uint8_t>& packet,
                             bool retaining,
                             core::ErrorContext* ctx);
    
    core::Status handleRollback(FBClientState& state,
                               const std::vector<uint8_t>& packet,
                               bool retaining,
                               core::ErrorContext* ctx);
    
    core::Status handlePrepare(FBClientState& state,
                              const std::vector<uint8_t>& packet,
                              core::ErrorContext* ctx);

    core::Status handlePrepare2(FBClientState& state,
                               const std::vector<uint8_t>& packet,
                               core::ErrorContext* ctx);
    
    // Information operations
    core::Status handleInfo(FBClientState& state,
                           const std::vector<uint8_t>& packet,
                           uint32_t op,
                           core::ErrorContext* ctx);
    
    // BLOB operations
    core::Status handleBlobOpen(FBClientState& state,
                               const std::vector<uint8_t>& packet,
                               bool create,
                               core::ErrorContext* ctx);
    
    core::Status handleBlobGetSegment(FBClientState& state,
                                     const std::vector<uint8_t>& packet,
                                     core::ErrorContext* ctx);
    
    core::Status handleBlobPutSegment(FBClientState& state,
                                     const std::vector<uint8_t>& packet,
                                     core::ErrorContext* ctx);
    
    core::Status handleBlobClose(core::ErrorContext* ctx, bool cancel);
    
    // Wire encryption
    core::Status handleCrypt(FBClientState& state,
                            const std::vector<uint8_t>& packet,
                            core::ErrorContext* ctx);
    
    // Authentication
    core::Status handleAuthenticate(FBClientState& state,
                                   const std::vector<uint8_t>& packet,
                                   core::ErrorContext* ctx);
    
    // ========================================================================
    // Response Sending
    // ========================================================================
    
    /**
     * Send op_response
     */
    void sendResponse(FBClientState& state,
                     uint32_t handle,
                     uint32_t status_code,
                     const uint8_t* data,
                     size_t data_len,
                     core::ErrorContext* ctx);
    
    /**
     * Send error response (status vector)
     */
    core::Status sendErrorResponse(FBClientState& state,
                                  const std::string& message);
    
    // ========================================================================
    // I/O Helpers
    // ========================================================================
    
    /**
     * Read an XDR packet (4-byte length prefix)
     */
    core::Status readPacket(FBClientState& state,
                           std::vector<uint8_t>& packet,
                           core::ErrorContext* ctx);
    
    /**
     * Send an XDR packet
     */
    core::Status sendPacket(FBClientState& state,
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
    core::Status cleanupAttachmentState(FBClientState& state,
                                       core::ErrorContext* ctx);

    core::Status executeCompiledInternalQuery(
        FBClientState& state,
        const std::string& sql_text,
        std::vector<std::string>* column_names_out,
        std::deque<std::vector<std::optional<std::string>>>* rows_out,
        core::ErrorContext* ctx);

    core::Status ensureEngineSession(FBClientState& state,
                                     bool create_database_bootstrap,
                                     core::ErrorContext* ctx);
    core::Status executeSessionSql(FBClientState& state,
                                   const std::string& sql,
                                   bool ignore_exists_error,
                                   const char* failure_message,
                                   core::ErrorContext* ctx);
    core::Status ensureVirtualCatalogBinding(FBClientState& state,
                                             core::ErrorContext* ctx);
    core::Status refreshCommittedCatalogState(FBClientState& state,
                                              core::ErrorContext* ctx);

    core::Status closeEngineObject(FBClientState& state,
                                  uint32_t request_id_seed,
                                  char type,
                                  const std::string& name,
                                  core::ErrorContext* ctx);

    // Handle generation
    static uint32_t generateHandle();
};

} // namespace ipc
} // namespace scratchbird
