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
 * @file mysql_adapter.cpp
 * @brief MySQL Protocol Adapter Implementation
 *
 * Implements the MySQL wire protocol client for connecting to
 * remote MySQL/MariaDB databases.
 *
 * Protocol reference: https://dev.mysql.com/doc/internals/en/client-server-protocol.html
 *
 * Part of Phase 3.7: UDR Plugin System
 */

#include "scratchbird/fdw/mysql_adapter.h"

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <netinet/tcp.h>
    #include <sys/socket.h>
#endif
#include "scratchbird/core/posix_compat.h"

#include <algorithm>
#include <cstring>
#include <functional>
#include <sstream>

// For SHA1
#include <openssl/sha.h>

namespace scratchbird {
namespace fdw {

// =============================================================================
// MySQL Protocol Constants
// =============================================================================

namespace mysql_protocol {
    // Capability flags
    constexpr uint32_t CLIENT_LONG_PASSWORD     = 0x00000001;
    constexpr uint32_t CLIENT_FOUND_ROWS        = 0x00000002;
    constexpr uint32_t CLIENT_LONG_FLAG         = 0x00000004;
    constexpr uint32_t CLIENT_CONNECT_WITH_DB   = 0x00000008;
    constexpr uint32_t CLIENT_PROTOCOL_41       = 0x00000200;
    constexpr uint32_t CLIENT_TRANSACTIONS      = 0x00002000;
    constexpr uint32_t CLIENT_SECURE_CONNECTION = 0x00008000;
    constexpr uint32_t CLIENT_PLUGIN_AUTH       = 0x00080000;
    constexpr uint32_t CLIENT_DEPRECATE_EOF     = 0x01000000;

    // Command codes
    constexpr uint8_t COM_QUIT          = 0x01;
    constexpr uint8_t COM_INIT_DB       = 0x02;
    constexpr uint8_t COM_QUERY         = 0x03;
    constexpr uint8_t COM_FIELD_LIST    = 0x04;
    constexpr uint8_t COM_STMT_PREPARE  = 0x16;
    constexpr uint8_t COM_STMT_EXECUTE  = 0x17;
    constexpr uint8_t COM_STMT_CLOSE    = 0x19;
    constexpr uint8_t COM_STMT_RESET    = 0x1A;
    constexpr uint8_t COM_PING          = 0x0E;
    constexpr uint8_t COM_RESET_CONNECTION = 0x1F;

    // Packet types
    constexpr uint8_t OK_PACKET         = 0x00;
    constexpr uint8_t EOF_PACKET        = 0xFE;
    constexpr uint8_t ERR_PACKET        = 0xFF;
    constexpr uint8_t LOCAL_INFILE      = 0xFB;

    // Column types
    constexpr uint8_t MYSQL_TYPE_DECIMAL    = 0x00;
    constexpr uint8_t MYSQL_TYPE_TINY       = 0x01;
    constexpr uint8_t MYSQL_TYPE_SHORT      = 0x02;
    constexpr uint8_t MYSQL_TYPE_LONG       = 0x03;
    constexpr uint8_t MYSQL_TYPE_FLOAT      = 0x04;
    constexpr uint8_t MYSQL_TYPE_DOUBLE     = 0x05;
    constexpr uint8_t MYSQL_TYPE_NULL       = 0x06;
    constexpr uint8_t MYSQL_TYPE_TIMESTAMP  = 0x07;
    constexpr uint8_t MYSQL_TYPE_LONGLONG   = 0x08;
    constexpr uint8_t MYSQL_TYPE_INT24      = 0x09;
    constexpr uint8_t MYSQL_TYPE_DATE       = 0x0A;
    constexpr uint8_t MYSQL_TYPE_TIME       = 0x0B;
    constexpr uint8_t MYSQL_TYPE_DATETIME   = 0x0C;
    constexpr uint8_t MYSQL_TYPE_YEAR       = 0x0D;
    constexpr uint8_t MYSQL_TYPE_VARCHAR    = 0x0F;
    constexpr uint8_t MYSQL_TYPE_BIT        = 0x10;
    constexpr uint8_t MYSQL_TYPE_JSON       = 0xF5;
    constexpr uint8_t MYSQL_TYPE_NEWDECIMAL = 0xF6;
    constexpr uint8_t MYSQL_TYPE_BLOB       = 0xFC;
    constexpr uint8_t MYSQL_TYPE_VAR_STRING = 0xFD;
    constexpr uint8_t MYSQL_TYPE_STRING     = 0xFE;
}

// =============================================================================
// Implementation Details
// =============================================================================

struct MySQLAdapter::Impl {
    int socket_fd = -1;
    bool connected = false;
    uint8_t sequence_id = 0;
    uint32_t server_capabilities = 0;
    uint32_t client_capabilities = 0;
    std::string server_version;
    std::string current_database;

    // Prepared statements: local id -> server statement id
    uint64_t next_local_stmt_id = 1;
    std::unordered_map<uint64_t, uint32_t> prepared_statements;

    // Buffer
    std::vector<uint8_t> read_buffer;

    Impl() : read_buffer(65536) {}
};

// =============================================================================
// MySQLAdapter Implementation
// =============================================================================

MySQLAdapter::MySQLAdapter()
    : ProtocolAdapterBase()
    , impl_(std::make_unique<Impl>())
{
}

MySQLAdapter::~MySQLAdapter() {
    disconnect();
}

Result<void> MySQLAdapter::connect(const ServerDefinition& server,
                                    const UserMapping& mapping)
{
    if (impl_->connected) {
        disconnect();
    }

    setState(ConnectionState::CONNECTING);

    // Resolve hostname
    struct addrinfo hints{}, *result;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    uint16_t port = server.port ? server.port : 3306;
    std::string port_str = std::to_string(port);

    int err = getaddrinfo(server.host.c_str(), port_str.c_str(), &hints, &result);
    if (err != 0) {
        setState(ConnectionState::FAILED);
        return makeError(core::Status::IO_ERROR,
                         "Failed to resolve hostname: " + std::string(gai_strerror(err)));
    }

    // Create socket
    impl_->socket_fd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (impl_->socket_fd < 0) {
        freeaddrinfo(result);
        setState(ConnectionState::FAILED);
        return makeError(core::Status::IO_ERROR, "Failed to create socket");
    }

    // Set TCP_NODELAY
    int flag = 1;
    setsockopt(impl_->socket_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    // Set timeout
    struct timeval timeout;
    timeout.tv_sec = server.connection_timeout_ms / 1000;
    timeout.tv_usec = (server.connection_timeout_ms % 1000) * 1000;
    setsockopt(impl_->socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    setsockopt(impl_->socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    // Connect
    if (::connect(impl_->socket_fd, result->ai_addr, result->ai_addrlen) < 0) {
        freeaddrinfo(result);
        close(impl_->socket_fd);
        impl_->socket_fd = -1;
        setState(ConnectionState::FAILED);
        return makeError(core::Status::IO_ERROR, "Failed to connect to server");
    }

    freeaddrinfo(result);
    setState(ConnectionState::AUTHENTICATING);

    // Read initial handshake packet
    auto handshake = readPacket();
    if (!handshake) {
        close(impl_->socket_fd);
        impl_->socket_fd = -1;
        setState(ConnectionState::FAILED);
        return makeError(handshake.errorCode(), handshake.errorMessage());
    }

    // Parse handshake
    const auto& data = *handshake;
    if (data.empty() || data[0] == mysql_protocol::ERR_PACKET) {
        close(impl_->socket_fd);
        impl_->socket_fd = -1;
        setState(ConnectionState::FAILED);
        return makeError(core::Status::INVALID_AUTHORIZATION, "Server returned error packet");
    }

    // Protocol version (1 byte)
    uint8_t protocol_version = data[0];
    if (protocol_version != 10) {
        close(impl_->socket_fd);
        impl_->socket_fd = -1;
        setState(ConnectionState::FAILED);
        return makeError(core::Status::PROTOCOL_VIOLATION,
                         "Unsupported protocol version: " + std::to_string(protocol_version));
    }

    // Server version (null-terminated string)
    size_t offset = 1;
    while (offset < data.size() && data[offset] != 0) {
        offset++;
    }
    impl_->server_version = std::string(data.begin() + 1, data.begin() + offset);
    setServerVersion(impl_->server_version);
    offset++;

    // Skip connection id (4 bytes)
    offset += 4;

    // Auth-plugin-data-part-1 (8 bytes)
    std::vector<uint8_t> auth_data;
    if (offset + 8 <= data.size()) {
        auth_data.insert(auth_data.end(), data.begin() + offset, data.begin() + offset + 8);
        offset += 8;
    }

    // Filler (1 byte)
    offset += 1;

    // Capability flags (lower 2 bytes)
    if (offset + 2 <= data.size()) {
        impl_->server_capabilities =
            static_cast<uint32_t>(data[offset]) |
            (static_cast<uint32_t>(data[offset + 1]) << 8);
        offset += 2;
    }

    // Character set, status flags, capability flags (upper 2 bytes)
    if (offset + 5 <= data.size()) {
        offset += 3;  // charset + status
        impl_->server_capabilities |=
            (static_cast<uint32_t>(data[offset]) << 16) |
            (static_cast<uint32_t>(data[offset + 1]) << 24);
        offset += 2;
    }

    // Auth plugin data length (1 byte)
    uint8_t auth_data_len = 0;
    if (offset < data.size()) {
        auth_data_len = data[offset];
        offset++;
    }

    // Reserved (10 bytes)
    offset += 10;

    // Auth-plugin-data-part-2
    if (auth_data_len > 8 && offset + (auth_data_len - 8) <= data.size()) {
        auth_data.insert(auth_data.end(),
                         data.begin() + offset,
                         data.begin() + offset + auth_data_len - 8 - 1);  // -1 for null terminator
    }

    // Send handshake response
    auto response_result = sendHandshakeResponse(server, mapping, auth_data);
    if (!response_result) {
        close(impl_->socket_fd);
        impl_->socket_fd = -1;
        setState(ConnectionState::FAILED);
        return response_result;
    }

    // Read response
    auto auth_response = readPacket();
    if (!auth_response) {
        close(impl_->socket_fd);
        impl_->socket_fd = -1;
        setState(ConnectionState::FAILED);
        return makeError(auth_response.errorCode(), auth_response.errorMessage());
    }

    if (!auth_response->empty() && (*auth_response)[0] == mysql_protocol::ERR_PACKET) {
        std::string error_msg = "Authentication failed";
        if (auth_response->size() > 9) {
            error_msg = std::string(auth_response->begin() + 9, auth_response->end());
        }
        close(impl_->socket_fd);
        impl_->socket_fd = -1;
        setState(ConnectionState::FAILED);
        return makeError(core::Status::INVALID_AUTHORIZATION, error_msg);
    }

    impl_->connected = true;
    impl_->current_database = server.database;
    setState(ConnectionState::CONNECTED);
    return Result<void>();
}

Result<void> MySQLAdapter::disconnect() {
    if (impl_->socket_fd >= 0) {
        // Send COM_QUIT
        std::vector<uint8_t> quit_data = {mysql_protocol::COM_QUIT};
        writePacket(quit_data);

        close(impl_->socket_fd);
        impl_->socket_fd = -1;
    }

    impl_->connected = false;
    impl_->prepared_statements.clear();
    impl_->sequence_id = 0;
    setState(ConnectionState::DISCONNECTED);
    return Result<void>();
}

Result<bool> MySQLAdapter::ping() {
    if (!impl_->connected || impl_->socket_fd < 0) {
        return Result<bool>(false);
    }

    std::vector<uint8_t> ping_data = {mysql_protocol::COM_PING};
    auto result = sendCommand(mysql_protocol::COM_PING, {});

    if (!result) {
        return Result<bool>(false);
    }

    auto response = readPacket();
    if (!response || response->empty()) {
        return Result<bool>(false);
    }

    return Result<bool>((*response)[0] == mysql_protocol::OK_PACKET);
}

Result<void> MySQLAdapter::reset() {
    if (!impl_->connected) {
        return makeError(core::Status::INVALID_TRANSACTION_STATE, "Not connected");
    }

    auto result = sendCommand(mysql_protocol::COM_RESET_CONNECTION, {});
    if (!result) {
        return result;
    }

    auto response = readPacket();
    if (!response || response->empty() || (*response)[0] != mysql_protocol::OK_PACKET) {
        return makeError(core::Status::IO_ERROR, "Reset failed");
    }

    return Result<void>();
}

Result<RemoteQueryResult> MySQLAdapter::executeQuery(const std::string& sql) {
    if (!impl_->connected) {
        return makeError<RemoteQueryResult>(core::Status::INVALID_TRANSACTION_STATE, "Not connected");
    }

    recordQueryStart();

    auto send_result = sendQuery(sql);
    if (!send_result) {
        recordQueryEnd(false);
        return makeError<RemoteQueryResult>(send_result.errorCode(), send_result.errorMessage());
    }

    auto result = readQueryResult();
    if (!result) {
        recordQueryEnd(false);
        return result;
    }

    recordQueryEnd(true, result->rows.size());
    return result;
}

Result<RemoteQueryResult> MySQLAdapter::executeQueryWithParams(
    const std::string& sql,
    const std::vector<RemoteValue>& params)
{
    // MySQL requires prepared statements for parameters
    auto stmt_result = prepare(sql);
    if (!stmt_result) {
        return makeError<RemoteQueryResult>(stmt_result.errorCode(), stmt_result.errorMessage());
    }

    uint64_t stmt_id = *stmt_result;
    auto exec_result = executePrepared(stmt_id, params);
    deallocatePrepared(stmt_id);

    return exec_result;
}

Result<uint64_t> MySQLAdapter::prepare(const std::string& sql) {
    if (!impl_->connected) {
        return makeError<uint64_t>(core::Status::INVALID_TRANSACTION_STATE, "Not connected");
    }

    std::vector<uint8_t> data;
    data.push_back(mysql_protocol::COM_STMT_PREPARE);
    data.insert(data.end(), sql.begin(), sql.end());

    impl_->sequence_id = 0;
    auto write_result = writePacket(data);
    if (!write_result) {
        return makeError<uint64_t>(write_result.errorCode(), write_result.errorMessage());
    }

    // Read response
    auto response = readPacket();
    if (!response || response->empty()) {
        return makeError<uint64_t>(core::Status::IO_ERROR, "No response from server");
    }

    if ((*response)[0] == mysql_protocol::ERR_PACKET) {
        std::string error_msg = "Prepare failed";
        if (response->size() > 9) {
            error_msg = std::string(response->begin() + 9, response->end());
        }
        return makeError<uint64_t>(core::Status::SYNTAX_ERROR, error_msg);
    }

    // Parse OK response
    if (response->size() < 12) {
        return makeError<uint64_t>(core::Status::PROTOCOL_VIOLATION, "Invalid prepare response");
    }

    uint32_t server_stmt_id =
        static_cast<uint32_t>((*response)[1]) |
        (static_cast<uint32_t>((*response)[2]) << 8) |
        (static_cast<uint32_t>((*response)[3]) << 16) |
        (static_cast<uint32_t>((*response)[4]) << 24);

    uint16_t num_columns =
        static_cast<uint16_t>((*response)[5]) |
        (static_cast<uint16_t>((*response)[6]) << 8);

    uint16_t num_params =
        static_cast<uint16_t>((*response)[7]) |
        (static_cast<uint16_t>((*response)[8]) << 8);

    // Read parameter definitions (if any)
    for (uint16_t i = 0; i < num_params; i++) {
        auto param_def = readPacket();
        if (!param_def) break;
    }
    if (num_params > 0) {
        readPacket();  // EOF packet
    }

    // Read column definitions (if any)
    for (uint16_t i = 0; i < num_columns; i++) {
        auto col_def = readPacket();
        if (!col_def) break;
    }
    if (num_columns > 0) {
        readPacket();  // EOF packet
    }

    uint64_t local_id = impl_->next_local_stmt_id++;
    impl_->prepared_statements[local_id] = server_stmt_id;
    return Result<uint64_t>(local_id);
}

Result<RemoteQueryResult> MySQLAdapter::executePrepared(
    uint64_t stmt_id,
    const std::vector<RemoteValue>& params)
{
    if (!impl_->connected) {
        return makeError<RemoteQueryResult>(core::Status::INVALID_TRANSACTION_STATE, "Not connected");
    }

    auto it = impl_->prepared_statements.find(stmt_id);
    if (it == impl_->prepared_statements.end()) {
        return makeError<RemoteQueryResult>(core::Status::NOT_FOUND, "Statement not found");
    }

    recordQueryStart();

    uint32_t server_stmt_id = it->second;

    // Build COM_STMT_EXECUTE packet
    std::vector<uint8_t> data;
    data.push_back(mysql_protocol::COM_STMT_EXECUTE);

    // Statement ID (4 bytes, little-endian)
    data.push_back(server_stmt_id & 0xFF);
    data.push_back((server_stmt_id >> 8) & 0xFF);
    data.push_back((server_stmt_id >> 16) & 0xFF);
    data.push_back((server_stmt_id >> 24) & 0xFF);

    // Flags (1 byte)
    data.push_back(0x00);

    // Iteration count (4 bytes)
    data.push_back(0x01);
    data.push_back(0x00);
    data.push_back(0x00);
    data.push_back(0x00);

    // NULL bitmap and parameters (simplified - would need proper encoding)
    if (!params.empty()) {
        // Calculate NULL bitmap size
        size_t null_bitmap_size = (params.size() + 7) / 8;
        std::vector<uint8_t> null_bitmap(null_bitmap_size, 0);

        for (size_t i = 0; i < params.size(); i++) {
            if (isNull(params[i])) {
                null_bitmap[i / 8] |= (1 << (i % 8));
            }
        }

        data.insert(data.end(), null_bitmap.begin(), null_bitmap.end());

        // New-params-bound flag
        data.push_back(0x01);

        // Parameter types (2 bytes each)
        for (const auto& param : params) {
            uint8_t mysql_type = mysql_protocol::MYSQL_TYPE_VAR_STRING;
            data.push_back(mysql_type);
            data.push_back(0x00);  // unsigned flag
        }

        // Parameter values
        for (const auto& param : params) {
            if (!isNull(param)) {
                auto bytes = convertToRemote(param, 0);
                // Length-encoded string
                if (bytes.size() < 251) {
                    data.push_back(static_cast<uint8_t>(bytes.size()));
                } else if (bytes.size() < 65536) {
                    data.push_back(0xFC);
                    data.push_back(bytes.size() & 0xFF);
                    data.push_back((bytes.size() >> 8) & 0xFF);
                }
                data.insert(data.end(), bytes.begin(), bytes.end());
            }
        }
    }

    impl_->sequence_id = 0;
    auto write_result = writePacket(data);
    if (!write_result) {
        recordQueryEnd(false);
        return makeError<RemoteQueryResult>(write_result.errorCode(), write_result.errorMessage());
    }

    auto result = readQueryResult();
    if (!result) {
        recordQueryEnd(false);
        return result;
    }

    recordQueryEnd(true, result->rows.size());
    return result;
}

Result<void> MySQLAdapter::deallocatePrepared(uint64_t stmt_id) {
    auto it = impl_->prepared_statements.find(stmt_id);
    if (it == impl_->prepared_statements.end()) {
        return Result<void>();
    }

    uint32_t server_stmt_id = it->second;

    std::vector<uint8_t> data;
    data.push_back(mysql_protocol::COM_STMT_CLOSE);
    data.push_back(server_stmt_id & 0xFF);
    data.push_back((server_stmt_id >> 8) & 0xFF);
    data.push_back((server_stmt_id >> 16) & 0xFF);
    data.push_back((server_stmt_id >> 24) & 0xFF);

    impl_->sequence_id = 0;
    writePacket(data);  // No response for COM_STMT_CLOSE

    impl_->prepared_statements.erase(it);
    return Result<void>();
}

Result<void> MySQLAdapter::beginTransaction() {
    auto result = executeQuery("START TRANSACTION");
    if (result) {
        setState(ConnectionState::IN_TRANSACTION);
    }
    return result.ok() ? Result<void>() : makeError(result.errorCode(), result.errorMessage());
}

Result<void> MySQLAdapter::commit() {
    auto result = executeQuery("COMMIT");
    if (result) {
        setState(ConnectionState::CONNECTED);
    }
    return result.ok() ? Result<void>() : makeError(result.errorCode(), result.errorMessage());
}

Result<void> MySQLAdapter::rollback() {
    auto result = executeQuery("ROLLBACK");
    if (result) {
        setState(ConnectionState::CONNECTED);
    }
    return result.ok() ? Result<void>() : makeError(result.errorCode(), result.errorMessage());
}

Result<void> MySQLAdapter::setSavepoint(const std::string& name) {
    auto result = executeQuery("SAVEPOINT " + name);
    return result.ok() ? Result<void>() : makeError(result.errorCode(), result.errorMessage());
}

Result<void> MySQLAdapter::rollbackToSavepoint(const std::string& name) {
    auto result = executeQuery("ROLLBACK TO SAVEPOINT " + name);
    return result.ok() ? Result<void>() : makeError(result.errorCode(), result.errorMessage());
}

Result<std::string> MySQLAdapter::declareCursor(const std::string& name,
                                                 const std::string& sql)
{
    // MySQL doesn't support SQL cursors outside of stored procedures
    return makeError<std::string>(core::Status::NOT_IMPLEMENTED,
                                   "MySQL cursors only supported in stored procedures");
}

Result<RemoteQueryResult> MySQLAdapter::fetchFromCursor(const std::string& name,
                                                         uint32_t count)
{
    return makeError<RemoteQueryResult>(core::Status::NOT_IMPLEMENTED,
                                         "MySQL cursors only supported in stored procedures");
}

Result<void> MySQLAdapter::closeCursor(const std::string& name) {
    return makeError(core::Status::NOT_IMPLEMENTED,
                     "MySQL cursors only supported in stored procedures");
}

Result<std::vector<std::string>> MySQLAdapter::listSchemas() {
    auto result = executeQuery("SHOW DATABASES");
    if (!result) {
        return makeError<std::vector<std::string>>(result.errorCode(), result.errorMessage());
    }

    std::vector<std::string> schemas;
    for (const auto& row : result->rows) {
        if (!row.empty() && std::holds_alternative<std::string>(row[0])) {
            schemas.push_back(std::get<std::string>(row[0]));
        }
    }
    return Result<std::vector<std::string>>(std::move(schemas));
}

Result<std::vector<std::string>> MySQLAdapter::listTables(const std::string& schema) {
    std::string query = "SHOW TABLES";
    if (!schema.empty()) {
        query += " FROM `" + schema + "`";
    }

    auto result = executeQuery(query);
    if (!result) {
        return makeError<std::vector<std::string>>(result.errorCode(), result.errorMessage());
    }

    std::vector<std::string> tables;
    for (const auto& row : result->rows) {
        if (!row.empty() && std::holds_alternative<std::string>(row[0])) {
            tables.push_back(std::get<std::string>(row[0]));
        }
    }
    return Result<std::vector<std::string>>(std::move(tables));
}

Result<std::vector<RemoteColumnDesc>> MySQLAdapter::describeTable(
    const std::string& schema,
    const std::string& table)
{
    std::string query = "DESCRIBE `" + schema + "`.`" + table + "`";

    auto result = executeQuery(query);
    if (!result) {
        return makeError<std::vector<RemoteColumnDesc>>(result.errorCode(), result.errorMessage());
    }

    std::vector<RemoteColumnDesc> columns;
    for (const auto& row : result->rows) {
        if (row.size() >= 3) {
            RemoteColumnDesc col;
            if (std::holds_alternative<std::string>(row[0])) {
                col.name = std::get<std::string>(row[0]);
            }
            if (std::holds_alternative<std::string>(row[2])) {
                col.nullable = std::get<std::string>(row[2]) == "YES";
            }
            columns.push_back(col);
        }
    }
    return Result<std::vector<RemoteColumnDesc>>(std::move(columns));
}

Result<std::vector<RemoteIndexDesc>> MySQLAdapter::describeIndexes(
    const std::string& schema,
    const std::string& table)
{
    std::string query = "SHOW INDEX FROM `" + schema + "`.`" + table + "`";

    auto result = executeQuery(query);
    if (!result) {
        return makeError<std::vector<RemoteIndexDesc>>(result.errorCode(), result.errorMessage());
    }

    std::vector<RemoteIndexDesc> indexes;
    // Parse results - simplified
    return Result<std::vector<RemoteIndexDesc>>(std::move(indexes));
}

Result<std::vector<RemoteForeignKey>> MySQLAdapter::describeForeignKeys(
    const std::string& schema,
    const std::string& table)
{
    std::string query =
        "SELECT CONSTRAINT_NAME, COLUMN_NAME, REFERENCED_TABLE_NAME, REFERENCED_COLUMN_NAME "
        "FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "
        "WHERE TABLE_SCHEMA = '" + schema + "' AND TABLE_NAME = '" + table + "' "
        "AND REFERENCED_TABLE_NAME IS NOT NULL";

    auto result = executeQuery(query);
    if (!result) {
        return makeError<std::vector<RemoteForeignKey>>(result.errorCode(), result.errorMessage());
    }

    std::vector<RemoteForeignKey> fks;
    // Parse results - simplified
    return Result<std::vector<RemoteForeignKey>>(std::move(fks));
}

PushdownCapability MySQLAdapter::getCapabilities() const {
    return PushdownCapability::STANDARD;
}

uint32_t MySQLAdapter::mapRemoteType(uint32_t remote_oid, int32_t modifier) const {
    static const std::unordered_map<uint8_t, uint32_t>& type_map = getTypeMap();
    auto it = type_map.find(static_cast<uint8_t>(remote_oid));
    if (it != type_map.end()) {
        return it->second;
    }
    return 0;
}

RemoteValue MySQLAdapter::convertToLocal(const void* data, size_t len,
                                          uint32_t remote_oid) const
{
    if (data == nullptr || len == 0) {
        return std::monostate{};
    }

    const char* str = static_cast<const char*>(data);
    std::string val(str, len);

    switch (static_cast<uint8_t>(remote_oid)) {
        case mysql_protocol::MYSQL_TYPE_TINY:
        case mysql_protocol::MYSQL_TYPE_SHORT:
        case mysql_protocol::MYSQL_TYPE_LONG:
        case mysql_protocol::MYSQL_TYPE_INT24:
            return static_cast<int32_t>(std::stoi(val));

        case mysql_protocol::MYSQL_TYPE_LONGLONG:
            return static_cast<int64_t>(std::stoll(val));

        case mysql_protocol::MYSQL_TYPE_FLOAT:
            return std::stof(val);

        case mysql_protocol::MYSQL_TYPE_DOUBLE:
        case mysql_protocol::MYSQL_TYPE_DECIMAL:
        case mysql_protocol::MYSQL_TYPE_NEWDECIMAL:
            return std::stod(val);

        default:
            return val;
    }
}

std::vector<uint8_t> MySQLAdapter::convertToRemote(const RemoteValue& value,
                                                    uint32_t remote_oid) const
{
    std::vector<uint8_t> result;

    std::visit([&result](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            // NULL
        } else if constexpr (std::is_same_v<T, bool>) {
            result.push_back(arg ? '1' : '0');
        } else if constexpr (std::is_same_v<T, int16_t> || std::is_same_v<T, int32_t> ||
                            std::is_same_v<T, int64_t>) {
            auto s = std::to_string(arg);
            result.insert(result.end(), s.begin(), s.end());
        } else if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>) {
            auto s = std::to_string(arg);
            result.insert(result.end(), s.begin(), s.end());
        } else if constexpr (std::is_same_v<T, std::string>) {
            result.insert(result.end(), arg.begin(), arg.end());
        } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
            result = arg;
        }
    }, value);

    return result;
}

const std::unordered_map<uint8_t, uint32_t>& MySQLAdapter::getTypeMap() {
    static const std::unordered_map<uint8_t, uint32_t> map = {
        {mysql_protocol::MYSQL_TYPE_TINY, 2},       // SMALLINT
        {mysql_protocol::MYSQL_TYPE_SHORT, 2},      // SMALLINT
        {mysql_protocol::MYSQL_TYPE_LONG, 3},       // INTEGER
        {mysql_protocol::MYSQL_TYPE_INT24, 3},      // INTEGER
        {mysql_protocol::MYSQL_TYPE_LONGLONG, 4},   // BIGINT
        {mysql_protocol::MYSQL_TYPE_FLOAT, 5},      // REAL
        {mysql_protocol::MYSQL_TYPE_DOUBLE, 6},     // DOUBLE
        {mysql_protocol::MYSQL_TYPE_DECIMAL, 7},    // DECIMAL
        {mysql_protocol::MYSQL_TYPE_NEWDECIMAL, 7}, // DECIMAL
        {mysql_protocol::MYSQL_TYPE_VARCHAR, 8},    // VARCHAR
        {mysql_protocol::MYSQL_TYPE_VAR_STRING, 8}, // VARCHAR
        {mysql_protocol::MYSQL_TYPE_STRING, 10},    // CHAR
        {mysql_protocol::MYSQL_TYPE_BLOB, 11},      // BLOB
        {mysql_protocol::MYSQL_TYPE_DATE, 12},      // DATE
        {mysql_protocol::MYSQL_TYPE_TIME, 13},      // TIME
        {mysql_protocol::MYSQL_TYPE_DATETIME, 14},  // TIMESTAMP
        {mysql_protocol::MYSQL_TYPE_TIMESTAMP, 14}, // TIMESTAMP
        {mysql_protocol::MYSQL_TYPE_JSON, 17},      // JSON
    };
    return map;
}

// =============================================================================
// Protocol Helpers
// =============================================================================

Result<void> MySQLAdapter::sendHandshakeResponse(const ServerDefinition& server,
                                                  const UserMapping& mapping,
                                                  const std::vector<uint8_t>& auth_data)
{
    // Calculate capabilities
    impl_->client_capabilities =
        mysql_protocol::CLIENT_LONG_PASSWORD |
        mysql_protocol::CLIENT_FOUND_ROWS |
        mysql_protocol::CLIENT_LONG_FLAG |
        mysql_protocol::CLIENT_CONNECT_WITH_DB |
        mysql_protocol::CLIENT_PROTOCOL_41 |
        mysql_protocol::CLIENT_TRANSACTIONS |
        mysql_protocol::CLIENT_SECURE_CONNECTION;

    std::vector<uint8_t> data;

    // Capabilities (4 bytes)
    data.push_back(impl_->client_capabilities & 0xFF);
    data.push_back((impl_->client_capabilities >> 8) & 0xFF);
    data.push_back((impl_->client_capabilities >> 16) & 0xFF);
    data.push_back((impl_->client_capabilities >> 24) & 0xFF);

    // Max packet size (4 bytes)
    uint32_t max_packet = 16777216;
    data.push_back(max_packet & 0xFF);
    data.push_back((max_packet >> 8) & 0xFF);
    data.push_back((max_packet >> 16) & 0xFF);
    data.push_back((max_packet >> 24) & 0xFF);

    // Character set (1 byte) - UTF-8
    data.push_back(33);

    // Reserved (23 bytes)
    for (int i = 0; i < 23; i++) {
        data.push_back(0);
    }

    // Username (null-terminated)
    for (char c : mapping.remote_user) {
        data.push_back(static_cast<uint8_t>(c));
    }
    data.push_back(0);

    // Auth response
    auto scrambled = scramblePassword(mapping.remote_password, auth_data);
    data.push_back(static_cast<uint8_t>(scrambled.size()));
    data.insert(data.end(), scrambled.begin(), scrambled.end());

    // Database (null-terminated)
    for (char c : server.database) {
        data.push_back(static_cast<uint8_t>(c));
    }
    data.push_back(0);

    return writePacket(data);
}

Result<void> MySQLAdapter::sendCommand(uint8_t command, const std::vector<uint8_t>& data) {
    std::vector<uint8_t> packet;
    packet.push_back(command);
    packet.insert(packet.end(), data.begin(), data.end());

    impl_->sequence_id = 0;
    return writePacket(packet);
}

Result<void> MySQLAdapter::sendQuery(const std::string& sql) {
    std::vector<uint8_t> data;
    data.push_back(mysql_protocol::COM_QUERY);
    data.insert(data.end(), sql.begin(), sql.end());

    impl_->sequence_id = 0;
    return writePacket(data);
}

Result<RemoteQueryResult> MySQLAdapter::readQueryResult() {
    RemoteQueryResult result;
    result.success = false;

    auto first_packet = readPacket();
    if (!first_packet || first_packet->empty()) {
        result.error_message = "No response from server";
        return Result<RemoteQueryResult>(std::move(result));
    }

    uint8_t first_byte = (*first_packet)[0];

    // OK packet
    if (first_byte == mysql_protocol::OK_PACKET) {
        result.success = true;
        // Parse affected rows from OK packet
        if (first_packet->size() > 1) {
            // Length-encoded integer for affected rows
            size_t offset = 1;
            if ((*first_packet)[offset] < 251) {
                result.rows_affected = (*first_packet)[offset];
            }
        }
        return Result<RemoteQueryResult>(std::move(result));
    }

    // Error packet
    if (first_byte == mysql_protocol::ERR_PACKET) {
        if (first_packet->size() > 3) {
            uint16_t error_code =
                static_cast<uint16_t>((*first_packet)[1]) |
                (static_cast<uint16_t>((*first_packet)[2]) << 8);

            size_t msg_start = 3;
            if (first_packet->size() > 9 && (*first_packet)[3] == '#') {
                // SQL state
                result.sql_state = std::string(first_packet->begin() + 4,
                                               first_packet->begin() + 9);
                msg_start = 9;
            }

            result.error_message = std::string(first_packet->begin() + msg_start,
                                               first_packet->end());
        } else {
            result.error_message = "Unknown error";
        }
        return Result<RemoteQueryResult>(std::move(result));
    }

    // Result set - first packet is column count
    uint64_t column_count = 0;
    if (first_byte < 251) {
        column_count = first_byte;
    } else if (first_byte == 0xFC && first_packet->size() >= 3) {
        column_count =
            static_cast<uint64_t>((*first_packet)[1]) |
            (static_cast<uint64_t>((*first_packet)[2]) << 8);
    }

    // Read column definitions
    for (uint64_t i = 0; i < column_count; i++) {
        auto col_packet = readPacket();
        if (!col_packet) break;

        RemoteColumnDesc col;
        // Parse column definition packet
        const auto& data = *col_packet;
        size_t offset = 0;

        // Skip catalog (length-encoded string)
        auto skipLenEncString = [&data, &offset]() {
            if (offset >= data.size()) return;
            uint8_t len = data[offset++];
            if (len < 251) {
                offset += len;
            }
        };

        skipLenEncString();  // catalog
        skipLenEncString();  // schema
        skipLenEncString();  // table alias
        skipLenEncString();  // table name

        // Column name
        if (offset < data.size()) {
            uint8_t name_len = data[offset++];
            if (offset + name_len <= data.size()) {
                col.name = std::string(data.begin() + offset, data.begin() + offset + name_len);
                offset += name_len;
            }
        }

        skipLenEncString();  // original column name

        // Fixed-length fields
        if (offset + 12 <= data.size()) {
            offset++;  // length of fixed fields
            offset += 2;  // character set
            offset += 4;  // max length
            col.type_oid = data[offset];
            offset++;
            // flags (2 bytes)
            // decimals (1 byte)
            col.nullable = true;  // Simplified
        }

        col.mapped_type_id = mapRemoteType(col.type_oid, -1);
        result.columns.push_back(col);
    }

    // Read EOF packet after columns
    auto eof_packet = readPacket();

    // Read rows
    while (true) {
        auto row_packet = readPacket();
        if (!row_packet) break;

        if (row_packet->empty()) continue;

        // Check for EOF or error
        if ((*row_packet)[0] == mysql_protocol::EOF_PACKET ||
            (*row_packet)[0] == mysql_protocol::ERR_PACKET) {
            break;
        }

        // Parse row data
        RemoteRow row;
        const auto& data = *row_packet;
        size_t offset = 0;

        for (size_t i = 0; i < result.columns.size() && offset < data.size(); i++) {
            if (data[offset] == 0xFB) {
                // NULL
                row.push_back(std::monostate{});
                offset++;
            } else {
                // Length-encoded string
                uint64_t len = 0;
                if (data[offset] < 251) {
                    len = data[offset++];
                } else if (data[offset] == 0xFC && offset + 2 < data.size()) {
                    len = static_cast<uint64_t>(data[offset + 1]) |
                          (static_cast<uint64_t>(data[offset + 2]) << 8);
                    offset += 3;
                }

                if (offset + len <= data.size()) {
                    auto value = convertToLocal(
                        data.data() + offset, len,
                        i < result.columns.size() ? result.columns[i].type_oid : 0);
                    row.push_back(value);
                    offset += len;
                }
            }
        }

        result.rows.push_back(std::move(row));
    }

    result.success = true;
    return Result<RemoteQueryResult>(std::move(result));
}

Result<std::vector<uint8_t>> MySQLAdapter::readPacket() {
    // Read 4-byte header
    uint8_t header[4];
    ssize_t received = recv(impl_->socket_fd, header, 4, MSG_WAITALL);
    if (received != 4) {
        return makeError<std::vector<uint8_t>>(core::Status::IO_ERROR,
                                                "Failed to read packet header");
    }

    // Parse length (3 bytes, little-endian)
    uint32_t length =
        static_cast<uint32_t>(header[0]) |
        (static_cast<uint32_t>(header[1]) << 8) |
        (static_cast<uint32_t>(header[2]) << 16);

    // Sequence ID
    impl_->sequence_id = header[3];

    // Read payload
    std::vector<uint8_t> data(length);
    if (length > 0) {
        received = recv(impl_->socket_fd, data.data(), length, MSG_WAITALL);
        if (received != static_cast<ssize_t>(length)) {
            return makeError<std::vector<uint8_t>>(core::Status::IO_ERROR,
                                                    "Failed to read packet payload");
        }
    }

    recordBytesReceived(4 + length);
    return Result<std::vector<uint8_t>>(std::move(data));
}

Result<void> MySQLAdapter::writePacket(const std::vector<uint8_t>& data) {
    // Build header
    std::vector<uint8_t> packet;

    // Length (3 bytes, little-endian)
    uint32_t length = static_cast<uint32_t>(data.size());
    packet.push_back(length & 0xFF);
    packet.push_back((length >> 8) & 0xFF);
    packet.push_back((length >> 16) & 0xFF);

    // Sequence ID
    packet.push_back(impl_->sequence_id++);

    // Payload
    packet.insert(packet.end(), data.begin(), data.end());

    ssize_t sent = send(impl_->socket_fd, packet.data(), packet.size(), 0);
    if (sent != static_cast<ssize_t>(packet.size())) {
        return makeError(core::Status::IO_ERROR, "Failed to send packet");
    }

    recordBytesSent(packet.size());
    return Result<void>();
}

std::vector<uint8_t> MySQLAdapter::scramblePassword(const std::string& password,
                                                     const std::vector<uint8_t>& auth_data)
{
    if (password.empty()) {
        return {};
    }

    // MySQL native password: SHA1(password) XOR SHA1(scramble + SHA1(SHA1(password)))
    unsigned char sha1_pass[SHA_DIGEST_LENGTH];
    unsigned char sha1_sha1_pass[SHA_DIGEST_LENGTH];

    SHA1(reinterpret_cast<const unsigned char*>(password.c_str()),
         password.length(), sha1_pass);
    SHA1(sha1_pass, SHA_DIGEST_LENGTH, sha1_sha1_pass);

    // Concatenate auth_data + SHA1(SHA1(password))
    std::vector<uint8_t> combined;
    combined.insert(combined.end(), auth_data.begin(), auth_data.end());
    combined.insert(combined.end(), sha1_sha1_pass, sha1_sha1_pass + SHA_DIGEST_LENGTH);

    unsigned char sha1_combined[SHA_DIGEST_LENGTH];
    SHA1(combined.data(), combined.size(), sha1_combined);

    // XOR
    std::vector<uint8_t> result(SHA_DIGEST_LENGTH);
    for (size_t i = 0; i < SHA_DIGEST_LENGTH; i++) {
        result[i] = sha1_pass[i] ^ sha1_combined[i];
    }

    return result;
}

void registerMySQLFDW() {
    // Registration will be handled by UDR system
}

}  // namespace fdw
}  // namespace scratchbird
