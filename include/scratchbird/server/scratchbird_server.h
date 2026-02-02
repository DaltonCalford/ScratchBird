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
 * ScratchBird Server
 *
 * Local Server Architecture - Phase 3
 *
 * This header defines the main server class that manages:
 * - Database lifecycle
 * - IPC listener
 * - Client connections
 * - Signal handling
 * - Graceful shutdown
 */

#include <cstdint>
#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <vector>
#include <functional>

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/server/ipc_server.h"
#include "scratchbird/server/server_session.h"

namespace scratchbird {
namespace server {

// ============================================================================
// Server Configuration
// ============================================================================

/**
 * Server configuration options
 */
struct ServerConfig {
    // Database
    std::string database_path;                  // Path to database file

    // IPC Settings
    IPCMethod ipc_method = IPCMethod::AUTO;     // IPC method (auto-detect by default)
    std::string ipc_path;                       // Custom IPC path (optional)
    uint16_t tcp_port = 5433;                   // TCP port (if using TCP)

    // Connection limits
    uint32_t max_connections = 100;             // Maximum concurrent connections
    uint32_t connection_timeout_ms = 30000;     // Connection timeout (30s)
    uint32_t accept_timeout_ms = 1000;          // Accept timeout (1s)

    // Server behavior
    bool auto_create_db = false;                // Create database if not exists
    uint32_t page_size = 16384;                 // Page size for new database

    // PID file
    std::string pid_file;                       // PID file path (optional)

    // Logging
    bool verbose = false;                       // Verbose logging
};

// ============================================================================
// Server State
// ============================================================================

/**
 * Server state enumeration
 */
enum class ServerState : uint8_t {
    CREATED = 0,        // Server created, not started
    STARTING,           // Server is starting up
    RUNNING,            // Server is accepting connections
    STOPPING,           // Server is shutting down
    STOPPED             // Server has stopped
};

/**
 * Server statistics
 */
struct ServerStats {
    uint64_t total_connections = 0;         // Total connections accepted
    uint64_t active_connections = 0;        // Currently active connections
    uint64_t total_queries = 0;             // Total queries executed
    uint64_t failed_queries = 0;            // Failed queries
    std::chrono::steady_clock::time_point started_at;
    std::chrono::steady_clock::time_point last_connection;
};

// ============================================================================
// ScratchBird Server
// ============================================================================

/**
 * ScratchBirdServer - Main server class
 *
 * The server manages:
 * - Database opening/creation
 * - IPC listener
 * - Client session handling
 * - Graceful shutdown
 */
class ScratchBirdServer {
public:
    /**
     * Create a new server with configuration
     */
    explicit ScratchBirdServer(const ServerConfig& config);

    ~ScratchBirdServer();

    // Disable copy/move
    ScratchBirdServer(const ScratchBirdServer&) = delete;
    ScratchBirdServer& operator=(const ScratchBirdServer&) = delete;

    // ========================================================================
    // Server Lifecycle
    // ========================================================================

    /**
     * Start the server
     *
     * Opens the database, starts the IPC listener, and begins accepting
     * connections. This method blocks until shutdown() is called.
     *
     * @param ctx Error context
     * @return Status::OK on clean shutdown, error status otherwise
     */
    core::Status start(core::ErrorContext* ctx = nullptr);

    /**
     * Start the server in background
     *
     * Same as start() but runs in a background thread.
     *
     * @param ctx Error context
     * @return Status::OK if started successfully
     */
    core::Status startAsync(core::ErrorContext* ctx = nullptr);

    /**
     * Request server shutdown
     *
     * Signals the server to stop accepting new connections and
     * gracefully close existing ones.
     */
    void shutdown();

    /**
     * Wait for server to stop
     *
     * Blocks until the server has completely stopped.
     *
     * @param timeout_ms Maximum time to wait (0 = forever)
     * @return true if server stopped, false on timeout
     */
    bool waitForShutdown(uint32_t timeout_ms = 0);

    // ========================================================================
    // Server Information
    // ========================================================================

    /**
     * Get server state
     */
    ServerState state() const { return state_; }

    /**
     * Check if server is running
     */
    bool isRunning() const { return state_ == ServerState::RUNNING; }

    /**
     * Get server statistics
     */
    ServerStats getStats() const;

    /**
     * Get configuration
     */
    const ServerConfig& config() const { return config_; }

    /**
     * Get IPC path being used
     */
    std::string getIPCPath() const;

    /**
     * Get database instance
     */
    core::Database* database() { return database_.get(); }

    /**
     * Get session manager
     */
    SessionManager* sessionManager() { return &session_manager_; }

    // ========================================================================
    // Static Utilities
    // ========================================================================

    /**
     * Check if a server is already running for a database
     *
     * @param database_path Path to the database file
     * @return true if a server is running
     */
    static bool isServerRunning(const std::string& database_path);

    /**
     * Get the PID of a running server
     *
     * @param database_path Path to the database file
     * @return PID if running, 0 otherwise
     */
    static pid_t getServerPID(const std::string& database_path);

private:
    // ========================================================================
    // Internal Methods
    // ========================================================================

    /**
     * Open or create the database
     */
    core::Status openDatabase(core::ErrorContext* ctx);

    /**
     * Start the IPC listener
     */
    core::Status startListener(core::ErrorContext* ctx);

    /**
     * Accept loop - accepts new connections
     */
    void acceptLoop();

    /**
     * Handle a single client connection
     */
    void handleClient(std::unique_ptr<IPCConnection> connection);

    /**
     * Write PID file
     */
    core::Status writePIDFile(core::ErrorContext* ctx);

    /**
     * Remove PID file
     */
    void removePIDFile();

    /**
     * Log message (if verbose)
     */
    void log(const std::string& message);

    // ========================================================================
    // Internal State
    // ========================================================================

    ServerConfig config_;                           // Configuration
    std::atomic<ServerState> state_;                // Current state
    std::atomic<bool> shutdown_requested_;          // Shutdown flag

    std::unique_ptr<core::Database> database_;      // Database instance
    std::unique_ptr<IPCServer> listener_;           // IPC listener
    SessionManager session_manager_;                // Session manager

    std::thread accept_thread_;                     // Accept loop thread
    std::vector<std::thread> client_threads_;       // Client handler threads

    ServerStats stats_;                             // Statistics
    mutable std::mutex stats_mutex_;                // Stats mutex

    std::mutex client_threads_mutex_;               // Mutex for client_threads_
};

// ============================================================================
// PID File Management
// ============================================================================

/**
 * Get the default PID file path for a database
 */
std::string getDefaultPIDPath(const std::string& database_path);

/**
 * Read PID from file
 *
 * @param pid_path Path to PID file
 * @return PID if file exists and is valid, 0 otherwise
 */
pid_t readPIDFile(const std::string& pid_path);

/**
 * Check if a process is running
 *
 * @param pid Process ID to check
 * @return true if process exists
 */
bool isProcessRunning(pid_t pid);

// ============================================================================
// Server State Utilities
// ============================================================================

/**
 * Convert server state to string
 */
const char* serverStateToString(ServerState state);

}  // namespace server
}  // namespace scratchbird
