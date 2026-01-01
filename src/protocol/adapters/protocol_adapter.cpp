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
#include "scratchbird/sblr/opcodes.h"

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
    if (!config.database_path.empty()) {
        database_path_ = config.database_path;
    }
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
    queries_executed_++;

    core::ErrorContext ctx;
    auto status = ensureEngine(&ctx);
    if (status != core::Status::OK) {
        result.has_error = true;
        result.error_code = static_cast<uint32_t>(status);
        result.sqlstate = "58000";
        result.error_message = ctx.message;
        return status;
    }

    // Track the statement for dormant reattach inspection (no cursor state retained).
    if (connection_ctx_) {
        connection_ctx_->beginStatementTracking(query.query);
    }

    std::vector<uint8_t> bytecode;
    std::string compile_error;
    status = compileQuery(query.query, bytecode, compile_error);
    if (status != core::Status::OK) {
        result.has_error = true;
        result.error_code = static_cast<uint32_t>(status);
        result.sqlstate = "42000";
        result.error_message = compile_error.empty() ? "Compilation error" : compile_error;
        if (connection_ctx_) {
            connection_ctx_->endStatementTrackingFailure(result.error_code, result.sqlstate);
        }
        return core::Status::OK;
    }

    status = executeBytecode(query.query, bytecode, result, &ctx);
    if (connection_ctx_) {
        if (result.has_error) {
            const std::string sqlstate = result.sqlstate.empty() ? "42000" : result.sqlstate;
            connection_ctx_->endStatementTrackingFailure(result.error_code, sqlstate);
        } else {
            connection_ctx_->endStatementTrackingSuccess(result.rows_affected);
        }
    }

    return status;
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
// Engine Helpers
// ============================================================================

protocol::WireType ProtocolAdapter::mapDataType(core::DataType type) const {
    using core::DataType;
    switch (type) {
        case DataType::BOOLEAN: return protocol::WireType::BOOLEAN;
        case DataType::INT16:   return protocol::WireType::INT16;
        case DataType::INT32:   return protocol::WireType::INT32;
        case DataType::INT64:   return protocol::WireType::INT64;
        case DataType::FLOAT32: return protocol::WireType::FLOAT32;
        case DataType::FLOAT64: return protocol::WireType::FLOAT64;
        case DataType::DECIMAL: return protocol::WireType::DECIMAL;
        case DataType::CHAR:    return protocol::WireType::CHAR;
        case DataType::VARCHAR:
        case DataType::TEXT:    return protocol::WireType::VARCHAR;
        case DataType::BYTEA:   return protocol::WireType::BYTEA;
        case DataType::DATE:    return protocol::WireType::DATE;
        case DataType::TIME:    return protocol::WireType::TIME;
        case DataType::TIMESTAMP: return protocol::WireType::TIMESTAMP;
        case DataType::INTERVAL: return protocol::WireType::INTERVAL;
        case DataType::UUID:    return protocol::WireType::UUID;
        case DataType::JSON:    return protocol::WireType::JSON;
        case DataType::JSONB:   return protocol::WireType::JSONB;
        case DataType::ARRAY:   return protocol::WireType::ARRAY;
        case DataType::COMPOSITE: return protocol::WireType::COMPOSITE;
        case DataType::VECTOR:   return protocol::WireType::VECTOR;
        default: return protocol::WireType::UNKNOWN;
    }
}

protocol::ProtocolCodec::ColumnValue ProtocolAdapter::toColumnValue(const sblr::Value& val) const {
    if (val.isNull()) {
        return protocol::ProtocolCodec::ColumnValue(nullptr);
    }

    using core::DataType;
    switch (val.type()) {
        case DataType::INT16:
        case DataType::INT32:
            return protocol::ProtocolCodec::ColumnValue::fromInt32(val.getInt32());
        case DataType::INT64:
            return protocol::ProtocolCodec::ColumnValue::fromInt64(val.getInt64());
        case DataType::FLOAT32:
        case DataType::FLOAT64:
            return protocol::ProtocolCodec::ColumnValue::fromDouble(val.getFloat64());
        case DataType::BOOLEAN:
            return protocol::ProtocolCodec::ColumnValue::fromBool(val.getBool());
        default:
            return protocol::ProtocolCodec::ColumnValue::fromString(val.toString());
    }
}

core::Status ProtocolAdapter::ensureEngine(core::ErrorContext* ctx) {
    if (database_ && connection_ctx_ && executor_) {
        return core::Status::OK;
    }

    // Default database path under build/database
    if (database_path_.empty()) {
        database_path_ = std::filesystem::path("build") / "database" / "protocol_default.sbdb";
    }

    std::error_code ec;
    std::filesystem::create_directories(database_path_.parent_path(), ec);
    if (ec) {
        SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR, "Failed to create database directory");
        return core::Status::IO_ERROR;
    }

    if (!std::filesystem::exists(database_path_)) {
        auto status = core::Database::create(database_path_.string(), 16384, ctx);
        if (status != core::Status::OK) {
            return status;
        }
    }

    database_ = std::make_unique<core::Database>();
    auto status = database_->open(database_path_.string(), ctx);
    if (status != core::Status::OK) {
        database_.reset();
        return status;
    }

    status = database_->connect(connection_ctx_, ctx);
    if (status != core::Status::OK) {
        database_.reset();
        connection_ctx_.reset();
        return status;
    }

    if (connection_ctx_) {
        const char* dialect_tag = "scratchbird";
        switch (getProtocolType()) {
            case network::ProtocolType::POSTGRESQL:
                dialect_tag = "postgresql";
                break;
            case network::ProtocolType::MYSQL:
                dialect_tag = "mysql";
                break;
            case network::ProtocolType::FIREBIRD:
                dialect_tag = "firebird";
                break;
            case network::ProtocolType::NATIVE:
            case network::ProtocolType::AUTO_DETECT:
            default:
                dialect_tag = "scratchbird";
                break;
        }
        connection_ctx_->set_dialect_tag(dialect_tag);

        core::ID protocol_session_id;
        generateSessionId(protocol_session_id.bytes.data());
        connection_ctx_->setProtocolSessionId(protocol_session_id);
    }

    executor_ = std::make_unique<sblr::Executor>(database_.get());
    executor_->setConnectionContext(connection_ctx_.get());
    compiler_v2_ = std::make_unique<sblr::QueryCompilerV2>(database_.get());

    return core::Status::OK;
}

core::Status ProtocolAdapter::compileQuery(const std::string& sql,
                                           std::vector<uint8_t>& bytecode_out,
                                           std::string& error_out) {
    struct ConnectionContextGuard
    {
        core::ConnectionContext* previous = nullptr;
        bool changed = false;

        explicit ConnectionContextGuard(core::ConnectionContext* current)
            : previous(core::ConnectionContext::getCurrent())
        {
            if (current && current != previous)
            {
                core::ConnectionContext::setCurrent(current);
                changed = true;
            }
        }

        ~ConnectionContextGuard()
        {
            if (changed)
            {
                core::ConnectionContext::setCurrent(previous);
            }
        }
    };

    ConnectionContextGuard ctx_guard(connection_ctx_.get());
    auto result = compiler_v2_->compile(sql);
    if (!result.success()) {
        error_out = result.errors().empty() ? "Compilation failed" : result.errors().front();
        return core::Status::INVALID_ARGUMENT;
    }
    bytecode_out = result.bytecode();
    return core::Status::OK;
}

core::Status ProtocolAdapter::executeBytecode(const std::string& sql,
                                              const std::vector<uint8_t>& bytecode,
                                              ResultContext& result,
                                              core::ErrorContext* ctx) {
    if (bytecode.size() < 2 ||
        bytecode[0] != static_cast<uint8_t>(sblr::Opcode::VERSION) ||
        bytecode[1] != static_cast<uint8_t>(sblr::SBLR_VERSION)) {
        result.has_error = true;
        result.error_code = static_cast<uint32_t>(core::Status::NOT_SUPPORTED);
        result.sqlstate = "0A000";
        result.error_message = "Unsupported SBLR version";
        return core::Status::OK;
    }

    auto exec_result = executor_->execute(bytecode);
    if (!exec_result.success()) {
        result.has_error = true;
        result.error_message = exec_result.error();
        result.sqlstate = "42000";
        return core::Status::OK;
    }

    if (exec_result.hasResultSet()) {
        auto* rs = exec_result.resultSet();
        result.columns.clear();
        for (size_t i = 0; i < rs->columnCount(); ++i) {
            ProtocolCodec::ColumnInfo col;
            col.name = rs->columnName(i);
            col.type = mapDataType(rs->columnType(i));
            col.type_modifier = 0;
            result.columns.push_back(col);
        }

        result.rows.clear();
        for (size_t r = 0; r < rs->rowCount(); ++r) {
            std::vector<ProtocolCodec::ColumnValue> row;
            for (size_t c = 0; c < rs->columnCount(); ++c) {
                row.push_back(toColumnValue(rs->getValue(r, c)));
            }
            result.rows.push_back(std::move(row));
        }
        result.rows_affected = static_cast<int64_t>(rs->rowCount());
        result.command_tag = "SELECT " + std::to_string(result.rows_affected);
    } else {
        result.rows_affected = exec_result.affectedCount();

        // Rough command tag inference
        std::string sql_upper = sql;
        for (auto& c : sql_upper) c = static_cast<char>(std::toupper(c));
        if (sql_upper.find("INSERT") == 0) result.command_tag = "INSERT";
        else if (sql_upper.find("UPDATE") == 0) result.command_tag = "UPDATE";
        else if (sql_upper.find("DELETE") == 0) result.command_tag = "DELETE";
        else if (sql_upper.find("CREATE") == 0) result.command_tag = "CREATE";
        else if (sql_upper.find("DROP") == 0) result.command_tag = "DROP";
        else if (sql_upper.find("ALTER") == 0) result.command_tag = "ALTER";
        else result.command_tag = "OK";
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
