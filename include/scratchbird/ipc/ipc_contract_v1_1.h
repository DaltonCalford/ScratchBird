/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0
 */
#pragma once

/**
 * IPC Contract v1.1
 * 
 * Section E1: IPC Contract - Message Types
 * 
 * Defines the inter-process communication protocol between the ScratchBird
 * engine and parser agents. Uses shared memory queues with socket notifications.
 */

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <optional>

namespace scratchbird {
namespace ipc {

// ============================================================================
// IPC Protocol Constants
// ============================================================================

constexpr uint16_t IPC_VERSION_1_1 = 0x0101;
constexpr uint16_t IPC_CURRENT_VERSION = IPC_VERSION_1_1;

// Maximum sizes
constexpr size_t IPC_MAX_MESSAGE_SIZE = 1024 * 1024;      // 1MB
constexpr size_t IPC_MAX_PAYLOAD_SIZE = 1020 * 1024;      // ~1MB minus header
constexpr size_t IPC_MAX_SQL_LENGTH = 512 * 1024;         // 512KB SQL
constexpr size_t IPC_MAX_PARAMS = 1024;                   // Max bind parameters
constexpr size_t IPC_MAX_FIELDS = 1024;                   // Max result fields
constexpr size_t IPC_MAX_BATCH_ROWS = 10000;              // Max rows per batch
constexpr size_t IPC_MAX_COPY_CHUNK = 64 * 1024;          // 64KB COPY chunks

// Feature flags (bitmask)
constexpr uint32_t IPC_FEATURE_PREPARED_STATEMENTS = 0x00000001;
constexpr uint32_t IPC_FEATURE_COPY_STREAMING = 0x00000002;
constexpr uint32_t IPC_FEATURE_NOTIFICATIONS = 0x00000004;
constexpr uint32_t IPC_FEATURE_CANCEL = 0x00000008;
constexpr uint32_t IPC_FEATURE_BINARY_RESULTS = 0x00000010;
constexpr uint32_t IPC_FEATURE_COMPRESSION = 0x00000020;
constexpr uint32_t IPC_FEATURE_ENCRYPTION = 0x00000040;
constexpr uint32_t IPC_FEATURE_BATCH_EXECUTION = 0x00000080;

// ============================================================================
// IPC Message Types
// ============================================================================

enum class IPCMessageType : uint16_t {
    // Connection Management (0x01-0x0F)
    STARTUP = 0x01,           // Client -> Server: Initial connection
    READY = 0x02,             // Server -> Client: Connection ready
    FEATURE_NEGOTIATE = 0x03, // Bidirectional: Feature negotiation
    TERMINATE = 0x04,         // Bidirectional: Clean disconnect
    PING = 0x05,              // Client -> Server: Keepalive
    PONG = 0x06,              // Server -> Client: Keepalive response
    
    // Session Management (0x10-0x1F)
    ATTACH = 0x10,            // Client -> Server: Attach to database
    DETACH = 0x11,            // Client -> Server: Detach from database
    ATTACHED = 0x12,          // Server -> Client: Attach successful
    DETACHED = 0x13,          // Server -> Client: Detach successful
    
    // Query Execution (0x20-0x2F)
    SIMPLE_QUERY = 0x20,      // Client -> Server: Execute SQL string
    PARSE = 0x21,             // Client -> Server: Parse SQL
    BIND = 0x22,              // Client -> Server: Bind parameters
    DESCRIBE = 0x23,          // Client -> Server: Describe statement/portal
    EXECUTE = 0x24,           // Client -> Server: Execute statement
    CLOSE = 0x25,             // Client -> Server: Close statement/portal
    SYNC = 0x26,              // Client -> Server: End request batch
    
    // Results (0x30-0x3F)
    ROW_DESCRIPTION = 0x30,   // Server -> Client: Result schema
    DATA_ROW = 0x31,          // Server -> Client: Single row
    DATA_BATCH = 0x32,        // Server -> Client: Multiple rows
    COMMAND_COMPLETE = 0x33,  // Server -> Client: Query done
    EMPTY_RESPONSE = 0x34,    // Server -> Client: No results
    PARSE_COMPLETE = 0x35,    // Server -> Client: Parse done
    BIND_COMPLETE = 0x36,     // Server -> Client: Bind done
    CLOSE_COMPLETE = 0x37,    // Server -> Client: Close done
    
    // Command Completion (0x38-0x3F)
    READY_FOR_QUERY = 0x38,   // Server -> Client: Ready for next query
    
    // COPY Operations (0x40-0x4F)
    COPY_IN_REQUEST = 0x40,   // Server -> Client: Expect COPY data
    COPY_OUT_RESPONSE = 0x41, // Server -> Client: COPY data incoming
    COPY_DATA = 0x42,         // Bidirectional: COPY data chunk
    COPY_DONE = 0x43,         // Client -> Server: COPY complete
    COPY_FAIL = 0x44,         // Client -> Server: COPY error
    COPY_COMPLETE = 0x45,     // Server -> Client: COPY done
    STREAM_CONTROL = 0x46,    // Bidirectional: Flow control
    
    // Transactions (0x50-0x5F)
    TXN_BEGIN = 0x50,         // Client -> Server: Begin transaction
    TXN_COMMIT = 0x51,        // Client -> Server: Commit transaction
    TXN_ROLLBACK = 0x52,      // Client -> Server: Rollback transaction
    SAVEPOINT = 0x53,         // Client -> Server: Create savepoint
    RELEASE = 0x54,           // Client -> Server: Release savepoint
    ROLLBACK_TO = 0x55,       // Client -> Server: Rollback to savepoint
    TXN_COMPLETE = 0x56,      // Server -> Client: Transaction command done
    
    // Asynchronous (0x60-0x6F)
    NOTIFY_SUBSCRIBE = 0x60,  // Client -> Server: Subscribe to channel
    NOTIFY_UNSUBSCRIBE = 0x61,// Client -> Server: Unsubscribe
    NOTIFY_DELIVER = 0x62,    // Server -> Client: Notification
    CANCEL_REQUEST = 0x63,    // Client -> Server: Cancel query
    CANCEL_ACK = 0x64,        // Server -> Client: Cancel accepted
    
    // Errors (0x70-0x7F)
    ERROR_RESPONSE = 0x70,    // Server -> Client: Error occurred
    NOTICE = 0x71,            // Server -> Client: Warning/info
    
    // Internal (0x80-0xFF)
    HEARTBEAT = 0x80,         // Internal: Health check
    SHUTDOWN = 0x81,          // Internal: Server shutdown notice
};

// ============================================================================
// IPC Header Structure
// ============================================================================

struct alignas(8) IPCHeader {
    uint32_t magic;           // 'SBIP' (0x53424950)
    uint16_t version;         // Protocol version (1.1)
    uint16_t type;            // IPCMessageType
    uint32_t length;          // Payload length (excluding header)
    uint32_t request_id;      // Request sequence number
    uint32_t session_id;      // Session identifier
    uint64_t timestamp;       // Message timestamp (ns since epoch)
    uint32_t flags;           // Message flags
    uint32_t reserved;        // Reserved for future use
    
    static constexpr uint32_t MAGIC = 0x53424950;  // "SBIP"
    static constexpr uint32_t FLAG_COMPRESSED = 0x00000001;
    static constexpr uint32_t FLAG_ENCRYPTED = 0x00000002;
    static constexpr uint32_t FLAG_URGENT = 0x00000004;
    
    bool isValid() const {
        return magic == MAGIC && version == IPC_CURRENT_VERSION;
    }
};

static_assert(sizeof(IPCHeader) == 40, "IPCHeader must be 40 bytes");

// ============================================================================
// Field Description
// ============================================================================

struct IPCFieldDesc {
    char name[64];            // Field name
    uint32_t table_oid;       // Table OID
    uint16_t column_num;      // Column number
    uint16_t type_oid;        // Type OID
    int16_t type_size;        // Type size (-1 for variable)
    int32_t type_modifier;    // Type modifier
    uint16_t format;          // 0=text, 1=binary
};

// ============================================================================
// Parameter Value
// ============================================================================

struct IPCParamValue {
    uint16_t type_oid;        // Parameter type
    uint16_t format;          // 0=text, 1=binary
    int32_t length;           // -1 for NULL
    // Data follows inline
};

// ============================================================================
// Message Payload Structures
// ============================================================================

// STARTUP payload
struct IPCStartupPayload {
    uint32_t process_id;      // Client process ID
    uint32_t secret_key;      // Secret key for cancel
    uint32_t feature_flags;   // Requested features
    char database[64];        // Database name
    char user[64];            // Username
    char application[64];     // Application name
};

// READY payload
struct IPCReadyPayload {
    uint32_t session_id;      // Assigned session ID
    uint32_t server_features; // Supported features
    char server_version[32];  // Server version string
};

// FEATURE_NEGOTIATE payload
struct IPCFeaturePayload {
    uint32_t client_features; // Client features
    uint32_t server_features; // Server features
    uint32_t agreed_features; // Negotiated features
};

// SIMPLE_QUERY payload
struct IPCSimpleQueryPayload {
    uint32_t flags;           // Query flags
    uint32_t query_length;    // Length of SQL text
    // SQL text follows
};

// PARSE payload
struct IPCParsePayload {
    char stmt_name[64];       // Statement name
    char sql[IPC_MAX_SQL_LENGTH]; // SQL text
    uint16_t param_types[IPC_MAX_PARAMS]; // Parameter type OIDs
};

// BIND payload
struct IPCBindPayload {
    char portal_name[64];     // Portal name
    char stmt_name[64];       // Statement name
    uint16_t num_params;      // Number of parameters
    // IPCParamValue + data follows
};

// EXECUTE payload
struct IPCExecutePayload {
    char portal_name[64];     // Portal name
    uint32_t max_rows;        // Max rows (0=unlimited)
};

// CLOSE payload
struct IPCClosePayload {
    uint8_t type;             // 'S'=statement, 'P'=portal
    char name[64];            // Name to close
};

// ROW_DESCRIPTION payload
struct IPCRowDescPayload {
    uint16_t num_fields;      // Number of fields
    // IPCFieldDesc array follows
};

// DATA_ROW payload
struct IPCDataRowPayload {
    uint16_t num_fields;      // Number of fields
    // Field data follows (length-prefixed)
};

// ROW_DESCRIPTION payload
struct IPCRowDescriptionPayload {
    uint16_t num_fields;      // Number of fields
    // Field descriptions follow (IPCFieldDesc array)
};

// COMMAND_COMPLETE payload
struct IPCCommandCompletePayload {
    char tag[64];             // Command tag (e.g., "SELECT 42")
    uint64_t rows_affected;   // Rows affected
    uint64_t last_insert_id;  // Last insert ID
};

// COPY_DATA payload
struct IPCCopyDataPayload {
    uint32_t chunk_id;        // Chunk sequence number
    uint32_t length;          // Data length
    // Data follows
};

// COPY_IN_REQUEST payload (server -> client: expect COPY data)
struct IPCCopyInRequestPayload {
    uint8_t format;           // 0=text, 1=binary
    uint16_t num_columns;     // Number of columns
    // Column formats follow (uint16_t per column)
};

// COPY_OUT_RESPONSE payload (server -> client: COPY data incoming)
struct IPCCopyOutResponsePayload {
    uint8_t format;           // 0=text, 1=binary
    uint16_t num_columns;     // Number of columns
    // Column formats follow (uint16_t per column)
};

// COPY_FAIL payload (client -> server: COPY error)
struct IPCCopyFailPayload {
    uint32_t error_len;       // Error message length
    // Error message follows
};

// STREAM_CONTROL payload (flow control)
struct IPCStreamControlPayload {
    int32_t credits;          // Positive=grant, negative=revoke
    uint32_t buffer_avail;    // Available buffer space
};

// ERROR_RESPONSE payload
struct IPCErrorPayload {
    char sqlstate[6];         // SQLSTATE code
    char message[512];        // Error message
    char detail[1024];        // Error detail
    char hint[512];           // Hint
    char file[128];           // Source file
    uint32_t line;            // Source line
    char function[128];       // Source function
};

// NOTIFY payload
struct IPCNotifyPayload {
    uint32_t channel_id;      // Channel identifier
    char channel[64];         // Channel name
    char payload[1024];       // Notification payload
    uint64_t timestamp;       // Notification timestamp
};

// ============================================================================
// IPC Message Container
// ============================================================================

class IPCMessage {
public:
    IPCHeader header;
    std::vector<uint8_t> payload;
    
    IPCMessage();
    IPCMessage(IPCMessageType type, uint32_t session_id = 0);
    
    // Serialization
    std::vector<uint8_t> serialize() const;
    bool deserialize(const uint8_t* data, size_t len);
    
    // Payload helpers
    template<typename T>
    const T* getPayload() const {
        if (payload.size() < sizeof(T)) return nullptr;
        return reinterpret_cast<const T*>(payload.data());
    }
    
    template<typename T>
    T* getPayload() {
        payload.resize(sizeof(T));
        return reinterpret_cast<T*>(payload.data());
    }
    
    // Type checking
    IPCMessageType getType() const { 
        return static_cast<IPCMessageType>(header.type); 
    }
    void setType(IPCMessageType type) { header.type = static_cast<uint16_t>(type); }
    
    // Validation
    bool isValid() const;
    size_t getTotalSize() const { return sizeof(header) + payload.size(); }
};

// ============================================================================
// IPC Channel Interface
// ============================================================================

class IPCChannel {
public:
    virtual ~IPCChannel() = default;
    
    // Connection
    virtual core::Status connect(const std::string& endpoint,
                                core::ErrorContext* ctx = nullptr) = 0;
    virtual core::Status disconnect(core::ErrorContext* ctx = nullptr) = 0;
    virtual bool isConnected() const = 0;
    
    // Message I/O
    virtual core::Status send(const IPCMessage& msg,
                             core::ErrorContext* ctx = nullptr) = 0;
    virtual core::Status receive(IPCMessage& msg,
                                core::ErrorContext* ctx = nullptr) = 0;
    virtual core::Status tryReceive(IPCMessage& msg, uint32_t timeout_ms,
                                   core::ErrorContext* ctx = nullptr) = 0;
    
    // Properties
    virtual std::string getEndpoint() const = 0;
    virtual uint32_t getSessionId() const = 0;
};

// ============================================================================
// IPC Channel Factory
// ============================================================================

enum class IPCChannelType {
    UNIX_SOCKET,    // Linux/macOS Unix domain sockets
    NAMED_PIPE,     // Windows named pipes
    TCP_LOOPBACK,   // TCP localhost (fallback)
    SHARED_MEMORY,  // Shared memory with events
};

class IPCChannelFactory {
public:
    static std::unique_ptr<IPCChannel> create(IPCChannelType type);
    static std::unique_ptr<IPCChannel> createDefault();
    
    // Platform detection
    static IPCChannelType getDefaultType();
    static bool isSupported(IPCChannelType type);
};

// ============================================================================
// Utility Functions
// ============================================================================

// Convert IPC message type to string
const char* ipcMessageTypeToString(IPCMessageType type);

// Get feature flag name
const char* ipcFeatureFlagToString(uint32_t flag);

// Validate IPC message
bool validateIPCMessage(const IPCMessage& msg, std::string& error);

} // namespace ipc
} // namespace scratchbird
