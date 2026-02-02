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
 * Protocol Adapter Base Class
 *
 * ScratchBird Network Layer - Phase 3.2
 *
 * Base class for wire protocol adapters that translate between external
 * database protocols (PostgreSQL, MySQL, Firebird) and ScratchBird's internal
 * wire protocol.
 */

#pragma once

#include "scratchbird/network/connection_handler.h"
#include "scratchbird/protocol/wire_protocol.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/sblr/query_compiler_v2.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/protocol/translation_cache.h"
#include <filesystem>

#include <memory>
#include <string>
#include <vector>
#include <functional>

namespace scratchbird {
namespace protocol {

// Forward declarations
class ProtocolAdapter;

// ============================================================================
// Protocol Adapter Configuration
// ============================================================================

/**
 * Protocol adapter configuration
 */
struct ProtocolAdapterConfig {
    // Connection settings
    uint32_t read_timeout_ms = 30000;       // Read timeout
    uint32_t write_timeout_ms = 30000;      // Write timeout
    uint32_t idle_timeout_ms = 3600000;     // 1 hour idle timeout
    std::string database_path;              // Database location for adapter
    std::string engine_endpoint;            // Engine IPC endpoint override
    uint32_t page_size = 16384;             // Page size for new databases
    bool auto_create_db = true;             // Create database if missing

    // Buffer settings
    size_t read_buffer_size = 65536;        // 64KB read buffer
    size_t write_buffer_size = 65536;       // 64KB write buffer
    size_t max_message_size = 16 * 1024 * 1024;  // 16MB max message
    size_t lob_stream_threshold_bytes = 256 * 1024;  // Stream LOBs larger than this

    // Feature flags
    bool enable_ssl = false;                // Enable TLS/SSL
    bool enable_compression = true;         // Enable compression
    bool strict_protocol = true;            // Strict protocol validation

    // Authentication
    std::string default_database;           // Default database if not specified
    bool require_authentication = true;     // Require authentication
    AuthMethod auth_method = AuthMethod::PASSWORD;
};

// ============================================================================
// Protocol State
// ============================================================================

/**
 * Protocol adapter state machine
 */
enum class ProtocolState {
    INITIAL,            // Initial state, waiting for handshake
    HANDSHAKE,          // Processing handshake
    SSL_NEGOTIATION,    // Negotiating SSL/TLS
    AUTHENTICATING,     // Processing authentication
    AUTHENTICATED,      // Authentication complete
    READY,              // Ready for queries
    QUERY_PROCESSING,   // Processing a query
    COPY_IN,            // COPY IN mode (PostgreSQL)
    COPY_OUT,           // COPY OUT mode (PostgreSQL)
    CLOSING,            // Connection closing
    CLOSED,             // Connection closed
    ERROR               // Protocol error state
};

/**
 * Convert state to string
 */
const char* protocolStateToString(ProtocolState state);

// ============================================================================
// Query Context
// ============================================================================

/**
 * Query execution context - passed from adapter to engine
 */
struct QueryContext {
    std::string query;              // SQL query text
    std::string statement_name;     // Prepared statement name (if any)
    std::string portal_name;        // Portal/cursor name (if any)

    // Parameters for prepared statements
    std::vector<std::string> parameter_values;
    std::vector<bool> parameter_nulls;
    std::vector<int32_t> parameter_formats;  // 0 = text, 1 = binary

    // Result format preferences
    std::vector<int16_t> result_formats;  // 0 = text, 1 = binary

    // Execution limits
    int32_t max_rows = 0;           // Max rows to return (0 = unlimited)

    // Flags
    bool describe_only = false;     // Only describe, don't execute
    bool want_description = true;   // Include column metadata
};

// ============================================================================
// Result Context
// ============================================================================

/**
 * Query result context - passed from engine to adapter
 */
struct ResultContext {
    // Column metadata
    std::vector<ProtocolCodec::ColumnInfo> columns;

    // Row data callback
    using RowCallback = std::function<bool(const std::vector<ProtocolCodec::ColumnValue>& row)>;
    RowCallback row_callback;
    // Materialized rows (optional)
    std::vector<std::vector<ProtocolCodec::ColumnValue>> rows;

    // Completion info
    std::string command_tag;        // e.g., "SELECT 100"
    int64_t rows_affected = 0;      // Rows affected/returned

    // Error info
    bool has_error = false;
    uint32_t error_code = 0;
    std::string sqlstate;
    std::string error_message;
    std::string error_detail;
    std::string error_hint;
};

// ============================================================================
// Protocol Adapter Interface
// ============================================================================

/**
 * Protocol adapter base class
 *
 * Subclasses implement specific wire protocols (PostgreSQL, MySQL, etc.)
 * and translate them to/from ScratchBird's internal format.
 */
class ProtocolAdapter : public network::ProtocolHandler {
public:
    explicit ProtocolAdapter(const ProtocolAdapterConfig& config = ProtocolAdapterConfig());
    virtual ~ProtocolAdapter();

    // Non-copyable
    ProtocolAdapter(const ProtocolAdapter&) = delete;
    ProtocolAdapter& operator=(const ProtocolAdapter&) = delete;

    // ========================================================================
    // ProtocolHandler Interface (from network layer)
    // ========================================================================

    /**
     * Get protocol identifier
     */
    network::ProtocolType getProtocolType() const override = 0;

    /**
     * Handle incoming data
     */
    core::Status handleData(network::Connection* conn) override;

    /**
     * Handle authentication
     */
    core::Status handleAuthentication(network::Connection* conn) override;

    /**
     * Initialize connection for this protocol
     */
    core::Status initializeConnection(network::Connection* conn) override;

    /**
     * Send error to client
     */
    void sendError(network::Connection* conn, const std::string& message,
                   const std::string& code = "") override;

    void setSharedDatabase(core::Database* db) { shared_database_ = db; }

    /**
     * Send ready for query
     */
    void sendReady(network::Connection* conn) override;

protected:
    // Engine helpers
    virtual core::Status compileQuery(const std::string& sql,
                                      std::vector<uint8_t>& bytecode_out,
                                      std::string& error_out);
    core::Status ensureEngine(core::ErrorContext* ctx);
    core::Status executeBytecode(const std::string& sql,
                                 const std::vector<uint8_t>& bytecode,
                                 ResultContext& result,
                                 core::ErrorContext* ctx);
    protocol::WireType mapDataType(core::DataType type) const;
    protocol::ProtocolCodec::ColumnValue toColumnValue(const sblr::Value& val) const;

    // ========================================================================
    // Protocol-Specific Methods (to be implemented by subclasses)
    // ========================================================================

protected:
    /**
     * Parse incoming data and extract messages
     *
     * Called by handleData() to parse protocol-specific messages.
     *
     * @param conn Connection with data in read buffer
     * @return Status::OK if message processed, IO_ERROR if more data needed
     */
    virtual core::Status parseMessage(network::Connection* conn) = 0;

    /**
     * Process a complete message
     *
     * Called after parseMessage() successfully extracts a message.
     *
     * @param conn Connection
     * @return Status::OK to continue, error to close connection
     */
    virtual core::Status processMessage(network::Connection* conn) = 0;

    /**
     * Send protocol-specific handshake/greeting
     *
     * @param conn Connection
     * @return Status::OK on success
     */
    virtual core::Status sendGreeting(network::Connection* conn) = 0;

    /**
     * Process authentication credentials
     *
     * @param conn Connection with auth data
     * @return Status::OK on success
     */
    virtual core::Status processAuthentication(network::Connection* conn) = 0;

    /**
     * Send authentication result
     *
     * @param conn Connection
     * @param success Whether authentication succeeded
     * @param error_msg Error message if failed
     * @return Status::OK on success
     */
    virtual core::Status sendAuthResult(network::Connection* conn,
                                        bool success,
                                        const std::string& error_msg = "") = 0;

    /**
     * Send query result to client
     *
     * @param conn Connection
     * @param result Query result
     * @return Status::OK on success
     */
    virtual core::Status sendQueryResult(network::Connection* conn,
                                         const ResultContext& result) = 0;

    /**
     * Send error to client (protocol-specific implementation)
     *
     * @param conn Connection
     * @param error_code Error code
     * @param sqlstate SQLSTATE
     * @param message Error message
     * @param detail Optional detail
     * @param hint Optional hint
     * @return Status::OK on success
     */
    virtual core::Status sendProtocolError(network::Connection* conn,
                                           uint32_t error_code,
                                           const std::string& sqlstate,
                                           const std::string& message,
                                           const std::string& detail = "",
                                           const std::string& hint = "") = 0;

    // ========================================================================
    // Query Execution (calls into ScratchBird engine)
    // ========================================================================

    /**
     * Execute a query
     *
     * This method calls into the ScratchBird engine to execute the query
     * and returns results via the ResultContext.
     */
    core::Status executeQuery(const QueryContext& query, ResultContext& result);

    /**
     * Prepare a statement
     */
    core::Status prepareStatement(const std::string& name,
                                  const std::string& query,
                                  std::vector<int32_t>& param_types);

    /**
     * Execute a prepared statement
     */
    core::Status executePrepared(const std::string& name,
                                 const QueryContext& params,
                                 ResultContext& result);

    /**
     * Close a prepared statement
     */
    core::Status closePrepared(const std::string& name);

    // ========================================================================
    // Transaction Management
    // ========================================================================

    core::Status beginTransaction();
    core::Status commitTransaction();
    core::Status rollbackTransaction();
    core::Status savepoint(const std::string& name);
    core::Status releaseSavepoint(const std::string& name);
    core::Status rollbackToSavepoint(const std::string& name);

    // ========================================================================
    // State Management
    // ========================================================================

    ProtocolState getState() const { return state_; }
    void setState(ProtocolState state) { state_ = state; }

    const ProtocolAdapterConfig& getConfig() const { return config_; }

    // ========================================================================
    // Helper Methods
    // ========================================================================

    /**
     * Write data to connection's write buffer
     */
    void writeToBuffer(network::Connection* conn, const void* data, size_t len);

    /**
     * Write a complete message to connection
     */
    core::Status sendBuffer(network::Connection* conn);

protected:
    core::Database* engineDatabase() { return shared_database_ ? shared_database_ : database_.get(); }
    const core::Database* engineDatabase() const { return shared_database_ ? shared_database_ : database_.get(); }

    ProtocolAdapterConfig config_;
    ProtocolState state_ = ProtocolState::INITIAL;

    // Session info
    std::string database_name_;
    std::string username_;
    uint32_t user_id_ = 0;
    uint64_t transaction_id_ = 0;
    bool in_transaction_ = false;

    // Prepared statements (name -> query)
    std::unordered_map<std::string, std::string> prepared_statements_;

    // Statistics
    uint64_t queries_executed_ = 0;
    uint64_t bytes_received_ = 0;
    uint64_t bytes_sent_ = 0;

    // Engine state
    std::filesystem::path database_path_;
    std::unique_ptr<core::Database> database_;
    core::Database* shared_database_ = nullptr;
    std::unique_ptr<core::ConnectionContext> connection_ctx_;
    std::unique_ptr<sblr::Executor> executor_;
    std::unique_ptr<sblr::QueryCompilerV2> compiler_v2_;
    TranslationCache* translation_cache_ = nullptr;
};

// ============================================================================
// Protocol Adapter Factory
// ============================================================================

/**
 * Create a protocol adapter based on detected protocol type
 */
std::unique_ptr<ProtocolAdapter> createProtocolAdapter(
    network::ProtocolType type,
    const ProtocolAdapterConfig& config = ProtocolAdapterConfig());

} // namespace protocol
} // namespace scratchbird
