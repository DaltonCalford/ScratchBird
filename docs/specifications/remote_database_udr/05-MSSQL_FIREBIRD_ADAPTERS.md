# Remote Database UDR - MSSQL and Firebird Adapters

> Reference-only: Canonical UDR and live-migration behavior now lives in
> `ScratchBird/docs/specifications/Alpha Phase 2/11-Remote-Database-UDR-Specification.md`
> and `ScratchBird/docs/specifications/Alpha Phase 2/11h-Live-Migration-Emulated-Listener.md`.

## Part 1: MSSQL Adapter (TDS Protocol)

**Scope Note:** MSSQL/TDS adapter work is deferred until after the project goes gold. This section documents planned behavior only.

### 1.1 Overview

The MSSQL adapter implements the `IProtocolAdapter` interface for connecting to Microsoft SQL Server using the Tabular Data Stream (TDS) protocol.

**Supported Versions**: SQL Server 2016, 2017, 2019, 2022; Azure SQL Database

---

### 1.2 TDS Protocol Reference

TDS uses a packet-based format:

```
┌────────────────────────────────────────┐
│ TDS Packet Header (8 bytes)            │
├────────────────────────────────────────┤
│ 1 byte  │ Packet Type                  │
│ 1 byte  │ Status                       │
│ 2 bytes │ Length (BE)                  │
│ 2 bytes │ SPID                         │
│ 1 byte  │ Packet ID                    │
│ 1 byte  │ Window                       │
└────────────────────────────────────────┘
```

### 1.3 TDS Packet Types

```cpp
namespace tds_packet_types {
    constexpr uint8_t SQL_BATCH       = 0x01;
    constexpr uint8_t PRE_TDS7_LOGIN  = 0x02;
    constexpr uint8_t RPC             = 0x03;
    constexpr uint8_t TABULAR_RESULT  = 0x04;
    constexpr uint8_t ATTENTION       = 0x06;
    constexpr uint8_t BULK_LOAD       = 0x07;
    constexpr uint8_t TRANSACTION_MGR = 0x0E;
    constexpr uint8_t TDS7_LOGIN      = 0x10;
    constexpr uint8_t SSPI            = 0x11;
    constexpr uint8_t PRELOGIN        = 0x12;
}
```

### 1.4 TDS Token Types

```cpp
namespace tds_tokens {
    constexpr uint8_t ALTMETADATA     = 0x88;
    constexpr uint8_t ALTROW          = 0xD3;
    constexpr uint8_t COLINFO         = 0xA5;
    constexpr uint8_t COLMETADATA     = 0x81;
    constexpr uint8_t DONE            = 0xFD;
    constexpr uint8_t DONEINPROC      = 0xFF;
    constexpr uint8_t DONEPROC        = 0xFE;
    constexpr uint8_t ENVCHANGE       = 0xE3;
    constexpr uint8_t ERROR_TOKEN     = 0xAA;
    constexpr uint8_t INFO            = 0xAB;
    constexpr uint8_t LOGINACK        = 0xAD;
    constexpr uint8_t NBCROW          = 0xD2;
    constexpr uint8_t ORDER           = 0xA9;
    constexpr uint8_t RETURNSTATUS    = 0x79;
    constexpr uint8_t RETURNVALUE     = 0xAC;
    constexpr uint8_t ROW             = 0xD1;
    constexpr uint8_t SSPI_TOKEN      = 0xED;
}
```

---

### 1.5 MSSQLAdapter Class

```cpp
class MSSQLAdapter : public IProtocolAdapter {
public:
    MSSQLAdapter();
    ~MSSQLAdapter() override;

    // Connection
    Result<void> connect(const ServerDefinition& server,
                          const UserMapping& mapping) override;
    Result<void> disconnect() override;
    ConnectionState getState() const override { return state_; }
    bool isConnected() const override;

    // Query execution
    Result<RemoteQueryResult> executeQuery(const std::string& sql) override;
    Result<RemoteQueryResult> executeQueryWithParams(
        const std::string& sql,
        const std::vector<TypedValue>& params) override;

    // Prepared statements (via sp_prepare/sp_execute)
    Result<uint64_t> prepare(const std::string& sql) override;
    Result<RemoteQueryResult> executePrepared(
        uint64_t stmt_id,
        const std::vector<TypedValue>& params) override;
    Result<void> deallocatePrepared(uint64_t stmt_id) override;

    // Transaction control
    Result<void> beginTransaction() override;
    Result<void> commit() override;
    Result<void> rollback() override;
    Result<void> setSavepoint(const std::string& name) override;
    Result<void> rollbackToSavepoint(const std::string& name) override;

    // Schema introspection
    Result<std::vector<std::string>> listSchemas() override;
    Result<std::vector<std::string>> listTables(const std::string& schema) override;
    Result<std::vector<RemoteColumnDesc>> describeTable(
        const std::string& schema,
        const std::string& table) override;

    // Metadata
    RemoteDatabaseType getDatabaseType() const override {
        return RemoteDatabaseType::MSSQL;
    }
    std::string getServerVersion() const override { return server_version_; }
    PushdownCapability getCapabilities() const override;

    // Type mapping
    ScratchBirdType mapRemoteType(uint32_t remote_oid,
                                   int32_t modifier) const override;

private:
    ConnectionState state_{ConnectionState::DISCONNECTED};
    int socket_fd_{-1};
    std::unique_ptr<TLSConnection> tls_;
    std::string server_version_;
    uint32_t tds_version_{0x74000004};  // TDS 7.4
    uint16_t spid_{0};
    uint8_t packet_id_{0};

    // Login state
    std::string database_;
    std::string collation_;

    // Prepared statements
    std::unordered_map<uint64_t, int32_t> prepared_handles_;
    uint64_t next_stmt_id_{1};

    // Internal methods
    Result<void> sendPreLogin();
    Result<void> handlePreLoginResponse();
    Result<void> sendTDS7Login(const UserMapping& mapping);
    Result<void> handleLoginResponse();

    Result<void> sendPacket(uint8_t type, const std::vector<uint8_t>& data,
                            bool end_of_message = true);
    Result<std::vector<uint8_t>> receivePacket();

    Result<RemoteQueryResult> processTabularResult();
    Result<std::vector<ColumnMetadata>> parseColMetadata(
        const std::vector<uint8_t>& data, size_t& offset);
    Result<std::vector<TypedValue>> parseRow(
        const std::vector<uint8_t>& data, size_t& offset,
        const std::vector<ColumnMetadata>& columns);
};
```

---

### 1.6 Connection Establishment

```cpp
Result<void> MSSQLAdapter::connect(
    const ServerDefinition& server,
    const UserMapping& mapping)
{
    state_ = ConnectionState::CONNECTING;

    // 1. Create socket and connect
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    // ... connect to server.host:server.port ...

    // 2. Send PRELOGIN
    auto prelogin_result = sendPreLogin();
    if (!prelogin_result) return prelogin_result;

    // 3. Handle PRELOGIN response
    auto prelogin_response = handlePreLoginResponse();
    if (!prelogin_response) return prelogin_response;

    // 4. TLS handshake if required
    if (server.use_ssl || encryption_required_) {
        auto tls_result = initiateTLS(server);
        if (!tls_result) return tls_result;
    }

    // 5. Send TDS7 Login
    state_ = ConnectionState::AUTHENTICATING;
    auto login_result = sendTDS7Login(mapping);
    if (!login_result) return login_result;

    // 6. Handle login response
    auto login_response = handleLoginResponse();
    if (!login_response) return login_response;

    // 7. Set database context if specified
    if (!server.database.empty()) {
        auto use_result = executeQuery("USE " + quoteName(server.database));
        if (!use_result || !use_result->success) {
            return Error(remote_sqlstate::CONNECTION_FAILED,
                         "Failed to select database");
        }
    }

    state_ = ConnectionState::CONNECTED;
    return {};
}
```

### 1.7 TDS7 Login Packet

```cpp
Result<void> MSSQLAdapter::sendTDS7Login(const UserMapping& mapping) {
    std::vector<uint8_t> login_data;

    // Calculate offsets for variable-length fields
    uint16_t offset = 94;  // Fixed length portion

    // Convert strings to UCS-2
    auto hostname = toUCS2(getHostname());
    auto username = toUCS2(mapping.remote_user);
    auto password = encryptPassword(mapping.remote_password);
    auto appname = toUCS2("ScratchBird");
    auto servername = toUCS2(server_host_);
    auto library = toUCS2("ScratchBird Remote DB");
    auto database = toUCS2(database_);

    // Length (4 bytes) - filled later
    appendInt32LE(login_data, 0);

    // TDS version
    appendInt32BE(login_data, tds_version_);

    // Packet size
    appendInt32LE(login_data, 4096);

    // Client version
    appendInt32LE(login_data, 0x00000007);

    // Client PID
    appendInt32LE(login_data, getpid());

    // Connection ID
    appendInt32LE(login_data, 0);

    // Option flags 1
    login_data.push_back(0xE0);  // ORDER_X86 | CHAR_ASCII | FLOAT_IEEE_754

    // Option flags 2
    login_data.push_back(0x03);  // LANGUAGE | ODBC

    // Type flags
    login_data.push_back(0x00);

    // Option flags 3
    login_data.push_back(0x00);

    // Client time zone
    appendInt32LE(login_data, 0);

    // Client LCID
    appendInt32LE(login_data, 0x0409);  // English

    // Offset/length pairs for variable data
    appendOffsetLength(login_data, offset, hostname.size());
    offset += hostname.size();

    appendOffsetLength(login_data, offset, username.size());
    offset += username.size();

    appendOffsetLength(login_data, offset, password.size());
    offset += password.size();

    appendOffsetLength(login_data, offset, appname.size());
    offset += appname.size();

    appendOffsetLength(login_data, offset, servername.size());
    offset += servername.size();

    // Extension offset/length (unused)
    appendOffsetLength(login_data, 0, 0);

    appendOffsetLength(login_data, offset, library.size());
    offset += library.size();

    // Language
    appendOffsetLength(login_data, 0, 0);

    appendOffsetLength(login_data, offset, database.size());
    offset += database.size();

    // Client MAC address (6 bytes)
    login_data.insert(login_data.end(), 6, 0);

    // SSPI offset/length
    appendOffsetLength(login_data, 0, 0);

    // Attach DB filename
    appendOffsetLength(login_data, 0, 0);

    // New password
    appendOffsetLength(login_data, 0, 0);

    // SSPI long
    appendInt32LE(login_data, 0);

    // Append variable data
    login_data.insert(login_data.end(), hostname.begin(), hostname.end());
    login_data.insert(login_data.end(), username.begin(), username.end());
    login_data.insert(login_data.end(), password.begin(), password.end());
    login_data.insert(login_data.end(), appname.begin(), appname.end());
    login_data.insert(login_data.end(), servername.begin(), servername.end());
    login_data.insert(login_data.end(), library.begin(), library.end());
    login_data.insert(login_data.end(), database.begin(), database.end());

    // Update length at beginning
    writeInt32LE(login_data, 0, login_data.size());

    return sendPacket(tds_packet_types::TDS7_LOGIN, login_data);
}
```

---

### 1.8 Query Execution

```cpp
Result<RemoteQueryResult> MSSQLAdapter::executeQuery(const std::string& sql) {
    state_ = ConnectionState::EXECUTING;

    // Convert SQL to UCS-2
    auto sql_ucs2 = toUCS2(sql);

    // Build ALL_HEADERS
    std::vector<uint8_t> payload;

    // Total length of all headers (4 bytes)
    appendInt32LE(payload, 22);  // 4 + 18 (transaction descriptor header)

    // Transaction descriptor header
    appendInt32LE(payload, 18);  // Header length
    appendInt16LE(payload, 0x0002);  // Transaction descriptor type
    appendInt64LE(payload, 0);  // Transaction descriptor
    appendInt32LE(payload, 1);  // Outstanding request count

    // SQL text
    payload.insert(payload.end(), sql_ucs2.begin(), sql_ucs2.end());

    auto send_result = sendPacket(tds_packet_types::SQL_BATCH, payload);
    if (!send_result) {
        state_ = ConnectionState::FAILED;
        return send_result.error();
    }

    return processTabularResult();
}

Result<RemoteQueryResult> MSSQLAdapter::processTabularResult() {
    RemoteQueryResult result;
    result.success = true;

    std::vector<ColumnMetadata> columns;
    bool done = false;

    while (!done) {
        auto packet = receivePacket();
        if (!packet) {
            state_ = ConnectionState::FAILED;
            return packet.error();
        }

        size_t offset = 0;
        while (offset < packet->size()) {
            uint8_t token = (*packet)[offset++];

            switch (token) {
                case tds_tokens::COLMETADATA: {
                    auto cols = parseColMetadata(*packet, offset);
                    if (!cols) return cols.error();
                    columns = *cols;

                    for (const auto& col : columns) {
                        RemoteColumnDesc rcd;
                        rcd.name = col.name;
                        rcd.type_oid = col.type;
                        rcd.nullable = col.nullable;
                        rcd.mapped_type = mapRemoteType(col.type, col.length);
                        result.columns.push_back(rcd);
                    }
                    break;
                }

                case tds_tokens::ROW: {
                    auto row = parseRow(*packet, offset, columns);
                    if (!row) return row.error();
                    result.rows.push_back(*row);
                    break;
                }

                case tds_tokens::NBCROW: {
                    // Row with null bitmap compression
                    auto row = parseNBCRow(*packet, offset, columns);
                    if (!row) return row.error();
                    result.rows.push_back(*row);
                    break;
                }

                case tds_tokens::DONE:
                case tds_tokens::DONEPROC:
                case tds_tokens::DONEINPROC: {
                    uint16_t status = readInt16LE(*packet, offset);
                    offset += 2;
                    uint16_t curcmd = readInt16LE(*packet, offset);
                    offset += 2;
                    uint64_t row_count = readInt64LE(*packet, offset);
                    offset += 8;

                    result.rows_affected = row_count;

                    if (!(status & 0x0001)) {  // DONE_MORE
                        done = true;
                    }
                    break;
                }

                case tds_tokens::ERROR_TOKEN: {
                    auto err = parseErrorToken(*packet, offset);
                    result.success = false;
                    result.error_message = err.message;
                    result.sql_state = mssqlErrorToSqlState(err.number);
                    break;
                }

                case tds_tokens::INFO: {
                    // Parse but ignore info messages
                    parseInfoToken(*packet, offset);
                    break;
                }

                case tds_tokens::ENVCHANGE: {
                    parseEnvChange(*packet, offset);
                    break;
                }

                default:
                    // Skip unknown token
                    break;
            }
        }
    }

    state_ = ConnectionState::CONNECTED;
    return result;
}
```

---

### 1.9 Schema Introspection

```cpp
Result<std::vector<std::string>> MSSQLAdapter::listSchemas() {
    auto result = executeQuery(R"(
        SELECT name FROM sys.schemas
        WHERE schema_id < 16384  -- User schemas only
          AND name NOT IN ('guest', 'INFORMATION_SCHEMA', 'sys')
        ORDER BY name
    )");

    if (!result || !result->success) {
        return Error(result->sql_state, result->error_message);
    }

    std::vector<std::string> schemas;
    for (const auto& row : result->rows) {
        schemas.push_back(row[0].asString());
    }
    return schemas;
}

Result<std::vector<RemoteColumnDesc>> MSSQLAdapter::describeTable(
    const std::string& schema,
    const std::string& table)
{
    std::string sql = R"(
        SELECT
            c.name AS column_name,
            t.name AS type_name,
            c.max_length,
            c.precision,
            c.scale,
            c.is_nullable,
            dc.definition AS default_value
        FROM sys.columns c
        JOIN sys.types t ON c.user_type_id = t.user_type_id
        JOIN sys.tables tb ON c.object_id = tb.object_id
        JOIN sys.schemas s ON tb.schema_id = s.schema_id
        LEFT JOIN sys.default_constraints dc ON c.default_object_id = dc.object_id
        WHERE s.name = ')" + escapeString(schema) + R"('
          AND tb.name = ')" + escapeString(table) + R"('
        ORDER BY c.column_id
    )";

    auto result = executeQuery(sql);
    if (!result || !result->success) {
        return Error(result->sql_state, result->error_message);
    }

    std::vector<RemoteColumnDesc> columns;
    for (const auto& row : result->rows) {
        RemoteColumnDesc col;
        col.name = row[0].asString();
        col.type_oid = typeNameToOid(row[1].asString());
        col.type_modifier = row[2].asInteger();
        col.nullable = row[5].asBoolean();
        col.mapped_type = mapRemoteType(col.type_oid, col.type_modifier);
        columns.push_back(col);
    }
    return columns;
}
```

---

## Part 2: Firebird Adapter

### 2.1 Overview

The Firebird adapter implements the `IProtocolAdapter` interface for connecting to Firebird 2.5+ servers using the native wire protocol.

**Supported Versions**: Firebird 2.5, 3.0, 4.0, 5.0

---

### 2.2 Firebird Wire Protocol

Firebird uses XDR encoding for wire communication; packets are XDR-encoded `PACKET` unions with `p_operation` first (32-bit enum), followed by op-specific fields. There is no fixed length field at the protocol layer.

```
┌────────────────────────────────────────┐
│ Firebird Packet                        │
├────────────────────────────────────────┤
│ 4 bytes │ Operation code (BE)          │
│ n bytes │ XDR-encoded parameters       │
└────────────────────────────────────────┘
```

### 2.3 Operation Codes

```cpp
namespace fb_operations {
    constexpr int32_t op_connect        = 1;
    constexpr int32_t op_accept         = 3;
    constexpr int32_t op_reject         = 4;
    constexpr int32_t op_accept_data    = 94;
    constexpr int32_t op_cond_accept    = 98;
    constexpr int32_t op_attach         = 19;
    constexpr int32_t op_create         = 20;
    constexpr int32_t op_detach         = 21;
    constexpr int32_t op_compile        = 22;
    constexpr int32_t op_start          = 23;
    constexpr int32_t op_start_and_send = 24;
    constexpr int32_t op_send           = 25;
    constexpr int32_t op_receive        = 26;
    constexpr int32_t op_release        = 28;
    constexpr int32_t op_transaction    = 29;
    constexpr int32_t op_commit         = 30;
    constexpr int32_t op_rollback       = 31;
    constexpr int32_t op_commit_retaining = 50;
    constexpr int32_t op_info_database  = 40;
    constexpr int32_t op_info_transaction = 42;
    constexpr int32_t op_info_sql       = 70;
    constexpr int32_t op_allocate_statement = 62;
    constexpr int32_t op_prepare_statement = 68;
    constexpr int32_t op_execute        = 63;
    constexpr int32_t op_exec_immediate = 64;
    constexpr int32_t op_fetch          = 65;
    constexpr int32_t op_fetch_response = 66;
    constexpr int32_t op_free_statement = 67;
    constexpr int32_t op_exec_immediate2 = 75;
    constexpr int32_t op_execute2       = 76;
    constexpr int32_t op_sql_response   = 78;
    constexpr int32_t op_crypt          = 96;
    constexpr int32_t op_crypt_key_callback = 97;
    constexpr int32_t op_cont_auth      = 92;

    // Protocol 17+ batch/replication opcodes (for completeness)
    constexpr int32_t op_batch_create   = 99;
    constexpr int32_t op_batch_msg      = 100;
    constexpr int32_t op_batch_exec     = 101;
    constexpr int32_t op_batch_rls      = 102;
    constexpr int32_t op_batch_cs       = 103;
    constexpr int32_t op_batch_regblob  = 104;
    constexpr int32_t op_batch_blob_stream = 105;
    constexpr int32_t op_batch_set_bpb  = 106;
    constexpr int32_t op_repl_data      = 107;
    constexpr int32_t op_repl_req       = 108;
    constexpr int32_t op_batch_cancel   = 109;
    constexpr int32_t op_batch_sync     = 110;
    constexpr int32_t op_info_batch     = 111;
    constexpr int32_t op_fetch_scroll   = 112;
    constexpr int32_t op_info_cursor    = 113;
    constexpr int32_t op_inline_blob    = 114;

    // Response codes
    constexpr int32_t op_response       = 9;
}
```

---

### 2.4 FirebirdAdapter Class

```cpp
class FirebirdAdapter : public IProtocolAdapter {
public:
    FirebirdAdapter();
    ~FirebirdAdapter() override;

    Result<void> connect(const ServerDefinition& server,
                          const UserMapping& mapping) override;
    Result<void> disconnect() override;
    ConnectionState getState() const override { return state_; }
    bool isConnected() const override;

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

    Result<std::vector<std::string>> listSchemas() override;
    Result<std::vector<std::string>> listTables(const std::string& schema) override;
    Result<std::vector<RemoteColumnDesc>> describeTable(
        const std::string& schema,
        const std::string& table) override;

    RemoteDatabaseType getDatabaseType() const override {
        return RemoteDatabaseType::FIREBIRD;
    }
    std::string getServerVersion() const override { return server_version_; }
    PushdownCapability getCapabilities() const override;

    ScratchBirdType mapRemoteType(uint32_t remote_oid,
                                   int32_t modifier) const override;

private:
    ConnectionState state_{ConnectionState::DISCONNECTED};
    int socket_fd_{-1};
    std::unique_ptr<TLSConnection> tls_;
    std::string server_version_;

    // Firebird handles
    uint32_t db_handle_{0};
    uint32_t tr_handle_{0};

    // Wire protocol version
    uint32_t protocol_version_{0};
    uint32_t accept_type_{0};

    // Prepared statements
    struct FirebirdStatement {
        uint32_t stmt_handle;
        std::vector<XSQLDA> input_params;
        std::vector<XSQLDA> output_columns;
    };
    std::unordered_map<uint64_t, FirebirdStatement> prepared_stmts_;
    uint64_t next_stmt_id_{1};

    // XDR encoding/decoding
    void writeXdrInt32(std::vector<uint8_t>& buffer, int32_t value);
    void writeXdrString(std::vector<uint8_t>& buffer, const std::string& str);
    void writeXdrOpaque(std::vector<uint8_t>& buffer,
                        const std::vector<uint8_t>& data);

    int32_t readXdrInt32(const std::vector<uint8_t>& buffer, size_t& offset);
    std::string readXdrString(const std::vector<uint8_t>& buffer, size_t& offset);
    std::vector<uint8_t> readXdrOpaque(const std::vector<uint8_t>& buffer,
                                        size_t& offset);

    Result<void> sendOperation(int32_t op, const std::vector<uint8_t>& params);
    Result<std::pair<int32_t, std::vector<uint8_t>>> receiveResponse();

    Result<void> doConnect(const ServerDefinition& server);
    Result<void> doAttach(const ServerDefinition& server,
                           const UserMapping& mapping);
    Result<void> doStartTransaction();

    Result<uint32_t> allocateStatement();
    Result<void> prepareStatement(uint32_t handle, const std::string& sql);
    Result<std::vector<XSQLDA>> describeInput(uint32_t handle);
    Result<std::vector<XSQLDA>> describeOutput(uint32_t handle);
};
```

---

### 2.5 Connection Establishment

```cpp
Result<void> FirebirdAdapter::connect(
    const ServerDefinition& server,
    const UserMapping& mapping)
{
    state_ = ConnectionState::CONNECTING;

    // 1. Create socket and connect
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    // ... connect to server.host:server.port (default 3050) ...

    // 2. Send op_connect
    auto connect_result = doConnect(server);
    if (!connect_result) {
        disconnect();
        return connect_result;
    }

    // 3. Attach to database
    state_ = ConnectionState::AUTHENTICATING;
    auto attach_result = doAttach(server, mapping);
    if (!attach_result) {
        disconnect();
        return attach_result;
    }

    state_ = ConnectionState::CONNECTED;
    return {};
}

Result<void> FirebirdAdapter::doConnect(const ServerDefinition& server) {
    std::vector<uint8_t> params;

    // P_CNCT header
    writeXdrInt32(params, 0);  // p_cnct_operation (unused)
    writeXdrInt32(params, 3);  // CONNECT_VERSION3
    writeXdrInt32(params, 1);  // ARCH_GENERIC

    // p_cnct_file (database path)
    writeXdrString(params, server.database);

    // p_cnct_count
    writeXdrInt32(params, 1);  // One protocol entry

    // p_cnct_user_id TLV block
    std::vector<uint8_t> user_id;
    user_id.push_back(1);  // CNCT_user
    user_id.push_back(mapping.remote_user.size());
    user_id.insert(user_id.end(), mapping.remote_user.begin(), mapping.remote_user.end());
    user_id.push_back(2);  // CNCT_passwd
    user_id.push_back(mapping.remote_password.size());
    user_id.insert(user_id.end(), mapping.remote_password.begin(), mapping.remote_password.end());
    writeXdrOpaque(params, user_id);

    // p_cnct_versions[0]
    writeXdrInt32(params, 13); // PROTOCOL_VERSION13
    writeXdrInt32(params, 1);  // ARCH_GENERIC
    writeXdrInt32(params, 0);  // min_type (unused)
    writeXdrInt32(params, 5);  // max_type (ptype_lazy_send)
    writeXdrInt32(params, 1);  // weight

    auto result = sendOperation(fb_operations::op_connect, params);
    if (!result) return result;

    auto response = receiveResponse();
    if (!response) return response.error();

    if (response->first == fb_operations::op_reject) {
        return Error(remote_sqlstate::CONNECTION_FAILED,
                     "Connection rejected by server");
    }

    if (response->first == fb_operations::op_accept ||
        response->first == fb_operations::op_accept_data ||
        response->first == fb_operations::op_cond_accept) {
        size_t offset = 0;
        protocol_version_ = readXdrInt32(response->second, offset);
        accept_type_ = readXdrInt32(response->second, offset);
        return {};
    }

    return Error(remote_sqlstate::PROTOCOL_ERROR,
                 "Unexpected response: " + std::to_string(response->first));
}

Result<void> FirebirdAdapter::doAttach(
    const ServerDefinition& server,
    const UserMapping& mapping)
{
    std::vector<uint8_t> params;

    // P_ATCH: database handle (0 for new), file name, DPB
    writeXdrInt32(params, 0);
    writeXdrString(params, server.database);

    // DPB (Database Parameter Block)
    std::vector<uint8_t> dpb;
    dpb.push_back(1);  // isc_dpb_version1

    // User name
    dpb.push_back(28);  // isc_dpb_user_name
    dpb.push_back(mapping.remote_user.size());
    dpb.insert(dpb.end(), mapping.remote_user.begin(), mapping.remote_user.end());

    // Password
    dpb.push_back(29);  // isc_dpb_password
    dpb.push_back(mapping.remote_password.size());
    dpb.insert(dpb.end(), mapping.remote_password.begin(), mapping.remote_password.end());

    // Character set
    dpb.push_back(68);  // isc_dpb_charset
    std::string charset = "UTF8";
    dpb.push_back(charset.size());
    dpb.insert(dpb.end(), charset.begin(), charset.end());

    writeXdrOpaque(params, dpb);

    auto result = sendOperation(fb_operations::op_attach, params);
    if (!result) return result;

    auto response = receiveResponse();
    if (!response) return response.error();

    if (response->first != fb_operations::op_response) {
        return Error(remote_sqlstate::AUTH_FAILED,
                     "Attach failed");
    }

    size_t offset = 0;
    db_handle_ = readXdrInt32(response->second, offset);

    // Parse status vector for errors
    auto status = parseStatusVector(response->second, offset);
    if (status.hasError()) {
        return Error(remote_sqlstate::AUTH_FAILED, status.message);
    }

    return {};
}
```

Authentication notes:
- Servers may use `op_accept_data` / `op_cond_accept` with plugin data; follow up with `op_cont_auth` exchanges until authentication completes.
- If wire encryption is negotiated (`op_crypt` / `op_crypt_key_callback`), wrap all subsequent traffic per the negotiated plugin.

---

### 2.6 Query Execution

```cpp
Result<RemoteQueryResult> FirebirdAdapter::executeQuery(const std::string& sql) {
    state_ = ConnectionState::EXECUTING;

    // Start transaction if needed
    if (tr_handle_ == 0) {
        auto tr_result = doStartTransaction();
        if (!tr_result) return tr_result.error();
    }

    // Allocate statement
    auto stmt_result = allocateStatement();
    if (!stmt_result) return stmt_result.error();
    uint32_t stmt_handle = *stmt_result;

    // Prepare statement
    auto prep_result = prepareStatement(stmt_handle, sql);
    if (!prep_result) {
        freeStatement(stmt_handle);
        return prep_result.error();
    }

    // Describe output
    auto output_result = describeOutput(stmt_handle);
    if (!output_result) {
        freeStatement(stmt_handle);
        return output_result.error();
    }

    RemoteQueryResult result;
    result.success = true;

    // Convert XSQLDA to RemoteColumnDesc
    for (const auto& sqlvar : *output_result) {
        RemoteColumnDesc col;
        col.name = sqlvar.sqlname;
        col.type_oid = sqlvar.sqltype & ~1;  // Remove null flag
        col.nullable = (sqlvar.sqltype & 1) != 0;
        col.type_modifier = sqlvar.sqllen;
        col.mapped_type = mapRemoteType(col.type_oid, col.type_modifier);
        result.columns.push_back(col);
    }

    // Execute
    std::vector<uint8_t> exec_params;
    writeXdrInt32(exec_params, stmt_handle);
    writeXdrInt32(exec_params, tr_handle_);
    writeXdrOpaque(exec_params, {});  // No input params
    writeXdrInt32(exec_params, 0);    // No flags

    auto exec_result = sendOperation(fb_operations::op_execute2, exec_params);
    if (!exec_result) {
        freeStatement(stmt_handle);
        return exec_result.error();
    }

    // Fetch rows
    bool has_rows = !output_result->empty();
    while (has_rows) {
        std::vector<uint8_t> fetch_params;
        writeXdrInt32(fetch_params, stmt_handle);
        writeXdrInt32(fetch_params, 100);  // Fetch 100 rows

        sendOperation(fb_operations::op_fetch, fetch_params);
        auto fetch_response = receiveResponse();

        if (!fetch_response) {
            freeStatement(stmt_handle);
            return fetch_response.error();
        }

        if (fetch_response->first == fb_operations::op_fetch_response) {
            size_t offset = 0;
            int32_t status = readXdrInt32(fetch_response->second, offset);
            int32_t count = readXdrInt32(fetch_response->second, offset);

            if (status == 100 || count == 0) {  // End of data
                has_rows = false;
            } else {
                // Parse rows
                for (int32_t i = 0; i < count; ++i) {
                    std::vector<TypedValue> row;
                    for (const auto& sqlvar : *output_result) {
                        auto value = readFirebirdValue(fetch_response->second,
                                                        offset, sqlvar);
                        row.push_back(value);
                    }
                    result.rows.push_back(row);
                }
            }
        } else {
            has_rows = false;
        }
    }

    freeStatement(stmt_handle);
    state_ = ConnectionState::IN_TRANSACTION;
    return result;
}
```

---

### 2.7 Type Mapping

```cpp
ScratchBirdType FirebirdAdapter::mapRemoteType(
    uint32_t sql_type,
    int32_t length) const
{
    // Remove null indicator flag
    uint32_t type = sql_type & ~1;

    switch (type) {
        case SQL_SHORT:   return ScratchBirdType::SMALLINT;
        case SQL_LONG:    return ScratchBirdType::INTEGER;
        case SQL_INT64:   return ScratchBirdType::BIGINT;
        case SQL_INT128:  return ScratchBirdType::DECIMAL;

        case SQL_FLOAT:   return ScratchBirdType::REAL;
        case SQL_DOUBLE:
        case SQL_D_FLOAT: return ScratchBirdType::DOUBLE;

        case SQL_DEC_FIXED:
        case SQL_DEC64:
        case SQL_DEC128:  return ScratchBirdType::DECIMAL;

        case SQL_TEXT:    return ScratchBirdType::CHAR;
        case SQL_VARYING: return ScratchBirdType::VARCHAR;

        case SQL_BLOB:
            // Check subtype
            if (length == 1) {  // Text blob
                return ScratchBirdType::TEXT;
            }
            return ScratchBirdType::BLOB;

        case SQL_TYPE_DATE: return ScratchBirdType::DATE;
        case SQL_TYPE_TIME: return ScratchBirdType::TIME;
        case SQL_TIMESTAMP: return ScratchBirdType::TIMESTAMP;
        case SQL_TIME_TZ:   return ScratchBirdType::TIME_TZ;
        case SQL_TIMESTAMP_TZ: return ScratchBirdType::TIMESTAMP_TZ;

        case SQL_BOOLEAN: return ScratchBirdType::BOOLEAN;

        default: return ScratchBirdType::TEXT;
    }
}
```

---

### 2.8 Schema Introspection

```cpp
Result<std::vector<std::string>> FirebirdAdapter::listTables(
    const std::string& schema)
{
    // Firebird doesn't have schemas in the same way - use RDB$RELATIONS
    auto result = executeQuery(R"(
        SELECT TRIM(RDB$RELATION_NAME)
        FROM RDB$RELATIONS
        WHERE RDB$SYSTEM_FLAG = 0
          AND RDB$VIEW_BLR IS NULL
        ORDER BY RDB$RELATION_NAME
    )");

    if (!result || !result->success) {
        return Error(result->sql_state, result->error_message);
    }

    std::vector<std::string> tables;
    for (const auto& row : result->rows) {
        tables.push_back(row[0].asString());
    }
    return tables;
}

Result<std::vector<RemoteColumnDesc>> FirebirdAdapter::describeTable(
    const std::string& schema,
    const std::string& table)
{
    std::string sql = R"(
        SELECT
            TRIM(RF.RDB$FIELD_NAME) AS column_name,
            F.RDB$FIELD_TYPE AS field_type,
            F.RDB$FIELD_LENGTH AS field_length,
            F.RDB$FIELD_SCALE AS field_scale,
            RF.RDB$NULL_FLAG AS not_null,
            RF.RDB$DEFAULT_SOURCE AS default_value
        FROM RDB$RELATION_FIELDS RF
        JOIN RDB$FIELDS F ON RF.RDB$FIELD_SOURCE = F.RDB$FIELD_NAME
        WHERE RF.RDB$RELATION_NAME = ')" + escapeString(table) + R"('
        ORDER BY RF.RDB$FIELD_POSITION
    )";

    auto result = executeQuery(sql);
    if (!result || !result->success) {
        return Error(result->sql_state, result->error_message);
    }

    std::vector<RemoteColumnDesc> columns;
    for (const auto& row : result->rows) {
        RemoteColumnDesc col;
        col.name = row[0].asString();
        col.type_oid = row[1].asInteger();
        col.type_modifier = row[2].asInteger();
        col.nullable = row[4].isNull() || row[4].asInteger() == 0;
        col.mapped_type = mapRemoteType(col.type_oid, col.type_modifier);
        columns.push_back(col);
    }
    return columns;
}
```

---

## Part 3: ScratchBird Native Adapter

### 3.1 Overview

The ScratchBird adapter enables federation between ScratchBird instances using the native wire protocol.

```cpp
class ScratchBirdAdapter : public IProtocolAdapter {
public:
    ScratchBirdAdapter();
    ~ScratchBirdAdapter() override;

    Result<void> connect(const ServerDefinition& server,
                          const UserMapping& mapping) override;
    // ... implements full IProtocolAdapter interface ...

    RemoteDatabaseType getDatabaseType() const override {
        return RemoteDatabaseType::SCRATCHBIRD;
    }

    // Full pushdown capability for ScratchBird-to-ScratchBird
    PushdownCapability getCapabilities() const override {
        return PushdownCapability::FULL;
    }

    // Direct SBLR bytecode transmission (optimization)
    Result<RemoteQueryResult> executeByteCode(
        const std::vector<uint8_t>& sblr_bytecode);

private:
    // Uses native wire protocol from scratchbird_native_wire_protocol.md
    // Direct type mapping (no conversion needed for same types)
    // Cluster PKI authentication for federated queries
};
```

Protocol notes:
- Handshake uses `STARTUP` → `AUTH_REQUEST`/`AUTH_RESPONSE` → `AUTH_OK` → `READY`.
- After `AUTH_OK`, every message must carry non-zero `attachment_id` and `txn_id` in the header.
- If `MSG_FLAG_CHECKSUM` is set, validate CRC32C as specified in `scratchbird_native_wire_protocol.md`.
- Support `NEGOTIATE_VERSION` and `PARAMETER_STATUS` during startup before `READY`.

### 3.2 Advantages of Native Adapter

- **No Type Conversion**: Same 86-type system on both ends
- **Direct SBLR**: Can send pre-compiled bytecode for maximum performance
- **Cluster Trust**: Uses Cluster PKI for seamless cross-database authentication
- **Full MGA Semantics**: Preserves MGA visibility rules across federation
- **Query Plan Sharing**: Can share query plans for distributed optimization
