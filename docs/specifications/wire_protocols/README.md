# Database Wire Protocol Specifications

## Overview

This directory contains detailed byte-level wire protocol specifications for major database systems. These documents provide the low-level protocol details necessary to implement database drivers from scratch.

**Scope Note:** TDS/SQL Server is Beta scope for UDR connectors only; there is no
listener/compatibility surface in Alpha.

## Available Protocol Specifications

### 1. [MySQL Wire Protocol](mysql_wire_protocol.md)
- **Protocol Version**: MySQL 4.1+ (Protocol v10)
- **Default Port**: 3306
- **Key Features**:
  - Length-encoded integers
  - Native password and caching_sha2_password authentication
  - Text and binary result protocols
  - Prepared statements
  - Compression support
  - Replication protocol

### 2. [PostgreSQL Wire Protocol](postgresql_wire_protocol.md)
- **Protocol Version**: 3.0 (PostgreSQL 7.4+)
- **Default Port**: 5432
- **Key Features**:
  - Message-based protocol
  - MD5 and SCRAM-SHA-256 authentication
  - Simple and extended query protocols
  - COPY protocol for bulk operations
  - Asynchronous notifications
  - Streaming replication

### 3. [Firebird Wire Protocol](firebird_wire_protocol.md)
- **Protocol Version**: 10-13 (Firebird 1.0-4.0)
- **Default Port**: 3050
- **Key Features**:
  - XDR encoding for platform independence
  - SRP authentication
  - Operation-based protocol
  - BLOB streaming
  - Event notifications
  - Wire compression

### 4. [TDS Wire Protocol](tds_wire_protocol.md) (SQL Server, Beta UDR)
- **Protocol Version**: TDS 7.0-7.4 (SQL Server 2000-2019)
- **Default Port**: 1433
- **Key Features**:
  - Token stream protocol
  - Pre-login negotiation
  - SSPI/Windows authentication
  - Bulk copy protocol
  - MARS (Multiple Active Result Sets)
  - Always Encrypted support

## Protocol Comparison

| Feature | MySQL | PostgreSQL | Firebird | TDS/SQL Server (Beta UDR) |
|---------|-------|------------|----------|----------------|
| **Packet Size** | Max 16MB | No fixed limit | No fixed limit | Max 64KB |
| **Byte Order** | Little-endian | Big-endian | Big-endian (XDR) | Mixed |
| **Authentication** | Native, SHA2 | MD5, SCRAM | SRP, Legacy | SSPI, SQL |
| **Prepared Statements** | Binary protocol | Extended query | Statement handles | sp_prepare |
| **Bulk Operations** | LOAD DATA | COPY | Batch segments | BCP |
| **Compression** | zlib | No | zlib | No |
| **Encryption** | SSL/TLS | SSL/TLS | Wire encryption | SSL/TLS |
| **Async Operations** | Limited | Yes | Events | Limited |
| **Streaming** | No | COPY | BLOBs | No |

## Common Protocol Patterns

### 1. Handshake Pattern
```
Client → Server: Initial connection
Server → Client: Server capabilities/challenge
Client → Server: Authentication response
Server → Client: Authentication result
```

### 2. Query Execution Pattern
```
Client → Server: Query/Command
Server → Client: Metadata (if result set)
Server → Client: Row data (repeated)
Server → Client: Completion status
```

### 3. Prepared Statement Pattern
```
Client → Server: Prepare statement
Server → Client: Statement handle/ID
Client → Server: Bind parameters
Client → Server: Execute
Server → Client: Results
Client → Server: Close statement
```

## Implementation Considerations

### Buffer Management
- **MySQL**: Fixed packet header (4 bytes), max packet 16MB
- **PostgreSQL**: 5-byte message header, no size limit
- **Firebird**: XDR alignment (4-byte boundaries)
- **TDS**: 8-byte packet header, max packet 64KB

### Error Handling
- **MySQL**: Error packet (0xFF marker)
- **PostgreSQL**: ErrorResponse message with field codes
- **Firebird**: Status vector with error codes
- **TDS**: ERROR token in response stream

### State Machines
All protocols follow similar state transitions:
1. Disconnected
2. Connecting/Handshake
3. Authenticating
4. Ready/Idle
5. Executing
6. Fetching results

### Security Features
- **Authentication**: All support multiple authentication methods
- **Encryption**: All support SSL/TLS
- **Password Security**: Various levels of password protection
- **SQL Injection**: Use prepared statements/parameterized queries

## Testing Wire Protocols

### Using Wireshark
All protocols have Wireshark dissectors:
- MySQL: `mysql` filter
- PostgreSQL: `pgsql` filter
- Firebird: `firebird` filter
- TDS: `tds` filter

### Example Capture Filter
```
tcp port 3306 or tcp port 5432 or tcp port 3050 or tcp port 1433
```

### Using netcat for Testing
```bash
# MySQL
echo -ne '\x00\x00\x00\x00' | nc localhost 3306 | hexdump -C

# PostgreSQL
echo -ne '\x00\x00\x00\x08\x04\xd2\x16\x2f' | nc localhost 5432

# Firebird
echo -ne '\x00\x00\x00\x01' | nc localhost 3050 | hexdump -C

# TDS
echo -ne '\x12\x01\x00\x00\x00\x00\x00\x00' | nc localhost 1433 | hexdump -C
```

## Building a Protocol Parser

### Basic Structure
```c
struct protocol_parser {
    enum state current_state;
    uint8_t* buffer;
    size_t buffer_size;
    size_t buffer_pos;
    
    // Protocol-specific fields
    union {
        struct mysql_state mysql;
        struct pgsql_state pgsql;
        struct firebird_state firebird;
        struct tds_state tds;
    } protocol;
};
```

### Parsing Steps
1. Read packet/message header
2. Validate length and type
3. Read complete packet/message body
4. Parse based on current state and message type
5. Update state machine
6. Generate response (if client) or process data (if server)

## Performance Optimization

### Network Optimization
- Use TCP_NODELAY for interactive queries
- Enable TCP keepalive for long connections
- Implement connection pooling
- Use compression when available

### Protocol Optimization
- Batch multiple operations when possible
- Use prepared statements for repeated queries
- Utilize bulk operations for large data transfers
- Implement client-side caching of metadata

### Buffer Optimization
- Pre-allocate buffers based on expected size
- Reuse buffers across operations
- Implement zero-copy techniques where possible
- Use memory pools for frequent allocations

## Security Best Practices

1. **Always use encryption** (SSL/TLS) in production
2. **Implement timeout mechanisms** for all operations
3. **Validate all packet/message lengths** before processing
4. **Use strongest available authentication** methods
5. **Implement rate limiting** for connection attempts
6. **Log protocol violations** for security monitoring
7. **Keep protocol libraries updated**
8. **Use prepared statements** to prevent injection

## References

### Official Documentation
- [MySQL Internals Manual](https://dev.mysql.com/doc/internals/en/)
- [PostgreSQL Protocol Documentation](https://www.postgresql.org/docs/current/protocol.html)
- [Firebird Wire Protocol Guide](https://firebirdsql.org/file/documentation/html/en/refdocs/fblangref30/firebird-30-language-reference.html)
- [MS-TDS Specification](https://docs.microsoft.com/en-us/openspecs/windows_protocols/ms-tds/)

### Tools and Libraries
- [Wireshark](https://www.wireshark.org/) - Network protocol analyzer
- [FreeTDS](https://www.freetds.org/) - Open source TDS implementation
- [libpq](https://www.postgresql.org/docs/current/libpq.html) - PostgreSQL C library
- [MySQL Connector/C](https://dev.mysql.com/doc/c-api/8.0/en/) - MySQL C API

### Related Specifications
- [RFC 1832](https://tools.ietf.org/html/rfc1832) - XDR: External Data Representation Standard
- [RFC 5802](https://tools.ietf.org/html/rfc5802) - SCRAM-SHA authentication
- [RFC 2743](https://tools.ietf.org/html/rfc2743) - GSSAPI
- [RFC 7519](https://tools.ietf.org/html/rfc7519) - JSON Web Token (JWT)
- PostgreSQL Emulation Behavior: `docs/specifications/wire_protocols/POSTGRESQL_EMULATION_BEHAVIOR.md`
- MySQL Emulation Behavior: `docs/specifications/wire_protocols/MYSQL_EMULATION_BEHAVIOR.md`
- Firebird Emulation Behavior: `docs/specifications/wire_protocols/FIREBIRD_EMULATION_BEHAVIOR.md`
