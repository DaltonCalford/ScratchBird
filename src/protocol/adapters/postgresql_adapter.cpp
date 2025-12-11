/**
 * PostgreSQL Wire Protocol Adapter Implementation
 *
 * ScratchBird Network Layer - Phase 3.2
 *
 * Implements PostgreSQL v3 wire protocol.
 */

#include "scratchbird/protocol/adapters/postgresql_adapter.h"
#include "scratchbird/core/error_context.h"

#include <cstring>
#include <random>
#include <sstream>
#include <iomanip>

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

        // If there's a row callback, call it to get rows
        if (result.row_callback) {
            // The callback will call sendDataRow for each row
            // For now, just send completion
        }
    }

    // Send command complete
    sendCommandComplete(conn, result.command_tag);

    return core::Status::OK;
}

core::Status PostgresqlAdapter::sendProtocolError(network::Connection* conn,
                                                   uint32_t /*error_code*/,
                                                   const std::string& sqlstate,
                                                   const std::string& message,
                                                   const std::string& detail,
                                                   const std::string& hint) {
    sendErrorResponse(conn, "ERROR", sqlstate, message, detail, hint);
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
    auto status = executeQuery(ctx, result);

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

    for (int16_t i = 0; i < num_params; ++i) {
        if (offset + 4 > current_msg_data_.size()) break;
        int32_t len = readInt32(current_msg_data_.data() + offset);
        offset += 4;

        if (len == -1) {
            // NULL
            portal.param_values.emplace_back("");
            portal.param_nulls.push_back(true);
        } else {
            // Value
            if (offset + static_cast<size_t>(len) > current_msg_data_.size()) break;
            portal.param_values.emplace_back(
                reinterpret_cast<const char*>(current_msg_data_.data() + offset), len);
            portal.param_nulls.push_back(false);
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
            sendRowDescription(conn, stmt_it->second.result_columns);
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

    // Execute
    QueryContext ctx;
    ctx.query = stmt_it->second.query;
    ctx.statement_name = portal.statement_name;
    ctx.portal_name = portal_name;
    ctx.parameter_values = portal.param_values;
    ctx.parameter_nulls = portal.param_nulls;
    ctx.max_rows = max_rows;

    ResultContext result;
    executeQuery(ctx, result);

    if (result.has_error) {
        sendProtocolError(conn, result.error_code, result.sqlstate,
                  result.error_message, result.error_detail, result.error_hint);
    } else {
        // Send results
        if (!result.columns.empty()) {
            // Rows would be sent here via row_callback
        }
        sendCommandComplete(conn, result.command_tag);
    }

    portal.executed = true;
    pending_operations_.push_back('E');

    return core::Status::OK;
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
    char status = in_transaction_ ? pg::TransactionStatus::IN_TRANSACTION : pg::TransactionStatus::IDLE;
    writeByte(payload, static_cast<uint8_t>(status));
    sendMessage(conn, pg::BackendMsg::READY_FOR_QUERY, payload);
}

void PostgresqlAdapter::sendRowDescription(network::Connection* conn,
                                            const std::vector<ProtocolCodec::ColumnInfo>& columns) {
    std::vector<uint8_t> payload;

    // Number of fields
    writeInt16(payload, static_cast<int16_t>(columns.size()));

    for (const auto& col : columns) {
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
        writeInt16(payload, 0);  // Text format
    }

    sendMessage(conn, pg::BackendMsg::ROW_DESCRIPTION, payload);
}

void PostgresqlAdapter::sendDataRow(network::Connection* conn,
                                     const std::vector<ProtocolCodec::ColumnValue>& values) {
    std::vector<uint8_t> payload;

    // Number of columns
    writeInt16(payload, static_cast<int16_t>(values.size()));

    for (const auto& val : values) {
        if (val.is_null) {
            // NULL value
            writeInt32(payload, -1);
        } else {
            // Value length
            writeInt32(payload, static_cast<int32_t>(val.data.size()));
            // Value data
            writeBytes(payload, val.data.data(), val.data.size());
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
