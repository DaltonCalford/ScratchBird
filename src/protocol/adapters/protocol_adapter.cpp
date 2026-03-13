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
 * Shared protocol-emulation adapter implementation.
 */

#include "scratchbird/protocol/adapters/protocol_adapter.h"
#include "scratchbird/protocol/adapters/postgresql_adapter.h"
#include "scratchbird/protocol/adapters/mysql_adapter.h"
#include "scratchbird/protocol/adapters/native_adapter.h"
#include "scratchbird/protocol/adapters/firebird_adapter.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/sqlstate.h"
#include "scratchbird/core/workload_governance.h"
#include "scratchbird/sblr/bytecode_validator.h"
#include "scratchbird/sblr/opcodes.h"
#include "scratchbird/sblr/query_compiler_v3.h"
#include "scratchbird/sblr/v3_container.h"
#include "scratchbird/core/telemetry.h"
#include "scratchbird/parser/v3_compiler.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace scratchbird {
namespace protocol {

namespace {
struct ConnectionContextGuard {
    core::ConnectionContext* previous = nullptr;
    bool changed = false;

    explicit ConnectionContextGuard(core::ConnectionContext* current)
        : previous(core::ConnectionContext::getCurrent()) {
        if (current && current != previous) {
            core::ConnectionContext::setCurrent(current);
            changed = true;
        }
    }

    ~ConnectionContextGuard() {
        if (changed) {
            core::ConnectionContext::setCurrent(previous);
        }
    }
};

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

ProtocolCodec::ColumnValue formatMySqlTextValue(const ProtocolCodec::ColumnValue& value,
                                                WireType type) {
    if (value.is_null) {
        return ProtocolCodec::ColumnValue(nullptr);
    }

    auto raw_string = [&value]() {
        return ProtocolCodec::ColumnValue::fromString(
            std::string(value.data.begin(), value.data.end()));
    };

    auto format_unknown = [&]() -> ProtocolCodec::ColumnValue {
        const bool printable =
            !value.data.empty() &&
            std::all_of(value.data.begin(), value.data.end(), [](uint8_t byte) {
                return byte >= 0x20 && byte <= 0x7e;
            });
        if (printable) {
            return raw_string();
        }
        if (value.data.size() == sizeof(int64_t)) {
            int64_t decoded = 0;
            std::memcpy(&decoded, value.data.data(), sizeof(decoded));
            return ProtocolCodec::ColumnValue::fromString(std::to_string(decoded));
        }
        if (value.data.size() == sizeof(int32_t)) {
            int32_t decoded = 0;
            std::memcpy(&decoded, value.data.data(), sizeof(decoded));
            return ProtocolCodec::ColumnValue::fromString(std::to_string(decoded));
        }
        if (value.data.size() == 1) {
            return ProtocolCodec::ColumnValue::fromString(
                std::to_string(static_cast<unsigned int>(value.data[0])));
        }
        return raw_string();
    };

    switch (type) {
        case WireType::INT16:
        case WireType::INT32: {
            if (value.data.size() < sizeof(int32_t)) {
                return raw_string();
            }
            int32_t decoded = 0;
            std::memcpy(&decoded, value.data.data(), sizeof(decoded));
            if (type == WireType::INT16) {
                decoded = static_cast<int16_t>(decoded);
            }
            return ProtocolCodec::ColumnValue::fromString(std::to_string(decoded));
        }
        case WireType::INT64: {
            if (value.data.size() < sizeof(int64_t)) {
                return raw_string();
            }
            int64_t decoded = 0;
            std::memcpy(&decoded, value.data.data(), sizeof(decoded));
            return ProtocolCodec::ColumnValue::fromString(std::to_string(decoded));
        }
        case WireType::FLOAT32:
        case WireType::FLOAT64: {
            if (value.data.size() < sizeof(double)) {
                return raw_string();
            }
            double decoded = 0.0;
            std::memcpy(&decoded, value.data.data(), sizeof(decoded));
            std::ostringstream out;
            out << std::setprecision(std::numeric_limits<double>::digits10 + 1)
                << decoded;
            return ProtocolCodec::ColumnValue::fromString(out.str());
        }
        case WireType::BOOLEAN:
            return ProtocolCodec::ColumnValue::fromString(
                (!value.data.empty() && value.data[0] != 0) ? "1" : "0");
        case WireType::UNKNOWN:
            return format_unknown();
        default:
            return raw_string();
    }
}

std::vector<ProtocolCodec::ColumnValue> formatMySqlTextRow(
    const std::vector<ProtocolCodec::ColumnValue>& row,
    const std::vector<ProtocolCodec::ColumnInfo>& columns) {
    std::vector<ProtocolCodec::ColumnValue> formatted;
    formatted.reserve(row.size());
    for (size_t index = 0; index < row.size(); ++index) {
        const WireType type = index < columns.size() ? columns[index].type : WireType::UNKNOWN;
        formatted.push_back(formatMySqlTextValue(row[index], type));
    }
    return formatted;
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

auto buildOptimizerParameterBindings(const std::vector<std::string>& parameter_values,
                                     const std::vector<bool>& parameter_nulls)
    -> optimizer::ParameterBindings {
    optimizer::ParameterBindings bindings;
    bindings.positional.reserve(parameter_values.size());
    for (size_t index = 0; index < parameter_values.size(); ++index) {
        optimizer::BoundParameterValue value;
        value.is_null = index < parameter_nulls.size() && parameter_nulls[index];
        if (!value.is_null) {
            value.text = parameter_values[index];
        }
        bindings.positional.push_back(std::move(value));
    }
    return bindings;
}

auto buildOptimizerParameterSignature(const optimizer::ParameterBindings& bindings) -> std::string {
    std::ostringstream signature;
    for (const auto& value : bindings.positional) {
        signature << (value.is_null ? 'N' : 'V') << ':';
        if (!value.is_null) {
            signature << sblr::v3::stableHash64(value.text);
        }
        signature << ';';
    }
    return signature.str();
}

auto compileScratchBirdQuery(core::Database* db,
                             core::ConnectionContext* connection_ctx,
                             const std::string& sql,
                             const optimizer::ParameterBindings* parameter_bindings,
                             sblr::detail::QueryCompilerV3PlanProfileMode plan_profile_mode,
                             std::vector<uint8_t>& bytecode_out,
                             std::string& error_out,
                             sblr::QueryCompilerV3::CompileResult* compile_result_out = nullptr)
    -> core::Status {
    if (db == nullptr) {
        error_out = "Database context is required for native compilation";
        return core::Status::INVALID_ARGUMENT;
    }

    ConnectionContextGuard ctx_guard(connection_ctx);
    sblr::QueryCompilerV3 compiler(db);
    if (connection_ctx != nullptr) {
        compiler.setCurrentSchema(connection_ctx->getCurrentSchemaId());
    }

    sblr::QueryCompilerV3::CompileResult compiled =
        (parameter_bindings != nullptr && !parameter_bindings->empty())
            ? compiler.compileWithParameters(sql, *parameter_bindings, plan_profile_mode)
            : compiler.compile(sql);
    if (!compiled.success()) {
        error_out = compiled.errors().empty() ? "Compilation failed" : compiled.errors().front();
        return core::Status::INVALID_ARGUMENT;
    }

    bytecode_out = compiled.bytecode();
    if (compile_result_out != nullptr) {
        *compile_result_out = std::move(compiled);
    }
    return core::Status::OK;
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

std::string trimAscii(const std::string& input) {
    size_t begin = 0;
    while (begin < input.size() &&
           std::isspace(static_cast<unsigned char>(input[begin])) != 0) {
        ++begin;
    }
    size_t end = input.size();
    while (end > begin &&
           std::isspace(static_cast<unsigned char>(input[end - 1])) != 0) {
        --end;
    }
    return input.substr(begin, end - begin);
}

std::string toUpperAscii(const std::string& input) {
    std::string out = input;
    for (char& ch : out) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return out;
}

bool isPostgresDialectTag(const std::string& tag) {
    const std::string upper = toUpperAscii(trimAscii(tag));
    return upper == "POSTGRESQL" || upper == "POSTGRES" || upper == "PG";
}

std::string stripIdentifierQuotes(std::string token) {
    token = trimAscii(token);
    if (token.size() >= 2 && token.front() == '"' && token.back() == '"') {
        token = token.substr(1, token.size() - 2);
    }
    return trimAscii(token);
}

bool parseInheritsParentNames(const std::string& sql, std::vector<std::string>& parents_out) {
    parents_out.clear();
    const std::string trimmed = trimAscii(sql);
    const std::string upper = toUpperAscii(trimmed);
    if (upper.rfind("CREATE", 0) != 0 || upper.find("TABLE") == std::string::npos) {
        return false;
    }

    const size_t inherits_pos = upper.find("INHERITS");
    if (inherits_pos == std::string::npos) {
        return false;
    }

    size_t open = trimmed.find('(', inherits_pos);
    if (open == std::string::npos) {
        return false;
    }

    bool in_quotes = false;
    size_t close = std::string::npos;
    int depth = 0;
    for (size_t i = open; i < trimmed.size(); ++i) {
        char ch = trimmed[i];
        if (ch == '"') {
            in_quotes = !in_quotes;
        }
        if (in_quotes) {
            continue;
        }
        if (ch == '(') {
            ++depth;
        } else if (ch == ')') {
            --depth;
            if (depth == 0) {
                close = i;
                break;
            }
        }
    }
    if (close == std::string::npos || close <= open + 1) {
        return false;
    }

    const std::string list = trimmed.substr(open + 1, close - open - 1);
    std::string current;
    in_quotes = false;
    for (size_t i = 0; i < list.size(); ++i) {
        const char ch = list[i];
        if (ch == '"') {
            in_quotes = !in_quotes;
            current.push_back(ch);
            continue;
        }
        if (ch == ',' && !in_quotes) {
            std::string token = trimAscii(current);
            if (!token.empty()) {
                std::string token_upper = toUpperAscii(token);
                if (token_upper.rfind("ONLY ", 0) == 0) {
                    token = trimAscii(token.substr(5));
                }
                parents_out.push_back(token);
            }
            current.clear();
            continue;
        }
        current.push_back(ch);
    }
    std::string tail = trimAscii(current);
    if (!tail.empty()) {
        std::string tail_upper = toUpperAscii(tail);
        if (tail_upper.rfind("ONLY ", 0) == 0) {
            tail = trimAscii(tail.substr(5));
        }
        parents_out.push_back(tail);
    }

    return !parents_out.empty();
}

bool resolveParentTable(core::CatalogManager* catalog,
                        core::ConnectionContext* conn_ctx,
                        const std::string& parent_token,
                        core::CatalogManager::TableInfo& table_info) {
    if (catalog == nullptr || conn_ctx == nullptr) {
        return false;
    }

    auto try_get_table = [&](const core::ID& schema_id, const std::string& table_name) {
        if (schema_id == core::ID{} || table_name.empty()) {
            return false;
        }
        core::ErrorContext table_ctx;
        return catalog->getTable(schema_id, table_name, table_info, &table_ctx) == core::Status::OK;
    };

    std::string raw = trimAscii(parent_token);
    std::string table_name = stripIdentifierQuotes(raw);
    std::string schema_name;

    const size_t dot = raw.find_last_of('.');
    if (dot != std::string::npos) {
        schema_name = stripIdentifierQuotes(raw.substr(0, dot));
        table_name = stripIdentifierQuotes(raw.substr(dot + 1));
    }

    const core::ID current_schema_id = conn_ctx->getCurrentSchemaId();
    if (try_get_table(current_schema_id, table_name)) {
        return true;
    }

    if (!schema_name.empty()) {
        core::CatalogManager::SchemaInfo schema_info;
        core::ErrorContext schema_ctx;
        if (catalog->getSchema(schema_name, schema_info, &schema_ctx) == core::Status::OK &&
            try_get_table(schema_info.schema_id, table_name)) {
            return true;
        }
    }

    const auto& search_path = conn_ctx->search_path();
    for (const auto& path : search_path) {
        core::CatalogManager::SchemaInfo schema_info;
        core::ErrorContext schema_ctx;
        if (catalog->getSchema(path, schema_info, &schema_ctx) != core::Status::OK) {
            continue;
        }
        if (try_get_table(schema_info.schema_id, table_name)) {
            return true;
        }
    }

    core::CatalogManager::SchemaInfo public_schema;
    core::ErrorContext public_ctx;
    if (catalog->getSchema("public", public_schema, &public_ctx) == core::Status::OK &&
        try_get_table(public_schema.schema_id, table_name)) {
        return true;
    }

    return false;
}

std::vector<std::string> collectMergedInheritsColumnsForPg(const std::string& sql,
                                                           core::Database* db,
                                                           core::ConnectionContext* conn_ctx) {
    std::vector<std::string> notices;
    if (db == nullptr || conn_ctx == nullptr) {
        return notices;
    }

    std::vector<std::string> parents;
    if (!parseInheritsParentNames(sql, parents) || parents.size() < 2) {
        return notices;
    }

    auto* catalog = db->catalog_manager();
    if (catalog == nullptr) {
        return notices;
    }

    struct ColumnSignature {
        uint16_t type = 0;
        uint32_t precision = 0;
        uint32_t scale = 0;
        std::string display_name;
    };

    std::unordered_map<std::string, ColumnSignature> seen;
    std::unordered_set<std::string> emitted;

    for (const auto& parent : parents) {
        core::CatalogManager::TableInfo parent_table;
        if (!resolveParentTable(catalog, conn_ctx, parent, parent_table)) {
            continue;
        }

        std::vector<core::CatalogManager::ColumnInfo> parent_columns;
        core::ErrorContext cols_ctx;
        if (catalog->getColumns(parent_table.table_id, parent_columns, &cols_ctx) != core::Status::OK) {
            continue;
        }

        for (const auto& col : parent_columns) {
            const std::string key = toUpperAscii(col.column_name);
            auto it = seen.find(key);
            if (it == seen.end()) {
                ColumnSignature sig;
                sig.type = col.data_type;
                sig.precision = col.type_precision;
                sig.scale = col.type_scale;
                sig.display_name = col.column_name;
                seen.emplace(key, std::move(sig));
                continue;
            }

            const bool compatible =
                it->second.type == col.data_type &&
                it->second.precision == col.type_precision &&
                it->second.scale == col.type_scale;
            if (!compatible) {
                continue;
            }
            if (emitted.insert(key).second) {
                const std::string& display = it->second.display_name.empty()
                    ? col.column_name
                    : it->second.display_name;
                notices.push_back(display);
            }
        }
    }

    return notices;
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
            core::Status tracking_status =
                connection_ctx_->beginStatementTracking(query.query, &ctx);
            if (tracking_status != core::Status::OK) {
                result.has_error = true;
                result.error_code = static_cast<uint32_t>(tracking_status);
                result.sqlstate = tracking_status == core::Status::SERIALIZATION_FAILURE
                    ? "40001"
                    : "HY000";
                result.error_message =
                    ctx.message.empty() ? "Failed to initialize statement snapshot" : ctx.message;
                return core::Status::OK;
            }
        }

        std::vector<uint8_t> bytecode;
        std::string compile_error;
        const std::string dialect =
            core::IdentifierUtils::toUpper(
                std::string(dialectTagForProtocol(getProtocolType())));
        if (dialect == "SCRATCHBIRD" && !query.parameter_values.empty()) {
            const auto bindings =
                buildOptimizerParameterBindings(query.parameter_values, query.parameter_nulls);
            status = compileScratchBirdQuery(engineDatabase(),
                                             connection_ctx_.get(),
                                             query.query,
                                             &bindings,
                                             sblr::detail::QueryCompilerV3PlanProfileMode::CUSTOM,
                                             bytecode,
                                             compile_error,
                                             nullptr);
            if (status != core::Status::OK && engineDatabase() != nullptr) {
                compile_error = buildNativeCompileDiagnostic(
                    engineDatabase(),
                    query.query,
                    compile_error.empty() ? "Compilation failed" : compile_error);
            }
        } else {
            status = compileQuery(query.query, bytecode, compile_error);
        }
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
    sblr::QueryCompilerV3::CompileResult compile_result;
    const std::string dialect =
        core::IdentifierUtils::toUpper(
            std::string(dialectTagForProtocol(getProtocolType())));
    if (dialect == "SCRATCHBIRD") {
        status = compileScratchBirdQuery(engineDatabase(),
                                         connection_ctx_.get(),
                                         query,
                                         nullptr,
                                         sblr::detail::QueryCompilerV3PlanProfileMode::GENERIC,
                                         bytecode,
                                         compile_error,
                                         &compile_result);
    } else {
        status = compileQuery(query, bytecode, compile_error);
    }
    if (status != core::Status::OK) {
        return status;
    }

    if (connection_ctx_) {
        std::vector<uint16_t> param_types_u16(param_count, 0);
        status = connection_ctx_->prepareStatement(name, query, bytecode, param_types_u16, &ctx);
        if (status != core::Status::OK) {
            return status;
        }

        if (dialect == "SCRATCHBIRD") {
            if (auto* prepared = connection_ctx_->getPreparedStatement(name)) {
                prepared->optimizer_generic_plan_hash =
                    compile_result.planProfile().runtime_plan_hash;
                prepared->optimizer_plan_mode =
                    core::ConnectionContext::PreparedStatement::OptimizerPlanMode::AUTO;
            }
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
                std::vector<uint8_t> selected_bytecode = prepared->bytecode;
                const std::string dialect =
                    core::IdentifierUtils::toUpper(
                        std::string(dialectTagForProtocol(getProtocolType())));
                if (dialect == "SCRATCHBIRD" && !params.parameter_values.empty()) {
                    const auto bindings = buildOptimizerParameterBindings(params.parameter_values,
                                                                          params.parameter_nulls);
                    const std::string parameter_signature =
                        buildOptimizerParameterSignature(bindings);

                    if (prepared->optimizer_plan_mode !=
                        core::ConnectionContext::PreparedStatement::OptimizerPlanMode::GENERIC) {
                        auto mapped_bucket =
                            prepared->optimizer_parameter_signature_to_bucket.find(
                                parameter_signature);
                        if (mapped_bucket !=
                            prepared->optimizer_parameter_signature_to_bucket.end()) {
                            auto cached_variant =
                                prepared->optimizer_bucketed_bytecode.find(
                                    mapped_bucket->second);
                            if (cached_variant !=
                                prepared->optimizer_bucketed_bytecode.end()) {
                                selected_bytecode = cached_variant->second;
                            }
                        } else {
                            std::string compile_error;
                            sblr::QueryCompilerV3::CompileResult compiled;
                            auto compile_status = compileScratchBirdQuery(
                                engineDatabase(),
                                connection_ctx_.get(),
                                prepared->sql_text,
                                &bindings,
                                sblr::detail::QueryCompilerV3PlanProfileMode::CUSTOM,
                                selected_bytecode,
                                compile_error,
                                &compiled);
                            if (compile_status != core::Status::OK) {
                                result.has_error = true;
                                result.error_code =
                                    static_cast<uint32_t>(compile_status);
                                result.sqlstate = "42000";
                                result.error_message =
                                    compile_error.empty() ? "Compilation failed"
                                                          : compile_error;
                                return core::Status::OK;
                            }

                            const auto& profile = compiled.planProfile();
                            prepared->optimizer_parameter_signature_to_bucket
                                [parameter_signature] = profile.signature;
                            prepared->optimizer_bucketed_bytecode[profile.signature] =
                                selected_bytecode;
                            if (!profile.runtime_plan_hash.empty()) {
                                prepared->optimizer_bucket_plan_hash[profile.signature] =
                                    profile.runtime_plan_hash;
                            }
                            ++prepared->optimizer_custom_sample_count;

                            if (prepared->optimizer_bucket_plan_hash.size() >= 2) {
                                prepared->optimizer_plan_mode =
                                    core::ConnectionContext::PreparedStatement::OptimizerPlanMode::
                                        CUSTOM_BUCKETED;
                            } else if (prepared->optimizer_custom_sample_count >= 3 &&
                                       !prepared->optimizer_generic_plan_hash.empty() &&
                                       prepared->optimizer_bucket_plan_hash.size() == 1 &&
                                       prepared->optimizer_bucket_plan_hash.begin()->second ==
                                           prepared->optimizer_generic_plan_hash) {
                                prepared->optimizer_plan_mode =
                                    core::ConnectionContext::PreparedStatement::OptimizerPlanMode::
                                        GENERIC;
                                selected_bytecode = prepared->bytecode;
                            }
                        }
                    }
                }

                struct ParameterGuard {
                    sblr::Executor* executor = nullptr;
                    explicit ParameterGuard(sblr::Executor* exec,
                                            const QueryContext& query_ctx)
                        : executor(exec) {
                        if (executor) {
                            executor->setParameters(query_ctx.parameter_values,
                                                    query_ctx.parameter_nulls);
                        }
                    }
                    ~ParameterGuard() {
                        if (executor) {
                            executor->clearParameters();
                        }
                    }
                } param_guard(executor_.get(), ctx);

                auto status =
                    executeBytecode(ctx.query, selected_bytecode, result, nullptr);
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
        privilege_signature.append("|db=");
        privilege_signature.append(db->uuid().toString());
        privilege_signature.append("|schema_id=");
        privilege_signature.append(connection_ctx_->getCurrentSchemaId().toString());
        privilege_signature.append("|schema_name=");
        privilege_signature.append(connection_ctx_->current_schema());
        privilege_signature.append("|search_path=");
        const auto& search_path = connection_ctx_->search_path();
        for (size_t i = 0; i < search_path.size(); ++i) {
            if (i != 0) {
                privilege_signature.push_back(',');
            }
            privilege_signature.append(search_path[i]);
        }
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
        auto status = compileScratchBirdQuery(db,
                                              connection_ctx_.get(),
                                              sql,
                                              nullptr,
                                              sblr::detail::QueryCompilerV3PlanProfileMode::GENERIC,
                                              bytecode_out,
                                              error_out,
                                              nullptr);
        if (status != core::Status::OK) {
            error_out = buildNativeCompileDiagnostic(
                db,
                sql,
                error_out.empty() ? "Compilation failed" : error_out);
            return core::Status::INVALID_ARGUMENT;
        }
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

    if (connection_ctx_ != nullptr) {
        connection_ctx_->clearNotices();
    }

    struct DialectTagExecutionScope {
        core::ConnectionContext* conn_ctx = nullptr;
        std::string previous_tag;
        bool active = false;

        DialectTagExecutionScope(core::ConnectionContext* ctx, network::ProtocolType protocol_type)
            : conn_ctx(ctx) {
            if (conn_ctx == nullptr) {
                return;
            }

            const char* protocol_tag = dialectTagForProtocol(protocol_type);
            if (protocol_tag == nullptr || protocol_tag[0] == '\0') {
                return;
            }

            previous_tag = conn_ctx->dialect_tag();
            const std::string previous_upper = toUpperAscii(trimAscii(previous_tag));
            if (previous_upper.empty() || previous_upper == "SCRATCHBIRD") {
                conn_ctx->set_dialect_tag(protocol_tag);
                active = true;
            }
        }

        ~DialectTagExecutionScope() {
            if (active && conn_ctx != nullptr) {
                conn_ctx->set_dialect_tag(previous_tag);
            }
        }
    } dialect_scope(connection_ctx_.get(), getProtocolType());

    core::WorkloadGovernance::AdmissionLease admission_lease;
    if (core::Database* db = engineDatabase(); db != nullptr && db->workload_governance() != nullptr) {
        core::WorkloadGovernance::QueryDescriptor descriptor;
        descriptor.connection = connection_ctx_.get();
        descriptor.sql = sql;
        if (connection_ctx_ != nullptr) {
            descriptor.schema_name = connection_ctx_->current_schema();
            connection_ctx_->getSessionVariable("APPLICATION_NAME", descriptor.client_app);
            connection_ctx_->getSessionVariable("RESOURCE_TAG", descriptor.resource_tag);
        }

        core::ErrorContext governance_ctx;
        auto decision =
            db->workload_governance()->acquire(descriptor, admission_lease, &governance_ctx);
        if (!decision.admitted) {
            result.has_error = true;
            result.error_code = static_cast<uint32_t>(decision.status);
            result.sqlstate = core::statusToSQLState(decision.status);
            result.error_message = decision.detail.empty()
                ? (governance_ctx.message.empty()
                    ? "Workload governance rejected the request"
                    : governance_ctx.message)
                : decision.detail;
            return core::Status::OK;
        }
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
            if (getProtocolType() == network::ProtocolType::MYSQL) {
                row = formatMySqlTextRow(row, result.columns);
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

    if (!exec_result.hasResultSet() &&
        connection_ctx_ != nullptr &&
        isPostgresDialectTag(connection_ctx_->dialect_tag())) {
        auto merged_columns = collectMergedInheritsColumnsForPg(sql,
                                                                engineDatabase(),
                                                                connection_ctx_.get());
        for (const auto& column_name : merged_columns) {
            result.notices.push_back(
                "merging multiple inherited definitions of column \"" + column_name + "\"");
        }
    }

    if (connection_ctx_ != nullptr) {
        auto pending_notices = connection_ctx_->consumeNotices();
        for (const auto& notice : pending_notices) {
            if (notice.empty()) {
                continue;
            }
            bool exists = false;
            for (const auto& existing : result.notices) {
                if (existing == notice) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                result.notices.push_back(notice);
            }
        }
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
