/**
 * Socket Types and Constants
 *
 * ScratchBird Network Layer - Phase 3.1
 *
 * Common types, constants, and platform-specific definitions for the network layer.
 */

#pragma once

#include <cstdint>
#include <string>
#include <chrono>
#include <memory>
#include <functional>
#include <vector>
#include <atomic>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    using socket_t = SOCKET;
    constexpr socket_t INVALID_SOCKET_VALUE = INVALID_SOCKET;
    constexpr int SOCKET_ERROR_VALUE = SOCKET_ERROR;
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    #include <sys/un.h>
    using socket_t = int;
    constexpr socket_t INVALID_SOCKET_VALUE = -1;
    constexpr int SOCKET_ERROR_VALUE = -1;
#endif

namespace scratchbird {
namespace network {

using ::socket_t;
using ::INVALID_SOCKET_VALUE;
using ::SOCKET_ERROR_VALUE;

// ============================================================================
// Forward Declarations
// ============================================================================

class Socket;
class ServerSocket;
class EventLoop;
class ThreadPool;
class ConnectionHandler;

// ============================================================================
// Constants
// ============================================================================

// Default ports for protocols
constexpr uint16_t DEFAULT_NATIVE_PORT = 3092;      // ScratchBird native protocol
constexpr uint16_t DEFAULT_POSTGRESQL_PORT = 5432;  // PostgreSQL emulation
constexpr uint16_t DEFAULT_MYSQL_PORT = 3306;       // MySQL emulation
constexpr uint16_t DEFAULT_FIREBIRD_PORT = 3050;    // Firebird emulation

// Buffer sizes
constexpr size_t DEFAULT_RECV_BUFFER_SIZE = 65536;      // 64KB receive buffer
constexpr size_t DEFAULT_SEND_BUFFER_SIZE = 65536;      // 64KB send buffer
constexpr size_t MAX_MESSAGE_SIZE = 1073741824;         // 1GB max message
constexpr size_t SOCKET_READ_CHUNK = 8192;              // 8KB read chunk

// Timeouts (milliseconds)
constexpr uint32_t DEFAULT_CONNECT_TIMEOUT_MS = 30000;  // 30 seconds
constexpr uint32_t DEFAULT_READ_TIMEOUT_MS = 300000;    // 5 minutes
constexpr uint32_t DEFAULT_WRITE_TIMEOUT_MS = 300000;   // 5 minutes
constexpr uint32_t DEFAULT_ACCEPT_TIMEOUT_MS = 1000;    // 1 second
constexpr uint32_t DEFAULT_KEEPALIVE_INTERVAL_MS = 60000; // 1 minute

// Connection limits
constexpr uint32_t DEFAULT_MAX_CONNECTIONS = 1000;
constexpr uint32_t DEFAULT_LISTEN_BACKLOG = 128;
constexpr uint32_t DEFAULT_WORKER_THREADS = 0;  // 0 = auto (CPU cores * 2)

// ============================================================================
// Enums
// ============================================================================

/**
 * Protocol type for wire protocol selection
 */
enum class ProtocolType : uint8_t {
    NATIVE = 0,         // ScratchBird native binary protocol
    POSTGRESQL = 1,     // PostgreSQL wire protocol v3
    MYSQL = 2,          // MySQL wire protocol
    FIREBIRD = 3,       // Firebird wire protocol
    AUTO_DETECT = 255   // Auto-detect from first bytes
};

/**
 * Socket address family
 */
enum class AddressFamily : uint8_t {
    IPV4 = 0,           // IPv4 (AF_INET)
    IPV6 = 1,           // IPv6 (AF_INET6)
    UNIX = 2,           // Unix domain socket (AF_UNIX)
    UNSPEC = 255        // Unspecified (AF_UNSPEC)
};

/**
 * Socket type
 */
enum class SocketType : uint8_t {
    STREAM = 0,         // TCP (SOCK_STREAM)
    DATAGRAM = 1,       // UDP (SOCK_DGRAM)
    RAW = 2             // Raw socket (SOCK_RAW)
};

/**
 * Socket state
 */
enum class SocketState : uint8_t {
    CLOSED = 0,         // Socket not open
    CREATED = 1,        // Socket created but not connected/bound
    BOUND = 2,          // Socket bound to address
    LISTENING = 3,      // Server socket listening
    CONNECTING = 4,     // Connection in progress
    CONNECTED = 5,      // Connection established
    SSL_HANDSHAKE = 6,  // SSL/TLS handshake in progress
    READY = 7,          // Ready for I/O
    CLOSING = 8,        // Close in progress
    ERROR_STATE = 9     // Error state
};

/**
 * Event types for event loop
 */
enum class EventType : uint32_t {
    NONE = 0x00,
    READ = 0x01,        // Socket ready for read
    WRITE = 0x02,       // Socket ready for write
    ERROR = 0x04,       // Socket error
    HANGUP = 0x08,      // Connection closed
    TIMEOUT = 0x10,     // Operation timeout
    ACCEPT = 0x20       // New connection available
};

// Allow bitwise operations on EventType
inline EventType operator|(EventType a, EventType b) {
    return static_cast<EventType>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline EventType operator&(EventType a, EventType b) {
    return static_cast<EventType>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}
inline EventType& operator|=(EventType& a, EventType b) {
    return a = a | b;
}
inline bool hasEvent(EventType events, EventType check) {
    return (static_cast<uint32_t>(events) & static_cast<uint32_t>(check)) != 0;
}

/**
 * SSL/TLS mode
 */
enum class SSLMode : uint8_t {
    DISABLED = 0,       // No SSL/TLS
    ALLOW = 1,          // Allow but don't require
    PREFER = 2,         // Prefer SSL/TLS if available
    REQUIRE = 3,        // Require SSL/TLS
    VERIFY_CA = 4,      // Require and verify CA
    VERIFY_FULL = 5     // Require and verify CA + hostname
};

/**
 * Connection close reason
 */
enum class CloseReason : uint8_t {
    NORMAL = 0,             // Normal close
    TIMEOUT = 1,            // Connection timeout
    PROTOCOL_ERROR = 2,     // Protocol violation
    AUTH_FAILURE = 3,       // Authentication failed
    RESOURCE_LIMIT = 4,     // Resource limit exceeded
    SERVER_SHUTDOWN = 5,    // Server is shutting down
    CLIENT_DISCONNECT = 6,  // Client disconnected
    SSL_ERROR = 7,          // SSL/TLS error
    IO_ERROR = 8,           // I/O error
    INTERNAL_ERROR = 9      // Internal server error
};

// ============================================================================
// Structures
// ============================================================================

/**
 * Network address (IP + port or Unix path)
 */
struct NetworkAddress {
    AddressFamily family = AddressFamily::UNSPEC;
    std::string host;       // Hostname or IP address
    uint16_t port = 0;      // Port number (TCP/UDP)
    std::string path;       // Unix socket path

    NetworkAddress() = default;

    NetworkAddress(const std::string& h, uint16_t p, AddressFamily af = AddressFamily::IPV4)
        : family(af), host(h), port(p) {}

    explicit NetworkAddress(const std::string& unix_path)
        : family(AddressFamily::UNIX), path(unix_path) {}

    std::string toString() const {
        if (family == AddressFamily::UNIX) {
            return "unix:" + path;
        }
        if (family == AddressFamily::IPV6) {
            return "[" + host + "]:" + std::to_string(port);
        }
        return host + ":" + std::to_string(port);
    }

    bool isValid() const {
        if (family == AddressFamily::UNIX) {
            return !path.empty();
        }
        return !host.empty() && port > 0;
    }
};

/**
 * Socket options
 */
struct SocketOptions {
    bool reuse_address = true;          // SO_REUSEADDR
    bool reuse_port = false;            // SO_REUSEPORT
    bool tcp_nodelay = true;            // TCP_NODELAY (disable Nagle)
    bool tcp_keepalive = true;          // SO_KEEPALIVE
    uint32_t keepalive_idle_sec = 60;   // TCP_KEEPIDLE (seconds)
    uint32_t keepalive_interval_sec = 10; // TCP_KEEPINTVL (seconds)
    uint32_t keepalive_count = 5;       // TCP_KEEPCNT
    size_t recv_buffer_size = DEFAULT_RECV_BUFFER_SIZE;
    size_t send_buffer_size = DEFAULT_SEND_BUFFER_SIZE;
    uint32_t connect_timeout_ms = DEFAULT_CONNECT_TIMEOUT_MS;
    uint32_t read_timeout_ms = DEFAULT_READ_TIMEOUT_MS;
    uint32_t write_timeout_ms = DEFAULT_WRITE_TIMEOUT_MS;
    bool non_blocking = false;          // Non-blocking I/O mode
};

/**
 * Connection statistics
 */
struct NetworkStats {
    std::atomic<uint64_t> bytes_sent{0};
    std::atomic<uint64_t> bytes_received{0};
    std::atomic<uint64_t> messages_sent{0};
    std::atomic<uint64_t> messages_received{0};
    std::atomic<uint64_t> errors{0};
    std::chrono::steady_clock::time_point connected_at;
    std::chrono::steady_clock::time_point last_activity;
    std::chrono::steady_clock::time_point last_read;
    std::chrono::steady_clock::time_point last_write;

    NetworkStats() : connected_at(std::chrono::steady_clock::now()),
                     last_activity(connected_at), last_read(connected_at), last_write(connected_at) {}

    void reset() {
        bytes_sent = 0;
        bytes_received = 0;
        messages_sent = 0;
        messages_received = 0;
        errors = 0;
        connected_at = std::chrono::steady_clock::now();
        last_activity = connected_at;
        last_read = connected_at;
        last_write = connected_at;
    }
};

/**
 * Server statistics
 */
struct ServerStats {
    std::atomic<uint64_t> total_connections{0};
    std::atomic<uint64_t> active_connections{0};
    std::atomic<uint64_t> total_bytes_sent{0};
    std::atomic<uint64_t> total_bytes_received{0};
    std::atomic<uint64_t> total_queries{0};
    std::atomic<uint64_t> errors{0};
    std::atomic<uint64_t> auth_failures{0};
    std::atomic<uint64_t> timeouts{0};
    std::chrono::steady_clock::time_point started_at;

    ServerStats() : started_at(std::chrono::steady_clock::now()) {}
};

/**
 * Event callback data
 */
struct EventData {
    socket_t fd;                        // Socket file descriptor
    EventType events;                   // Events that occurred
    void* user_data;                    // User-provided data pointer
    std::chrono::steady_clock::time_point timestamp;

    EventData(socket_t f, EventType e, void* ud = nullptr)
        : fd(f), events(e), user_data(ud), timestamp(std::chrono::steady_clock::now()) {}
};

// ============================================================================
// Callback Types
// ============================================================================

/**
 * Event callback function type
 */
using EventCallback = std::function<void(const EventData& event)>;

/**
 * Connection callback function type
 */
using ConnectionCallback = std::function<void(std::unique_ptr<Socket> socket)>;

/**
 * Error callback function type
 */
using ErrorCallback = std::function<void(socket_t fd, int error_code, const std::string& message)>;

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Convert ProtocolType to string
 */
inline const char* protocolTypeToString(ProtocolType type) {
    switch (type) {
        case ProtocolType::NATIVE: return "native";
        case ProtocolType::POSTGRESQL: return "postgresql";
        case ProtocolType::MYSQL: return "mysql";
        case ProtocolType::FIREBIRD: return "firebird";
        case ProtocolType::AUTO_DETECT: return "auto";
        default: return "unknown";
    }
}

/**
 * Convert SocketState to string
 */
inline const char* socketStateToString(SocketState state) {
    switch (state) {
        case SocketState::CLOSED: return "closed";
        case SocketState::CREATED: return "created";
        case SocketState::BOUND: return "bound";
        case SocketState::LISTENING: return "listening";
        case SocketState::CONNECTING: return "connecting";
        case SocketState::CONNECTED: return "connected";
        case SocketState::SSL_HANDSHAKE: return "ssl_handshake";
        case SocketState::READY: return "ready";
        case SocketState::CLOSING: return "closing";
        case SocketState::ERROR_STATE: return "error";
        default: return "unknown";
    }
}

/**
 * Convert CloseReason to string
 */
inline const char* closeReasonToString(CloseReason reason) {
    switch (reason) {
        case CloseReason::NORMAL: return "normal";
        case CloseReason::TIMEOUT: return "timeout";
        case CloseReason::PROTOCOL_ERROR: return "protocol_error";
        case CloseReason::AUTH_FAILURE: return "auth_failure";
        case CloseReason::RESOURCE_LIMIT: return "resource_limit";
        case CloseReason::SERVER_SHUTDOWN: return "server_shutdown";
        case CloseReason::CLIENT_DISCONNECT: return "client_disconnect";
        case CloseReason::SSL_ERROR: return "ssl_error";
        case CloseReason::IO_ERROR: return "io_error";
        case CloseReason::INTERNAL_ERROR: return "internal_error";
        default: return "unknown";
    }
}

/**
 * Get default port for protocol
 */
inline uint16_t getDefaultPort(ProtocolType protocol) {
    switch (protocol) {
        case ProtocolType::NATIVE: return DEFAULT_NATIVE_PORT;
        case ProtocolType::POSTGRESQL: return DEFAULT_POSTGRESQL_PORT;
        case ProtocolType::MYSQL: return DEFAULT_MYSQL_PORT;
        case ProtocolType::FIREBIRD: return DEFAULT_FIREBIRD_PORT;
        default: return DEFAULT_NATIVE_PORT;
    }
}

/**
 * Get last socket error
 */
inline int getLastSocketError() {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

/**
 * Get error string for socket error
 */
std::string getSocketErrorString(int error_code);

} // namespace network
} // namespace scratchbird
