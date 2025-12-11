/**
 * Protocol Adapter Base Implementation
 *
 * ScratchBird Network Layer - Phase 3.2
 */

#include "scratchbird/protocol/adapters/protocol_adapter.h"
#include "scratchbird/protocol/adapters/postgresql_adapter.h"
#include "scratchbird/protocol/adapters/mysql_adapter.h"
#include "scratchbird/protocol/adapters/native_adapter.h"
#include "scratchbird/protocol/adapters/firebird_adapter.h"
#include "scratchbird/core/error_context.h"

#include <cstring>

namespace scratchbird {
namespace protocol {

// ============================================================================
// Protocol State Helpers
// ============================================================================

const char* protocolStateToString(ProtocolState state) {
    switch (state) {
        case ProtocolState::INITIAL: return "INITIAL";
        case ProtocolState::HANDSHAKE: return "HANDSHAKE";
        case ProtocolState::SSL_NEGOTIATION: return "SSL_NEGOTIATION";
        case ProtocolState::AUTHENTICATING: return "AUTHENTICATING";
        case ProtocolState::AUTHENTICATED: return "AUTHENTICATED";
        case ProtocolState::READY: return "READY";
        case ProtocolState::QUERY_PROCESSING: return "QUERY_PROCESSING";
        case ProtocolState::COPY_IN: return "COPY_IN";
        case ProtocolState::COPY_OUT: return "COPY_OUT";
        case ProtocolState::CLOSING: return "CLOSING";
        case ProtocolState::CLOSED: return "CLOSED";
        case ProtocolState::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

// ============================================================================
// ProtocolAdapter Implementation
// ============================================================================

ProtocolAdapter::ProtocolAdapter(const ProtocolAdapterConfig& config)
    : config_(config) {
}

ProtocolAdapter::~ProtocolAdapter() = default;

// ============================================================================
// ProtocolHandler Interface
// ============================================================================

core::Status ProtocolAdapter::initializeConnection(network::Connection* conn) {
    if (state_ != ProtocolState::INITIAL) {
        return core::Status::INTERNAL_ERROR;
    }

    state_ = ProtocolState::HANDSHAKE;

    // Send protocol-specific greeting
    auto status = sendGreeting(conn);
    if (status != core::Status::OK) {
        state_ = ProtocolState::ERROR;
        return status;
    }

    return core::Status::OK;
}

core::Status ProtocolAdapter::handleAuthentication(network::Connection* conn) {
    if (state_ != ProtocolState::HANDSHAKE && state_ != ProtocolState::AUTHENTICATING) {
        return core::Status::INTERNAL_ERROR;
    }

    state_ = ProtocolState::AUTHENTICATING;

    // Process protocol-specific authentication
    auto status = processAuthentication(conn);
    if (status != core::Status::OK) {
        // Auth failed
        sendAuthResult(conn, false, "Authentication failed");
        return status;
    }

    // Auth succeeded
    state_ = ProtocolState::AUTHENTICATED;
    sendAuthResult(conn, true);

    // Move to ready state
    state_ = ProtocolState::READY;

    return core::Status::OK;
}

void ProtocolAdapter::sendError(network::Connection* conn, const std::string& message,
                                 const std::string& code) {
    sendProtocolError(conn, 0, code.empty() ? "42000" : code, message);
}

void ProtocolAdapter::sendReady(network::Connection* conn) {
    // Base implementation does nothing - subclasses override
    (void)conn;
}

core::Status ProtocolAdapter::handleData(network::Connection* conn) {
    // Parse incoming message
    auto status = parseMessage(conn);
    if (status != core::Status::OK) {
        // Need more data or error
        return status;
    }

    // Process the complete message
    status = processMessage(conn);
    if (status != core::Status::OK) {
        state_ = ProtocolState::ERROR;
    }

    return status;
}

// ============================================================================
// Query Execution
// ============================================================================

core::Status ProtocolAdapter::executeQuery(const QueryContext& query, ResultContext& result) {
    // TODO: Connect to ScratchBird engine and execute query
    // For now, return a simple result to test the protocol layer

    queries_executed_++;

    // Parse simple commands for testing
    std::string upper_query = query.query;
    for (char& c : upper_query) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }

    // Handle SELECT 1 type queries
    if (upper_query.find("SELECT") == 0) {
        // Simple SELECT result
        result.command_tag = "SELECT 1";
        result.rows_affected = 1;

        // Add a simple column
        ProtocolCodec::ColumnInfo col;
        col.name = "result";
        col.type = WireType::INT32;
        col.type_modifier = 0;
        result.columns.push_back(col);

        return core::Status::OK;
    }

    // Handle other commands
    if (upper_query.find("INSERT") == 0) {
        result.command_tag = "INSERT 0 1";
        result.rows_affected = 1;
        return core::Status::OK;
    }

    if (upper_query.find("UPDATE") == 0) {
        result.command_tag = "UPDATE 1";
        result.rows_affected = 1;
        return core::Status::OK;
    }

    if (upper_query.find("DELETE") == 0) {
        result.command_tag = "DELETE 1";
        result.rows_affected = 1;
        return core::Status::OK;
    }

    if (upper_query.find("CREATE") == 0 || upper_query.find("DROP") == 0 ||
        upper_query.find("ALTER") == 0) {
        // DDL command
        if (upper_query.find("TABLE") != std::string::npos) {
            result.command_tag = upper_query.substr(0, upper_query.find(' ')) + " TABLE";
        } else {
            result.command_tag = "OK";
        }
        result.rows_affected = 0;
        return core::Status::OK;
    }

    if (upper_query.find("BEGIN") == 0 || upper_query.find("START TRANSACTION") == 0) {
        in_transaction_ = true;
        result.command_tag = "BEGIN";
        return core::Status::OK;
    }

    if (upper_query.find("COMMIT") == 0) {
        in_transaction_ = false;
        result.command_tag = "COMMIT";
        return core::Status::OK;
    }

    if (upper_query.find("ROLLBACK") == 0) {
        in_transaction_ = false;
        result.command_tag = "ROLLBACK";
        return core::Status::OK;
    }

    // Unknown command - return error
    result.has_error = true;
    result.error_code = static_cast<uint32_t>(core::Status::INVALID_ARGUMENT);
    result.sqlstate = "42601";  // Syntax error
    result.error_message = "Unrecognized command";

    return core::Status::OK;
}

core::Status ProtocolAdapter::prepareStatement(const std::string& name,
                                               const std::string& query,
                                               std::vector<int32_t>& /*param_types*/) {
    prepared_statements_[name] = query;
    return core::Status::OK;
}

core::Status ProtocolAdapter::executePrepared(const std::string& name,
                                               const QueryContext& params,
                                               ResultContext& result) {
    auto it = prepared_statements_.find(name);
    if (it == prepared_statements_.end()) {
        result.has_error = true;
        result.error_code = static_cast<uint32_t>(core::Status::NOT_FOUND);
        result.sqlstate = "26000";  // Invalid SQL statement name
        result.error_message = "Prepared statement not found: " + name;
        return core::Status::NOT_FOUND;
    }

    // Execute with the stored query
    QueryContext ctx = params;
    ctx.query = it->second;
    return executeQuery(ctx, result);
}

core::Status ProtocolAdapter::closePrepared(const std::string& name) {
    auto it = prepared_statements_.find(name);
    if (it != prepared_statements_.end()) {
        prepared_statements_.erase(it);
    }
    return core::Status::OK;
}

// ============================================================================
// Transaction Management
// ============================================================================

core::Status ProtocolAdapter::beginTransaction() {
    if (in_transaction_) {
        return core::Status::INTERNAL_ERROR;
    }
    in_transaction_ = true;
    return core::Status::OK;
}

core::Status ProtocolAdapter::commitTransaction() {
    if (!in_transaction_) {
        return core::Status::INTERNAL_ERROR;
    }
    in_transaction_ = false;
    return core::Status::OK;
}

core::Status ProtocolAdapter::rollbackTransaction() {
    if (!in_transaction_) {
        return core::Status::INTERNAL_ERROR;
    }
    in_transaction_ = false;
    return core::Status::OK;
}

core::Status ProtocolAdapter::savepoint(const std::string& /*name*/) {
    if (!in_transaction_) {
        return core::Status::INTERNAL_ERROR;
    }
    return core::Status::OK;
}

core::Status ProtocolAdapter::releaseSavepoint(const std::string& /*name*/) {
    if (!in_transaction_) {
        return core::Status::INTERNAL_ERROR;
    }
    return core::Status::OK;
}

core::Status ProtocolAdapter::rollbackToSavepoint(const std::string& /*name*/) {
    if (!in_transaction_) {
        return core::Status::INTERNAL_ERROR;
    }
    return core::Status::OK;
}

// ============================================================================
// Helper Methods
// ============================================================================

void ProtocolAdapter::writeToBuffer(network::Connection* conn, const void* data, size_t len) {
    conn->appendToWriteBuffer(data, len);
    bytes_sent_ += len;
}

core::Status ProtocolAdapter::sendBuffer(network::Connection* conn) {
    // The connection manager will handle flushing the write buffer
    // when the connection becomes writable
    if (conn->hasPendingWrites()) {
        // Mark connection as wanting write notification
        return core::Status::OK;
    }
    return core::Status::OK;
}

// ============================================================================
// Factory
// ============================================================================

std::unique_ptr<ProtocolAdapter> createProtocolAdapter(
    network::ProtocolType type,
    const ProtocolAdapterConfig& config) {

    switch (type) {
        case network::ProtocolType::POSTGRESQL:
            return std::make_unique<PostgresqlAdapter>(config);

        case network::ProtocolType::MYSQL:
            return std::make_unique<MySqlAdapter>(config);

        case network::ProtocolType::FIREBIRD:
            return std::make_unique<FirebirdAdapter>(config);

        case network::ProtocolType::NATIVE:
            return std::make_unique<NativeAdapter>(config);

        default:
            return nullptr;
    }
}

} // namespace protocol
} // namespace scratchbird
