/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0
 */
#pragma once

/**
 * PostgreSQL UDR Connector
 * 
 * Section D2: Remote Engine UDR Connectors - PostgreSQL
 * 
 * Implements UDRConnector interface for PostgreSQL databases using
 * the native PostgreSQL wire protocol (v3).
 */

#include "scratchbird/udr/udr_connector.h"
#include "scratchbird/udr/connection_pool.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>

namespace scratchbird {
namespace udr {

// ============================================================================
// PostgreSQL Protocol Constants
// ============================================================================

namespace pg {

// Message types (first byte of message)
constexpr char MSG_ERROR = 'E';
constexpr char MSG_NOTICE = 'N';
constexpr char MSG_AUTHENTICATION = 'R';
constexpr char MSG_PARAMETER_STATUS = 'S';
constexpr char MSG_BACKEND_KEY_DATA = 'K';
constexpr char MSG_READY_FOR_QUERY = 'Z';
constexpr char MSG_ROW_DESCRIPTION = 'T';
constexpr char MSG_DATA_ROW = 'D';
constexpr char MSG_COMMAND_COMPLETE = 'C';
constexpr char MSG_PARSE_COMPLETE = '1';
constexpr char MSG_BIND_COMPLETE = '2';
constexpr char MSG_CLOSE_COMPLETE = '3';
constexpr char MSG_NO_DATA = 'n';
constexpr char MSG_PORTAL_SUSPENDED = 's';
constexpr char MSG_PARAMETER_DESCRIPTION = 't';
constexpr char MSG_COPY_IN_RESPONSE = 'G';
constexpr char MSG_COPY_OUT_RESPONSE = 'H';
constexpr char MSG_COPY_BOTH_RESPONSE = 'W';
constexpr char MSG_COPY_DATA = 'd';
constexpr char MSG_COPY_DONE = 'c';
constexpr char MSG_EMPTY_QUERY_RESPONSE = 'I';
constexpr char MSG_NEGOTIATE_PROTOCOL_VERSION = 'v';

// Frontend messages
constexpr char MSG_QUERY = 'Q';
constexpr char MSG_PARSE = 'P';
constexpr char MSG_BIND = 'B';
constexpr char MSG_EXECUTE = 'E';
constexpr char MSG_SYNC = 'S';
constexpr char MSG_CLOSE = 'C';
constexpr char MSG_DESCRIBE = 'D';
constexpr char MSG_COPY_FAIL = 'f';
constexpr char MSG_PASSWORD_MESSAGE = 'p';
constexpr char MSG_TERMINATE = 'X';
constexpr char MSG_CANCEL_REQUEST = 'F';
constexpr char MSG_GSSENC_REQUEST = 'G';
constexpr char MSG_SSL_REQUEST = 'S';

// Authentication types
constexpr int32_t AUTH_OK = 0;
constexpr int32_t AUTH_KERBEROS_V5 = 2;
constexpr int32_t AUTH_CLEARTEXT_PASSWORD = 3;
constexpr int32_t AUTH_MD5_PASSWORD = 5;
constexpr int32_t AUTH_SCM_CREDENTIAL = 6;
constexpr int32_t AUTH_GSS = 7;
constexpr int32_t AUTH_GSS_CONTINUE = 8;
constexpr int32_t AUTH_SSPI = 9;
constexpr int32_t AUTH_SASL = 10;
constexpr int32_t AUTH_SASL_CONTINUE = 11;
constexpr int32_t AUTH_SASL_FINAL = 12;

// Transaction status
constexpr char TXN_IDLE = 'I';
constexpr char TXN_ACTIVE = 'T';
constexpr char TXN_ERROR = 'E';

// SSL mode
constexpr char SSL_NO = 'N';
constexpr char SSL_YES = 'S';

// Protocol version
constexpr int32_t PROTOCOL_VERSION = 196608;  // 3.0
constexpr int32_t CANCEL_CODE = 80877102;
constexpr int32_t SSL_CODE = 80877103;
constexpr int32_t GSSENC_CODE = 80877104;

} // namespace pg

// ============================================================================
// PostgreSQL Connection
// ============================================================================

class PostgreSQLConnection : public PooledConnection {
public:
    PostgreSQLConnection();
    ~PostgreSQLConnection() override;

    // PooledConnection interface
    bool isOpen() const override;
    bool isValid() const override;
    core::Status ping(core::ErrorContext* ctx = nullptr) override;
    void close() override;
    
    std::string getRemoteAddress() const override;
    std::string getRemoteVersion() const override;

    // PostgreSQL-specific
    core::Status connect(const std::string& host, uint16_t port,
                         const std::string& database,
                         const std::string& user,
                         const std::string& password,
                         const std::string& ssl_mode,
                         core::ErrorContext* ctx = nullptr);

    // Protocol operations
    core::Status sendQuery(const std::string& sql, core::ErrorContext* ctx);
    core::Status sendParse(const std::string& name, const std::string& sql,
                          const std::vector<uint32_t>& param_types,
                          core::ErrorContext* ctx);
    core::Status sendBind(const std::string& portal, const std::string& statement,
                         const std::vector<RemoteValue>& params,
                         core::ErrorContext* ctx);
    core::Status sendExecute(const std::string& portal, int32_t max_rows,
                            core::ErrorContext* ctx);
    core::Status sendSync(core::ErrorContext* ctx);
    core::Status sendClose(char type, const std::string& name, core::ErrorContext* ctx);
    core::Status sendDescribe(char type, const std::string& name, core::ErrorContext* ctx);
    core::Status sendCopyData(const uint8_t* data, size_t len, core::ErrorContext* ctx);
    core::Status sendCopyDone(core::ErrorContext* ctx);
    core::Status sendCopyFail(const std::string& error, core::ErrorContext* ctx);

    // Result handling
    core::Status readResultSet(RemoteResultSet& result, core::ErrorContext* ctx);
    core::Status readCommandComplete(std::string& tag, uint64_t& rows, core::ErrorContext* ctx);
    core::Status readError(std::string& severity, std::string& sqlstate,
                          std::string& message, core::ErrorContext* ctx);
    
    // Low-level protocol (public for connector use)
    core::Status readMessage(char& msg_type, std::vector<uint8_t>& payload,
                            core::ErrorContext* ctx);

    // Cancel
    core::Status sendCancelRequest(core::ErrorContext* ctx);

    // State
    bool isInTransaction() const { return txn_status_ != pg::TXN_IDLE; }
    bool hasTransactionError() const { return txn_status_ == pg::TXN_ERROR; }

private:
    // Network
    int socket_fd_ = -1;
    std::string host_;
    uint16_t port_ = 5432;
    
    // Connection state
    int32_t backend_pid_ = 0;
    int32_t backend_key_ = 0;
    char txn_status_ = pg::TXN_IDLE;
    std::unordered_map<std::string, std::string> parameters_;
    
    // SSL
    bool ssl_enabled_ = false;
    void* ssl_ctx_ = nullptr;  // SSL* (opaque to avoid openssl header dependency)
    
    // Buffer for reading
    std::vector<uint8_t> read_buffer_;
    
    // Internal methods
    core::Status startup(const std::string& database, const std::string& user,
                        core::ErrorContext* ctx);
    core::Status authenticate(int32_t auth_type, const std::string& password,
                             const std::vector<uint8_t>& auth_data,
                             core::ErrorContext* ctx);
    core::Status handleAuthMD5(const std::string& password,
                              const std::vector<uint8_t>& salt,
                              core::ErrorContext* ctx);
    core::Status handleAuthSASL(const std::string& password,
                               const std::vector<uint8_t>& data,
                               core::ErrorContext* ctx);
    
    core::Status writeMessage(char msg_type, const std::vector<uint8_t>& payload,
                             core::ErrorContext* ctx);
    
    core::Status readExactly(void* buffer, size_t len, core::ErrorContext* ctx);
    core::Status writeExactly(const void* buffer, size_t len, core::ErrorContext* ctx);
    
    core::Status negotiateSSL(const std::string& ssl_mode, core::ErrorContext* ctx);
    core::Status initSSL(core::ErrorContext* ctx);
    void cleanupSSL();
    
    // Utility
    void cleanupSocket();
    std::string md5Hash(const std::string& input);
    std::string md5Password(const std::string& password, const std::string& user,
                           const std::vector<uint8_t>& salt);
};

// ============================================================================
// PostgreSQL Connection Factory
// ============================================================================

class PostgreSQLConnectionFactory : public ConnectionFactory {
public:
    PostgreSQLConnectionFactory(const UDRServerConfig& config);
    
    std::unique_ptr<PooledConnection> createConnection(
        core::ErrorContext* ctx = nullptr) override;
    
    bool validateConnection(PooledConnection* conn,
                           core::ErrorContext* ctx = nullptr) override;
    
    void destroyConnection(PooledConnection* conn) override;

private:
    UDRServerConfig config_;
};

// ============================================================================
// PostgreSQL UDR Connector
// ============================================================================

class PostgreSQLUDRConnector : public UDRConnector {
public:
    PostgreSQLUDRConnector();
    ~PostgreSQLUDRConnector() override;

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

    ConnectorType getType() const override { return ConnectorType::POSTGRESQL; }
    std::string getVersion() const override;
    std::string getRemoteVersion() const override;
    std::vector<std::string> getSupportedFeatures() const override;

private:
    UDRServerConfig config_;
    std::unique_ptr<ConnectionPool> pool_;
    std::string remote_version_;
    
    // Prepared statement tracking
    std::unordered_map<std::string, std::vector<uint32_t>> prepared_statements_;
    mutable std::mutex prepared_mutex_;
    
    // Active COPY state
    bool copy_in_progress_ = false;
    bool copy_is_in_ = false;  // true = COPY IN, false = COPY OUT
    std::unique_ptr<PostgreSQLConnection> copy_connection_;  // Connection used for COPY
    
    // Helper methods
    std::unique_ptr<PostgreSQLConnection> acquireConnection(core::ErrorContext* ctx);
    void releaseConnection(std::unique_ptr<PostgreSQLConnection> conn);
    
    // Type mapping
    uint32_t mapTypeToOid(const std::string& type_name) const;
    std::string mapOidToType(uint32_t oid) const;
    RemoteValue decodeValue(const std::vector<uint8_t>& data, uint32_t oid) const;
    std::vector<uint8_t> encodeValue(const RemoteValue& value, uint32_t oid) const;
};

} // namespace udr
} // namespace scratchbird
