/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * Connection Handler
 *
 * ScratchBird Network Layer - Phase 3.1
 *
 * Manages client connection lifecycle and routes to protocol handlers.
 */

#pragma once

#include "scratchbird/network/socket_types.h"
#include "scratchbird/network/socket.h"
#include "scratchbird/network/event_loop.h"
#include "scratchbird/network/thread_pool.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"

#include <memory>
#include <functional>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <chrono>
#include <string>

namespace scratchbird {
namespace network {

// Forward declarations
class Connection;
class ConnectionManager;
class ProtocolHandler;

// ============================================================================
// Connection ID
// ============================================================================

using ConnectionId = uint64_t;
constexpr ConnectionId INVALID_CONNECTION_ID = 0;

// ============================================================================
// Connection State Machine
// ============================================================================

/**
 * Connection state
 */
enum class ConnectionState : uint8_t {
    NEW = 0,                // Just accepted, no data yet
    PROTOCOL_DETECTION = 1, // Detecting wire protocol
    SSL_HANDSHAKE = 2,      // SSL/TLS handshake
    AUTHENTICATING = 3,     // Client authentication
    AUTHENTICATED = 4,      // Authentication successful
    READY = 5,              // Ready to process queries
    PROCESSING = 6,         // Processing a query
    CLOSING = 7,            // Connection closing
    CLOSED = 8              // Connection closed
};

/**
 * Convert ConnectionState to string
 */
inline const char* connectionStateToString(ConnectionState state) {
    switch (state) {
        case ConnectionState::NEW: return "new";
        case ConnectionState::PROTOCOL_DETECTION: return "protocol_detection";
        case ConnectionState::SSL_HANDSHAKE: return "ssl_handshake";
        case ConnectionState::AUTHENTICATING: return "authenticating";
        case ConnectionState::AUTHENTICATED: return "authenticated";
        case ConnectionState::READY: return "ready";
        case ConnectionState::PROCESSING: return "processing";
        case ConnectionState::CLOSING: return "closing";
        case ConnectionState::CLOSED: return "closed";
        default: return "unknown";
    }
}

// ============================================================================
// Connection Events
// ============================================================================

/**
 * Connection event type
 */
enum class ConnectionEventType : uint8_t {
    CONNECTED = 0,          // New connection established
    DATA_RECEIVED = 1,      // Data available to read
    DATA_SENT = 2,          // Data sent successfully
    AUTHENTICATED = 3,      // Authentication completed
    QUERY_STARTED = 4,      // Query execution started
    QUERY_COMPLETED = 5,    // Query execution completed
    ERROR = 6,              // Error occurred
    TIMEOUT = 7,            // Timeout occurred
    CLOSING = 8,            // Connection closing
    DISCONNECTED = 9        // Connection disconnected
};

/**
 * Connection event data
 */
struct ConnectionEvent {
    ConnectionId connection_id;
    ConnectionEventType type;
    std::chrono::steady_clock::time_point timestamp;
    std::string message;
    int error_code = 0;

    ConnectionEvent(ConnectionId id, ConnectionEventType t, const std::string& msg = "")
        : connection_id(id), type(t),
          timestamp(std::chrono::steady_clock::now()), message(msg) {}
};

/**
 * Connection event callback
 */
using ConnectionEventCallback = std::function<void(const ConnectionEvent&)>;

// ============================================================================
// Connection
// ============================================================================

/**
 * Client connection
 *
 * Represents a single client connection with its state, protocol, and I/O buffers.
 */
class Connection {
public:
    /**
     * Create a new connection
     */
    Connection(std::unique_ptr<Socket> socket, ConnectionId id);
    ~Connection();

    // Non-copyable, movable
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&&) noexcept;
    Connection& operator=(Connection&&) noexcept;

    // ========================================================================
    // Identity
    // ========================================================================

    ConnectionId getId() const { return id_; }
    socket_t getFd() const { return socket_ ? socket_->getFd() : INVALID_SOCKET_VALUE; }

    // ========================================================================
    // State
    // ========================================================================

    ConnectionState getState() const { return state_.load(std::memory_order_acquire); }
    void setState(ConnectionState state) { state_.store(state, std::memory_order_release); }

    bool isOpen() const { return socket_ && socket_->isOpen(); }
    bool isReady() const { return getState() == ConnectionState::READY; }
    bool isClosing() const {
        auto s = getState();
        return s == ConnectionState::CLOSING || s == ConnectionState::CLOSED;
    }

    // ========================================================================
    // Protocol
    // ========================================================================

    ProtocolType getProtocol() const { return protocol_; }
    void setProtocol(ProtocolType protocol) { protocol_ = protocol; }

    // ========================================================================
    // I/O
    // ========================================================================

    /**
     * Read data from socket into internal buffer
     *
     * @return Number of bytes read, or -1 on error
     */
    ssize_t readIntoBuffer();

    /**
     * Write data from internal buffer to socket
     *
     * @return Number of bytes written, or -1 on error
     */
    ssize_t writeFromBuffer();

    /**
     * Get read buffer
     */
    const std::vector<uint8_t>& getReadBuffer() const { return read_buffer_; }
    std::vector<uint8_t>& getReadBuffer() { return read_buffer_; }

    /**
     * Get write buffer
     */
    const std::vector<uint8_t>& getWriteBuffer() const { return write_buffer_; }
    std::vector<uint8_t>& getWriteBuffer() { return write_buffer_; }

    /**
     * Consume bytes from read buffer
     */
    void consumeReadBuffer(size_t bytes);

    /**
     * Append data to write buffer
     */
    void appendToWriteBuffer(const void* data, size_t size);

    /**
     * Clear write buffer
     */
    void clearWriteBuffer() { write_buffer_.clear(); write_offset_ = 0; }

    /**
     * Has pending write data
     */
    bool hasPendingWrites() const { return write_offset_ < write_buffer_.size(); }

    /**
     * Get underlying socket
     */
    Socket* getSocket() { return socket_.get(); }
    const Socket* getSocket() const { return socket_.get(); }

    // ========================================================================
    // Authentication
    // ========================================================================

    const std::string& getUsername() const { return username_; }
    void setUsername(const std::string& username) { username_ = username; }

    const std::string& getDatabase() const { return database_; }
    void setDatabase(const std::string& database) { database_ = database; }

    const std::string& getApplicationName() const { return application_name_; }
    void setApplicationName(const std::string& name) { application_name_ = name; }

    // ========================================================================
    // Timeouts
    // ========================================================================

    void setReadTimeout(std::chrono::milliseconds timeout) { read_timeout_ = timeout; }
    void setWriteTimeout(std::chrono::milliseconds timeout) { write_timeout_ = timeout; }
    void setIdleTimeout(std::chrono::milliseconds timeout) { idle_timeout_ = timeout; }

    bool isReadTimedOut() const;
    bool isWriteTimedOut() const;
    bool isIdleTimedOut() const;

    void updateLastActivity() {
        last_activity_ = std::chrono::steady_clock::now();
    }

    // ========================================================================
    // Statistics
    // ========================================================================

    const NetworkStats& getStats() const { return stats_; }

    std::chrono::steady_clock::time_point getConnectedAt() const { return connected_at_; }

    std::string getRemoteAddress() const {
        auto addr = socket_ ? socket_->getRemoteAddress() : std::nullopt;
        return addr ? addr->toString() : "unknown";
    }

    // ========================================================================
    // Close
    // ========================================================================

    void close(CloseReason reason = CloseReason::NORMAL);

    CloseReason getCloseReason() const { return close_reason_; }

private:
    ConnectionId id_;
    std::unique_ptr<Socket> socket_;
    std::atomic<ConnectionState> state_{ConnectionState::NEW};
    ProtocolType protocol_ = ProtocolType::AUTO_DETECT;
    CloseReason close_reason_ = CloseReason::NORMAL;

    // I/O buffers
    std::vector<uint8_t> read_buffer_;
    std::vector<uint8_t> write_buffer_;
    size_t read_offset_ = 0;
    size_t write_offset_ = 0;

    // Authentication
    std::string username_;
    std::string database_;
    std::string application_name_;

    // Timeouts
    std::chrono::milliseconds read_timeout_{DEFAULT_READ_TIMEOUT_MS};
    std::chrono::milliseconds write_timeout_{DEFAULT_WRITE_TIMEOUT_MS};
    std::chrono::milliseconds idle_timeout_{300000}; // 5 minutes

    // Timestamps
    std::chrono::steady_clock::time_point connected_at_;
    std::chrono::steady_clock::time_point last_activity_;
    std::chrono::steady_clock::time_point last_read_;
    std::chrono::steady_clock::time_point last_write_;

    // Statistics
    NetworkStats stats_;
};

// ============================================================================
// Connection Manager Configuration
// ============================================================================

/**
 * Connection manager configuration
 */
struct ConnectionManagerConfig {
    uint32_t max_connections = DEFAULT_MAX_CONNECTIONS;
    std::chrono::milliseconds idle_timeout{300000};     // 5 minutes
    std::chrono::milliseconds auth_timeout{30000};      // 30 seconds
    std::chrono::milliseconds connect_timeout{30000};   // 30 seconds
    bool enable_keepalive = true;
    bool enable_ssl = false;
    SSLMode ssl_mode = SSLMode::PREFER;
    bool auto_detect_protocol = true;
    std::vector<ProtocolType> allowed_protocols{
        ProtocolType::NATIVE,
        ProtocolType::POSTGRESQL,
        ProtocolType::MYSQL,
        ProtocolType::FIREBIRD
    };
};

// ============================================================================
// Connection Manager
// ============================================================================

/**
 * Connection Manager
 *
 * Manages all client connections, handling connection lifecycle,
 * protocol detection, and routing to appropriate handlers.
 *
 * Thread safety: All methods are thread-safe.
 */
class ConnectionManager {
public:
    /**
     * Create connection manager
     */
    static std::unique_ptr<ConnectionManager> create(
        EventLoop* event_loop,
        ThreadPool* thread_pool,
        const ConnectionManagerConfig& config = ConnectionManagerConfig(),
        core::ErrorContext* ctx = nullptr);

    virtual ~ConnectionManager();

    // Non-copyable
    ConnectionManager(const ConnectionManager&) = delete;
    ConnectionManager& operator=(const ConnectionManager&) = delete;

    // ========================================================================
    // Connection Management
    // ========================================================================

    /**
     * Accept a new connection
     *
     * Takes ownership of the socket and creates a Connection.
     *
     * @param socket Client socket
     * @return Connection ID, or INVALID_CONNECTION_ID on failure
     */
    ConnectionId acceptConnection(std::unique_ptr<Socket> socket);

    /**
     * Get a connection by ID
     *
     * @return Pointer to connection, or nullptr if not found
     */
    Connection* getConnection(ConnectionId id);

    /**
     * Close a connection
     *
     * @param id Connection ID
     * @param reason Close reason
     */
    void closeConnection(ConnectionId id, CloseReason reason = CloseReason::NORMAL);

    /**
     * Close all connections
     */
    void closeAllConnections(CloseReason reason = CloseReason::SERVER_SHUTDOWN);

    /**
     * Get number of active connections
     */
    uint32_t getConnectionCount() const { return connection_count_.load(); }

    /**
     * Check if at connection limit
     */
    bool isAtLimit() const { return getConnectionCount() >= config_.max_connections; }

    // ========================================================================
    // Event Handling
    // ========================================================================

    /**
     * Handle connection event (called from event loop)
     */
    void handleEvent(ConnectionId id, EventType events);

    /**
     * Process pending I/O for all connections
     */
    void processPendingIO();

    /**
     * Check for and close timed-out connections
     */
    void checkTimeouts();

    // ========================================================================
    // Event Callbacks
    // ========================================================================

    /**
     * Set event callback
     */
    void setEventCallback(ConnectionEventCallback callback) {
        event_callback_ = std::move(callback);
    }

    // ========================================================================
    // Protocol Handlers
    // ========================================================================

    /**
     * Register a protocol handler
     */
    void registerProtocolHandler(ProtocolType protocol,
                                 std::shared_ptr<ProtocolHandler> handler);

    /**
     * Get protocol handler
     */
    std::shared_ptr<ProtocolHandler> getProtocolHandler(ProtocolType protocol);

    // ========================================================================
    // Statistics
    // ========================================================================

    const ServerStats& getStats() const { return stats_; }
    const ConnectionManagerConfig& getConfig() const { return config_; }

    /**
     * Get connection list (for monitoring)
     */
    std::vector<ConnectionId> getConnectionIds() const;

private:
    ConnectionManager(EventLoop* event_loop, ThreadPool* thread_pool,
                      const ConnectionManagerConfig& config);

    // Handle state transitions
    void handleNewConnection(Connection* conn);
    void handleProtocolDetection(Connection* conn);
    void handleAuthentication(Connection* conn);
    void handleReady(Connection* conn);
    void handleData(Connection* conn);

    // Detect wire protocol from first bytes
    ProtocolType detectProtocol(const std::vector<uint8_t>& data);

    // Fire event callback
    void fireEvent(const ConnectionEvent& event);

    // Generate unique connection ID
    ConnectionId generateConnectionId();

    EventLoop* event_loop_;
    ThreadPool* thread_pool_;
    ConnectionManagerConfig config_;
    ServerStats stats_;
    ConnectionEventCallback event_callback_;

    // Connections (protected by mutex)
    mutable std::mutex connections_mutex_;
    std::unordered_map<ConnectionId, std::unique_ptr<Connection>> connections_;
    std::atomic<uint32_t> connection_count_{0};
    std::atomic<ConnectionId> next_connection_id_{1};

    // Protocol handlers
    mutable std::mutex handlers_mutex_;
    std::unordered_map<ProtocolType, std::shared_ptr<ProtocolHandler>> protocol_handlers_;
};

// ============================================================================
// Protocol Handler Interface
// ============================================================================

/**
 * Protocol Handler Interface
 *
 * Base class for wire protocol handlers (PostgreSQL, MySQL, etc.)
 */
class ProtocolHandler {
public:
    virtual ~ProtocolHandler() = default;

    /**
     * Get protocol type
     */
    virtual ProtocolType getProtocolType() const = 0;

    /**
     * Handle incoming data
     *
     * @param conn Connection
     * @return Status::OK to continue, error to close connection
     */
    virtual core::Status handleData(Connection* conn) = 0;

    /**
     * Handle authentication
     *
     * @param conn Connection
     * @return Status::OK on success, error on failure
     */
    virtual core::Status handleAuthentication(Connection* conn) = 0;

    /**
     * Initialize connection for this protocol
     */
    virtual core::Status initializeConnection(Connection* conn) = 0;

    /**
     * Send error to client
     */
    virtual void sendError(Connection* conn, const std::string& message,
                           const std::string& code = "") = 0;

    /**
     * Send ready for query
     */
    virtual void sendReady(Connection* conn) = 0;
};

} // namespace network
} // namespace scratchbird
