# Specification: ScratchBird Wire Protocol (SBWP)

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | protocol/wire_protocol |
| **Spec Version** | 1.1.0 |
| **Status** | 🟢 Approved |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird 1.0.0-alpha1 |
| **Authors** | ScratchBird Engineering |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/protocol/sbwp_protocol.cpp:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/protocol/wire_protocol.cpp:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/protocol/sbwp_protocol.h:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/protocol/wire_protocol.h:1`
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
- Connection lifecycle messages
- Query execution messages
- Transaction messages
- COPY operation messages
- Streaming and flow control

### Out of Scope

- Transport layer implementation details (TCP, Unix sockets, named pipes)
- Authentication mechanism details (see ipc_session_lifecycle.md)
- SQL parsing and SBLR generation (see parser_agent_contract.md)
- Emulated protocol specifics (PostgreSQL, MySQL, Firebird - see protocol_adapters.md)
- FDW protocol details (see fdw_protocol.md)
- UDR protocol details (see udr_protocol.md)

## Background

SBWP serves as the canonical wire protocol for ScratchBird, designed to:

1. **Unify Communication**: Single protocol for native clients, parser agents, and internal IPC
2. **Support Emulation**: Enable protocol adapters to translate PostgreSQL, MySQL, and Firebird wire formats
3. **Enable Streaming**: Support large result sets and COPY operations via chunked transfer
4. **Ensure Security**: Built-in support for TLS and application-layer encryption
5. **Enable Federation**: Support federated queries across multiple data sources

The protocol uses little-endian byte order for numeric fields and UTF-8 for text encoding.

## Specification

### Protocol Constants

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/protocol/sbwp_protocol.h:14-19

constexpr uint32_t kProtocolMagic = 0x53425750;           // "SBWP"
constexpr uint8_t kProtocolMajor = 1;
constexpr uint8_t kProtocolMinor = 1;
constexpr uint16_t kProtocolVersion = (static_cast<uint16_t>(kProtocolMajor) << 8) | kProtocolMinor;
constexpr size_t kHeaderSize = 40;
constexpr size_t kMaxMessageSize = 1024u * 1024u * 1024u;  // 1GB max
```

### SBWP Frame Header (40 bytes)

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/protocol/sbwp_protocol.h:225-232

struct MessageHeader {
    MessageType type{MessageType::Query};     // Offset 0: Message type (1 byte)
    uint8_t flags{0};                         // Offset 1: Message flags
    uint32_t length{0};                       // Offset 2: Payload length (4 bytes)
    uint32_t sequence{0};                     // Offset 6: Sequence number (4 bytes)
    std::array<uint8_t, 16> attachment_id{};  // Offset 10: Attachment UUID (16 bytes)
    uint64_t txn_id{0};                       // Offset 26: Transaction ID (8 bytes)
};
// Total: 35 bytes + 5 bytes padding = 40 bytes
```

**Binary Frame Layout:**

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|     Type      |     Flags     |            Length             | 0-5
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         Sequence Number                       | 6-9
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               | 10-25
|                      Attachment ID (16 bytes)                 |
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               | 26-33
|                      Transaction ID (8 bytes)                 |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|     Padding (5 bytes)                                         | 34-39
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

### Message Types

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/protocol/sbwp_protocol.h:21-89

enum class MessageType : uint8_t {
    // Client Request Messages (0x01-0x3F)
    Startup = 0x01,           // Connection startup
    AuthResponse = 0x02,      // Authentication response
    Query = 0x03,             // Execute SQL query
    Parse = 0x04,             // Parse SQL statement
    Bind = 0x05,              // Bind parameters to statement
    Describe = 0x06,          // Describe statement/portal
    Execute = 0x07,           // Execute prepared statement
    Close = 0x08,             // Close statement/portal
    Sync = 0x09,              // End request batch
    Flush = 0x0A,             // Flush output buffer
    Cancel = 0x0B,            // Cancel query
    Terminate = 0x0C,         // Close connection
    CopyData = 0x0D,          // COPY data chunk
    CopyDone = 0x0E,          // COPY complete
    CopyFail = 0x0F,          // COPY failed
    SblrExecute = 0x10,       // Execute SBLR bytecode
    Subscribe = 0x11,         // Subscribe to notifications
    Unsubscribe = 0x12,       // Unsubscribe from notifications
    FederatedQuery = 0x13,    // Execute federated query
    StreamControl = 0x14,     // Flow control message
    TxnBegin = 0x15,          // Begin transaction
    TxnCommit = 0x16,         // Commit transaction
    TxnRollback = 0x17,       // Rollback transaction
    TxnSavepoint = 0x18,      // Create savepoint
    TxnRelease = 0x19,        // Release savepoint
    TxnRollbackTo = 0x1A,     // Rollback to savepoint
    Ping = 0x1B,              // Keepalive ping
    SetOption = 0x1C,         // Set connection option
    ClusterAuth = 0x1D,       // Cluster authentication
    AttachCreate = 0x1E,      // Create attachment
    AttachDetach = 0x1F,      // Detach from database
    AttachList = 0x20,        // List attachments

    // Server Response Messages (0x40-0x7F)
    AuthRequest = 0x40,       // Authentication required
    AuthOk = 0x41,            // Authentication successful
    AuthContinue = 0x42,      // Authentication continue
    Ready = 0x43,             // Connection ready
    RowDescription = 0x44,    // Result column metadata
    DataRow = 0x45,           // Single data row
    CommandComplete = 0x46,   // Query completion
    EmptyQuery = 0x47,        // Empty result
    Error = 0x48,             // Error response
    Notice = 0x49,            // Notice/warning
    ParseComplete = 0x4A,     // Parse completion
    BindComplete = 0x4B,      // Bind completion
    CloseComplete = 0x4C,     // Close completion
    PortalSuspended = 0x4D,   // Portal suspended (row limit)
    NoData = 0x4E,            // No data available
    ParameterStatus = 0x4F,   // Parameter status update
    ParameterDescription = 0x50,  // Parameter types
    CopyInResponse = 0x51,    // Expect COPY data
    CopyOutResponse = 0x52,   // COPY data incoming
    CopyBothResponse = 0x53,  // Bidirectional COPY
    Notification = 0x54,      // Async notification
    FunctionResult = 0x55,    // Function call result
    NegotiateVersion = 0x56,  // Version negotiation
    SblrCompiled = 0x57,      // SBLR compilation result
    QueryPlan = 0x58,         // Query execution plan
    StreamReady = 0x59,       // Stream ready
    StreamData = 0x5A,        // Stream data chunk
    StreamEnd = 0x5B,         // Stream end
    TxnStatus = 0x5C,         // Transaction status
    Pong = 0x5D,              // Keepalive response
    ClusterAuthOk = 0x5E,     // Cluster auth success
    FederatedResult = 0x5F,   // Federated query result

    // Internal Messages (0x80-0xFF)
    Heartbeat = 0x80,         // Health check
    Extension = 0x81,         // Extension message
};
```

### Header Flags

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/protocol/sbwp_protocol.h:91-98

enum MessageFlags : uint8_t {
    kFlagCompressed = 0x01,   // Payload is compressed
    kFlagContinued = 0x02,    // Multi-part message (not final)
    kFlagFinal = 0x04,        // Final part of multi-part message
    kFlagUrgent = 0x08,       // Urgent priority message
    kFlagEncrypted = 0x10,    // Payload is encrypted
    kFlagChecksum = 0x20,     // Checksum included
};
```

### Feature Flags

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/protocol/sbwp_protocol.h:100-142

constexpr uint64_t kFeatureCompression = 1ULL << 0;
constexpr uint64_t kFeatureStreaming = 1ULL << 1;
constexpr uint64_t kFeatureSblr = 1ULL << 2;
constexpr uint64_t kFeatureFederation = 1ULL << 3;
constexpr uint64_t kFeatureNotifications = 1ULL << 4;
constexpr uint64_t kFeatureQueryPlan = 1ULL << 5;
constexpr uint64_t kFeatureBatch = 1ULL << 6;
constexpr uint64_t kFeaturePipeline = 1ULL << 7;
constexpr uint64_t kFeatureBinaryCopy = 1ULL << 8;
constexpr uint64_t kFeatureSavepoints = 1ULL << 9;
constexpr uint64_t kFeatureTwoPhase = 1ULL << 10;
constexpr uint64_t kFeatureChecksums = 1ULL << 11;

// Engine-profile capability bits
constexpr uint64_t kFeatureProfilePostgresql = 1ULL << 16;
constexpr uint64_t kFeatureProfileMysql = 1ULL << 17;
constexpr uint64_t kFeatureProfileFirebird = 1ULL << 18;
constexpr uint64_t kFeatureProfileCassandra = 1ULL << 19;
constexpr uint64_t kFeatureProfileMariadb = 1ULL << 20;
constexpr uint64_t kFeatureProfileClickhouse = 1ULL << 21;
constexpr uint64_t kFeatureProfileDuckdb = 1ULL << 22;
constexpr uint64_t kFeatureProfileInfluxdb = 1ULL << 23;
constexpr uint64_t kFeatureProfileMongodb = 1ULL << 24;
constexpr uint64_t kFeatureProfileRedis = 1ULL << 25;
constexpr uint64_t kFeatureProfileNeo4j = 1ULL << 26;
constexpr uint64_t kFeatureProfileMilvus = 1ULL << 27;
constexpr uint64_t kFeatureProfileOpensearch = 1ULL << 28;
```

### Query Flags

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/protocol/sbwp_protocol.h:143-148

constexpr uint32_t kQueryFlagDescribeOnly = 0x01;    // Describe only, don't execute
constexpr uint32_t kQueryFlagNoPortal = 0x02;        // Don't create portal
constexpr uint32_t kQueryFlagBinaryResult = 0x04;    // Return binary results
constexpr uint32_t kQueryFlagIncludePlan = 0x08;     // Include query plan
constexpr uint32_t kQueryFlagReturnSblr = 0x10;      // Return compiled SBLR
constexpr uint32_t kQueryFlagNoCache = 0x20;         // Bypass query cache
```

### Interface Contracts

#### Function: `encodeMessage()`

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/protocol/sbwp_protocol.cpp:131

std::vector<uint8_t> encodeMessage(const MessageHeader& header,
                                   const std::vector<uint8_t>& payload);
```

**Preconditions:**
- Header type must be a valid MessageType value
- Payload size must not exceed kMaxMessageSize
- Header flags must be valid combination

**Postconditions:**
- Returns complete frame (header + payload) in wire format
- All numeric fields are serialized in little-endian
- Header is exactly kHeaderSize (40) bytes

**Error Handling:**
- Returns empty vector if payload exceeds kMaxMessageSize
- Undefined behavior for invalid message types

#### Function: `decodeHeader()`

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/protocol/sbwp_protocol.cpp:150

core::Status decodeHeader(const std::vector<uint8_t>& header_bytes,
                          MessageHeader& header,
                          core::ErrorContext* ctx);
```

**Preconditions:**
- header_bytes must contain exactly kHeaderSize (40) bytes

**Postconditions:**
- header is populated with decoded values
- All numeric fields are deserialized from little-endian

**Error Handling:**
- Returns PROTOCOL_VIOLATION for:
  - Invalid header length
  - Unsupported protocol version
  - Payload length exceeds kMaxMessageSize

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

2. **Magic Value Invariant**: Protocol magic MUST be 0x53425750 ('SBWP')
   - Verification: Header magic == kProtocolMagic

3. **Payload Size Limit**: Payload MUST NOT exceed 1GB
   - Verification: `length <= kMaxMessageSize`

4. **Sequence Monotonicity**: Sequence numbers MUST increase monotonically per connection
   - Verification: Track last sequence number, reject if `new_seq <= last_seq`

5. **Little-Endian Invariant**: All multi-byte numeric fields MUST be little-endian
   - Verification: Use platform-agnostic read/write functions

6. **Type Validity Invariant**: Message type MUST be a valid MessageType enum value
   - Verification: `type >= 0x01 && type <= 0x81`

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `PROTOCOL_VIOLATION` | Invalid magic, version, or header length | Close connection |
| `MESSAGE_TOO_LARGE` | Payload exceeds 1GB limit | Reject with error frame |
| `INVALID_SEQUENCE` | Out-of-order sequence number | Close connection |
| `UNSUPPORTED_VERSION` | Protocol version mismatch | Send NegotiateVersion frame |
| `COMPRESSION_ERROR` | Failed to decompress payload | Close connection |
| `ENCRYPTION_ERROR` | Decryption failure | Close connection |
| `CHECKSUM_ERROR` | Checksum validation failed | Request retransmission |

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_ipc_contract.cpp` | Header encoding/decoding, message validation |
| `tests/unit/test_ipc_server.cpp` | Connection lifecycle, message handling |
| `tests/unit/test_ipc_policy.cpp` | Protocol policy enforcement |

## Migration Notes

- SBWP 1.1 is the current stable protocol version
- Maintains backward compatibility with SBWP 1.0 through version negotiation
- Future versions will add new message types but preserve existing type codes
- Deprecated message types are marked but still supported for compatibility

## Related Specifications

- [sbwp_frames.md](./sbwp_frames.md) - Detailed frame formats
- [sbwp_messages.md](./sbwp_messages.md) - All message type schemas
- [sbwp_error_handling.md](./sbwp_error_handling.md) - Error frames and status codes
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
| Feature Flag | Bitmask indicating protocol capabilities |
| Continued Message | Part of a multi-part message sequence |

### References

- `/home/dcalford/CliWork/ScratchBird/src/protocol/sbwp_protocol.cpp` - Core SBWP implementation
- `/home/dcalford/CliWork/ScratchBird/src/protocol/wire_protocol.cpp` - Message class implementation
- PostgreSQL Protocol Documentation: https://www.postgresql.org/docs/current/protocol.html

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.1.0 | 2026-03-08 | Updated for SBWP v1.1, added federation support | ScratchBird Engineering |
| 1.0.0 | 2026-03-01 | Initial specification | ScratchBird Engineering |
