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
 * IPC Server Abstraction Layer
 *
 * ScratchBird Local Server Architecture - Phase 1
 *
 * This header defines the platform-agnostic IPC (Inter-Process Communication)
 * server interface for ScratchBird's local client-server architecture.
 *
 * Supported IPC Methods:
 * - Unix Domain Sockets (Linux/macOS) - Default on Unix-like systems
 * - Named Pipes (Windows) - Default on Windows
 * - TCP Localhost (All platforms) - Fallback/debug option
 *
 * Usage:
 *   auto server = IPCServer::create(IPCMethod::AUTO, "mydb");
 *   server->listen();
 *   while (running) {
 *       auto conn = server->accept();
 *       // Handle connection...
 *   }
 *   server->close();
 */

#include <cstdint>
#include <memory>
#include <string>
#include <functional>
#include <vector>
#include <chrono>

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"

namespace scratchbird {
namespace server {

// Forward declarations
class IPCConnection;

/**
 * IPC method selection
 */
enum class IPCMethod : uint8_t {
    AUTO = 0,           // Platform-specific default (recommended)
    UNIX_SOCKET = 1,    // Unix domain socket (Linux/macOS)
    NAMED_PIPE = 2,     // Named pipe (Windows)
    TCP_LOCALHOST = 3   // TCP localhost (all platforms, debugging)
};

/**
 * Connection state
 */
enum class ConnectionState : uint8_t {
    DISCONNECTED = 0,
    CONNECTING = 1,
    CONNECTED = 2,
    AUTHENTICATED = 3,
    CLOSING = 4,
    ERROR_STATE = 5
};

/**
 * Server configuration
 */
struct IPCServerConfig {
    IPCMethod method = IPCMethod::AUTO;      // IPC method to use
    std::string database_name;                // Database name (used in socket/pipe path)
    uint16_t tcp_port = 5433;                // TCP port (if using TCP)
    uint32_t max_connections = 100;          // Maximum concurrent connections
    uint32_t accept_timeout_ms = 1000;       // Accept timeout in milliseconds (0 = blocking)
    uint32_t read_timeout_ms = 30000;        // Read timeout in milliseconds
    uint32_t write_timeout_ms = 30000;       // Write timeout in milliseconds
    bool require_authentication = true;       // Require client authentication
    std::string socket_path;                  // Override socket/pipe path (optional)

    // Default constructor
    IPCServerConfig() = default;

    // Convenience constructor
    explicit IPCServerConfig(const std::string& db_name, IPCMethod m = IPCMethod::AUTO)
        : method(m), database_name(db_name) {}
};

/**
 * Client configuration (for IPCClient)
 */
struct IPCClientConfig {
    IPCMethod method = IPCMethod::AUTO;      // IPC method to use
    std::string database_name;                // Database name
    uint16_t tcp_port = 5433;                // TCP port (if using TCP)
    uint32_t connect_timeout_ms = 5000;      // Connection timeout in milliseconds
    uint32_t read_timeout_ms = 30000;        // Read timeout in milliseconds
    uint32_t write_timeout_ms = 30000;       // Write timeout in milliseconds
    std::string socket_path;                  // Override socket/pipe path (optional)

    // Default constructor
    IPCClientConfig() = default;

    // Convenience constructor
    explicit IPCClientConfig(const std::string& db_name, IPCMethod m = IPCMethod::AUTO)
        : method(m), database_name(db_name) {}
};

/**
 * Peer credentials (for Unix sockets)
 */
struct PeerCredentials {
    uint32_t uid = 0;          // User ID
    uint32_t gid = 0;          // Group ID
    uint32_t pid = 0;          // Process ID
    bool available = false;    // True if credentials were retrieved
};

/**
 * Connection statistics
 */
struct ConnectionStats {
    uint64_t bytes_sent = 0;
    uint64_t bytes_received = 0;
    uint64_t messages_sent = 0;
    uint64_t messages_received = 0;
    std::chrono::steady_clock::time_point connected_at;
    std::chrono::steady_clock::time_point last_activity;
};

/**
 * Abstract base class for IPC connections
 *
 * Represents a single client connection to the server.
 * Implementations exist for Unix sockets, named pipes, and TCP.
 */
class IPCConnection {
public:
    virtual ~IPCConnection() = default;

    // Delete copy/move (connections are not copyable)
    IPCConnection(const IPCConnection&) = delete;
    IPCConnection& operator=(const IPCConnection&) = delete;
    IPCConnection(IPCConnection&&) = delete;
    IPCConnection& operator=(IPCConnection&&) = delete;

    /**
     * Read data from the connection
     *
     * @param buffer Buffer to read into
     * @param size Maximum bytes to read
     * @param bytes_read Output: actual bytes read
     * @param ctx Error context
     * @return Status::OK on success, Status::CONNECTION_FAILURE on disconnect
     */
    virtual core::Status read(void* buffer, size_t size, size_t* bytes_read,
                              core::ErrorContext* ctx = nullptr) = 0;

    /**
     * Read exactly 'size' bytes from the connection
     *
     * @param buffer Buffer to read into
     * @param size Exact bytes to read
     * @param ctx Error context
     * @return Status::OK on success, error on failure or partial read
     */
    virtual core::Status readExact(void* buffer, size_t size,
                                   core::ErrorContext* ctx = nullptr) = 0;

    /**
     * Write data to the connection
     *
     * @param buffer Buffer to write from
     * @param size Bytes to write
     * @param bytes_written Output: actual bytes written
     * @param ctx Error context
     * @return Status::OK on success
     */
    virtual core::Status write(const void* buffer, size_t size, size_t* bytes_written,
                               core::ErrorContext* ctx = nullptr) = 0;

    /**
     * Write exactly 'size' bytes to the connection
     *
     * @param buffer Buffer to write from
     * @param size Exact bytes to write
     * @param ctx Error context
     * @return Status::OK on success, error on failure or partial write
     */
    virtual core::Status writeExact(const void* buffer, size_t size,
                                    core::ErrorContext* ctx = nullptr) = 0;

    /**
     * Close the connection
     */
    virtual void close() = 0;

    /**
     * Check if connection is open
     */
    virtual bool isOpen() const = 0;

    /**
     * Get connection state
     */
    virtual ConnectionState getState() const = 0;

    /**
     * Set connection state (called by server/protocol handlers)
     */
    virtual void setState(ConnectionState state) = 0;

    /**
     * Get peer credentials (Unix sockets only)
     *
     * @return PeerCredentials with available=true if supported, false otherwise
     */
    virtual PeerCredentials getPeerCredentials() const = 0;

    /**
     * Get remote address/identifier string
     *
     * @return Human-readable connection identifier (e.g., "unix:pid=1234" or "tcp:127.0.0.1:54321")
     */
    virtual std::string getRemoteAddress() const = 0;

    /**
     * Get IPC method used by this connection
     */
    virtual IPCMethod getMethod() const = 0;

    /**
     * Get connection statistics
     */
    virtual ConnectionStats getStats() const = 0;

    /**
     * Set read timeout
     *
     * @param timeout_ms Timeout in milliseconds (0 = blocking)
     */
    virtual void setReadTimeout(uint32_t timeout_ms) = 0;

    /**
     * Set write timeout
     *
     * @param timeout_ms Timeout in milliseconds (0 = blocking)
     */
    virtual void setWriteTimeout(uint32_t timeout_ms) = 0;

protected:
    IPCConnection() = default;
};

/**
 * Abstract base class for IPC servers
 *
 * Factory method IPCServer::create() returns the appropriate implementation
 * based on the requested IPC method and platform.
 */
class IPCServer {
public:
    virtual ~IPCServer() = default;

    // Delete copy/move
    IPCServer(const IPCServer&) = delete;
    IPCServer& operator=(const IPCServer&) = delete;
    IPCServer(IPCServer&&) = delete;
    IPCServer& operator=(IPCServer&&) = delete;

    /**
     * Create an IPC server instance
     *
     * Factory method that returns the appropriate implementation based on
     * the configuration. For IPCMethod::AUTO, selects the best method for
     * the current platform.
     *
     * @param config Server configuration
     * @param ctx Error context
     * @return Unique pointer to server instance, or nullptr on error
     */
    static std::unique_ptr<IPCServer> create(const IPCServerConfig& config,
                                             core::ErrorContext* ctx = nullptr);

    /**
     * Start listening for connections
     *
     * Creates the socket/pipe and starts listening. Must be called before accept().
     *
     * @param ctx Error context
     * @return Status::OK on success
     */
    virtual core::Status listen(core::ErrorContext* ctx = nullptr) = 0;

    /**
     * Accept a new client connection
     *
     * Blocks until a connection is available (or timeout expires).
     *
     * @param ctx Error context
     * @return Unique pointer to connection, or nullptr on timeout/error
     */
    virtual std::unique_ptr<IPCConnection> accept(core::ErrorContext* ctx = nullptr) = 0;

    /**
     * Close the server
     *
     * Stops accepting connections and cleans up resources (socket file, etc.).
     */
    virtual void close() = 0;

    /**
     * Check if server is listening
     */
    virtual bool isListening() const = 0;

    /**
     * Get the actual IPC method being used
     *
     * @return The resolved IPC method (never AUTO after listen() is called)
     */
    virtual IPCMethod getMethod() const = 0;

    /**
     * Get the socket/pipe path or TCP address string
     *
     * @return Connection address (e.g., "build/ipc/scratchbird-mydb.sock" or "127.0.0.1:5433")
     */
    virtual std::string getAddress() const = 0;

    /**
     * Get current number of connected clients
     */
    virtual uint32_t getConnectionCount() const = 0;

    /**
     * Get server configuration
     */
    virtual const IPCServerConfig& getConfig() const = 0;

protected:
    IPCServer() = default;
};

/**
 * Abstract base class for IPC clients
 *
 * Factory method IPCClient::create() returns the appropriate implementation.
 */
class IPCClient {
public:
    virtual ~IPCClient() = default;

    // Delete copy/move
    IPCClient(const IPCClient&) = delete;
    IPCClient& operator=(const IPCClient&) = delete;
    IPCClient(IPCClient&&) = delete;
    IPCClient& operator=(IPCClient&&) = delete;

    /**
     * Create an IPC client instance
     *
     * @param config Client configuration
     * @param ctx Error context
     * @return Unique pointer to client instance, or nullptr on error
     */
    static std::unique_ptr<IPCClient> create(const IPCClientConfig& config,
                                             core::ErrorContext* ctx = nullptr);

    /**
     * Connect to the server
     *
     * @param ctx Error context
     * @return Status::OK on success, Status::CONNECTION_FAILURE on failure
     */
    virtual core::Status connect(core::ErrorContext* ctx = nullptr) = 0;

    /**
     * Get the underlying connection
     *
     * @return Pointer to connection (owned by client), or nullptr if not connected
     */
    virtual IPCConnection* getConnection() = 0;

    /**
     * Check if connected
     */
    virtual bool isConnected() const = 0;

    /**
     * Disconnect from server
     */
    virtual void disconnect() = 0;

    /**
     * Get client configuration
     */
    virtual const IPCClientConfig& getConfig() const = 0;

protected:
    IPCClient() = default;
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Get the default IPC method for the current platform
 *
 * @return IPCMethod::UNIX_SOCKET on Linux/macOS, IPCMethod::NAMED_PIPE on Windows
 */
IPCMethod getDefaultIPCMethod();

/**
 * Get the socket/pipe path for a database name
 *
 * @param database_name Database name
 * @param method IPC method (AUTO uses platform default)
 * @return Socket path (e.g., "build/ipc/scratchbird-mydb.sock")
 */
std::string getIPCPath(const std::string& database_name, IPCMethod method = IPCMethod::AUTO);

/**
 * Check if a server is already running for the given database
 *
 * Checks for the existence of a PID file and verifies the process is alive.
 *
 * @param database_name Database name
 * @return true if server is running, false otherwise
 */
bool isServerRunning(const std::string& database_name);

/**
 * Get the PID file path for a database
 *
 * @param database_name Database name
 * @return PID file path (e.g., "build/run/scratchbird-mydb.pid")
 */
std::string getPIDFilePath(const std::string& database_name);

/**
 * Convert IPCMethod to string
 */
const char* ipcMethodToString(IPCMethod method);

/**
 * Convert string to IPCMethod
 *
 * @param str Method name (case-insensitive): "auto", "unix", "pipe", "tcp"
 * @return Corresponding IPCMethod, or IPCMethod::AUTO if not recognized
 */
IPCMethod stringToIPCMethod(const std::string& str);

} // namespace server
} // namespace scratchbird
