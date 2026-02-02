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
 * ScratchBird Client Library
 *
 * Local Server Architecture - Phase 4
 *
 * This header defines the client API for connecting to a ScratchBird server.
 * The library handles:
 * - Connection establishment via IPC (Unix sockets, named pipes, TCP)
 * - Auto-start of server if not running
 * - Wire protocol communication
 * - Query execution and result set handling
 * - Transaction management
 *
 * Usage:
 *   scratchbird::client::Connection conn;
 *   conn.connect("mydb", "admin", "password");
 *
 *   ResultSet results;
 *   conn.executeQuery("SELECT * FROM users", &results);
 *   while (results.next()) {
 *       std::cout << results.getString(0) << std::endl;
 *   }
 *   conn.disconnect();
 */

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <istream>
#include <ostream>
#include <chrono>
#include <functional>

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/protocol/wire_protocol.h"

namespace scratchbird {
namespace client {

// Forward declarations
class ConnectionImpl;
class ResultSetImpl;
class PreparedStatementImpl;

// ============================================================================
// Client Configuration
// ============================================================================

/**
 * Connection configuration options
 */
struct ConnectionConfig {
    std::string database_name;              // Database name (required)
    std::string username;                   // Username for authentication
    std::string password;                   // Password for authentication

    // Connection settings
    uint32_t connect_timeout_ms = 5000;     // Connection timeout
    uint32_t query_timeout_ms = 30000;      // Query timeout
    uint32_t read_timeout_ms = 30000;       // Socket read timeout
    uint32_t write_timeout_ms = 30000;      // Socket write timeout
    uint32_t copy_window_bytes = 65536;     // COPY stream window
    uint32_t copy_chunk_bytes = 16384;      // COPY chunk size

    // Auto-start settings
    bool auto_start_server = true;          // Auto-start server if not running
    uint32_t auto_start_timeout_ms = 10000; // Timeout for server to start
    std::string server_executable;          // Path to sb_server (optional, uses PATH)

    // IPC settings
    server::IPCMethod ipc_method = server::IPCMethod::AUTO;
    uint16_t tcp_port = 3092;               // TCP port (if using TCP)
    std::string socket_path;                // Override socket/pipe path (optional)

    // Behavior settings
    bool auto_commit = true;                // Auto-commit mode (no explicit transaction)
    bool manual_auth = false;               // Caller handles AUTH_REQUEST/RESPONSE

    // Default constructor
    ConnectionConfig() = default;

    // Convenience constructor
    explicit ConnectionConfig(const std::string& db_name,
                             const std::string& user = "",
                             const std::string& pass = "")
        : database_name(db_name), username(user), password(pass) {}
};

// ============================================================================
// Result Set
// ============================================================================

/**
 * Column metadata
 */
struct ColumnMeta {
    std::string name;                       // Column name
    protocol::WireType type;                // Wire type
    uint32_t type_modifier;                 // Type-specific modifier
    size_t index;                           // Column index (0-based)
};

/**
 * ResultSet - Iterator for query results
 *
 * Provides row-by-row access to query results with typed value accessors.
 * The ResultSet does NOT own the data - it references data stored in the
 * Connection's result buffer.
 */
class ResultSet {
public:
    ResultSet();
    ~ResultSet();

    // Move-only
    ResultSet(ResultSet&& other) noexcept;
    ResultSet& operator=(ResultSet&& other) noexcept;
    ResultSet(const ResultSet&) = delete;
    ResultSet& operator=(const ResultSet&) = delete;

    // ============================
    // Metadata
    // ============================

    /**
     * Get number of columns in the result set
     */
    size_t getColumnCount() const;

    /**
     * Get column name by index
     *
     * @param index Column index (0-based)
     * @return Column name, or empty string if index out of range
     */
    std::string getColumnName(size_t index) const;

    /**
     * Get column index by name
     *
     * @param name Column name (case-insensitive)
     * @return Column index, or -1 if not found
     */
    int getColumnIndex(const std::string& name) const;

    /**
     * Get column type
     *
     * @param index Column index
     * @return Wire type of the column
     */
    protocol::WireType getColumnType(size_t index) const;

    /**
     * Get all column metadata
     */
    const std::vector<ColumnMeta>& getColumns() const;

    /**
     * Get total row count (if known)
     *
     * @return Row count, or -1 if unknown (streaming results)
     */
    int64_t getRowCount() const;

    /**
     * Get number of rows affected (for INSERT/UPDATE/DELETE)
     */
    int64_t getRowsAffected() const;

    /**
     * Check if result set is empty
     */
    bool isEmpty() const;

    /**
     * Get raw row values by index (adapter use)
     */
    const std::vector<protocol::ProtocolCodec::ColumnValue>& getRowValues(size_t index) const;

    /**
     * Get the command tag returned by the server (if any)
     */
    const std::string& getCommandTag() const;

    // ============================
    // Navigation
    // ============================

    /**
     * Move to the next row
     *
     * @return true if a row is available, false if no more rows
     */
    bool next();

    /**
     * Reset to before the first row
     */
    void reset();

    /**
     * Get current row number (0-based)
     *
     * @return Current row index, or -1 if before first row
     */
    int64_t getCurrentRow() const;

    // ============================
    // Value Accessors (by index)
    // ============================

    /**
     * Check if a column value is NULL
     *
     * @param column Column index
     * @return true if NULL
     */
    bool isNull(size_t column) const;

    /**
     * Get boolean value
     */
    bool getBool(size_t column) const;

    /**
     * Get 16-bit integer value
     */
    int16_t getInt16(size_t column) const;

    /**
     * Get 32-bit integer value
     */
    int32_t getInt32(size_t column) const;

    /**
     * Get 64-bit integer value
     */
    int64_t getInt64(size_t column) const;

    /**
     * Get float value
     */
    float getFloat(size_t column) const;

    /**
     * Get double value
     */
    double getDouble(size_t column) const;

    /**
     * Get string value
     *
     * Works for VARCHAR, TEXT, and converts numeric types to string.
     */
    std::string getString(size_t column) const;

    /**
     * Get binary data (BYTEA)
     */
    std::vector<uint8_t> getBytes(size_t column) const;

    /**
     * Get timestamp as microseconds since epoch
     */
    int64_t getTimestamp(size_t column) const;

    /**
     * Get date as days since 2000-01-01
     */
    int32_t getDate(size_t column) const;

    /**
     * Get time as microseconds since midnight
     */
    int64_t getTime(size_t column) const;

    /**
     * Get UUID as string (formatted: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx)
     */
    std::string getUUID(size_t column) const;

    /**
     * Get raw value data
     *
     * @param column Column index
     * @param length Output: length of data
     * @return Pointer to raw data, or nullptr if NULL
     */
    const uint8_t* getRaw(size_t column, size_t* length) const;

    // ============================
    // Value Accessors (by name)
    // ============================

    bool isNull(const std::string& column) const;
    bool getBool(const std::string& column) const;
    int16_t getInt16(const std::string& column) const;
    int32_t getInt32(const std::string& column) const;
    int64_t getInt64(const std::string& column) const;
    float getFloat(const std::string& column) const;
    double getDouble(const std::string& column) const;
    std::string getString(const std::string& column) const;
    std::vector<uint8_t> getBytes(const std::string& column) const;
    int64_t getTimestamp(const std::string& column) const;
    int32_t getDate(const std::string& column) const;
    int64_t getTime(const std::string& column) const;
    std::string getUUID(const std::string& column) const;

private:
    friend class Connection;
    friend class ConnectionImpl;

    std::unique_ptr<ResultSetImpl> impl_;
};

// ============================================================================
// Prepared Statement
// ============================================================================

/**
 * PreparedStatement - Precompiled SQL statement with parameter binding
 *
 * Allows efficient repeated execution of the same query with different parameters.
 */
class PreparedStatement {
public:
    PreparedStatement();
    ~PreparedStatement();

    // Move-only
    PreparedStatement(PreparedStatement&& other) noexcept;
    PreparedStatement& operator=(PreparedStatement&& other) noexcept;
    PreparedStatement(const PreparedStatement&) = delete;
    PreparedStatement& operator=(const PreparedStatement&) = delete;

    /**
     * Get the SQL text
     */
    const std::string& getSQL() const;

    /**
     * Get parameter count
     */
    size_t getParameterCount() const;

    /**
     * Check if statement is valid/prepared
     */
    bool isValid() const;

    // ============================
    // Parameter Binding
    // ============================

    /**
     * Bind NULL to a parameter
     *
     * @param index Parameter index (1-based, like JDBC/ODBC)
     */
    void setNull(size_t index);

    void setBool(size_t index, bool value);
    void setInt16(size_t index, int16_t value);
    void setInt32(size_t index, int32_t value);
    void setInt64(size_t index, int64_t value);
    void setFloat(size_t index, float value);
    void setDouble(size_t index, double value);
    void setString(size_t index, const std::string& value);
    void setBytes(size_t index, const std::vector<uint8_t>& value);
    void setBytes(size_t index, const uint8_t* data, size_t length);
    void setTimestamp(size_t index, int64_t microseconds);
    void setDate(size_t index, int32_t days);
    void setTime(size_t index, int64_t microseconds);
    void setUUID(size_t index, const std::vector<uint8_t>& value);
    void setUUID(size_t index, const std::string& value);
    void setNull(size_t index, protocol::WireType type);

    /**
     * Clear all bound parameters
     */
    void clearParameters();

private:
    friend class Connection;
    friend class ConnectionImpl;

    std::unique_ptr<PreparedStatementImpl> impl_;
};

// ============================================================================
// Connection
// ============================================================================

/**
 * Connection state
 */
enum class ConnectionState : uint8_t {
    DISCONNECTED = 0,
    CONNECTING = 1,
    CONNECTED = 2,
    IN_TRANSACTION = 3,
    ERROR_STATE = 4
};

/**
 * Connection - Client connection to ScratchBird server
 *
 * Main entry point for the client library. Handles connection lifecycle,
 * query execution, and transaction management.
 */
class Connection {
public:
    Connection();
    ~Connection();

    // Move-only
    Connection(Connection&& other) noexcept;
    Connection& operator=(Connection&& other) noexcept;
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    // ============================
    // Connection Lifecycle
    // ============================

    /**
     * Connect to a database (simple interface)
     *
     * @param database Database name
     * @param username Username (empty for trust authentication)
     * @param password Password
     * @param ctx Error context
     * @return Status::OK on success
     */
    core::Status connect(const std::string& database,
                        const std::string& username = "",
                        const std::string& password = "",
                        core::ErrorContext* ctx = nullptr);

    /**
     * Connect with full configuration
     *
     * @param config Connection configuration
     * @param ctx Error context
     * @return Status::OK on success
     */
    core::Status connect(const ConnectionConfig& config,
                        core::ErrorContext* ctx = nullptr);

    /**
     * Disconnect from server
     *
     * Rolls back any active transaction before disconnecting.
     */
    void disconnect();

    /**
     * Check if connected
     */
    bool isConnected() const;

    /**
     * Get connection state
     */
    ConnectionState getState() const;

    /**
     * Get last error message
     */
    std::string getLastError() const;

    /**
     * Ping the server
     *
     * @param ctx Error context
     * @return Status::OK if server is responsive
     */
    core::Status ping(core::ErrorContext* ctx = nullptr);

    void setCopyInputStream(std::istream* in);
    void setCopyOutputStream(std::ostream* out);

    struct AuthResponse {
        protocol::AuthStatus status = protocol::AuthStatus::ERROR;
        uint32_t user_id = 0;
        std::string error_message;
        std::vector<uint8_t> data;
    };

    /**
     * Send an authentication request (manual auth mode)
     */
    core::Status sendAuthRequest(protocol::AuthMethod method,
                                 const std::vector<uint8_t>& payload,
                                 AuthResponse& response,
                                 core::ErrorContext* ctx = nullptr);

    // ============================
    // Query Execution
    // ============================

    /**
     * Execute a query and get results
     *
     * @param sql SQL query
     * @param results Output result set
     * @param ctx Error context
     * @return Status::OK on success
     */
    core::Status executeQuery(const std::string& sql,
                              ResultSet* results,
                              core::ErrorContext* ctx = nullptr);

    /**
     * Execute a precompiled SBLR bytecode program
     *
     * @param bytecode SBLR bytecode payload
     * @param results Output result set
     * @param ctx Error context
     * @return Status::OK on success
     */
    core::Status executeBytecode(const std::vector<uint8_t>& bytecode,
                                 const std::string& sql,
                                 ResultSet* results,
                                 core::ErrorContext* ctx = nullptr);
    core::Status executeBytecode(const std::vector<uint8_t>& bytecode,
                                 ResultSet* results,
                                 core::ErrorContext* ctx = nullptr);

    /**
     * Execute a statement without results (INSERT/UPDATE/DELETE/DDL)
     *
     * @param sql SQL statement
     * @param rows_affected Output: number of rows affected (optional)
     * @param ctx Error context
     * @return Status::OK on success
     */
    core::Status execute(const std::string& sql,
                        int64_t* rows_affected = nullptr,
                        core::ErrorContext* ctx = nullptr);

    // ============================
    // Prepared Statements
    // ============================

    /**
     * Prepare a statement for repeated execution
     *
     * @param sql SQL with parameter placeholders ($1, $2, ... or ?)
     * @param stmt Output prepared statement
     * @param ctx Error context
     * @return Status::OK on success
     */
    core::Status prepare(const std::string& sql,
                        PreparedStatement* stmt,
                        core::ErrorContext* ctx = nullptr);

    /**
     * Execute a prepared statement and get results
     *
     * @param stmt Prepared statement with bound parameters
     * @param results Output result set
     * @param ctx Error context
     * @return Status::OK on success
     */
    core::Status executeQuery(PreparedStatement& stmt,
                              ResultSet* results,
                              core::ErrorContext* ctx = nullptr);

    /**
     * Execute a prepared statement without results
     *
     * @param stmt Prepared statement
     * @param rows_affected Output: rows affected
     * @param ctx Error context
     * @return Status::OK on success
     */
    core::Status execute(PreparedStatement& stmt,
                        int64_t* rows_affected = nullptr,
                        core::ErrorContext* ctx = nullptr);

    /**
     * Close a prepared statement
     *
     * @param stmt Statement to close
     */
    void closeStatement(PreparedStatement& stmt);

    // ============================
    // Transaction Management
    // ============================

    /**
     * Begin a transaction
     *
     * @param ctx Error context
     * @return Status::OK on success
     */
    core::Status beginTransaction(core::ErrorContext* ctx = nullptr);

    /**
     * Commit the current transaction
     *
     * @param ctx Error context
     * @return Status::OK on success
     */
    core::Status commit(core::ErrorContext* ctx = nullptr);

    /**
     * Rollback the current transaction
     *
     * @param ctx Error context
     * @return Status::OK on success
     */
    core::Status rollback(core::ErrorContext* ctx = nullptr);

    /**
     * Create a savepoint
     *
     * @param name Savepoint name
     * @param ctx Error context
     * @return Status::OK on success
     */
    core::Status savepoint(const std::string& name,
                          core::ErrorContext* ctx = nullptr);

    /**
     * Release a savepoint
     *
     * @param name Savepoint name
     * @param ctx Error context
     * @return Status::OK on success
     */
    core::Status releaseSavepoint(const std::string& name,
                                  core::ErrorContext* ctx = nullptr);

    /**
     * Rollback to a savepoint
     *
     * @param name Savepoint name
     * @param ctx Error context
     * @return Status::OK on success
     */
    core::Status rollbackTo(const std::string& name,
                           core::ErrorContext* ctx = nullptr);

    /**
     * Check if in a transaction
     */
    bool inTransaction() const;

    /**
     * Set auto-commit mode
     *
     * @param enabled If true, each statement auto-commits
     */
    void setAutoCommit(bool enabled);

    /**
     * Get auto-commit mode
     */
    bool getAutoCommit() const;

    // ============================
    // Connection Information
    // ============================

    /**
     * Get database name
     */
    std::string getDatabaseName() const;

    /**
     * Get current username
     */
    std::string getUsername() const;

    /**
     * Get server version string
     */
    std::string getServerVersion() const;

    /**
     * Get connection configuration
     */
    const ConnectionConfig& getConfig() const;

    // ============================
    // Server Control (Static)
    // ============================

    /**
     * Start the server for a database
     *
     * @param database_name Database name
     * @param server_path Path to sb_server (optional, uses PATH)
     * @param timeout_ms Timeout waiting for server to start
     * @param ctx Error context
     * @return Status::OK on success
     */
    static core::Status startServer(const std::string& database_name,
                                    const std::string& server_path = "",
                                    uint32_t timeout_ms = 10000,
                                    core::ErrorContext* ctx = nullptr);

    /**
     * Stop the server for a database
     *
     * @param database_name Database name
     * @param ctx Error context
     * @return Status::OK on success
     */
    static core::Status stopServer(const std::string& database_name,
                                   core::ErrorContext* ctx = nullptr);

    /**
     * Check if server is running
     *
     * @param database_name Database name
     * @return true if server is running
     */
    static bool isServerRunning(const std::string& database_name);

    /**
     * Get server PID (if running)
     *
     * @param database_name Database name
     * @return PID or 0 if not running
     */
    static int32_t getServerPID(const std::string& database_name);

private:
    std::unique_ptr<ConnectionImpl> impl_;
};

// ============================================================================
// Connection Pool (Optional)
// ============================================================================

/**
 * Connection pool configuration
 */
struct PoolConfig {
    ConnectionConfig connection_config;     // Base connection settings
    uint32_t min_connections = 1;          // Minimum pool size
    uint32_t max_connections = 10;         // Maximum pool size
    uint32_t idle_timeout_ms = 300000;     // Idle connection timeout (5 min)
    uint32_t acquire_timeout_ms = 5000;    // Timeout to acquire connection
    bool validate_on_acquire = true;       // Ping connection before returning
};

/**
 * PooledConnection - RAII wrapper for pooled connections
 *
 * Automatically returns connection to pool when destroyed.
 */
class PooledConnection {
public:
    ~PooledConnection();

    // Move-only
    PooledConnection(PooledConnection&& other) noexcept;
    PooledConnection& operator=(PooledConnection&& other) noexcept;
    PooledConnection(const PooledConnection&) = delete;
    PooledConnection& operator=(const PooledConnection&) = delete;

    /**
     * Access the underlying connection
     */
    Connection* operator->() { return connection_; }
    Connection& operator*() { return *connection_; }
    const Connection* operator->() const { return connection_; }
    const Connection& operator*() const { return *connection_; }

    /**
     * Get raw connection pointer
     */
    Connection* get() { return connection_; }
    const Connection* get() const { return connection_; }

    /**
     * Check if connection is valid
     */
    explicit operator bool() const { return connection_ != nullptr; }

private:
    friend class ConnectionPool;

    PooledConnection(Connection* conn, std::function<void(Connection*)> returner);

    Connection* connection_;
    std::function<void(Connection*)> returner_;
};

/**
 * ConnectionPool - Pool of reusable connections
 *
 * Manages a pool of connections for efficient reuse in multi-threaded applications.
 */
class ConnectionPool {
public:
    explicit ConnectionPool(const PoolConfig& config);
    ~ConnectionPool();

    // Non-copyable, non-movable
    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;
    ConnectionPool(ConnectionPool&&) = delete;
    ConnectionPool& operator=(ConnectionPool&&) = delete;

    /**
     * Acquire a connection from the pool
     *
     * @param ctx Error context
     * @return Pooled connection (automatically returned when destroyed)
     */
    PooledConnection acquire(core::ErrorContext* ctx = nullptr);

    /**
     * Get current pool statistics
     */
    struct Stats {
        uint32_t total_connections;         // Total connections in pool
        uint32_t available_connections;     // Connections available for use
        uint32_t in_use_connections;        // Connections currently in use
        uint64_t total_acquisitions;        // Total acquire() calls
        uint64_t total_releases;            // Total connection returns
        uint64_t wait_count;                // Times acquire() had to wait
    };

    Stats getStats() const;

    /**
     * Close all connections and shutdown pool
     */
    void shutdown();

private:
    class PoolImpl;
    std::unique_ptr<PoolImpl> impl_;
};

} // namespace client
} // namespace scratchbird
