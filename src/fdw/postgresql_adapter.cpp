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
 * @file postgresql_adapter.cpp
 * @brief PostgreSQL Protocol Adapter Implementation
 *
 * Implements the PostgreSQL v3 wire protocol client for connecting to
 * remote PostgreSQL databases.
 *
 * Protocol reference: https://www.postgresql.org/docs/current/protocol.html
 *
 * Part of Phase 3.7: UDR Plugin System
 */

#include "scratchbird/fdw/postgresql_adapter.h"

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
#include <sstream>

#ifdef _WIN32
#ifndef MSG_WAITALL
#define MSG_WAITALL 0
#endif
#define SB_SOCKET_RECV_BUF(buf) reinterpret_cast<char*>(buf)
#define SB_SOCKET_SEND_BUF(buf) reinterpret_cast<const char*>(buf)
#define recv(fd, buf, len, flags) ::recv((fd), SB_SOCKET_RECV_BUF(buf), static_cast<int>(len), (flags))
#define send(fd, buf, len, flags) ::send((fd), SB_SOCKET_SEND_BUF(buf), static_cast<int>(len), (flags))
#define setsockopt(fd, level, optname, optval, optlen) \
    ::setsockopt((fd), (level), (optname), SB_SOCKET_SEND_BUF(optval), static_cast<int>(optlen))
#define getsockopt(fd, level, optname, optval, optlen) \
    ::getsockopt((fd), (level), (optname), SB_SOCKET_RECV_BUF(optval), (optlen))
#endif

namespace scratchbird {
namespace fdw {

// =============================================================================
// PostgreSQL Protocol Constants
// =============================================================================

namespace pg_protocol {
    // Protocol version 3.0
    constexpr uint32_t PROTOCOL_VERSION = (3 << 16) | 0;

    // Message types (frontend -> backend)
    constexpr char MSG_STARTUP      = '\0';  // Special - no type byte
    constexpr char MSG_QUERY        = 'Q';
    constexpr char MSG_PARSE        = 'P';
    constexpr char MSG_BIND         = 'B';
    constexpr char MSG_DESCRIBE     = 'D';
    constexpr char MSG_EXECUTE      = 'E';
    constexpr char MSG_SYNC         = 'S';
    constexpr char MSG_CLOSE        = 'C';
    constexpr char MSG_TERMINATE    = 'X';
    constexpr char MSG_PASSWORD     = 'p';

    // Message types (backend -> frontend)
    constexpr char MSG_AUTH         = 'R';
    constexpr char MSG_BACKEND_KEY  = 'K';
    constexpr char MSG_PARAM_STATUS = 'S';
    constexpr char MSG_READY        = 'Z';
    constexpr char MSG_ROW_DESC     = 'T';
    constexpr char MSG_DATA_ROW     = 'D';
    constexpr char MSG_COMMAND_COMPLETE = 'C';
    constexpr char MSG_EMPTY_QUERY  = 'I';
    constexpr char MSG_ERROR        = 'E';
    constexpr char MSG_NOTICE       = 'N';
    constexpr char MSG_PARSE_COMPLETE = '1';
    constexpr char MSG_BIND_COMPLETE = '2';
    constexpr char MSG_CLOSE_COMPLETE = '3';
    constexpr char MSG_NO_DATA      = 'n';
    constexpr char MSG_PORTAL_SUSPENDED = 's';

    // Authentication methods
    constexpr uint32_t AUTH_OK              = 0;
    constexpr uint32_t AUTH_CLEARTEXT       = 3;
    constexpr uint32_t AUTH_MD5             = 5;
    constexpr uint32_t AUTH_SCRAM_SHA_256   = 10;
}

// =============================================================================
// Implementation Details
// =============================================================================

struct PostgreSQLAdapter::Impl {
    int socket_fd = -1;
    bool connected = false;
    uint32_t backend_pid = 0;
    uint32_t backend_secret_key = 0;
    char transaction_status = 'I';  // I=idle, T=in transaction, E=error

    // Server parameters
    std::unordered_map<std::string, std::string> server_params;

    // Prepared statements
    uint64_t next_stmt_id = 1;
    std::unordered_map<uint64_t, std::string> prepared_statements;

    // Buffer for reading
    std::vector<uint8_t> read_buffer;

    Impl() : read_buffer(65536) {}
};

// =============================================================================
// PostgreSQLAdapter Implementation
// =============================================================================

PostgreSQLAdapter::PostgreSQLAdapter()
    : ProtocolAdapterBase()
    , impl_(std::make_unique<Impl>())
{
}

PostgreSQLAdapter::~PostgreSQLAdapter() {
    disconnect();
}

Result<void> PostgreSQLAdapter::connect(const ServerDefinition& server,
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

    uint16_t port = server.port ? server.port : 5432;
    std::string port_str = std::to_string(port);

    int err = getaddrinfo(server.host.c_str(), port_str.c_str(), &hints, &result);
    if (err != 0) {
        setState(ConnectionState::FAILED);
        return makeError(core::Status::IO_ERROR,
                         "Failed to resolve hostname: " + std::string(gai_strerror(err)));
    }

    // Create socket and connect
    impl_->socket_fd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (impl_->socket_fd < 0) {
        freeaddrinfo(result);
        setState(ConnectionState::FAILED);
        return makeError(core::Status::IO_ERROR, "Failed to create socket");
    }

    // Set TCP_NODELAY
    int flag = 1;
    setsockopt(impl_->socket_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    // Set connection timeout
    struct timeval timeout;
    timeout.tv_sec = server.connection_timeout_ms / 1000;
    timeout.tv_usec = (server.connection_timeout_ms % 1000) * 1000;
    setsockopt(impl_->socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    setsockopt(impl_->socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    if (::connect(impl_->socket_fd, result->ai_addr, result->ai_addrlen) < 0) {
        freeaddrinfo(result);
        close(impl_->socket_fd);
        impl_->socket_fd = -1;
        setState(ConnectionState::FAILED);
        return makeError(core::Status::IO_ERROR, "Failed to connect to server");
    }

    freeaddrinfo(result);
    setState(ConnectionState::AUTHENTICATING);

    // Send startup message
    auto startup_result = sendStartupMessage(server, mapping);
    if (!startup_result) {
        close(impl_->socket_fd);
        impl_->socket_fd = -1;
        setState(ConnectionState::FAILED);
        return startup_result;
    }

    // Handle authentication
    auto auth_result = handleAuthentication(mapping);
    if (!auth_result) {
        close(impl_->socket_fd);
        impl_->socket_fd = -1;
        setState(ConnectionState::FAILED);
        return auth_result;
    }

    impl_->connected = true;
    setState(ConnectionState::CONNECTED);
    return Result<void>();
}

Result<void> PostgreSQLAdapter::disconnect() {
    if (impl_->socket_fd >= 0) {
        // Send terminate message
        std::vector<uint8_t> empty;
        writeMessage(pg_protocol::MSG_TERMINATE, empty);

        close(impl_->socket_fd);
        impl_->socket_fd = -1;
    }

    impl_->connected = false;
    impl_->prepared_statements.clear();
    setState(ConnectionState::DISCONNECTED);
    return Result<void>();
}

Result<bool> PostgreSQLAdapter::ping() {
    if (!impl_->connected || impl_->socket_fd < 0) {
        return Result<bool>(false);
    }

    // Execute simple query
    auto result = executeQuery("SELECT 1");
    return Result<bool>(result.ok());
}

Result<void> PostgreSQLAdapter::reset() {
    if (!impl_->connected) {
        return makeError(core::Status::INVALID_TRANSACTION_STATE, "Not connected");
    }

    // Reset session
    auto result = executeQuery("DISCARD ALL");
    if (!result) {
        return makeError(result.errorCode(), result.errorMessage());
    }

    impl_->transaction_status = 'I';
    return Result<void>();
}

Result<RemoteQueryResult> PostgreSQLAdapter::executeQuery(const std::string& sql) {
    if (!impl_->connected) {
        return makeError<RemoteQueryResult>(core::Status::INVALID_TRANSACTION_STATE, "Not connected");
    }

    recordQueryStart();

    // Send simple query
    auto send_result = sendSimpleQuery(sql);
    if (!send_result) {
        recordQueryEnd(false);
        return makeError<RemoteQueryResult>(send_result.errorCode(), send_result.errorMessage());
    }

    // Read result
    auto result = readQueryResult();
    if (!result) {
        recordQueryEnd(false);
        return result;
    }

    recordQueryEnd(true, result->rows.size());
    return result;
}

Result<RemoteQueryResult> PostgreSQLAdapter::executeQueryWithParams(
    const std::string& sql,
    const std::vector<RemoteValue>& params)
{
    if (!impl_->connected) {
        return makeError<RemoteQueryResult>(core::Status::INVALID_TRANSACTION_STATE, "Not connected");
    }

    recordQueryStart();

    // Use extended query protocol
    auto send_result = sendExtendedQuery(sql, params);
    if (!send_result) {
        recordQueryEnd(false);
        return makeError<RemoteQueryResult>(send_result.errorCode(), send_result.errorMessage());
    }

    // Read result
    auto result = readQueryResult();
    if (!result) {
        recordQueryEnd(false);
        return result;
    }

    recordQueryEnd(true, result->rows.size());
    return result;
}

Result<uint64_t> PostgreSQLAdapter::prepare(const std::string& sql) {
    if (!impl_->connected) {
        return makeError<uint64_t>(core::Status::INVALID_TRANSACTION_STATE, "Not connected");
    }

    uint64_t stmt_id = impl_->next_stmt_id++;
    std::string stmt_name = "stmt_" + std::to_string(stmt_id);

    // Build Parse message
    std::vector<uint8_t> parse_data;
    // Statement name
    for (char c : stmt_name) parse_data.push_back(static_cast<uint8_t>(c));
    parse_data.push_back(0);
    // Query string
    for (char c : sql) parse_data.push_back(static_cast<uint8_t>(c));
    parse_data.push_back(0);
    // Number of parameter types (0 = let server infer)
    parse_data.push_back(0);
    parse_data.push_back(0);

    auto write_result = writeMessage(pg_protocol::MSG_PARSE, parse_data);
    if (!write_result) {
        return makeError<uint64_t>(write_result.errorCode(), write_result.errorMessage());
    }

    // Send Sync
    auto sync_result = sendSync();
    if (!sync_result) {
        return makeError<uint64_t>(sync_result.errorCode(), sync_result.errorMessage());
    }

    // Read responses until ReadyForQuery
    while (true) {
        auto msg = readMessage();
        if (!msg) {
            return makeError<uint64_t>(msg.errorCode(), msg.errorMessage());
        }

        char type = msg->first;
        if (type == pg_protocol::MSG_PARSE_COMPLETE) {
            continue;
        } else if (type == pg_protocol::MSG_READY) {
            if (!msg->second.empty()) {
                impl_->transaction_status = static_cast<char>(msg->second[0]);
            }
            break;
        } else if (type == pg_protocol::MSG_ERROR) {
            // Parse error message
            std::string error_msg = "Prepare failed";
            // TODO: Parse error fields
            return makeError<uint64_t>(core::Status::SYNTAX_ERROR, error_msg);
        }
    }

    impl_->prepared_statements[stmt_id] = stmt_name;
    return Result<uint64_t>(stmt_id);
}

Result<RemoteQueryResult> PostgreSQLAdapter::executePrepared(
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

    const std::string& stmt_name = it->second;
    std::string portal_name = "";  // Empty = unnamed portal

    // Build Bind message
    std::vector<uint8_t> bind_data;
    // Portal name (empty)
    bind_data.push_back(0);
    // Statement name
    for (char c : stmt_name) bind_data.push_back(static_cast<uint8_t>(c));
    bind_data.push_back(0);
    // Number of format codes
    uint16_t num_formats = 0;
    bind_data.push_back((num_formats >> 8) & 0xFF);
    bind_data.push_back(num_formats & 0xFF);
    // Number of parameters
    uint16_t num_params = static_cast<uint16_t>(params.size());
    bind_data.push_back((num_params >> 8) & 0xFF);
    bind_data.push_back(num_params & 0xFF);

    // Parameter values
    for (const auto& param : params) {
        if (isNull(param)) {
            // NULL: length -1
            bind_data.push_back(0xFF);
            bind_data.push_back(0xFF);
            bind_data.push_back(0xFF);
            bind_data.push_back(0xFF);
        } else {
            auto bytes = convertToRemote(param, 0);  // TODO: proper type OID
            int32_t len = static_cast<int32_t>(bytes.size());
            bind_data.push_back((len >> 24) & 0xFF);
            bind_data.push_back((len >> 16) & 0xFF);
            bind_data.push_back((len >> 8) & 0xFF);
            bind_data.push_back(len & 0xFF);
            bind_data.insert(bind_data.end(), bytes.begin(), bytes.end());
        }
    }

    // Number of result format codes
    uint16_t num_result_formats = 0;
    bind_data.push_back((num_result_formats >> 8) & 0xFF);
    bind_data.push_back(num_result_formats & 0xFF);

    auto write_result = writeMessage(pg_protocol::MSG_BIND, bind_data);
    if (!write_result) {
        recordQueryEnd(false);
        return makeError<RemoteQueryResult>(write_result.errorCode(), write_result.errorMessage());
    }

    // Build Execute message
    std::vector<uint8_t> execute_data;
    execute_data.push_back(0);  // Portal name (empty)
    // Max rows (0 = unlimited)
    execute_data.push_back(0);
    execute_data.push_back(0);
    execute_data.push_back(0);
    execute_data.push_back(0);

    write_result = writeMessage(pg_protocol::MSG_EXECUTE, execute_data);
    if (!write_result) {
        recordQueryEnd(false);
        return makeError<RemoteQueryResult>(write_result.errorCode(), write_result.errorMessage());
    }

    // Send Sync
    auto sync_result = sendSync();
    if (!sync_result) {
        recordQueryEnd(false);
        return makeError<RemoteQueryResult>(sync_result.errorCode(), sync_result.errorMessage());
    }

    // Read result
    auto result = readQueryResult();
    if (!result) {
        recordQueryEnd(false);
        return result;
    }

    recordQueryEnd(true, result->rows.size());
    return result;
}

Result<void> PostgreSQLAdapter::deallocatePrepared(uint64_t stmt_id) {
    auto it = impl_->prepared_statements.find(stmt_id);
    if (it == impl_->prepared_statements.end()) {
        return Result<void>();  // Already deallocated
    }

    const std::string& stmt_name = it->second;

    // Build Close message
    std::vector<uint8_t> close_data;
    close_data.push_back('S');  // Close statement
    for (char c : stmt_name) close_data.push_back(static_cast<uint8_t>(c));
    close_data.push_back(0);

    auto write_result = writeMessage(pg_protocol::MSG_CLOSE, close_data);
    if (!write_result) {
        return write_result;
    }

    // Send Sync
    auto sync_result = sendSync();
    if (!sync_result) {
        return sync_result;
    }

    // Read responses
    while (true) {
        auto msg = readMessage();
        if (!msg) {
            return makeError(msg.errorCode(), msg.errorMessage());
        }

        char type = msg->first;
        if (type == pg_protocol::MSG_CLOSE_COMPLETE) {
            continue;
        } else if (type == pg_protocol::MSG_READY) {
            if (!msg->second.empty()) {
                impl_->transaction_status = static_cast<char>(msg->second[0]);
            }
            break;
        }
    }

    impl_->prepared_statements.erase(it);
    return Result<void>();
}

Result<void> PostgreSQLAdapter::beginTransaction() {
    auto result = executeQuery("BEGIN");
    if (result) {
        setState(ConnectionState::IN_TRANSACTION);
    }
    return result.ok() ? Result<void>() : makeError(result.errorCode(), result.errorMessage());
}

Result<void> PostgreSQLAdapter::commit() {
    auto result = executeQuery("COMMIT");
    if (result) {
        setState(ConnectionState::CONNECTED);
    }
    return result.ok() ? Result<void>() : makeError(result.errorCode(), result.errorMessage());
}

Result<void> PostgreSQLAdapter::rollback() {
    auto result = executeQuery("ROLLBACK");
    if (result) {
        setState(ConnectionState::CONNECTED);
    }
    return result.ok() ? Result<void>() : makeError(result.errorCode(), result.errorMessage());
}

Result<void> PostgreSQLAdapter::setSavepoint(const std::string& name) {
    auto result = executeQuery("SAVEPOINT " + name);
    return result.ok() ? Result<void>() : makeError(result.errorCode(), result.errorMessage());
}

Result<void> PostgreSQLAdapter::rollbackToSavepoint(const std::string& name) {
    auto result = executeQuery("ROLLBACK TO SAVEPOINT " + name);
    return result.ok() ? Result<void>() : makeError(result.errorCode(), result.errorMessage());
}

Result<std::string> PostgreSQLAdapter::declareCursor(const std::string& name,
                                                      const std::string& sql)
{
    std::string declare_sql = "DECLARE " + name + " CURSOR FOR " + sql;
    auto result = executeQuery(declare_sql);
    if (!result) {
        return makeError<std::string>(result.errorCode(), result.errorMessage());
    }
    return Result<std::string>(name);
}

Result<RemoteQueryResult> PostgreSQLAdapter::fetchFromCursor(const std::string& name,
                                                              uint32_t count)
{
    std::string fetch_sql = "FETCH " + std::to_string(count) + " FROM " + name;
    return executeQuery(fetch_sql);
}

Result<void> PostgreSQLAdapter::closeCursor(const std::string& name) {
    auto result = executeQuery("CLOSE " + name);
    return result.ok() ? Result<void>() : makeError(result.errorCode(), result.errorMessage());
}

Result<std::vector<std::string>> PostgreSQLAdapter::listSchemas() {
    auto result = executeQuery(
        "SELECT schema_name FROM information_schema.schemata "
        "WHERE schema_name NOT LIKE 'pg_%' AND schema_name != 'information_schema' "
        "ORDER BY schema_name");

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

Result<std::vector<std::string>> PostgreSQLAdapter::listTables(const std::string& schema) {
    std::string query =
        "SELECT table_name FROM information_schema.tables "
        "WHERE table_schema = '" + schema + "' AND table_type = 'BASE TABLE' "
        "ORDER BY table_name";

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

Result<std::vector<RemoteColumnDesc>> PostgreSQLAdapter::describeTable(
    const std::string& schema,
    const std::string& table)
{
    std::string query =
        "SELECT column_name, udt_name, character_maximum_length, is_nullable "
        "FROM information_schema.columns "
        "WHERE table_schema = '" + schema + "' AND table_name = '" + table + "' "
        "ORDER BY ordinal_position";

    auto result = executeQuery(query);
    if (!result) {
        return makeError<std::vector<RemoteColumnDesc>>(result.errorCode(), result.errorMessage());
    }

    std::vector<RemoteColumnDesc> columns;
    for (const auto& row : result->rows) {
        if (row.size() >= 4) {
            RemoteColumnDesc col;
            if (std::holds_alternative<std::string>(row[0])) {
                col.name = std::get<std::string>(row[0]);
            }
            if (std::holds_alternative<std::string>(row[1])) {
                // Map PostgreSQL type name to OID
                // This is simplified - real implementation would use pg_type
            }
            if (std::holds_alternative<std::string>(row[3])) {
                col.nullable = std::get<std::string>(row[3]) == "YES";
            }
            columns.push_back(col);
        }
    }
    return Result<std::vector<RemoteColumnDesc>>(std::move(columns));
}

Result<std::vector<RemoteIndexDesc>> PostgreSQLAdapter::describeIndexes(
    const std::string& schema,
    const std::string& table)
{
    std::string query =
        "SELECT indexname, indexdef FROM pg_indexes "
        "WHERE schemaname = '" + schema + "' AND tablename = '" + table + "'";

    auto result = executeQuery(query);
    if (!result) {
        return makeError<std::vector<RemoteIndexDesc>>(result.errorCode(), result.errorMessage());
    }

    std::vector<RemoteIndexDesc> indexes;
    for (const auto& row : result->rows) {
        if (row.size() >= 2) {
            RemoteIndexDesc idx;
            if (std::holds_alternative<std::string>(row[0])) {
                idx.index_name = std::get<std::string>(row[0]);
            }
            idx.table_name = table;
            // Parse columns from indexdef - simplified
            indexes.push_back(idx);
        }
    }
    return Result<std::vector<RemoteIndexDesc>>(std::move(indexes));
}

Result<std::vector<RemoteForeignKey>> PostgreSQLAdapter::describeForeignKeys(
    const std::string& schema,
    const std::string& table)
{
    std::string query =
        "SELECT tc.constraint_name, kcu.column_name, "
        "ccu.table_name AS referenced_table, ccu.column_name AS referenced_column "
        "FROM information_schema.table_constraints AS tc "
        "JOIN information_schema.key_column_usage AS kcu "
        "ON tc.constraint_name = kcu.constraint_name "
        "JOIN information_schema.constraint_column_usage AS ccu "
        "ON ccu.constraint_name = tc.constraint_name "
        "WHERE tc.constraint_type = 'FOREIGN KEY' "
        "AND tc.table_schema = '" + schema + "' AND tc.table_name = '" + table + "'";

    auto result = executeQuery(query);
    if (!result) {
        return makeError<std::vector<RemoteForeignKey>>(result.errorCode(), result.errorMessage());
    }

    std::vector<RemoteForeignKey> fks;
    // Parse results - simplified
    return Result<std::vector<RemoteForeignKey>>(std::move(fks));
}

PushdownCapability PostgreSQLAdapter::getCapabilities() const {
    return PushdownCapability::FULL;  // PostgreSQL supports all pushdown
}

uint32_t PostgreSQLAdapter::mapRemoteType(uint32_t remote_oid, int32_t modifier) const {
    // Map PostgreSQL OID to ScratchBird type ID
    static const std::unordered_map<uint32_t, uint32_t>& type_map = getTypeMap();
    auto it = type_map.find(remote_oid);
    if (it != type_map.end()) {
        return it->second;
    }
    return 0;  // Unknown type
}

RemoteValue PostgreSQLAdapter::convertToLocal(const void* data, size_t len,
                                               uint32_t remote_oid) const
{
    if (data == nullptr || len == 0) {
        return std::monostate{};
    }

    const char* str = static_cast<const char*>(data);

    switch (remote_oid) {
        case pg_types::BOOL:
            return (*str == 't' || *str == '1' || *str == 'T');

        case pg_types::INT2:
        case pg_types::INT4:
            return static_cast<int32_t>(std::stoi(std::string(str, len)));

        case pg_types::INT8:
            return static_cast<int64_t>(std::stoll(std::string(str, len)));

        case pg_types::FLOAT4:
            return std::stof(std::string(str, len));

        case pg_types::FLOAT8:
        case pg_types::NUMERIC:
            return std::stod(std::string(str, len));

        case pg_types::TEXT:
        case pg_types::VARCHAR:
        case pg_types::CHAR:
        case pg_types::JSON:
        case pg_types::JSONB:
        case pg_types::UUID:
            return std::string(str, len);

        case pg_types::BYTEA:
            // Handle hex encoding
            if (len >= 2 && str[0] == '\\' && str[1] == 'x') {
                std::vector<uint8_t> bytes;
                for (size_t i = 2; i < len; i += 2) {
                    if (i + 1 < len) {
                        char hex[3] = {str[i], str[i + 1], 0};
                        bytes.push_back(static_cast<uint8_t>(std::stoul(hex, nullptr, 16)));
                    }
                }
                return bytes;
            }
            return std::vector<uint8_t>(str, str + len);

        default:
            // Return as string by default
            return std::string(str, len);
    }
}

std::vector<uint8_t> PostgreSQLAdapter::convertToRemote(const RemoteValue& value,
                                                         uint32_t remote_oid) const
{
    std::vector<uint8_t> result;

    std::visit([&result](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            // NULL - handled separately
        } else if constexpr (std::is_same_v<T, bool>) {
            result.push_back(arg ? 't' : 'f');
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
            // Hex encode
            result.push_back('\\');
            result.push_back('x');
            static const char hex[] = "0123456789abcdef";
            for (uint8_t b : arg) {
                result.push_back(hex[b >> 4]);
                result.push_back(hex[b & 0xF]);
            }
        }
    }, value);

    return result;
}

const std::unordered_map<uint32_t, uint32_t>& PostgreSQLAdapter::getTypeMap() {
    static const std::unordered_map<uint32_t, uint32_t> map = {
        {pg_types::BOOL, 1},       // BOOLEAN
        {pg_types::INT2, 2},       // SMALLINT
        {pg_types::INT4, 3},       // INTEGER
        {pg_types::INT8, 4},       // BIGINT
        {pg_types::FLOAT4, 5},     // REAL
        {pg_types::FLOAT8, 6},     // DOUBLE
        {pg_types::NUMERIC, 7},    // DECIMAL
        {pg_types::VARCHAR, 8},    // VARCHAR
        {pg_types::TEXT, 9},       // TEXT
        {pg_types::CHAR, 10},      // CHAR
        {pg_types::BYTEA, 11},     // BLOB
        {pg_types::DATE, 12},      // DATE
        {pg_types::TIME, 13},      // TIME
        {pg_types::TIMESTAMP, 14}, // TIMESTAMP
        {pg_types::TIMESTAMPTZ, 15}, // TIMESTAMP_TZ
        {pg_types::UUID, 16},      // UUID
        {pg_types::JSON, 17},      // JSON
        {pg_types::JSONB, 17},     // JSON
    };
    return map;
}

// =============================================================================
// Protocol Helpers
// =============================================================================

Result<void> PostgreSQLAdapter::sendStartupMessage(const ServerDefinition& server,
                                                    const UserMapping& mapping)
{
    std::vector<uint8_t> data;

    // Protocol version
    uint32_t version = pg_protocol::PROTOCOL_VERSION;
    data.push_back((version >> 24) & 0xFF);
    data.push_back((version >> 16) & 0xFF);
    data.push_back((version >> 8) & 0xFF);
    data.push_back(version & 0xFF);

    // Parameters
    auto addParam = [&data](const std::string& name, const std::string& value) {
        for (char c : name) data.push_back(static_cast<uint8_t>(c));
        data.push_back(0);
        for (char c : value) data.push_back(static_cast<uint8_t>(c));
        data.push_back(0);
    };

    addParam("user", mapping.remote_user);
    addParam("database", server.database);
    addParam("client_encoding", "UTF8");
    addParam("application_name", "ScratchBird FDW");

    // Terminator
    data.push_back(0);

    // Calculate total length (includes length field itself)
    uint32_t total_len = 4 + static_cast<uint32_t>(data.size());

    // Build final message
    std::vector<uint8_t> message;
    message.push_back((total_len >> 24) & 0xFF);
    message.push_back((total_len >> 16) & 0xFF);
    message.push_back((total_len >> 8) & 0xFF);
    message.push_back(total_len & 0xFF);
    message.insert(message.end(), data.begin(), data.end());

    // Send
    ssize_t sent = send(impl_->socket_fd, message.data(), message.size(), 0);
    if (sent != static_cast<ssize_t>(message.size())) {
        return makeError(core::Status::IO_ERROR, "Failed to send startup message");
    }

    recordBytesSent(message.size());
    return Result<void>();
}

Result<void> PostgreSQLAdapter::handleAuthentication(const UserMapping& mapping) {
    while (true) {
        auto msg = readMessage();
        if (!msg) {
            return makeError(msg.errorCode(), msg.errorMessage());
        }

        char type = msg->first;
        const auto& data = msg->second;

        switch (type) {
            case pg_protocol::MSG_AUTH: {
                if (data.size() < 4) {
                    return makeError(core::Status::PROTOCOL_VIOLATION,
                                     "Invalid authentication message");
                }

                uint32_t auth_type =
                    (static_cast<uint32_t>(data[0]) << 24) |
                    (static_cast<uint32_t>(data[1]) << 16) |
                    (static_cast<uint32_t>(data[2]) << 8) |
                    static_cast<uint32_t>(data[3]);

                if (auth_type == pg_protocol::AUTH_OK) {
                    // Authentication successful
                    continue;
                } else if (auth_type == pg_protocol::AUTH_CLEARTEXT) {
                    // Send password
                    std::vector<uint8_t> pwd_data;
                    for (char c : mapping.remote_password) {
                        pwd_data.push_back(static_cast<uint8_t>(c));
                    }
                    pwd_data.push_back(0);

                    auto write_result = writeMessage(pg_protocol::MSG_PASSWORD, pwd_data);
                    if (!write_result) {
                        return write_result;
                    }
                } else if (auth_type == pg_protocol::AUTH_MD5) {
                    // MD5 authentication - would need to implement
                    return makeError(core::Status::NOT_IMPLEMENTED,
                                     "MD5 authentication not implemented");
                } else {
                    return makeError(core::Status::NOT_IMPLEMENTED,
                                     "Unsupported authentication method: " +
                                     std::to_string(auth_type));
                }
                break;
            }

            case pg_protocol::MSG_BACKEND_KEY: {
                if (data.size() >= 8) {
                    impl_->backend_pid =
                        (static_cast<uint32_t>(data[0]) << 24) |
                        (static_cast<uint32_t>(data[1]) << 16) |
                        (static_cast<uint32_t>(data[2]) << 8) |
                        static_cast<uint32_t>(data[3]);
                    impl_->backend_secret_key =
                        (static_cast<uint32_t>(data[4]) << 24) |
                        (static_cast<uint32_t>(data[5]) << 16) |
                        (static_cast<uint32_t>(data[6]) << 8) |
                        static_cast<uint32_t>(data[7]);
                }
                break;
            }

            case pg_protocol::MSG_PARAM_STATUS: {
                // Parse parameter name and value
                size_t null_pos = 0;
                while (null_pos < data.size() && data[null_pos] != 0) {
                    null_pos++;
                }
                if (null_pos < data.size()) {
                    std::string name(data.begin(), data.begin() + null_pos);
                    size_t val_start = null_pos + 1;
                    size_t val_end = val_start;
                    while (val_end < data.size() && data[val_end] != 0) {
                        val_end++;
                    }
                    std::string value(data.begin() + val_start, data.begin() + val_end);
                    impl_->server_params[name] = value;

                    if (name == "server_version") {
                        setServerVersion(value);
                    }
                }
                break;
            }

            case pg_protocol::MSG_READY: {
                if (!data.empty()) {
                    impl_->transaction_status = static_cast<char>(data[0]);
                }
                return Result<void>();
            }

            case pg_protocol::MSG_ERROR: {
                std::string error_msg = "Authentication failed";
                // Parse error fields
                for (size_t i = 0; i < data.size(); i++) {
                    if (data[i] == 'M') {
                        size_t j = i + 1;
                        while (j < data.size() && data[j] != 0) j++;
                        error_msg = std::string(data.begin() + i + 1, data.begin() + j);
                        break;
                    }
                }
                return makeError(core::Status::INVALID_AUTHORIZATION, error_msg);
            }

            case pg_protocol::MSG_NOTICE:
                // Ignore notices during startup
                break;

            default:
                // Unknown message type during startup
                break;
        }
    }
}

Result<void> PostgreSQLAdapter::sendSimpleQuery(const std::string& sql) {
    std::vector<uint8_t> data;
    for (char c : sql) data.push_back(static_cast<uint8_t>(c));
    data.push_back(0);

    return writeMessage(pg_protocol::MSG_QUERY, data);
}

Result<void> PostgreSQLAdapter::sendExtendedQuery(const std::string& sql,
                                                   const std::vector<RemoteValue>& params)
{
    // Parse
    std::vector<uint8_t> parse_data;
    parse_data.push_back(0);  // Unnamed statement
    for (char c : sql) parse_data.push_back(static_cast<uint8_t>(c));
    parse_data.push_back(0);
    parse_data.push_back(0);  // No parameter type OIDs
    parse_data.push_back(0);

    auto result = writeMessage(pg_protocol::MSG_PARSE, parse_data);
    if (!result) return result;

    // Bind
    std::vector<uint8_t> bind_data;
    bind_data.push_back(0);  // Unnamed portal
    bind_data.push_back(0);  // Unnamed statement
    bind_data.push_back(0);  // No format codes
    bind_data.push_back(0);

    // Parameter count
    uint16_t param_count = static_cast<uint16_t>(params.size());
    bind_data.push_back((param_count >> 8) & 0xFF);
    bind_data.push_back(param_count & 0xFF);

    // Parameter values
    for (const auto& param : params) {
        if (isNull(param)) {
            bind_data.push_back(0xFF);
            bind_data.push_back(0xFF);
            bind_data.push_back(0xFF);
            bind_data.push_back(0xFF);
        } else {
            auto bytes = convertToRemote(param, 0);
            int32_t len = static_cast<int32_t>(bytes.size());
            bind_data.push_back((len >> 24) & 0xFF);
            bind_data.push_back((len >> 16) & 0xFF);
            bind_data.push_back((len >> 8) & 0xFF);
            bind_data.push_back(len & 0xFF);
            bind_data.insert(bind_data.end(), bytes.begin(), bytes.end());
        }
    }

    bind_data.push_back(0);  // No result format codes
    bind_data.push_back(0);

    result = writeMessage(pg_protocol::MSG_BIND, bind_data);
    if (!result) return result;

    // Describe portal
    std::vector<uint8_t> describe_data;
    describe_data.push_back('P');  // Portal
    describe_data.push_back(0);    // Unnamed

    result = writeMessage(pg_protocol::MSG_DESCRIBE, describe_data);
    if (!result) return result;

    // Execute
    std::vector<uint8_t> execute_data;
    execute_data.push_back(0);  // Unnamed portal
    execute_data.push_back(0);  // Max rows (unlimited)
    execute_data.push_back(0);
    execute_data.push_back(0);
    execute_data.push_back(0);

    result = writeMessage(pg_protocol::MSG_EXECUTE, execute_data);
    if (!result) return result;

    // Sync
    return sendSync();
}

Result<RemoteQueryResult> PostgreSQLAdapter::readQueryResult() {
    RemoteQueryResult result;
    result.success = false;

    while (true) {
        auto msg = readMessage();
        if (!msg) {
            result.error_message = msg.errorMessage();
            return Result<RemoteQueryResult>(std::move(result));
        }

        char type = msg->first;
        const auto& data = msg->second;

        switch (type) {
            case pg_protocol::MSG_ROW_DESC: {
                if (data.size() < 2) break;

                uint16_t num_fields =
                    (static_cast<uint16_t>(data[0]) << 8) |
                    static_cast<uint16_t>(data[1]);

                result.columns.clear();
                size_t offset = 2;

                for (uint16_t i = 0; i < num_fields && offset < data.size(); i++) {
                    RemoteColumnDesc col;

                    // Field name
                    size_t name_end = offset;
                    while (name_end < data.size() && data[name_end] != 0) {
                        name_end++;
                    }
                    col.name = std::string(data.begin() + offset, data.begin() + name_end);
                    offset = name_end + 1;

                    // Skip table OID (4 bytes)
                    offset += 4;
                    // Skip column attribute number (2 bytes)
                    offset += 2;

                    // Type OID (4 bytes)
                    if (offset + 4 <= data.size()) {
                        col.type_oid =
                            (static_cast<uint32_t>(data[offset]) << 24) |
                            (static_cast<uint32_t>(data[offset + 1]) << 16) |
                            (static_cast<uint32_t>(data[offset + 2]) << 8) |
                            static_cast<uint32_t>(data[offset + 3]);
                        offset += 4;
                    }

                    // Skip type size (2 bytes)
                    offset += 2;

                    // Type modifier (4 bytes)
                    if (offset + 4 <= data.size()) {
                        col.type_modifier =
                            static_cast<int32_t>(
                                (static_cast<uint32_t>(data[offset]) << 24) |
                                (static_cast<uint32_t>(data[offset + 1]) << 16) |
                                (static_cast<uint32_t>(data[offset + 2]) << 8) |
                                static_cast<uint32_t>(data[offset + 3]));
                        offset += 4;
                    }

                    // Skip format code (2 bytes)
                    offset += 2;

                    col.mapped_type_id = mapRemoteType(col.type_oid, col.type_modifier);
                    result.columns.push_back(col);
                }
                break;
            }

            case pg_protocol::MSG_DATA_ROW: {
                if (data.size() < 2) break;

                uint16_t num_columns =
                    (static_cast<uint16_t>(data[0]) << 8) |
                    static_cast<uint16_t>(data[1]);

                RemoteRow row;
                size_t offset = 2;

                for (uint16_t i = 0; i < num_columns && offset < data.size(); i++) {
                    if (offset + 4 > data.size()) break;

                    int32_t col_len =
                        static_cast<int32_t>(
                            (static_cast<uint32_t>(data[offset]) << 24) |
                            (static_cast<uint32_t>(data[offset + 1]) << 16) |
                            (static_cast<uint32_t>(data[offset + 2]) << 8) |
                            static_cast<uint32_t>(data[offset + 3]));
                    offset += 4;

                    if (col_len == -1) {
                        row.push_back(std::monostate{});
                    } else if (offset + col_len <= data.size()) {
                        uint32_t type_oid = 0;
                        if (i < result.columns.size()) {
                            type_oid = result.columns[i].type_oid;
                        }
                        auto value = convertToLocal(
                            data.data() + offset,
                            static_cast<size_t>(col_len),
                            type_oid);
                        row.push_back(value);
                        offset += col_len;
                    }
                }

                result.rows.push_back(std::move(row));
                break;
            }

            case pg_protocol::MSG_COMMAND_COMPLETE: {
                result.success = true;
                // Parse rows affected from tag
                std::string tag(data.begin(), data.end() - 1);  // Remove null terminator
                size_t space_pos = tag.rfind(' ');
                if (space_pos != std::string::npos) {
                    try {
                        result.rows_affected = std::stoull(tag.substr(space_pos + 1));
                    } catch (...) {
                        // Not a number - OK
                    }
                }
                break;
            }

            case pg_protocol::MSG_EMPTY_QUERY:
                result.success = true;
                break;

            case pg_protocol::MSG_ERROR: {
                // Parse error fields
                for (size_t i = 0; i < data.size(); i++) {
                    char field_type = static_cast<char>(data[i]);
                    if (field_type == 0) break;

                    size_t j = i + 1;
                    while (j < data.size() && data[j] != 0) j++;

                    std::string field_value(data.begin() + i + 1, data.begin() + j);

                    switch (field_type) {
                        case 'M':
                            result.error_message = field_value;
                            break;
                        case 'C':
                            result.sql_state = field_value;
                            break;
                    }

                    i = j;
                }
                result.success = false;
                break;
            }

            case pg_protocol::MSG_PARSE_COMPLETE:
            case pg_protocol::MSG_BIND_COMPLETE:
            case pg_protocol::MSG_NO_DATA:
            case pg_protocol::MSG_NOTICE:
                // Ignore these
                break;

            case pg_protocol::MSG_READY: {
                if (!data.empty()) {
                    impl_->transaction_status = static_cast<char>(data[0]);
                }
                return Result<RemoteQueryResult>(std::move(result));
            }

            default:
                // Unknown message type
                break;
        }
    }
}

Result<void> PostgreSQLAdapter::sendSync() {
    std::vector<uint8_t> empty;
    return writeMessage(pg_protocol::MSG_SYNC, empty);
}

Result<void> PostgreSQLAdapter::writeMessage(char type, const std::vector<uint8_t>& data) {
    std::vector<uint8_t> message;

    // Type byte
    message.push_back(static_cast<uint8_t>(type));

    // Length (includes length field, excludes type)
    uint32_t len = 4 + static_cast<uint32_t>(data.size());
    message.push_back((len >> 24) & 0xFF);
    message.push_back((len >> 16) & 0xFF);
    message.push_back((len >> 8) & 0xFF);
    message.push_back(len & 0xFF);

    // Data
    message.insert(message.end(), data.begin(), data.end());

    ssize_t sent = send(impl_->socket_fd, message.data(), message.size(), 0);
    if (sent != static_cast<ssize_t>(message.size())) {
        return makeError(core::Status::IO_ERROR, "Failed to send message");
    }

    recordBytesSent(message.size());
    return Result<void>();
}

Result<std::pair<char, std::vector<uint8_t>>> PostgreSQLAdapter::readMessage() {
    uint8_t header[5];

    // Read type and length
    ssize_t received = recv(impl_->socket_fd, header, 5, MSG_WAITALL);
    if (received != 5) {
        return makeError<std::pair<char, std::vector<uint8_t>>>(
            core::Status::IO_ERROR, "Failed to read message header");
    }

    char type = static_cast<char>(header[0]);
    uint32_t len =
        (static_cast<uint32_t>(header[1]) << 24) |
        (static_cast<uint32_t>(header[2]) << 16) |
        (static_cast<uint32_t>(header[3]) << 8) |
        static_cast<uint32_t>(header[4]);

    // Sanity check length
    if (len < 4 || len > 1024 * 1024 * 16) {  // Max 16 MB
        return makeError<std::pair<char, std::vector<uint8_t>>>(
            core::Status::PROTOCOL_VIOLATION, "Invalid message length");
    }

    // Read data (len includes itself, so subtract 4)
    uint32_t data_len = len - 4;
    std::vector<uint8_t> data(data_len);

    if (data_len > 0) {
        received = recv(impl_->socket_fd, data.data(), data_len, MSG_WAITALL);
        if (received != static_cast<ssize_t>(data_len)) {
            return makeError<std::pair<char, std::vector<uint8_t>>>(
                core::Status::IO_ERROR, "Failed to read message data");
        }
    }

    recordBytesReceived(5 + data_len);
    return Result<std::pair<char, std::vector<uint8_t>>>(
        std::make_pair(type, std::move(data)));
}

void registerPostgreSQLFDW() {
    // Registration will be handled by UDR system
}

}  // namespace fdw
}  // namespace scratchbird
