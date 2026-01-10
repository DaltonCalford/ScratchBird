/**
 * PostgreSQL Wire Protocol Adapter Implementation
 *
 * ScratchBird Network Layer - Phase 3.2
 *
 * Implements PostgreSQL v3 wire protocol.
 */

#include "scratchbird/protocol/adapters/postgresql_adapter.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/types.h"
#include "scratchbird/sblr/postgresql_query_compiler.h"
#include "scratchbird/server/ipc_server.h"
#include "scratchbird/client/connection.h"

#include <filesystem>
#include <cstring>
#include <random>
#include <sstream>
#include <iomanip>
#include <cctype>

// For MD5
#ifdef HAVE_OPENSSL
#include <openssl/md5.h>
#else
// Simple MD5 fallback (for testing only)
#include <functional>
#endif

namespace scratchbird {
namespace protocol {

// ============================================================================
// Constructor/Destructor
// ============================================================================

PostgresqlAdapter::PostgresqlAdapter(const ProtocolAdapterConfig& config)
    : ProtocolAdapter(config) {

    // Initialize default server parameters
    server_parameters_["server_version"] = "15.0.0 ScratchBird";
    server_parameters_["server_encoding"] = "UTF8";
    server_parameters_["client_encoding"] = "UTF8";
    server_parameters_["DateStyle"] = "ISO, MDY";
    server_parameters_["TimeZone"] = "UTC";
    server_parameters_["integer_datetimes"] = "on";
    server_parameters_["standard_conforming_strings"] = "on";

    // Generate backend key data
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int32_t> dist;
    backend_pid_ = dist(gen) & 0x7FFFFFFF;  // Positive value
    backend_secret_key_ = dist(gen);

    // Generate MD5 salt
    for (int i = 0; i < 4; ++i) {
        md5_salt_[i] = static_cast<uint8_t>(dist(gen) & 0xFF);
    }
}

PostgresqlAdapter::~PostgresqlAdapter() = default;
core::Status PostgresqlAdapter::ensureRemoteClient(core::ErrorContext* ctx) {
    if (client_) {
        return core::Status::OK;
    }

    client_config_.database_name = database_name_.empty() ? "default" : database_name_;
    client_config_.ipc_method = server::IPCMethod::UNIX_SOCKET;
    client_config_.socket_path = server::getIPCPath(client_config_.database_name, client_config_.ipc_method);
    client_config_.connect_timeout_ms = config_.read_timeout_ms;
    client_config_.read_timeout_ms = config_.read_timeout_ms;
    client_config_.write_timeout_ms = config_.write_timeout_ms;
    client_config_.auto_commit = true;
    client_config_.auto_start_server = false;
    client_config_.username = username_.empty() ? "BOOTSTRAP" : username_;

    client_ = std::make_unique<client::Connection>();
    auto status = client_->connect(client_config_, ctx);
    if (status != core::Status::OK) {
        client_.reset();
        return status;
    }

    // Set search_path to emulated schema (if it exists or can be created)
    if (!search_path_set_) {
        std::string db_name = database_name_.empty() ? std::string("default") : database_name_;
        std::string schema_name = "emulation.postgresql.localhost.databases." + db_name;
        std::string set_path = "SET search_path TO '" + escapeLiteral(schema_name) + "'";
        client::ResultSet rs;
        auto set_status = client_->executeQuery(set_path, &rs, ctx);
        if (set_status == core::Status::OK) {
            search_path_set_ = true;
        }
    }

    return core::Status::OK;
}

core::Status PostgresqlAdapter::executeRemoteQuery(const QueryContext& query,
                                                   ResultContext& result,
                                                   core::ErrorContext* ctx) {
    auto status = ensureRemoteClient(ctx);
    if (status != core::Status::OK) {
        result.has_error = true;
        result.error_code = static_cast<uint32_t>(status);
        result.error_message = ctx ? ctx->message : "Failed to connect to engine";
        return status;
    }

    client::ResultSet rs;
    QueryContext rewritten = query;
    if (!query.parameter_values.empty()) {
        rewritten.query = substitutePositionalParameters(query);
    }
    status = client_->executeQuery(rewritten.query, &rs, ctx);
    if (status != core::Status::OK) {
        result.has_error = true;
        result.error_code = static_cast<uint32_t>(status);
        std::string err = client_->getLastError();
        if (err.empty() && ctx) {
            err = ctx->message;
        }
        result.error_message = err.empty() ? "Query execution failed" : err;
        return status;
    }

    result.columns.clear();
    for (const auto& col : rs.getColumns()) {
        ProtocolCodec::ColumnInfo info;
        info.name = col.name;
        info.type = col.type;
        info.type_modifier = col.type_modifier;
        result.columns.push_back(info);
    }

    result.rows.clear();
    const auto row_count = static_cast<size_t>(rs.getRowCount());
    for (size_t i = 0; i < row_count; ++i) {
        result.rows.push_back(rs.getRowValues(i));
    }

    result.rows_affected = rs.getRowsAffected();
    result.command_tag = rs.getCommandTag();
    if (result.command_tag.empty()) {
        if (row_count > 0) {
            result.command_tag = "SELECT " + std::to_string(row_count);
        } else if (result.rows_affected > 0) {
            result.command_tag = "OK";
        }
    }

    return core::Status::OK;
}

// ============================================================================
// Configuration
// ============================================================================

void PostgresqlAdapter::setServerParameter(const std::string& name, const std::string& value) {
    server_parameters_[name] = value;
}

// ============================================================================
// ProtocolAdapter Implementation
// ============================================================================

core::Status PostgresqlAdapter::parseMessage(network::Connection* conn) {
    const auto& buffer = conn->getReadBuffer();

    if (pg_state_ == PgProtocolState::STARTUP) {
        // Startup message has no type byte, just length
        if (buffer.size() < 4) {
            return core::Status::IO_ERROR;  // Need more data
        }

        int32_t length = readInt32(buffer.data());
        if (length < 4 || length > 10000) {
            // Invalid startup message
            return core::Status::INVALID_ARGUMENT;
        }

        if (buffer.size() < static_cast<size_t>(length)) {
            return core::Status::IO_ERROR;  // Need more data
        }

        // Full startup message received
        current_msg_type_ = 0;  // No type for startup
        current_msg_length_ = length;
        current_msg_data_.assign(buffer.begin(), buffer.begin() + length);

        // Consume from read buffer
        conn->consumeReadBuffer(length);

        return core::Status::OK;
    }

    // Regular message: type (1 byte) + length (4 bytes) + payload
    if (buffer.size() < 5) {
        return core::Status::IO_ERROR;  // Need more data
    }

    current_msg_type_ = static_cast<char>(buffer[0]);
    current_msg_length_ = readInt32(buffer.data() + 1);

    if (current_msg_length_ < 4) {
        return core::Status::INVALID_ARGUMENT;
    }

    size_t total_length = 1 + static_cast<size_t>(current_msg_length_);
    if (buffer.size() < total_length) {
        return core::Status::IO_ERROR;  // Need more data
    }

    // Full message received
    current_msg_data_.assign(buffer.begin() + 5, buffer.begin() + total_length);

    // Consume from read buffer
    conn->consumeReadBuffer(total_length);

    return core::Status::OK;
}

core::Status PostgresqlAdapter::processMessage(network::Connection* conn) {
    bytes_received_ += current_msg_data_.size() + 5;

    if (pg_state_ == PgProtocolState::STARTUP) {
        return handleStartupMessage(conn);
    }

    switch (current_msg_type_) {
        case pg::FrontendMsg::PASSWORD:
            return handlePasswordMessage(conn);

        case pg::FrontendMsg::QUERY:
            return handleQuery(conn);

        case pg::FrontendMsg::PARSE:
            return handleParse(conn);

        case pg::FrontendMsg::BIND:
            return handleBind(conn);

        case pg::FrontendMsg::DESCRIBE:
            return handleDescribe(conn);

        case pg::FrontendMsg::EXECUTE:
            return handleExecute(conn);

        case pg::FrontendMsg::CLOSE:
            return handleClose(conn);

        case pg::FrontendMsg::SYNC:
            return handleSync(conn);

        case pg::FrontendMsg::FLUSH:
            return handleFlush(conn);

        case pg::FrontendMsg::TERMINATE:
            return handleTerminate(conn);

        case pg::FrontendMsg::COPY_DATA:
            return handleCopyData(conn);

        case pg::FrontendMsg::COPY_DONE:
            return handleCopyDone(conn);

        case pg::FrontendMsg::COPY_FAIL:
            return handleCopyFail(conn);

        default:
            // Unknown message type
            sendErrorResponse(conn, "ERROR", "08P01",
                              "Unknown message type: " + std::string(1, current_msg_type_));
            return core::Status::INVALID_ARGUMENT;
    }
}

core::Status PostgresqlAdapter::sendGreeting(network::Connection* /*conn*/) {
    // PostgreSQL server doesn't send a greeting - it waits for client startup
    return core::Status::OK;
}

core::Status PostgresqlAdapter::processAuthentication(network::Connection* /*conn*/) {
    // Authentication is handled in handlePasswordMessage
    return core::Status::OK;
}

core::Status PostgresqlAdapter::sendAuthResult(network::Connection* conn,
                                                bool success,
                                                const std::string& error_msg) {
    if (success) {
        sendAuthenticationOk(conn);
        sendBackendKeyData(conn);

        // Send server parameters
        for (const auto& param : server_parameters_) {
            sendParameterStatus(conn, param.first, param.second);
        }

        sendReadyForQuery(conn);
        pg_state_ = PgProtocolState::READY;
    } else {
        sendErrorResponse(conn, "FATAL", "28P01", error_msg.empty() ? "Authentication failed" : error_msg);
    }

    return sendBuffer(conn);
}

core::Status PostgresqlAdapter::sendQueryResult(network::Connection* conn,
                                                 const ResultContext& result) {
    if (result.has_error) {
        return sendProtocolError(conn, result.error_code, result.sqlstate,
                                 result.error_message, result.error_detail, result.error_hint);
    }

    if (!result.columns.empty()) {
        // Send row description
        sendRowDescription(conn, result.columns);

        // Send rows if available
        for (const auto& row : result.rows) {
            sendDataRow(conn, result.columns, row);
        }
    }

    // Send command complete
    sendCommandComplete(conn, result.command_tag);

    return core::Status::OK;
}

core::Status PostgresqlAdapter::sendProtocolError(network::Connection* conn,
                                                   uint32_t error_code,
                                                   const std::string& sqlstate,
                                                   const std::string& message,
                                                   const std::string& detail,
                                                   const std::string& hint) {
    std::string mapped_state = sqlstate;
    mapStatusToSqlstate(error_code, mapped_state);
    sendErrorResponse(conn, "ERROR", mapped_state.empty() ? "XX000" : mapped_state, message, detail, hint);
    return core::Status::OK;
}

// ============================================================================
// Startup/Authentication Handling
// ============================================================================

core::Status PostgresqlAdapter::handleStartupMessage(network::Connection* conn) {
    if (current_msg_data_.size() < 8) {
        return core::Status::INVALID_ARGUMENT;
    }

    int32_t protocol_version = readInt32(current_msg_data_.data() + 4);

    // Check for special requests
    if (protocol_version == pg::SSL_REQUEST) {
        return handleSSLRequest(conn);
    }

    if (protocol_version == pg::CANCEL_REQUEST) {
        return handleCancelRequest(conn);
    }

    // Check protocol version
    if (protocol_version != pg::PROTOCOL_VERSION_3) {
        sendErrorResponse(conn, "FATAL", "08P01",
                          "Unsupported protocol version: " + std::to_string(protocol_version >> 16) +
                          "." + std::to_string(protocol_version & 0xFFFF));
        return core::Status::NOT_SUPPORTED;
    }

    // Parse startup parameters (null-terminated key-value pairs)
    size_t offset = 8;
    while (offset < current_msg_data_.size() - 1) {
        std::string key = readString(current_msg_data_.data() + offset, current_msg_data_.size() - offset);
        if (key.empty()) break;
        offset += key.size() + 1;

        if (offset >= current_msg_data_.size()) break;
        std::string value = readString(current_msg_data_.data() + offset, current_msg_data_.size() - offset);
        offset += value.size() + 1;

        client_parameters_[key] = value;

        // Extract important parameters
        if (key == "user") {
            username_ = value;
        } else if (key == "database") {
            database_name_ = value;
        }
    }

    // Default database to username if not specified
    if (database_name_.empty()) {
        database_name_ = username_;
    }

    // Request authentication
    if (config_.require_authentication) {
        // Use MD5 authentication
        sendAuthenticationMD5Password(conn, md5_salt_);
        pg_state_ = PgProtocolState::AUTH_MD5;
    } else {
        // Trust authentication
        sendAuthResult(conn, true);
        pg_state_ = PgProtocolState::READY;
    }

    return sendBuffer(conn);
}

core::Status PostgresqlAdapter::handleSSLRequest(network::Connection* conn) {
    // SSL not supported yet - send 'N' (no SSL)
    uint8_t response = 'N';
    writeToBuffer(conn, &response, 1);
    sendBuffer(conn);

    // Reset state to wait for another startup message
    pg_state_ = PgProtocolState::STARTUP;
    return core::Status::OK;
}

core::Status PostgresqlAdapter::handleCancelRequest(network::Connection* /*conn*/) {
    // Cancel request handling
    if (current_msg_data_.size() < 16) {
        return core::Status::INVALID_ARGUMENT;
    }

    int32_t pid = readInt32(current_msg_data_.data() + 8);
    int32_t key = readInt32(current_msg_data_.data() + 12);

    // TODO: Implement cancel request handling
    // For now, just close the connection
    (void)pid;
    (void)key;

    return core::Status::OK;
}

core::Status PostgresqlAdapter::handlePasswordMessage(network::Connection* conn) {
    // Password is null-terminated string
    std::string password = readString(current_msg_data_.data(), current_msg_data_.size());

    // Validate password based on auth method
    if (pg_state_ == PgProtocolState::AUTH_MD5) {
        // Expected format: "md5" + MD5(MD5(password + user) + salt)
        std::string expected = computeMD5Hash(password, username_, md5_salt_);

        // For testing, accept any password
        // TODO: Implement proper password validation
        bool valid = true;  // password == expected or trust mode

        if (valid) {
            return sendAuthResult(conn, true);
        } else {
            return sendAuthResult(conn, false, "Password authentication failed for user \"" + username_ + "\"");
        }
    } else if (pg_state_ == PgProtocolState::AUTH_REQUESTED) {
        // Cleartext password
        // TODO: Validate password
        return sendAuthResult(conn, true);
    }

    return core::Status::INTERNAL_ERROR;
}

// ============================================================================
// Query Handling
// ============================================================================

core::Status PostgresqlAdapter::handleQuery(network::Connection* conn) {
    // Simple query protocol
    std::string query = readString(current_msg_data_.data(), current_msg_data_.size());

    // Check for empty query
    if (query.empty() || query == ";") {
        sendEmptyQueryResponse(conn);
        sendReadyForQuery(conn);
        return sendBuffer(conn);
    }

    // Execute query
    QueryContext ctx;
    ctx.query = query;

    ResultContext result;
    auto status = executeRemoteQuery(ctx, result);

    updateTransactionStatus(query, result.has_error);

    if (result.has_error) {
        sendProtocolError(conn, result.error_code, result.sqlstate,
                  result.error_message, result.error_detail, result.error_hint);
    } else {
        sendQueryResult(conn, result);
    }

    sendReadyForQuery(conn);
    return sendBuffer(conn);
}

// ============================================================================
// Extended Query Protocol
// ============================================================================

core::Status PostgresqlAdapter::handleParse(network::Connection* conn) {
    size_t offset = 0;

    // Statement name (empty = unnamed)
    std::string stmt_name = readString(current_msg_data_.data() + offset, current_msg_data_.size() - offset);
    offset += stmt_name.size() + 1;

    // Query string
    std::string query = readString(current_msg_data_.data() + offset, current_msg_data_.size() - offset);
    offset += query.size() + 1;

    // Number of parameter types
    if (offset + 2 > current_msg_data_.size()) {
        sendErrorResponse(conn, "ERROR", "08P01", "Invalid Parse message");
        return core::Status::INVALID_ARGUMENT;
    }
    int16_t num_params = readInt16(current_msg_data_.data() + offset);
    offset += 2;

    // Parameter OIDs
    std::vector<int32_t> param_types;
    for (int16_t i = 0; i < num_params; ++i) {
        if (offset + 4 > current_msg_data_.size()) break;
        param_types.push_back(readInt32(current_msg_data_.data() + offset));
        offset += 4;
    }

    // Store prepared statement
    PgPreparedStatement stmt;
    stmt.name = stmt_name;
    stmt.query = query;
    stmt.param_types = param_types;
    statements_[stmt_name] = stmt;

    sendParseComplete(conn);
    pending_operations_.push_back('P');

    return core::Status::OK;
}

core::Status PostgresqlAdapter::handleBind(network::Connection* conn) {
    size_t offset = 0;

    // Portal name (empty = unnamed)
    std::string portal_name = readString(current_msg_data_.data() + offset, current_msg_data_.size() - offset);
    offset += portal_name.size() + 1;

    // Statement name
    std::string stmt_name = readString(current_msg_data_.data() + offset, current_msg_data_.size() - offset);
    offset += stmt_name.size() + 1;

    // Check statement exists
    auto stmt_it = statements_.find(stmt_name);
    if (stmt_it == statements_.end()) {
        sendErrorResponse(conn, "ERROR", "26000", "Prepared statement does not exist: " + stmt_name);
        return core::Status::NOT_FOUND;
    }

    // Number of parameter format codes
    if (offset + 2 > current_msg_data_.size()) {
        sendErrorResponse(conn, "ERROR", "08P01", "Invalid Bind message");
        return core::Status::INVALID_ARGUMENT;
    }
    int16_t num_format_codes = readInt16(current_msg_data_.data() + offset);
    offset += 2;

    // Format codes
    std::vector<int16_t> param_formats;
    for (int16_t i = 0; i < num_format_codes; ++i) {
        if (offset + 2 > current_msg_data_.size()) break;
        param_formats.push_back(readInt16(current_msg_data_.data() + offset));
        offset += 2;
    }

    // Number of parameters
    if (offset + 2 > current_msg_data_.size()) {
        sendErrorResponse(conn, "ERROR", "08P01", "Invalid Bind message");
        return core::Status::INVALID_ARGUMENT;
    }
    int16_t num_params = readInt16(current_msg_data_.data() + offset);
    offset += 2;

    // Parameter values
    PgPortal portal;
    portal.name = portal_name;
    portal.statement_name = stmt_name;
    portal.fetch_pos = 0;
    portal.buffered_rows.clear();
    portal.more_rows_available = false;

    for (int16_t i = 0; i < num_params; ++i) {
        int16_t fmt = 0;
        if (!param_formats.empty()) {
            if (param_formats.size() == 1) {
                fmt = param_formats.front();
            } else if (static_cast<size_t>(i) < param_formats.size()) {
                fmt = param_formats[i];
            }
        }
        if (offset + 4 > current_msg_data_.size()) break;
        int32_t len = readInt32(current_msg_data_.data() + offset);
        offset += 4;

        if (len == -1) {
            // NULL
            portal.param_values.emplace_back("");
            portal.param_nulls.push_back(true);
            portal.param_formats.push_back(fmt);
        } else {
            // Value
            if (offset + static_cast<size_t>(len) > current_msg_data_.size()) break;
            WireType ptype = WireType::UNKNOWN;
            if (static_cast<size_t>(i) < stmt_it->second.param_types.size()) {
                ptype = oidToWireType(stmt_it->second.param_types[i]);
            }
            if (fmt == 1) {
                std::string text_val = decodeBinaryParamToText(
                    ptype,
                    current_msg_data_.data() + offset,
                    static_cast<size_t>(len));
                portal.param_values.emplace_back(std::move(text_val));
            } else {
                portal.param_values.emplace_back(
                    reinterpret_cast<const char*>(current_msg_data_.data() + offset), len);
            }
            portal.param_nulls.push_back(false);
            portal.param_formats.push_back(fmt);
            offset += len;
        }
    }

    // Result format codes
    if (offset + 2 <= current_msg_data_.size()) {
        int16_t num_result_formats = readInt16(current_msg_data_.data() + offset);
        offset += 2;

        for (int16_t i = 0; i < num_result_formats; ++i) {
            if (offset + 2 > current_msg_data_.size()) break;
            portal.result_formats.push_back(readInt16(current_msg_data_.data() + offset));
            offset += 2;
        }
    }

    portals_[portal_name] = portal;

    sendBindComplete(conn);
    pending_operations_.push_back('B');

    return core::Status::OK;
}

core::Status PostgresqlAdapter::handleDescribe(network::Connection* conn) {
    if (current_msg_data_.empty()) {
        sendErrorResponse(conn, "ERROR", "08P01", "Invalid Describe message");
        return core::Status::INVALID_ARGUMENT;
    }

    char type = static_cast<char>(current_msg_data_[0]);  // 'S' for statement, 'P' for portal
    std::string name = readString(current_msg_data_.data() + 1, current_msg_data_.size() - 1);

    if (type == 'S') {
        // Describe statement
        auto it = statements_.find(name);
        if (it == statements_.end()) {
            sendErrorResponse(conn, "ERROR", "26000", "Prepared statement does not exist: " + name);
            return core::Status::NOT_FOUND;
        }

        // Send parameter description
        sendParameterDescription(conn, it->second.param_types);

        // Send row description (if it's a SELECT)
        if (it->second.result_columns.empty()) {
            sendNoData(conn);
        } else {
            sendRowDescription(conn, it->second.result_columns);
        }
    } else if (type == 'P') {
        // Describe portal
        auto it = portals_.find(name);
        if (it == portals_.end()) {
            sendErrorResponse(conn, "ERROR", "34000", "Portal does not exist: " + name);
            return core::Status::NOT_FOUND;
        }

        // Get statement for this portal
        auto stmt_it = statements_.find(it->second.statement_name);
        if (stmt_it == statements_.end()) {
            sendNoData(conn);
        } else if (stmt_it->second.result_columns.empty()) {
            sendNoData(conn);
        } else {
            sendRowDescription(conn, stmt_it->second.result_columns, it->second.result_formats);
        }
    }

    pending_operations_.push_back('D');
    return core::Status::OK;
}

core::Status PostgresqlAdapter::handleExecute(network::Connection* conn) {
    // Portal name
    std::string portal_name = readString(current_msg_data_.data(), current_msg_data_.size());
    size_t offset = portal_name.size() + 1;

    // Max rows
    int32_t max_rows = 0;
    if (offset + 4 <= current_msg_data_.size()) {
        max_rows = readInt32(current_msg_data_.data() + offset);
    }

    // Find portal
    auto it = portals_.find(portal_name);
    if (it == portals_.end()) {
        sendErrorResponse(conn, "ERROR", "34000", "Portal does not exist: " + portal_name);
        return core::Status::NOT_FOUND;
    }

    PgPortal& portal = it->second;
    // Find statement
    auto stmt_it = statements_.find(portal.statement_name);
    if (stmt_it == statements_.end()) {
        sendErrorResponse(conn, "ERROR", "26000", "Prepared statement does not exist");
        return core::Status::NOT_FOUND;
    }

    // If this portal already executed, reuse buffered rows instead of re-executing
    if (portal.executed) {
        if (!portal.buffered_rows.empty() && portal.fetch_pos < portal.buffered_rows.size()) {
            size_t remaining = portal.buffered_rows.size() - portal.fetch_pos;
            size_t to_send = max_rows > 0
                ? std::min<size_t>(static_cast<size_t>(max_rows), remaining)
                : remaining;

            if (!stmt_it->second.result_columns.empty()) {
                for (size_t i = 0; i < to_send; ++i) {
                    sendDataRow(conn, stmt_it->second.result_columns,
                                portal.buffered_rows[portal.fetch_pos + i],
                                portal.result_formats);
                }
            }

            portal.fetch_pos += to_send;
            if (portal.fetch_pos < portal.buffered_rows.size()) {
                sendPortalSuspended(conn);
            } else {
                std::string tag = portal.command_tag.empty()
                    ? "SELECT " + std::to_string(portal.fetch_pos)
                    : portal.command_tag;
                sendCommandComplete(conn, tag);
                portal.completed = true;
            }
            pending_operations_.push_back('E');
            return sendBuffer(conn);
        }

        // Already executed and drained: acknowledge completion without rerun
        std::string tag = portal.command_tag.empty()
            ? "SELECT " + std::to_string(portal.fetch_pos)
            : portal.command_tag;
        sendCommandComplete(conn, tag);
        pending_operations_.push_back('E');
        portal.completed = true;
        return sendBuffer(conn);
    }

    // Execute
    QueryContext ctx;
    ctx.query = stmt_it->second.query;
    ctx.statement_name = portal.statement_name;
    ctx.portal_name = portal_name;
    ctx.parameter_values = portal.param_values;
    ctx.parameter_nulls = portal.param_nulls;
    ctx.parameter_formats.clear();
    ctx.parameter_formats.reserve(portal.param_formats.size());
    for (auto f : portal.param_formats) {
        ctx.parameter_formats.push_back(static_cast<int32_t>(f));
    }
    ctx.result_formats = portal.result_formats;
    ctx.max_rows = max_rows;

    ResultContext result;
    result.row_callback = [&](const std::vector<ProtocolCodec::ColumnValue>& row) {
        // Buffer rows when a max_rows limit is present
        if (max_rows > 0) {
            if (portal.buffered_rows.size() < static_cast<size_t>(max_rows)) {
                portal.buffered_rows.push_back(row);
            }
            // Stop fetching once we've buffered the requested slice
            if (portal.buffered_rows.size() >= static_cast<size_t>(max_rows)) {
                portal.more_rows_available = true;
                return false;
            }
            return true;
        }
        if (!result.columns.empty()) {
            sendDataRow(conn, result.columns, row, portal.result_formats);
        }
        return true;
    };
    executeRemoteQuery(ctx, result);

    updateTransactionStatus(stmt_it->second.query, result.has_error);

    if (result.has_error) {
        sendProtocolError(conn, result.error_code, result.sqlstate,
                  result.error_message, result.error_detail, result.error_hint);
    } else {
        if (portal.buffered_rows.empty() && !result.rows.empty() && max_rows > 0) {
            portal.buffered_rows = result.rows;
        }
        if (!result.columns.empty()) {
            sendRowDescription(conn, result.columns, portal.result_formats);
            // Send buffered rows according to portal fetch state
            if (max_rows > 0) {
                const auto& source_rows = portal.buffered_rows.empty() ? result.rows : portal.buffered_rows;
                size_t remaining = source_rows.size() > portal.fetch_pos
                    ? source_rows.size() - portal.fetch_pos
                    : 0;
                size_t to_send = std::min<size_t>(static_cast<size_t>(max_rows), remaining);
                for (size_t i = 0; i < to_send; ++i) {
                    const auto& row = source_rows[portal.fetch_pos + i];
                    sendDataRow(conn, result.columns, row, portal.result_formats);
                }
                portal.fetch_pos += to_send;
                if (portal.more_rows_available || remaining > static_cast<size_t>(max_rows)) {
                    sendPortalSuspended(conn);
                } else {
                    sendCommandComplete(conn, result.command_tag);
                }
            } else {
                const auto& source_rows = portal.buffered_rows.empty() ? result.rows : portal.buffered_rows;
                for (const auto& row : source_rows) {
                    sendDataRow(conn, result.columns, row, portal.result_formats);
                }
                portal.fetch_pos = source_rows.size();
                sendCommandComplete(conn, result.command_tag);
            }
        } else {
            sendCommandComplete(conn, result.command_tag);
        }
        if (stmt_it->second.result_columns.empty() && !result.columns.empty()) {
            stmt_it->second.result_columns = result.columns;
        }
    }

    portal.executed = true;
    portal.command_tag = result.command_tag;
    // If we suspended due to max_rows, keep the portal open for fetches
    if (!portal.more_rows_available && portal.fetch_pos >= portal.buffered_rows.size()) {
        portal.completed = true;
    }
    pending_operations_.push_back('E');

    return core::Status::OK;
}

core::Status PostgresqlAdapter::handleFetch(network::Connection* conn) {
    // Treat fetch as an execute against an already-bound portal
    // Portal name
    std::string portal_name = readString(current_msg_data_.data(), current_msg_data_.size());
    size_t offset = portal_name.size() + 1;

    // Max rows (optional)
    int32_t max_rows = 0;
    if (offset + 4 <= current_msg_data_.size()) {
        max_rows = readInt32(current_msg_data_.data() + offset);
    }

    auto it = portals_.find(portal_name);
    if (it == portals_.end()) {
        sendErrorResponse(conn, "ERROR", "34000", "Portal does not exist: " + portal_name);
        return core::Status::NOT_FOUND;
    }

    PgPortal& portal = it->second;
    auto stmt_it = statements_.find(portal.statement_name);
    if (stmt_it == statements_.end()) {
        sendErrorResponse(conn, "ERROR", "26000", "Prepared statement does not exist");
        return core::Status::NOT_FOUND;
    }
    // Reuse existing buffered rows
    size_t remaining = portal.buffered_rows.size() > portal.fetch_pos
        ? portal.buffered_rows.size() - portal.fetch_pos
        : 0;
    size_t to_send = max_rows > 0
        ? std::min<size_t>(static_cast<size_t>(max_rows), remaining)
        : remaining;

    if (!portal.buffered_rows.empty()) {
        // Only send rows; RowDescription has already been sent
        for (size_t i = 0; i < to_send; ++i) {
            sendDataRow(conn, stmt_it->second.result_columns,
                        portal.buffered_rows[portal.fetch_pos + i],
                        portal.result_formats);
        }
    }

    portal.fetch_pos += to_send;
    if (portal.fetch_pos < portal.buffered_rows.size() || portal.more_rows_available) {
        sendPortalSuspended(conn);
    } else {
        std::string tag = "SELECT " + std::to_string(portal.fetch_pos);
        sendCommandComplete(conn, tag);
        portal.completed = true;
        portal.more_rows_available = false;
    }
    pending_operations_.push_back('E');
    return sendBuffer(conn);
}

core::Status PostgresqlAdapter::handleClose(network::Connection* conn) {
    if (current_msg_data_.empty()) {
        sendErrorResponse(conn, "ERROR", "08P01", "Invalid Close message");
        return core::Status::INVALID_ARGUMENT;
    }

    char type = static_cast<char>(current_msg_data_[0]);
    std::string name = readString(current_msg_data_.data() + 1, current_msg_data_.size() - 1);

    if (type == 'S') {
        statements_.erase(name);
    } else if (type == 'P') {
        portals_.erase(name);
    }

    sendCloseComplete(conn);
    pending_operations_.push_back('C');

    return core::Status::OK;
}

core::Status PostgresqlAdapter::handleSync(network::Connection* conn) {
    // Sync marks end of extended query batch
    pending_operations_.clear();
    sync_pending_ = false;

    sendReadyForQuery(conn);
    return sendBuffer(conn);
}

core::Status PostgresqlAdapter::handleFlush(network::Connection* conn) {
    // Flush sends all pending output
    return sendBuffer(conn);
}

core::Status PostgresqlAdapter::handleCopyData(network::Connection* /*conn*/) {
    // TODO: Implement COPY IN handling
    return core::Status::NOT_SUPPORTED;
}

core::Status PostgresqlAdapter::handleCopyDone(network::Connection* /*conn*/) {
    // TODO: Implement COPY IN completion
    return core::Status::NOT_SUPPORTED;
}

core::Status PostgresqlAdapter::handleCopyFail(network::Connection* /*conn*/) {
    // TODO: Implement COPY IN failure
    return core::Status::NOT_SUPPORTED;
}

core::Status PostgresqlAdapter::handleTerminate(network::Connection* conn) {
    pg_state_ = PgProtocolState::CLOSING;
    conn->close(network::CloseReason::CLIENT_DISCONNECT);
    return core::Status::OK;
}

// ============================================================================
// Message Sending
// ============================================================================

void PostgresqlAdapter::sendMessage(network::Connection* conn, char type, const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> msg;
    msg.reserve(1 + 4 + payload.size());

    // Type
    writeByte(msg, static_cast<uint8_t>(type));

    // Length (includes itself but not type)
    writeInt32(msg, static_cast<int32_t>(4 + payload.size()));

    // Payload
    writeBytes(msg, payload.data(), payload.size());

    writeToBuffer(conn, msg.data(), msg.size());
}

void PostgresqlAdapter::sendAuthenticationOk(network::Connection* conn) {
    std::vector<uint8_t> payload;
    writeInt32(payload, pg::AuthType::OK);
    sendMessage(conn, pg::BackendMsg::AUTHENTICATION, payload);
}

void PostgresqlAdapter::sendAuthenticationMD5Password(network::Connection* conn, const uint8_t salt[4]) {
    std::vector<uint8_t> payload;
    writeInt32(payload, pg::AuthType::MD5_PASSWORD);
    writeBytes(payload, salt, 4);
    sendMessage(conn, pg::BackendMsg::AUTHENTICATION, payload);
}

void PostgresqlAdapter::sendAuthenticationCleartextPassword(network::Connection* conn) {
    std::vector<uint8_t> payload;
    writeInt32(payload, pg::AuthType::CLEARTEXT_PASSWORD);
    sendMessage(conn, pg::BackendMsg::AUTHENTICATION, payload);
}

void PostgresqlAdapter::sendBackendKeyData(network::Connection* conn) {
    std::vector<uint8_t> payload;
    writeInt32(payload, backend_pid_);
    writeInt32(payload, backend_secret_key_);
    sendMessage(conn, pg::BackendMsg::BACKEND_KEY_DATA, payload);
}

void PostgresqlAdapter::sendParameterStatus(network::Connection* conn,
                                             const std::string& name,
                                             const std::string& value) {
    std::vector<uint8_t> payload;
    writeString(payload, name);
    writeString(payload, value);
    sendMessage(conn, pg::BackendMsg::PARAMETER_STATUS, payload);
}

void PostgresqlAdapter::sendReadyForQuery(network::Connection* conn) {
    std::vector<uint8_t> payload;
    char status = pg::TransactionStatus::IDLE;
    if (txn_failed_) {
        status = pg::TransactionStatus::FAILED;
    } else if (in_transaction_) {
        status = pg::TransactionStatus::IN_TRANSACTION;
    }
    writeByte(payload, static_cast<uint8_t>(status));
    sendMessage(conn, pg::BackendMsg::READY_FOR_QUERY, payload);
}

core::Status PostgresqlAdapter::ensurePostgresSystemCatalog(core::ErrorContext* ctx) {
    if (!database_) {
        SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT, "Database not initialized");
        return core::Status::INVALID_ARGUMENT;
    }

    auto* catalog = database_->catalog_manager();
    if (!catalog) {
        SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT, "Catalog manager not available");
        return core::Status::INVALID_ARGUMENT;
    }

    std::string db_name = database_name_;
    if (db_name.empty() && !database_path_.empty()) {
        auto stem = std::filesystem::path(database_path_).stem().string();
        if (!stem.empty()) {
            db_name = stem;
        }
    }
    if (db_name.empty()) {
        db_name = "default";
    }

    std::string schema_name = "emulation.postgresql.localhost.databases." + db_name;

    core::CatalogManager::SchemaInfo schema_info;
    auto status = catalog->getSchema(schema_name, schema_info, ctx);
    if (status != core::Status::OK) {
        if (status != core::Status::INVALID_ARGUMENT && status != core::Status::NOT_FOUND) {
            return status;
        }
        core::ID schema_id;
        status = catalog->createSchemaPath(schema_name,
                                           core::CatalogManager::SchemaType::REMOTE_EMULATED,
                                           schema_id,
                                           ctx);
        if (status != core::Status::OK) {
            return status;
        }
        status = catalog->getSchema(schema_id, schema_info, ctx);
        if (status != core::Status::OK) {
            return status;
        }
    }
    pg_schema_id_ = schema_info.schema_id;

    auto ensure_view = [&](const std::string& name, const std::string& definition) -> core::Status {
        core::CatalogManager::ViewInfo view_info;
        auto s = catalog->getView(schema_info.schema_id, name, view_info, ctx);
        if (s == core::Status::OK) {
            return core::Status::OK;
        }
        if (s != core::Status::INVALID_ARGUMENT && s != core::Status::NOT_FOUND) {
            return s;
        }
        return catalog->createView(schema_info.schema_id, name, definition, false,
                                   false, false, {}, core::ID{}, ctx);
    };

    // Minimal pg_catalog placeholders
    status = ensure_view("pg_database", "SELECT NULL AS datname, NULL AS oid, NULL AS encoding, NULL AS datcollate, NULL AS datctype, NULL AS datistemplate, NULL AS datallowconn WHERE 1 = 0");
    if (status != core::Status::OK) return status;
    status = ensure_view("pg_namespace", "SELECT NULL AS nspname, NULL AS oid, NULL AS nspowner WHERE 1 = 0");
    if (status != core::Status::OK) return status;
    status = ensure_view("pg_class", "SELECT NULL AS relname, NULL AS oid, NULL AS relnamespace, NULL AS relkind, NULL AS relowner WHERE 1 = 0");
    if (status != core::Status::OK) return status;
    status = ensure_view("pg_attribute", "SELECT NULL AS attname, NULL AS attrelid, NULL AS attnum, NULL AS atttypid, NULL AS attnotnull WHERE 1 = 0");
    if (status != core::Status::OK) return status;
    status = ensure_view("pg_type", "SELECT NULL AS typname, NULL AS oid, NULL AS typnamespace, NULL AS typlen, NULL AS typtype WHERE 1 = 0");
    if (status != core::Status::OK) return status;
    status = ensure_view("pg_roles", "SELECT NULL AS rolname, NULL AS rolsuper, NULL AS rolcreaterole, NULL AS rolcreatedb, NULL AS rolinherit, NULL AS rolcanlogin, NULL AS rolreplication WHERE 1 = 0");
    if (status != core::Status::OK) return status;
    status = ensure_view("pg_proc", "SELECT NULL AS proname, NULL AS pronamespace, NULL AS proowner, NULL AS prolang, NULL AS prorettype WHERE 1 = 0");
    if (status != core::Status::OK) return status;
    status = ensure_view("pg_index", "SELECT NULL AS indexrelid, NULL AS indrelid, NULL AS indkey WHERE 1 = 0");
    if (status != core::Status::OK) return status;

    return core::Status::OK;
}

core::Status PostgresqlAdapter::compileQuery(const std::string& sql,
                                             std::vector<uint8_t>& bytecode_out,
                                             std::string& error_out) {
    core::ErrorContext ctx;
    auto status = ensureEngine(&ctx);
    if (status != core::Status::OK) {
        error_out = ctx.message;
        return status;
    }

    status = ensurePostgresSystemCatalog(&ctx);
    if (status != core::Status::OK) {
        error_out = ctx.message.empty() ? "Failed to initialize PostgreSQL catalog" : ctx.message;
        return status;
    }

    sblr::PostgreSQLQueryCompiler compiler(database_.get());
    std::string db_name = database_name_.empty() ? std::string("default") : database_name_;
    compiler.setDefaultSchema("emulation.postgresql.localhost.databases." + db_name);
    if (pg_schema_id_ != core::ID{}) {
        compiler.setCurrentSchema(pg_schema_id_);
    }
    auto result = compiler.compile(sql);
    if (!result.success()) {
        error_out = result.errors().empty() ? "Compilation failed" : result.errors().front();
        return core::Status::INVALID_ARGUMENT;
    }
    bytecode_out = result.bytecode();
    return core::Status::OK;
}

int16_t PostgresqlAdapter::selectFormatForColumn(size_t idx,
                                                 const std::vector<int16_t>& formats,
                                                 WireType type) {
    int16_t format = 0;
    if (!formats.empty()) {
        if (formats.size() == 1) {
            format = formats.front();
        } else if (idx < formats.size()) {
            format = formats[idx];
        }
    }
    if (format == 1 && !supportsBinaryFormat(type)) {
        format = 0;
    }
    return format;
}

bool PostgresqlAdapter::supportsBinaryFormat(WireType type) {
    switch (type) {
        case WireType::BOOLEAN:
        case WireType::INT16:
        case WireType::INT32:
        case WireType::INT64:
        case WireType::FLOAT32:
        case WireType::FLOAT64:
        case WireType::BYTEA:
        case WireType::VARCHAR:
        case WireType::CHAR:
        case WireType::JSON:
        case WireType::JSONB:
        case WireType::XML:
            return true;
        default:
            return false;
    }
}

static std::string hexEncode(const std::vector<uint8_t>& bytes) {
    std::ostringstream oss;
    oss << "\\x";
    oss << std::hex << std::setfill('0');
    for (uint8_t b : bytes) {
        oss << std::setw(2) << static_cast<int>(b);
    }
    return oss.str();
}

std::vector<uint8_t> PostgresqlAdapter::encodeTextValue(const ProtocolCodec::ColumnValue& val,
                                                        WireType type) {
    if (val.is_null) {
        return {};
    }

    auto toStringInt = [&](auto value) {
        return std::to_string(value);
    };

    std::string out;
    switch (type) {
        case WireType::BOOLEAN: {
            bool b = !val.data.empty() && val.data[0] != 0;
            out = b ? "t" : "f";
            break;
        }
        case WireType::INT16: {
            int16_t v = 0;
            if (val.data.size() >= sizeof(int16_t)) {
                std::memcpy(&v, val.data.data(), sizeof(int16_t));
            }
            out = toStringInt(v);
            break;
        }
        case WireType::INT32: {
            int32_t v = 0;
            if (val.data.size() >= sizeof(int32_t)) {
                std::memcpy(&v, val.data.data(), sizeof(int32_t));
            }
            out = toStringInt(v);
            break;
        }
        case WireType::INT64: {
            int64_t v = 0;
            if (val.data.size() >= sizeof(int64_t)) {
                std::memcpy(&v, val.data.data(), sizeof(int64_t));
            }
            out = toStringInt(v);
            break;
        }
        case WireType::FLOAT32: {
            float v = 0.0f;
            if (val.data.size() >= sizeof(float)) {
                std::memcpy(&v, val.data.data(), sizeof(float));
            }
            out = toStringInt(static_cast<double>(v));
            break;
        }
        case WireType::FLOAT64: {
            double v = 0.0;
            if (val.data.size() >= sizeof(double)) {
                std::memcpy(&v, val.data.data(), sizeof(double));
            }
            out = toStringInt(v);
            break;
        }
        case WireType::BYTEA: {
            out = hexEncode(val.data);
            break;
        }
        default: {
            out.assign(reinterpret_cast<const char*>(val.data.data()),
                       val.data.size());
            break;
        }
    }

    return std::vector<uint8_t>(out.begin(), out.end());
}

template <typename T>
static void appendBigEndian(std::vector<uint8_t>& out, T value) {
    for (int i = sizeof(T) - 1; i >= 0; --i) {
        out.push_back(static_cast<uint8_t>((static_cast<uint64_t>(value) >> (i * 8)) & 0xFF));
    }
}

std::vector<uint8_t> PostgresqlAdapter::encodeBinaryValue(const ProtocolCodec::ColumnValue& val,
                                                          WireType type,
                                                          bool& ok) {
    ok = true;
    if (val.is_null) {
        return {};
    }
    std::vector<uint8_t> out;
    switch (type) {
        case WireType::BOOLEAN: {
            out.push_back(!val.data.empty() && val.data[0] != 0 ? 1 : 0);
            break;
        }
        case WireType::INT16: {
            int16_t v = 0;
            if (val.data.size() >= sizeof(int16_t)) {
                std::memcpy(&v, val.data.data(), sizeof(int16_t));
            }
            appendBigEndian<int16_t>(out, v);
            break;
        }
        case WireType::INT32: {
            int32_t v = 0;
            if (val.data.size() >= sizeof(int32_t)) {
                std::memcpy(&v, val.data.data(), sizeof(int32_t));
            }
            appendBigEndian<int32_t>(out, v);
            break;
        }
        case WireType::INT64: {
            int64_t v = 0;
            if (val.data.size() >= sizeof(int64_t)) {
                std::memcpy(&v, val.data.data(), sizeof(int64_t));
            }
            appendBigEndian<int64_t>(out, v);
            break;
        }
        case WireType::FLOAT32: {
            uint32_t bits = 0;
            if (val.data.size() >= sizeof(uint32_t)) {
                std::memcpy(&bits, val.data.data(), sizeof(uint32_t));
            }
            appendBigEndian<uint32_t>(out, bits);
            break;
        }
        case WireType::FLOAT64: {
            uint64_t bits = 0;
            if (val.data.size() >= sizeof(uint64_t)) {
                std::memcpy(&bits, val.data.data(), sizeof(uint64_t));
            }
            appendBigEndian<uint64_t>(out, bits);
            break;
        }
        case WireType::BYTEA:
        case WireType::VARCHAR:
        case WireType::CHAR:
        case WireType::JSON:
        case WireType::JSONB:
        case WireType::XML: {
            out = val.data;
            break;
        }
        default: {
            ok = false;
            break;
        }
    }
    return out;
}

std::string PostgresqlAdapter::decodeBinaryParamToText(WireType type,
                                                       const uint8_t* data,
                                                       size_t len) {
    if (data == nullptr || len == 0) {
        return {};
    }
    auto readBE16 = [&](const uint8_t* p) -> int16_t {
        return static_cast<int16_t>((p[0] << 8) | p[1]);
    };
    auto readBE32 = [&](const uint8_t* p) -> int32_t {
        return (static_cast<int32_t>(p[0]) << 24) |
               (static_cast<int32_t>(p[1]) << 16) |
               (static_cast<int32_t>(p[2]) << 8) |
               static_cast<int32_t>(p[3]);
    };
    auto readBE64 = [&](const uint8_t* p) -> int64_t {
        uint64_t v = 0;
        for (int i = 0; i < 8; ++i) {
            v = (v << 8) | p[i];
        }
        return static_cast<int64_t>(v);
    };

    switch (type) {
        case WireType::BOOLEAN:
            return data[0] ? "t" : "f";
        case WireType::INT16:
            if (len >= 2) return std::to_string(readBE16(data));
            break;
        case WireType::INT32:
            if (len >= 4) return std::to_string(readBE32(data));
            break;
        case WireType::INT64:
            if (len >= 8) return std::to_string(readBE64(data));
            break;
        case WireType::FLOAT32:
            if (len >= 4) {
                uint32_t bits = static_cast<uint32_t>(readBE32(data));
                float f;
                std::memcpy(&f, &bits, sizeof(float));
                return std::to_string(static_cast<double>(f));
            }
            break;
        case WireType::FLOAT64:
            if (len >= 8) {
                uint64_t bits = static_cast<uint64_t>(readBE64(data));
                double d;
                std::memcpy(&d, &bits, sizeof(double));
                return std::to_string(d);
            }
            break;
        case WireType::BYTEA:
            return hexEncode(std::vector<uint8_t>(data, data + len));
        default:
            break;
    }
    return std::string(reinterpret_cast<const char*>(data), len);
}

std::string PostgresqlAdapter::escapeLiteral(const std::string& input) {
    std::string out;
    out.reserve(input.size() + 8);
    for (char c : input) {
        out.push_back(c);
        if (c == '\'') {
            out.push_back('\'');
        }
    }
    return out;
}

std::string PostgresqlAdapter::substitutePositionalParameters(const QueryContext& query) {
    if (query.parameter_values.empty()) {
        return query.query;
    }

    std::string sql = query.query;
    size_t param_count = query.parameter_values.size();

    auto literal_for_index = [&](size_t idx) -> std::string {
        if (idx >= query.parameter_values.size()) {
            return "NULL";
        }
        if (!query.parameter_nulls.empty() && query.parameter_nulls[idx]) {
            return "NULL";
        }
        return "'" + escapeLiteral(query.parameter_values[idx]) + "'";
    };

    // Scan and replace while skipping quoted literals and identifiers
    for (size_t i = param_count; i >= 1; --i) {
        std::string placeholder = "$" + std::to_string(i);
        std::string replacement = literal_for_index(i - 1);

        std::string rewritten;
        rewritten.reserve(sql.size());
        enum class ScanState { Normal, SingleQuote, DoubleQuote, DollarQuote };
        ScanState state = ScanState::Normal;
        std::string dollar_tag;

        for (size_t pos = 0; pos < sql.size();) {
            char c = sql[pos];
            if (state == ScanState::Normal) {
                if (c == '\'') {
                    state = ScanState::SingleQuote;
                    rewritten.push_back(c);
                    ++pos;
                    continue;
                }
                if (c == '"') {
                    state = ScanState::DoubleQuote;
                    rewritten.push_back(c);
                    ++pos;
                    continue;
                }
                if (c == '$') {
                    // Check for dollar-quote start
                    size_t tag_end = sql.find('$', pos + 1);
                    if (tag_end != std::string::npos && tag_end > pos + 1) {
                        dollar_tag = sql.substr(pos, tag_end - pos + 1);
                        state = ScanState::DollarQuote;
                        rewritten.append(dollar_tag);
                        pos = tag_end + 1;
                        continue;
                    }
                }
                // Placeholder replacement
                if (sql.compare(pos, placeholder.size(), placeholder) == 0) {
                    rewritten.append(replacement);
                    pos += placeholder.size();
                    continue;
                }
                rewritten.push_back(c);
                ++pos;
            } else if (state == ScanState::SingleQuote) {
                rewritten.push_back(c);
                ++pos;
                if (c == '\'' && !(pos < sql.size() && sql[pos] == '\'')) {
                    state = ScanState::Normal;
                } else if (c == '\'' && pos < sql.size() && sql[pos] == '\'') {
                    // Escaped single quote
                    rewritten.push_back(sql[pos]);
                    ++pos;
                }
            } else if (state == ScanState::DoubleQuote) {
                rewritten.push_back(c);
                ++pos;
                if (c == '"') {
                    state = ScanState::Normal;
                }
            } else if (state == ScanState::DollarQuote) {
                if (!dollar_tag.empty() && sql.compare(pos, dollar_tag.size(), dollar_tag) == 0) {
                    rewritten.append(dollar_tag);
                    pos += dollar_tag.size();
                    state = ScanState::Normal;
                } else {
                    rewritten.push_back(c);
                    ++pos;
                }
            }
        }
        sql.swap(rewritten);
        if (i == 1) break;  // prevent underflow
    }
    return sql;
}

void PostgresqlAdapter::sendRowDescription(network::Connection* conn,
                                            const std::vector<ProtocolCodec::ColumnInfo>& columns,
                                            const std::vector<int16_t>& formats) {
    std::vector<uint8_t> payload;

    // Number of fields
    writeInt16(payload, static_cast<int16_t>(columns.size()));

    for (size_t i = 0; i < columns.size(); ++i) {
        const auto& col = columns[i];
        // Field name
        writeString(payload, col.name);

        // Table OID (0 = not from a table)
        writeInt32(payload, 0);

        // Column number (0 = not from a table)
        writeInt16(payload, 0);

        // Type OID
        writeInt32(payload, wireTypeToOid(col.type));

        // Type size
        writeInt16(payload, -1);  // Variable

        // Type modifier
        writeInt32(payload, static_cast<int32_t>(col.type_modifier));

        // Format code (0 = text, 1 = binary)
        writeInt16(payload, selectFormatForColumn(i, formats, col.type));
    }

    sendMessage(conn, pg::BackendMsg::ROW_DESCRIPTION, payload);
}

void PostgresqlAdapter::sendDataRow(network::Connection* conn,
                                     const std::vector<ProtocolCodec::ColumnInfo>& columns,
                                     const std::vector<ProtocolCodec::ColumnValue>& values,
                                     const std::vector<int16_t>& formats) {
    std::vector<uint8_t> payload;

    // Number of columns
    writeInt16(payload, static_cast<int16_t>(values.size()));

    for (size_t i = 0; i < values.size(); ++i) {
        const auto& val = values[i];
        WireType type = i < columns.size() ? columns[i].type : WireType::UNKNOWN;
        int16_t format = selectFormatForColumn(i, formats, type);

        if (val.is_null) {
            // NULL value
            writeInt32(payload, -1);
        } else {
            std::vector<uint8_t> encoded;
            if (format == 1) {
                bool ok = false;
                encoded = encodeBinaryValue(val, type, ok);
                if (!ok) {
                    // Fallback to text if binary not supported for this type/value
                    encoded = encodeTextValue(val, type);
                    format = 0;
                }
            } else {
                encoded = encodeTextValue(val, type);
            }
            writeInt32(payload, static_cast<int32_t>(encoded.size()));
            if (!encoded.empty()) {
                writeBytes(payload, encoded.data(), encoded.size());
            }
        }
    }

    sendMessage(conn, pg::BackendMsg::DATA_ROW, payload);
}

void PostgresqlAdapter::sendCommandComplete(network::Connection* conn, const std::string& tag) {
    std::vector<uint8_t> payload;
    writeString(payload, tag);
    sendMessage(conn, pg::BackendMsg::COMMAND_COMPLETE, payload);
}

void PostgresqlAdapter::sendEmptyQueryResponse(network::Connection* conn) {
    sendMessage(conn, pg::BackendMsg::EMPTY_QUERY_RESPONSE, {});
}

void PostgresqlAdapter::sendParseComplete(network::Connection* conn) {
    sendMessage(conn, pg::BackendMsg::PARSE_COMPLETE, {});
}

void PostgresqlAdapter::sendBindComplete(network::Connection* conn) {
    sendMessage(conn, pg::BackendMsg::BIND_COMPLETE, {});
}

void PostgresqlAdapter::sendCloseComplete(network::Connection* conn) {
    sendMessage(conn, pg::BackendMsg::CLOSE_COMPLETE, {});
}

void PostgresqlAdapter::sendNoData(network::Connection* conn) {
    sendMessage(conn, pg::BackendMsg::NO_DATA, {});
}

void PostgresqlAdapter::sendParameterDescription(network::Connection* conn,
                                                  const std::vector<int32_t>& param_types) {
    std::vector<uint8_t> payload;

    writeInt16(payload, static_cast<int16_t>(param_types.size()));
    for (int32_t oid : param_types) {
        writeInt32(payload, oid);
    }

    sendMessage(conn, pg::BackendMsg::PARAMETER_DESCRIPTION, payload);
}

void PostgresqlAdapter::sendPortalSuspended(network::Connection* conn) {
    sendMessage(conn, pg::BackendMsg::PORTAL_SUSPENDED, {});
}

void PostgresqlAdapter::sendErrorResponse(network::Connection* conn,
                                           const std::string& severity,
                                           const std::string& sqlstate,
                                           const std::string& message,
                                           const std::string& detail,
                                           const std::string& hint) {
    std::vector<uint8_t> payload;

    // Severity
    writeByte(payload, static_cast<uint8_t>(pg::ErrorField::SEVERITY));
    writeString(payload, severity);

    // SQLSTATE
    writeByte(payload, static_cast<uint8_t>(pg::ErrorField::CODE));
    writeString(payload, sqlstate.empty() ? "42000" : sqlstate);

    // Message
    writeByte(payload, static_cast<uint8_t>(pg::ErrorField::MESSAGE));
    writeString(payload, message);

    // Detail (optional)
    if (!detail.empty()) {
        writeByte(payload, static_cast<uint8_t>(pg::ErrorField::DETAIL));
        writeString(payload, detail);
    }

    // Hint (optional)
    if (!hint.empty()) {
        writeByte(payload, static_cast<uint8_t>(pg::ErrorField::HINT));
        writeString(payload, hint);
    }

    // Terminator
    writeByte(payload, 0);

    sendMessage(conn, pg::BackendMsg::ERROR_RESPONSE, payload);
}

void PostgresqlAdapter::sendNoticeResponse(network::Connection* conn,
                                            const std::string& severity,
                                            const std::string& message) {
    std::vector<uint8_t> payload;

    writeByte(payload, static_cast<uint8_t>(pg::ErrorField::SEVERITY));
    writeString(payload, severity);

    writeByte(payload, static_cast<uint8_t>(pg::ErrorField::MESSAGE));
    writeString(payload, message);

    writeByte(payload, 0);  // Terminator

    sendMessage(conn, pg::BackendMsg::NOTICE_RESPONSE, payload);
}

void PostgresqlAdapter::sendCopyInResponse(network::Connection* conn,
                                            int8_t overall_format,
                                            const std::vector<int16_t>& column_formats) {
    std::vector<uint8_t> payload;

    writeByte(payload, static_cast<uint8_t>(overall_format));
    writeInt16(payload, static_cast<int16_t>(column_formats.size()));
    for (int16_t fmt : column_formats) {
        writeInt16(payload, fmt);
    }

    sendMessage(conn, pg::BackendMsg::COPY_IN_RESPONSE, payload);
}

void PostgresqlAdapter::sendCopyOutResponse(network::Connection* conn,
                                             int8_t overall_format,
                                             const std::vector<int16_t>& column_formats) {
    std::vector<uint8_t> payload;

    writeByte(payload, static_cast<uint8_t>(overall_format));
    writeInt16(payload, static_cast<int16_t>(column_formats.size()));
    for (int16_t fmt : column_formats) {
        writeInt16(payload, fmt);
    }

    sendMessage(conn, pg::BackendMsg::COPY_OUT_RESPONSE, payload);
}

void PostgresqlAdapter::sendCopyData(network::Connection* conn, const void* data, size_t len) {
    std::vector<uint8_t> payload(static_cast<const uint8_t*>(data),
                                  static_cast<const uint8_t*>(data) + len);
    sendMessage(conn, pg::BackendMsg::COPY_DATA, payload);
}

void PostgresqlAdapter::sendCopyDone(network::Connection* conn) {
    sendMessage(conn, pg::BackendMsg::COPY_DONE, {});
}

// ============================================================================
// Helper Methods
// ============================================================================

void PostgresqlAdapter::writeInt32(std::vector<uint8_t>& buf, int32_t value) {
    // Network byte order (big-endian)
    buf.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    buf.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>(value & 0xFF));
}

void PostgresqlAdapter::writeInt16(std::vector<uint8_t>& buf, int16_t value) {
    buf.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>(value & 0xFF));
}

void PostgresqlAdapter::writeByte(std::vector<uint8_t>& buf, uint8_t value) {
    buf.push_back(value);
}

void PostgresqlAdapter::writeString(std::vector<uint8_t>& buf, const std::string& str) {
    buf.insert(buf.end(), str.begin(), str.end());
    buf.push_back(0);  // Null terminator
}

void PostgresqlAdapter::writeBytes(std::vector<uint8_t>& buf, const void* data, size_t len) {
    const uint8_t* ptr = static_cast<const uint8_t*>(data);
    buf.insert(buf.end(), ptr, ptr + len);
}

int32_t PostgresqlAdapter::readInt32(const uint8_t* data) {
    return (static_cast<int32_t>(data[0]) << 24) |
           (static_cast<int32_t>(data[1]) << 16) |
           (static_cast<int32_t>(data[2]) << 8) |
           static_cast<int32_t>(data[3]);
}

int16_t PostgresqlAdapter::readInt16(const uint8_t* data) {
    return static_cast<int16_t>((static_cast<int16_t>(data[0]) << 8) |
                                 static_cast<int16_t>(data[1]));
}

std::string PostgresqlAdapter::readString(const uint8_t* data, size_t max_len) {
    std::string result;
    for (size_t i = 0; i < max_len && data[i] != 0; ++i) {
        result.push_back(static_cast<char>(data[i]));
    }
    return result;
}

int32_t PostgresqlAdapter::wireTypeToOid(WireType type) {
    switch (type) {
        case WireType::BOOLEAN: return pg::TypeOid::BOOL;
        case WireType::INT16: return pg::TypeOid::INT2;
        case WireType::INT32: return pg::TypeOid::INT4;
        case WireType::INT64: return pg::TypeOid::INT8;
        case WireType::FLOAT32: return pg::TypeOid::FLOAT4;
        case WireType::FLOAT64: return pg::TypeOid::FLOAT8;
        case WireType::DECIMAL: return pg::TypeOid::NUMERIC;
        case WireType::VARCHAR: return pg::TypeOid::VARCHAR;
        case WireType::CHAR: return pg::TypeOid::CHAR;
        case WireType::BYTEA: return pg::TypeOid::BYTEA;
        case WireType::DATE: return pg::TypeOid::DATE;
        case WireType::TIME: return pg::TypeOid::TIME;
        case WireType::TIMESTAMP: return pg::TypeOid::TIMESTAMP;
        case WireType::TIMESTAMPTZ: return pg::TypeOid::TIMESTAMPTZ;
        case WireType::INTERVAL: return pg::TypeOid::INTERVAL;
        case WireType::UUID: return pg::TypeOid::UUID;
        case WireType::JSON: return pg::TypeOid::JSON;
        case WireType::JSONB: return pg::TypeOid::JSONB;
        default: return pg::TypeOid::TEXT;
    }
}

WireType PostgresqlAdapter::oidToWireType(int32_t oid) {
    switch (oid) {
        case pg::TypeOid::BOOL: return WireType::BOOLEAN;
        case pg::TypeOid::INT2: return WireType::INT16;
        case pg::TypeOid::INT4: return WireType::INT32;
        case pg::TypeOid::INT8: return WireType::INT64;
        case pg::TypeOid::FLOAT4: return WireType::FLOAT32;
        case pg::TypeOid::FLOAT8: return WireType::FLOAT64;
        case pg::TypeOid::NUMERIC: return WireType::DECIMAL;
        case pg::TypeOid::VARCHAR: return WireType::VARCHAR;
        case pg::TypeOid::CHAR: return WireType::CHAR;
        case pg::TypeOid::TEXT: return WireType::VARCHAR;
        case pg::TypeOid::BYTEA: return WireType::BYTEA;
        case pg::TypeOid::DATE: return WireType::DATE;
        case pg::TypeOid::TIME: return WireType::TIME;
        case pg::TypeOid::TIMESTAMP: return WireType::TIMESTAMP;
        case pg::TypeOid::TIMESTAMPTZ: return WireType::TIMESTAMPTZ;
        case pg::TypeOid::INTERVAL: return WireType::INTERVAL;
        case pg::TypeOid::UUID: return WireType::UUID;
        case pg::TypeOid::JSON: return WireType::JSON;
        case pg::TypeOid::JSONB: return WireType::JSONB;
        default: return WireType::VARCHAR;
    }
}

void PostgresqlAdapter::mapStatusToSqlstate(uint32_t status, std::string& sqlstate) {
    switch (static_cast<core::Status>(status)) {
        case core::Status::SYNTAX_ERROR:
        case core::Status::INVALID_ARGUMENT:
            sqlstate = "42601";
            break;
        case core::Status::UNDEFINED_TABLE:
        case core::Status::NOT_FOUND:
            sqlstate = "42P01";
            break;
        case core::Status::UNDEFINED_COLUMN:
            sqlstate = "42703";
            break;
        case core::Status::CONSTRAINT_VIOLATION:
        case core::Status::UNIQUE_VIOLATION:
            sqlstate = "23505";
            break;
        case core::Status::FOREIGN_KEY_VIOLATION:
            sqlstate = "23503";
            break;
        case core::Status::CHECK_VIOLATION:
            sqlstate = "23514";
            break;
        case core::Status::NOT_NULL_VIOLATION:
        case core::Status::NULL_VALUE_NOT_ALLOWED:
            sqlstate = "23502";
            break;
        case core::Status::PERMISSION_DENIED:
        case core::Status::INSUFFICIENT_PRIVILEGE:
            sqlstate = "42501";
            break;
        case core::Status::INVALID_CURSOR_NAME:
        case core::Status::CURSOR_NOT_FOUND:
            sqlstate = "34000";
            break;
        case core::Status::INVALID_CURSOR_STATE:
        case core::Status::CURSOR_NOT_OPEN:
        case core::Status::CURSOR_ALREADY_OPEN:
            sqlstate = "24000";
            break;
        case core::Status::READ_ONLY_TRANSACTION:
            sqlstate = "25006";
            break;
        case core::Status::INVALID_TRANSACTION_STATE:
        case core::Status::NO_ACTIVE_TRANSACTION:
            sqlstate = "25P01";
            break;
        case core::Status::DEADLOCK:
            sqlstate = "40P01";
            break;
        case core::Status::SERIALIZATION_FAILURE:
            sqlstate = "40001";
            break;
        case core::Status::LOCK_TIMEOUT:
        case core::Status::LOCK_NOT_AVAILABLE:
            sqlstate = "55P03";
            break;
        case core::Status::QUERY_CANCELED:
            sqlstate = "57014";
            break;
        case core::Status::TOO_MANY_CONNECTIONS:
            sqlstate = "53300";
            break;
        case core::Status::NUMERIC_VALUE_OUT_OF_RANGE:
        case core::Status::OUT_OF_RANGE:
            sqlstate = "22003";
            break;
        case core::Status::STRING_DATA_RIGHT_TRUNCATION:
            sqlstate = "22001";
            break;
        case core::Status::DIVISION_BY_ZERO:
            sqlstate = "22012";
            break;
        case core::Status::INVALID_TEXT_REPRESENTATION:
            sqlstate = "22P02";
            break;
        case core::Status::DATETIME_FIELD_OVERFLOW:
            sqlstate = "22008";
            break;
        case core::Status::INVALID_DATETIME_FORMAT:
            sqlstate = "22007";
            break;
        case core::Status::DATATYPE_MISMATCH:
            sqlstate = "42804";
            break;
        case core::Status::NOT_IMPLEMENTED:
        case core::Status::NOT_SUPPORTED:
            sqlstate = "0A000";
            break;
        default:
            if (sqlstate.empty()) {
                sqlstate = "XX000";
            }
            break;
    }
}

void PostgresqlAdapter::updateTransactionStatus(const std::string& sql, bool has_error) {
    auto ltrim_upper = [](const std::string& input) {
        size_t pos = 0;
        while (pos < input.size() && std::isspace(static_cast<unsigned char>(input[pos]))) {
            ++pos;
        }
        std::string upper;
        upper.reserve(input.size() - pos);
        for (; pos < input.size(); ++pos) {
            upper.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(input[pos]))));
        }
        return upper;
    };

    std::string normalized = ltrim_upper(sql);
    auto starts_with = [&](const std::string& prefix) {
        return normalized.rfind(prefix, 0) == 0;
    };

    if (starts_with("BEGIN") || starts_with("START TRANSACTION")) {
        in_transaction_ = true;
        txn_failed_ = false;
        return;
    }
    if (starts_with("COMMIT")) {
        in_transaction_ = false;
        txn_failed_ = false;
        return;
    }
    if (starts_with("ROLLBACK")) {
        in_transaction_ = false;
        txn_failed_ = false;
        return;
    }

    if (has_error && in_transaction_) {
        txn_failed_ = true;
        return;
    }

    if (!in_transaction_) {
        txn_failed_ = false;
    }
}

std::string PostgresqlAdapter::computeMD5Hash(const std::string& password,
                                               const std::string& username,
                                               const uint8_t salt[4]) {
    // MD5 auth: md5(md5(password + username) + salt)
    // For testing, just return a placeholder
    // TODO: Implement proper MD5 hash computation

#ifdef HAVE_OPENSSL
    // First hash: MD5(password + username)
    std::string input1 = password + username;
    unsigned char hash1[MD5_DIGEST_LENGTH];
    MD5(reinterpret_cast<const unsigned char*>(input1.c_str()), input1.length(), hash1);

    // Convert to hex
    std::stringstream ss1;
    for (int i = 0; i < MD5_DIGEST_LENGTH; ++i) {
        ss1 << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(hash1[i]);
    }
    std::string hex1 = ss1.str();

    // Second hash: MD5(hex1 + salt)
    std::string input2 = hex1 + std::string(reinterpret_cast<const char*>(salt), 4);
    unsigned char hash2[MD5_DIGEST_LENGTH];
    MD5(reinterpret_cast<const unsigned char*>(input2.c_str()), input2.length(), hash2);

    // Convert to hex and prepend "md5"
    std::stringstream ss2;
    ss2 << "md5";
    for (int i = 0; i < MD5_DIGEST_LENGTH; ++i) {
        ss2 << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(hash2[i]);
    }
    return ss2.str();
#else
    // Fallback: simple hash for testing
    (void)password;
    (void)username;
    (void)salt;
    return "md5" + std::string(32, '0');
#endif
}

} // namespace protocol
} // namespace scratchbird
