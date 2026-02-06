/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#pragma once

/**
 * ScratchBird Wire Protocol
 *
 * Local Server Architecture - Phase 2
 *
 * This header defines the binary wire protocol for client-server communication.
 * The protocol is designed to be:
 * - Simple and efficient for local IPC
 * - Extensible for future network protocols (Alpha 3)
 * - Compatible with streaming large result sets
 *
 * Protocol Version: 1.0 (Alpha 1 Local Protocol)
 */

#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include <memory>

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/server/ipc_server.h"

namespace scratchbird {
namespace protocol {

// ============================================================================
// Protocol Constants
// ============================================================================

/// Protocol magic bytes: "SBDB" (ScratchBird DataBase)
constexpr uint32_t PROTOCOL_MAGIC = 0x42444253;  // "SBDB" in little-endian

/// Current protocol version (1.1)
constexpr uint16_t PROTOCOL_VERSION_MAJOR = 1;
constexpr uint16_t PROTOCOL_VERSION_MINOR = 1;
constexpr uint16_t PROTOCOL_VERSION = (PROTOCOL_VERSION_MAJOR << 8) | PROTOCOL_VERSION_MINOR;

/// Maximum message payload size (16 MB)
constexpr uint32_t MAX_MESSAGE_SIZE = 16 * 1024 * 1024;

/// Maximum query length (1 MB)
constexpr uint32_t MAX_QUERY_LENGTH = 1 * 1024 * 1024;

/// Maximum column name length
constexpr uint32_t MAX_COLUMN_NAME_LENGTH = 256;

/// Maximum error message length
constexpr uint32_t MAX_ERROR_MESSAGE_LENGTH = 4096;

/// Session ID size (UUID)
constexpr size_t SESSION_ID_SIZE = 16;

// ============================================================================
// Authentication Methods/Status
// ============================================================================

enum class AuthMethod : uint8_t {
    PASSWORD       = 0,
    MD5            = 1,
    SCRAM_SHA_256  = 2,
    SCRAM_SHA_512  = 3
};

enum class AuthStatus : uint8_t {
    OK        = 0,
    ERROR     = 1,
    CONTINUE  = 2
};

// ============================================================================
// Message Types
// ============================================================================

/**
 * Message type identifiers
 *
 * Ranges:
 * - 0x01-0x0F: Connection lifecycle
 * - 0x10-0x1F: Authentication
 * - 0x20-0x2F: Query execution
 * - 0x30-0x3F: Prepared statements
 * - 0x40-0x4F: Transactions
 * - 0x50-0x5F: Result streaming
 * - 0x60-0x6F: Administrative
 * - 0x70-0x7F: Extended features (streaming/COPY)
 * - 0xF0-0xFF: Internal/Debug
 */
enum class MessageType : uint8_t {
    // Connection lifecycle (0x01-0x0F)
    CONNECT_REQUEST     = 0x01,
    CONNECT_RESPONSE    = 0x02,
    DISCONNECT          = 0x03,

    // Authentication (0x10-0x1F)
    AUTH_REQUEST        = 0x10,
    AUTH_RESPONSE       = 0x11,
    AUTH_CHALLENGE      = 0x12,  // For future challenge-response auth
    SUBSCRIBE           = 0x13,
    UNSUBSCRIBE         = 0x14,

    // Query execution (0x20-0x2F)
    QUERY               = 0x20,
    QUERY_RESULT        = 0x21,
    QUERY_ERROR         = 0x22,
    QUERY_CANCEL        = 0x23,

    // Prepared statements (0x30-0x3F)
    PREPARE             = 0x30,
    PREPARE_RESPONSE    = 0x31,
    EXECUTE             = 0x32,
    CLOSE_STATEMENT     = 0x33,
    DESCRIBE            = 0x34,
    DESCRIBE_RESPONSE   = 0x35,

    // Transactions (0x40-0x4F)
    BEGIN_TRANSACTION   = 0x40,
    COMMIT              = 0x41,
    ROLLBACK            = 0x42,
    SAVEPOINT           = 0x43,
    RELEASE_SAVEPOINT   = 0x44,
    ROLLBACK_TO         = 0x45,
    TRANSACTION_STATUS  = 0x46,

    // Result streaming (0x50-0x5F)
    ROW_DESCRIPTION     = 0x50,
    ROW_DATA            = 0x51,
    END_OF_RESULTS      = 0x52,
    COMMAND_COMPLETE    = 0x53,
    PORTAL_SUSPENDED    = 0x4D,
    NOTIFICATION        = 0x54,
    QUERY_PROGRESS      = 0x5C,

    // Administrative (0x60-0x6F)
    SHUTDOWN            = 0x60,
    PING                = 0x61,
    PONG                = 0x62,
    STATUS_REQUEST      = 0x63,
    STATUS_RESPONSE     = 0x64,

    // Streaming/COPY (0x70-0x7F)
    COPY_DATA           = 0x70,
    COPY_DONE           = 0x71,
    COPY_FAIL           = 0x72,
    COPY_IN_RESPONSE    = 0x73,
    COPY_OUT_RESPONSE   = 0x74,
    COPY_BOTH_RESPONSE  = 0x75,
    STREAM_CONTROL      = 0x76,
    STREAM_READY        = 0x77,
    STREAM_DATA         = 0x78,
    STREAM_END          = 0x79,

    // Internal/Debug (0xF0-0xFF)
    DEBUG_MESSAGE       = 0xF0,
    PROTOCOL_ERROR      = 0xFF
};

// ============================================================================
// Message Header
// ============================================================================

/**
 * Message header structure (12 bytes)
 *
 * All messages begin with this header.
 * All multi-byte values are little-endian.
 */
#pragma pack(push, 1)
struct MessageHeader {
    uint32_t magic;           // PROTOCOL_MAGIC ("SBDB")
    uint16_t version;         // Protocol version (0x0100 for 1.0)
    uint8_t  type;            // MessageType
    uint8_t  flags;           // Reserved flags
    uint32_t payload_length;  // Length of payload in bytes
};
#pragma pack(pop)

static_assert(sizeof(MessageHeader) == 12, "MessageHeader must be 12 bytes");

/// Message flags (for future use)
enum class MessageFlags : uint8_t {
    NONE            = 0x00,
    COMPRESSED      = 0x01,  // Payload is compressed
    ENCRYPTED       = 0x02,  // Payload is encrypted (future)
    CONTINUATION    = 0x04,  // Message continues in next packet
    LAST_FRAGMENT   = 0x08   // Last fragment of a multi-packet message
};

/// Connection capability flags
constexpr uint16_t CONNECT_FLAG_ZSTD_COMPRESSION = 0x0001;
constexpr uint16_t CONNECT_FLAG_COPY             = 0x0002;
constexpr uint16_t CONNECT_FLAG_LOB_STREAM       = 0x0004;
constexpr uint16_t CONNECT_FLAG_PORTAL_PAGING    = 0x0008;
constexpr uint16_t CONNECT_FLAG_NOTIFICATIONS    = 0x0010;
constexpr uint16_t CONNECT_FLAG_PROGRESS         = 0x0020;

constexpr uint16_t CONNECT_FLAG_BASE_CAPABILITIES =
    CONNECT_FLAG_COPY |
    CONNECT_FLAG_LOB_STREAM |
    CONNECT_FLAG_PORTAL_PAGING |
    CONNECT_FLAG_PROGRESS |
    CONNECT_FLAG_NOTIFICATIONS;

// ============================================================================
// Data Type Encoding
// ============================================================================

/**
 * Wire type identifiers for data encoding
 *
 * These map to ScratchBird's internal type system but are stable
 * for wire protocol compatibility.
 */
enum class WireType : uint8_t {
    NULL_TYPE       = 0x00,
    BOOLEAN         = 0x01,
    INT16           = 0x02,
    INT32           = 0x03,
    INT64           = 0x04,
    FLOAT32         = 0x05,
    FLOAT64         = 0x06,
    DECIMAL         = 0x07,  // String representation
    VARCHAR         = 0x08,  // Length-prefixed UTF-8
    CHAR            = 0x09,  // Fixed-length UTF-8
    BYTEA           = 0x0A,  // Length-prefixed binary
    DATE            = 0x0B,  // int32 (days since 2000-01-01)
    TIME            = 0x0C,  // int64 (microseconds since midnight)
    TIMESTAMP       = 0x0D,  // int64 (microseconds since epoch)
    TIMESTAMPTZ     = 0x0E,  // int64 + int16 timezone offset
    INTERVAL        = 0x0F,  // months(int32) + days(int32) + microseconds(int64)
    UUID            = 0x10,  // 16 bytes
    JSON            = 0x11,  // Length-prefixed UTF-8 JSON
    JSONB           = 0x12,  // Binary JSON
    ARRAY           = 0x13,  // Array type (element type + elements)
    COMPOSITE       = 0x14,  // Composite/record type
    GEOMETRY        = 0x15,  // WKB format
    VECTOR          = 0x16,  // Float array for vector similarity
    MONEY           = 0x17,  // int64 (cents)
    XML             = 0x18,  // Length-prefixed UTF-8 XML
    INET            = 0x19,  // IP address
    CIDR            = 0x1A,  // Network address
    MACADDR         = 0x1B,  // MAC address
    TSVECTOR        = 0x1C,  // Text search vector
    TSQUERY         = 0x1D,  // Text search query
    RANGE           = 0x1E,  // Range type

    // Extended types (0x80+)
    UNKNOWN         = 0xFF
};

// ============================================================================
// Message Payloads
// ============================================================================

/**
 * CONNECT_REQUEST payload
 */
#pragma pack(push, 1)
struct ConnectRequestPayload {
    uint16_t protocol_version;      // Requested protocol version
    uint16_t client_flags;          // Client capability flags
    uint32_t client_pid;            // Client process ID
    char database_name[256];        // Database name (null-terminated)
    char client_name[64];           // Client identifier (e.g., "sb_isql")
    char client_version[32];        // Client version string
};
#pragma pack(pop)

/**
 * CONNECT_RESPONSE payload
 */
#pragma pack(push, 1)
struct ConnectResponsePayload {
    uint8_t  status;                // 0 = success, non-zero = error
    uint16_t server_version;        // Server protocol version
    uint16_t server_flags;          // Server capability flags
    uint8_t  session_id[16];        // UUID session identifier
    char     server_name[64];       // Server identifier
    char     server_version_str[32];// Server version string
};
#pragma pack(pop)

/**
 * AUTH_REQUEST payload
 */
#pragma pack(push, 1)
struct AuthRequestPayload {
    uint8_t  session_id[16];        // From CONNECT_RESPONSE
    char     username[64];          // Username (null-terminated)
    uint8_t  auth_method;           // 0 = password, 1 = trust, 2+ = future
    uint16_t credential_length;     // Length of credential data
    // Followed by: credential data (password hash, etc.)
};
#pragma pack(pop)

/**
 * AUTH_RESPONSE payload
 */
#pragma pack(push, 1)
struct AuthResponsePayload {
    uint8_t  status;                // 0 = success, 1 = invalid credentials, 2+ = other errors
    uint32_t user_id;               // Internal user ID (if success)
    char     error_message[256];    // Error message (if failure)
};
#pragma pack(pop)

/**
 * QUERY payload
 *
 * Variable length: header + query text
 */
#pragma pack(push, 1)
struct QueryPayloadHeader {
    uint8_t  session_id[16];        // Session ID
    uint32_t query_length;          // Length of query text (UTF-8)
    uint8_t  query_flags;           // Query execution flags
    // Followed by: query text (UTF-8, NOT null-terminated)
};
#pragma pack(pop)

/// Query execution flags
enum class QueryFlags : uint8_t {
    NONE            = 0x00,
    WANT_ROWCOUNT   = 0x01,  // Client wants row count in response
    STREAMING       = 0x02,  // Use streaming mode for large results
    EXPLAIN_ONLY    = 0x04,  // Don't execute, just explain
    NO_RESULTS      = 0x08,  // Don't send results (for DDL)
    BYTECODE        = 0x10,  // Payload is SBLR bytecode (not UTF-8 SQL)
    BYTECODE_HAS_SQL = 0x20  // Payload includes original SQL after bytecode
};

/**
 * QUERY_RESULT payload (header only, followed by ROW_DESCRIPTION and rows)
 */
#pragma pack(push, 1)
struct QueryResultPayloadHeader {
    uint8_t  status;                // 0 = success with rows, 1 = success no rows, 2+ = partial
    uint32_t column_count;          // Number of columns
    int64_t  row_count;             // Total rows (-1 = unknown/streaming)
    // Followed by: column descriptions (ROW_DESCRIPTION messages)
};
#pragma pack(pop)

/**
 * QUERY_ERROR payload
 */
#pragma pack(push, 1)
struct QueryErrorPayloadHeader {
    uint32_t error_code;            // Error code (Status enum value)
    char     sqlstate[6];           // SQLSTATE (5 chars + null)
    uint16_t message_length;        // Error message length
    uint16_t detail_length;         // Detail message length
    uint16_t hint_length;           // Hint message length
    // Followed by: message, detail, hint (UTF-8 strings)
};
#pragma pack(pop)

/**
 * ROW_DESCRIPTION payload (column metadata)
 *
 * Sent once before row data to describe column types.
 */
#pragma pack(push, 1)
struct ColumnDescription {
    uint16_t name_length;           // Column name length
    // Followed by: column name (UTF-8)
    uint8_t  type;                  // WireType
    uint32_t type_modifier;         // Type-specific modifier (e.g., precision)
    uint16_t format;                // 0 = text, 1 = binary
};
#pragma pack(pop)

/**
 * ROW_DATA payload header
 */
#pragma pack(push, 1)
struct RowDataHeader {
    uint16_t column_count;          // Number of columns in this row
    // Followed by: column values
    // Each column: int32_t length (-1 = NULL), then data
};
#pragma pack(pop)

/**
 * COMMAND_COMPLETE payload
 */
#pragma pack(push, 1)
struct CommandCompletePayload {
    char     command_tag[64];       // e.g., "SELECT 100", "INSERT 0 1"
    int64_t  rows_affected;         // Number of rows affected
};
#pragma pack(pop)

/**
 * COPY response payload (COPY_IN/OUT/BOTH_RESPONSE)
 */
#pragma pack(push, 1)
struct CopyResponsePayload {
    uint8_t  format;                // 0 = text, 1 = binary
    uint8_t  reserved;
    uint16_t column_count;
    // Followed by: uint16_t column_formats[column_count]
};
#pragma pack(pop)

/**
 * COPY_FAIL payload
 */
#pragma pack(push, 1)
struct CopyFailPayload {
    uint32_t error_length;
    // Followed by: error string (UTF-8, not null-terminated)
};
#pragma pack(pop)

/**
 * Stream control payload
 */
#pragma pack(push, 1)
struct StreamControlPayload {
    uint8_t  control_type;          // StreamControlType
    uint8_t  reserved[3];
    uint32_t window_size;           // Bytes (COPY) or rows (result streaming)
    uint32_t timeout_ms;
};
#pragma pack(pop)

/**
 * Stream ready payload
 */
#pragma pack(push, 1)
struct StreamReadyPayload {
    uint64_t stream_id;
    uint64_t total_rows;            // 0 if unknown
    uint64_t estimated_bytes;
};
#pragma pack(pop)

/**
 * Stream data payload header
 */
#pragma pack(push, 1)
struct StreamDataHeader {
    uint64_t stream_id;
    uint32_t chunk_rows;            // 0 if not row-based
    uint32_t chunk_bytes;
    // Followed by: data bytes
};
#pragma pack(pop)

/**
 * Stream end payload
 */
#pragma pack(push, 1)
struct StreamEndPayload {
    uint64_t stream_id;
    uint64_t total_rows;
    uint64_t total_bytes;
};
#pragma pack(pop)

enum class CopyFormat : uint8_t {
    TEXT = 0,
    BINARY = 1
};

enum class StreamControlType : uint8_t {
    START  = 0,
    PAUSE  = 1,
    RESUME = 2,
    CANCEL = 3,
    ACK    = 4
};

/**
 * TRANSACTION control payloads
 */
#pragma pack(push, 1)
struct TransactionPayload {
    uint8_t  session_id[16];        // Session ID
    uint8_t  isolation_level;       // 0 = default, 1 = read committed, 2 = serializable
    uint8_t  read_only;             // 0 = read-write, 1 = read-only
};

struct SavepointPayload {
    uint8_t  session_id[16];        // Session ID
    char     savepoint_name[64];    // Savepoint name (null-terminated)
};

struct TransactionStatusPayload {
    uint8_t  status;                // 0 = idle, 1 = in transaction, 2 = failed
    uint64_t transaction_id;        // Current XID (if in transaction)
};
#pragma pack(pop)

/**
 * SUBSCRIBE payload
 */
#pragma pack(push, 1)
struct SubscribePayload {
    uint8_t subscribe_type;
    uint8_t reserved[3];
    uint32_t channel_length;
    // channel bytes...
    uint32_t filter_length;
    // filter bytes...
};
#pragma pack(pop)

/**
 * UNSUBSCRIBE payload
 */
#pragma pack(push, 1)
struct UnsubscribePayload {
    uint32_t channel_length;
    // channel bytes...
};
#pragma pack(pop)

/**
 * NOTIFICATION payload
 */
#pragma pack(push, 1)
struct NotificationPayload {
    uint32_t process_id;
    uint32_t channel_length;
    // channel bytes...
    uint32_t payload_length;
    // payload bytes...
    uint8_t change_type;
    uint64_t row_id;
};
#pragma pack(pop)

/**
 * QUERY_PROGRESS payload
 */
#pragma pack(push, 1)
struct QueryProgressPayload {
    uint64_t rows_processed;
    uint64_t bytes_processed;
};
#pragma pack(pop)

/**
 * PING/PONG payload
 */
#pragma pack(push, 1)
struct PingPayload {
    uint64_t timestamp;             // Client timestamp (for latency measurement)
    uint32_t sequence;              // Sequence number
};
#pragma pack(pop)

/**
 * STATUS_REQUEST types
 */
enum class StatusRequestType : uint8_t {
    SERVER_INFO     = 0x01,
    CONNECTION_INFO = 0x02,
    DATABASE_INFO   = 0x03,
    STATISTICS      = 0x04
};

// ============================================================================
// Message Builder and Parser Classes
// ============================================================================

/**
 * Message class - represents a complete wire protocol message
 *
 * Can be built incrementally and serialized/deserialized.
 */
class Message {
public:
    Message();
    explicit Message(MessageType type);
    ~Message();

    // Disable copy
    Message(const Message&) = delete;
    Message& operator=(const Message&) = delete;

    // Allow move
    Message(Message&& other) noexcept;
    Message& operator=(Message&& other) noexcept;

    // Header access
    MessageType getType() const { return static_cast<MessageType>(header_.type); }
    void setType(MessageType type) { header_.type = static_cast<uint8_t>(type); }

    uint8_t getFlags() const { return header_.flags; }
    void setFlags(uint8_t flags) { header_.flags = flags; }

    uint32_t getPayloadLength() const { return header_.payload_length; }

    // Payload access
    const uint8_t* getPayload() const { return payload_.data(); }
    uint8_t* getPayloadMutable() { return payload_.data(); }
    size_t getPayloadSize() const { return payload_.size(); }

    // Payload building
    void clearPayload();
    void reservePayload(size_t size);

    // Write methods (append to payload)
    void writeUInt8(uint8_t value);
    void writeUInt16(uint16_t value);
    void writeUInt32(uint32_t value);
    void writeUInt64(uint64_t value);
    void writeInt32(int32_t value);
    void writeInt64(int64_t value);
    void writeFloat(float value);
    void writeDouble(double value);
    void writeBytes(const void* data, size_t length);
    void writeString(const std::string& str);
    void writeLengthPrefixedString(const std::string& str);
    void writeNullTerminatedString(const std::string& str, size_t max_length);

    // Read methods (read from payload at current offset)
    bool readUInt8(uint8_t& value);
    bool readUInt16(uint16_t& value);
    bool readUInt32(uint32_t& value);
    bool readUInt64(uint64_t& value);
    bool readInt32(int32_t& value);
    bool readInt64(int64_t& value);
    bool readFloat(float& value);
    bool readDouble(double& value);
    bool readBytes(void* buffer, size_t length);
    bool readString(std::string& str, size_t length);
    bool readLengthPrefixedString(std::string& str);
    bool readNullTerminatedString(std::string& str, size_t max_length);

    // Read position management
    size_t getReadOffset() const { return read_offset_; }
    void setReadOffset(size_t offset) { read_offset_ = offset; }
    void resetReadOffset() { read_offset_ = 0; }
    void resetReadPosition() { read_offset_ = 0; }  // Alias for convenience
    size_t getRemainingBytes() const { return payload_.size() - read_offset_; }

    // Convenience read methods (return value directly, default on error)
    uint8_t readUInt8() { uint8_t v = 0; readUInt8(v); return v; }
    uint16_t readUInt16() { uint16_t v = 0; readUInt16(v); return v; }
    uint32_t readUInt32() { uint32_t v = 0; readUInt32(v); return v; }
    uint64_t readUInt64() { uint64_t v = 0; readUInt64(v); return v; }
    int32_t readInt32() { int32_t v = 0; readInt32(v); return v; }
    int64_t readInt64() { int64_t v = 0; readInt64(v); return v; }
    std::string readLengthPrefixedString() { std::string s; readLengthPrefixedString(s); return s; }

    // Header access
    const MessageHeader& getHeader() const { return header_; }
    const uint8_t* getPayloadData() const { return payload_.data(); }

    // Serialization
    core::Status serialize(std::vector<uint8_t>& buffer) const;
    core::Status serializeHeader(uint8_t* buffer) const;

    // Deserialization
    static core::Status parseHeader(const uint8_t* data, MessageHeader& header,
                                    core::ErrorContext* ctx = nullptr);
    core::Status setPayload(const uint8_t* data, size_t length);

    // Validation
    bool isValid() const;
    static bool validateHeader(const MessageHeader& header);

private:
    MessageHeader header_;
    std::vector<uint8_t> payload_;
    size_t read_offset_ = 0;

    void updatePayloadLength() { header_.payload_length = static_cast<uint32_t>(payload_.size()); }
};

// ============================================================================
// Protocol Codec - High-level message building/parsing
// ============================================================================

/**
 * ProtocolCodec - Utility class for building and parsing protocol messages
 */
class ProtocolCodec {
public:
    // ========================================
    // Connection Messages
    // ========================================

    static Message buildConnectRequest(const std::string& database,
                                       const std::string& client_name,
                                       uint32_t client_pid,
                                       uint16_t client_flags = 0);

    static core::Status parseConnectRequest(const Message& msg,
                                            std::string& database,
                                            std::string& client_name,
                                            uint32_t& client_pid,
                                            uint16_t* client_flags_out = nullptr,
                                            core::ErrorContext* ctx = nullptr);

    static Message buildConnectResponse(bool success,
                                        const uint8_t session_id[16],
                                        const std::string& error_message = "",
                                        uint16_t server_flags = 0);

    static core::Status parseConnectResponse(const Message& msg,
                                             bool& success,
                                             uint8_t session_id[16],
                                             std::string& error_message,
                                             uint16_t* server_flags_out = nullptr,
                                             core::ErrorContext* ctx = nullptr);

    // ========================================
    // Authentication Messages
    // ========================================

    static Message buildAuthRequest(const uint8_t session_id[16],
                                    const std::string& username,
                                    const std::string& password);

    static core::Status parseAuthRequest(const Message& msg,
                                         uint8_t session_id[16],
                                         std::string& username,
                                         std::string& password,
                                         core::ErrorContext* ctx = nullptr);

    static Message buildAuthRequest(const uint8_t session_id[16],
                                    const std::string& username,
                                    AuthMethod auth_method,
                                    const std::vector<uint8_t>& payload);

    static core::Status parseAuthRequest(const Message& msg,
                                         uint8_t session_id[16],
                                         std::string& username,
                                         AuthMethod& auth_method,
                                         std::vector<uint8_t>& payload,
                                         core::ErrorContext* ctx = nullptr);

    static Message buildAuthResponse(bool success,
                                     uint32_t user_id = 0,
                                     const std::string& error_message = "");

    static core::Status parseAuthResponse(const Message& msg,
                                          bool& success,
                                          uint32_t& user_id,
                                          std::string& error_message,
                                          core::ErrorContext* ctx = nullptr);

    static Message buildAuthResponse(AuthStatus status,
                                     uint32_t user_id,
                                     const std::string& error_message,
                                     const std::vector<uint8_t>& data = {});

    static core::Status parseAuthResponse(const Message& msg,
                                          AuthStatus& status,
                                          uint32_t& user_id,
                                          std::string& error_message,
                                          std::vector<uint8_t>* data = nullptr,
                                          core::ErrorContext* ctx = nullptr);

    // ========================================
    // Query Messages
    // ========================================

    static Message buildQuery(const uint8_t session_id[16],
                              const std::string& query,
                              uint8_t flags = 0);
    static Message buildQueryCancel();
    static Message buildQueryBytecode(const uint8_t session_id[16],
                                      const std::vector<uint8_t>& bytecode,
                                      const std::string& sql,
                                      uint8_t flags = 0);
    static Message buildQueryBytecode(const uint8_t session_id[16],
                                      const std::vector<uint8_t>& bytecode,
                                      uint8_t flags = 0);

    static core::Status parseQuery(const Message& msg,
                                   uint8_t session_id[16],
                                   std::string& query,
                                   uint8_t& flags,
                                   std::vector<uint8_t>* bytecode_out,
                                   core::ErrorContext* ctx = nullptr);
    static core::Status parseQuery(const Message& msg,
                                   uint8_t session_id[16],
                                   std::string& query,
                                   uint8_t& flags,
                                   core::ErrorContext* ctx = nullptr);

    static Message buildQueryError(uint32_t error_code,
                                   const std::string& sqlstate,
                                   const std::string& message,
                                   const std::string& detail = "",
                                   const std::string& hint = "");

    static core::Status parseQueryError(const Message& msg,
                                        uint32_t& error_code,
                                        std::string& sqlstate,
                                        std::string& message,
                                        std::string& detail,
                                        std::string& hint,
                                        core::ErrorContext* ctx = nullptr);

    // ========================================
    // Result Messages
    // ========================================

    /**
     * Column metadata for result sets
     */
    struct ColumnInfo {
        std::string name;
        WireType type{WireType::UNKNOWN};
        uint32_t type_modifier{0};
        uint32_t table_oid{0};
        uint16_t column_index{0};
        uint32_t type_oid{0};
        int16_t type_size{0};
        uint8_t format{1};
        bool nullable{true};
    };

    static Message buildRowDescription(const std::vector<ColumnInfo>& columns);

    static core::Status parseRowDescription(const Message& msg,
                                            std::vector<ColumnInfo>& columns,
                                            core::ErrorContext* ctx = nullptr);

    /**
     * Row value - can be NULL or contain typed data
     */
    struct ColumnValue {
        bool is_null = true;
        bool is_stream = false;
        uint64_t stream_id = 0;
        uint64_t stream_length = 0;
        std::vector<uint8_t> data;

        ColumnValue() = default;
        explicit ColumnValue(std::nullptr_t) : is_null(true) {}

        template<typename T>
        static ColumnValue fromValue(const T& value);

        // Convenience constructors
        static ColumnValue fromInt32(int32_t value);
        static ColumnValue fromInt64(int64_t value);
        static ColumnValue fromDouble(double value);
        static ColumnValue fromString(const std::string& value);
        static ColumnValue fromBool(bool value);
        static ColumnValue fromBytes(const uint8_t* data, size_t length);
        static ColumnValue fromStream(uint64_t stream_id,
                                      uint64_t stream_length,
                                      const uint8_t* data,
                                      size_t length);
    };

    static Message buildRowData(const std::vector<ColumnValue>& values);

    static core::Status parseRowData(const Message& msg,
                                     std::vector<ColumnValue>& values,
                                     core::ErrorContext* ctx = nullptr);

    static Message buildEndOfResults();

    static Message buildCommandComplete(const std::string& command_tag,
                                        int64_t rows_affected);

    static core::Status parseCommandComplete(const Message& msg,
                                             std::string& command_tag,
                                             int64_t& rows_affected,
                                             core::ErrorContext* ctx = nullptr);

    static Message buildSubscribe(uint8_t subscribe_type,
                                  const std::string& channel,
                                  const std::string& filter);
    static core::Status parseSubscribe(const Message& msg,
                                       uint8_t& subscribe_type,
                                       std::string& channel,
                                       std::string& filter,
                                       core::ErrorContext* ctx = nullptr);

    static Message buildUnsubscribe(const std::string& channel);
    static core::Status parseUnsubscribe(const Message& msg,
                                         std::string& channel,
                                         core::ErrorContext* ctx = nullptr);

    static Message buildNotification(uint32_t process_id,
                                     const std::string& channel,
                                     const std::vector<uint8_t>& payload,
                                     uint8_t change_type,
                                     uint64_t row_id);
    static core::Status parseNotification(const Message& msg,
                                          uint32_t& process_id,
                                          std::string& channel,
                                          std::vector<uint8_t>& payload,
                                          uint8_t& change_type,
                                          uint64_t& row_id,
                                          core::ErrorContext* ctx = nullptr);

    static Message buildQueryProgress(uint64_t rows_processed,
                                      uint64_t bytes_processed);

    static core::Status parseQueryProgress(const Message& msg,
                                           uint64_t& rows_processed,
                                           uint64_t& bytes_processed,
                                           core::ErrorContext* ctx = nullptr);

    // ========================================
    // COPY Messages
    // ========================================

    static Message buildCopyInResponse(CopyFormat format,
                                       const std::vector<uint16_t>& column_formats);
    static Message buildCopyOutResponse(CopyFormat format,
                                        const std::vector<uint16_t>& column_formats);
    static Message buildCopyBothResponse(CopyFormat format,
                                         const std::vector<uint16_t>& column_formats);
    static Message buildCopyData(const uint8_t* data, size_t length);
    static core::Status parseCopyData(const Message& msg,
                                      const uint8_t** data,
                                      size_t* length,
                                      core::ErrorContext* ctx = nullptr);
    static Message buildCopyDone();
    static Message buildCopyFail(const std::string& error_message);
    static core::Status parseCopyFail(const Message& msg,
                                      std::string& error_message,
                                      core::ErrorContext* ctx = nullptr);

    // ========================================
    // Streaming Messages
    // ========================================

    static Message buildStreamControl(StreamControlType control_type,
                                      uint32_t window_size,
                                      uint32_t timeout_ms);
    static core::Status parseStreamControl(const Message& msg,
                                           StreamControlType& control_type,
                                           uint32_t& window_size,
                                           uint32_t& timeout_ms,
                                           core::ErrorContext* ctx = nullptr);
    static Message buildStreamReady(uint64_t stream_id,
                                    uint64_t total_rows,
                                    uint64_t estimated_bytes);
    static core::Status parseStreamReady(const Message& msg,
                                         uint64_t& stream_id,
                                         uint64_t& total_rows,
                                         uint64_t& estimated_bytes,
                                         core::ErrorContext* ctx = nullptr);
    static Message buildStreamData(uint64_t stream_id,
                                   uint32_t chunk_rows,
                                   const uint8_t* data,
                                   size_t length);
    static core::Status parseStreamData(const Message& msg,
                                        uint64_t& stream_id,
                                        uint32_t& chunk_rows,
                                        const uint8_t** data,
                                        size_t* length,
                                        core::ErrorContext* ctx = nullptr);
    static Message buildStreamEnd(uint64_t stream_id,
                                  uint64_t total_rows,
                                  uint64_t total_bytes);
    static core::Status parseStreamEnd(const Message& msg,
                                       uint64_t& stream_id,
                                       uint64_t& total_rows,
                                       uint64_t& total_bytes,
                                       core::ErrorContext* ctx = nullptr);

    // ========================================
    // Transaction Messages
    // ========================================

    static Message buildBeginTransaction(const uint8_t session_id[16],
                                         uint8_t isolation_level = 0,
                                         bool read_only = false);

    static Message buildCommit(const uint8_t session_id[16]);
    static Message buildRollback(const uint8_t session_id[16]);

    static Message buildSavepoint(const uint8_t session_id[16],
                                  const std::string& name);

    static Message buildReleaseSavepoint(const uint8_t session_id[16],
                                         const std::string& name);

    static Message buildRollbackTo(const uint8_t session_id[16],
                                   const std::string& name);

    static Message buildTransactionStatus(uint8_t status, uint64_t xid);

    // ========================================
    // Administrative Messages
    // ========================================

    static Message buildPing(uint64_t timestamp, uint32_t sequence);
    static Message buildPong(uint64_t timestamp, uint32_t sequence);

    static core::Status parsePing(const Message& msg,
                                  uint64_t& timestamp,
                                  uint32_t& sequence,
                                  core::ErrorContext* ctx = nullptr);
    // ========================================
    // Status Messages
    // ========================================

    struct StatusEntry {
        std::string key;
        std::string value;
    };

    static Message buildStatusRequest(StatusRequestType request_type);
    static core::Status parseStatusRequest(const Message& msg,
                                           StatusRequestType& request_type,
                                           core::ErrorContext* ctx = nullptr);
    static Message buildStatusResponse(StatusRequestType request_type,
                                       const std::vector<StatusEntry>& entries);
    static core::Status parseStatusResponse(const Message& msg,
                                            StatusRequestType& request_type,
                                            std::vector<StatusEntry>& entries,
                                            core::ErrorContext* ctx = nullptr);

    static Message buildDisconnect();
    static Message buildShutdown();

    static Message buildProtocolError(const std::string& message);

private:
    ProtocolCodec() = delete;  // Static utility class
};

// ============================================================================
// Protocol Session - Manages message I/O over a connection
// ============================================================================

/**
 * ProtocolSession - Handles message-level I/O over an IPC connection
 *
 * Provides reliable message framing and delivery over the raw byte stream.
 */
class ProtocolSession {
public:
    explicit ProtocolSession(scratchbird::server::IPCConnection* connection);
    ~ProtocolSession();

    // Disable copy/move
    ProtocolSession(const ProtocolSession&) = delete;
    ProtocolSession& operator=(const ProtocolSession&) = delete;

    /**
     * Send a message
     *
     * Serializes the message and sends it over the connection.
     *
     * @param msg Message to send
     * @param ctx Error context
     * @return Status::OK on success
     */
    core::Status sendMessage(const Message& msg, core::ErrorContext* ctx = nullptr);

    /**
     * Receive a message
     *
     * Reads a complete message from the connection.
     * Blocks until a message is available or error occurs.
     *
     * @param msg Output message
     * @param ctx Error context
     * @return Status::OK on success
     */
    core::Status receiveMessage(Message& msg, core::ErrorContext* ctx = nullptr);

    /**
     * Check if connection is still valid
     */
    bool isConnected() const;

    /**
     * Get underlying connection
     */
    scratchbird::server::IPCConnection* getConnection() { return connection_; }

    /**
     * Get session statistics
     */
    uint64_t getMessagesSent() const { return messages_sent_; }
    uint64_t getMessagesReceived() const { return messages_received_; }
    uint64_t getBytesSent() const { return bytes_sent_; }
    uint64_t getBytesReceived() const { return bytes_received_; }

private:
    scratchbird::server::IPCConnection* connection_;  // Not owned
    mutable std::mutex send_mutex_;
    uint64_t messages_sent_ = 0;
    uint64_t messages_received_ = 0;
    uint64_t bytes_sent_ = 0;
    uint64_t bytes_received_ = 0;
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Convert MessageType to string for debugging
 */
const char* messageTypeToString(MessageType type);

/**
 * Convert WireType to string
 */
const char* wireTypeToString(WireType type);

/**
 * Generate a random session ID (UUID v4)
 */
void generateSessionId(uint8_t session_id[16]);

/**
 * Format session ID as string (for logging)
 */
std::string sessionIdToString(const uint8_t session_id[16]);

} // namespace protocol
} // namespace scratchbird
