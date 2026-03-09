# Specification: ScratchBird Wire Protocol (SBWP)

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | ipc/protocol |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird 1.0.0-alpha1 |
| **Authors** | ScratchBird Engineering |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/protocol/sbwp_protocol.cpp:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/protocol/wire_protocol.cpp:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/protocol/sbwp_protocol.h`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/protocol/wire_protocol.h`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_ipc_contract.cpp:1`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_ipc_server.cpp:1`

## Synopsis

This specification defines the ScratchBird Wire Protocol (SBWP) - a binary protocol for communication between ScratchBird clients, parser agents, and the engine. SBWP supports both native ScratchBird clients and protocol adapters for emulated database wire protocols. The protocol provides framed message exchange with support for compression, encryption, and streaming.

## Scope

### In Scope

- SBWP frame format and header structure
- Message type definitions and payload schemas
- Endianness and encoding rules
- Compression and encryption extensions
- Protocol version negotiation
- Error frame format

### Out of Scope

- Transport layer implementation details (TCP, Unix sockets, named pipes)
- Authentication mechanism details (see ipc_session_lifecycle.md)
- SQL parsing and SBLR generation (see parser_agent_contract.md)
- Emulated protocol specifics (PostgreSQL, MySQL, Firebird - see protocol_adapters.md)

## Background

SBWP serves as the canonical wire protocol for ScratchBird, designed to:

1. **Unify Communication**: Single protocol for native clients, parser agents, and internal IPC
2. **Support Emulation**: Enable protocol adapters to translate PostgreSQL, MySQL, and Firebird wire formats
3. **Enable Streaming**: Support large result sets and COPY operations via chunked transfer
4. **Ensure Security**: Built-in support for TLS and application-layer encryption

The protocol uses little-endian byte order for numeric fields and UTF-8 for text encoding.

## Specification

### Data Structures

#### SBWP Frame Header (40 bytes)

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/protocol/sbwp_protocol.h
// Fixed 40-byte header for all SBWP messages

struct MessageHeader {
    uint8_t  magic[4];           // Offset 0: 'S', 'B', 'W', 'P' (0x53425750)
    uint8_t  version_major;      // Offset 4: Protocol major version (1)
    uint8_t  version_minor;      // Offset 5: Protocol minor version (0)
    uint8_t  type;               // Offset 6: MessageType enum value
    uint8_t  flags;              // Offset 7: Message flags bitmask
    uint32_t length;             // Offset 8: Payload length in bytes (little-endian)
    uint32_t sequence;           // Offset 12: Sequence number (little-endian)
    uint8_t  attachment_id[16];  // Offset 16: Attachment/connection UUID
    uint64_t txn_id;             // Offset 32: Transaction ID (little-endian)
};

static_assert(sizeof(MessageHeader) == 40, "Header must be 40 bytes");
```

**Binary Frame Layout:**

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|     'S'       |     'B'       |     'W'       |     'P'       | 0-3: Magic
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Maj  |  Min  |     Type      |     Flags     |    Length...  | 4-7: Version/Type/Flags/Length
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| ...Length (cont)  |              Sequence Number...             | 8-11: Length cont/Sequence
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                        Sequence Number (cont)                   | 12-15: Sequence cont
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               | 16-31: Attachment ID (16 bytes)
|                      Attachment UUID (16 bytes)               |
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               | 32-39: Transaction ID (8 bytes)
|                      Transaction ID (uint64)                  |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

#### Header Flags

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/protocol/sbwp_protocol.cpp

constexpr uint8_t kFlagCompressed = 0x01;    // Payload is compressed
constexpr uint8_t kFlagEncrypted  = 0x02;    // Payload is encrypted
constexpr uint8_t kFlagStreaming  = 0x04;    // Streaming/chunked message
constexpr uint8_t kFlagFinalChunk = 0x08;    // Final chunk in stream
constexpr uint8_t kFlagError      = 0x80;    // Error response message
```

#### Message Types

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/protocol/sbwp_protocol.h
// Message type enumeration (1 byte)

enum class MessageType : uint8_t {
    // Connection Management (0x01-0x0F)
    STARTUP         = 0x01,  // Client -> Server: Connection startup
    READY           = 0x02,  // Server -> Client: Connection ready
    AUTH_REQUEST    = 0x03,  // Server -> Client: Authentication required
    AUTH_CONTINUE   = 0x04,  // Bidirectional: Auth challenge/response
    AUTH_OK         = 0x05,  // Server -> Client: Authentication successful
    TERMINATE       = 0x06,  // Bidirectional: Clean disconnect
    
    // Query Execution (0x10-0x1F)
    QUERY           = 0x10,  // Client -> Server: Execute SQL query
    PARSE           = 0x11,  // Client -> Server: Parse SQL statement
    BIND            = 0x12,  // Client -> Server: Bind parameters
    EXECUTE         = 0x13,  // Client -> Server: Execute prepared statement
    DESCRIBE        = 0x14,  // Client -> Server: Describe statement/portal
    CLOSE           = 0x15,  // Client -> Server: Close statement/portal
    SYNC            = 0x16,  // Client -> Server: Synchronize
    
    // Results (0x20-0x2F)
    ROW_DESCRIPTION = 0x20,  // Server -> Client: Result column metadata
    DATA_ROW        = 0x21,  // Server -> Client: Single data row
    COMMAND_COMPLETE= 0x22,  // Server -> Client: Query completion
    PARSE_COMPLETE  = 0x23,  // Server -> Client: Parse completion
    BIND_COMPLETE   = 0x24,  // Server -> Client: Bind completion
    CLOSE_COMPLETE  = 0x25,  // Server -> Client: Close completion
    EMPTY_RESPONSE  = 0x26,  // Server -> Client: Empty result
    
    // COPY Operations (0x30-0x3F)
    COPY_DATA       = 0x30,  // Bidirectional: COPY data chunk
    COPY_DONE       = 0x31,  // Client -> Server: COPY complete
    COPY_FAIL       = 0x32,  // Client -> Server: COPY failed
    COPY_IN_RESPONSE= 0x33,  // Server -> Client: Expect COPY data
    COPY_OUT_RESPONSE=0x34,  // Server -> Client: COPY data incoming
    COPY_BOTH_RESPONSE=0x35, // Server -> Client: Bidirectional COPY
    
    // Transactions (0x40-0x4F)
    TXN_BEGIN       = 0x40,  // Client -> Server: Begin transaction
    TXN_COMMIT      = 0x41,  // Client -> Server: Commit transaction
    TXN_ROLLBACK    = 0x42,  // Client -> Server: Rollback transaction
    TXN_SAVEPOINT   = 0x43,  // Client -> Server: Create savepoint
    TXN_RELEASE     = 0x44,  // Client -> Server: Release savepoint
    TXN_ROLLBACK_TO = 0x45,  // Client -> Server: Rollback to savepoint
    
    // Asynchronous (0x50-0x5F)
    SUBSCRIBE       = 0x50,  // Client -> Server: Subscribe to channel
    UNSUBSCRIBE     = 0x51,  // Client -> Server: Unsubscribe from channel
    NOTIFY          = 0x52,  // Server -> Client: Notification delivery
    CANCEL          = 0x53,  // Client -> Server: Cancel request
    
    // SBLR Execution (0x60-0x6F)
    SBLR_EXECUTE    = 0x60,  // Client -> Server: Execute SBLR bytecode
    SBLR_COMPILED   = 0x61,  // Server -> Client: SBLR compilation result
    QUERY_PLAN      = 0x62,  // Server -> Client: Query execution plan
    
    // Errors (0x70-0x7F)
    ERROR_RESPONSE  = 0x70,  // Server -> Client: Error occurred
    NOTICE          = 0x71,  // Server -> Client: Notice/warning
    
    // Flow Control (0x80-0x8F)
    STREAM_CONTROL  = 0x80,  // Bidirectional: Flow control
    
    // Database Attach (0x90-0x9F)
    ATTACH_CREATE   = 0x90,  // Client -> Server: Create attachment
    ATTACH_DETACH   = 0x91,  // Client -> Server: Detach from database
    ATTACH_LIST     = 0x92,  // Client -> Server: List attachments
};
```

#### Protocol Constants

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/protocol/sbwp_protocol.cpp

constexpr uint8_t  kProtocolMajor = 1;
constexpr uint8_t  kProtocolMinor = 0;
constexpr uint32_t kHeaderSize = 40;
constexpr uint32_t kMaxMessageSize = 64 * 1024 * 1024;  // 64 MB
```

### Payload Schemas

#### STARTUP Payload

```
Offset  Size  Description
------  ----  -----------
0       2     Protocol major version (1)
2       2     Protocol minor version (0)
4       8     Feature flags bitmask (uint64)
12      var   Connection parameters (null-terminated key=value pairs)
```

**Feature Profile Bits:**

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/protocol/sbwp_protocol.cpp:68

constexpr uint64_t kFeatureProfilePostgresql = 0x00000001;
constexpr uint64_t kFeatureProfileMysql      = 0x00000002;
constexpr uint64_t kFeatureProfileFirebird   = 0x00000004;
constexpr uint64_t kFeatureProfileCassandra  = 0x00000008;
constexpr uint64_t kFeatureProfileMariadb    = 0x00000010;
constexpr uint64_t kFeatureProfileClickhouse = 0x00000020;
constexpr uint64_t kFeatureProfileDuckdb     = 0x00000040;
constexpr uint64_t kFeatureProfileInfluxdb   = 0x00000080;
constexpr uint64_t kFeatureProfileMongodb    = 0x00000100;
constexpr uint64_t kFeatureProfileRedis      = 0x00000200;
constexpr uint64_t kFeatureProfileNeo4j      = 0x00000400;
constexpr uint64_t kFeatureProfileMilvus     = 0x00000800;
constexpr uint64_t kFeatureProfileOpensearch = 0x00001000;
```

#### QUERY Payload

```
Offset  Size  Description
------  ----  -----------
0       4     Query flags (uint32)
4       4     Max rows to return (uint32, 0=unlimited)
8       4     Timeout in milliseconds (uint32)
12      var   SQL query text (null-terminated)
```

#### PARSE Payload

```
Offset  Size  Description
------  ----  -----------
0       4     Statement name length (uint32)
4       len   Statement name bytes
4+len   4     Query length (uint32)
8+len   len2  Query bytes
8+len+len2 2  Parameter count (uint16)
10+len+len2 2 Reserved (0)
12+len+len2 var Parameter type OIDs (uint32 each)
```

#### BIND Payload

```
Offset  Size  Description
------  ----  -----------
0       4     Portal name length (uint32)
4       len   Portal name bytes
4+len   4     Statement name length (uint32)
8+len   len2  Statement name bytes
8+len+len2 2  Parameter format count (uint16)
10+len+len2 var Parameter formats (uint16 each, 0=text, 1=binary)
var     2     Parameter count (uint16)
var+2   2     Reserved (0)
var+4   var   Parameter values (length-prefixed)
var     2     Result format count (uint16)
var+2   var   Result formats (uint16 each)
```

#### ROW_DESCRIPTION Payload

```
Offset  Size  Description
------  ----  -----------
0       2     Field count (uint16)
2       2     Reserved (0)
4       var   Field descriptors (repeated):
              - Name length (uint32)
              - Name bytes
              - Table OID (uint32)
              - Column index (uint16)
              - Type OID (uint32)
              - Type size (int16)
              - Type modifier (int32)
              - Format (uint8)
              - Nullable (uint8)
              - Padding (2 bytes)
```

#### DATA_ROW Payload

```
Offset  Size  Description
------  ----  -----------
0       2     Field count (uint16)
2       2     Null bitmap bytes (uint16)
4       var   Null bitmap (1 bit per field)
var     var   Field values (repeated):
              - Length (int32, -1 for NULL)
              - Data bytes (if not NULL)
```

#### ERROR_RESPONSE Payload

```
Offset  Size  Description
------  ----  -----------
0       var   Error fields (null-terminated key=value pairs):
              'S' = Severity (ERROR, FATAL, PANIC)
              'C' = SQLSTATE code (5 characters)
              'M' = Primary message
              'D' = Detail
              'H' = Hint
              'P' = Position
              'F' = File
              'L' = Line
              'R' = Routine
              0x00 = End of fields
```

#### COPY_DATA Payload

```
Offset  Size  Description
------  ----  -----------
0       8     Data length (uint64)
8       len   Data bytes
```

### Interface Contracts

#### Function: `encodeMessage()`

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/protocol/sbwp_protocol.cpp:131

std::vector<uint8_t> encodeMessage(const MessageHeader& header,
                                   const std::vector<uint8_t>& payload);
```

**Preconditions:**
- Header magic must be valid ('SBWP')
- Protocol version must be supported (1.0)
- Payload length must not exceed kMaxMessageSize

**Postconditions:**
- Returns complete frame (header + payload)
- Header fields are serialized in little-endian

**Error Handling:**
- Returns empty vector on invalid header

#### Function: `decodeHeader()`

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/protocol/sbwp_protocol.cpp:150

core::Status decodeHeader(const std::vector<uint8_t>& header_bytes,
                          MessageHeader& header,
                          core::ErrorContext* ctx);
```

**Preconditions:**
- header_bytes must be exactly 40 bytes

**Postconditions:**
- header is populated with decoded values

**Error Handling:**
- Returns PROTOCOL_VIOLATION for invalid magic, version, or oversized payload

### State Machines

#### Connection State Machine

```
                         ┌─────────────┐
                    ┌────┤   CLOSED    │◄─────────────────────────┐
                    │    └──────┬──────┘                          │
                    │           │ CONNECT                          │
                    │           ▼                                  │
                    │    ┌─────────────┐     AUTH_REQUIRED         │
                    │    │  STARTING   │────────────────────────►│
                    │    └──────┬──────┘                           │
                    │           │ STARTUP_OK                       │
                    │           ▼                                  │
                    │    ┌─────────────┐     ERROR/FAIL           │
                    │    │  NEGOTIATE  │─────────────────────────┘
                    │    └──────┬──────┘
                    │           │ NEGOTIATION_OK
                    │           ▼
                    │    ┌─────────────┐
                    └────┤    READY    │◄──────────────┐
                         └──────┬──────┘               │
                                │ QUERY/EXECUTE        │
                                ▼                      │
                         ┌─────────────┐    COMPLETE   │
                         │  EXECUTING  │───────────────┘
                         └──────┬──────┘
                                │ ERROR
                                ▼
                         ┌─────────────┐
                         │    ERROR    │
                         └─────────────┘
```

**State Transitions:**

| Current State | Event | Action | Next State |
|---------------|-------|--------|------------|
| CLOSED | CONNECT | Initialize connection | STARTING |
| STARTING | STARTUP_OK | Validate protocol version | NEGOTIATE |
| STARTING | AUTH_REQUIRED | Send auth challenge | AUTH_REQUIRED |
| NEGOTIATE | NEGOTIATION_OK | Finalize feature flags | READY |
| READY | QUERY/EXECUTE | Begin execution | EXECUTING |
| EXECUTING | COMPLETE | Return results | READY |
| EXECUTING | ERROR | Log error details | ERROR |
| * | TERMINATE | Close connection | CLOSED |

### Invariants

1. **Header Size Invariant**: All SBWP frames MUST have exactly 40-byte headers
   - Verification: `assert(header_bytes.size() == kHeaderSize)`

2. **Magic Value Invariant**: Header magic MUST be 0x53425750 ('SBWP')
   - Verification: Header bytes [0-3] == {0x53, 0x42, 0x57, 0x50}

3. **Payload Size Limit**: Payload MUST NOT exceed 64MB
   - Verification: `length <= kMaxMessageSize`

4. **Sequence Monotonicity**: Sequence numbers MUST increase monotonically per connection
   - Verification: Track last sequence number, reject if `new_seq <= last_seq`

5. **Little-Endian Invariant**: All multi-byte numeric fields MUST be little-endian
   - Verification: Use platform-agnostic read/write functions

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `PROTOCOL_VIOLATION` | Invalid magic, version, or header length | Close connection |
| `MESSAGE_TOO_LARGE` | Payload exceeds 64MB limit | Reject with error frame |
| `INVALID_SEQUENCE` | Out-of-order sequence number | Close connection |
| `UNSUPPORTED_VERSION` | Protocol version mismatch | Send version negotiation frame |
| `COMPRESSION_ERROR` | Failed to decompress payload | Close connection |
| `ENCRYPTION_ERROR` | Decryption failure | Close connection |

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_ipc_contract.cpp` | Header encoding/decoding, message validation |
| `tests/unit/test_ipc_server.cpp` | Connection lifecycle, message handling |
| `tests/unit/test_ipc_policy.cpp` | Protocol policy enforcement |

## Migration Notes

- SBWP 1.0 is the initial protocol version
- Future versions will maintain backward compatibility through version negotiation
- Deprecated message types will be documented with migration paths

## Related Specifications

- [ipc_session_lifecycle.md](./ipc_session_lifecycle.md) - Connection handshake and authentication
- [parser_agent_contract.md](./parser_agent_contract.md) - Parser agent IPC protocol
- [protocol_adapters.md](./protocol_adapters.md) - Emulated protocol adapters

## Appendix

### Glossary

| Term | Definition |
|------|------------|
| SBWP | ScratchBird Wire Protocol |
| SBLR | ScratchBird Language Representation (bytecode) |
| Attachment | A logical connection to a database |
| Portal | A cursor for executing a prepared statement |
| OID | Object Identifier (type reference) |

### References

- `/home/dcalford/CliWork/ScratchBird/src/protocol/sbwp_protocol.cpp` - Core SBWP implementation
- `/home/dcalford/CliWork/ScratchBird/src/protocol/wire_protocol.cpp` - Message class implementation
- `/home/dcalford/CliWork/local_work/docs/specifications/26_Native_Wire_Protocol/` - External wire protocol reference

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Engineering |
