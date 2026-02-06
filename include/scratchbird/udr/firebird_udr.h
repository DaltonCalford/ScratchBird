/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0
 */
#pragma once

/**
 * Firebird UDR Connector
 * 
 * Section D4: Remote Engine UDR Connectors - Firebird
 * 
 * Implements UDRConnector interface for Firebird databases using
 * the native Firebird wire protocol with XDR encoding.
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
// Firebird Protocol Constants (from firebird_adapter.h)
// ============================================================================

namespace firebird {

// Protocol versions
constexpr uint32_t PROTOCOL_VERSION_10 = 10;
constexpr uint32_t PROTOCOL_VERSION_11 = 11;
constexpr uint32_t PROTOCOL_VERSION_12 = 12;
constexpr uint32_t PROTOCOL_VERSION_13 = 13;

// Operation codes (subset for UDR)
constexpr uint32_t op_connect = 1;
constexpr uint32_t op_exit = 2;
constexpr uint32_t op_accept = 3;
constexpr uint32_t op_reject = 4;
constexpr uint32_t op_protocol = 5;
constexpr uint32_t op_disconnect = 6;
constexpr uint32_t op_response = 9;
constexpr uint32_t op_attach = 19;
constexpr uint32_t op_create = 20;
constexpr uint32_t op_detach = 21;
constexpr uint32_t op_transaction = 29;
constexpr uint32_t op_commit = 30;
constexpr uint32_t op_rollback = 31;
constexpr uint32_t op_prepare = 32;
constexpr uint32_t op_info_database = 35;
constexpr uint32_t op_info_transaction = 37;
constexpr uint32_t op_allocate_statement = 44;
constexpr uint32_t op_execute = 45;
constexpr uint32_t op_exec_immediate = 46;
constexpr uint32_t op_fetch = 47;
constexpr uint32_t op_free_statement = 48;
constexpr uint32_t op_prepare_statement = 49;
constexpr uint32_t op_info_sql = 50;
constexpr uint32_t op_insert = 51;
constexpr uint32_t op_execute2 = 52;
constexpr uint32_t op_execute_immediate2 = 53;
constexpr uint32_t op_open_blob = 70;
constexpr uint32_t op_get_segment = 71;
constexpr uint32_t op_close_blob = 72;
constexpr uint32_t op_commit_retaining = 73;
constexpr uint32_t op_rollback_retaining = 74;
constexpr uint32_t op_create_blob = 75;
constexpr uint32_t op_put_segment = 76;
constexpr uint32_t op_get_slice = 81;
constexpr uint32_t op_put_slice = 82;
constexpr uint32_t op_seek_blob = 83;
constexpr uint32_t op_cancel = 97;
constexpr uint32_t op_connect_request = 100;

// Protocol types (architectures)
constexpr uint32_t ARCHITECTURE_GENERIC = 1;

// Protocol versions understood
constexpr uint32_t PROTOCOL_VERSION_1 = 1;
constexpr uint32_t PROTOCOL_VERSION_2 = 2;

// Protocol types (ptype)
constexpr uint32_t ptype_rpc = 1;      // Simple RPC
constexpr uint32_t ptype_batch_send = 2; // Batch send
constexpr uint32_t ptype_out_of_band = 3; // Out of band
constexpr uint32_t ptype_lazy = 4;     // Lazy
constexpr uint32_t ptype_mask = 0xF;   // Type mask

// DPB (Database Parameter Block) items
constexpr uint8_t isc_dpb_version1 = 1;
constexpr uint8_t isc_dpb_user_name = 28;
constexpr uint8_t isc_dpb_password = 29;
constexpr uint8_t isc_dpb_lc_ctype = 48;
constexpr uint8_t isc_dpb_sql_role_name = 60;
constexpr uint8_t isc_dpb_trusted_auth = 71;
constexpr uint8_t isc_dpb_utf8_filename = 77;

// TPB (Transaction Parameter Block) items
constexpr uint8_t isc_tpb_version3 = 3;
constexpr uint8_t isc_tpb_write = 9;
constexpr uint8_t isc_tpb_read_committed = 15;
constexpr uint8_t isc_tpb_rec_version = 17;
constexpr uint8_t isc_tpb_no_rec_version = 18;
constexpr uint8_t isc_tpb_wait = 19;
constexpr uint8_t isc_tpb_nowait = 20;

// SQL info items
constexpr uint8_t isc_info_sql_stmt_type = 21;
constexpr uint8_t isc_info_sql_get_plan = 22;
constexpr uint8_t isc_info_sql_records = 23;
constexpr uint8_t isc_info_sql_stmt_select = 1;
constexpr uint8_t isc_info_sql_stmt_insert = 2;
constexpr uint8_t isc_info_sql_stmt_update = 3;
constexpr uint8_t isc_info_sql_stmt_delete = 4;

// Error codes
constexpr uint32_t isc_arg_end = 0;
constexpr uint32_t isc_arg_gds = 1;
constexpr uint32_t isc_arg_string = 2;
constexpr uint32_t isc_arg_cstring = 3;
constexpr uint32_t isc_arg_number = 4;
constexpr uint32_t isc_arg_interpreted = 5;
constexpr uint32_t isc_arg_vms = 6;
constexpr uint32_t isc_arg_unix = 7;
constexpr uint32_t isc_arg_domain = 8;
constexpr uint32_t isc_arg_dos = 9;
constexpr uint32_t isc_arg_mpexl = 10;
constexpr uint32_t isc_arg_mpexl_ipc = 11;
constexpr uint32_t isc_arg_next_mach = 15;
constexpr uint32_t isc_arg_netware = 16;
constexpr uint32_t isc_arg_win32 = 17;
constexpr uint32_t isc_arg_warning = 18;
constexpr uint32_t isc_arg_sql_state = 19;

} // namespace firebird

// ============================================================================
// XDR Utilities
// ============================================================================

class XDREncoder {
public:
    static std::vector<uint8_t> encodeInt32(int32_t value);
    static std::vector<uint8_t> encodeUint32(uint32_t value);
    static std::vector<uint8_t> encodeInt64(int64_t value);
    static std::vector<uint8_t> encodeString(const std::string& value);
    static std::vector<uint8_t> encodeBytes(const std::vector<uint8_t>& value);
    static std::vector<uint8_t> encodeOpaque(const void* data, size_t len);
};

class XDRDecoder {
public:
    XDRDecoder(const uint8_t* data, size_t len);
    
    bool decodeInt32(int32_t& value);
    bool decodeUint32(uint32_t& value);
    bool decodeInt64(int64_t& value);
    bool decodeString(std::string& value);
    bool decodeBytes(std::vector<uint8_t>& value);
    bool decodeOpaque(void* data, size_t len);
    
    bool atEnd() const { return pos_ >= len_; }
    size_t remaining() const { return len_ - pos_; }
    
private:
    const uint8_t* data_;
    size_t len_;
    size_t pos_;
};

// ============================================================================
// Firebird Connection
// ============================================================================

class FirebirdConnection : public PooledConnection {
public:
    FirebirdConnection();
    ~FirebirdConnection() override;

    // PooledConnection interface
    bool isOpen() const override;
    bool isValid() const override;
    core::Status ping(core::ErrorContext* ctx = nullptr) override;
    void close() override;
    
    std::string getRemoteAddress() const override;
    std::string getRemoteVersion() const override;

    // Firebird-specific
    core::Status connect(const std::string& host, uint16_t port,
                        const std::string& database,
                        const std::string& user,
                        const std::string& password,
                        const std::string& role,
                        const std::string& charset,
                        core::ErrorContext* ctx = nullptr);

    // Protocol operations
    core::Status attachDatabase(core::ErrorContext* ctx);
    core::Status detachDatabase(core::ErrorContext* ctx);
    
    // Transactions
    core::Status startTransaction(uint32_t& handle, core::ErrorContext* ctx);
    core::Status commitTransaction(uint32_t handle, core::ErrorContext* ctx);
    core::Status rollbackTransaction(uint32_t handle, core::ErrorContext* ctx);
    
    // Statement operations
    core::Status allocateStatement(uint32_t& stmt_handle, core::ErrorContext* ctx);
    core::Status prepareStatement(uint32_t stmt_handle, uint32_t txn_handle,
                                  const std::string& sql, core::ErrorContext* ctx);
    core::Status executeStatement(uint32_t stmt_handle, uint32_t txn_handle,
                                  const std::string& sql, core::ErrorContext* ctx);
    core::Status freeStatement(uint32_t stmt_handle, core::ErrorContext* ctx);
    
    // BLOB operations
    core::Status openBlob(uint64_t blob_id, uint32_t txn_handle,
                         uint32_t& blob_handle, core::ErrorContext* ctx);
    core::Status getSegment(uint32_t blob_handle, std::vector<uint8_t>& segment,
                           bool& eof, core::ErrorContext* ctx);
    core::Status closeBlob(uint32_t blob_handle, core::ErrorContext* ctx);

    // State
    uint32_t getProtocolVersion() const { return protocol_version_; }
    uint32_t getHandle() const { return db_handle_; }

private:
    // Network
    int socket_fd_ = -1;
    std::string host_;
    uint16_t port_ = 3050;
    
    // Connection state
    uint32_t db_handle_ = 0;
    uint32_t protocol_version_ = firebird::PROTOCOL_VERSION_10;
    uint32_t architecture_ = firebird::ARCHITECTURE_GENERIC;
    
    // Configuration
    std::string database_;
    std::string user_;
    std::string password_;
    std::string role_;
    std::string charset_;
    
    // Internal methods
    core::Status doConnect(core::ErrorContext* ctx);
    core::Status doProtocol(core::ErrorContext* ctx);
    
    // Packet I/O
    core::Status sendOp(uint32_t op, core::ErrorContext* ctx);
    core::Status readPacket(std::vector<uint8_t>& packet, core::ErrorContext* ctx);
    core::Status writePacket(const std::vector<uint8_t>& packet, core::ErrorContext* ctx);
    
    core::Status readExactly(void* buffer, size_t len, core::ErrorContext* ctx);
    core::Status writeExactly(const void* buffer, size_t len, core::ErrorContext* ctx);
    
    // Response handling
    core::Status readResponse(uint32_t& handle, std::vector<uint8_t>& data,
                             core::ErrorContext* ctx);
    core::Status parseStatusVector(core::ErrorContext* ctx);
    
    // Utility
    void cleanupSocket();
    std::vector<uint8_t> buildDPB() const;
    std::vector<uint8_t> buildTPB() const;
};

// ============================================================================
// Firebird Connection Factory
// ============================================================================

class FirebirdConnectionFactory : public ConnectionFactory {
public:
    FirebirdConnectionFactory(const UDRServerConfig& config);
    
    std::unique_ptr<PooledConnection> createConnection(
        core::ErrorContext* ctx = nullptr) override;
    
    bool validateConnection(PooledConnection* conn,
                           core::ErrorContext* ctx = nullptr) override;
    
    void destroyConnection(PooledConnection* conn) override;

private:
    UDRServerConfig config_;
};

// ============================================================================
// Firebird UDR Connector
// ============================================================================

class FirebirdUDRConnector : public UDRConnector {
public:
    FirebirdUDRConnector();
    ~FirebirdUDRConnector() override;

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

    ConnectorType getType() const override { return ConnectorType::FIREBIRD; }
    std::string getVersion() const override;
    std::string getRemoteVersion() const override;
    std::vector<std::string> getSupportedFeatures() const override;

private:
    UDRServerConfig config_;
    std::unique_ptr<ConnectionPool> pool_;
    std::string remote_version_;
    uint32_t active_transaction_ = 0;
    
    // Prepared statement tracking
    std::unordered_map<std::string, uint32_t> prepared_statements_;
    mutable std::mutex prepared_mutex_;
    
    // Helper methods
    std::unique_ptr<FirebirdConnection> acquireConnection(core::ErrorContext* ctx);
    void releaseConnection(std::unique_ptr<FirebirdConnection> conn);
};

} // namespace udr
} // namespace scratchbird
