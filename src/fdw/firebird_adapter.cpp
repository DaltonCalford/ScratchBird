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
 * @file firebird_adapter.cpp
 * @brief Firebird Protocol Adapter Implementation
 *
 * Implements the Firebird wire protocol client for connecting to
 * remote Firebird databases.
 *
 * Protocol reference: Firebird Wire Protocol
 *
 * Part of Phase 3.7: UDR Plugin System
 */

#include "scratchbird/fdw/firebird_adapter.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <sstream>

namespace scratchbird {
namespace fdw {

// =============================================================================
// Firebird Protocol Constants
// =============================================================================

namespace fb_protocol {
    // Protocol versions
    constexpr uint32_t PROTOCOL_VERSION10 = 10;
    constexpr uint32_t PROTOCOL_VERSION11 = 11;
    constexpr uint32_t PROTOCOL_VERSION12 = 12;
    constexpr uint32_t PROTOCOL_VERSION13 = 13;

    // Architecture types
    constexpr uint32_t CNCT_generic = 1;

    // Operations
    constexpr uint32_t op_connect       = 1;
    constexpr uint32_t op_accept        = 3;
    constexpr uint32_t op_reject        = 4;
    constexpr uint32_t op_attach        = 19;
    constexpr uint32_t op_response      = 9;
    constexpr uint32_t op_detach        = 21;
    constexpr uint32_t op_transaction   = 29;
    constexpr uint32_t op_commit        = 30;
    constexpr uint32_t op_rollback      = 31;
    constexpr uint32_t op_allocate_statement = 62;
    constexpr uint32_t op_prepare_statement = 68;
    constexpr uint32_t op_execute       = 63;
    constexpr uint32_t op_execute2      = 76;
    constexpr uint32_t op_fetch         = 65;
    constexpr uint32_t op_free_statement = 67;
    constexpr uint32_t op_info_database = 40;
    constexpr uint32_t op_info_sql      = 73;

    // SQL types (from Firebird's ibase.h)
    constexpr int16_t SQL_TEXT      = 452;
    constexpr int16_t SQL_VARYING   = 448;
    constexpr int16_t SQL_SHORT     = 500;
    constexpr int16_t SQL_LONG      = 496;
    constexpr int16_t SQL_FLOAT     = 482;
    constexpr int16_t SQL_DOUBLE    = 480;
    constexpr int16_t SQL_D_FLOAT   = 530;
    constexpr int16_t SQL_TIMESTAMP = 510;
    constexpr int16_t SQL_BLOB      = 520;
    constexpr int16_t SQL_ARRAY     = 540;
    constexpr int16_t SQL_QUAD      = 550;
    constexpr int16_t SQL_TYPE_TIME = 560;
    constexpr int16_t SQL_TYPE_DATE = 570;
    constexpr int16_t SQL_INT64     = 580;
    constexpr int16_t SQL_BOOLEAN   = 32764;
    constexpr int16_t SQL_NULL      = 32766;

    // Statement types
    constexpr uint32_t DSQL_free = 1;
    constexpr uint32_t DSQL_close = 2;
    constexpr uint32_t DSQL_drop = 4;

    // Info items
    constexpr uint8_t isc_info_sql_stmt_type = 21;
    constexpr uint8_t isc_info_sql_select = 4;
    constexpr uint8_t isc_info_end = 1;
}

// =============================================================================
// Implementation Details
// =============================================================================

struct FirebirdAdapter::Impl {
    int socket_fd = -1;
    bool connected = false;
    uint32_t protocol_version = 0;
    uint32_t db_handle = 0;
    uint32_t transaction_handle = 0;

    // Prepared statements: local id -> server statement handle
    uint64_t next_local_stmt_id = 1;
    std::unordered_map<uint64_t, uint32_t> prepared_statements;

    // Server info
    std::string server_version;

    // Buffer
    std::vector<uint8_t> read_buffer;

    Impl() : read_buffer(65536) {}
};

// =============================================================================
// FirebirdAdapter Implementation
// =============================================================================

FirebirdAdapter::FirebirdAdapter()
    : ProtocolAdapterBase()
    , impl_(std::make_unique<Impl>())
{
}

FirebirdAdapter::~FirebirdAdapter() {
    disconnect();
}

Result<void> FirebirdAdapter::connect(const ServerDefinition& server,
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

    uint16_t port = server.port ? server.port : 3050;
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

    // Send connect packet
    auto connect_result = sendConnect(server, mapping);
    if (!connect_result) {
        close(impl_->socket_fd);
        impl_->socket_fd = -1;
        setState(ConnectionState::FAILED);
        return connect_result;
    }

    // Receive accept/reject
    auto response = receiveResponse();
    if (!response) {
        close(impl_->socket_fd);
        impl_->socket_fd = -1;
        setState(ConnectionState::FAILED);
        return makeError(response.errorCode(), response.errorMessage());
    }

    if (response->first == fb_protocol::op_reject) {
        close(impl_->socket_fd);
        impl_->socket_fd = -1;
        setState(ConnectionState::FAILED);
        return makeError(core::Status::INVALID_AUTHORIZATION, "Connection rejected by server");
    }

    if (response->first != fb_protocol::op_accept) {
        close(impl_->socket_fd);
        impl_->socket_fd = -1;
        setState(ConnectionState::FAILED);
        return makeError(core::Status::PROTOCOL_VIOLATION, "Unexpected response from server");
    }

    // Parse accept response
    if (response->second.size() >= 4) {
        impl_->protocol_version = ntohl(*reinterpret_cast<const uint32_t*>(response->second.data()));
    }

    // Send attach
    auto attach_result = sendAttach(server, mapping);
    if (!attach_result) {
        close(impl_->socket_fd);
        impl_->socket_fd = -1;
        setState(ConnectionState::FAILED);
        return attach_result;
    }

    // Receive attach response
    response = receiveResponse();
    if (!response) {
        close(impl_->socket_fd);
        impl_->socket_fd = -1;
        setState(ConnectionState::FAILED);
        return makeError(response.errorCode(), response.errorMessage());
    }

    if (response->first != fb_protocol::op_response) {
        close(impl_->socket_fd);
        impl_->socket_fd = -1;
        setState(ConnectionState::FAILED);
        return makeError(core::Status::INVALID_AUTHORIZATION, "Attach failed");
    }

    // Parse database handle from response
    if (response->second.size() >= 4) {
        impl_->db_handle = ntohl(*reinterpret_cast<const uint32_t*>(response->second.data()));
    }

    impl_->connected = true;
    setState(ConnectionState::CONNECTED);
    return Result<void>();
}

Result<void> FirebirdAdapter::disconnect() {
    if (impl_->socket_fd >= 0) {
        // Send detach
        std::vector<uint8_t> detach_data;
        uint32_t db_handle_net = htonl(impl_->db_handle);
        detach_data.insert(detach_data.end(),
                           reinterpret_cast<uint8_t*>(&db_handle_net),
                           reinterpret_cast<uint8_t*>(&db_handle_net) + 4);

        sendOperation(fb_protocol::op_detach, detach_data);

        close(impl_->socket_fd);
        impl_->socket_fd = -1;
    }

    impl_->connected = false;
    impl_->db_handle = 0;
    impl_->transaction_handle = 0;
    impl_->prepared_statements.clear();
    setState(ConnectionState::DISCONNECTED);
    return Result<void>();
}

Result<bool> FirebirdAdapter::ping() {
    if (!impl_->connected || impl_->socket_fd < 0) {
        return Result<bool>(false);
    }

    // Execute simple query
    auto result = executeQuery("SELECT 1 FROM RDB$DATABASE");
    return Result<bool>(result.ok());
}

Result<void> FirebirdAdapter::reset() {
    if (!impl_->connected) {
        return makeError(core::Status::INVALID_TRANSACTION_STATE, "Not connected");
    }

    // Rollback any active transaction
    if (impl_->transaction_handle != 0) {
        rollback();
    }

    return Result<void>();
}

Result<RemoteQueryResult> FirebirdAdapter::executeQuery(const std::string& sql) {
    if (!impl_->connected) {
        return makeError<RemoteQueryResult>(core::Status::INVALID_TRANSACTION_STATE, "Not connected");
    }

    recordQueryStart();

    auto result = executeAndFetch(sql);
    if (!result) {
        recordQueryEnd(false);
        return result;
    }

    recordQueryEnd(true, result->rows.size());
    return result;
}

Result<RemoteQueryResult> FirebirdAdapter::executeQueryWithParams(
    const std::string& sql,
    const std::vector<RemoteValue>& params)
{
    // Use prepared statement for parameterized queries
    auto stmt_result = prepare(sql);
    if (!stmt_result) {
        return makeError<RemoteQueryResult>(stmt_result.errorCode(), stmt_result.errorMessage());
    }

    uint64_t stmt_id = *stmt_result;
    auto exec_result = executePrepared(stmt_id, params);
    deallocatePrepared(stmt_id);

    return exec_result;
}

Result<uint64_t> FirebirdAdapter::prepare(const std::string& sql) {
    if (!impl_->connected) {
        return makeError<uint64_t>(core::Status::INVALID_TRANSACTION_STATE, "Not connected");
    }

    // Allocate statement
    std::vector<uint8_t> alloc_data;
    uint32_t db_handle_net = htonl(impl_->db_handle);
    alloc_data.insert(alloc_data.end(),
                      reinterpret_cast<uint8_t*>(&db_handle_net),
                      reinterpret_cast<uint8_t*>(&db_handle_net) + 4);

    auto send_result = sendOperation(fb_protocol::op_allocate_statement, alloc_data);
    if (!send_result) {
        return makeError<uint64_t>(send_result.errorCode(), send_result.errorMessage());
    }

    auto response = receiveResponse();
    if (!response || response->first != fb_protocol::op_response) {
        return makeError<uint64_t>(core::Status::SYNTAX_ERROR, "Failed to allocate statement");
    }

    uint32_t stmt_handle = 0;
    if (response->second.size() >= 4) {
        stmt_handle = ntohl(*reinterpret_cast<const uint32_t*>(response->second.data()));
    }

    // Prepare statement
    std::vector<uint8_t> prepare_data;

    // Transaction handle (may be 0)
    uint32_t tr_handle_net = htonl(impl_->transaction_handle);
    prepare_data.insert(prepare_data.end(),
                        reinterpret_cast<uint8_t*>(&tr_handle_net),
                        reinterpret_cast<uint8_t*>(&tr_handle_net) + 4);

    // Statement handle
    uint32_t stmt_handle_net = htonl(stmt_handle);
    prepare_data.insert(prepare_data.end(),
                        reinterpret_cast<uint8_t*>(&stmt_handle_net),
                        reinterpret_cast<uint8_t*>(&stmt_handle_net) + 4);

    // SQL dialect
    uint32_t dialect = htonl(3);  // SQL dialect 3
    prepare_data.insert(prepare_data.end(),
                        reinterpret_cast<uint8_t*>(&dialect),
                        reinterpret_cast<uint8_t*>(&dialect) + 4);

    // SQL string (length + data)
    uint32_t sql_len_net = htonl(static_cast<uint32_t>(sql.length()));
    prepare_data.insert(prepare_data.end(),
                        reinterpret_cast<uint8_t*>(&sql_len_net),
                        reinterpret_cast<uint8_t*>(&sql_len_net) + 4);
    prepare_data.insert(prepare_data.end(), sql.begin(), sql.end());

    // Pad to 4-byte boundary
    while (prepare_data.size() % 4 != 0) {
        prepare_data.push_back(0);
    }

    // Info items
    std::vector<uint8_t> info_items = {fb_protocol::isc_info_sql_stmt_type, fb_protocol::isc_info_end};
    uint32_t info_len_net = htonl(static_cast<uint32_t>(info_items.size()));
    prepare_data.insert(prepare_data.end(),
                        reinterpret_cast<uint8_t*>(&info_len_net),
                        reinterpret_cast<uint8_t*>(&info_len_net) + 4);
    prepare_data.insert(prepare_data.end(), info_items.begin(), info_items.end());
    while (prepare_data.size() % 4 != 0) {
        prepare_data.push_back(0);
    }

    // Buffer size
    uint32_t buffer_size = htonl(256);
    prepare_data.insert(prepare_data.end(),
                        reinterpret_cast<uint8_t*>(&buffer_size),
                        reinterpret_cast<uint8_t*>(&buffer_size) + 4);

    send_result = sendOperation(fb_protocol::op_prepare_statement, prepare_data);
    if (!send_result) {
        return makeError<uint64_t>(send_result.errorCode(), send_result.errorMessage());
    }

    response = receiveResponse();
    if (!response || response->first != fb_protocol::op_response) {
        return makeError<uint64_t>(core::Status::SYNTAX_ERROR, "Failed to prepare statement");
    }

    uint64_t local_id = impl_->next_local_stmt_id++;
    impl_->prepared_statements[local_id] = stmt_handle;
    return Result<uint64_t>(local_id);
}

Result<RemoteQueryResult> FirebirdAdapter::executePrepared(
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

    uint32_t stmt_handle = it->second;

    // Ensure we have a transaction
    if (impl_->transaction_handle == 0) {
        auto tr_result = beginTransaction();
        if (!tr_result) {
            recordQueryEnd(false);
            return makeError<RemoteQueryResult>(tr_result.errorCode(), tr_result.errorMessage());
        }
    }

    // Execute statement
    std::vector<uint8_t> execute_data;

    // Transaction handle
    uint32_t tr_handle_net = htonl(impl_->transaction_handle);
    execute_data.insert(execute_data.end(),
                        reinterpret_cast<uint8_t*>(&tr_handle_net),
                        reinterpret_cast<uint8_t*>(&tr_handle_net) + 4);

    // Statement handle
    uint32_t stmt_handle_net = htonl(stmt_handle);
    execute_data.insert(execute_data.end(),
                        reinterpret_cast<uint8_t*>(&stmt_handle_net),
                        reinterpret_cast<uint8_t*>(&stmt_handle_net) + 4);

    auto send_result = sendOperation(fb_protocol::op_execute2, execute_data);
    if (!send_result) {
        recordQueryEnd(false);
        return makeError<RemoteQueryResult>(send_result.errorCode(), send_result.errorMessage());
    }

    // Read response
    auto response = receiveResponse();
    if (!response) {
        recordQueryEnd(false);
        return makeError<RemoteQueryResult>(response.errorCode(), response.errorMessage());
    }

    RemoteQueryResult result;
    result.success = (response->first == fb_protocol::op_response);

    // TODO: Parse result rows - requires XSQLDA parsing

    recordQueryEnd(result.success, result.rows.size());
    return Result<RemoteQueryResult>(std::move(result));
}

Result<void> FirebirdAdapter::deallocatePrepared(uint64_t stmt_id) {
    auto it = impl_->prepared_statements.find(stmt_id);
    if (it == impl_->prepared_statements.end()) {
        return Result<void>();
    }

    uint32_t stmt_handle = it->second;

    std::vector<uint8_t> free_data;
    uint32_t stmt_handle_net = htonl(stmt_handle);
    free_data.insert(free_data.end(),
                     reinterpret_cast<uint8_t*>(&stmt_handle_net),
                     reinterpret_cast<uint8_t*>(&stmt_handle_net) + 4);

    uint32_t option = htonl(fb_protocol::DSQL_drop);
    free_data.insert(free_data.end(),
                     reinterpret_cast<uint8_t*>(&option),
                     reinterpret_cast<uint8_t*>(&option) + 4);

    sendOperation(fb_protocol::op_free_statement, free_data);
    receiveResponse();  // Ignore response

    impl_->prepared_statements.erase(it);
    return Result<void>();
}

Result<void> FirebirdAdapter::beginTransaction() {
    if (!impl_->connected) {
        return makeError(core::Status::INVALID_TRANSACTION_STATE, "Not connected");
    }

    if (impl_->transaction_handle != 0) {
        return Result<void>();  // Already in transaction
    }

    std::vector<uint8_t> tr_data;

    // Database handle
    uint32_t db_handle_net = htonl(impl_->db_handle);
    tr_data.insert(tr_data.end(),
                   reinterpret_cast<uint8_t*>(&db_handle_net),
                   reinterpret_cast<uint8_t*>(&db_handle_net) + 4);

    // TPB (Transaction Parameter Block) - simple read-write
    std::vector<uint8_t> tpb = {3, 9, 6, 15, 17};  // isc_tpb_version3, read_committed, rec_version, wait, read_write
    uint32_t tpb_len = htonl(static_cast<uint32_t>(tpb.size()));
    tr_data.insert(tr_data.end(),
                   reinterpret_cast<uint8_t*>(&tpb_len),
                   reinterpret_cast<uint8_t*>(&tpb_len) + 4);
    tr_data.insert(tr_data.end(), tpb.begin(), tpb.end());
    while (tr_data.size() % 4 != 0) {
        tr_data.push_back(0);
    }

    auto send_result = sendOperation(fb_protocol::op_transaction, tr_data);
    if (!send_result) {
        return send_result;
    }

    auto response = receiveResponse();
    if (!response || response->first != fb_protocol::op_response) {
        return makeError(core::Status::SYNTAX_ERROR, "Failed to start transaction");
    }

    // Parse transaction handle
    if (response->second.size() >= 4) {
        impl_->transaction_handle = ntohl(*reinterpret_cast<const uint32_t*>(response->second.data()));
    }

    setState(ConnectionState::IN_TRANSACTION);
    return Result<void>();
}

Result<void> FirebirdAdapter::commit() {
    if (impl_->transaction_handle == 0) {
        return Result<void>();
    }

    std::vector<uint8_t> commit_data;
    uint32_t tr_handle_net = htonl(impl_->transaction_handle);
    commit_data.insert(commit_data.end(),
                       reinterpret_cast<uint8_t*>(&tr_handle_net),
                       reinterpret_cast<uint8_t*>(&tr_handle_net) + 4);

    auto send_result = sendOperation(fb_protocol::op_commit, commit_data);
    if (!send_result) {
        return send_result;
    }

    auto response = receiveResponse();
    impl_->transaction_handle = 0;
    setState(ConnectionState::CONNECTED);

    if (!response || response->first != fb_protocol::op_response) {
        return makeError(core::Status::SYNTAX_ERROR, "Commit failed");
    }

    return Result<void>();
}

Result<void> FirebirdAdapter::rollback() {
    if (impl_->transaction_handle == 0) {
        return Result<void>();
    }

    std::vector<uint8_t> rollback_data;
    uint32_t tr_handle_net = htonl(impl_->transaction_handle);
    rollback_data.insert(rollback_data.end(),
                         reinterpret_cast<uint8_t*>(&tr_handle_net),
                         reinterpret_cast<uint8_t*>(&tr_handle_net) + 4);

    auto send_result = sendOperation(fb_protocol::op_rollback, rollback_data);
    if (!send_result) {
        return send_result;
    }

    auto response = receiveResponse();
    impl_->transaction_handle = 0;
    setState(ConnectionState::CONNECTED);

    if (!response || response->first != fb_protocol::op_response) {
        return makeError(core::Status::SYNTAX_ERROR, "Rollback failed");
    }

    return Result<void>();
}

Result<void> FirebirdAdapter::setSavepoint(const std::string& name) {
    auto result = executeQuery("SAVEPOINT " + name);
    return result.ok() ? Result<void>() : makeError(result.errorCode(), result.errorMessage());
}

Result<void> FirebirdAdapter::rollbackToSavepoint(const std::string& name) {
    auto result = executeQuery("ROLLBACK TO SAVEPOINT " + name);
    return result.ok() ? Result<void>() : makeError(result.errorCode(), result.errorMessage());
}

Result<std::string> FirebirdAdapter::declareCursor(const std::string& name,
                                                    const std::string& sql)
{
    std::string declare_sql = "DECLARE " + name + " CURSOR FOR " + sql;
    auto result = executeQuery(declare_sql);
    if (!result) {
        return makeError<std::string>(result.errorCode(), result.errorMessage());
    }
    return Result<std::string>(name);
}

Result<RemoteQueryResult> FirebirdAdapter::fetchFromCursor(const std::string& name,
                                                            uint32_t count)
{
    std::string fetch_sql = "FETCH " + std::to_string(count) + " FROM " + name;
    return executeQuery(fetch_sql);
}

Result<void> FirebirdAdapter::closeCursor(const std::string& name) {
    auto result = executeQuery("CLOSE " + name);
    return result.ok() ? Result<void>() : makeError(result.errorCode(), result.errorMessage());
}

Result<std::vector<std::string>> FirebirdAdapter::listSchemas() {
    // Firebird doesn't have schemas in the PostgreSQL sense
    // Return empty list - tables are in a flat namespace
    std::vector<std::string> empty;
    return Result<std::vector<std::string>>(std::move(empty));
}

Result<std::vector<std::string>> FirebirdAdapter::listTables(const std::string& schema) {
    auto result = executeQuery(
        "SELECT TRIM(RDB$RELATION_NAME) FROM RDB$RELATIONS "
        "WHERE RDB$SYSTEM_FLAG = 0 AND RDB$VIEW_BLR IS NULL "
        "ORDER BY RDB$RELATION_NAME");

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

Result<std::vector<RemoteColumnDesc>> FirebirdAdapter::describeTable(
    const std::string& schema,
    const std::string& table)
{
    std::string query =
        "SELECT TRIM(RF.RDB$FIELD_NAME), F.RDB$FIELD_TYPE, RF.RDB$NULL_FLAG "
        "FROM RDB$RELATION_FIELDS RF "
        "JOIN RDB$FIELDS F ON RF.RDB$FIELD_SOURCE = F.RDB$FIELD_NAME "
        "WHERE RF.RDB$RELATION_NAME = '" + table + "' "
        "ORDER BY RF.RDB$FIELD_POSITION";

    auto result = executeQuery(query);
    if (!result) {
        return makeError<std::vector<RemoteColumnDesc>>(result.errorCode(), result.errorMessage());
    }

    std::vector<RemoteColumnDesc> columns;
    for (const auto& row : result->rows) {
        if (row.size() >= 2) {
            RemoteColumnDesc col;
            if (std::holds_alternative<std::string>(row[0])) {
                col.name = std::get<std::string>(row[0]);
            }
            if (std::holds_alternative<int32_t>(row[1])) {
                col.type_oid = static_cast<uint32_t>(std::get<int32_t>(row[1]));
            }
            col.nullable = true;
            if (row.size() > 2 && std::holds_alternative<int32_t>(row[2])) {
                col.nullable = std::get<int32_t>(row[2]) == 0;
            }
            col.mapped_type_id = mapRemoteType(col.type_oid, -1);
            columns.push_back(col);
        }
    }
    return Result<std::vector<RemoteColumnDesc>>(std::move(columns));
}

Result<std::vector<RemoteIndexDesc>> FirebirdAdapter::describeIndexes(
    const std::string& schema,
    const std::string& table)
{
    std::string query =
        "SELECT TRIM(RDB$INDEX_NAME), RDB$UNIQUE_FLAG "
        "FROM RDB$INDICES "
        "WHERE RDB$RELATION_NAME = '" + table + "'";

    auto result = executeQuery(query);
    if (!result) {
        return makeError<std::vector<RemoteIndexDesc>>(result.errorCode(), result.errorMessage());
    }

    std::vector<RemoteIndexDesc> indexes;
    for (const auto& row : result->rows) {
        if (!row.empty()) {
            RemoteIndexDesc idx;
            if (std::holds_alternative<std::string>(row[0])) {
                idx.index_name = std::get<std::string>(row[0]);
            }
            idx.table_name = table;
            if (row.size() > 1 && std::holds_alternative<int32_t>(row[1])) {
                idx.is_unique = std::get<int32_t>(row[1]) == 1;
            }
            indexes.push_back(idx);
        }
    }
    return Result<std::vector<RemoteIndexDesc>>(std::move(indexes));
}

Result<std::vector<RemoteForeignKey>> FirebirdAdapter::describeForeignKeys(
    const std::string& schema,
    const std::string& table)
{
    std::vector<RemoteForeignKey> fks;
    // Firebird FK query is complex - simplified for now
    return Result<std::vector<RemoteForeignKey>>(std::move(fks));
}

PushdownCapability FirebirdAdapter::getCapabilities() const {
    return PushdownCapability::STANDARD;
}

uint32_t FirebirdAdapter::mapRemoteType(uint32_t remote_oid, int32_t modifier) const {
    static const std::unordered_map<int16_t, uint32_t>& type_map = getTypeMap();
    auto it = type_map.find(static_cast<int16_t>(remote_oid));
    if (it != type_map.end()) {
        return it->second;
    }
    return 0;
}

RemoteValue FirebirdAdapter::convertToLocal(const void* data, size_t len,
                                             uint32_t remote_oid) const
{
    if (data == nullptr || len == 0) {
        return std::monostate{};
    }

    const char* str = static_cast<const char*>(data);
    int16_t fb_type = static_cast<int16_t>(remote_oid);

    switch (fb_type) {
        case fb_protocol::SQL_SHORT:
            if (len >= 2) {
                return static_cast<int16_t>(ntohs(*reinterpret_cast<const uint16_t*>(data)));
            }
            break;

        case fb_protocol::SQL_LONG:
            if (len >= 4) {
                return static_cast<int32_t>(ntohl(*reinterpret_cast<const uint32_t*>(data)));
            }
            break;

        case fb_protocol::SQL_INT64:
            if (len >= 8) {
                // 64-bit network byte order
                const uint8_t* bytes = static_cast<const uint8_t*>(data);
                int64_t val = 0;
                for (int i = 0; i < 8; i++) {
                    val = (val << 8) | bytes[i];
                }
                return val;
            }
            break;

        case fb_protocol::SQL_FLOAT:
            if (len >= 4) {
                float f;
                uint32_t n = ntohl(*reinterpret_cast<const uint32_t*>(data));
                std::memcpy(&f, &n, sizeof(f));
                return f;
            }
            break;

        case fb_protocol::SQL_DOUBLE:
        case fb_protocol::SQL_D_FLOAT:
            if (len >= 8) {
                double d;
                const uint8_t* bytes = static_cast<const uint8_t*>(data);
                uint64_t n = 0;
                for (int i = 0; i < 8; i++) {
                    n = (n << 8) | bytes[i];
                }
                std::memcpy(&d, &n, sizeof(d));
                return d;
            }
            break;

        case fb_protocol::SQL_TEXT:
        case fb_protocol::SQL_VARYING:
            return std::string(str, len);

        case fb_protocol::SQL_BLOB:
            return std::vector<uint8_t>(str, str + len);

        case fb_protocol::SQL_BOOLEAN:
            if (len >= 1) {
                return static_cast<bool>(str[0] != 0);
            }
            break;

        default:
            return std::string(str, len);
    }

    return std::monostate{};
}

std::vector<uint8_t> FirebirdAdapter::convertToRemote(const RemoteValue& value,
                                                       uint32_t remote_oid) const
{
    std::vector<uint8_t> result;

    std::visit([&result](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            // NULL
        } else if constexpr (std::is_same_v<T, bool>) {
            result.push_back(arg ? 1 : 0);
        } else if constexpr (std::is_same_v<T, int16_t>) {
            uint16_t n = htons(static_cast<uint16_t>(arg));
            result.insert(result.end(), reinterpret_cast<uint8_t*>(&n),
                          reinterpret_cast<uint8_t*>(&n) + 2);
        } else if constexpr (std::is_same_v<T, int32_t>) {
            uint32_t n = htonl(static_cast<uint32_t>(arg));
            result.insert(result.end(), reinterpret_cast<uint8_t*>(&n),
                          reinterpret_cast<uint8_t*>(&n) + 4);
        } else if constexpr (std::is_same_v<T, int64_t>) {
            for (int i = 7; i >= 0; i--) {
                result.push_back((arg >> (i * 8)) & 0xFF);
            }
        } else if constexpr (std::is_same_v<T, float>) {
            uint32_t n;
            std::memcpy(&n, &arg, sizeof(n));
            n = htonl(n);
            result.insert(result.end(), reinterpret_cast<uint8_t*>(&n),
                          reinterpret_cast<uint8_t*>(&n) + 4);
        } else if constexpr (std::is_same_v<T, double>) {
            uint64_t n;
            std::memcpy(&n, &arg, sizeof(n));
            for (int i = 7; i >= 0; i--) {
                result.push_back((n >> (i * 8)) & 0xFF);
            }
        } else if constexpr (std::is_same_v<T, std::string>) {
            result.insert(result.end(), arg.begin(), arg.end());
        } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
            result = arg;
        }
    }, value);

    return result;
}

const std::unordered_map<int16_t, uint32_t>& FirebirdAdapter::getTypeMap() {
    static const std::unordered_map<int16_t, uint32_t> map = {
        {fb_protocol::SQL_SHORT, 2},        // SMALLINT
        {fb_protocol::SQL_LONG, 3},         // INTEGER
        {fb_protocol::SQL_INT64, 4},        // BIGINT
        {fb_protocol::SQL_FLOAT, 5},        // REAL
        {fb_protocol::SQL_DOUBLE, 6},       // DOUBLE
        {fb_protocol::SQL_D_FLOAT, 6},      // DOUBLE
        {fb_protocol::SQL_TEXT, 10},        // CHAR
        {fb_protocol::SQL_VARYING, 8},      // VARCHAR
        {fb_protocol::SQL_BLOB, 11},        // BLOB
        {fb_protocol::SQL_TYPE_DATE, 12},   // DATE
        {fb_protocol::SQL_TYPE_TIME, 13},   // TIME
        {fb_protocol::SQL_TIMESTAMP, 14},   // TIMESTAMP
        {fb_protocol::SQL_BOOLEAN, 1},      // BOOLEAN
    };
    return map;
}

// =============================================================================
// Protocol Helpers
// =============================================================================

Result<void> FirebirdAdapter::sendConnect(const ServerDefinition& server,
                                           const UserMapping& mapping)
{
    std::vector<uint8_t> data;

    // op_connect is handled specially

    // Database path
    std::string db_path = server.database;
    uint32_t path_len = htonl(static_cast<uint32_t>(db_path.length()));
    data.insert(data.end(), reinterpret_cast<uint8_t*>(&path_len),
                reinterpret_cast<uint8_t*>(&path_len) + 4);
    data.insert(data.end(), db_path.begin(), db_path.end());
    while (data.size() % 4 != 0) {
        data.push_back(0);
    }

    // Protocol count
    uint32_t protocol_count = htonl(1);
    data.insert(data.end(), reinterpret_cast<uint8_t*>(&protocol_count),
                reinterpret_cast<uint8_t*>(&protocol_count) + 4);

    // User identification (CNCT block)
    std::vector<uint8_t> cnct;

    // User name
    cnct.push_back(1);  // isc_dpb_sys_user_name
    cnct.push_back(static_cast<uint8_t>(mapping.remote_user.length()));
    cnct.insert(cnct.end(), mapping.remote_user.begin(), mapping.remote_user.end());

    // Password (legacy - should use SRP in production)
    cnct.push_back(5);  // isc_dpb_password
    cnct.push_back(static_cast<uint8_t>(mapping.remote_password.length()));
    cnct.insert(cnct.end(), mapping.remote_password.begin(), mapping.remote_password.end());

    uint32_t cnct_len = htonl(static_cast<uint32_t>(cnct.size()));
    data.insert(data.end(), reinterpret_cast<uint8_t*>(&cnct_len),
                reinterpret_cast<uint8_t*>(&cnct_len) + 4);
    data.insert(data.end(), cnct.begin(), cnct.end());
    while (data.size() % 4 != 0) {
        data.push_back(0);
    }

    // Protocol version/architecture
    uint32_t version = htonl(fb_protocol::PROTOCOL_VERSION13);
    data.insert(data.end(), reinterpret_cast<uint8_t*>(&version),
                reinterpret_cast<uint8_t*>(&version) + 4);

    uint32_t arch = htonl(fb_protocol::CNCT_generic);
    data.insert(data.end(), reinterpret_cast<uint8_t*>(&arch),
                reinterpret_cast<uint8_t*>(&arch) + 4);

    uint32_t min_type = htonl(0);
    data.insert(data.end(), reinterpret_cast<uint8_t*>(&min_type),
                reinterpret_cast<uint8_t*>(&min_type) + 4);

    uint32_t max_type = htonl(3);
    data.insert(data.end(), reinterpret_cast<uint8_t*>(&max_type),
                reinterpret_cast<uint8_t*>(&max_type) + 4);

    uint32_t weight = htonl(2);
    data.insert(data.end(), reinterpret_cast<uint8_t*>(&weight),
                reinterpret_cast<uint8_t*>(&weight) + 4);

    return sendOperation(fb_protocol::op_connect, data);
}

Result<void> FirebirdAdapter::sendAttach(const ServerDefinition& server,
                                          const UserMapping& mapping)
{
    std::vector<uint8_t> data;

    // Database handle (0 for attach)
    uint32_t db_handle = htonl(0);
    data.insert(data.end(), reinterpret_cast<uint8_t*>(&db_handle),
                reinterpret_cast<uint8_t*>(&db_handle) + 4);

    // Database path
    std::string db_path = server.database;
    uint32_t path_len = htonl(static_cast<uint32_t>(db_path.length()));
    data.insert(data.end(), reinterpret_cast<uint8_t*>(&path_len),
                reinterpret_cast<uint8_t*>(&path_len) + 4);
    data.insert(data.end(), db_path.begin(), db_path.end());
    while (data.size() % 4 != 0) {
        data.push_back(0);
    }

    // DPB (Database Parameter Block)
    std::vector<uint8_t> dpb;
    dpb.push_back(1);  // isc_dpb_version1

    // User name
    dpb.push_back(28);  // isc_dpb_user_name
    dpb.push_back(static_cast<uint8_t>(mapping.remote_user.length()));
    dpb.insert(dpb.end(), mapping.remote_user.begin(), mapping.remote_user.end());

    // Password
    dpb.push_back(29);  // isc_dpb_password
    dpb.push_back(static_cast<uint8_t>(mapping.remote_password.length()));
    dpb.insert(dpb.end(), mapping.remote_password.begin(), mapping.remote_password.end());

    // SQL dialect
    dpb.push_back(63);  // isc_dpb_sql_dialect
    dpb.push_back(4);   // length
    dpb.push_back(3);   // dialect 3
    dpb.push_back(0);
    dpb.push_back(0);
    dpb.push_back(0);

    uint32_t dpb_len = htonl(static_cast<uint32_t>(dpb.size()));
    data.insert(data.end(), reinterpret_cast<uint8_t*>(&dpb_len),
                reinterpret_cast<uint8_t*>(&dpb_len) + 4);
    data.insert(data.end(), dpb.begin(), dpb.end());
    while (data.size() % 4 != 0) {
        data.push_back(0);
    }

    return sendOperation(fb_protocol::op_attach, data);
}

Result<void> FirebirdAdapter::sendOperation(uint32_t operation, const std::vector<uint8_t>& data) {
    std::vector<uint8_t> packet;

    // Operation code
    uint32_t op_net = htonl(operation);
    packet.insert(packet.end(), reinterpret_cast<uint8_t*>(&op_net),
                  reinterpret_cast<uint8_t*>(&op_net) + 4);

    // Data
    packet.insert(packet.end(), data.begin(), data.end());

    ssize_t sent = send(impl_->socket_fd, packet.data(), packet.size(), 0);
    if (sent != static_cast<ssize_t>(packet.size())) {
        return makeError(core::Status::IO_ERROR, "Failed to send operation");
    }

    recordBytesSent(packet.size());
    return Result<void>();
}

Result<std::pair<uint32_t, std::vector<uint8_t>>> FirebirdAdapter::receiveResponse() {
    // Read operation code (4 bytes)
    uint32_t op_net;
    ssize_t received = recv(impl_->socket_fd, &op_net, 4, MSG_WAITALL);
    if (received != 4) {
        return makeError<std::pair<uint32_t, std::vector<uint8_t>>>(
            core::Status::IO_ERROR, "Failed to read response operation");
    }

    uint32_t operation = ntohl(op_net);

    // Read response data based on operation type
    std::vector<uint8_t> data;

    if (operation == fb_protocol::op_response) {
        // Read object handle (4 bytes)
        uint32_t handle;
        received = recv(impl_->socket_fd, &handle, 4, MSG_WAITALL);
        if (received == 4) {
            data.insert(data.end(), reinterpret_cast<uint8_t*>(&handle),
                        reinterpret_cast<uint8_t*>(&handle) + 4);
        }

        // Read blob id (8 bytes)
        uint64_t blob_id;
        received = recv(impl_->socket_fd, &blob_id, 8, MSG_WAITALL);
        if (received == 8) {
            data.insert(data.end(), reinterpret_cast<uint8_t*>(&blob_id),
                        reinterpret_cast<uint8_t*>(&blob_id) + 8);
        }

        // Read data length and data
        uint32_t data_len_net;
        received = recv(impl_->socket_fd, &data_len_net, 4, MSG_WAITALL);
        if (received == 4) {
            uint32_t data_len = ntohl(data_len_net);
            if (data_len > 0 && data_len < 1000000) {  // Sanity check
                std::vector<uint8_t> buffer(data_len);
                received = recv(impl_->socket_fd, buffer.data(), data_len, MSG_WAITALL);
                if (received == static_cast<ssize_t>(data_len)) {
                    data.insert(data.end(), buffer.begin(), buffer.end());
                }
                // Skip padding
                size_t padding = (4 - (data_len % 4)) % 4;
                if (padding > 0) {
                    uint8_t pad[4];
                    recv(impl_->socket_fd, pad, padding, MSG_WAITALL);
                }
            }
        }

        // Read status vector
        // ... simplified - full implementation would parse ISC status
    } else if (operation == fb_protocol::op_accept) {
        // Read version, arch, type
        uint32_t version, arch, type;
        recv(impl_->socket_fd, &version, 4, MSG_WAITALL);
        recv(impl_->socket_fd, &arch, 4, MSG_WAITALL);
        recv(impl_->socket_fd, &type, 4, MSG_WAITALL);

        data.insert(data.end(), reinterpret_cast<uint8_t*>(&version),
                    reinterpret_cast<uint8_t*>(&version) + 4);
    }

    recordBytesReceived(4 + data.size());
    return Result<std::pair<uint32_t, std::vector<uint8_t>>>(
        std::make_pair(operation, std::move(data)));
}

Result<RemoteQueryResult> FirebirdAdapter::executeAndFetch(const std::string& sql) {
    // For simple queries, prepare and execute
    auto stmt_result = prepare(sql);
    if (!stmt_result) {
        RemoteQueryResult result;
        result.success = false;
        result.error_message = stmt_result.errorMessage();
        return Result<RemoteQueryResult>(std::move(result));
    }

    uint64_t stmt_id = *stmt_result;
    auto exec_result = executePrepared(stmt_id, {});
    deallocatePrepared(stmt_id);

    return exec_result;
}

void registerFirebirdFDW() {
    // Registration will be handled by UDR system
}

}  // namespace fdw
}  // namespace scratchbird
