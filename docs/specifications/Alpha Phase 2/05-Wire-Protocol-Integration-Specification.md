# Wire Protocol Integration Specification

**Document Version:** 1.0  
**Date:** 2025-01-25  
**Status:** Draft Specification

---

## Overview

This document specifies how the database engine provides drop-in compatibility with PostgreSQL, MySQL, Firebird, and MSSQL wire protocols. MSSQL/TDS is Beta scope.

---

## Design Goals

1. **Wire Compatibility**: Support native wire protocols directly (no external client libraries required)
2. **Parser Isolation**: Each protocol has dedicated parser plugin
3. **Hot-Swappable**: Update parsers without engine downtime
4. **System Catalog Virtualization**: Emulate protocol-specific system tables
5. **Authentication Integration**: Unified auth backend with protocol-specific handshakes

---

## Architecture

### Per-Protocol Process

```
┌─────────────────────────────────────────────────────────┐
│  Protocol Handler Process (One per protocol)            │
│                                                          │
│  ┌────────────────────────────────────────────────────┐ │
│  │  Wire Protocol Handler                             │ │
│  │  - Listen on protocol-specific port                │ │
│  │  - Connection pooling                              │ │
│  │  - Protocol-specific authentication               │ │
│  │  - Message encoding/decoding                       │ │
│  │  - Session state management                        │ │
│  └────────────────────┬───────────────────────────────┘ │
│                       │                                  │
│  ┌────────────────────▼───────────────────────────────┐ │
│  │  SQL Parser Plugin (.so / .dll)                    │ │
│  │  - Loaded dynamically                              │ │
│  │  - Dialect-specific SQL parsing                    │ │
│  │  - AST → Bytecode compilation                      │ │
│  │  - Statement cache (SQL → Bytecode)               │ │
│  │  - System catalog query virtualization            │ │
│  └────────────────────┬───────────────────────────────┘ │
│                       │                                  │
│  ┌────────────────────▼───────────────────────────────┐ │
│  │  Engine Client (Unix Domain Socket)               │ │
│  │  - Send bytecode + parameters                      │ │
│  │  - Receive result sets                             │ │
│  │  - MessagePack/Protobuf serialization             │ │
│  └──────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────┘
         │
         ▼ Unix Domain Socket
    Engine Core
```

---

## Protocol Specifications

### PostgreSQL Wire Protocol

**Port**: 5432  
**Documentation**: https://www.postgresql.org/docs/current/protocol.html

#### Key Messages

```c
// Startup
StartupMessage:
  protocol_version: 3.0
  user: string
  database: string
  options: key-value pairs

AuthenticationRequest:
  type: OK (0) | Cleartext (3) | MD5 (5) | SCRAM-SHA-256 (10)

// Query flow
Query:
  query_string: string

Parse:
  prepared_statement_name: string
  query_string: string
  param_types: oid[]

Bind:
  portal_name: string
  prepared_statement_name: string
  parameters: binary[]

Execute:
  portal_name: string
  max_rows: int32

// Response
RowDescription:
  field_count: int16
  fields: [name, table_oid, column_id, type_oid, type_size, type_mod, format][]

DataRow:
  column_count: int16
  column_values: bytes[]

CommandComplete:
  command_tag: string (e.g., "INSERT 0 1")
```

#### Implementation

```c
// PostgreSQL protocol handler
struct PGProtocolHandler {
    int listen_socket;
    ThreadPool* connection_threads;
    SessionManager* sessions;
    PGParserPlugin* parser;
};

// Handle PostgreSQL connection
void* pg_connection_handler(void* arg) {
    PGConnection* conn = (PGConnection*)arg;
    
    // 1. Receive startup message
    StartupMessage startup = pg_recv_startup(conn->socket);
    
    // 2. Authenticate
    if (!pg_authenticate(conn, &startup)) {
        pg_send_error(conn, "28000", "Authentication failed");
        return NULL;
    }
    
    // 3. Send auth OK + ready for query
    pg_send_auth_ok(conn);
    pg_send_ready_for_query(conn, 'I');  // Idle
    
    // 4. Main query loop
    while (conn->alive) {
        PGMessage msg = pg_recv_message(conn->socket);
        
        switch (msg.type) {
            case 'Q':  // Simple query
                pg_handle_simple_query(conn, msg.query_string);
                break;
                
            case 'P':  // Parse
                pg_handle_parse(conn, &msg.parse);
                break;
                
            case 'B':  // Bind
                pg_handle_bind(conn, &msg.bind);
                break;
                
            case 'E':  // Execute
                pg_handle_execute(conn, &msg.execute);
                break;
                
            case 'X':  // Terminate
                conn->alive = false;
                break;
        }
    }
    
    return NULL;
}

// Handle simple query
void pg_handle_simple_query(PGConnection* conn, const char* sql) {
    // 1. Parse SQL → bytecode
    BytecodeProgram* bc = parser_parse_sql(conn->parser, sql);
    
    if (!bc) {
        pg_send_error(conn, "42601", "Syntax error");
        return;
    }
    
    // 2. Send to engine
    EngineRequest req = {
        .session_id = conn->session_id,
        .bytecode = bc,
        .parameters = NULL,
    };
    
    EngineResponse* resp = engine_execute(&req);
    
    if (resp->error) {
        pg_send_error(conn, resp->error->sqlstate, resp->error->message);
        return;
    }
    
    // 3. Format result set
    if (resp->result_set) {
        // Send RowDescription
        pg_send_row_description(conn, resp->result_set->columns);
        
        // Send DataRows
        for (int i = 0; i < resp->result_set->row_count; i++) {
            pg_send_data_row(conn, resp->result_set->rows[i]);
        }
        
        // Send CommandComplete
        pg_send_command_complete(conn, resp->command_tag);
    } else {
        // Non-SELECT (INSERT/UPDATE/DELETE)
        pg_send_command_complete(conn, resp->command_tag);
    }
    
    // Ready for next query
    pg_send_ready_for_query(conn, 'I');
}
```

#### System Catalog Virtualization

```sql
-- PostgreSQL system catalogs that must be supported
pg_catalog.pg_tables
pg_catalog.pg_class
pg_catalog.pg_attribute
pg_catalog.pg_index
pg_catalog.pg_namespace
information_schema.tables
information_schema.columns
... (100+ system tables)

-- Example: Query on pg_catalog.pg_tables
SELECT schemaname, tablename 
FROM pg_catalog.pg_tables
WHERE schemaname = 'public';

-- Parser detects system catalog query
-- Translates to engine's internal metadata query
-- Formats result to match PostgreSQL's pg_tables schema
```

```c
// Virtual system catalog handler
bool is_system_catalog_query(const char* sql) {
    return (strstr(sql, "pg_catalog.") != NULL ||
            strstr(sql, "information_schema.") != NULL);
}

BytecodeProgram* translate_system_catalog_query(const char* sql) {
    // Parse SQL
    AST* ast = parse_sql(sql);
    
    // Identify catalog table
    const char* catalog_table = extract_catalog_table(ast);
    
    if (strcmp(catalog_table, "pg_catalog.pg_tables") == 0) {
        // Translate to internal metadata query
        return generate_list_tables_bytecode(ast->where_clause);
    } else if (strcmp(catalog_table, "pg_catalog.pg_attribute") == 0) {
        return generate_list_columns_bytecode(ast->where_clause);
    }
    // ... handle other catalog tables
    
    return NULL;
}
```

---

### MySQL Wire Protocol

**Port**: 3306  
**Documentation**: https://dev.mysql.com/doc/internals/en/client-server-protocol.html

#### Key Messages

```c
// Handshake
HandshakeV10:
  protocol_version: 10
  server_version: string
  connection_id: uint32
  auth_plugin_data: bytes[8]
  capability_flags: uint32
  character_set: uint8
  status_flags: uint16

HandshakeResponse:
  capability_flags: uint32
  max_packet_size: uint32
  character_set: uint8
  username: string
  auth_response: bytes
  database: string

// Commands
COM_QUERY:
  command: 0x03
  query: string

COM_STMT_PREPARE:
  command: 0x16
  query: string

COM_STMT_EXECUTE:
  command: 0x17
  statement_id: uint32
  flags: uint8
  iteration_count: uint32
  parameters: binary[]

// Response
ResultSet:
  column_count: lenenc_int
  columns: ColumnDefinition[]
  rows: Row[]
  EOF or OK
```

#### Implementation

```c
// MySQL protocol handler
void* mysql_connection_handler(void* arg) {
    MySQLConnection* conn = (MySQLConnection*)arg;
    
    // 1. Send handshake
    mysql_send_handshake(conn);
    
    // 2. Receive handshake response
    HandshakeResponse resp = mysql_recv_handshake_response(conn->socket);
    
    // 3. Authenticate
    if (!mysql_authenticate(conn, &resp)) {
        mysql_send_err(conn, 1045, "28000", "Access denied");
        return NULL;
    }
    
    mysql_send_ok(conn, 0, 0, 0, 0, NULL);
    
    // 4. Command loop
    while (conn->alive) {
        MySQLCommand cmd = mysql_recv_command(conn->socket);
        
        switch (cmd.type) {
            case COM_QUERY:
                mysql_handle_query(conn, cmd.query);
                break;
                
            case COM_STMT_PREPARE:
                mysql_handle_prepare(conn, cmd.query);
                break;
                
            case COM_STMT_EXECUTE:
                mysql_handle_execute(conn, &cmd.execute);
                break;
                
            case COM_QUIT:
                conn->alive = false;
                break;
        }
    }
    
    return NULL;
}

// Handle COM_QUERY
void mysql_handle_query(MySQLConnection* conn, const char* sql) {
    // Parse SQL
    BytecodeProgram* bc = parser_parse_sql(conn->parser, sql);
    
    if (!bc) {
        mysql_send_err(conn, 1064, "42000", "Syntax error");
        return;
    }
    
    // Execute
    EngineResponse* resp = engine_execute_query(conn->session_id, bc, NULL);
    
    if (resp->error) {
        mysql_send_err(conn, 1105, resp->error->sqlstate, resp->error->message);
        return;
    }
    
    // Format result
    if (resp->result_set) {
        // Send column count
        mysql_send_column_count(conn, resp->result_set->column_count);
        
        // Send column definitions
        for (int i = 0; i < resp->result_set->column_count; i++) {
            mysql_send_column_def(conn, &resp->result_set->columns[i]);
        }
        
        mysql_send_eof(conn, 0, 0);
        
        // Send rows
        for (int i = 0; i < resp->result_set->row_count; i++) {
            mysql_send_text_result_row(conn, resp->result_set->rows[i]);
        }
        
        mysql_send_eof(conn, 0, 0);
    } else {
        // OK packet for INSERT/UPDATE/DELETE
        mysql_send_ok(conn, resp->affected_rows, 0, 0, 0, NULL);
    }
}
```

#### MySQL System Catalogs

```sql
-- MySQL uses information_schema
information_schema.TABLES
information_schema.COLUMNS
information_schema.STATISTICS
mysql.user
mysql.db
... (50+ tables)
```

---

### MSSQL TDS Protocol (post-gold)

**Scope Note:** TDS/MSSQL support is deferred until after the project goes gold. This section is retained for future compatibility work.

**Port**: 1433  
**Documentation**: https://docs.microsoft.com/en-us/openspecs/windows_protocols/ms-tds/

#### Key Messages

```c
// Prelogin
PRELOGIN:
  version: bytes[6]
  encryption: uint8
  instance: string
  thread_id: uint32

// Login
LOGIN7:
  hostname: string
  username: string
  password: string
  app_name: string
  server_name: string
  library_name: string

// SQL Batch
SQL_BATCH:
  sql_text: string

// RPC (Remote Procedure Call)
RPC_REQUEST:
  proc_name: string
  parameters: []

// Response
DONE:
  status: uint16
  cur_cmd: uint16
  row_count: uint64

COLMETADATA:
  column_count: uint16
  columns: []

ROW:
  column_data: []
```

#### Implementation

```c
// TDS protocol handler
void* tds_connection_handler(void* arg) {
    TDSConnection* conn = (TDSConnection*)arg;
    
    // 1. Receive PRELOGIN
    TDSPrelogin prelogin = tds_recv_prelogin(conn->socket);
    
    // 2. Send PRELOGIN response
    tds_send_prelogin_response(conn, &prelogin);
    
    // 3. Receive LOGIN7
    TDSLogin7 login = tds_recv_login7(conn->socket);
    
    // 4. Authenticate
    if (!tds_authenticate(conn, &login)) {
        tds_send_error(conn, 18456, 14, "Login failed");
        return NULL;
    }
    
    tds_send_login_ack(conn);
    tds_send_done(conn, 0, 0, 0);
    
    // 5. Command loop
    while (conn->alive) {
        TDSPacket pkt = tds_recv_packet(conn->socket);
        
        switch (pkt.type) {
            case TDS_SQL_BATCH:
                tds_handle_sql_batch(conn, pkt.sql_text);
                break;
                
            case TDS_RPC:
                tds_handle_rpc(conn, &pkt.rpc);
                break;
                
            case TDS_ATTENTION:
                // Query cancellation
                tds_handle_attention(conn);
                break;
        }
    }
    
    return NULL;
}
```

---

### Firebird Wire Protocol

**Port**: 3050  
**Documentation**: https://firebirdsql.org/file/documentation/html/en/refdocs/fblangref40/firebird-40-language-reference.html

#### Key Messages

```c
// Connect
op_connect:
  database_name: string
  user: string
  password: string

// Attach database
op_attach:
  database_handle: uint32

// Allocate statement
op_allocate_statement:
  database_handle: uint32
  statement_handle: uint32

// Prepare
op_prepare_statement:
  statement_handle: uint32
  sql_text: string

// Execute
op_execute:
  statement_handle: uint32
  parameters: []

// Fetch
op_fetch:
  statement_handle: uint32

// Response
op_response:
  object_handle: uint32
  object_id: uint64
  data: bytes
```

---

## Parser Plugin API

### Plugin Interface

```c
// SQL Parser plugin interface
struct SQLParserPlugin {
    // Plugin metadata
    const char* dialect_name;          // "postgresql", "mysql", "tsql", "firebird"
    uint32_t version;                  // Plugin version
    
    // Lifecycle
    int (*init)(ParserConfig* config);
    void (*shutdown)();
    
    // Parsing
    BytecodeProgram* (*parse_sql)(const char* sql, ParserContext* ctx);
    void (*free_bytecode)(BytecodeProgram* bc);
    
    // Statement cache
    BytecodeProgram* (*cache_lookup)(const char* sql);
    void (*cache_insert)(const char* sql, BytecodeProgram* bc);
    void (*cache_invalidate)(const char* pattern);
    
    // System catalog virtualization
    bool (*is_system_query)(const char* sql);
    BytecodeProgram* (*translate_system_query)(const char* sql);
    
    // Error reporting
    const char* (*get_error_message)();
    uint32_t (*get_error_position)();
};

// Load parser plugin
SQLParserPlugin* load_parser_plugin(const char* dialect) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "/usr/lib/db/parsers/%s_parser.so", dialect);
    
    void* handle = dlopen(path, RTLD_LAZY);
    if (!handle) {
        log_error("Failed to load parser plugin: %s", dlerror());
        return NULL;
    }
    
    // Get plugin interface
    SQLParserPlugin* (*get_plugin)() = dlsym(handle, "get_parser_plugin");
    if (!get_plugin) {
        log_error("Invalid parser plugin: missing get_parser_plugin()");
        dlclose(handle);
        return NULL;
    }
    
    SQLParserPlugin* plugin = get_plugin();
    
    log_info("Loaded parser plugin: %s v%u", plugin->dialect_name, plugin->version);
    
    return plugin;
}
```

### Parser Context

```c
// Context passed to parser
struct ParserContext {
    uint64_t session_id;
    const char* database_name;
    const char* username;
    SchemaMetadata* schema;
    
    // Parser options
    bool case_sensitive;
    bool strict_mode;
    IsolationLevel default_isolation;
};
```

### Bytecode Format

```c
// Bytecode program (protocol-agnostic)
struct BytecodeProgram {
    // Identification
    uint32_t version;
    uint8_t hash[32];               // SHA256 of bytecode
    
    // Parameter info
    uint32_t param_count;
    DataType* param_types;
    
    // Result set metadata
    uint32_t column_count;
    ColumnMetadata* columns;
    
    // Instructions
    uint32_t instruction_count;
    Instruction* instructions;
    
    // Constant pool
    uint32_t constant_count;
    Constant* constants;
};

// Example instructions
enum Opcode {
    OP_LOAD_TABLE = 0x01,
    OP_FILTER = 0x02,
    OP_PROJECT = 0x03,
    OP_JOIN = 0x04,
    OP_AGGREGATE = 0x05,
    OP_SORT = 0x06,
    OP_LIMIT = 0x07,
    OP_RETURN = 0xFF,
};
```

---

## Authentication Integration

### Unified Auth Backend

```c
// Authentication backend (unified across protocols)
struct AuthBackend {
    // User database
    UserDatabase* users;
    
    // SSO/Domain integration
    SSOProvider* sso;
    KerberosConfig* kerberos;
    LDAPConfig* ldap;
    
    // Methods
    bool (*verify_password)(const char* username, const char* password);
    bool (*verify_kerberos)(const char* principal, KerberosTicket* ticket);
    bool (*verify_sso)(const char* token);
};

// PostgreSQL authentication
bool pg_authenticate(PGConnection* conn, StartupMessage* startup) {
    // Try auth methods in order
    if (auth_backend.kerberos) {
        // GSSAPI/Kerberos
        if (pg_gssapi_auth(conn, startup->user)) {
            return true;
        }
    }
    
    if (auth_backend.sso) {
        // Check for SSO token
        const char* token = startup->options["sso_token"];
        if (token && auth_backend.verify_sso(token)) {
            return true;
        }
    }
    
    // Fallback to password
    // Send SCRAM-SHA-256 challenge
    if (pg_scram_sha256_auth(conn, startup->user, startup->password)) {
        return true;
    }
    
    return false;
}
```

---

## Session Management

### Session State

```c
// Session state maintained per connection
struct SessionState {
    uint64_t session_id;
    uint16_t server_id;
    
    // User info
    char username[256];
    char database[256];
    
    // Transaction state
    Transaction* active_txn;
    IsolationLevel isolation_level;
    bool autocommit;
    
    // Prepared statements
    HashMap* prepared_statements;
    
    // Temporary tables
    HashMap* temp_tables;
    
    // Session variables
    HashMap* variables;
    
    // Protocol-specific state
    void* protocol_state;
};

// Session manager
struct SessionManager {
    HashMap* sessions;          // session_id → SessionState
    pthread_rwlock_t lock;
};

// Create session
SessionState* session_create(const char* username, const char* database) {
    SessionState* session = alloc_session_state();
    
    session->session_id = generate_session_id();
    session->server_id = get_server_id();
    strncpy(session->username, username, sizeof(session->username) - 1);
    strncpy(session->database, database, sizeof(session->database) - 1);
    
    session->autocommit = true;
    session->isolation_level = ISOLATION_READ_COMMITTED;
    
    // Register session
    pthread_rwlock_wrlock(&session_manager.lock);
    hashmap_insert(&session_manager.sessions, session->session_id, session);
    pthread_rwlock_unlock(&session_manager.lock);
    
    log_info("Created session %lu for user %s", session->session_id, username);
    
    return session;
}
```

---

## Protocol-Specific Considerations

### PostgreSQL

- **Extended Query Protocol**: Parse/Bind/Execute flow
- **COPY Protocol**: Bulk import/export
- **LISTEN/NOTIFY**: Async notifications (limited support initially)
- **Cursors**: Named cursors with FETCH
- **Savepoints**: Nested transactions

### MySQL

- **Binary Protocol**: Prepared statements use binary encoding
- **Multi-statements**: Multiple queries in one packet
- **LOCAL INFILE**: Client-side file upload
- **Character Sets**: utf8, utf8mb4, latin1, etc.

### MSSQL (post-gold)

- **MARS** (Multiple Active Result Sets): Complex to support
- **Bulk Copy**: High-speed bulk insert
- **Table-Valued Parameters**: Pass tables as parameters
- **Output Parameters**: Stored procedure OUT params

### Firebird

- **Blob Streaming**: Large object handling
- **Array Types**: Native array support
- **Events**: Async event notifications
- **Scrollable Cursors**: Bidirectional cursor movement

---

## Configuration

```yaml
protocols:
  postgresql:
    enabled: true
    port: 5432
    max_connections: 1000
    parser_plugin: /usr/lib/db/parsers/postgresql_parser.so
    
    # Protocol options
    ssl: true
    ssl_cert: /etc/db/certs/server.crt
    ssl_key: /etc/db/certs/server.key
    
  mysql:
    enabled: true
    port: 3306
    max_connections: 500
    parser_plugin: /usr/lib/db/parsers/mysql_parser.so
    
    # MySQL options
    character_set: utf8mb4
    
  mssql:
    enabled: false  # post-gold
    port: 1433
    max_connections: 500
    parser_plugin: /usr/lib/db/parsers/tsql_parser.so
    
    # TDS options
    encryption: required
    
  firebird:
    enabled: true
    port: 3050
    max_connections: 200
    parser_plugin: /usr/lib/db/parsers/firebird_parser.so
```

---

## Testing

### Protocol Compliance Tests

```python
# PostgreSQL compliance test
import psycopg2

def test_postgresql_basic():
    conn = psycopg2.connect(
        host='localhost',
        port=5432,
        user='test',
        password='test',
        database='test'
    )
    
    cur = conn.cursor()
    cur.execute("SELECT version()")
    result = cur.fetchone()
    
    assert result is not None
    
    cur.close()
    conn.close()

def test_postgresql_prepared():
    conn = psycopg2.connect(...)
    cur = conn.cursor()
    
    # Prepared statement
    cur.execute("PREPARE test_stmt AS SELECT * FROM users WHERE id = $1")
    cur.execute("EXECUTE test_stmt (123)")
    
    result = cur.fetchone()
    assert result is not None

# MySQL compliance test
import mysql.connector

def test_mysql_basic():
    conn = mysql.connector.connect(
        host='localhost',
        port=3306,
        user='test',
        password='test',
        database='test'
    )
    
    cursor = conn.cursor()
    cursor.execute("SELECT VERSION()")
    result = cursor.fetchone()
    
    assert result is not None
```

---

## Implementation Checklist

### PostgreSQL
- [ ] Startup message handling
- [ ] Authentication (SCRAM-SHA-256, MD5)
- [ ] Simple query protocol
- [ ] Extended query protocol (Parse/Bind/Execute)
- [ ] Result set encoding
- [ ] Error response format
- [ ] System catalog virtualization

### MySQL
- [ ] Handshake v10
- [ ] Authentication (caching_sha2_password)
- [ ] COM_QUERY handler
- [ ] COM_STMT_PREPARE/EXECUTE
- [ ] Result set encoding (text + binary)
- [ ] Error packet format
- [ ] Character set handling

### MSSQL (post-gold)
- [ ] PRELOGIN/LOGIN7
- [ ] SQL Batch handling
- [ ] RPC handling
- [ ] COLMETADATA/ROW encoding
- [ ] Error INFO tokens
- [ ] DONE/DONEINPROC/DONEPROC

### Firebird
- [ ] op_connect/op_attach
- [ ] op_allocate_statement
- [ ] op_prepare_statement
- [ ] op_execute/op_fetch
- [ ] Blob handling
- [ ] Event notifications

---

**Document Status**: Draft for Review  
**Next Review Date**: TBD  
**Approval Required**: Protocol Team
