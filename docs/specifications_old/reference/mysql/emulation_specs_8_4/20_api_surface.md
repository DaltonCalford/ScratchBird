# MySQL 8.4 API Surface — Draft 1

## 1. Command Set (COM_*)
- The authoritative list of server command codes is in `source_copies/include/my_command.h` (`enum enum_server_command`).
- Each command’s protocol payload and expected response is defined by MySQL classic protocol implementation in `source_copies/sql/protocol_classic.*` and dispatch in `source_copies/sql/sql_parse.cc`.

## 2. Capability Flags
- Client/server capability flags (CLIENT_*) are defined in `source_copies/include/mysql_com.h`.
- Server must honor capability negotiation during handshake.

## 3. Session State and Status
- Server status flags are defined in `source_copies/include/mysql_com.h` and used in OK/EOF packets.
- Session state tracking (CLIENT_SESSION_TRACK) is implemented in `source_copies/sql/`.

## 4. Authentication Plugins
- Auth plugin names and behavior are defined in `source_copies/plugin/` and `source_copies/sql/auth/*`.
- Plugin negotiation occurs during handshake (classic protocol).

## 5. Prepared Statements
- Prepared statement protocol is defined in `source_copies/sql/protocol_classic.*` and `source_copies/sql/sql_prepare.cc`.

## 6. SSL/TLS and Compression
- SSL capability and negotiation are defined in `source_copies/vio/` and `source_copies/sql-common/`.
- Compression (CLIENT_COMPRESS / zlib / zstd) is defined in `source_copies/vio/` and protocol code.

## 7. Compliance Rule
All behavior must match authoritative source copies.
