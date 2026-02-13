# Remote Database UDR - PostgreSQL Adapter

> Reference-only: Canonical UDR and live-migration behavior now lives in
> `ScratchBird/docs/specifications/Alpha Phase 2/11-Remote-Database-UDR-Specification.md`
> and `ScratchBird/docs/specifications/Alpha Phase 2/11h-Live-Migration-Emulated-Listener.md`.

## 1. Overview

The PostgreSQL adapter implements the `IProtocolAdapter` interface for connecting to PostgreSQL 9.6+ and compatible databases (including CockroachDB, YugabyteDB, Citus).

**Supported Versions**: PostgreSQL 9.6, 10, 11, 12, 13, 14, 15, 16, 17

---

## 2. Protocol Implementation

### 2.1 Wire Protocol Reference

The PostgreSQL wire protocol uses a message-based format:

```
┌──────────────────────────────────────┐
│ Message Format                        │
├──────────────────────────────────────┤
│ 1 byte  │ Message Type (char)        │
│ 4 bytes │ Message Length (int32, BE) │
│ n bytes │ Payload                    │
└──────────────────────────────────────┘
```

Startup messages may be preceded by SSLRequest or GSSENCRequest. If the server replies with an ErrorResponse to those requests, close the connection and retry without encryption (do not surface the error to users).

### 2.2 Message Types

```cpp
namespace pg_messages {
    // Frontend (client) messages
    constexpr char STARTUP_MESSAGE = '\0';  // Special - no type byte
    constexpr char PASSWORD = 'p';
    constexpr char QUERY = 'Q';
    constexpr char PARSE = 'P';
    constexpr char BIND = 'B';
    constexpr char DESCRIBE = 'D';
    constexpr char EXECUTE = 'E';
    constexpr char CLOSE = 'C';
    constexpr char SYNC = 'S';
    constexpr char FLUSH = 'H';
    constexpr char TERMINATE = 'X';
    constexpr char COPY_DATA = 'd';
    constexpr char COPY_DONE = 'c';
    constexpr char COPY_FAIL = 'f';

    // Backend (server) messages
    constexpr char AUTHENTICATION = 'R';
    constexpr char BACKEND_KEY_DATA = 'K';
    constexpr char BIND_COMPLETE = '2';
    constexpr char CLOSE_COMPLETE = '3';
    constexpr char COMMAND_COMPLETE = 'C';
    constexpr char COPY_IN_RESPONSE = 'G';
    constexpr char COPY_OUT_RESPONSE = 'H';
    constexpr char COPY_BOTH_RESPONSE = 'W';
    constexpr char DATA_ROW = 'D';
    constexpr char EMPTY_QUERY_RESPONSE = 'I';
    constexpr char ERROR_RESPONSE = 'E';
    constexpr char NOTICE_RESPONSE = 'N';
    constexpr char NOTIFICATION_RESPONSE = 'A';
    constexpr char NO_DATA = 'n';
    constexpr char NEGOTIATE_VERSION = 'v';
    constexpr char PARAMETER_DESCRIPTION = 't';
    constexpr char PARAMETER_STATUS = 'S';
    constexpr char PARSE_COMPLETE = '1';
    constexpr char PORTAL_SUSPENDED = 's';
    constexpr char READY_FOR_QUERY = 'Z';
    constexpr char ROW_DESCRIPTION = 'T';
}
```

---

## 3. PostgreSQLAdapter Class

```cpp
class PostgreSQLAdapter : public IProtocolAdapter {
public:
    PostgreSQLAdapter();
    ~PostgreSQLAdapter() override;

    // IProtocolAdapter implementation
    Result<void> connect(const ServerDefinition& server,
                          const UserMapping& mapping) override;
    Result<void> disconnect() override;
    ConnectionState getState() const override { return state_; }
    bool isConnected() const override { return state_ == ConnectionState::CONNECTED; }

    Result<bool> ping() override;
    Result<void> reset() override;

    Result<RemoteQueryResult> executeQuery(const std::string& sql) override;
    Result<RemoteQueryResult> executeQueryWithParams(
        const std::string& sql,
        const std::vector<TypedValue>& params) override;

    Result<uint64_t> prepare(const std::string& sql) override;
    Result<RemoteQueryResult> executePrepared(
        uint64_t stmt_id,
        const std::vector<TypedValue>& params) override;
    Result<void> deallocatePrepared(uint64_t stmt_id) override;

    Result<void> beginTransaction() override;
    Result<void> commit() override;
    Result<void> rollback() override;
    Result<void> setSavepoint(const std::string& name) override;
    Result<void> rollbackToSavepoint(const std::string& name) override;

    Result<std::string> declareCursor(const std::string& name,
                                       const std::string& sql) override;
    Result<RemoteQueryResult> fetchFromCursor(const std::string& name,
                                               uint32_t count) override;
    Result<void> closeCursor(const std::string& name) override;

    Result<std::vector<std::string>> listSchemas() override;
    Result<std::vector<std::string>> listTables(const std::string& schema) override;
    Result<std::vector<RemoteColumnDesc>> describeTable(
        const std::string& schema,
        const std::string& table) override;
    Result<std::vector<RemoteIndexDesc>> describeIndexes(
        const std::string& schema,
        const std::string& table) override;
    Result<std::vector<RemoteForeignKey>> describeForeignKeys(
        const std::string& schema,
        const std::string& table) override;

    RemoteDatabaseType getDatabaseType() const override {
        return RemoteDatabaseType::POSTGRESQL;
    }
    std::string getServerVersion() const override { return server_version_; }
    PushdownCapability getCapabilities() const override {
        return PushdownCapability::FULL;  // PostgreSQL supports all pushdown
    }

    ScratchBirdType mapRemoteType(uint32_t remote_oid,
                                   int32_t modifier) const override;
    TypedValue convertToLocal(const void* data, size_t len,
                               uint32_t remote_oid) const override;
    std::vector<uint8_t> convertToRemote(const TypedValue& value,
                                          uint32_t remote_oid) const override;

    ConnectionStats getStats() const override { return stats_; }

private:
    // Connection state
    ConnectionState state_{ConnectionState::DISCONNECTED};
    int socket_fd_{-1};
    std::unique_ptr<TLSConnection> tls_;
    std::string server_version_;
    int32_t backend_pid_{0};
    int32_t secret_key_{0};

    // Buffers
    std::vector<uint8_t> send_buffer_;
    std::vector<uint8_t> recv_buffer_;
    size_t recv_offset_{0};

    // Prepared statements
    std::unordered_map<uint64_t, PreparedStatement> prepared_stmts_;
    uint64_t next_stmt_id_{1};

    // Server parameters
    std::unordered_map<std::string, std::string> server_params_;

    // Statistics
    ConnectionStats stats_;

    // Internal methods
    Result<void> sendStartupMessage(const ServerDefinition& server,
                                     const UserMapping& mapping);
    Result<void> handleAuthentication(const UserMapping& mapping);
    Result<void> sendPasswordMessage(const std::string& password,
                                      uint32_t auth_type,
                                      const std::vector<uint8_t>& salt);

    Result<void> sendMessage(char type, const std::vector<uint8_t>& payload);
    Result<Message> receiveMessage();
    Result<void> waitForReadyForQuery();

    Result<RowDescription> parseRowDescription(const std::vector<uint8_t>& data);
    Result<std::vector<TypedValue>> parseDataRow(
        const std::vector<uint8_t>& data,
        const RowDescription& desc);
    Result<std::string> parseCommandComplete(const std::vector<uint8_t>& data);
    Result<PostgreSQLError> parseErrorResponse(const std::vector<uint8_t>& data);
};
```

---

## 4. Connection Establishment

### 4.1 Startup Sequence

```cpp
Result<void> PostgreSQLAdapter::connect(
    const ServerDefinition& server,
    const UserMapping& mapping)
{
    state_ = ConnectionState::CONNECTING;

    // 1. Create TCP socket
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
        return Error(remote_sqlstate::NETWORK_ERROR, "Failed to create socket");
    }

    // 2. Set socket options
    setSocketOptions(socket_fd_, server);

    // 3. Connect to server
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(server.port);
    inet_pton(AF_INET, server.host.c_str(), &addr.sin_addr);

    if (::connect(socket_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(socket_fd_);
        return Error(remote_sqlstate::CONNECTION_FAILED,
                     "Failed to connect to " + server.host);
    }

    // 4. SSL/GSSENC negotiation if required
    if (server.use_ssl) {
        auto ssl_result = sendSSLRequest();
        if (!ssl_result) {
            close(socket_fd_);
            return ssl_result.error();
        }

        // Server returns single byte 'S' or 'N'
        if (ssl_result.value() == 'S') {
            auto tls_result = initiateTLS(server);
            if (!tls_result) {
                close(socket_fd_);
                return tls_result.error();
            }
        } else if (ssl_result.value() != 'N') {
            close(socket_fd_);
            return Error(remote_sqlstate::PROTOCOL_ERROR,
                         "Unexpected SSLRequest response");
        }
    }

    // 5. Send startup message (over TLS if enabled)
    state_ = ConnectionState::AUTHENTICATING;
    auto startup_result = sendStartupMessage(server, mapping);
    if (!startup_result) {
        disconnect();
        return startup_result;
    }

    // 6. Handle authentication
    auto auth_result = handleAuthentication(mapping);
    if (!auth_result) {
        disconnect();
        return auth_result;
    }

    // 7. Wait for ReadyForQuery
    auto ready_result = waitForReadyForQuery();
    if (!ready_result) {
        disconnect();
        return ready_result;
    }

    state_ = ConnectionState::CONNECTED;
    return {};
}
```

Startup handling requirements:
- Accept `NegotiateProtocolVersion` ('v') before authentication completes.
- Capture `ParameterStatus` values and `BackendKeyData` for cancellation.
- `ReadyForQuery` marks the end of the startup/auth sequence.

### 4.2 Startup Message Format

```cpp
Result<void> PostgreSQLAdapter::sendStartupMessage(
    const ServerDefinition& server,
    const UserMapping& mapping)
{
    // Build startup message (no type byte for startup)
    std::vector<uint8_t> payload;

    // Protocol version: 3.0
    appendInt32BE(payload, 0x00030000);

    // Parameters
    appendCString(payload, "user");
    appendCString(payload, mapping.remote_user);

    appendCString(payload, "database");
    appendCString(payload, server.database);

    appendCString(payload, "application_name");
    appendCString(payload, "ScratchBird");

    appendCString(payload, "client_encoding");
    appendCString(payload, "UTF8");

    // Terminating null
    payload.push_back(0);

    // Send with length prefix (no type byte)
    std::vector<uint8_t> message;
    appendInt32BE(message, payload.size() + 4);  // +4 for length itself
    message.insert(message.end(), payload.begin(), payload.end());

    return sendRaw(message);
}
```

### 4.3 Authentication Handling

```cpp
Result<void> PostgreSQLAdapter::handleAuthentication(const UserMapping& mapping) {
    while (true) {
        auto msg_result = receiveMessage();
        if (!msg_result) {
            return msg_result.error();
        }

        auto& msg = *msg_result;

        if (msg.type == pg_messages::AUTHENTICATION) {
            uint32_t auth_type = readInt32BE(msg.payload, 0);

            switch (auth_type) {
                case 0:  // AuthenticationOk
                    return {};

                case 3:  // AuthenticationCleartextPassword
                    return sendPasswordMessage(mapping.remote_password, 3, {});

                case 5: {  // AuthenticationMD5Password
                    std::vector<uint8_t> salt(msg.payload.begin() + 4,
                                               msg.payload.begin() + 8);
                    return sendPasswordMessage(mapping.remote_password, 5, salt);
                }

                case 10: {  // AuthenticationSASL
                    return handleSASLAuthentication(mapping, msg.payload);
                }

                default:
                    return Error(remote_sqlstate::AUTH_FAILED,
                                 "Unsupported auth type: " + std::to_string(auth_type));
            }
        } else if (msg.type == pg_messages::ERROR_RESPONSE) {
            auto error = parseErrorResponse(msg.payload);
            return Error(remote_sqlstate::AUTH_FAILED, error->message);
        } else if (msg.type == pg_messages::PARAMETER_STATUS) {
            // Store server parameters
            auto [key, value] = parseParameterStatus(msg.payload);
            server_params_[key] = value;
        } else if (msg.type == pg_messages::BACKEND_KEY_DATA) {
            backend_pid_ = readInt32BE(msg.payload, 0);
            secret_key_ = readInt32BE(msg.payload, 4);
        }
    }
}
```

---

## 5. Query Execution

### 5.1 Simple Query Protocol

```cpp
Result<RemoteQueryResult> PostgreSQLAdapter::executeQuery(const std::string& sql) {
    state_ = ConnectionState::EXECUTING;
    auto start_time = std::chrono::steady_clock::now();

    // Send Query message
    std::vector<uint8_t> payload;
    appendCString(payload, sql);
    auto send_result = sendMessage(pg_messages::QUERY, payload);
    if (!send_result) {
        state_ = ConnectionState::FAILED;
        return send_result.error();
    }

    RemoteQueryResult result;
    result.success = true;
    RowDescription row_desc;

    // Process response messages
    while (true) {
        auto msg_result = receiveMessage();
        if (!msg_result) {
            state_ = ConnectionState::FAILED;
            return msg_result.error();
        }

        auto& msg = *msg_result;

        switch (msg.type) {
            case pg_messages::ROW_DESCRIPTION: {
                auto desc_result = parseRowDescription(msg.payload);
                if (!desc_result) return desc_result.error();
                row_desc = *desc_result;

                // Convert to RemoteColumnDesc
                for (const auto& col : row_desc.columns) {
                    RemoteColumnDesc rcd;
                    rcd.name = col.name;
                    rcd.type_oid = col.type_oid;
                    rcd.type_modifier = col.type_modifier;
                    rcd.mapped_type = mapRemoteType(col.type_oid, col.type_modifier);
                    result.columns.push_back(rcd);
                }
                break;
            }

            case pg_messages::DATA_ROW: {
                auto row_result = parseDataRow(msg.payload, row_desc);
                if (!row_result) return row_result.error();
                result.rows.push_back(*row_result);
                break;
            }

            case pg_messages::COMMAND_COMPLETE: {
                auto cmd = parseCommandComplete(msg.payload);
                result.rows_affected = extractRowCount(*cmd);
                break;
            }

            case pg_messages::EMPTY_QUERY_RESPONSE:
                break;

            case pg_messages::ERROR_RESPONSE: {
                auto error = parseErrorResponse(msg.payload);
                result.success = false;
                result.error_message = error->message;
                result.sql_state = error->sql_state;
                break;
            }

            case pg_messages::READY_FOR_QUERY:
                state_ = ConnectionState::CONNECTED;
                result.execution_time_us = std::chrono::duration_cast<
                    std::chrono::microseconds>(
                        std::chrono::steady_clock::now() - start_time).count();
                stats_.queries_executed++;
                return result;

            case pg_messages::NOTICE_RESPONSE:
                // Log but don't error
                break;
        }
    }
}
```

### 5.2 Extended Query Protocol (Parameterized)

```cpp
Result<RemoteQueryResult> PostgreSQLAdapter::executeQueryWithParams(
    const std::string& sql,
    const std::vector<TypedValue>& params)
{
    // 1. Parse
    std::string stmt_name = "";  // Unnamed statement
    std::vector<uint8_t> parse_payload;
    appendCString(parse_payload, stmt_name);
    appendCString(parse_payload, sql);
    appendInt16BE(parse_payload, 0);  // No type hints

    auto parse_result = sendMessage(pg_messages::PARSE, parse_payload);
    if (!parse_result) return parse_result.error();

    // 2. Bind
    std::string portal_name = "";  // Unnamed portal
    std::vector<uint8_t> bind_payload;
    appendCString(bind_payload, portal_name);
    appendCString(bind_payload, stmt_name);

    // Format codes (0 = text, 1 = binary)
    appendInt16BE(bind_payload, 1);  // One format code
    appendInt16BE(bind_payload, 0);  // All text format

    // Parameter values
    appendInt16BE(bind_payload, params.size());
    for (const auto& param : params) {
        auto bytes = convertToRemote(param, 0);  // 0 = infer type
        if (bytes.empty() && param.isNull()) {
            appendInt32BE(bind_payload, -1);  // NULL
        } else {
            appendInt32BE(bind_payload, bytes.size());
            bind_payload.insert(bind_payload.end(), bytes.begin(), bytes.end());
        }
    }

    // Result format codes
    appendInt16BE(bind_payload, 0);  // Text format for all results

    auto bind_result = sendMessage(pg_messages::BIND, bind_payload);
    if (!bind_result) return bind_result.error();

    // 3. Describe portal
    std::vector<uint8_t> describe_payload;
    describe_payload.push_back('P');  // Portal
    appendCString(describe_payload, portal_name);
    auto describe_result = sendMessage(pg_messages::DESCRIBE, describe_payload);
    if (!describe_result) return describe_result.error();

    // 4. Execute
    std::vector<uint8_t> execute_payload;
    appendCString(execute_payload, portal_name);
    appendInt32BE(execute_payload, 0);  // No row limit
    auto execute_result = sendMessage(pg_messages::EXECUTE, execute_payload);
    if (!execute_result) return execute_result.error();

    // 5. Sync
    auto sync_result = sendMessage(pg_messages::SYNC, {});
    if (!sync_result) return sync_result.error();

    // 6. Process results (same as simple query)
    return processQueryResults();
}
```

---

## 6. Prepared Statements

```cpp
Result<uint64_t> PostgreSQLAdapter::prepare(const std::string& sql) {
    uint64_t stmt_id = next_stmt_id_++;
    std::string stmt_name = "sb_stmt_" + std::to_string(stmt_id);

    // Parse message
    std::vector<uint8_t> payload;
    appendCString(payload, stmt_name);
    appendCString(payload, sql);
    appendInt16BE(payload, 0);  // No type hints

    auto result = sendMessage(pg_messages::PARSE, payload);
    if (!result) return result.error();

    // Sync to check for errors
    sendMessage(pg_messages::SYNC, {});

    // Wait for ParseComplete or error
    while (true) {
        auto msg = receiveMessage();
        if (!msg) return msg.error();

        if (msg->type == pg_messages::PARSE_COMPLETE) {
            prepared_stmts_[stmt_id] = {stmt_name, sql};
            break;
        } else if (msg->type == pg_messages::ERROR_RESPONSE) {
            auto error = parseErrorResponse(msg->payload);
            return Error(remote_sqlstate::REMOTE_SYNTAX_ERROR, error->message);
        } else if (msg->type == pg_messages::READY_FOR_QUERY) {
            break;
        }
    }

    return stmt_id;
}

Result<RemoteQueryResult> PostgreSQLAdapter::executePrepared(
    uint64_t stmt_id,
    const std::vector<TypedValue>& params)
{
    auto it = prepared_stmts_.find(stmt_id);
    if (it == prepared_stmts_.end()) {
        return Error(remote_sqlstate::PREPARED_STMT_ERROR,
                     "Unknown prepared statement: " + std::to_string(stmt_id));
    }

    const std::string& stmt_name = it->second.name;

    // Bind, Execute, Sync sequence
    // ... (similar to executeQueryWithParams but using named statement)
}

Result<void> PostgreSQLAdapter::deallocatePrepared(uint64_t stmt_id) {
    auto it = prepared_stmts_.find(stmt_id);
    if (it == prepared_stmts_.end()) {
        return {};  // Already deallocated
    }

    // Close statement
    std::vector<uint8_t> payload;
    payload.push_back('S');  // Statement
    appendCString(payload, it->second.name);

    auto result = sendMessage(pg_messages::CLOSE, payload);
    if (!result) return result.error();

    sendMessage(pg_messages::SYNC, {});
    waitForReadyForQuery();

    prepared_stmts_.erase(it);
    return {};
}
```

---

## 7. Type Conversion

### 7.1 PostgreSQL to ScratchBird

```cpp
TypedValue PostgreSQLAdapter::convertToLocal(
    const void* data,
    size_t len,
    uint32_t remote_oid) const
{
    if (data == nullptr) {
        return TypedValue::null();
    }

    const char* str = static_cast<const char*>(data);

    switch (remote_oid) {
        case pg_types::BOOL:
            return TypedValue::boolean(str[0] == 't' || str[0] == 'T');

        case pg_types::INT2:
            return TypedValue::smallint(std::stoi(std::string(str, len)));

        case pg_types::INT4:
            return TypedValue::integer(std::stoi(std::string(str, len)));

        case pg_types::INT8:
            return TypedValue::bigint(std::stoll(std::string(str, len)));

        case pg_types::FLOAT4:
            return TypedValue::real(std::stof(std::string(str, len)));

        case pg_types::FLOAT8:
            return TypedValue::double_(std::stod(std::string(str, len)));

        case pg_types::NUMERIC: {
            auto decimal = Decimal::parse(std::string(str, len));
            return TypedValue::decimal(decimal);
        }

        case pg_types::TEXT:
        case pg_types::VARCHAR:
            return TypedValue::varchar(std::string(str, len));

        case pg_types::BYTEA: {
            // Handle hex or escape format
            auto bytes = decodeBytea(str, len);
            return TypedValue::blob(bytes);
        }

        case pg_types::DATE: {
            auto date = Date::parse(std::string(str, len));
            return TypedValue::date(date);
        }

        case pg_types::TIME: {
            auto time = Time::parse(std::string(str, len));
            return TypedValue::time(time);
        }

        case pg_types::TIMESTAMP:
        case pg_types::TIMESTAMPTZ: {
            auto ts = Timestamp::parse(std::string(str, len));
            return TypedValue::timestamp(ts);
        }

        case pg_types::UUID: {
            auto uuid = UUID::parse(std::string(str, len));
            return TypedValue::uuid(uuid);
        }

        case pg_types::JSON:
        case pg_types::JSONB:
            return TypedValue::json(std::string(str, len));

        // Arrays
        case 1007: {  // int4[]
            auto arr = parsePostgreSQLArray<int32_t>(str, len);
            return TypedValue::array(arr);
        }

        // Range types
        case pg_types::INT4RANGE: {
            auto range = parsePostgreSQLRange<int32_t>(str, len);
            return TypedValue::int4range(range);
        }

        default:
            // Unknown type - return as text
            return TypedValue::text(std::string(str, len));
    }
}
```

### 7.2 ScratchBird to PostgreSQL

```cpp
std::vector<uint8_t> PostgreSQLAdapter::convertToRemote(
    const TypedValue& value,
    uint32_t remote_oid) const
{
    if (value.isNull()) {
        return {};
    }

    std::string str;

    switch (value.type()) {
        case ScratchBirdType::BOOLEAN:
            str = value.asBoolean() ? "t" : "f";
            break;

        case ScratchBirdType::SMALLINT:
            str = std::to_string(value.asSmallInt());
            break;

        case ScratchBirdType::INTEGER:
            str = std::to_string(value.asInteger());
            break;

        case ScratchBirdType::BIGINT:
            str = std::to_string(value.asBigInt());
            break;

        case ScratchBirdType::REAL:
            str = std::to_string(value.asReal());
            break;

        case ScratchBirdType::DOUBLE:
            str = std::to_string(value.asDouble());
            break;

        case ScratchBirdType::DECIMAL:
            str = value.asDecimal().toString();
            break;

        case ScratchBirdType::VARCHAR:
        case ScratchBirdType::TEXT:
            str = value.asString();
            break;

        case ScratchBirdType::BLOB:
            str = encodeBytea(value.asBlob());
            break;

        case ScratchBirdType::DATE:
            str = value.asDate().toISOString();
            break;

        case ScratchBirdType::TIME:
            str = value.asTime().toISOString();
            break;

        case ScratchBirdType::TIMESTAMP:
        case ScratchBirdType::TIMESTAMP_TZ:
            str = value.asTimestamp().toISOString();
            break;

        case ScratchBirdType::UUID:
            str = value.asUUID().toString();
            break;

        case ScratchBirdType::JSON:
            str = value.asJSON();
            break;

        default:
            str = value.toString();
    }

    return std::vector<uint8_t>(str.begin(), str.end());
}
```

---

## 8. Schema Introspection

### 8.1 List Schemas

```cpp
Result<std::vector<std::string>> PostgreSQLAdapter::listSchemas() {
    const char* sql = R"(
        SELECT nspname
        FROM pg_namespace
        WHERE nspname NOT LIKE 'pg_%'
          AND nspname != 'information_schema'
        ORDER BY nspname
    )";

    auto result = executeQuery(sql);
    if (!result) return result.error();
    if (!result->success) {
        return Error(result->sql_state, result->error_message);
    }

    std::vector<std::string> schemas;
    for (const auto& row : result->rows) {
        schemas.push_back(row[0].asString());
    }
    return schemas;
}
```

### 8.2 List Tables

```cpp
Result<std::vector<std::string>> PostgreSQLAdapter::listTables(
    const std::string& schema)
{
    const char* sql = R"(
        SELECT tablename
        FROM pg_tables
        WHERE schemaname = $1
        ORDER BY tablename
    )";

    auto result = executeQueryWithParams(sql, {TypedValue::varchar(schema)});
    if (!result) return result.error();
    if (!result->success) {
        return Error(result->sql_state, result->error_message);
    }

    std::vector<std::string> tables;
    for (const auto& row : result->rows) {
        tables.push_back(row[0].asString());
    }
    return tables;
}
```

### 8.3 Describe Table

```cpp
Result<std::vector<RemoteColumnDesc>> PostgreSQLAdapter::describeTable(
    const std::string& schema,
    const std::string& table)
{
    const char* sql = R"(
        SELECT
            a.attname AS column_name,
            a.atttypid AS type_oid,
            a.atttypmod AS type_modifier,
            a.attnotnull AS not_null,
            pg_get_expr(d.adbin, d.adrelid) AS default_value,
            t.typname AS type_name
        FROM pg_attribute a
        JOIN pg_class c ON a.attrelid = c.oid
        JOIN pg_namespace n ON c.relnamespace = n.oid
        JOIN pg_type t ON a.atttypid = t.oid
        LEFT JOIN pg_attrdef d ON a.attrelid = d.adrelid AND a.attnum = d.adnum
        WHERE n.nspname = $1
          AND c.relname = $2
          AND a.attnum > 0
          AND NOT a.attisdropped
        ORDER BY a.attnum
    )";

    auto result = executeQueryWithParams(sql, {
        TypedValue::varchar(schema),
        TypedValue::varchar(table)
    });

    if (!result) return result.error();
    if (!result->success) {
        return Error(result->sql_state, result->error_message);
    }

    std::vector<RemoteColumnDesc> columns;
    for (const auto& row : result->rows) {
        RemoteColumnDesc col;
        col.name = row[0].asString();
        col.type_oid = row[1].asInteger();
        col.type_modifier = row[2].asInteger();
        col.nullable = !row[3].asBoolean();
        col.mapped_type = mapRemoteType(col.type_oid, col.type_modifier);
        columns.push_back(col);
    }
    return columns;
}
```

### 8.4 Describe Indexes

```cpp
Result<std::vector<RemoteIndexDesc>> PostgreSQLAdapter::describeIndexes(
    const std::string& schema,
    const std::string& table)
{
    const char* sql = R"(
        SELECT
            i.relname AS index_name,
            ix.indisunique AS is_unique,
            ix.indisprimary AS is_primary,
            array_to_string(array_agg(a.attname ORDER BY k.n), ',') AS columns,
            am.amname AS index_type
        FROM pg_index ix
        JOIN pg_class i ON ix.indexrelid = i.oid
        JOIN pg_class t ON ix.indrelid = t.oid
        JOIN pg_namespace n ON t.relnamespace = n.oid
        JOIN pg_am am ON i.relam = am.oid
        CROSS JOIN LATERAL unnest(ix.indkey) WITH ORDINALITY AS k(attnum, n)
        JOIN pg_attribute a ON a.attrelid = t.oid AND a.attnum = k.attnum
        WHERE n.nspname = $1
          AND t.relname = $2
        GROUP BY i.relname, ix.indisunique, ix.indisprimary, am.amname
        ORDER BY i.relname
    )";

    auto result = executeQueryWithParams(sql, {
        TypedValue::varchar(schema),
        TypedValue::varchar(table)
    });

    if (!result) return result.error();
    if (!result->success) {
        return Error(result->sql_state, result->error_message);
    }

    std::vector<RemoteIndexDesc> indexes;
    for (const auto& row : result->rows) {
        RemoteIndexDesc idx;
        idx.name = row[0].asString();
        idx.is_unique = row[1].asBoolean();
        idx.is_primary = row[2].asBoolean();

        // Parse column list
        std::string cols = row[3].asString();
        std::stringstream ss(cols);
        std::string col;
        while (std::getline(ss, col, ',')) {
            idx.columns.push_back(col);
        }

        idx.type = row[4].asString();
        indexes.push_back(idx);
    }
    return indexes;
}
```

---

## 9. Transaction Management

```cpp
Result<void> PostgreSQLAdapter::beginTransaction() {
    auto result = executeQuery("BEGIN");
    if (!result) return result.error();
    if (!result->success) {
        return Error(result->sql_state, result->error_message);
    }
    state_ = ConnectionState::IN_TRANSACTION;
    return {};
}

Result<void> PostgreSQLAdapter::commit() {
    auto result = executeQuery("COMMIT");
    if (!result) return result.error();
    if (!result->success) {
        return Error(result->sql_state, result->error_message);
    }
    state_ = ConnectionState::CONNECTED;
    return {};
}

Result<void> PostgreSQLAdapter::rollback() {
    auto result = executeQuery("ROLLBACK");
    if (!result) return result.error();
    state_ = ConnectionState::CONNECTED;  // Even on error
    return {};
}

Result<void> PostgreSQLAdapter::setSavepoint(const std::string& name) {
    auto result = executeQuery("SAVEPOINT " + quoteIdentifier(name));
    if (!result) return result.error();
    if (!result->success) {
        return Error(result->sql_state, result->error_message);
    }
    return {};
}

Result<void> PostgreSQLAdapter::rollbackToSavepoint(const std::string& name) {
    auto result = executeQuery("ROLLBACK TO SAVEPOINT " + quoteIdentifier(name));
    if (!result) return result.error();
    return {};
}
```

---

## 10. Error Handling

### 10.1 Error Response Parsing

```cpp
struct PostgreSQLError {
    std::string severity;      // ERROR, FATAL, PANIC
    std::string sql_state;     // 5-character SQLSTATE
    std::string message;       // Primary message
    std::string detail;        // Optional detail
    std::string hint;          // Optional hint
    std::string position;      // Error position in query
    std::string internal_query;
    std::string where_;
    std::string schema;
    std::string table;
    std::string column;
    std::string data_type;
    std::string constraint;
    std::string file;
    std::string line;
    std::string routine;
};

Result<PostgreSQLError> PostgreSQLAdapter::parseErrorResponse(
    const std::vector<uint8_t>& data)
{
    PostgreSQLError error;
    size_t offset = 0;

    while (offset < data.size()) {
        char field_type = data[offset++];
        if (field_type == 0) break;

        std::string value = readCString(data, offset);

        switch (field_type) {
            case 'S': error.severity = value; break;
            case 'V': /* localized severity */ break;
            case 'C': error.sql_state = value; break;
            case 'M': error.message = value; break;
            case 'D': error.detail = value; break;
            case 'H': error.hint = value; break;
            case 'P': error.position = value; break;
            case 'p': /* internal position */ break;
            case 'q': error.internal_query = value; break;
            case 'W': error.where_ = value; break;
            case 's': error.schema = value; break;
            case 't': error.table = value; break;
            case 'c': error.column = value; break;
            case 'd': error.data_type = value; break;
            case 'n': error.constraint = value; break;
            case 'F': error.file = value; break;
            case 'L': error.line = value; break;
            case 'R': error.routine = value; break;
        }
    }

    return error;
}
```

### 10.2 Error Classification

```cpp
bool PostgreSQLAdapter::isRetryableError(const std::string& sql_state) {
    // Connection errors
    if (sql_state.substr(0, 2) == "08") return true;  // connection_exception
    // Serialization failures
    if (sql_state == "40001") return true;  // serialization_failure
    if (sql_state == "40P01") return true;  // deadlock_detected
    return false;
}

bool PostgreSQLAdapter::isConnectionError(const std::string& sql_state) {
    return sql_state.substr(0, 2) == "08";
}
```
