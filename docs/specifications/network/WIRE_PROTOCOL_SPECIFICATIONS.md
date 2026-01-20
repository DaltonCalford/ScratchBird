# ScratchBird Wire Protocol Requirements

## Overview

For ScratchBird to act as a drop-in replacement for multiple database engines, per‑connection parser processes implement native wire protocols. The Y‑Valve selects the appropriate parser and transfers the client socket; thereafter the parser handles the full wire protocol lifecycle and communicates with the engine via the internal API. This document outlines protocol specifics for each parser.

## Required Wire Protocol Specifications

### 1. PostgreSQL Wire Protocol v3

#### Essential Components
- **Message Flow**
  - StartupMessage format (version, parameters)
  - Authentication flow (MD5, SCRAM-SHA-256, plain)
  - Simple Query protocol
  - Extended Query protocol (Parse, Bind, Describe, Execute, Sync)
  - COPY protocol (COPY IN/OUT/BOTH)
  
- **Message Formats**
  ```
  Message Header: [Type:1 byte][Length:4 bytes][Payload]
  
  Key Messages:
  - 'Q' Query
  - 'P' Parse
  - 'B' Bind  
  - 'E' Execute
  - 'D' Describe
  - 'C' Close
  - 'S' Sync
  - 'X' Terminate
  ```

- **Data Type Formats**
  - Text format representations
  - Binary format representations
  - Array encoding
  - Composite type encoding

- **Error and Notice Messages**
  - Field codes (S, C, M, D, H, P, etc.)
  - Severity levels
  - SQLSTATE codes

### 2. MySQL/MariaDB Wire Protocol

#### Essential Components
- **Handshake**
  - Initial handshake packet v10
  - Capability flags negotiation
  - Authentication plugins (mysql_native_password, caching_sha2_password)
  - SSL negotiation

- **Command Protocol**
  ```
  Command Packet: [Length:3][Seq:1][Command:1][Args]
  
  Commands:
  - COM_QUERY (0x03)
  - COM_PREPARE (0x16)
  - COM_EXECUTE (0x17)
  - COM_FIELD_LIST (0x04)
  - COM_QUIT (0x01)
  ```

- **Result Set Protocol**
  - Column count packet
  - Column definition packets
  - EOF packets
  - Row data packets (text/binary)

- **Prepared Statement Protocol**
  - Statement prepare response
  - Parameter binding
  - Binary result set format

### 3. Firebird Wire Protocol

#### Essential Components
- **Connection Phase**
  ```
  Operations:
  - op_connect (1)
  - op_accept (2)
  - op_attach (19)
  - op_create (20)
  ```

- **Transaction Management**
  - op_transaction (29)
  - Transaction handles
  - TPB (Transaction Parameter Buffer)

- **Statement Execution**
  - op_prepare_statement (68)
  - op_execute (63)
  - op_execute2 (104)
  - op_fetch (65)
  - DSQL info/describe

- **Data Representation**
  - XDR encoding
  - Blob protocol
  - Array protocol
  - Parameter buffers (DPB, TPB, SPB)

### 4. TDS Protocol (MSSQL)

**Scope Note:** MSSQL/TDS support is post-gold; this section is forward-looking.

#### Essential Components
- **Login Process**
  - Pre-login packet
  - Login7 packet structure
  - SSPI/NTLM authentication
  - TLS/SSL negotiation

- **Packet Structure**
  ```
  TDS Packet Header:
  - Type (1 byte)
  - Status (1 byte)
  - Length (2 bytes)
  - SPID (2 bytes)
  - Packet ID (1 byte)
  - Window (1 byte)
  ```

- **Data Stream Types**
  - SQL Batch
  - RPC (Remote Procedure Call)
  - Bulk Load
  - Attention signal
  - Transaction Manager

- **Token Streams**
  - COLMETADATA token
  - ROW token
  - DONE/DONEPROC/DONEINPROC tokens
  - ERROR/INFO tokens

## Protocol Implementation Priority

### Phase 1: Core Protocol Support
1. **PostgreSQL v3** - Most common, well-documented
   - Simple Query protocol
   - Basic authentication (MD5, plain)
   - Text format data

2. **MySQL** - Wide adoption
   - Handshake v10
   - COM_QUERY command
   - Text result sets

### Phase 2: Extended Features
1. **PostgreSQL**
   - Extended Query protocol
   - Binary formats
   - COPY protocol
   - SCRAM authentication

2. **MySQL**
   - Prepared statements
   - Binary protocol
   - Compression
   - Plugin authentication

### Phase 3: Additional Protocols
1. **Firebird** - Native protocol
   - Full operation set
   - XDR encoding
   - Blob/Array support

2. **TDS/MSSQL** - Enterprise support (post-gold)
   - Login7 protocol
   - RPC calls
   - Bulk operations

## Testing Requirements

### Protocol Conformance Tests
- **Connection Tests**
  - Various authentication methods
  - SSL/TLS negotiation
  - Connection parameters

- **Query Execution Tests**
  - Simple queries
  - Prepared statements
  - Transactions
  - Cursors

- **Data Type Tests**
  - All native types
  - NULL handling
  - Arrays/composites
  - Large objects

- **Error Handling Tests**
  - Syntax errors
  - Constraint violations
  - Connection failures
  - Transaction conflicts

### Compatibility Testing Tools

#### PostgreSQL
```bash
# Using psql client
psql -h localhost -p 5432 -U user -d testdb

# Using pgbench
pgbench -h localhost -p 5432 -U user testdb

# Using language drivers
python: psycopg2, asyncpg
java: JDBC PostgreSQL driver
```

#### MySQL
```bash
# Using mysql client
mysql -h localhost -P 3306 -u user -p testdb

# Using mysqlslap
mysqlslap --host=localhost --user=user

# Using language drivers
python: mysql-connector, PyMySQL
java: JDBC MySQL driver
```

#### Firebird
```bash
# Using isql
isql localhost:testdb -u user -p pass

# Using flamerobin (GUI)
# Using language drivers
python: fdb, firebird-driver
java: Jaybird JDBC driver
```

#### MSSQL (post-gold)
```bash
# Using sqlcmd
sqlcmd -S localhost -U user -P pass

# Using FreeTDS tsql
tsql -S localhost -U user -P pass

# Using language drivers
python: pyodbc, pymssql
java: JDBC SQL Server driver
```

## Implementation Notes

### Listener/Parser Pool Router Design (Legacy Y-Valve Term)
```
Client Connection → Listener/Parser Pool → Protocol Handler → SBLR Generator → Engine

Protocol Detector (listener control plane):
- Examines first packet
- PostgreSQL: StartupMessage with version 196608
- MySQL: Handshake packet
- Firebird: op_connect
- TDS (post-gold): Pre-login packet

Protocol Handler (parser process):
- Maintains protocol state machine
- Translates to/from internal format
- Handles authentication
- Manages transactions
```

### Buffer Management
- Each protocol has different packet size limits
- PostgreSQL: No inherent limit
- MySQL: 16MB default (max_allowed_packet)
- Firebird: 32KB segments
- TDS (post-gold): 4KB default (negotiable to 32KB)

### Character Encoding
- PostgreSQL: Client encoding parameter
- MySQL: Character set negotiation
- Firebird: Connection character set
- TDS (post-gold): Collation in login packet

### Transaction Handling
- Map protocol-specific transaction commands to MGA
- Handle autocommit modes
- Support savepoints where applicable
- Manage distributed transactions (2PC)

## Resources Needed

### Official Documentation
- [ ] PostgreSQL Frontend/Backend Protocol
- [ ] MySQL Client/Server Protocol  
- [ ] Firebird Remote Protocol
- [ ] MS-TDS Specification (post-gold)

### Reference Implementations
- [ ] PostgreSQL: src/backend/libpq
- [ ] MySQL: sql/protocol.cc
- [ ] Firebird: src/remote/protocol.cpp
- [ ] FreeTDS: src/tds/ (post-gold)

### Testing Tools
- [ ] Wireshark with protocol dissectors
- [ ] Protocol-specific test suites
- [ ] Fuzzing tools for protocol testing
- [ ] Performance benchmarking tools

## Success Criteria

1. **Transparent Compatibility**
   - Existing clients connect without modification
   - All standard drivers work unchanged
   - Applications run without code changes

2. **Protocol Compliance**
   - Pass official protocol test suites
   - Handle edge cases correctly
   - Support protocol extensions

3. **Performance**
   - Minimal overhead in protocol translation
   - Efficient buffer management
   - Low latency message handling

4. **Diagnostics**
   - Protocol-level logging
   - Performance metrics per protocol
   - Connection tracking and debugging
