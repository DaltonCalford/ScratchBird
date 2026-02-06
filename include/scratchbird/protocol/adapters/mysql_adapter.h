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
 * MySQL Wire Protocol Adapter
 *
 * ScratchBird Network Layer - Phase 3.2
 *
 * Implements MySQL wire protocol for client compatibility.
 * Supports MySQL 5.7+ protocol with native password authentication.
 *
 * Reference: https://dev.mysql.com/doc/dev/mysql-server/latest/PAGE_PROTOCOL.html
 */

#pragma once

#include "scratchbird/protocol/adapters/protocol_adapter.h"
#include "scratchbird/client/connection.h"
#include "scratchbird/server/ipc_server.h"
#include "scratchbird/security/tls_config.h"

#include <unordered_map>
#include <deque>
#include <chrono>

namespace scratchbird {
namespace protocol {

// ============================================================================
// MySQL Protocol Constants
// ============================================================================

namespace mysql {

// Protocol version
constexpr uint8_t PROTOCOL_VERSION = 10;  // MySQL 4.1+

// Server capabilities (lower 16 bits)
namespace Capability {
    constexpr uint32_t LONG_PASSWORD           = 0x00000001;
    constexpr uint32_t FOUND_ROWS              = 0x00000002;
    constexpr uint32_t LONG_FLAG               = 0x00000004;
    constexpr uint32_t CONNECT_WITH_DB         = 0x00000008;
    constexpr uint32_t NO_SCHEMA               = 0x00000010;
    constexpr uint32_t COMPRESS                = 0x00000020;
    constexpr uint32_t ODBC                    = 0x00000040;
    constexpr uint32_t LOCAL_FILES             = 0x00000080;
    constexpr uint32_t IGNORE_SPACE            = 0x00000100;
    constexpr uint32_t PROTOCOL_41             = 0x00000200;
    constexpr uint32_t INTERACTIVE             = 0x00000400;
    constexpr uint32_t SSL                     = 0x00000800;
    constexpr uint32_t IGNORE_SIGPIPE          = 0x00001000;
    constexpr uint32_t TRANSACTIONS            = 0x00002000;
    constexpr uint32_t RESERVED                = 0x00004000;
    constexpr uint32_t SECURE_CONNECTION       = 0x00008000;
    // Extended capabilities (upper 16 bits)
    constexpr uint32_t MULTI_STATEMENTS        = 0x00010000;
    constexpr uint32_t MULTI_RESULTS           = 0x00020000;
    constexpr uint32_t PS_MULTI_RESULTS        = 0x00040000;
    constexpr uint32_t PLUGIN_AUTH             = 0x00080000;
    constexpr uint32_t CONNECT_ATTRS           = 0x00100000;
    constexpr uint32_t PLUGIN_AUTH_LENENC_DATA = 0x00200000;
    constexpr uint32_t CAN_HANDLE_EXPIRED_PW   = 0x00400000;
    constexpr uint32_t SESSION_TRACK           = 0x00800000;
    constexpr uint32_t DEPRECATE_EOF           = 0x01000000;
}

// Server status flags
namespace ServerStatus {
    constexpr uint16_t IN_TRANS             = 0x0001;
    constexpr uint16_t AUTOCOMMIT           = 0x0002;
    constexpr uint16_t MORE_RESULTS_EXISTS  = 0x0008;
    constexpr uint16_t NO_GOOD_INDEX_USED   = 0x0010;
    constexpr uint16_t NO_INDEX_USED        = 0x0020;
    constexpr uint16_t CURSOR_EXISTS        = 0x0040;
    constexpr uint16_t LAST_ROW_SENT        = 0x0080;
    constexpr uint16_t DB_DROPPED           = 0x0100;
    constexpr uint16_t NO_BACKSLASH_ESCAPES = 0x0200;
    constexpr uint16_t METADATA_CHANGED     = 0x0400;
    constexpr uint16_t QUERY_WAS_SLOW       = 0x0800;
    constexpr uint16_t PS_OUT_PARAMS        = 0x1000;
    constexpr uint16_t IN_TRANS_READONLY    = 0x2000;
    constexpr uint16_t SESSION_STATE_CHANGED = 0x4000;
}

// Command types
namespace Command {
    constexpr uint8_t COM_SLEEP              = 0x00;
    constexpr uint8_t COM_QUIT               = 0x01;
    constexpr uint8_t COM_INIT_DB            = 0x02;
    constexpr uint8_t COM_QUERY              = 0x03;
    constexpr uint8_t COM_FIELD_LIST         = 0x04;
    constexpr uint8_t COM_CREATE_DB          = 0x05;
    constexpr uint8_t COM_DROP_DB            = 0x06;
    constexpr uint8_t COM_REFRESH            = 0x07;
    constexpr uint8_t COM_SHUTDOWN           = 0x08;
    constexpr uint8_t COM_STATISTICS         = 0x09;
    constexpr uint8_t COM_PROCESS_INFO       = 0x0a;
    constexpr uint8_t COM_CONNECT            = 0x0b;
    constexpr uint8_t COM_PROCESS_KILL       = 0x0c;
    constexpr uint8_t COM_DEBUG              = 0x0d;
    constexpr uint8_t COM_PING               = 0x0e;
    constexpr uint8_t COM_TIME               = 0x0f;
    constexpr uint8_t COM_DELAYED_INSERT     = 0x10;
    constexpr uint8_t COM_CHANGE_USER        = 0x11;
    constexpr uint8_t COM_BINLOG_DUMP        = 0x12;
    constexpr uint8_t COM_TABLE_DUMP         = 0x13;
    constexpr uint8_t COM_CONNECT_OUT        = 0x14;
    constexpr uint8_t COM_REGISTER_SLAVE     = 0x15;
    constexpr uint8_t COM_STMT_PREPARE       = 0x16;
    constexpr uint8_t COM_STMT_EXECUTE       = 0x17;
    constexpr uint8_t COM_STMT_SEND_LONG_DATA = 0x18;
    constexpr uint8_t COM_STMT_CLOSE         = 0x19;
    constexpr uint8_t COM_STMT_RESET         = 0x1a;
    constexpr uint8_t COM_SET_OPTION         = 0x1b;
    constexpr uint8_t COM_STMT_FETCH         = 0x1c;
    constexpr uint8_t COM_DAEMON             = 0x1d;
    constexpr uint8_t COM_BINLOG_DUMP_GTID   = 0x1e;
    constexpr uint8_t COM_RESET_CONNECTION   = 0x1f;
}

// Column types
namespace FieldType {
    constexpr uint8_t DECIMAL     = 0x00;
    constexpr uint8_t TINY        = 0x01;
    constexpr uint8_t SHORT       = 0x02;
    constexpr uint8_t LONG        = 0x03;
    constexpr uint8_t FLOAT       = 0x04;
    constexpr uint8_t DOUBLE      = 0x05;
    constexpr uint8_t NULL_TYPE   = 0x06;
    constexpr uint8_t TIMESTAMP   = 0x07;
    constexpr uint8_t LONGLONG    = 0x08;
    constexpr uint8_t INT24       = 0x09;
    constexpr uint8_t DATE        = 0x0a;
    constexpr uint8_t TIME        = 0x0b;
    constexpr uint8_t DATETIME    = 0x0c;
    constexpr uint8_t YEAR        = 0x0d;
    constexpr uint8_t NEWDATE     = 0x0e;
    constexpr uint8_t VARCHAR     = 0x0f;
    constexpr uint8_t BIT         = 0x10;
    constexpr uint8_t TIMESTAMP2  = 0x11;
    constexpr uint8_t DATETIME2   = 0x12;
    constexpr uint8_t TIME2       = 0x13;
    constexpr uint8_t JSON        = 0xf5;
    constexpr uint8_t NEWDECIMAL  = 0xf6;
    constexpr uint8_t ENUM        = 0xf7;
    constexpr uint8_t SET         = 0xf8;
    constexpr uint8_t TINY_BLOB   = 0xf9;
    constexpr uint8_t MEDIUM_BLOB = 0xfa;
    constexpr uint8_t LONG_BLOB   = 0xfb;
    constexpr uint8_t BLOB        = 0xfc;
    constexpr uint8_t VAR_STRING  = 0xfd;
    constexpr uint8_t STRING      = 0xfe;
    constexpr uint8_t GEOMETRY    = 0xff;
}

// Column flags
namespace FieldFlag {
    constexpr uint16_t NOT_NULL       = 0x0001;
    constexpr uint16_t PRI_KEY        = 0x0002;
    constexpr uint16_t UNIQUE_KEY     = 0x0004;
    constexpr uint16_t MULTIPLE_KEY   = 0x0008;
    constexpr uint16_t BLOB           = 0x0010;
    constexpr uint16_t UNSIGNED       = 0x0020;
    constexpr uint16_t ZEROFILL       = 0x0040;
    constexpr uint16_t BINARY         = 0x0080;
    constexpr uint16_t ENUM           = 0x0100;
    constexpr uint16_t AUTO_INCREMENT = 0x0200;
    constexpr uint16_t TIMESTAMP      = 0x0400;
    constexpr uint16_t SET            = 0x0800;
    constexpr uint16_t NO_DEFAULT     = 0x1000;
    constexpr uint16_t ON_UPDATE_NOW  = 0x2000;
    constexpr uint16_t NUM            = 0x8000;
}

// Charset numbers
namespace Charset {
    constexpr uint8_t LATIN1_SWEDISH_CI = 8;
    constexpr uint8_t UTF8_GENERAL_CI   = 33;
    constexpr uint8_t UTF8MB4_GENERAL_CI = 45;
    constexpr uint8_t UTF8MB4_UNICODE_CI = 224;
    constexpr uint8_t BINARY            = 63;
}

// Response packet types
constexpr uint8_t OK_PACKET     = 0x00;
constexpr uint8_t EOF_PACKET    = 0xfe;
constexpr uint8_t ERR_PACKET    = 0xff;

// Common error codes
namespace ErrorCode {
    constexpr uint16_t ACCESS_DENIED         = 1045;
    constexpr uint16_t NO_DB_ERROR           = 1046;
    constexpr uint16_t BAD_DB_ERROR          = 1049;
    constexpr uint16_t TABLE_EXISTS_ERROR    = 1050;
    constexpr uint16_t BAD_TABLE_ERROR       = 1051;
    constexpr uint16_t BAD_FIELD_ERROR       = 1054;
    constexpr uint16_t SYNTAX_ERROR          = 1064;
    constexpr uint16_t PARSE_ERROR           = 1064;
    constexpr uint16_t EMPTY_QUERY           = 1065;
    constexpr uint16_t NO_SUCH_TABLE         = 1146;
    constexpr uint16_t UNKNOWN_COM_ERROR     = 1047;
    constexpr uint16_t HANDSHAKE_ERROR       = 1043;
    constexpr uint16_t UNKNOWN_ERROR         = 1105;
}

} // namespace mysql

// ============================================================================
// MySQL Protocol State
// ============================================================================

enum class MySqlProtocolState {
    INITIAL,              // Before handshake sent
    HANDSHAKE_SENT,       // Handshake sent, waiting for response
    AUTH_SWITCH,          // Auth method switch
    AUTHENTICATED,        // Authentication complete
    READY,                // Ready for commands
    QUERY_RESULT,         // Sending query result
    STMT_PREPARE,         // Prepared statement creation
    STMT_EXECUTE,         // Prepared statement execution
    CLOSING,              // Connection closing
    ERROR                 // Protocol error
};

// ============================================================================
// MySQL Prepared Statement Info
// ============================================================================

struct MySqlPreparedStatement {
    uint32_t id;
    std::string query;
    uint16_t num_params;
    uint16_t num_columns;
    std::vector<uint8_t> param_types;
    std::vector<uint8_t> param_unsigned;
    std::vector<ProtocolCodec::ColumnInfo> columns;
};

// ============================================================================
// MySQL Protocol Adapter
// ============================================================================

class MySqlAdapter : public ProtocolAdapter {
public:
    explicit MySqlAdapter(const ProtocolAdapterConfig& config = ProtocolAdapterConfig());
    ~MySqlAdapter() override;

    // ========================================================================
    // ProtocolHandler Interface
    // ========================================================================

    network::ProtocolType getProtocolType() const override {
        return network::ProtocolType::MYSQL;
    }

    // ========================================================================
    // Configuration
    // ========================================================================

    void setServerVersion(const std::string& version) { server_version_ = version; }
    const std::string& getServerVersion() const { return server_version_; }

    // C3: Emulation target version configuration
    enum class EmulationTarget {
        MYSQL_5_7,
        MYSQL_8_0,
        MARIADB_10_5
    };
    void setEmulationTarget(EmulationTarget target) { emulation_target_ = target; }
    EmulationTarget getEmulationTarget() const { return emulation_target_; }

    // C3: TLS configuration
    void setTLSConfig(const security::TLSConfig& config);
    bool isTLSEnabled() const { return tls_enabled_; }
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

    // IPC bridge helpers
    core::Status ensureRemoteClient(core::ErrorContext* ctx = nullptr);
    core::Status executeRemoteQuery(const QueryContext& query,
                                    ResultContext& result,
                                    core::ErrorContext* ctx = nullptr);

private:
    // ========================================================================
    // Command Handling
    // ========================================================================

    core::Status handleHandshakeResponse(network::Connection* conn);
    core::Status handleCommand(network::Connection* conn);
    core::Status handleComQuery(network::Connection* conn);
    core::Status handleComInitDb(network::Connection* conn);
    core::Status handleComPing(network::Connection* conn);
    core::Status handleComQuit(network::Connection* conn);
    core::Status handleComStmtPrepare(network::Connection* conn);
    core::Status handleComStmtExecute(network::Connection* conn);
    core::Status handleComStmtClose(network::Connection* conn);
    core::Status handleComStmtReset(network::Connection* conn);
    core::Status handleComFieldList(network::Connection* conn);
    core::Status handleComStatistics(network::Connection* conn);
    core::Status handleComResetConnection(network::Connection* conn);
    bool handleShowQuery(const std::string& query, ResultContext& result);

    // ========================================================================
    // Packet Sending
    // ========================================================================

    void sendHandshakePacket(network::Connection* conn);
    void sendOkPacket(network::Connection* conn, uint64_t affected_rows = 0,
                      uint64_t last_insert_id = 0, const std::string& info = "");
    void sendEofPacket(network::Connection* conn);
    void sendErrorPacket(network::Connection* conn, uint16_t error_code,
                         const std::string& sqlstate, const std::string& message);
    void sendResultSetHeader(network::Connection* conn, uint64_t column_count);
    void sendColumnDefinition(network::Connection* conn,
                              const ProtocolCodec::ColumnInfo& col,
                              const std::string& schema = "",
                              const std::string& table = "",
                              const std::string& org_table = "",
                              const std::string& org_name = "");
    void sendResultRow(network::Connection* conn,
                       const std::vector<ProtocolCodec::ColumnValue>& values);
    void sendPrepareOk(network::Connection* conn, uint32_t stmt_id,
                       uint16_t num_columns, uint16_t num_params);

    // ========================================================================
    // Helper Methods
    // ========================================================================

    // Packet I/O
    void sendPacket(network::Connection* conn, const std::vector<uint8_t>& payload);
    void resetSequence() { sequence_id_ = 0; }

    // Length-encoded integers
    void writeLenEncInt(std::vector<uint8_t>& buf, uint64_t value);
    uint64_t readLenEncInt(const uint8_t* data, size_t& offset, size_t max_len);

    // Length-encoded strings
    void writeLenEncString(std::vector<uint8_t>& buf, const std::string& str);
    std::string readLenEncString(const uint8_t* data, size_t& offset, size_t max_len);

    // Null-terminated strings
    void writeNullString(std::vector<uint8_t>& buf, const std::string& str);
    std::string readNullString(const uint8_t* data, size_t& offset, size_t max_len);

    // Fixed-length integers
    void writeInt1(std::vector<uint8_t>& buf, uint8_t value);
    void writeInt2(std::vector<uint8_t>& buf, uint16_t value);
    void writeInt3(std::vector<uint8_t>& buf, uint32_t value);
    void writeInt4(std::vector<uint8_t>& buf, uint32_t value);
    void writeInt8(std::vector<uint8_t>& buf, uint64_t value);

    uint8_t readInt1(const uint8_t* data);
    uint16_t readInt2(const uint8_t* data);
    uint32_t readInt3(const uint8_t* data);
    uint32_t readInt4(const uint8_t* data);
    uint64_t readInt8(const uint8_t* data);

    // Type conversion
    uint8_t wireTypeToMySqlType(WireType type);
    WireType mysqlTypeToWireType(uint8_t type);
    void mapStatusToMySqlError(uint32_t status,
                               uint16_t& error_code,
                               std::string& sqlstate);
    uint16_t mysqlCharsetForType(WireType type) const;

protected:
    // ========================================================================
    // Testing Support (protected for test harness access)
    // ========================================================================
    
    // C3: Authentication methods
    std::vector<uint8_t> computeNativePasswordAuth(const std::string& password,
                                                    const uint8_t* scramble);
    std::vector<uint8_t> computeCachingSha2PasswordAuth(const std::string& password,
                                                         const uint8_t* scramble);
    bool validateAuthResponse(const std::string& expected_plugin,
                              const std::string& auth_response,
                              const uint8_t* scramble,
                              const std::string& password);

private:
    void updateTransactionStatus(const std::string& sql, bool has_error);
    void bootstrapInformationSchema(core::ErrorContext* ctx);
    // Prepared statement helpers
    uint16_t countParameters(const std::string& query) const;
    std::string escapeLiteral(const std::string& value) const;
    bool decodePsParameter(uint8_t type, bool is_unsigned, const uint8_t* data,
                           size_t& offset, size_t max_len, std::string& out_literal);

    // ========================================================================
    // State
    // ========================================================================

    MySqlProtocolState mysql_state_ = MySqlProtocolState::INITIAL;

    // Packet parsing
    uint8_t sequence_id_ = 0;
    std::vector<uint8_t> current_packet_;

    // C3: Server info with emulation target
    std::string server_version_ = "8.0.35-ScratchBird";
    EmulationTarget emulation_target_ = EmulationTarget::MYSQL_8_0;
    uint32_t connection_id_ = 0;
    uint8_t auth_scramble_[20] = {0};  // 20-byte scramble for auth
    std::string auth_plugin_name_ = "caching_sha2_password";  // C3: Default to caching_sha2_password

    // Client info from handshake response
    uint32_t client_capabilities_ = 0;
    uint32_t max_packet_size_ = 16777215;
    uint8_t client_charset_ = mysql::Charset::UTF8MB4_GENERAL_CI;
    std::string auth_response_;

    // Server capabilities
    uint32_t server_capabilities_ =
        mysql::Capability::LONG_PASSWORD |
        mysql::Capability::FOUND_ROWS |
        mysql::Capability::LONG_FLAG |
        mysql::Capability::CONNECT_WITH_DB |
        mysql::Capability::PROTOCOL_41 |
        mysql::Capability::TRANSACTIONS |
        mysql::Capability::SECURE_CONNECTION |
        mysql::Capability::MULTI_STATEMENTS |
        mysql::Capability::MULTI_RESULTS |
        mysql::Capability::PLUGIN_AUTH |
        mysql::Capability::DEPRECATE_EOF;

    // Server status
    uint16_t server_status_ = mysql::ServerStatus::AUTOCOMMIT;

    // Prepared statements
    uint32_t next_stmt_id_ = 1;
    std::unordered_map<uint32_t, MySqlPreparedStatement> prepared_statements_;
    std::vector<std::string> last_warnings_;
    std::vector<std::string> last_errors_;
    uint16_t last_error_code_ = 0;
    std::string last_error_sqlstate_;
    std::chrono::steady_clock::time_point start_time_;

    // C3: TLS support
    std::unique_ptr<security::TLSContext> tls_context_;
    std::unique_ptr<security::TLSConnection> tls_connection_;
    bool tls_enabled_ = false;
    bool tls_negotiated_ = false;

    // IPC client (bridge to engine)
    std::unique_ptr<client::Connection> client_;
    client::ConnectionConfig client_config_;
    bool default_db_set_ = false;
    bool information_schema_bootstrapped_ = false;

    // C3: Database validation
    bool validateDatabaseExists(const std::string& db_name, core::ErrorContext* ctx = nullptr);
    void updateServerCapabilities();
};

} // namespace protocol
} // namespace scratchbird
