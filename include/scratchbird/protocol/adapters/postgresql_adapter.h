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
 * PostgreSQL Wire Protocol Adapter
 *
 * ScratchBird Network Layer - Phase 3.2
 *
 * Implements PostgreSQL v3 wire protocol for client compatibility.
 * Supports Simple Query and Extended Query protocols.
 *
 * Reference: https://www.postgresql.org/docs/current/protocol.html
 */

#pragma once

#include "scratchbird/protocol/adapters/protocol_adapter.h"
#include "scratchbird/client/connection.h"
#include "scratchbird/server/ipc_server.h"
#include "scratchbird/security/tls_config.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <deque>

namespace scratchbird {
namespace protocol {

// ============================================================================
// PostgreSQL Protocol Constants
// ============================================================================

namespace pg {

// Protocol version (3.0)
constexpr int32_t PROTOCOL_VERSION_3 = 196608;  // (3 << 16)

// SSL request code
constexpr int32_t SSL_REQUEST = 80877103;
// GSS encryption request code
constexpr int32_t GSSENC_REQUEST = 80877104;

// Cancel request code
constexpr int32_t CANCEL_REQUEST = 80877102;

// Message types (frontend -> backend)
namespace FrontendMsg {
    constexpr char BIND = 'B';
    constexpr char CLOSE = 'C';
    constexpr char COPY_DATA = 'd';
    constexpr char COPY_DONE = 'c';
    constexpr char COPY_FAIL = 'f';
    constexpr char DESCRIBE = 'D';
    constexpr char EXECUTE = 'E';
    constexpr char FLUSH = 'H';
    constexpr char FUNCTION_CALL = 'F';
    constexpr char GSS_RESPONSE = 'p';
    constexpr char PARSE = 'P';
    constexpr char PASSWORD = 'p';
    constexpr char QUERY = 'Q';
    constexpr char SASL_INITIAL = 'p';
    constexpr char SASL_RESPONSE = 'p';
    constexpr char SYNC = 'S';
    constexpr char TERMINATE = 'X';
}

// Message types (backend -> frontend)
namespace BackendMsg {
    constexpr char AUTHENTICATION = 'R';
    constexpr char BACKEND_KEY_DATA = 'K';
    constexpr char BIND_COMPLETE = '2';
    constexpr char CLOSE_COMPLETE = '3';
    constexpr char COMMAND_COMPLETE = 'C';
    constexpr char COPY_DATA = 'd';
    constexpr char COPY_DONE = 'c';
    constexpr char COPY_IN_RESPONSE = 'G';
    constexpr char COPY_OUT_RESPONSE = 'H';
    constexpr char COPY_BOTH_RESPONSE = 'W';
    constexpr char DATA_ROW = 'D';
    constexpr char EMPTY_QUERY_RESPONSE = 'I';
    constexpr char ERROR_RESPONSE = 'E';
    constexpr char FUNCTION_CALL_RESPONSE = 'V';
    constexpr char NEGOTIATE_PROTOCOL_VERSION = 'v';
    constexpr char NO_DATA = 'n';
    constexpr char NOTICE_RESPONSE = 'N';
    constexpr char NOTIFICATION_RESPONSE = 'A';
    constexpr char PARAMETER_DESCRIPTION = 't';
    constexpr char PARAMETER_STATUS = 'S';
    constexpr char PARSE_COMPLETE = '1';
    constexpr char PORTAL_SUSPENDED = 's';
    constexpr char READY_FOR_QUERY = 'Z';
    constexpr char ROW_DESCRIPTION = 'T';
}

// Authentication types
namespace AuthType {
    constexpr int32_t OK = 0;
    constexpr int32_t KERBEROS_V5 = 2;
    constexpr int32_t CLEARTEXT_PASSWORD = 3;
    constexpr int32_t MD5_PASSWORD = 5;
    constexpr int32_t SCM_CREDENTIAL = 6;
    constexpr int32_t GSS = 7;
    constexpr int32_t GSS_CONTINUE = 8;
    constexpr int32_t SSPI = 9;
    constexpr int32_t SASL = 10;
    constexpr int32_t SASL_CONTINUE = 11;
    constexpr int32_t SASL_FINAL = 12;
}

// Error/Notice field codes
namespace ErrorField {
    constexpr char SEVERITY = 'S';
    constexpr char SEVERITY_V = 'V';  // Non-localized severity
    constexpr char CODE = 'C';         // SQLSTATE
    constexpr char MESSAGE = 'M';
    constexpr char DETAIL = 'D';
    constexpr char HINT = 'H';
    constexpr char POSITION = 'P';
    constexpr char INTERNAL_POSITION = 'p';
    constexpr char INTERNAL_QUERY = 'q';
    constexpr char WHERE = 'W';
    constexpr char SCHEMA = 's';
    constexpr char TABLE = 't';
    constexpr char COLUMN = 'c';
    constexpr char DATATYPE = 'd';
    constexpr char CONSTRAINT = 'n';
    constexpr char FILE = 'F';
    constexpr char LINE = 'L';
    constexpr char ROUTINE = 'R';
}

// Transaction states for ReadyForQuery
namespace TransactionStatus {
    constexpr char IDLE = 'I';
    constexpr char IN_TRANSACTION = 'T';
    constexpr char FAILED = 'E';
}

// Common OIDs for type mapping
namespace TypeOid {
    constexpr int32_t BOOL = 16;
    constexpr int32_t BYTEA = 17;
    constexpr int32_t CHAR = 18;
    constexpr int32_t INT8 = 20;
    constexpr int32_t INT2 = 21;
    constexpr int32_t INT4 = 23;
    constexpr int32_t TEXT = 25;
    constexpr int32_t OID = 26;
    constexpr int32_t FLOAT4 = 700;
    constexpr int32_t FLOAT8 = 701;
    constexpr int32_t VARCHAR = 1043;
    constexpr int32_t DATE = 1082;
    constexpr int32_t TIME = 1083;
    constexpr int32_t TIMESTAMP = 1114;
    constexpr int32_t TIMESTAMPTZ = 1184;
    constexpr int32_t INTERVAL = 1186;
    constexpr int32_t NUMERIC = 1700;
    constexpr int32_t UUID = 2950;
    constexpr int32_t JSON = 114;
    constexpr int32_t JSONB = 3802;
}

} // namespace pg

// ============================================================================
// PostgreSQL Protocol State
// ============================================================================

/**
 * PostgreSQL-specific protocol state
 */
enum class PgProtocolState {
    STARTUP,            // Waiting for startup message
    SSL_REQUEST,        // Received SSL request
    AUTH_REQUESTED,     // Sent authentication request
    AUTH_MD5,           // MD5 auth in progress
    AUTH_SCRAM,         // SCRAM auth in progress
    AUTHENTICATED,      // Authentication complete
    READY,              // Ready for queries
    SIMPLE_QUERY,       // Processing simple query
    EXTENDED_PARSE,     // Extended: parsing
    EXTENDED_BIND,      // Extended: binding
    EXTENDED_DESCRIBE,  // Extended: describing
    EXTENDED_EXECUTE,   // Extended: executing
    COPY_IN,            // COPY IN mode
    COPY_OUT,           // COPY OUT mode
    CLOSING,            // Connection closing
    ERROR               // Protocol error
};

// ============================================================================
// PostgreSQL Prepared Statement Info
// ============================================================================

/**
 * Prepared statement metadata
 */
struct PgPreparedStatement {
    std::string name;
    std::string query;
    std::vector<int32_t> param_types;   // OIDs
    std::vector<ProtocolCodec::ColumnInfo> result_columns;
    bool described = false;
};

/**
 * Portal (bound statement) metadata
 */
struct PgPortal {
    std::string name;
    std::string statement_name;
    std::vector<std::string> param_values;
    std::vector<bool> param_nulls;
    std::vector<int16_t> param_formats;
    std::vector<int16_t> result_formats;  // 0 = text, 1 = binary
    int32_t max_rows = 0;  // 0 = unlimited
    bool executed = false;
    bool completed = false;
    bool more_rows_available = false;
    std::string command_tag;
    size_t fetch_pos = 0;
    std::vector<std::vector<ProtocolCodec::ColumnValue>> buffered_rows;
};

// ============================================================================
// PostgreSQL Protocol Adapter
// ============================================================================

/**
 * PostgreSQL wire protocol adapter
 *
 * Implements PostgreSQL v3 protocol including:
 * - Simple Query protocol
 * - Extended Query protocol (Parse, Bind, Describe, Execute, Sync)
 * - Cleartext password authentication (engine delegated; MD5 pending)
 * - COPY protocol (basic support)
 *
 * Future:
 * - SCRAM-SHA-256 authentication
 * - SSL/TLS support
 * - Streaming replication protocol
 */
class PostgresqlAdapter : public ProtocolAdapter {
public:
    explicit PostgresqlAdapter(const ProtocolAdapterConfig& config = ProtocolAdapterConfig());
    ~PostgresqlAdapter() override;

    // ========================================================================
    // ProtocolHandler Interface
    // ========================================================================

    network::ProtocolType getProtocolType() const override {
        return network::ProtocolType::POSTGRESQL;
    }

    // ========================================================================
    // Configuration
    // ========================================================================

    /**
     * Set server parameters to send on connection
     */
    void setServerParameter(const std::string& name, const std::string& value);

    /**
     * Get server parameters
     */
    const std::unordered_map<std::string, std::string>& getServerParameters() const {
        return server_parameters_;
    }

    /**
     * Configure TLS/SSL for this adapter instance
     */
    void setTLSConfig(const security::TLSConfig& config);

    /**
     * Check if TLS is enabled for this adapter
     */
    bool isTLSEnabled() const { return tls_enabled_; }

    /**
     * Check if TLS has been negotiated
     */
    bool isTLSNegotiated() const { return tls_negotiated_; }

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

    void recordCopyMetrics(const std::string& direction,
                           uint64_t rows,
                           uint64_t bytes,
                           bool error,
                           const std::chrono::steady_clock::time_point& start_time);

protected:
    // ====================================================================
    // Testing Support (protected for test harness access)
    // ====================================================================

    // MD5 authentication helpers
    std::string computeMD5Hash(const std::string& password,
                               const std::string& username,
                               const uint8_t salt[4]);
    bool validateMD5Response(const std::string& response,
                             const std::string& expected_hash);

private:
    struct CopyOptions {
        enum class Format {
            TEXT,
            CSV
        };
        Format format = Format::TEXT;
        char delimiter = '\t';
        std::string null_string = "\\N";
        bool header = false;
    };

    struct CopyContext {
        bool active = false;
        bool from_stdin = false;
        bool to_stdout = false;
        bool from_extended = false;
        std::string table_name;
        std::vector<std::string> columns;
        std::string select_query;
        std::string buffer;
        size_t rows = 0;
        uint64_t bytes = 0;
        std::chrono::steady_clock::time_point start_time{};
        std::string portal_name;
        std::string statement_name;
        CopyOptions options;
    };

    // ========================================================================
    // Message Handling
    // ========================================================================

    // Startup phase
    core::Status handleStartupMessage(network::Connection* conn);
    core::Status handleSSLRequest(network::Connection* conn);
    core::Status handleGSSENCRequest(network::Connection* conn);
    core::Status handleCancelRequest(network::Connection* conn);

    // Authentication
    core::Status handlePasswordMessage(network::Connection* conn);

    // Simple Query
    core::Status handleQuery(network::Connection* conn);

    // Extended Query
    core::Status handleParse(network::Connection* conn);
    core::Status handleBind(network::Connection* conn);
    core::Status handleDescribe(network::Connection* conn);
    core::Status handleExecute(network::Connection* conn);
    core::Status handleFetch(network::Connection* conn);
    core::Status handleClose(network::Connection* conn);
    core::Status handleSync(network::Connection* conn);
    core::Status handleFlush(network::Connection* conn);

    // COPY
    core::Status handleCopyData(network::Connection* conn);
    core::Status handleCopyDone(network::Connection* conn);
    core::Status handleCopyFail(network::Connection* conn);

    // Termination
    core::Status handleTerminate(network::Connection* conn);

    // ========================================================================
    // Message Sending
    // ========================================================================

    // Authentication messages
    void sendAuthenticationOk(network::Connection* conn);
    void sendAuthenticationMD5Password(network::Connection* conn, const uint8_t salt[4]);
    void sendAuthenticationCleartextPassword(network::Connection* conn);
    void sendAuthenticationSASL(network::Connection* conn,
                                const std::vector<std::string>& mechanisms);
    void sendAuthenticationSASLContinue(network::Connection* conn, const std::string& data);
    void sendAuthenticationSASLFinal(network::Connection* conn, const std::string& data);

    // Startup messages
    void sendBackendKeyData(network::Connection* conn);
    void sendParameterStatus(network::Connection* conn,
                             const std::string& name,
                             const std::string& value);
    void sendReadyForQuery(network::Connection* conn);

    // Query result messages
    void sendRowDescription(network::Connection* conn,
                            const std::vector<ProtocolCodec::ColumnInfo>& columns,
                            const std::vector<int16_t>& formats = {});
    void sendDataRow(network::Connection* conn,
                     const std::vector<ProtocolCodec::ColumnInfo>& columns,
                     const std::vector<ProtocolCodec::ColumnValue>& values,
                     const std::vector<int16_t>& formats = {});
    void sendCommandComplete(network::Connection* conn, const std::string& tag);
    void sendEmptyQueryResponse(network::Connection* conn);

    // Extended query messages
    void sendParseComplete(network::Connection* conn);
    void sendBindComplete(network::Connection* conn);
    void sendCloseComplete(network::Connection* conn);
    void sendNoData(network::Connection* conn);
    void sendParameterDescription(network::Connection* conn,
                                  const std::vector<int32_t>& param_types);
    void sendPortalSuspended(network::Connection* conn);

    // Error/Notice messages
    void sendErrorResponse(network::Connection* conn,
                           const std::string& severity,
                           const std::string& sqlstate,
                           const std::string& message,
                           const std::string& detail = "",
                           const std::string& hint = "");
    void sendNoticeResponse(network::Connection* conn,
                            const std::string& severity,
                            const std::string& message);

    // COPY messages
    void sendCopyInResponse(network::Connection* conn,
                            int8_t overall_format,
                            const std::vector<int16_t>& column_formats);
    void sendCopyOutResponse(network::Connection* conn,
                             int8_t overall_format,
                             const std::vector<int16_t>& column_formats);
    void sendCopyData(network::Connection* conn, const void* data, size_t len);
    void sendCopyDone(network::Connection* conn);

    // ========================================================================
    // Helper Methods
    // ========================================================================

    // Buffer I/O
    void writeInt32(std::vector<uint8_t>& buf, int32_t value);
    void writeInt16(std::vector<uint8_t>& buf, int16_t value);
    void writeByte(std::vector<uint8_t>& buf, uint8_t value);
    void writeString(std::vector<uint8_t>& buf, const std::string& str);
    void writeBytes(std::vector<uint8_t>& buf, const void* data, size_t len);

    int32_t readInt32(const uint8_t* data);
    int16_t readInt16(const uint8_t* data);
    std::string readString(const uint8_t* data, size_t max_len);

    // Type conversion
    int32_t wireTypeToOid(WireType type);
    WireType oidToWireType(int32_t oid);
    void mapStatusToSqlstate(uint32_t status, std::string& sqlstate);
    void updateTransactionStatus(const std::string& sql, bool has_error);
    int16_t selectFormatForColumn(size_t idx,
                                  const std::vector<int16_t>& formats,
                                  WireType type);
    bool supportsBinaryFormat(WireType type);
    std::vector<uint8_t> encodeTextValue(const ProtocolCodec::ColumnValue& val,
                                         WireType type);
    std::vector<uint8_t> encodeBinaryValue(const ProtocolCodec::ColumnValue& val,
                                           WireType type,
                                           bool& ok);
    std::string decodeBinaryParamToText(WireType type,
                                        const uint8_t* data,
                                        size_t len);
    std::string substitutePositionalParameters(const QueryContext& query);
    std::string escapeLiteral(const std::string& input);
    bool parseCopyQuery(const std::string& sql, CopyContext& ctx, std::string& error);
    core::Status startCopyOut(network::Connection* conn, CopyContext& ctx);
    core::Status startCopyIn(network::Connection* conn, CopyContext& ctx);
    core::Status finishCopyIn(network::Connection* conn);

    // TLS/SSL handling
    core::Status performTLSHandshake(network::Connection* conn);
    core::Status sendSSLResponse(network::Connection* conn, bool accept);
    core::Status sendGSSENCResponse(network::Connection* conn, bool accept);

    // Ensure per-emulated PostgreSQL catalog schema/views exist
    core::Status ensurePostgresSystemCatalog(core::ErrorContext* ctx);

    // Initialize default server parameters (server_version, etc.)
    void initializeServerParameters();

    // Send message helper
    void sendMessage(network::Connection* conn, char type, const std::vector<uint8_t>& payload);

    // IPC bridge helpers
    core::Status connectRemoteClient(core::ErrorContext* ctx = nullptr);
    core::Status ensureRemoteClient(core::ErrorContext* ctx = nullptr);
    core::Status executeRemoteQuery(const QueryContext& query,
                                    ResultContext& result,
                                    core::ErrorContext* ctx = nullptr);

    void registerBackend();
    void unregisterBackend();
    void requestCancel();
    static PostgresqlAdapter* findBackend(int32_t pid, int32_t key);

    // ========================================================================
    // State
    // ========================================================================

    PgProtocolState pg_state_ = PgProtocolState::STARTUP;

    // Current message being parsed
    char current_msg_type_ = 0;
    int32_t current_msg_length_ = 0;
    std::vector<uint8_t> current_msg_data_;

    // Backend key data (for cancel requests)
    int32_t backend_pid_ = 0;
    int32_t backend_secret_key_ = 0;
    std::atomic<bool> cancel_requested_{false};

    struct BackendEntry {
        PostgresqlAdapter* adapter = nullptr;
        int32_t secret_key = 0;
    };
    static std::mutex backend_registry_mutex_;
    static std::unordered_map<int32_t, BackendEntry> backend_registry_;

    // MD5 auth salt
    uint8_t md5_salt_[4] = {0};
    uint8_t scram_step_ = 0;
    AuthMethod auth_method_ = AuthMethod::PASSWORD;

    // TLS support for PostgreSQL SSLRequest
    std::unique_ptr<security::TLSContext> tls_context_;
    std::unique_ptr<security::TLSConnection> tls_connection_;
    bool tls_enabled_ = false;
    bool tls_negotiated_ = false;

    // Startup parameters from client
    std::unordered_map<std::string, std::string> client_parameters_;

    // Server parameters to send
    std::unordered_map<std::string, std::string> server_parameters_;

    // Emulated schema for PostgreSQL catalog
    core::ID pg_schema_id_;

    // Prepared statements
    std::unordered_map<std::string, PgPreparedStatement> statements_;

    // Portals (bound statements)
    std::unordered_map<std::string, PgPortal> portals_;

    // Extended query state
    bool sync_pending_ = false;
    std::deque<char> pending_operations_;  // Queue of operations before Sync
    CopyContext copy_context_;

    // IPC client (bridge to engine)
    std::unique_ptr<client::Connection> client_;
    client::ConnectionConfig client_config_;
    bool search_path_set_ = false;
    bool txn_failed_ = false;
};

} // namespace protocol
} // namespace scratchbird
