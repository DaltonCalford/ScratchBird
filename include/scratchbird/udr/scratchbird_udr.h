/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0
 */
#pragma once

/**
 * ScratchBird UDR Connector
 * 
 * Section D5: Remote Engine UDR Connectors - ScratchBird
 * 
 * Implements UDRConnector interface for remote ScratchBird databases using
 * the native ScratchBird Wire Protocol (SBWP).
 */

#include "scratchbird/udr/udr_connector.h"
#include "scratchbird/udr/connection_pool.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <optional>

namespace scratchbird {
namespace udr {

// ============================================================================
// SBWP Protocol Constants
// ============================================================================

namespace sbwp {

// Protocol versions
constexpr uint16_t PROTOCOL_VERSION_1_0 = 0x0100;
constexpr uint16_t PROTOCOL_VERSION_1_1 = 0x0101;
constexpr uint16_t CURRENT_VERSION = PROTOCOL_VERSION_1_1;

// Message types
enum class MessageType : uint8_t {
    // Connection
    STARTUP = 0x01,
    SSL_REQUEST = 0x02,
    AUTHENTICATE = 0x03,
    READY = 0x04,
    ERROR_MESSAGE = 0x05,
    
    // Query
    SIMPLE_QUERY = 0x10,
    PARSE = 0x11,
    BIND = 0x12,
    EXECUTE = 0x13,
    DESCRIBE = 0x14,
    CLOSE = 0x15,
    SYNC = 0x16,
    
    // Results
    ROW_DESCRIPTION = 0x20,
    DATA_ROW = 0x21,
    COMMAND_COMPLETE = 0x22,
    EMPTY_QUERY_RESPONSE = 0x23,
    
    // COPY
    COPY_IN_REQUEST = 0x30,
    COPY_OUT_RESPONSE = 0x31,
    COPY_DATA = 0x32,
    COPY_DONE = 0x33,
    COPY_FAIL = 0x34,
    
    // Transactions
    BEGIN = 0x40,
    COMMIT = 0x41,
    ROLLBACK = 0x42,
    SAVEPOINT = 0x43,
    
    // Asynchronous
    NOTIFICATION = 0x50,
    CANCEL_REQUEST = 0x51,
    
    // System
    PING = 0x60,
    PONG = 0x61,
    TERMINATE = 0x62
};

// SSL modes
constexpr uint8_t SSL_MODE_DISABLE = 0;
constexpr uint8_t SSL_MODE_PREFER = 1;
constexpr uint8_t SSL_MODE_REQUIRE = 2;
constexpr uint8_t SSL_MODE_VERIFY_CA = 3;
constexpr uint8_t SSL_MODE_VERIFY_FULL = 4;

// Authentication methods
constexpr uint8_t AUTH_TRUST = 0;
constexpr uint8_t AUTH_PASSWORD = 1;
constexpr uint8_t AUTH_SCRAM_SHA256 = 2;
constexpr uint8_t AUTH_SCRAM_SHA512 = 3;
constexpr uint8_t AUTH_CERTIFICATE = 4;

// Status codes
constexpr uint16_t STATUS_OK = 0;
constexpr uint16_t STATUS_ERROR = 1;
constexpr uint16_t STATUS_AUTH_REQUIRED = 2;
constexpr uint16_t STATUS_SSL_REQUIRED = 3;
constexpr uint16_t STATUS_VERSION_MISMATCH = 4;

// Format codes
constexpr uint16_t FORMAT_TEXT = 0;
constexpr uint16_t FORMAT_BINARY = 1;

// Maximum sizes
constexpr size_t MAX_MESSAGE_SIZE = 16 * 1024 * 1024;  // 16MB
constexpr size_t MAX_PACKET_SIZE = 8192;

} // namespace sbwp

// ============================================================================
// SBWP Message Structure
// ============================================================================

struct SBWPMessage {
    sbwp::MessageType type;
    uint32_t length;
    std::vector<uint8_t> payload;
    
    SBWPMessage() : type(sbwp::MessageType::ERROR_MESSAGE), length(0) {}
    SBWPMessage(sbwp::MessageType t, const std::vector<uint8_t>& p)
        : type(t), length(static_cast<uint32_t>(p.size())), payload(p) {}
};

// ============================================================================
// SBWP Row Description
// ============================================================================

struct SBWPField {
    std::string name;
    uint32_t table_oid;
    uint16_t column_number;
    uint32_t type_oid;
    int16_t type_size;
    int32_t type_modifier;
    uint16_t format_code;
};

// ============================================================================
// SBWP Data Row
// ============================================================================

struct SBWPRow {
    std::vector<std::optional<std::vector<uint8_t>>> fields;
};

// ============================================================================
// ScratchBird Connection
// ============================================================================

class ScratchBirdConnection : public PooledConnection {
public:
    ScratchBirdConnection();
    ~ScratchBirdConnection() override;

    // PooledConnection interface
    bool isOpen() const override;
    bool isValid() const override;
    core::Status ping(core::ErrorContext* ctx = nullptr) override;
    void close() override;
    
    std::string getRemoteAddress() const override;
    std::string getRemoteVersion() const override;

    // ScratchBird-specific
    core::Status connect(const std::string& host, uint16_t port,
                        const std::string& database,
                        const std::string& user,
                        const std::string& password,
                        const std::string& ssl_mode,
                        core::ErrorContext* ctx = nullptr);

    // Protocol operations
    core::Status sendSimpleQuery(const std::string& sql, core::ErrorContext* ctx);
    core::Status sendParse(const std::string& name, const std::string& sql,
                          core::ErrorContext* ctx);
    core::Status sendBind(const std::string& portal, const std::string& stmt,
                         const std::vector<RemoteValue>& params,
                         core::ErrorContext* ctx);
    core::Status sendExecute(const std::string& portal, uint32_t max_rows,
                            core::ErrorContext* ctx);
    core::Status sendSync(core::ErrorContext* ctx);
    core::Status sendTerminate(core::ErrorContext* ctx);
    
    // Transaction operations
    core::Status sendBegin(core::ErrorContext* ctx);
    core::Status sendCommit(core::ErrorContext* ctx);
    core::Status sendRollback(core::ErrorContext* ctx);
    core::Status sendSavepoint(const std::string& name, core::ErrorContext* ctx);
    
    // COPY operations
    core::Status sendCopyData(const uint8_t* data, size_t len, core::ErrorContext* ctx);
    core::Status sendCopyDone(core::ErrorContext* ctx);
    core::Status sendCopyFail(const std::string& reason, core::ErrorContext* ctx);
    
    // Result handling
    core::Status readMessage(SBWPMessage& msg, core::ErrorContext* ctx);
    core::Status readRowDescription(std::vector<SBWPField>& fields, core::ErrorContext* ctx);
    core::Status readDataRow(SBWPRow& row, const std::vector<SBWPField>& fields,
                            core::ErrorContext* ctx);
    
    // State
    uint16_t getProtocolVersion() const { return protocol_version_; }
    bool isSSL() const { return ssl_enabled_; }

private:
    // Network
    int socket_fd_ = -1;
    std::string host_;
    uint16_t port_ = 5433;  // Default ScratchBird port
    
    // Connection state
    uint16_t protocol_version_ = sbwp::CURRENT_VERSION;
    uint32_t process_id_ = 0;
    uint32_t secret_key_ = 0;
    bool ssl_enabled_ = false;
    
    // Configuration
    std::string database_;
    std::string user_;
    std::string password_;
    std::string ssl_mode_;
    
    // SSL context
    void* ssl_ctx_ = nullptr;
    void* ssl_ = nullptr;
    
    // Internal methods
    core::Status startup(core::ErrorContext* ctx);
    core::Status negotiateSSL(core::ErrorContext* ctx);
    core::Status authenticate(core::ErrorContext* ctx);
    
    // Message I/O
    core::Status sendMessage(const SBWPMessage& msg, core::ErrorContext* ctx);
    core::Status writeMessage(const SBWPMessage& msg, core::ErrorContext* ctx);
    
    core::Status readExactly(void* buffer, size_t len, core::ErrorContext* ctx);
    core::Status writeExactly(const void* buffer, size_t len, core::ErrorContext* ctx);
    
    // SSL
    void cleanupSSL();
    
    // Utility
    void cleanupSocket();
    std::vector<uint8_t> encodeStartupParams() const;
    sbwp::MessageType readMessageType(core::ErrorContext* ctx);
};

// ============================================================================
// ScratchBird Connection Factory
// ============================================================================

class ScratchBirdConnectionFactory : public ConnectionFactory {
public:
    ScratchBirdConnectionFactory(const UDRServerConfig& config);
    
    std::unique_ptr<PooledConnection> createConnection(
        core::ErrorContext* ctx = nullptr) override;
    
    bool validateConnection(PooledConnection* conn,
                           core::ErrorContext* ctx = nullptr) override;
    
    void destroyConnection(PooledConnection* conn) override;

private:
    UDRServerConfig config_;
};

// ============================================================================
// ScratchBird UDR Connector
// ============================================================================

class ScratchBirdUDRConnector : public UDRConnector {
public:
    ScratchBirdUDRConnector();
    ~ScratchBirdUDRConnector() override;

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

    ConnectorType getType() const override { return ConnectorType::SCRATCHBIRD; }
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
    
    // Cursor tracking
    std::string active_cursor_;
    
    // COPY state
    bool in_copy_in_ = false;
    bool in_copy_out_ = false;
    ScratchBirdConnection* copy_conn_ = nullptr;
    
    // Helper methods
    std::unique_ptr<ScratchBirdConnection> acquireConnection(core::ErrorContext* ctx);
    void releaseConnection(std::unique_ptr<ScratchBirdConnection> conn);
    
    // Type mapping
    uint32_t mapTypeToSBWP(core::DataType type) const;
    core::DataType mapTypeFromSBWP(uint32_t oid) const;
};

} // namespace udr
} // namespace scratchbird
