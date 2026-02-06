/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0
 */
#pragma once

/**
 * MySQL UDR Connector
 * 
 * Section D3: Remote Engine UDR Connectors - MySQL
 * 
 * Implements UDRConnector interface for MySQL databases using
 * the native MySQL wire protocol.
 */

#include "scratchbird/udr/udr_connector.h"
#include "scratchbird/udr/connection_pool.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace scratchbird {
namespace udr {

// ============================================================================
// MySQL Protocol Constants
// ============================================================================

namespace mysql {

// Capability flags
constexpr uint32_t CLIENT_LONG_PASSWORD = 0x00000001;
constexpr uint32_t CLIENT_FOUND_ROWS = 0x00000002;
constexpr uint32_t CLIENT_LONG_FLAG = 0x00000004;
constexpr uint32_t CLIENT_CONNECT_WITH_DB = 0x00000008;
constexpr uint32_t CLIENT_NO_SCHEMA = 0x00000010;
constexpr uint32_t CLIENT_COMPRESS = 0x00000020;
constexpr uint32_t CLIENT_ODBC = 0x00000040;
constexpr uint32_t CLIENT_LOCAL_FILES = 0x00000080;
constexpr uint32_t CLIENT_IGNORE_SPACE = 0x00000100;
constexpr uint32_t CLIENT_PROTOCOL_41 = 0x00000200;
constexpr uint32_t CLIENT_INTERACTIVE = 0x00000400;
constexpr uint32_t CLIENT_SSL = 0x00000800;
constexpr uint32_t CLIENT_IGNORE_SIGPIPE = 0x00001000;
constexpr uint32_t CLIENT_TRANSACTIONS = 0x00002000;
constexpr uint32_t CLIENT_RESERVED = 0x00004000;
constexpr uint32_t CLIENT_SECURE_CONNECTION = 0x00008000;
constexpr uint32_t CLIENT_MULTI_STATEMENTS = 0x00010000;
constexpr uint32_t CLIENT_MULTI_RESULTS = 0x00020000;
constexpr uint32_t CLIENT_PS_MULTI_RESULTS = 0x00040000;
constexpr uint32_t CLIENT_PLUGIN_AUTH = 0x00080000;
constexpr uint32_t CLIENT_CONNECT_ATTRS = 0x00100000;
constexpr uint32_t CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA = 0x00200000;
constexpr uint32_t CLIENT_SESSION_TRACK = 0x00800000;
constexpr uint32_t CLIENT_DEPRECATE_EOF = 0x01000000;

// Commands
constexpr uint8_t COM_SLEEP = 0x00;
constexpr uint8_t COM_QUIT = 0x01;
constexpr uint8_t COM_INIT_DB = 0x02;
constexpr uint8_t COM_QUERY = 0x03;
constexpr uint8_t COM_FIELD_LIST = 0x04;
constexpr uint8_t COM_CREATE_DB = 0x05;
constexpr uint8_t COM_DROP_DB = 0x06;
constexpr uint8_t COM_REFRESH = 0x07;
constexpr uint8_t COM_SHUTDOWN = 0x08;
constexpr uint8_t COM_STATISTICS = 0x09;
constexpr uint8_t COM_PROCESS_INFO = 0x0a;
constexpr uint8_t COM_CONNECT = 0x0b;
constexpr uint8_t COM_PROCESS_KILL = 0x0c;
constexpr uint8_t COM_DEBUG = 0x0d;
constexpr uint8_t COM_PING = 0x0e;
constexpr uint8_t COM_TIME = 0x0f;
constexpr uint8_t COM_DELAYED_INSERT = 0x10;
constexpr uint8_t COM_CHANGE_USER = 0x11;
constexpr uint8_t COM_BINLOG_DUMP = 0x12;
constexpr uint8_t COM_TABLE_DUMP = 0x13;
constexpr uint8_t COM_CONNECT_OUT = 0x14;
constexpr uint8_t COM_REGISTER_SLAVE = 0x15;
constexpr uint8_t COM_STMT_PREPARE = 0x16;
constexpr uint8_t COM_STMT_EXECUTE = 0x17;
constexpr uint8_t COM_STMT_SEND_LONG_DATA = 0x18;
constexpr uint8_t COM_STMT_CLOSE = 0x19;
constexpr uint8_t COM_STMT_RESET = 0x1a;
constexpr uint8_t COM_SET_OPTION = 0x1b;
constexpr uint8_t COM_STMT_FETCH = 0x1c;

// Response types
constexpr uint8_t PACKET_OK = 0x00;
constexpr uint8_t PACKET_EOF = 0xfe;
constexpr uint8_t PACKET_ERROR = 0xff;

// Auth plugins
constexpr const char* MYSQL_NATIVE_PASSWORD = "mysql_native_password";
constexpr const char* CACHING_SHA2_PASSWORD = "caching_sha2_password";
constexpr const char* SHA256_PASSWORD = "sha256_password";

} // namespace mysql

// ============================================================================
// MySQL Connection
// ============================================================================

class MySQLConnection : public PooledConnection {
public:
    MySQLConnection();
    ~MySQLConnection() override;

    // PooledConnection interface
    bool isOpen() const override;
    bool isValid() const override;
    core::Status ping(core::ErrorContext* ctx = nullptr) override;
    void close() override;
    
    std::string getRemoteAddress() const override;
    std::string getRemoteVersion() const override;

    // MySQL-specific
    core::Status connect(const std::string& host, uint16_t port,
                        const std::string& database,
                        const std::string& user,
                        const std::string& password,
                        const std::string& ssl_mode,
                        core::ErrorContext* ctx = nullptr);

    // Protocol operations
    core::Status sendQuery(const std::string& sql, core::ErrorContext* ctx);
    core::Status sendInitDB(const std::string& database, core::ErrorContext* ctx);
    core::Status sendPing(core::ErrorContext* ctx);
    core::Status sendQuit(core::ErrorContext* ctx);
    
    // Prepared statements
    core::Status sendStmtPrepare(const std::string& sql, uint32_t& stmt_id,
                                core::ErrorContext* ctx);
    core::Status sendStmtExecute(uint32_t stmt_id, core::ErrorContext* ctx);
    core::Status sendStmtClose(uint32_t stmt_id, core::ErrorContext* ctx);
    
    // Result handling
    core::Status readResultSet(RemoteResultSet& result, core::ErrorContext* ctx);
    core::Status readOKPacket(uint64_t& affected_rows, uint64_t& last_insert_id,
                             core::ErrorContext* ctx);
    core::Status readErrorPacket(std::string& error_message, core::ErrorContext* ctx);

    // State
    uint32_t getCapabilities() const { return server_capabilities_; }
    const std::string& getAuthPlugin() const { return auth_plugin_name_; }
    
    // Packet I/O (public for connector use)
    core::Status readPacket(std::vector<uint8_t>& payload, core::ErrorContext* ctx);
    core::Status writePacket(const std::vector<uint8_t>& payload, core::ErrorContext* ctx);
    
    // Length-encoded integer (public for connector use)
    static size_t readLengthEncodedInt(const uint8_t* data, size_t& bytes_read);
    static std::vector<uint8_t> writeLengthEncodedInt(uint64_t value);

private:
    // Network
    int socket_fd_ = -1;
    std::string host_;
    uint16_t port_ = 3306;
    
    // Connection state
    uint32_t server_capabilities_ = 0;
    uint32_t client_capabilities_ = 0;
    uint8_t charset_ = 33;  // utf8_general_ci
    std::string auth_plugin_name_;
    std::vector<uint8_t> auth_plugin_data_;
    uint32_t connection_id_ = 0;
    
    // SSL
    bool ssl_enabled_ = false;
    void* ssl_ctx_ = nullptr;
    
    // Sequence number for packets
    uint8_t sequence_ = 0;
    
    // Internal methods
    core::Status handshake(const std::string& database, const std::string& user,
                          const std::string& password, core::ErrorContext* ctx);
    core::Status authenticate(const std::string& user, const std::string& password,
                             const std::string& database, core::ErrorContext* ctx);
    
    // Auth methods
    core::Status authNativePassword(const std::string& password, core::ErrorContext* ctx);
    core::Status authCachingSha2Password(const std::string& password, core::ErrorContext* ctx);
    
    core::Status readExactly(void* buffer, size_t len, core::ErrorContext* ctx);
    core::Status writeExactly(const void* buffer, size_t len, core::ErrorContext* ctx);
    
    // SSL
    core::Status upgradeToSSL(const std::string& ssl_mode, core::ErrorContext* ctx);
    void cleanupSSL();
    
    // Utility
    void cleanupSocket();
    void resetSequence() { sequence_ = 0; }
    
    // Encoding
    std::vector<uint8_t> scrambleNativePassword(const std::string& password,
                                               const std::vector<uint8_t>& scramble);
    std::vector<uint8_t> scrambleCachingSha2Password(const std::string& password,
                                                    const std::vector<uint8_t>& scramble);
};

// ============================================================================
// MySQL Connection Factory
// ============================================================================

class MySQLConnectionFactory : public ConnectionFactory {
public:
    MySQLConnectionFactory(const UDRServerConfig& config);
    
    std::unique_ptr<PooledConnection> createConnection(
        core::ErrorContext* ctx = nullptr) override;
    
    bool validateConnection(PooledConnection* conn,
                           core::ErrorContext* ctx = nullptr) override;
    
    void destroyConnection(PooledConnection* conn) override;

private:
    UDRServerConfig config_;
};

// ============================================================================
// MySQL UDR Connector
// ============================================================================

class MySQLUDRConnector : public UDRConnector {
public:
    MySQLUDRConnector();
    ~MySQLUDRConnector() override;

    // UDRConnector interface
    core::Status initialize(const UDRServerConfig& config,
                           core::ErrorContext* ctx = nullptr) override;
    core::Status shutdown(core::ErrorContext* ctx = nullptr) override;
    bool isConnected() const override;

    core::Status ping(core::ErrorContext* ctx = nullptr) override;
    core::Status reconnect(core::ErrorContext* ctx = nullptr) override;

    core::Status executeQuery(const std::string& sql,
                             RemoteResultSet& result,
                             core::ErrorContext* ctx = nullptr) override;
    
    core::Status executeCommand(const std::string& sql,
                               uint64_t& rows_affected,
                               core::ErrorContext* ctx = nullptr) override;
    
    core::Status prepareStatement(const std::string& name,
                                 const std::string& sql,
                                 std::vector<uint32_t>& param_types,
                                 core::ErrorContext* ctx = nullptr) override;
    
    core::Status executePrepared(const std::string& name,
                                const std::vector<RemoteValue>& params,
                                RemoteResultSet& result,
                                core::ErrorContext* ctx = nullptr) override;
    
    core::Status closeStatement(const std::string& name,
                               core::ErrorContext* ctx = nullptr) override;

    core::Status declareCursor(const std::string& cursor_name,
                              const std::string& query,
                              bool scrollable,
                              core::ErrorContext* ctx = nullptr) override;
    
    core::Status fetchCursor(const std::string& cursor_name,
                            uint32_t count,
                            RemoteResultSet& result,
                            core::ErrorContext* ctx = nullptr) override;
    
    core::Status closeCursor(const std::string& cursor_name,
                            core::ErrorContext* ctx = nullptr) override;

    core::Status beginTransaction(core::ErrorContext* ctx = nullptr) override;
    core::Status commitTransaction(core::ErrorContext* ctx = nullptr) override;
    core::Status rollbackTransaction(core::ErrorContext* ctx = nullptr) override;
    core::Status savepoint(const std::string& name,
                          core::ErrorContext* ctx = nullptr) override;
    core::Status rollbackToSavepoint(const std::string& name,
                                    core::ErrorContext* ctx = nullptr) override;

    core::Status getTableInfo(const std::string& schema,
                             const std::string& table,
                             RemoteTableInfo& info,
                             core::ErrorContext* ctx = nullptr) override;
    
    core::Status listTables(const std::string& schema,
                           std::vector<std::string>& tables,
                           core::ErrorContext* ctx = nullptr) override;
    
    core::Status getProcedureInfo(const std::string& schema,
                                 const std::string& procedure,
                                 RemoteProcedureInfo& info,
                                 core::ErrorContext* ctx = nullptr) override;
    
    core::Status listProcedures(const std::string& schema,
                               std::vector<std::string>& procedures,
                               core::ErrorContext* ctx = nullptr) override;

    core::Status startCopyIn(const std::string& table,
                            const std::vector<std::string>& columns,
                            core::ErrorContext* ctx = nullptr) override;
    
    core::Status sendCopyData(const uint8_t* data, size_t len,
                             core::ErrorContext* ctx = nullptr) override;
    
    core::Status endCopyIn(uint64_t& rows_inserted,
                          core::ErrorContext* ctx = nullptr) override;
    
    core::Status startCopyOut(const std::string& query,
                             core::ErrorContext* ctx = nullptr) override;
    
    core::Status receiveCopyData(std::vector<uint8_t>& data,
                                bool& done,
                                core::ErrorContext* ctx = nullptr) override;

    ConnectorType getType() const override { return ConnectorType::MYSQL; }
    std::string getVersion() const override;
    std::string getRemoteVersion() const override;
    std::vector<std::string> getSupportedFeatures() const override;

private:
    UDRServerConfig config_;
    std::unique_ptr<ConnectionPool> pool_;
    std::string remote_version_;
    
    // Prepared statement tracking
    std::unordered_map<std::string, uint32_t> prepared_statements_;
    mutable std::mutex prepared_mutex_;
    
    // Cursor tracking
    struct Cursor {
        std::string query;
        std::string name;
        bool scrollable = false;
        bool open = false;
    };
    std::unordered_map<std::string, Cursor> active_cursors_;
    mutable std::mutex cursor_mutex_;
    
    // COPY state tracking
    struct CopyState {
        std::string table_name;
        std::vector<std::string> columns;
        std::vector<uint8_t> buffer;
        bool active = false;
        bool is_in = true;
    };
    CopyState copy_state_;
    mutable std::mutex copy_mutex_;
    
    // Helper methods
    std::unique_ptr<MySQLConnection> acquireConnection(core::ErrorContext* ctx);
    void releaseConnection(std::unique_ptr<MySQLConnection> conn);
};

} // namespace udr
} // namespace scratchbird
