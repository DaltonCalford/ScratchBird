# MySQL Emulation Protocol Behavior Specification

Status: Draft (Target). This document defines the server-side wire protocol
behavior for the MySQL emulation listener. It is intended to be compatible
with MySQL 5.7/8.0 and MariaDB 10.3+ for the covered features.

References:
- mysql_wire_protocol.md
- parser/MYSQL_PARSER_SPECIFICATION.md
- https://dev.mysql.com/doc/dev/mysql-server/latest/PAGE_PROTOCOL.html

## 1. Startup/Handshake and Authentication

### 1.1 Initial Handshake
- Server sends HandshakeV10 packet with:
  - protocol_version = 10
  - server_version string matching configured emulation target
  - connection_id (thread id)
  - auth_plugin_data (20 bytes total for native auth)
  - capability flags (lower/upper 2 bytes)
  - character_set (default utf8mb4)
  - status_flags (SERVER_STATUS_AUTOCOMMIT by default)
  - auth_plugin_name (e.g., caching_sha2_password)

### 1.2 SSL/TLS
- If client sets CLIENT_SSL, accept SSLRequest and negotiate TLS if enabled.
- If TLS is required and not negotiated, return ERR and close.

### 1.3 Authentication
- Supported auth plugins:
  - caching_sha2_password (default for MySQL 8.0)
  - mysql_native_password (legacy)
- For caching_sha2_password:
  - If TLS active, use fast auth path.
  - If not TLS, use RSA key exchange or return ERR if RSA is unavailable.

## 2. Capability Flags and Defaults

### 2.1 Server Capability Flags (Minimum)
- CLIENT_PROTOCOL_41
- CLIENT_SECURE_CONNECTION
- CLIENT_PLUGIN_AUTH
- CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA
- CLIENT_DEPRECATE_EOF (8.0 default)
- CLIENT_MULTI_RESULTS

### 2.2 Defaults
- character_set: utf8mb4
- sql_mode: configured (default empty or STRICT_TRANS_TABLES)
- autocommit: ON

## 3. Error/Notice Mapping

### 3.1 ERR Packet
- Include SQLSTATE (5 bytes) with "#" prefix in error packet.
- Map engine errors to MySQL SQLSTATE codes and native error numbers.

### 3.2 Warnings
- Use OK packet with warning count.
- Warnings should be accessible via SHOW WARNINGS.

## 4. Metadata and Result Shapes

### 4.1 Column Definition
- Use text protocol by default.
- Provide catalog/schema/table/orig_table, name/orig_name, charset,
  column length, type, flags, decimals.

### 4.2 Rows
- Text protocol row data for COM_QUERY.
- Binary protocol rows for COM_STMT_EXECUTE.

### 4.3 EOF vs OK
- If CLIENT_DEPRECATE_EOF is set, terminate result sets with OK packet.
- Otherwise, use EOF packets.

## 5. Version-Specific Deltas

### 5.1 MySQL 5.7
- Default auth plugin: mysql_native_password.
- CLIENT_DEPRECATE_EOF is not default.

### 5.2 MySQL 8.0
- Default auth plugin: caching_sha2_password.
- CLIENT_DEPRECATE_EOF is default.

### 5.3 MariaDB 10.3+
- Capability flags differ; plugin list may include ed25519.
- Report server_version string in MariaDB format.

## 6. Test Vectors

### 6.1 Handshake + Auth
- Client receives HandshakeV10.
- Client sends HandshakeResponse41.
- Server sends OK on success.

### 6.2 Simple Query
- COM_QUERY "SELECT 1" returns column count, column defs, row data,
  and OK/EOF terminator based on capabilities.

### 6.3 Prepared Statement
- COM_STMT_PREPARE returns statement id and param metadata.
- COM_STMT_EXECUTE returns binary rows.

