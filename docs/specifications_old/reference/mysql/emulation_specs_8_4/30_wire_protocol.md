# MySQL 8.4 Wire Protocol — Draft 1

## Authoritative Sources
- `source_copies/sql/protocol_classic.cc`
- `source_copies/sql/protocol_classic.h`
- `source_copies/include/mysql_com.h`
- `source_copies/include/my_command.h`
- `source_copies/vio/`

## 1. Packet Framing
- MySQL classic protocol uses 3-byte payload length + 1-byte sequence ID header.
- Payload length max per packet is 16MB (0xFFFFFF); larger payloads are split across packets.

## 2. Handshake
- Initial handshake, capability negotiation, auth plugin negotiation, and optional SSL/TLS upgrade are defined in `protocol_classic.*` and `mysql_com.h`.

## 3. Commands
- Command packet payloads correspond to `enum_server_command` (COM_*) values; each command’s data and response are defined by the protocol implementation and `sql_parse.cc` dispatch.

## 4. Prepared Statement Protocol
- `COM_STMT_PREPARE`, `COM_STMT_EXECUTE`, `COM_STMT_SEND_LONG_DATA`, `COM_STMT_CLOSE`, `COM_STMT_RESET`, `COM_STMT_FETCH` are defined in `protocol_classic.*` and `sql/sql_prepare.cc`.

## 5. Compression
- Compression capability is negotiated via `CLIENT_COMPRESS` and/or `CLIENT_ZSTD_COMPRESSION_ALGORITHM`.
- Compressed packet format is defined in `vio/`.

## 6. Compliance Rule
All packet fields, ordering, and edge cases must match the source copies.
