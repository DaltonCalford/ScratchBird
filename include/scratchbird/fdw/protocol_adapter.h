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
 * @file protocol_adapter.h
 * @brief Protocol Adapter Interface for Foreign Data Wrappers
 *
 * Defines the IProtocolAdapter interface that all database-specific
 * protocol adapters must implement to connect to remote databases.
 *
 * Part of Phase 3.7: UDR Plugin System
 */

#ifndef SCRATCHBIRD_FDW_PROTOCOL_ADAPTER_H
#define SCRATCHBIRD_FDW_PROTOCOL_ADAPTER_H

#include "scratchbird/fdw/fdw_types.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace scratchbird {
namespace fdw {

// Forward declarations
class IProtocolAdapter;

/**
 * @brief Result type wrapping a value or error
 */
template<typename T>
class Result {
public:
    Result() : has_value_(false), error_code_(core::Status::OK), error_message_() {}

    explicit Result(T value) : has_value_(true), value_(std::move(value)), error_code_(core::Status::OK) {}

    Result(core::Status code, std::string message)
        : has_value_(false), error_code_(code), error_message_(std::move(message)) {}

    bool ok() const { return has_value_; }
    bool hasError() const { return !has_value_; }
    explicit operator bool() const { return has_value_; }

    T& value() { return value_; }
    const T& value() const { return value_; }
    T* operator->() { return &value_; }
    const T* operator->() const { return &value_; }
    T& operator*() { return value_; }
    const T& operator*() const { return value_; }

    core::Status errorCode() const { return error_code_; }
    const std::string& errorMessage() const { return error_message_; }

    struct Error {
        core::Status code;
        std::string message;
    };

    Error error() const { return {error_code_, error_message_}; }

private:
    bool has_value_;
    T value_;
    core::Status error_code_;
    std::string error_message_;
};

/**
 * @brief Specialization for void results
 */
template<>
class Result<void> {
public:
    Result() : has_error_(false), error_code_(core::Status::OK) {}

    Result(core::Status code, std::string message)
        : has_error_(true), error_code_(code), error_message_(std::move(message)) {}

    bool ok() const { return !has_error_; }
    bool hasError() const { return has_error_; }
    explicit operator bool() const { return !has_error_; }

    core::Status errorCode() const { return error_code_; }
    const std::string& errorMessage() const { return error_message_; }

    struct Error {
        core::Status code;
        std::string message;
    };

    Error error() const { return {error_code_, error_message_}; }

private:
    bool has_error_;
    core::Status error_code_;
    std::string error_message_;
};

/**
 * @brief Factory function for creating error results
 */
template<typename T>
Result<T> makeError(core::Status code, std::string message) {
    return Result<T>(code, std::move(message));
}

inline Result<void> makeError(core::Status code, std::string message) {
    return Result<void>(code, std::move(message));
}

/**
 * @brief Abstract interface for database-specific protocol adapters
 *
 * Each supported database type (PostgreSQL, MySQL, etc.) implements this
 * interface to provide protocol-specific communication.
 */
class IProtocolAdapter {
public:
    virtual ~IProtocolAdapter() = default;

    // =========================================================================
    // Connection Lifecycle
    // =========================================================================

    /**
     * @brief Establish connection to remote database
     * @param server Server definition with connection parameters
     * @param mapping User credentials for authentication
     * @return Success or error
     */
    virtual Result<void> connect(const ServerDefinition& server,
                                  const UserMapping& mapping) = 0;

    /**
     * @brief Gracefully disconnect from remote database
     * @return Success or error
     */
    virtual Result<void> disconnect() = 0;

    /**
     * @brief Get current connection state
     */
    virtual ConnectionState getState() const = 0;

    /**
     * @brief Check if connected to remote database
     */
    virtual bool isConnected() const = 0;

    // =========================================================================
    // Health Check
    // =========================================================================

    /**
     * @brief Simple connection health check
     * @return true if connection is healthy
     */
    virtual Result<bool> ping() = 0;

    /**
     * @brief Reset connection state (rollback transaction, reset parameters)
     * @return Success or error
     */
    virtual Result<void> reset() = 0;

    // =========================================================================
    // Query Execution
    // =========================================================================

    /**
     * @brief Execute a SQL query
     * @param sql SQL statement to execute
     * @return Query result or error
     */
    virtual Result<RemoteQueryResult> executeQuery(const std::string& sql) = 0;

    /**
     * @brief Execute a parameterized SQL query
     * @param sql SQL statement with $1, $2, ... placeholders
     * @param params Parameter values
     * @return Query result or error
     */
    virtual Result<RemoteQueryResult> executeQueryWithParams(
        const std::string& sql,
        const std::vector<RemoteValue>& params) = 0;

    // =========================================================================
    // Prepared Statements
    // =========================================================================

    /**
     * @brief Prepare a SQL statement for repeated execution
     * @param sql SQL statement to prepare
     * @return Statement handle or error
     */
    virtual Result<uint64_t> prepare(const std::string& sql) = 0;

    /**
     * @brief Execute a prepared statement
     * @param stmt_id Statement handle from prepare()
     * @param params Parameter values
     * @return Query result or error
     */
    virtual Result<RemoteQueryResult> executePrepared(
        uint64_t stmt_id,
        const std::vector<RemoteValue>& params) = 0;

    /**
     * @brief Release a prepared statement
     * @param stmt_id Statement handle
     * @return Success or error
     */
    virtual Result<void> deallocatePrepared(uint64_t stmt_id) = 0;

    // =========================================================================
    // Transaction Control
    // =========================================================================

    /**
     * @brief Begin a transaction
     */
    virtual Result<void> beginTransaction() = 0;

    /**
     * @brief Commit current transaction
     */
    virtual Result<void> commit() = 0;

    /**
     * @brief Rollback current transaction
     */
    virtual Result<void> rollback() = 0;

    /**
     * @brief Create a savepoint
     * @param name Savepoint name
     */
    virtual Result<void> setSavepoint(const std::string& name) = 0;

    /**
     * @brief Rollback to a savepoint
     * @param name Savepoint name
     */
    virtual Result<void> rollbackToSavepoint(const std::string& name) = 0;

    // =========================================================================
    // Cursor Operations (for large result sets)
    // =========================================================================

    /**
     * @brief Declare a server-side cursor
     * @param name Cursor name
     * @param sql Query for the cursor
     * @return Cursor identifier or error
     */
    virtual Result<std::string> declareCursor(const std::string& name,
                                               const std::string& sql) = 0;

    /**
     * @brief Fetch rows from a cursor
     * @param name Cursor name
     * @param count Number of rows to fetch
     * @return Fetched rows or error
     */
    virtual Result<RemoteQueryResult> fetchFromCursor(const std::string& name,
                                                       uint32_t count) = 0;

    /**
     * @brief Close a cursor
     * @param name Cursor name
     */
    virtual Result<void> closeCursor(const std::string& name) = 0;

    // =========================================================================
    // Schema Introspection
    // =========================================================================

    /**
     * @brief List schemas in the remote database
     * @return List of schema names
     */
    virtual Result<std::vector<std::string>> listSchemas() = 0;

    /**
     * @brief List tables in a schema
     * @param schema Schema name (empty for default)
     * @return List of table names
     */
    virtual Result<std::vector<std::string>> listTables(const std::string& schema) = 0;

    /**
     * @brief Get column descriptions for a table
     * @param schema Schema name
     * @param table Table name
     * @return Column descriptions
     */
    virtual Result<std::vector<RemoteColumnDesc>> describeTable(
        const std::string& schema,
        const std::string& table) = 0;

    /**
     * @brief Get index descriptions for a table
     * @param schema Schema name
     * @param table Table name
     * @return Index descriptions
     */
    virtual Result<std::vector<RemoteIndexDesc>> describeIndexes(
        const std::string& schema,
        const std::string& table) = 0;

    /**
     * @brief Get foreign key descriptions for a table
     * @param schema Schema name
     * @param table Table name
     * @return Foreign key descriptions
     */
    virtual Result<std::vector<RemoteForeignKey>> describeForeignKeys(
        const std::string& schema,
        const std::string& table) = 0;

    // =========================================================================
    // Metadata
    // =========================================================================

    /**
     * @brief Get the database type this adapter handles
     */
    virtual RemoteDatabaseType getDatabaseType() const = 0;

    /**
     * @brief Get the remote server version string
     */
    virtual std::string getServerVersion() const = 0;

    /**
     * @brief Get pushdown capabilities for this connection
     */
    virtual PushdownCapability getCapabilities() const = 0;

    // =========================================================================
    // Type Conversion
    // =========================================================================

    /**
     * @brief Map remote type OID to ScratchBird type
     * @param remote_oid Remote type identifier
     * @param modifier Type modifier (e.g., varchar length)
     * @return ScratchBird type ID
     */
    virtual uint32_t mapRemoteType(uint32_t remote_oid, int32_t modifier) const = 0;

    /**
     * @brief Convert remote data to ScratchBird value
     * @param data Raw data bytes
     * @param len Data length
     * @param remote_oid Remote type identifier
     * @return Converted value
     */
    virtual RemoteValue convertToLocal(const void* data, size_t len,
                                        uint32_t remote_oid) const = 0;

    /**
     * @brief Convert ScratchBird value to remote format
     * @param value Value to convert
     * @param remote_oid Target remote type
     * @return Serialized bytes
     */
    virtual std::vector<uint8_t> convertToRemote(const RemoteValue& value,
                                                  uint32_t remote_oid) const = 0;

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get connection statistics
     */
    virtual ConnectionStats getStats() const = 0;
};

/**
 * @brief Factory for creating protocol adapters
 */
class ProtocolAdapterFactory {
public:
    /**
     * @brief Create an adapter for the specified database type
     * @param type Database type
     * @return Protocol adapter instance
     */
    static std::unique_ptr<IProtocolAdapter> create(RemoteDatabaseType type);

    /**
     * @brief Check if a database type is supported
     * @param type Database type
     * @return true if supported
     */
    static bool isSupported(RemoteDatabaseType type);

    /**
     * @brief Get list of supported database types
     */
    static std::vector<RemoteDatabaseType> supportedTypes();
};

/**
 * @brief Base implementation with common functionality
 */
class ProtocolAdapterBase : public IProtocolAdapter {
public:
    ProtocolAdapterBase();
    virtual ~ProtocolAdapterBase() = default;

    // Default implementations
    ConnectionState getState() const override { return state_; }
    bool isConnected() const override { return state_ == ConnectionState::CONNECTED ||
                                                state_ == ConnectionState::IN_TRANSACTION; }
    ConnectionStats getStats() const override { return stats_; }
    std::string getServerVersion() const override { return server_version_; }

protected:
    void setState(ConnectionState state) { state_ = state; }
    void recordQueryStart();
    void recordQueryEnd(bool success, uint64_t rows = 0);
    void recordBytesSent(uint64_t bytes);
    void recordBytesReceived(uint64_t bytes);
    void setServerVersion(const std::string& version) { server_version_ = version; }

    ConnectionState state_ = ConnectionState::DISCONNECTED;
    ConnectionStats stats_;
    std::string server_version_;
    std::chrono::steady_clock::time_point query_start_;
};

}  // namespace fdw
}  // namespace scratchbird

#endif  // SCRATCHBIRD_FDW_PROTOCOL_ADAPTER_H
