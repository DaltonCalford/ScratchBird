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
#include "scratchbird/sblr/bytecode_validator.h"
#include "scratchbird/sblr/opcodes.h"
#include "scratchbird/sblr/query_compiler_v3.h"
#include "scratchbird/sblr/v3_container.h"
#include "scratchbird/core/telemetry.h"
#include "scratchbird/parser/v3_compiler.h"

#include <cctype>
#include <cstring>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace scratchbird {
namespace protocol {

namespace {
const char* dialectTagForProtocol(network::ProtocolType type) {
    switch (type) {
        case network::ProtocolType::POSTGRESQL:
            return "postgresql";
        case network::ProtocolType::MYSQL:
            return "mysql";
        case network::ProtocolType::FIREBIRD:
            return "firebird";
        case network::ProtocolType::NATIVE:
        case network::ProtocolType::AUTO_DETECT:
        default:
            return "scratchbird";
    }
}

uint64_t currentProcessIdForPath() {
#ifdef _WIN32
    return static_cast<uint64_t>(::_getpid());
#else
    return static_cast<uint64_t>(::getpid());
#endif
}

size_t countParameterPlaceholders(const std::string& sql) {
    size_t max_index = 0;
    for (size_t i = 0; i < sql.size(); ++i) {
        if (sql[i] != '$') {
            continue;
        }
        size_t j = i + 1;
        if (j >= sql.size() || !std::isdigit(static_cast<unsigned char>(sql[j]))) {
            continue;
        }
        size_t value = 0;
        while (j < sql.size() && std::isdigit(static_cast<unsigned char>(sql[j]))) {
            value = value * 10 + static_cast<size_t>(sql[j] - '0');
            ++j;
        }
        if (value > max_index) {
            max_index = value;
        }
        i = j;
    }
    return max_index;
}

std::string buildNativeCompileDiagnostic(core::Database* db,
                                         const std::string& sql,
                                         const std::string& fallback_error) {
    std::string message = fallback_error.empty() ? "Compilation failed" : fallback_error;
    if (db == nullptr) {
        return message;
    }

    sblr::QueryCompilerV3 trace_compiler(db);
    auto trace = trace_compiler.compileTrace(sql);
    if (!trace.errors().empty()) {
        message = trace.errors().front();
    }
    if (!trace.diagnostic_sql_context().empty() &&
        message.find("SQL_CONTEXT:") == std::string::npos) {
        message.append(" | SQL_CONTEXT: ");
        message.append(trace.diagnostic_sql_context());
    }
    return message;
}
} // namespace

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
    translation_cache_ = &TranslationCacheManager::getInstance();
}

ProtocolAdapter::~ProtocolAdapter() = default;

bool ProtocolAdapter::resolveDatabaseSelection(const std::string& requested_database,
                                               std::string& selected_database) const {
    if (config_.enforce_bound_database && !config_.default_database.empty()) {
        if (!requested_database.empty() &&
            !equalsDatabaseName(requested_database, config_.default_database)) {
            return false;
        }
        selected_database = config_.default_database;
        return true;
    }

    if (!requested_database.empty()) {
        selected_database = requested_database;
        return true;
    }
    if (!config_.default_database.empty()) {
        selected_database = config_.default_database;
        return true;
    }
    selected_database = "default";
    return true;
}

bool ProtocolAdapter::equalsDatabaseName(const std::string& lhs, const std::string& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (size_t i = 0; i < lhs.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(lhs[i])) !=
            std::tolower(static_cast<unsigned char>(rhs[i]))) {
            return false;
        }
    }
    return true;
}

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
    try {
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

        struct ParameterGuard {
            sblr::Executor* executor = nullptr;
            explicit ParameterGuard(sblr::Executor* exec, const QueryContext& query_ctx)
                : executor(exec)
            {
                if (executor) {
                    executor->setParameters(query_ctx.parameter_values, query_ctx.parameter_nulls);
                }
            }
            ~ParameterGuard()
            {
                if (executor) {
                    executor->clearParameters();
                }
            }
        };

        ParameterGuard param_guard(executor_.get(), query);

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
    } catch (const std::exception& ex) {
        result.has_error = true;
        result.error_code = static_cast<uint32_t>(core::Status::INTERNAL_ERROR);
        result.sqlstate = "XX000";
        result.error_message = std::string("Unhandled adapter exception: ") + ex.what();
        if (connection_ctx_) {
            connection_ctx_->endStatementTrackingFailure(result.error_code, result.sqlstate);
        }
        return core::Status::OK;
    } catch (...) {
        result.has_error = true;
        result.error_code = static_cast<uint32_t>(core::Status::INTERNAL_ERROR);
        result.sqlstate = "XX000";
        result.error_message = "Unhandled adapter exception";
        if (connection_ctx_) {
            connection_ctx_->endStatementTrackingFailure(result.error_code, result.sqlstate);
        }
        return core::Status::OK;
    }
}

core::Status ProtocolAdapter::prepareStatement(const std::string& name,
                                               const std::string& query,
                                               std::vector<int32_t>& param_types) {
    auto param_count = countParameterPlaceholders(query);
    param_types.assign(param_count, 0);

    if (!config_.engine_endpoint.empty()) {
        prepared_statements_[name] = query;
        return core::Status::OK;
    }

    core::ErrorContext ctx;
    auto status = ensureEngine(&ctx);
    if (status != core::Status::OK) {
        return status;
    }

    std::vector<uint8_t> bytecode;
    std::string compile_error;
    status = compileQuery(query, bytecode, compile_error);
    if (status != core::Status::OK) {
        return status;
    }

    if (connection_ctx_) {
        std::vector<uint16_t> param_types_u16(param_count, 0);
        status = connection_ctx_->prepareStatement(name, query, bytecode, param_types_u16, &ctx);
        if (status != core::Status::OK) {
            return status;
        }
    } else {
        prepared_statements_[name] = query;
    }

    return core::Status::OK;
}

core::Status ProtocolAdapter::executePrepared(const std::string& name,
                                               const QueryContext& params,
                                               ResultContext& result) {
    try {
        core::ScratchBirdMetrics& metrics = core::ScratchBirdMetrics::getInstance();
        metrics.initialize();

        if (config_.engine_endpoint.empty()) {
            core::ErrorContext ctx;
            auto status = ensureEngine(&ctx);
            if (status != core::Status::OK) {
                result.has_error = true;
                result.error_code = static_cast<uint32_t>(status);
                result.sqlstate = "58000";
                result.error_message = ctx.message;
                return status;
            }
        }

        if (connection_ctx_) {
            auto* prepared = connection_ctx_->getPreparedStatement(name);
            if (prepared) {
                if (metrics.statement_cache_hits_total) {
                    metrics.statement_cache_hits_total->inc(1.0);
                }
                QueryContext ctx = params;
                ctx.query = prepared->sql_text;
                auto status = executeBytecode(ctx.query, prepared->bytecode, result, nullptr);
                connection_ctx_->recordStatementExecution(name);
                return status;
            }
            if (metrics.statement_cache_misses_total) {
                metrics.statement_cache_misses_total->inc(1.0);
            }
        }

        auto it = prepared_statements_.find(name);
        if (it != prepared_statements_.end()) {
            QueryContext ctx = params;
            ctx.query = it->second;
            return executeQuery(ctx, result);
        }

        result.has_error = true;
        result.error_code = static_cast<uint32_t>(core::Status::NOT_FOUND);
        result.sqlstate = "26000";  // Invalid SQL statement name
        result.error_message = "Prepared statement not found: " + name;
        return core::Status::NOT_FOUND;
    } catch (const std::exception& ex) {
        result.has_error = true;
        result.error_code = static_cast<uint32_t>(core::Status::INTERNAL_ERROR);
        result.sqlstate = "XX000";
        result.error_message = std::string("Unhandled adapter exception: ") + ex.what();
        return core::Status::OK;
    } catch (...) {
        result.has_error = true;
        result.error_code = static_cast<uint32_t>(core::Status::INTERNAL_ERROR);
        result.sqlstate = "XX000";
        result.error_message = "Unhandled adapter exception";
        return core::Status::OK;
    }
}

core::Status ProtocolAdapter::closePrepared(const std::string& name) {
    if (connection_ctx_) {
        core::ErrorContext ctx;
        connection_ctx_->deallocatePreparedStatement(name, &ctx);
    }

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
        case DataType::DECFLOAT16: return protocol::WireType::DECIMAL;
        case DataType::DECFLOAT34: return protocol::WireType::DECIMAL;
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
            return protocol::ProtocolCodec::ColumnValue::fromDouble(static_cast<double>(val.getFloat32()));
        case DataType::FLOAT64:
            return protocol::ProtocolCodec::ColumnValue::fromDouble(val.getFloat64());
        case DataType::BOOLEAN:
            return protocol::ProtocolCodec::ColumnValue::fromBool(val.getBool());
        default:
            return protocol::ProtocolCodec::ColumnValue::fromString(val.toString());
    }
}

core::Status ProtocolAdapter::ensureEngine(core::ErrorContext* ctx) {
    if ((database_ || shared_database_) && connection_ctx_ && executor_) {
        return core::Status::OK;
    }

    core::Database* db = shared_database_;
    if (!db) {
        // Default database path under build/database
        if (database_path_.empty()) {
            if (!config_.engine_endpoint.empty()) {
                const std::string local_name =
                    std::string("protocol_") +
                    dialectTagForProtocol(getProtocolType()) +
                    "_" +
                    std::to_string(currentProcessIdForPath()) +
                    ".sbdb";
                std::filesystem::path endpoint_path(config_.engine_endpoint);
                std::filesystem::path base_dir = endpoint_path.parent_path();
                if (base_dir.empty()) {
                    std::error_code tmp_ec;
                    std::filesystem::path tmp_dir = std::filesystem::temp_directory_path(tmp_ec);
                    if (tmp_ec || tmp_dir.empty()) {
                        tmp_dir = std::filesystem::path("/tmp");
                    }
                    base_dir = tmp_dir / "scratchbird" / "protocol";
                }
                database_path_ = base_dir / local_name;
            } else {
                database_path_ = std::filesystem::path("build") / "database" / "protocol_default.sbdb";
            }
        }

        std::error_code ec;
        const std::filesystem::path parent_dir = database_path_.parent_path();
        if (!parent_dir.empty()) {
            std::filesystem::create_directories(parent_dir, ec);
        }
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
        db = database_.get();
    }

    auto status = db->connect(connection_ctx_, ctx);
    if (status != core::Status::OK) {
        database_.reset();
        connection_ctx_.reset();
        return status;
    }

    if (connection_ctx_) {
        connection_ctx_->set_dialect_tag(dialectTagForProtocol(getProtocolType()));

        core::ID protocol_session_id;
        generateSessionId(protocol_session_id.bytes.data());
        connection_ctx_->setProtocolSessionId(protocol_session_id);
    }

    executor_ = std::make_unique<sblr::Executor>(db);
    executor_->setConnectionContext(connection_ctx_.get());
    compiler_v3_ = std::make_unique<parser::v3::Compiler>();

    return core::Status::OK;
}

core::Status ProtocolAdapter::compileQuery(const std::string& sql,
                                           std::vector<uint8_t>& bytecode_out,
                                           std::string& error_out) {
    core::Database* db = engineDatabase();
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
    const char* dialect_tag = dialectTagForProtocol(getProtocolType());
    uint64_t schema_version = 0;
    std::string privilege_signature;
    if (db && db->catalog_manager() && connection_ctx_) {
        core::CatalogManager::SchemaInfo schema_info;
        if (db->catalog_manager()->getSchema(connection_ctx_->getCurrentSchemaId(),
                                              schema_info, nullptr) == core::Status::OK) {
            schema_version = schema_info.last_modified_time;
        }
        uint64_t policy_epoch = 0;
        db->catalog_manager()->getSecurityPolicyEpoch(policy_epoch, nullptr);
        privilege_signature = connection_ctx_->getCurrentUserId().toString();
        privilege_signature.push_back('|');
        privilege_signature.append(connection_ctx_->getActiveRoleId().toString());
        privilege_signature.push_back('|');
        privilege_signature.append(std::to_string(policy_epoch));
    }
    if (translation_cache_ && translation_cache_->isEnabled()) {
        if (translation_cache_->get(dialect_tag, sql, schema_version,
                                    privilege_signature, bytecode_out)) {
            return core::Status::OK;
        }
    }
    std::string dialect = dialect_tag ? std::string(dialect_tag) : std::string();
    for (auto& ch : dialect) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }

    if (dialect == "SCRATCHBIRD") {
        if (!compiler_v3_) {
            // Remote listener mode can compile without a local ensureEngine() call.
            compiler_v3_ = std::make_unique<parser::v3::Compiler>();
        }
        auto result = compiler_v3_->compile(sql);
        if (!result.ok) {
            error_out = buildNativeCompileDiagnostic(
                db,
                sql,
                result.error.empty() ? "Compilation failed" : result.error);
            return core::Status::INVALID_ARGUMENT;
        }
        bytecode_out = result.bytecode;
    } else if (dialect == "POSTGRESQL" || dialect == "POSTGRES" || dialect == "PG") {
        if (!compiler_pg_) {
            compiler_pg_ = std::make_unique<sblr::PostgreSQLQueryCompiler>(db);
        }
        auto result = compiler_pg_->compile(sql);
        if (!result.success()) {
            error_out = result.errors().empty() ? "Compilation failed" : result.errors().front();
            return core::Status::INVALID_ARGUMENT;
        }
        bytecode_out = result.bytecode();
    } else if (dialect == "MYSQL") {
        if (!compiler_mysql_) {
            compiler_mysql_ = std::make_unique<sblr::MySQLQueryCompiler>(db);
        }
        auto result = compiler_mysql_->compile(sql);
        if (!result.success()) {
            error_out = result.errors().empty() ? "Compilation failed" : result.errors().front();
            return core::Status::INVALID_ARGUMENT;
        }
        bytecode_out = result.bytecode();
    } else {
        error_out = "Unsupported dialect for protocol adapter";
        return core::Status::INVALID_ARGUMENT;
    }
    if (translation_cache_ && translation_cache_->isEnabled()) {
        translation_cache_->put(dialect_tag, sql, schema_version,
                                privilege_signature, bytecode_out);
    }
    return core::Status::OK;
}

core::Status ProtocolAdapter::executeBytecode(const std::string& sql,
                                              const std::vector<uint8_t>& bytecode,
                                              ResultContext& result,
                                              core::ErrorContext* ctx) {
    core::ErrorContext validate_ctx;
    core::Status validate_status = sblr::validateBytecode(bytecode, &validate_ctx);
    if (validate_status != core::Status::OK) {
        result.has_error = true;
        result.error_code = static_cast<uint32_t>(validate_status);
        result.sqlstate = "0A000";
        result.error_message = validate_ctx.message.empty()
            ? "Invalid bytecode"
            : validate_ctx.message;
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
