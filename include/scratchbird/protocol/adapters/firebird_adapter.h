/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * Firebird Wire Protocol Adapter
 *
 * ScratchBird Network Layer - Phase 3.2
 *
 * Implements Firebird wire protocol for client compatibility.
 * Supports Firebird 3.0+ protocol with SRP authentication.
 *
 * Reference: Firebird Wire Protocol Documentation
 * https://firebirdsql.org/file/documentation/html/en/firebirddocs/wireprotocol/firebird-wire-protocol.html
 */

#pragma once

#include "scratchbird/protocol/adapters/protocol_adapter.h"
#include "scratchbird/client/connection.h"
#include "scratchbird/server/ipc_server.h"

#include <unordered_map>
#include <unordered_set>
#include <deque>

namespace scratchbird {
namespace protocol {

// ============================================================================
// Firebird Protocol Constants
// ============================================================================

namespace firebird {

// Protocol versions
constexpr uint32_t PROTOCOL_VERSION_10 = 10;  // Firebird 1.0
constexpr uint32_t PROTOCOL_VERSION_11 = 11;  // Firebird 1.5
constexpr uint32_t PROTOCOL_VERSION_12 = 12;  // Firebird 2.0
constexpr uint32_t PROTOCOL_VERSION_13 = 13;  // Firebird 2.1
constexpr uint32_t PROTOCOL_VERSION_15 = 15;  // Firebird 3.0
constexpr uint32_t PROTOCOL_VERSION_16 = 16;  // Firebird 4.0
constexpr uint32_t PROTOCOL_VERSION_18 = 18;  // Firebird 5.0

// Default protocol version to advertise (Firebird 5.0)
constexpr uint32_t DEFAULT_PROTOCOL_VERSION = PROTOCOL_VERSION_18;

// Architecture types
constexpr uint32_t ARCH_GENERIC = 1;

// Operation codes (opcodes)
namespace Opcode {
    // Connection operations
    constexpr uint32_t op_connect          = 1;
    constexpr uint32_t op_exit             = 2;
    constexpr uint32_t op_accept           = 3;
    constexpr uint32_t op_reject           = 4;
    constexpr uint32_t op_disconnect       = 6;
    constexpr uint32_t op_accept_data      = 76;
    constexpr uint32_t op_cond_accept      = 77;

    // Database operations
    constexpr uint32_t op_attach           = 19;
    constexpr uint32_t op_create           = 20;
    constexpr uint32_t op_detach           = 21;
    constexpr uint32_t op_drop_database    = 81;

    // Transaction operations
    constexpr uint32_t op_transaction      = 29;
    constexpr uint32_t op_commit           = 30;
    constexpr uint32_t op_rollback         = 31;
    constexpr uint32_t op_commit_retaining = 50;
    constexpr uint32_t op_rollback_retaining = 86;
    constexpr uint32_t op_prepare          = 67;

    // Statement operations
    constexpr uint32_t op_allocate_statement = 62;
    constexpr uint32_t op_prepare_statement  = 68;
    constexpr uint32_t op_execute           = 63;
    constexpr uint32_t op_execute2          = 71;  // execute with output blr
    constexpr uint32_t op_exec_immediate    = 64;
    constexpr uint32_t op_exec_immediate2   = 65;  // exec immediate with output blr
    constexpr uint32_t op_fetch             = 66;  // fetch rows
    constexpr uint32_t op_free_statement    = 67;
    constexpr uint32_t op_set_cursor        = 69;

    // Info operations
    constexpr uint32_t op_info_database    = 40;
    constexpr uint32_t op_info_transaction = 42;
    constexpr uint32_t op_info_sql         = 70;
    constexpr uint32_t op_info_blob        = 43;

    // BLOB operations
    constexpr uint32_t op_create_blob      = 34;
    constexpr uint32_t op_open_blob        = 35;
    constexpr uint32_t op_get_segment      = 36;
    constexpr uint32_t op_put_segment      = 37;
    constexpr uint32_t op_cancel_blob      = 38;
    constexpr uint32_t op_close_blob       = 39;
    constexpr uint32_t op_seek_blob        = 61;
    constexpr uint32_t op_open_blob2       = 56;
    constexpr uint32_t op_create_blob2     = 57;

    // Response operations
    constexpr uint32_t op_response         = 9;
    constexpr uint32_t op_fetch_response   = 72;  // fetch response
    constexpr uint32_t op_sql_response     = 78;

    // Batch operations (Firebird 4.0+)
    constexpr uint32_t op_batch_create     = 99;
    constexpr uint32_t op_batch_msg        = 100;
    constexpr uint32_t op_batch_exec       = 101;
    constexpr uint32_t op_batch_rls        = 102;
    constexpr uint32_t op_batch_cs         = 103;
    constexpr uint32_t op_batch_regblob    = 104;
    constexpr uint32_t op_batch_blob_stream = 105;
    constexpr uint32_t op_batch_set_bpb    = 106;

    // Service Manager operations
    constexpr uint32_t op_service_attach   = 82;
    constexpr uint32_t op_service_detach   = 83;
    constexpr uint32_t op_service_info     = 84;
    constexpr uint32_t op_service_start    = 85;

    // Event operations
    constexpr uint32_t op_que_events       = 48;
    constexpr uint32_t op_cancel_events    = 49;
    constexpr uint32_t op_event            = 52;

    // Authentication (Firebird 3.0+)
    constexpr uint32_t op_cont_auth        = 79;
    constexpr uint32_t op_ping             = 93;
    constexpr uint32_t op_cancel           = 91;
}

// SQL statement types
namespace StatementType {
    constexpr uint32_t TYPE_SELECT         = 1;
    constexpr uint32_t TYPE_INSERT         = 2;
    constexpr uint32_t TYPE_UPDATE         = 3;
    constexpr uint32_t TYPE_DELETE         = 4;
    constexpr uint32_t TYPE_DDL            = 5;
    constexpr uint32_t TYPE_GET_SEGMENT    = 6;
    constexpr uint32_t TYPE_PUT_SEGMENT    = 7;
    constexpr uint32_t TYPE_EXEC_PROCEDURE = 8;
    constexpr uint32_t TYPE_START_TRANS    = 9;
    constexpr uint32_t TYPE_COMMIT         = 10;
    constexpr uint32_t TYPE_ROLLBACK       = 11;
    constexpr uint32_t TYPE_SELECT_FOR_UPD = 12;
    constexpr uint32_t TYPE_SET_GENERATOR  = 13;
    constexpr uint32_t TYPE_SAVEPOINT      = 14;
}

// SQL data types
namespace SqlType {
    constexpr uint16_t SQL_TEXT      = 452;
    constexpr uint16_t SQL_VARYING   = 448;
    constexpr uint16_t SQL_SHORT     = 500;
    constexpr uint16_t SQL_LONG      = 496;
    constexpr uint16_t SQL_FLOAT     = 482;
    constexpr uint16_t SQL_DOUBLE    = 480;
    constexpr uint16_t SQL_D_FLOAT   = 530;
    constexpr uint16_t SQL_TIMESTAMP = 510;
    constexpr uint16_t SQL_BLOB      = 520;
    constexpr uint16_t SQL_ARRAY     = 540;
    constexpr uint16_t SQL_QUAD      = 550;
    constexpr uint16_t SQL_TYPE_TIME = 560;
    constexpr uint16_t SQL_TYPE_DATE = 570;
    constexpr uint16_t SQL_INT64     = 580;
    constexpr uint16_t SQL_INT128    = 32752;
    constexpr uint16_t SQL_TIMESTAMP_TZ = 32754;
    constexpr uint16_t SQL_TIME_TZ   = 32756;
    constexpr uint16_t SQL_DEC16     = 32760;
    constexpr uint16_t SQL_DEC34     = 32762;
    constexpr uint16_t SQL_BOOLEAN   = 32764;
    constexpr uint16_t SQL_NULL      = 32766;
}

// Database parameter buffer (DPB) items
namespace DpbItem {
    constexpr uint8_t isc_dpb_version1        = 1;
    constexpr uint8_t isc_dpb_version2        = 2;
    constexpr uint8_t isc_dpb_user_name       = 28;
    constexpr uint8_t isc_dpb_password        = 29;
    constexpr uint8_t isc_dpb_password_enc    = 30;
    constexpr uint8_t isc_dpb_sql_dialect     = 63;
    constexpr uint8_t isc_dpb_lc_ctype        = 48;
    constexpr uint8_t isc_dpb_process_id      = 71;
    constexpr uint8_t isc_dpb_process_name    = 72;
    constexpr uint8_t isc_dpb_utf8_filename   = 77;
    constexpr uint8_t isc_dpb_client_version  = 80;
    constexpr uint8_t isc_dpb_specific_auth_data = 84;
    constexpr uint8_t isc_dpb_auth_plugin_list = 85;
    constexpr uint8_t isc_dpb_auth_plugin_name = 86;
    constexpr uint8_t isc_dpb_config          = 87;
    constexpr uint8_t isc_dpb_session_time_zone = 91;
}

// Service parameter buffer (SPB) items
namespace SpbItem {
    constexpr uint8_t isc_spb_version1        = 1;
    constexpr uint8_t isc_spb_version2        = 2;
    constexpr uint8_t isc_spb_user_name       = 28;
    constexpr uint8_t isc_spb_password        = 29;
    constexpr uint8_t isc_spb_password_enc    = 30;
    constexpr uint8_t isc_spb_trusted_auth    = 31;
    constexpr uint8_t isc_spb_auth_plugin_list = 42;
    constexpr uint8_t isc_spb_auth_plugin_name = 43;
    
    // Service actions
    constexpr uint8_t isc_action_svc_db_stats = 5;
    constexpr uint8_t isc_action_svc_backup   = 6;
    constexpr uint8_t isc_action_svc_restore  = 7;
    constexpr uint8_t isc_action_svc_repair   = 8;
    constexpr uint8_t isc_action_svc_add_user = 9;
    constexpr uint8_t isc_action_svc_delete_user = 10;
    constexpr uint8_t isc_action_svc_modify_user = 11;
    constexpr uint8_t isc_action_svc_display_user = 12;
    constexpr uint8_t isc_action_svc_properties = 13;
    constexpr uint8_t isc_action_svc_license = 14;
    constexpr uint8_t isc_action_svc_add_license = 15;
    constexpr uint8_t isc_action_svc_remove_license = 16;
    constexpr uint8_t isc_action_svc_db_check = 17;
    constexpr uint8_t isc_action_svc_get_db_log = 18;
    constexpr uint8_t isc_action_svc_get_fb_log = 19;
    constexpr uint8_t isc_action_svc_nbak     = 20;
    constexpr uint8_t isc_action_svc_nrest    = 21;
    constexpr uint8_t isc_action_svc_trace_start = 22;
    constexpr uint8_t isc_action_svc_trace_stop = 23;
    constexpr uint8_t isc_action_svc_trace_suspend = 24;
    constexpr uint8_t isc_action_svc_trace_resume = 25;
    constexpr uint8_t isc_action_svc_trace_list = 26;
    constexpr uint8_t isc_action_svc_set_mapping = 27;
    constexpr uint8_t isc_action_svc_drop_mapping = 28;
    constexpr uint8_t isc_action_svc_display_user_adm = 29;
    constexpr uint8_t isc_action_svc_validate = 30;
    
    // Service info items
    constexpr uint8_t isc_info_svc_version    = 1;
    constexpr uint8_t isc_info_svc_server_version = 2;
    constexpr uint8_t isc_info_svc_implementation = 3;
    constexpr uint8_t isc_info_svc_capabilities = 4;
    constexpr uint8_t isc_info_svc_user_dbpath = 5;
    constexpr uint8_t isc_info_svc_get_env    = 6;
    constexpr uint8_t isc_info_svc_get_env_lock = 7;
    constexpr uint8_t isc_info_svc_get_env_msg = 8;
    constexpr uint8_t isc_info_svc_line       = 9;
    constexpr uint8_t isc_info_svc_to_eof     = 10;
    constexpr uint8_t isc_info_svc_timeout    = 11;
    constexpr uint8_t isc_info_svc_get_licensed_users = 12;
    constexpr uint8_t isc_info_svc_limbo_trans = 13;
    constexpr uint8_t isc_info_svc_running    = 14;
    constexpr uint8_t isc_info_svc_get_users  = 15;
    constexpr uint8_t isc_info_svc_stdin      = 16;
}

// Transaction parameter buffer (TPB) items
namespace TpbItem {
    constexpr uint8_t isc_tpb_version3        = 3;
    constexpr uint8_t isc_tpb_consistency     = 1;
    constexpr uint8_t isc_tpb_concurrency     = 2;
    constexpr uint8_t isc_tpb_shared          = 3;
    constexpr uint8_t isc_tpb_protected       = 4;
    constexpr uint8_t isc_tpb_exclusive       = 5;
    constexpr uint8_t isc_tpb_wait            = 6;
    constexpr uint8_t isc_tpb_nowait          = 7;
    constexpr uint8_t isc_tpb_read            = 8;
    constexpr uint8_t isc_tpb_write           = 9;
    constexpr uint8_t isc_tpb_lock_read       = 10;
    constexpr uint8_t isc_tpb_lock_write      = 11;
    constexpr uint8_t isc_tpb_verb_time       = 12;
    constexpr uint8_t isc_tpb_commit_time     = 13;
    constexpr uint8_t isc_tpb_ignore_limbo    = 14;
    constexpr uint8_t isc_tpb_read_committed  = 15;
    constexpr uint8_t isc_tpb_autocommit      = 16;
    constexpr uint8_t isc_tpb_rec_version     = 17;
    constexpr uint8_t isc_tpb_no_rec_version  = 18;
    constexpr uint8_t isc_tpb_restart_requests = 19;
    constexpr uint8_t isc_tpb_no_auto_undo    = 20;
    constexpr uint8_t isc_tpb_read_consistency = 21;
}

// Error codes
namespace ErrorCode {
    constexpr int32_t isc_arg_gds           = 1;
    constexpr int32_t isc_arg_string        = 2;
    constexpr int32_t isc_arg_cstring       = 3;
    constexpr int32_t isc_arg_number        = 4;
    constexpr int32_t isc_arg_interpreted   = 5;
    constexpr int32_t isc_arg_unix          = 7;
    constexpr int32_t isc_arg_next_mach     = 15;
    constexpr int32_t isc_arg_win32         = 17;
    constexpr int32_t isc_arg_warning       = 18;
    constexpr int32_t isc_arg_sql_state     = 19;
    constexpr int32_t isc_arg_end           = 0;

    // Common error codes
    constexpr int32_t isc_unavailable       = 335544375;
    constexpr int32_t isc_bad_db_handle     = 335544324;
    constexpr int32_t isc_bad_tr_handle     = 335544327;
    constexpr int32_t isc_bad_stmt_handle   = 335544485;
    constexpr int32_t isc_dsql_error        = 335544569;
    constexpr int32_t isc_sqlerr            = 335544436;
    constexpr int32_t isc_login             = 335544472;
}

// Authentication plugin names
constexpr const char* AUTH_PLUGIN_SRP        = "Srp";
constexpr const char* AUTH_PLUGIN_SRP256     = "Srp256";
constexpr const char* AUTH_PLUGIN_LEGACY     = "Legacy_Auth";

// Info end marker
constexpr uint8_t isc_info_end = 1;

// SQL info items
namespace SqlInfo {
    constexpr uint8_t isc_info_sql_stmt_type = 21;
    constexpr uint8_t isc_info_sql_get_plan = 22;
    constexpr uint8_t isc_info_sql_records = 23;
    constexpr uint8_t isc_info_sql_batch_fetch = 24;
    constexpr uint8_t isc_info_sql_sqlda_start = 25;
    constexpr uint8_t isc_info_sql_sqlda_end = 26;
}

// SQL statement types
namespace SqlStmtType {
    constexpr uint8_t isc_info_sql_stmt_select = 1;
    constexpr uint8_t isc_info_sql_stmt_insert = 2;
    constexpr uint8_t isc_info_sql_stmt_update = 3;
    constexpr uint8_t isc_info_sql_stmt_delete = 4;
    constexpr uint8_t isc_info_sql_stmt_ddl = 5;
    constexpr uint8_t isc_info_sql_stmt_exec_procedure = 8;
}

} // namespace firebird

// ============================================================================
// Firebird Protocol State
// ============================================================================

enum class FirebirdProtocolState {
    INITIAL,              // Waiting for op_connect
    CONNECT_RECEIVED,     // Connect received, negotiating
    AUTH_CONTINUE,        // Authentication in progress
    AUTHENTICATED,        // Authentication complete
    ATTACHED,             // Database attached
    READY,                // Ready for operations
    CLOSING,              // Connection closing
    ERROR                 // Protocol error
};

// ============================================================================
// Firebird Handle Types
// ============================================================================

struct FirebirdStatement {
    uint32_t handle;
    std::string query;
    uint32_t type;
    std::vector<ProtocolCodec::ColumnInfo> columns;
    std::vector<uint8_t> input_blr;
    std::vector<uint8_t> output_blr;
    struct BlrField {
        uint8_t dtype = 0;
        int16_t scale = 0;
        uint16_t length = 0;
        bool is_text = false;
        bool is_varying = false;
        bool is_numeric() const { return dtype == 7 || dtype == 8 || dtype == 16; }
    };
    std::vector<BlrField> output_fields;
    std::vector<BlrField> input_fields;
    uint32_t output_message_length = 0;
    uint32_t input_message_length = 0;
    bool prepared = false;
    std::vector<std::vector<uint8_t>> row_buffers;  // Serialized rows for fetch
    size_t fetch_pos = 0;
};

// ============================================================================
// Firebird Protocol Adapter
// ============================================================================

class FirebirdAdapter : public ProtocolAdapter {
public:
    explicit FirebirdAdapter(const ProtocolAdapterConfig& config = ProtocolAdapterConfig());
    ~FirebirdAdapter() override;

    // ========================================================================
    // ProtocolHandler Interface
    // ========================================================================

    network::ProtocolType getProtocolType() const override {
        return network::ProtocolType::FIREBIRD;
    }

    // ========================================================================
    // Configuration
    // ========================================================================

    void setServerVersion(const std::string& version) { server_version_ = version; }
    const std::string& getServerVersion() const { return server_version_; }
    void setRemoteCredentials(const std::string& username, const std::string& password);

protected:
    // Execute against native server over IPC (bridge path)
    core::Status executeRemoteQuery(const QueryContext& query,
                                    ResultContext& result,
                                    core::ErrorContext* ctx = nullptr);
    core::Status ensureRemoteClient(core::ErrorContext* ctx);

protected:
    // ========================================================================
    // ProtocolAdapter Implementation
    // ========================================================================

    core::Status parseMessage(network::Connection* conn) override;
    core::Status processMessage(network::Connection* conn) override;
    core::Status sendGreeting(network::Connection* conn) override;
    core::Status processAuthentication(network::Connection* conn) override;
    core::Status sendAuthResult(network::Connection* conn,
                                bool success,
                                const std::string& error_msg = "") override;
    core::Status sendQueryResult(network::Connection* conn,
                                 const ResultContext& result) override;
    core::Status sendProtocolError(network::Connection* conn,
                                   uint32_t error_code,
                                   const std::string& sqlstate,
                                   const std::string& message,
                                   const std::string& detail = "",
                                   const std::string& hint = "") override;
    core::Status compileQuery(const std::string& sql,
                              std::vector<uint8_t>& bytecode_out,
                              std::string& error_out) override;

private:
    // ========================================================================
    // Operation Handling
    // ========================================================================

    core::Status handleConnect(network::Connection* conn);
    core::Status handleAttach(network::Connection* conn);
    core::Status handleDetach(network::Connection* conn);
    core::Status handleCreateDatabase(network::Connection* conn);
    core::Status handleDropDatabase(network::Connection* conn);

    core::Status handleTransaction(network::Connection* conn);
    core::Status handleCommit(network::Connection* conn);
    core::Status handleRollback(network::Connection* conn);
    core::Status handleCommitRetaining(network::Connection* conn);
    core::Status handleRollbackRetaining(network::Connection* conn);

    core::Status handleAllocateStatement(network::Connection* conn);
    core::Status handlePrepareStatement(network::Connection* conn);
    core::Status handleExecute(network::Connection* conn);
    core::Status handleExecute2(network::Connection* conn);
    core::Status handleExecImmediate(network::Connection* conn);
    core::Status handleExecImmediate2(network::Connection* conn);
    core::Status handleFetch(network::Connection* conn);
    core::Status handleFreeStatement(network::Connection* conn);
    core::Status handleSetCursor(network::Connection* conn);

    core::Status handleInfoDatabase(network::Connection* conn);
    core::Status handleInfoTransaction(network::Connection* conn);
    core::Status handleInfoSql(network::Connection* conn);

    // Service Manager operations (C5.1)
    core::Status handleServiceAttach(network::Connection* conn);
    core::Status handleServiceDetach(network::Connection* conn);
    core::Status handleServiceInfo(network::Connection* conn);
    core::Status handleServiceStart(network::Connection* conn);

    // Event operations (C5.2)
    core::Status handleQueEvents(network::Connection* conn);
    core::Status handleCancelEvents(network::Connection* conn);

    // BLOB operations (C5.3)
    core::Status handleCreateBlob(network::Connection* conn);
    core::Status handleCreateBlob2(network::Connection* conn);
    core::Status handleOpenBlob(network::Connection* conn);
    core::Status handleOpenBlob2(network::Connection* conn);
    core::Status handleCloseBlob(network::Connection* conn);
    core::Status handleCancelBlob(network::Connection* conn);
    core::Status handleGetSegment(network::Connection* conn);
    core::Status handlePutSegment(network::Connection* conn);
    core::Status handleSeekBlob(network::Connection* conn);

    core::Status handleContAuth(network::Connection* conn);
    core::Status handlePing(network::Connection* conn);
    core::Status handleCancel(network::Connection* conn);
    core::Status handleDisconnect(network::Connection* conn);

    // ========================================================================
    // Response Sending
    // ========================================================================

    void sendAccept(network::Connection* conn, uint32_t version, uint32_t arch, uint32_t type);
    void sendAcceptData(network::Connection* conn, uint32_t version, uint32_t arch,
                        uint32_t type, const std::vector<uint8_t>& data,
                        const std::string& plugin, bool authenticated,
                        const std::vector<uint8_t>& keys);
    void sendResponse(network::Connection* conn, uint32_t handle, uint64_t object_id,
                      const std::vector<uint8_t>& data);
    void sendFetchResponse(network::Connection* conn, uint32_t status, uint32_t count,
                           const std::vector<std::vector<uint8_t>>& rows);
    void sendSqlResponse(network::Connection* conn, uint32_t count);
    void sendErrorResponse(network::Connection* conn, int32_t error_code,
                           const std::string& message,
                           const std::string& sqlstate = "");

    // ========================================================================
    // Helper Methods
    // ========================================================================

    // Ensure Firebird-specific catalog objects exist in the local engine
    core::Status ensureFirebirdSystemTables(core::ErrorContext* ctx);

    // Packet I/O
    void sendPacket(network::Connection* conn, uint32_t opcode,
                    const std::vector<uint8_t>& data);

    // Integer encoding (XDR - big-endian)
    void writeInt32(std::vector<uint8_t>& buf, int32_t value);
    void writeUInt32(std::vector<uint8_t>& buf, uint32_t value);
    void writeInt64(std::vector<uint8_t>& buf, int64_t value);
    void writeBuffer(std::vector<uint8_t>& buf, const void* data, size_t len);
    void writeString(std::vector<uint8_t>& buf, const std::string& str);
    void writePaddedString(std::vector<uint8_t>& buf, const std::string& str);

    int32_t readInt32(const uint8_t* data);
    uint32_t readUInt32(const uint8_t* data);
    int64_t readInt64(const uint8_t* data);
    std::string readString(const uint8_t* data, size_t& offset, size_t max_len);
    std::vector<uint8_t> readBuffer(const uint8_t* data, size_t& offset, size_t max_len);

    // Type conversion
    uint16_t wireTypeToFirebirdType(WireType type);
    WireType firebirdTypeToWireType(uint16_t type);

    // DPB/TPB parsing
    void parseDpb(const std::vector<uint8_t>& dpb);
    std::vector<uint8_t> buildDefaultTpb();

    // ========================================================================
    // State
    // ========================================================================
    core::ID firebird_schema_id_;
    std::string firebird_schema_name_;
    std::unordered_set<uint32_t> active_transactions_;

    FirebirdProtocolState fb_state_ = FirebirdProtocolState::INITIAL;

    // Current packet
    uint32_t current_opcode_ = 0;
    std::vector<uint8_t> current_packet_;

    // Server info
    std::string server_version_ = "ScratchBird/5.0.0";  // Firebird 5 compatible
    uint32_t protocol_version_ = firebird::DEFAULT_PROTOCOL_VERSION;
    uint32_t client_protocol_version_ = 0;

    // Handles
    uint32_t db_handle_ = 0;
    uint32_t next_db_handle_ = 1;
    uint32_t next_tr_handle_ = 1;
    uint32_t next_stmt_handle_ = 1;

    // Active transaction
    uint32_t current_transaction_ = 0;

    // Statements
    std::unordered_map<uint32_t, FirebirdStatement> statements_;

    // Authentication state
    std::string auth_plugin_name_;
    std::vector<uint8_t> auth_data_;
    bool auth_complete_ = false;

    // Client info from attach
    uint8_t sql_dialect_ = 3;
    std::string client_charset_ = "UTF8";
    std::string remote_password_;

    // Remote engine client (IPC to native ScratchBird server)
    client::ConnectionConfig client_config_;
    std::unique_ptr<client::Connection> client_;

    // Service Manager state (C5.1)
    struct ServiceState {
        uint32_t handle = 0;
        std::string username;
        uint8_t action = 0;  // Current service action
        bool active = false;
    };
    ServiceState service_state_;
    
    // Event state (C5.2)
    struct EventState {
        uint32_t event_id = 0;
        std::vector<std::string> event_names;
        bool active = false;
    };
    EventState event_state_;
    uint32_t next_event_id_ = 1;
    
    // BLOB state (C5.3)
    struct BlobState {
        uint64_t blob_id = 0;
        std::vector<uint8_t> data;
        size_t position = 0;
        bool is_segmented = true;
        bool is_new = false;
    };
    std::unordered_map<uint64_t, BlobState> blobs_;
    uint64_t next_blob_id_ = 1;
    
    // Helper methods
    void parseSpb(const std::vector<uint8_t>& spb, ServiceState& state);
    void sendServiceResponse(network::Connection* conn, const std::vector<uint8_t>& data);
};

} // namespace protocol
} // namespace scratchbird
