/**
 * Native ScratchBird Protocol Adapter Implementation
 *
 * ScratchBird Network Layer - Phase 3.2
 *
 * Implements the native ScratchBird wire protocol.
 */

#include "scratchbird/protocol/adapters/native_adapter.h"
#include "scratchbird/core/error_context.h"

#include <cstring>

namespace scratchbird {
namespace protocol {

// ============================================================================
// Constructor/Destructor
// ============================================================================

NativeAdapter::NativeAdapter(const ProtocolAdapterConfig& config)
    : ProtocolAdapter(config) {
    // Generate session ID
    generateSessionId(session_id_);
}

NativeAdapter::~NativeAdapter() = default;

// ============================================================================
// ProtocolAdapter Implementation
// ============================================================================

core::Status NativeAdapter::parseMessage(network::Connection* conn) {
    const auto& buffer = conn->getReadBuffer();

    // Native protocol: MessageHeader (12 bytes) + payload
    if (buffer.size() < sizeof(MessageHeader)) {
        return core::Status::IO_ERROR;  // Need more data
    }

    // Parse header
    MessageHeader header;
    std::memcpy(&header, buffer.data(), sizeof(MessageHeader));

    // Validate magic
    if (header.magic != PROTOCOL_MAGIC) {
        return core::Status::INVALID_ARGUMENT;
    }

    // Validate version
    if (header.version != PROTOCOL_VERSION) {
        return core::Status::NOT_SUPPORTED;
    }

    // Check payload length
    if (header.payload_length > config_.max_message_size) {
        return core::Status::INVALID_ARGUMENT;
    }

    size_t total_length = sizeof(MessageHeader) + header.payload_length;
    if (buffer.size() < total_length) {
        return core::Status::IO_ERROR;  // Need more data
    }

    // Parse complete message
    current_message_ = Message(static_cast<MessageType>(header.type));
    if (header.payload_length > 0) {
        const uint8_t* payload_start = buffer.data() + sizeof(MessageHeader);
        current_message_.setPayload(payload_start, header.payload_length);
    }

    // Consume from read buffer
    conn->consumeReadBuffer(total_length);

    return core::Status::OK;
}

core::Status NativeAdapter::processMessage(network::Connection* conn) {
    bytes_received_ += sizeof(MessageHeader) + current_message_.getPayloadLength();

    MessageType type = current_message_.getType();

    switch (type) {
        case MessageType::CONNECT_REQUEST:
            return handleConnectRequest(conn);

        case MessageType::DISCONNECT:
            return handleDisconnect(conn);

        case MessageType::AUTH_REQUEST:
            return handleAuthRequest(conn);

        case MessageType::QUERY:
            return handleQuery(conn);

        case MessageType::QUERY_CANCEL:
            return handleQueryCancel(conn);

        case MessageType::PREPARE:
            return handlePrepare(conn);

        case MessageType::EXECUTE:
            return handleExecute(conn);

        case MessageType::CLOSE_STATEMENT:
            return handleCloseStatement(conn);

        case MessageType::DESCRIBE:
            return handleDescribe(conn);

        case MessageType::BEGIN_TRANSACTION:
            return handleBeginTransaction(conn);

        case MessageType::COMMIT:
            return handleCommit(conn);

        case MessageType::ROLLBACK:
            return handleRollback(conn);

        case MessageType::SAVEPOINT:
            return handleSavepoint(conn);

        case MessageType::RELEASE_SAVEPOINT:
            return handleReleaseSavepoint(conn);

        case MessageType::ROLLBACK_TO:
            return handleRollbackTo(conn);

        case MessageType::PING:
            return handlePing(conn);

        case MessageType::STATUS_REQUEST:
            return handleStatusRequest(conn);

        default:
            sendQueryError(conn, static_cast<uint32_t>(core::Status::NOT_SUPPORTED),
                          "HY000", "Unknown message type: " + std::to_string(static_cast<int>(type)));
            return sendBuffer(conn);
    }
}

core::Status NativeAdapter::sendGreeting(network::Connection* /*conn*/) {
    // Native protocol: server waits for client CONNECT_REQUEST
    return core::Status::OK;
}

core::Status NativeAdapter::processAuthentication(network::Connection* /*conn*/) {
    // Authentication handled in handleAuthRequest
    return core::Status::OK;
}

core::Status NativeAdapter::sendAuthResult(network::Connection* conn,
                                            bool success,
                                            const std::string& error_msg) {
    sendAuthResponse(conn, success, error_msg);
    if (success) {
        native_state_ = NativeProtocolState::READY;
    }
    return sendBuffer(conn);
}

core::Status NativeAdapter::sendQueryResult(network::Connection* conn,
                                             const ResultContext& result) {
    if (result.has_error) {
        sendQueryError(conn, result.error_code, result.sqlstate, result.error_message);
        return core::Status::OK;
    }

    if (!result.columns.empty()) {
        // Send row description
        sendRowDescription(conn, result.columns);

        // Rows would be sent via row_callback
        // For now, just send end of results
    }

    // Send command complete
    sendCommandComplete(conn, result.command_tag, result.rows_affected);

    // Send end of results
    sendEndOfResults(conn);

    return core::Status::OK;
}

core::Status NativeAdapter::sendProtocolError(network::Connection* conn,
                                               uint32_t error_code,
                                               const std::string& sqlstate,
                                               const std::string& message,
                                               const std::string& /*detail*/,
                                               const std::string& /*hint*/) {
    sendQueryError(conn, error_code, sqlstate, message);
    return core::Status::OK;
}

// ============================================================================
// Message Handling
// ============================================================================

core::Status NativeAdapter::handleConnectRequest(network::Connection* conn) {
    // Parse connect request
    current_message_.resetReadPosition();

    // Client version
    client_version_ = current_message_.readUInt16();

    // Database name (length-prefixed string)
    database_name_ = current_message_.readLengthPrefixedString();

    // Username (length-prefixed string)
    username_ = current_message_.readLengthPrefixedString();

    // Application name (optional)
    if (current_message_.getRemainingBytes() > 0) {
        std::string app_name = current_message_.readLengthPrefixedString();
        (void)app_name;  // Store if needed
    }

    // Send connect response
    sendConnectResponse(conn, true);
    native_state_ = NativeProtocolState::AUTHENTICATING;

    return sendBuffer(conn);
}

core::Status NativeAdapter::handleDisconnect(network::Connection* conn) {
    native_state_ = NativeProtocolState::CLOSING;
    conn->close(network::CloseReason::CLIENT_DISCONNECT);
    return core::Status::OK;
}

core::Status NativeAdapter::handleAuthRequest(network::Connection* conn) {
    // Parse auth request
    current_message_.resetReadPosition();

    // Auth method
    uint8_t auth_method = current_message_.readUInt8();
    (void)auth_method;

    // Auth data (password or token)
    std::string auth_data = current_message_.readLengthPrefixedString();
    (void)auth_data;

    // For testing, accept any authentication
    // TODO: Implement proper authentication
    return sendAuthResult(conn, true);
}

core::Status NativeAdapter::handleQuery(network::Connection* conn) {
    native_state_ = NativeProtocolState::QUERY_PROCESSING;

    // Parse query
    current_message_.resetReadPosition();
    std::string query = current_message_.readLengthPrefixedString();

    // Execute query
    QueryContext ctx;
    ctx.query = query;

    ResultContext result;
    executeQuery(ctx, result);

    sendQueryResult(conn, result);
    native_state_ = NativeProtocolState::READY;

    return sendBuffer(conn);
}

core::Status NativeAdapter::handleQueryCancel(network::Connection* conn) {
    // TODO: Implement query cancellation
    // For now, just acknowledge
    sendPong(conn);
    return sendBuffer(conn);
}

core::Status NativeAdapter::handlePrepare(network::Connection* conn) {
    current_message_.resetReadPosition();

    // Query to prepare
    std::string query = current_message_.readLengthPrefixedString();

    // Create prepared statement
    uint32_t stmt_id = next_stmt_id_++;
    native_prepared_statements_[stmt_id] = query;

    // Also register with base class
    std::vector<int32_t> param_types;
    prepareStatement(std::to_string(stmt_id), query, param_types);

    sendPrepareResponse(conn, stmt_id, true);
    return sendBuffer(conn);
}

core::Status NativeAdapter::handleExecute(network::Connection* conn) {
    native_state_ = NativeProtocolState::QUERY_PROCESSING;

    current_message_.resetReadPosition();

    // Statement ID
    uint32_t stmt_id = current_message_.readUInt32();

    auto it = native_prepared_statements_.find(stmt_id);
    if (it == native_prepared_statements_.end()) {
        sendQueryError(conn, static_cast<uint32_t>(core::Status::NOT_FOUND),
                      "26000", "Prepared statement not found");
        native_state_ = NativeProtocolState::READY;
        return sendBuffer(conn);
    }

    // Execute
    QueryContext ctx;
    ctx.query = it->second;
    ctx.statement_name = std::to_string(stmt_id);

    // Read parameters (if any)
    uint16_t param_count = current_message_.readUInt16();
    for (uint16_t i = 0; i < param_count; ++i) {
        bool is_null = current_message_.readUInt8() != 0;
        if (is_null) {
            ctx.parameter_values.push_back("");
            ctx.parameter_nulls.push_back(true);
        } else {
            ctx.parameter_values.push_back(current_message_.readLengthPrefixedString());
            ctx.parameter_nulls.push_back(false);
        }
    }

    ResultContext result;
    executeQuery(ctx, result);

    sendQueryResult(conn, result);
    native_state_ = NativeProtocolState::READY;

    return sendBuffer(conn);
}

core::Status NativeAdapter::handleCloseStatement(network::Connection* conn) {
    current_message_.resetReadPosition();

    uint32_t stmt_id = current_message_.readUInt32();
    native_prepared_statements_.erase(stmt_id);
    closePrepared(std::to_string(stmt_id));

    // No response for close
    (void)conn;
    return core::Status::OK;
}

core::Status NativeAdapter::handleDescribe(network::Connection* conn) {
    current_message_.resetReadPosition();

    uint32_t stmt_id = current_message_.readUInt32();

    auto it = native_prepared_statements_.find(stmt_id);
    if (it == native_prepared_statements_.end()) {
        sendQueryError(conn, static_cast<uint32_t>(core::Status::NOT_FOUND),
                      "26000", "Prepared statement not found");
        return sendBuffer(conn);
    }

    // For now, return empty description
    // TODO: Actually describe the statement
    std::vector<ProtocolCodec::ColumnInfo> columns;
    sendDescribeResponse(conn, stmt_id, columns, 0);

    return sendBuffer(conn);
}

core::Status NativeAdapter::handleBeginTransaction(network::Connection* conn) {
    auto status = beginTransaction();
    if (status != core::Status::OK) {
        sendQueryError(conn, static_cast<uint32_t>(status), "25000",
                      "Failed to begin transaction");
    } else {
        sendTransactionStatus(conn, true);
    }
    return sendBuffer(conn);
}

core::Status NativeAdapter::handleCommit(network::Connection* conn) {
    auto status = commitTransaction();
    if (status != core::Status::OK) {
        sendQueryError(conn, static_cast<uint32_t>(status), "25000",
                      "Failed to commit transaction");
    } else {
        sendTransactionStatus(conn, false);
    }
    return sendBuffer(conn);
}

core::Status NativeAdapter::handleRollback(network::Connection* conn) {
    auto status = rollbackTransaction();
    if (status != core::Status::OK) {
        sendQueryError(conn, static_cast<uint32_t>(status), "25000",
                      "Failed to rollback transaction");
    } else {
        sendTransactionStatus(conn, false);
    }
    return sendBuffer(conn);
}

core::Status NativeAdapter::handleSavepoint(network::Connection* conn) {
    current_message_.resetReadPosition();
    std::string name = current_message_.readLengthPrefixedString();

    auto status = savepoint(name);
    if (status != core::Status::OK) {
        sendQueryError(conn, static_cast<uint32_t>(status), "3B000",
                      "Failed to create savepoint");
    } else {
        sendTransactionStatus(conn, true);
    }
    return sendBuffer(conn);
}

core::Status NativeAdapter::handleReleaseSavepoint(network::Connection* conn) {
    current_message_.resetReadPosition();
    std::string name = current_message_.readLengthPrefixedString();

    auto status = releaseSavepoint(name);
    if (status != core::Status::OK) {
        sendQueryError(conn, static_cast<uint32_t>(status), "3B000",
                      "Failed to release savepoint");
    } else {
        sendTransactionStatus(conn, true);
    }
    return sendBuffer(conn);
}

core::Status NativeAdapter::handleRollbackTo(network::Connection* conn) {
    current_message_.resetReadPosition();
    std::string name = current_message_.readLengthPrefixedString();

    auto status = rollbackToSavepoint(name);
    if (status != core::Status::OK) {
        sendQueryError(conn, static_cast<uint32_t>(status), "3B000",
                      "Failed to rollback to savepoint");
    } else {
        sendTransactionStatus(conn, true);
    }
    return sendBuffer(conn);
}

core::Status NativeAdapter::handlePing(network::Connection* conn) {
    sendPong(conn);
    return sendBuffer(conn);
}

core::Status NativeAdapter::handleStatusRequest(network::Connection* conn) {
    sendStatusResponse(conn);
    return sendBuffer(conn);
}

// ============================================================================
// Message Sending
// ============================================================================

void NativeAdapter::sendMessage(network::Connection* conn, const Message& msg) {
    // Write header
    const MessageHeader& header = msg.getHeader();
    writeToBuffer(conn, &header, sizeof(MessageHeader));

    // Write payload
    if (msg.getPayloadLength() > 0) {
        writeToBuffer(conn, msg.getPayloadData(), msg.getPayloadLength());
    }
}

void NativeAdapter::sendConnectResponse(network::Connection* conn, bool success,
                                         const std::string& error_msg) {
    Message msg(MessageType::CONNECT_RESPONSE);

    // Success flag
    msg.writeUInt8(success ? 1 : 0);

    // Server version
    msg.writeUInt16(PROTOCOL_VERSION);

    if (success) {
        // Session ID
        msg.writeBytes(session_id_, SESSION_ID_SIZE);
    } else {
        // Error message
        msg.writeLengthPrefixedString(error_msg);
    }

    sendMessage(conn, msg);
}

void NativeAdapter::sendAuthResponse(network::Connection* conn, bool success,
                                      const std::string& error_msg) {
    Message msg(MessageType::AUTH_RESPONSE);

    // Success flag
    msg.writeUInt8(success ? 1 : 0);

    if (!success) {
        // Error message
        msg.writeLengthPrefixedString(error_msg);
    }

    sendMessage(conn, msg);
}

void NativeAdapter::sendQueryError(network::Connection* conn, uint32_t error_code,
                                    const std::string& sqlstate, const std::string& message) {
    Message msg(MessageType::QUERY_ERROR);

    msg.writeUInt32(error_code);
    msg.writeLengthPrefixedString(sqlstate);
    msg.writeLengthPrefixedString(message);

    sendMessage(conn, msg);
}

void NativeAdapter::sendRowDescription(network::Connection* conn,
                                        const std::vector<ProtocolCodec::ColumnInfo>& columns) {
    Message msg(MessageType::ROW_DESCRIPTION);

    // Column count
    msg.writeUInt16(static_cast<uint16_t>(columns.size()));

    for (const auto& col : columns) {
        // Column name
        msg.writeLengthPrefixedString(col.name);

        // Type
        msg.writeUInt8(static_cast<uint8_t>(col.type));

        // Type modifier
        msg.writeUInt32(col.type_modifier);
    }

    sendMessage(conn, msg);
}

void NativeAdapter::sendRowData(network::Connection* conn,
                                 const std::vector<ProtocolCodec::ColumnValue>& values) {
    Message msg(MessageType::ROW_DATA);

    // Column count
    msg.writeUInt16(static_cast<uint16_t>(values.size()));

    for (const auto& val : values) {
        if (val.is_null) {
            msg.writeUInt8(1);  // NULL flag
        } else {
            msg.writeUInt8(0);  // Not NULL
            // Convert vector<uint8_t> to string
            std::string data_str(val.data.begin(), val.data.end());
            msg.writeLengthPrefixedString(data_str);
        }
    }

    sendMessage(conn, msg);
}

void NativeAdapter::sendEndOfResults(network::Connection* conn) {
    Message msg(MessageType::END_OF_RESULTS);
    sendMessage(conn, msg);
}

void NativeAdapter::sendCommandComplete(network::Connection* conn, const std::string& tag,
                                         int64_t rows_affected) {
    Message msg(MessageType::COMMAND_COMPLETE);

    msg.writeLengthPrefixedString(tag);
    msg.writeInt64(rows_affected);

    sendMessage(conn, msg);
}

void NativeAdapter::sendPrepareResponse(network::Connection* conn, uint32_t stmt_id,
                                         bool success, const std::string& error_msg) {
    Message msg(MessageType::PREPARE_RESPONSE);

    msg.writeUInt8(success ? 1 : 0);

    if (success) {
        msg.writeUInt32(stmt_id);
    } else {
        msg.writeLengthPrefixedString(error_msg);
    }

    sendMessage(conn, msg);
}

void NativeAdapter::sendDescribeResponse(network::Connection* conn, uint32_t stmt_id,
                                          const std::vector<ProtocolCodec::ColumnInfo>& columns,
                                          uint16_t param_count) {
    Message msg(MessageType::DESCRIBE_RESPONSE);

    msg.writeUInt32(stmt_id);
    msg.writeUInt16(param_count);
    msg.writeUInt16(static_cast<uint16_t>(columns.size()));

    for (const auto& col : columns) {
        msg.writeLengthPrefixedString(col.name);
        msg.writeUInt8(static_cast<uint8_t>(col.type));
        msg.writeUInt32(col.type_modifier);
    }

    sendMessage(conn, msg);
}

void NativeAdapter::sendTransactionStatus(network::Connection* conn, bool in_transaction) {
    Message msg(MessageType::TRANSACTION_STATUS);
    msg.writeUInt8(in_transaction ? 1 : 0);
    sendMessage(conn, msg);
}

void NativeAdapter::sendPong(network::Connection* conn) {
    Message msg(MessageType::PONG);
    sendMessage(conn, msg);
}

void NativeAdapter::sendStatusResponse(network::Connection* conn) {
    Message msg(MessageType::STATUS_RESPONSE);

    // Server status
    msg.writeUInt8(native_state_ == NativeProtocolState::READY ? 1 : 0);
    msg.writeUInt8(in_transaction_ ? 1 : 0);
    msg.writeUInt64(queries_executed_);
    msg.writeUInt64(bytes_received_);
    msg.writeUInt64(bytes_sent_);

    sendMessage(conn, msg);
}

} // namespace protocol
} // namespace scratchbird
