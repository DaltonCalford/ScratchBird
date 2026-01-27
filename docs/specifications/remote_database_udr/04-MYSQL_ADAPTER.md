# Remote Database UDR - MySQL Adapter

> Reference-only: Canonical UDR and live-migration behavior now lives in
> `ScratchBird/docs/specifications/Alpha Phase 2/11-Remote-Database-UDR-Specification.md`
> and `ScratchBird/docs/specifications/Alpha Phase 2/11h-Live-Migration-Emulated-Listener.md`.

## 1. Overview

The MySQL adapter implements the `IProtocolAdapter` interface for connecting to MySQL 5.7+ and MariaDB 10+ servers.

**Supported Versions**: MySQL 5.7, 8.0, 8.1+; MariaDB 10.x, 11.x

---

## 2. Protocol Implementation

### 2.1 Wire Protocol Reference

MySQL uses a packet-based protocol with sequence numbers:

```
┌────────────────────────────────────────┐
│ Packet Format                           │
├────────────────────────────────────────┤
│ 3 bytes │ Payload Length (LE)          │
│ 1 byte  │ Sequence Number              │
│ n bytes │ Payload                       │
└────────────────────────────────────────┘
```

Notes:
- Maximum payload length is `0xFFFFFF`; larger payloads are split across multiple packets.
- Sequence numbers increment per packet and reset to 0 at the start of each command/response exchange.

### 2.2 Command Types

```cpp
namespace mysql_commands {
    constexpr uint8_t COM_QUIT           = 0x01;
    constexpr uint8_t COM_INIT_DB        = 0x02;
    constexpr uint8_t COM_QUERY          = 0x03;
    constexpr uint8_t COM_FIELD_LIST     = 0x04;
    constexpr uint8_t COM_CREATE_DB      = 0x05;
    constexpr uint8_t COM_DROP_DB        = 0x06;
    constexpr uint8_t COM_REFRESH        = 0x07;
    constexpr uint8_t COM_STATISTICS     = 0x09;
    constexpr uint8_t COM_PROCESS_INFO   = 0x0A;
    constexpr uint8_t COM_PROCESS_KILL   = 0x0C;
    constexpr uint8_t COM_PING           = 0x0E;
    constexpr uint8_t COM_STMT_PREPARE   = 0x16;
    constexpr uint8_t COM_STMT_EXECUTE   = 0x17;
    constexpr uint8_t COM_STMT_SEND_LONG_DATA = 0x18;
    constexpr uint8_t COM_STMT_CLOSE     = 0x19;
    constexpr uint8_t COM_STMT_RESET     = 0x1A;
    constexpr uint8_t COM_SET_OPTION     = 0x1B;
    constexpr uint8_t COM_STMT_FETCH     = 0x1C;
    constexpr uint8_t COM_RESET_CONNECTION = 0x1F;
}
```

### 2.3 Response Types

```cpp
namespace mysql_responses {
    constexpr uint8_t OK_PACKET      = 0x00;
    constexpr uint8_t EOF_PACKET     = 0xFE;
    constexpr uint8_t ERR_PACKET     = 0xFF;
    constexpr uint8_t LOCAL_INFILE   = 0xFB;
    constexpr uint8_t AUTH_SWITCH    = 0xFE;
    constexpr uint8_t AUTH_MORE_DATA = 0x01;
}
```

---

## 3. MySQLAdapter Class

```cpp
class MySQLAdapter : public IProtocolAdapter {
public:
    MySQLAdapter();
    ~MySQLAdapter() override;

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
        return RemoteDatabaseType::MYSQL;
    }
    std::string getServerVersion() const override { return server_version_; }
    PushdownCapability getCapabilities() const override;

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
    uint32_t connection_id_{0};
    uint32_t server_capabilities_{0};
    uint32_t client_capabilities_{0};
    uint8_t server_charset_{0};
    uint16_t server_status_{0};

    // Sequence number management
    uint8_t sequence_id_{0};

    // Buffers
    std::vector<uint8_t> send_buffer_;
    std::vector<uint8_t> recv_buffer_;

    // Prepared statements
    struct MySQLPreparedStatement {
        uint32_t stmt_id;
        uint16_t num_columns;
        uint16_t num_params;
        std::vector<ColumnDefinition> columns;
        std::vector<ColumnDefinition> params;
    };
    std::unordered_map<uint64_t, MySQLPreparedStatement> prepared_stmts_;
    uint64_t next_local_stmt_id_{1};

    // Statistics
    ConnectionStats stats_;

    // Internal methods
    Result<void> sendPacket(const std::vector<uint8_t>& payload);
    Result<std::vector<uint8_t>> receivePacket();
    void resetSequence() { sequence_id_ = 0; }

    Result<void> handleHandshake(const UserMapping& mapping);
    Result<void> sendAuthResponse(const std::vector<uint8_t>& handshake,
                                   const UserMapping& mapping);
    std::vector<uint8_t> encryptPassword(const std::string& password,
                                          const std::vector<uint8_t>& scramble,
                                          const std::string& auth_plugin);

    Result<OKPacket> parseOKPacket(const std::vector<uint8_t>& data);
    Result<ERRPacket> parseERRPacket(const std::vector<uint8_t>& data);
    Result<std::vector<ColumnDefinition>> readColumnDefinitions(uint64_t count);
    Result<std::vector<TypedValue>> readResultRow(
        const std::vector<ColumnDefinition>& columns);
};
```

---

## 4. Connection Establishment

### 4.1 Handshake Protocol

```cpp
Result<void> MySQLAdapter::connect(
    const ServerDefinition& server,
    const UserMapping& mapping)
{
    state_ = ConnectionState::CONNECTING;

    // 1. Create TCP socket
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
        return Error(remote_sqlstate::NETWORK_ERROR, "Failed to create socket");
    }

    // 2. Connect to server
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(server.port);
    inet_pton(AF_INET, server.host.c_str(), &addr.sin_addr);

    if (::connect(socket_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(socket_fd_);
        return Error(remote_sqlstate::CONNECTION_FAILED,
                     "Failed to connect to " + server.host);
    }

    // 3. Receive initial handshake
    state_ = ConnectionState::AUTHENTICATING;
    auto handshake_result = handleHandshake(mapping);
    if (!handshake_result) {
        disconnect();
        return handshake_result;
    }

    // 4. Set default database
    if (!server.database.empty()) {
        auto db_result = executeQuery("USE " + quoteIdentifier(server.database));
        if (!db_result || !db_result->success) {
            disconnect();
            return Error(remote_sqlstate::CONNECTION_FAILED,
                         "Failed to select database: " + server.database);
        }
    }

    state_ = ConnectionState::CONNECTED;
    return {};
}
```

Handshake handling requirements:
- If `CLIENT_SSL` is requested and the server advertises SSL, send `SSLRequest` (the 32-byte prefix of HandshakeResponse41) and perform TLS before sending the full response.
- Support `AuthSwitchRequest` (0xFE) and `AuthMoreData` (0x01) during authentication, including `caching_sha2_password` and `sha256_password` flows.
- Distinguish EOF vs OK when `CLIENT_DEPRECATE_EOF` is set.
- Handle `LOCAL_INFILE` (0xFB) during `COM_QUERY` if enabled.

### 4.2 Handshake Handling

```cpp
Result<void> MySQLAdapter::handleHandshake(const UserMapping& mapping) {
    // 1. Receive handshake packet
    resetSequence();
    auto packet = receivePacket();
    if (!packet) return packet.error();

    // 2. Parse handshake
    size_t offset = 0;
    uint8_t protocol_version = (*packet)[offset++];
    if (protocol_version != 10) {
        return Error(remote_sqlstate::PROTOCOL_ERROR,
                     "Unsupported protocol version: " + std::to_string(protocol_version));
    }

    // Server version (null-terminated string)
    server_version_ = readNullTerminatedString(*packet, offset);

    // Connection ID
    connection_id_ = readInt32LE(*packet, offset);
    offset += 4;

    // Auth plugin data part 1 (8 bytes)
    std::vector<uint8_t> scramble(packet->begin() + offset,
                                   packet->begin() + offset + 8);
    offset += 8;

    // Skip filler
    offset += 1;

    // Capability flags (lower 2 bytes)
    uint16_t cap_lower = readInt16LE(*packet, offset);
    offset += 2;

    // Character set
    server_charset_ = (*packet)[offset++];

    // Status flags
    server_status_ = readInt16LE(*packet, offset);
    offset += 2;

    // Capability flags (upper 2 bytes)
    uint16_t cap_upper = readInt16LE(*packet, offset);
    offset += 2;

    server_capabilities_ = (cap_upper << 16) | cap_lower;

    // Auth plugin data length
    uint8_t auth_data_len = (*packet)[offset++];

    // Skip reserved
    offset += 10;

    // Auth plugin data part 2 (if capability flag set)
    if (server_capabilities_ & CLIENT_SECURE_CONNECTION) {
        size_t part2_len = std::max(13, (int)auth_data_len - 8);
        scramble.insert(scramble.end(),
                        packet->begin() + offset,
                        packet->begin() + offset + part2_len - 1);  // -1 for null
        offset += part2_len;
    }

    // Auth plugin name
    std::string auth_plugin;
    if (server_capabilities_ & CLIENT_PLUGIN_AUTH) {
        auth_plugin = readNullTerminatedString(*packet, offset);
    } else {
        auth_plugin = "mysql_native_password";
    }

    // 3. Send auth response
    return sendAuthResponse(scramble, mapping, auth_plugin);
}
```

### 4.3 Authentication Response

```cpp
Result<void> MySQLAdapter::sendAuthResponse(
    const std::vector<uint8_t>& scramble,
    const UserMapping& mapping,
    const std::string& auth_plugin)
{
    std::vector<uint8_t> payload;

    // Client capabilities
    client_capabilities_ = CLIENT_PROTOCOL_41 |
                           CLIENT_SECURE_CONNECTION |
                           CLIENT_PLUGIN_AUTH |
                           CLIENT_DEPRECATE_EOF |
                           CLIENT_CONNECT_WITH_DB;

    appendInt32LE(payload, client_capabilities_);

    // Max packet size
    appendInt32LE(payload, 16777215);

    // Character set (utf8mb4)
    payload.push_back(45);  // utf8mb4_general_ci

    // Reserved (23 bytes of zero)
    payload.insert(payload.end(), 23, 0);

    // Username
    appendNullTerminatedString(payload, mapping.remote_user);

    // Auth response
    auto auth_data = encryptPassword(mapping.remote_password, scramble, auth_plugin);
    if (client_capabilities_ & CLIENT_SECURE_CONNECTION) {
        payload.push_back(static_cast<uint8_t>(auth_data.size()));
        payload.insert(payload.end(), auth_data.begin(), auth_data.end());
    } else {
        payload.insert(payload.end(), auth_data.begin(), auth_data.end());
        payload.push_back(0);  // Null terminator
    }

    // Database (if CLIENT_CONNECT_WITH_DB)
    // Auth plugin name
    appendNullTerminatedString(payload, auth_plugin);

    auto send_result = sendPacket(payload);
    if (!send_result) return send_result.error();

    // Receive response
    auto response = receivePacket();
    if (!response) return response.error();

    uint8_t first_byte = (*response)[0];
    if (first_byte == mysql_responses::OK_PACKET) {
        return {};  // Success
    } else if (first_byte == mysql_responses::ERR_PACKET) {
        auto err = parseERRPacket(*response);
        return Error(remote_sqlstate::AUTH_FAILED, err->message);
    } else if (first_byte == 0x01) {
        // Auth switch request - handle additional auth methods
        return handleAuthSwitch(*response, mapping);
    }

    return Error(remote_sqlstate::PROTOCOL_ERROR, "Unexpected auth response");
}
```

### 4.4 Password Encryption

```cpp
std::vector<uint8_t> MySQLAdapter::encryptPassword(
    const std::string& password,
    const std::vector<uint8_t>& scramble,
    const std::string& auth_plugin)
{
    if (password.empty()) {
        return {};
    }

    if (auth_plugin == "mysql_native_password") {
        // SHA1(password) XOR SHA1(scramble + SHA1(SHA1(password)))
        auto hash1 = sha1(password);
        auto hash2 = sha1(hash1);

        std::vector<uint8_t> combined = scramble;
        combined.insert(combined.end(), hash2.begin(), hash2.end());
        auto hash3 = sha1(combined);

        std::vector<uint8_t> result(20);
        for (size_t i = 0; i < 20; ++i) {
            result[i] = hash1[i] ^ hash3[i];
        }
        return result;

    } else if (auth_plugin == "caching_sha2_password") {
        // SHA256(password) XOR SHA256(SHA256(SHA256(password)) + scramble)
        auto hash1 = sha256(password);
        auto hash2 = sha256(hash1);

        std::vector<uint8_t> combined = hash2;
        combined.insert(combined.end(), scramble.begin(), scramble.end());
        auto hash3 = sha256(combined);

        std::vector<uint8_t> result(32);
        for (size_t i = 0; i < 32; ++i) {
            result[i] = hash1[i] ^ hash3[i];
        }
        return result;
    }

    // Unknown plugin - try sending password directly (will likely fail)
    return std::vector<uint8_t>(password.begin(), password.end());
}
```

---

## 5. Query Execution

### 5.1 Text Protocol (COM_QUERY)

```cpp
Result<RemoteQueryResult> MySQLAdapter::executeQuery(const std::string& sql) {
    state_ = ConnectionState::EXECUTING;
    auto start_time = std::chrono::steady_clock::now();

    // Send query
    resetSequence();
    std::vector<uint8_t> payload;
    payload.push_back(mysql_commands::COM_QUERY);
    payload.insert(payload.end(), sql.begin(), sql.end());

    auto send_result = sendPacket(payload);
    if (!send_result) {
        state_ = ConnectionState::FAILED;
        return send_result.error();
    }

    // Receive response
    auto response = receivePacket();
    if (!response) {
        state_ = ConnectionState::FAILED;
        return response.error();
    }

    RemoteQueryResult result;
    result.success = true;

    uint8_t first_byte = (*response)[0];

    if (first_byte == mysql_responses::OK_PACKET) {
        // OK packet (INSERT, UPDATE, DELETE, etc.)
        auto ok = parseOKPacket(*response);
        result.rows_affected = ok->affected_rows;
        server_status_ = ok->status_flags;

    } else if (first_byte == mysql_responses::ERR_PACKET) {
        // Error
        auto err = parseERRPacket(*response);
        result.success = false;
        result.error_message = err->message;
        result.sql_state = err->sql_state;

    } else if (first_byte == mysql_responses::LOCAL_INFILE) {
        // LOAD DATA LOCAL INFILE - not supported
        result.success = false;
        result.error_message = "LOAD DATA LOCAL INFILE not supported";
        result.sql_state = "0A000";

    } else {
        // Result set - first byte is column count (length-encoded integer)
        size_t offset = 0;
        uint64_t column_count = readLengthEncodedInt(*response, offset);

        // Read column definitions
        auto columns_result = readColumnDefinitions(column_count);
        if (!columns_result) {
            state_ = ConnectionState::FAILED;
            return columns_result.error();
        }

        // Convert to RemoteColumnDesc
        for (const auto& col : *columns_result) {
            RemoteColumnDesc rcd;
            rcd.name = col.name;
            rcd.type_oid = col.type;
            rcd.type_modifier = col.length;
            rcd.nullable = !(col.flags & NOT_NULL_FLAG);
            rcd.mapped_type = mapRemoteType(col.type, col.length);
            result.columns.push_back(rcd);
        }

        // Read rows until EOF/OK packet
        while (true) {
            auto row_packet = receivePacket();
            if (!row_packet) {
                state_ = ConnectionState::FAILED;
                return row_packet.error();
            }

            uint8_t row_first = (*row_packet)[0];

            if ((row_first == mysql_responses::EOF_PACKET && row_packet->size() < 9) ||
                (row_first == mysql_responses::OK_PACKET && (client_capabilities_ & CLIENT_DEPRECATE_EOF))) {
                // End of result set
                if (row_first == mysql_responses::OK_PACKET) {
                    auto ok = parseOKPacket(*row_packet);
                    server_status_ = ok->status_flags;
                }
                break;
            }

            if (row_first == mysql_responses::ERR_PACKET) {
                auto err = parseERRPacket(*row_packet);
                result.success = false;
                result.error_message = err->message;
                result.sql_state = err->sql_state;
                break;
            }

            // Parse data row
            auto row_result = readResultRow(*row_packet, *columns_result);
            if (!row_result) {
                state_ = ConnectionState::FAILED;
                return row_result.error();
            }
            result.rows.push_back(*row_result);
        }
    }

    result.execution_time_us = std::chrono::duration_cast<
        std::chrono::microseconds>(
            std::chrono::steady_clock::now() - start_time).count();

    state_ = ConnectionState::CONNECTED;
    stats_.queries_executed++;
    return result;
}
```

### 5.2 Binary Protocol (Prepared Statements)

```cpp
Result<uint64_t> MySQLAdapter::prepare(const std::string& sql) {
    resetSequence();
    std::vector<uint8_t> payload;
    payload.push_back(mysql_commands::COM_STMT_PREPARE);
    payload.insert(payload.end(), sql.begin(), sql.end());

    auto send_result = sendPacket(payload);
    if (!send_result) return send_result.error();

    auto response = receivePacket();
    if (!response) return response.error();

    if ((*response)[0] == mysql_responses::ERR_PACKET) {
        auto err = parseERRPacket(*response);
        return Error(remote_sqlstate::REMOTE_SYNTAX_ERROR, err->message);
    }

    // Parse COM_STMT_PREPARE_OK
    size_t offset = 1;  // Skip status
    uint32_t stmt_id = readInt32LE(*response, offset);
    offset += 4;
    uint16_t num_columns = readInt16LE(*response, offset);
    offset += 2;
    uint16_t num_params = readInt16LE(*response, offset);
    offset += 2;
    // Skip filler and warning count

    MySQLPreparedStatement stmt;
    stmt.stmt_id = stmt_id;
    stmt.num_columns = num_columns;
    stmt.num_params = num_params;

    // Read parameter definitions if any
    if (num_params > 0) {
        auto params = readColumnDefinitions(num_params);
        if (params) stmt.params = *params;
    }

    // Read column definitions if any
    if (num_columns > 0) {
        auto columns = readColumnDefinitions(num_columns);
        if (columns) stmt.columns = *columns;
    }

    uint64_t local_id = next_local_stmt_id_++;
    prepared_stmts_[local_id] = stmt;
    return local_id;
}

Result<RemoteQueryResult> MySQLAdapter::executePrepared(
    uint64_t stmt_id,
    const std::vector<TypedValue>& params)
{
    auto it = prepared_stmts_.find(stmt_id);
    if (it == prepared_stmts_.end()) {
        return Error(remote_sqlstate::PREPARED_STMT_ERROR,
                     "Unknown prepared statement");
    }

    const auto& stmt = it->second;

    if (params.size() != stmt.num_params) {
        return Error(remote_sqlstate::PREPARED_STMT_ERROR,
                     "Parameter count mismatch");
    }

    resetSequence();
    std::vector<uint8_t> payload;
    payload.push_back(mysql_commands::COM_STMT_EXECUTE);
    appendInt32LE(payload, stmt.stmt_id);

    // Flags: CURSOR_TYPE_NO_CURSOR
    payload.push_back(0x00);

    // Iteration count (always 1)
    appendInt32LE(payload, 1);

    if (stmt.num_params > 0) {
        // NULL bitmap
        size_t null_bitmap_size = (stmt.num_params + 7) / 8;
        size_t null_bitmap_offset = payload.size();
        payload.insert(payload.end(), null_bitmap_size, 0);

        for (size_t i = 0; i < params.size(); ++i) {
            if (params[i].isNull()) {
                payload[null_bitmap_offset + i / 8] |= (1 << (i % 8));
            }
        }

        // New params bound flag
        payload.push_back(0x01);  // Always send types

        // Parameter types
        for (size_t i = 0; i < params.size(); ++i) {
            uint16_t type_code = getMySQLTypeCode(params[i].type());
            appendInt16LE(payload, type_code);
        }

        // Parameter values
        for (const auto& param : params) {
            if (!param.isNull()) {
                auto bytes = serializeBinaryValue(param);
                payload.insert(payload.end(), bytes.begin(), bytes.end());
            }
        }
    }

    auto send_result = sendPacket(payload);
    if (!send_result) return send_result.error();

    // Process result (similar to text protocol but binary format)
    return processExecuteResult(stmt);
}
```

---

## 6. Type Conversion

### 6.1 MySQL to ScratchBird

```cpp
ScratchBirdType MySQLAdapter::mapRemoteType(
    uint32_t type_code,
    int32_t length) const
{
    switch (type_code) {
        case MYSQL_TYPE_TINY:
            return ScratchBirdType::SMALLINT;

        case MYSQL_TYPE_SHORT:
            return ScratchBirdType::SMALLINT;

        case MYSQL_TYPE_LONG:
        case MYSQL_TYPE_INT24:
            return ScratchBirdType::INTEGER;

        case MYSQL_TYPE_LONGLONG:
            return ScratchBirdType::BIGINT;

        case MYSQL_TYPE_FLOAT:
            return ScratchBirdType::REAL;

        case MYSQL_TYPE_DOUBLE:
            return ScratchBirdType::DOUBLE;

        case MYSQL_TYPE_DECIMAL:
        case MYSQL_TYPE_NEWDECIMAL:
            return ScratchBirdType::DECIMAL;

        case MYSQL_TYPE_VARCHAR:
        case MYSQL_TYPE_VAR_STRING:
        case MYSQL_TYPE_STRING:
            if (length <= 65535) {
                return ScratchBirdType::VARCHAR;
            }
            return ScratchBirdType::TEXT;

        case MYSQL_TYPE_TINY_BLOB:
        case MYSQL_TYPE_BLOB:
        case MYSQL_TYPE_MEDIUM_BLOB:
        case MYSQL_TYPE_LONG_BLOB:
            return ScratchBirdType::BLOB;

        case MYSQL_TYPE_DATE:
        case MYSQL_TYPE_NEWDATE:
            return ScratchBirdType::DATE;

        case MYSQL_TYPE_TIME:
        case MYSQL_TYPE_TIME2:
            return ScratchBirdType::TIME;

        case MYSQL_TYPE_DATETIME:
        case MYSQL_TYPE_DATETIME2:
        case MYSQL_TYPE_TIMESTAMP:
        case MYSQL_TYPE_TIMESTAMP2:
            return ScratchBirdType::TIMESTAMP;

        case MYSQL_TYPE_YEAR:
            return ScratchBirdType::SMALLINT;

        case MYSQL_TYPE_BIT:
            return ScratchBirdType::VARBIT;

        case MYSQL_TYPE_JSON:
            return ScratchBirdType::JSON;

        case MYSQL_TYPE_ENUM:
            return ScratchBirdType::VARCHAR;

        case MYSQL_TYPE_SET:
            return ScratchBirdType::VARCHAR;

        case MYSQL_TYPE_GEOMETRY:
            return ScratchBirdType::GEOMETRY;

        default:
            return ScratchBirdType::TEXT;
    }
}
```

### 6.2 Binary Format Parsing

```cpp
TypedValue MySQLAdapter::parseBinaryValue(
    const std::vector<uint8_t>& data,
    size_t& offset,
    const ColumnDefinition& col) const
{
    switch (col.type) {
        case MYSQL_TYPE_TINY:
            return TypedValue::smallint(static_cast<int8_t>(data[offset++]));

        case MYSQL_TYPE_SHORT: {
            int16_t val = readInt16LE(data, offset);
            offset += 2;
            return TypedValue::smallint(val);
        }

        case MYSQL_TYPE_LONG: {
            int32_t val = readInt32LE(data, offset);
            offset += 4;
            return TypedValue::integer(val);
        }

        case MYSQL_TYPE_LONGLONG: {
            int64_t val = readInt64LE(data, offset);
            offset += 8;
            return TypedValue::bigint(val);
        }

        case MYSQL_TYPE_FLOAT: {
            float val;
            memcpy(&val, data.data() + offset, 4);
            offset += 4;
            return TypedValue::real(val);
        }

        case MYSQL_TYPE_DOUBLE: {
            double val;
            memcpy(&val, data.data() + offset, 8);
            offset += 8;
            return TypedValue::double_(val);
        }

        case MYSQL_TYPE_DATE: {
            uint8_t len = data[offset++];
            if (len == 0) return TypedValue::null();

            uint16_t year = readInt16LE(data, offset);
            offset += 2;
            uint8_t month = data[offset++];
            uint8_t day = data[offset++];

            return TypedValue::date(Date(year, month, day));
        }

        case MYSQL_TYPE_DATETIME:
        case MYSQL_TYPE_TIMESTAMP: {
            uint8_t len = data[offset++];
            if (len == 0) return TypedValue::null();

            uint16_t year = readInt16LE(data, offset);
            offset += 2;
            uint8_t month = data[offset++];
            uint8_t day = data[offset++];
            uint8_t hour = 0, minute = 0, second = 0;
            uint32_t microsecond = 0;

            if (len >= 7) {
                hour = data[offset++];
                minute = data[offset++];
                second = data[offset++];
            }
            if (len == 11) {
                microsecond = readInt32LE(data, offset);
                offset += 4;
            }

            return TypedValue::timestamp(
                Timestamp(year, month, day, hour, minute, second, microsecond));
        }

        case MYSQL_TYPE_VARCHAR:
        case MYSQL_TYPE_VAR_STRING:
        case MYSQL_TYPE_STRING: {
            uint64_t str_len = readLengthEncodedInt(data, offset);
            std::string str(data.begin() + offset,
                           data.begin() + offset + str_len);
            offset += str_len;
            return TypedValue::varchar(str);
        }

        case MYSQL_TYPE_BLOB: {
            uint64_t blob_len = readLengthEncodedInt(data, offset);
            std::vector<uint8_t> blob(data.begin() + offset,
                                      data.begin() + offset + blob_len);
            offset += blob_len;
            return TypedValue::blob(blob);
        }

        default:
            // Treat as string
            uint64_t len = readLengthEncodedInt(data, offset);
            std::string str(data.begin() + offset,
                           data.begin() + offset + len);
            offset += len;
            return TypedValue::text(str);
    }
}
```

---

## 7. Schema Introspection

### 7.1 List Schemas (Databases)

```cpp
Result<std::vector<std::string>> MySQLAdapter::listSchemas() {
    auto result = executeQuery("SHOW DATABASES");
    if (!result) return result.error();
    if (!result->success) {
        return Error(result->sql_state, result->error_message);
    }

    std::vector<std::string> schemas;
    for (const auto& row : result->rows) {
        std::string name = row[0].asString();
        // Skip system databases
        if (name != "information_schema" &&
            name != "mysql" &&
            name != "performance_schema" &&
            name != "sys") {
            schemas.push_back(name);
        }
    }
    return schemas;
}
```

### 7.2 List Tables

```cpp
Result<std::vector<std::string>> MySQLAdapter::listTables(
    const std::string& schema)
{
    std::string sql = "SELECT TABLE_NAME FROM information_schema.TABLES "
                      "WHERE TABLE_SCHEMA = '" + escapeString(schema) + "' "
                      "AND TABLE_TYPE = 'BASE TABLE' "
                      "ORDER BY TABLE_NAME";

    auto result = executeQuery(sql);
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

### 7.3 Describe Table

```cpp
Result<std::vector<RemoteColumnDesc>> MySQLAdapter::describeTable(
    const std::string& schema,
    const std::string& table)
{
    std::string sql = R"(
        SELECT
            COLUMN_NAME,
            DATA_TYPE,
            CHARACTER_MAXIMUM_LENGTH,
            NUMERIC_PRECISION,
            NUMERIC_SCALE,
            IS_NULLABLE,
            COLUMN_DEFAULT,
            COLUMN_TYPE
        FROM information_schema.COLUMNS
        WHERE TABLE_SCHEMA = ')" + escapeString(schema) + R"('
          AND TABLE_NAME = ')" + escapeString(table) + R"('
        ORDER BY ORDINAL_POSITION
    )";

    auto result = executeQuery(sql);
    if (!result) return result.error();
    if (!result->success) {
        return Error(result->sql_state, result->error_message);
    }

    std::vector<RemoteColumnDesc> columns;
    for (const auto& row : result->rows) {
        RemoteColumnDesc col;
        col.name = row[0].asString();

        std::string data_type = row[1].asString();
        col.type_oid = dataTypeToMySQLType(data_type);

        if (!row[2].isNull()) {
            col.type_modifier = row[2].asInteger();
        } else if (!row[3].isNull()) {
            col.type_modifier = row[3].asInteger();
        }

        col.nullable = (row[5].asString() == "YES");
        col.mapped_type = mapRemoteType(col.type_oid, col.type_modifier);

        columns.push_back(col);
    }
    return columns;
}
```

---

## 8. Capability Detection

```cpp
PushdownCapability MySQLAdapter::getCapabilities() const {
    uint32_t caps = static_cast<uint32_t>(PushdownCapability::BASIC);

    // All MySQL versions support basic pushdown
    caps |= static_cast<uint32_t>(PushdownCapability::AGGREGATE);
    caps |= static_cast<uint32_t>(PushdownCapability::GROUP_BY);
    caps |= static_cast<uint32_t>(PushdownCapability::HAVING);
    caps |= static_cast<uint32_t>(PushdownCapability::DISTINCT);

    // MySQL 8.0+ supports window functions
    if (isVersion8OrLater()) {
        caps |= static_cast<uint32_t>(PushdownCapability::WINDOW_FUNC);
        caps |= static_cast<uint32_t>(PushdownCapability::CTE);
    }

    return static_cast<PushdownCapability>(caps);
}

bool MySQLAdapter::isVersion8OrLater() const {
    // Parse server_version_ (e.g., "8.0.28-mysql" or "10.6.5-MariaDB")
    int major = 0;
    sscanf(server_version_.c_str(), "%d", &major);
    return major >= 8;
}
```

---

## 9. Error Handling

### 9.1 Error Packet Parsing

```cpp
struct ERRPacket {
    uint16_t error_code;
    std::string sql_state;
    std::string message;
};

Result<ERRPacket> MySQLAdapter::parseERRPacket(
    const std::vector<uint8_t>& data)
{
    ERRPacket err;
    size_t offset = 1;  // Skip header byte

    err.error_code = readInt16LE(data, offset);
    offset += 2;

    if (client_capabilities_ & CLIENT_PROTOCOL_41) {
        // SQL state marker (#)
        offset += 1;
        err.sql_state = std::string(data.begin() + offset,
                                    data.begin() + offset + 5);
        offset += 5;
    }

    err.message = std::string(data.begin() + offset, data.end());
    return err;
}
```

### 9.2 Error Classification

```cpp
bool MySQLAdapter::isRetryableError(uint16_t error_code) {
    switch (error_code) {
        case 1205:  // Lock wait timeout
        case 1213:  // Deadlock
        case 2002:  // Can't connect
        case 2003:  // Can't connect
        case 2006:  // Server gone away
        case 2013:  // Lost connection
        case 1317:  // Query interrupted
            return true;
        default:
            return false;
    }
}
```
