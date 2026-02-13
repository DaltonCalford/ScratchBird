# MySQL Client Implementation

MySQL/MariaDB wire protocol client adapter for Remote Database UDR.

See: 11-Remote-Database-UDR-Specification.md and wire_protocols/mysql_wire_protocol.md

## 1. Scope
- Protocol: MySQL client/server protocol (4.1+)
- Supported MySQL: 5.7, 8.0
- Supported MariaDB: 10.3+
- Transport: TCP with optional TLS
- No libmysql usage; ScratchBird implements protocol directly

## 2. Connection and Authentication

### 2.1 Handshake
1. Server sends Initial Handshake packet:
   - protocol version, server version, connection id
   - auth-plugin data part 1 + capability flags
2. Client sends Handshake Response 41:
   - capability flags, max packet size, charset
   - username, auth response, database (optional), auth plugin name
3. Optional TLS:
   - If CLIENT_SSL set, send SSLRequest then start TLS and resend Handshake

### 2.2 Auth Plugins
Required support:
- caching_sha2_password (MySQL 8 default)
- mysql_native_password (legacy)

For caching_sha2_password:
- Prefer TLS; otherwise use RSA key exchange for full auth

### 2.3 Session Settings
Support per-server options:
- sql_mode
- time_zone
- character_set_results
- autocommit (default true)

## 3. Command Phase

### 3.1 Simple Query
- COM_QUERY with SQL text
- Resultset: column definitions + row data (text protocol)
- Terminator: OK packet or EOF (depending on CLIENT_DEPRECATE_EOF)

### 3.2 Prepared Statements
- COM_STMT_PREPARE -> returns statement id + param/column metadata
- COM_STMT_EXECUTE -> binary protocol rows
- COM_STMT_FETCH -> cursor paging (if CLIENT_CURSOR_TYPE_READ_ONLY)
- COM_STMT_CLOSE -> free server resources

### 3.3 Pagination
- Use COM_STMT_FETCH for scrollable cursor support
- For simple queries, prefer LIMIT/OFFSET pushdown

### 3.4 Streaming and Bulk
- MySQL LOAD DATA LOCAL INFILE is disabled by default
- If enabled, the adapter must stream file data from client and honor
  allow_passthrough + file permissions

## 4. Cancellation
- Use KILL QUERY <thread_id> or COM_PROCESS_KILL on server thread id
- Thread id is provided in Initial Handshake

## 5. Transactions
- BEGIN/COMMIT/ROLLBACK supported
- Savepoints: SAVEPOINT / ROLLBACK TO SAVEPOINT

## 6. Error Mapping
- ERR packet includes SQLSTATE (5 chars) and error code
- Map to ScratchBird error catalog

## 7. Type Mapping (Minimum)
- TINYINT/SMALLINT/INT/BIGINT -> INT
- FLOAT/DOUBLE -> FLOAT/DOUBLE
- DECIMAL -> DECIMAL
- CHAR/VARCHAR/TEXT -> STRING
- BLOB/VARBINARY -> BYTES
- DATE/DATETIME/TIMESTAMP -> DATE/TIMESTAMP
- TIME -> TIME
- JSON -> JSON
- ENUM/SET -> STRING (or ENUM if strict_types)

Unsupported types map to STRING unless strict_types is enabled.

## 8. Schema Introspection
Use information_schema:
- schemata, tables, columns, statistics
- table_constraints, key_column_usage, referential_constraints
- routines, parameters, triggers

## 9. Required Capabilities
- prepared statements
- paging with COM_STMT_FETCH
- cancellation
- schema introspection

## 10. References
- https://dev.mysql.com/doc/dev/mysql-server/latest/PAGE_PROTOCOL.html
- https://dev.mysql.com/doc/dev/mysql-server/latest/page_protocol_connection_phase_packets.html
- https://dev.mysql.com/doc/dev/mysql-server/latest/page_protocol_command_phase.html

