/**
 * Native ScratchBird Protocol Adapter Implementation
 *
 * ScratchBird Network Layer - Phase 3.2
 *
 * Implements the native ScratchBird wire protocol.
 */

#include "scratchbird/protocol/adapters/native_adapter.h"
#include "scratchbird/core/error_context.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <functional>
#include <iostream>
#include <streambuf>
#include <thread>
#include <chrono>

namespace scratchbird {
namespace protocol {

namespace {

class CopyOutStreambuf : public std::streambuf {
public:
    using WriteFn = std::function<bool(const uint8_t*, size_t, std::string&)>;

    CopyOutStreambuf(size_t buffer_size, WriteFn write_fn)
        : write_fn_(std::move(write_fn)) {
        if (buffer_size == 0) {
            buffer_size = 65536;
        }
        buffer_.resize(buffer_size);
        setp(buffer_.data(), buffer_.data() + buffer_.size());
    }

    ~CopyOutStreambuf() override {
        sync();
    }

protected:
    int_type overflow(int_type ch) override {
        if (ch != traits_type::eof()) {
            *pptr() = static_cast<char>(ch);
            pbump(1);
        }
        if (!flushBuffer()) {
            return traits_type::eof();
        }
        return ch;
    }

    int sync() override {
        return flushBuffer() ? 0 : -1;
    }

private:
    bool flushBuffer() {
        size_t len = static_cast<size_t>(pptr() - pbase());
        if (len == 0) {
            return true;
        }
        std::string error;
        bool ok = write_fn_(reinterpret_cast<const uint8_t*>(buffer_.data()), len, error);
        if (!ok) {
            throw std::runtime_error(error.empty() ? "COPY OUT stream error" : error);
        }
        setp(buffer_.data(), buffer_.data() + buffer_.size());
        return true;
    }

    std::vector<char> buffer_;
    WriteFn write_fn_;
};

class CopyInStreambuf : public std::streambuf {
public:
    using ReadFn = std::function<bool(std::string&, bool&, std::string&)>;

    explicit CopyInStreambuf(ReadFn read_fn)
        : read_fn_(std::move(read_fn)) {}

protected:
    int_type underflow() override {
        if (done_) {
            return traits_type::eof();
        }

        std::string chunk;
        std::string error;
        bool ok = read_fn_(chunk, done_, error);
        if (!ok) {
            throw std::runtime_error(error.empty() ? "COPY IN stream error" : error);
        }
        if (chunk.empty()) {
            return traits_type::eof();
        }

        buffer_ = std::move(chunk);
        setg(buffer_.data(), buffer_.data(), buffer_.data() + buffer_.size());
        return traits_type::to_int_type(*gptr());
    }

private:
    ReadFn read_fn_;
    std::string buffer_;
    bool done_ = false;
};

} // namespace

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

        for (const auto& row : result.rows) {
            sendRowData(conn, row);
        }
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

    bool from_stdin = false;
    bool to_stdout = false;
    if (parseCopyQuery(query, from_stdin, to_stdout)) {
        QueryContext ctx;
        ctx.query = query;
        auto status = handleCopyQuery(conn, ctx, from_stdin, to_stdout);
        native_state_ = NativeProtocolState::READY;
        return status;
    }

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

    bool from_stdin = false;
    bool to_stdout = false;
    if (parseCopyQuery(ctx.query, from_stdin, to_stdout)) {
        auto status = handleCopyQuery(conn, ctx, from_stdin, to_stdout);
        native_state_ = NativeProtocolState::READY;
        return status;
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

// ============================================================================
// COPY Helpers
// ============================================================================

core::Status NativeAdapter::handleCopyQuery(network::Connection* conn, const QueryContext& ctx,
                                            bool from_stdin, bool to_stdout) {
    std::vector<uint8_t> bytecode;
    std::string compile_error;
    auto status = compileQuery(ctx.query, bytecode, compile_error);
    if (status != core::Status::OK) {
        sendQueryError(conn, static_cast<uint32_t>(status), "42000",
                       compile_error.empty() ? "Compilation failed" : compile_error);
        return sendBuffer(conn);
    }

    if (from_stdin) {
        native_state_ = NativeProtocolState::COPY_IN;
    } else if (to_stdout) {
        native_state_ = NativeProtocolState::COPY_OUT;
    } else {
        sendQueryError(conn, static_cast<uint32_t>(core::Status::INVALID_ARGUMENT),
                       "0A000", "COPY only supports STDIN/STDOUT in native protocol");
        return sendBuffer(conn);
    }

    copy_stream_id_ = next_stream_id_++;
    copy_total_bytes_ = 0;

    if (from_stdin) {
        copy_in_window_grant_ = static_cast<uint32_t>(std::max<size_t>(config_.read_buffer_size, 16384));
        copy_in_window_bytes_ = 0;
        copy_in_low_watermark_ = copy_in_window_grant_ / 2;

        sendMessage(conn, ProtocolCodec::buildCopyInResponse(CopyFormat::TEXT, {}));
        sendMessage(conn, ProtocolCodec::buildStreamReady(copy_stream_id_, 0, 0));
        sendMessage(conn, ProtocolCodec::buildStreamControl(StreamControlType::START,
                                                            copy_in_window_grant_, 0));
        copy_in_window_bytes_ += copy_in_window_grant_;
        auto status_flush = flushWriteBuffer(conn);
        if (status_flush != core::Status::OK) {
            native_state_ = NativeProtocolState::READY;
            return status_flush;
        }

        auto read_fn = [this, conn](std::string& out, bool& done, std::string& error) {
            return readCopyInChunk(conn, out, done, error);
        };
        CopyInStreambuf streambuf(read_fn);
        std::istream in(&streambuf);

        struct CopyInputGuard {
            sblr::Executor* executor;
            explicit CopyInputGuard(sblr::Executor* exec, std::istream* in_stream)
                : executor(exec) {
                if (executor) executor->setCopyInputStream(in_stream);
            }
            ~CopyInputGuard() {
                if (executor) executor->setCopyInputStream(nullptr);
            }
        } input_guard(executor_.get(), &in);

        ResultContext result;
        executeQuery(ctx, result);

        if (result.has_error) {
            sendMessage(conn, ProtocolCodec::buildCopyFail(result.error_message));
            sendQueryError(conn, result.error_code, result.sqlstate,
                           result.error_message);
            native_state_ = NativeProtocolState::READY;
            return flushWriteBuffer(conn);
        }

        auto rows = executor_ ? executor_->getLastAffectedRows() : 0;
        sendMessage(conn, ProtocolCodec::buildStreamEnd(copy_stream_id_,
                                                        static_cast<uint64_t>(rows),
                                                        copy_total_bytes_));
        sendCommandComplete(conn, "COPY " + std::to_string(rows), rows);
        sendEndOfResults(conn);
        native_state_ = NativeProtocolState::READY;
        return flushWriteBuffer(conn);
    }

    // COPY OUT
    copy_out_window_bytes_ = 0;
    copy_out_paused_ = false;

    sendMessage(conn, ProtocolCodec::buildCopyOutResponse(CopyFormat::TEXT, {}));
    sendMessage(conn, ProtocolCodec::buildStreamReady(copy_stream_id_, 0, 0));
    auto status_flush = flushWriteBuffer(conn);
    if (status_flush != core::Status::OK) {
        native_state_ = NativeProtocolState::READY;
        return status_flush;
    }

    auto write_fn = [this, conn](const uint8_t* data, size_t len, std::string& error) {
        return sendCopyOutChunk(conn, data, len, error);
    };
    CopyOutStreambuf streambuf(config_.write_buffer_size, write_fn);
    std::ostream out(&streambuf);

    struct CopyOutputGuard {
        sblr::Executor* executor;
        explicit CopyOutputGuard(sblr::Executor* exec, std::ostream* out_stream)
            : executor(exec) {
            if (executor) executor->setCopyOutputStream(out_stream);
        }
        ~CopyOutputGuard() {
            if (executor) executor->setCopyOutputStream(nullptr);
        }
    } output_guard(executor_.get(), &out);

    ResultContext result;
    executeQuery(ctx, result);
    out.flush();

    if (result.has_error) {
        sendMessage(conn, ProtocolCodec::buildCopyFail(result.error_message));
        sendQueryError(conn, result.error_code, result.sqlstate,
                       result.error_message);
        native_state_ = NativeProtocolState::READY;
        return flushWriteBuffer(conn);
    }

    sendMessage(conn, ProtocolCodec::buildCopyDone());

    auto rows = executor_ ? executor_->getLastAffectedRows() : 0;
    sendMessage(conn, ProtocolCodec::buildStreamEnd(copy_stream_id_,
                                                    static_cast<uint64_t>(rows),
                                                    copy_total_bytes_));
    sendCommandComplete(conn, "COPY " + std::to_string(rows), rows);
    sendEndOfResults(conn);
    native_state_ = NativeProtocolState::READY;
    return flushWriteBuffer(conn);
}

core::Status NativeAdapter::flushWriteBuffer(network::Connection* conn) {
    auto start = std::chrono::steady_clock::now();
    while (conn->hasPendingWrites()) {
        auto written = conn->writeFromBuffer();
        if (written < 0) {
            return core::Status::IO_ERROR;
        }
        if (written == 0) {
            if (conn->isWriteTimedOut()) {
                return core::Status::IO_ERROR;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed > std::chrono::seconds(30)) {
            return core::Status::IO_ERROR;
        }
    }
    return core::Status::OK;
}

core::Status NativeAdapter::receiveMessageBlocking(network::Connection* conn, Message& msg) {
    auto start = std::chrono::steady_clock::now();
    while (true) {
        auto status = parseMessage(conn);
        if (status == core::Status::OK) {
            msg = std::move(current_message_);
            return core::Status::OK;
        }
        if (status != core::Status::IO_ERROR) {
            return status;
        }

        if (!conn->isOpen()) {
            return core::Status::CONNECTION_FAILURE;
        }

        auto bytes = conn->readIntoBuffer();
        if (bytes < 0) {
            return core::Status::IO_ERROR;
        }
        if (bytes == 0) {
            if (conn->isReadTimedOut()) {
                return core::Status::IO_ERROR;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed > std::chrono::seconds(30)) {
            return core::Status::IO_ERROR;
        }
    }
}

bool NativeAdapter::parseCopyQuery(const std::string& sql, bool& from_stdin, bool& to_stdout) const {
    from_stdin = false;
    to_stdout = false;

    auto trim_left = [](const std::string& input) {
        size_t pos = 0;
        while (pos < input.size() && std::isspace(static_cast<unsigned char>(input[pos]))) {
            ++pos;
        }
        return input.substr(pos);
    };

    std::string trimmed = trim_left(sql);
    if (trimmed.size() < 4) {
        return false;
    }

    std::string upper = trimmed;
    std::transform(upper.begin(), upper.end(), upper.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

    if (upper.rfind("COPY", 0) != 0) {
        return false;
    }
    if (upper.size() > 4) {
        char next = upper[4];
        if (!std::isspace(static_cast<unsigned char>(next)) && next != '(') {
            return false;
        }
    }

    if (upper.find("FROM STDIN") != std::string::npos) {
        from_stdin = true;
    }
    if (upper.find("TO STDOUT") != std::string::npos) {
        to_stdout = true;
    }
    return from_stdin || to_stdout;
}

bool NativeAdapter::waitForCopyOutWindow(network::Connection* conn, std::string& error) {
    while (copy_out_window_bytes_ == 0 || copy_out_paused_) {
        Message msg;
        auto status = receiveMessageBlocking(conn, msg);
        if (status != core::Status::OK) {
            error = "Failed to receive STREAM_CONTROL for COPY";
            return false;
        }

        switch (msg.getType()) {
            case MessageType::STREAM_CONTROL: {
                StreamControlType control;
                uint32_t window = 0;
                uint32_t timeout_ms = 0;
                if (ProtocolCodec::parseStreamControl(msg, control, window, timeout_ms) != core::Status::OK) {
                    error = "Malformed STREAM_CONTROL message";
                    return false;
                }
                (void)timeout_ms;
                switch (control) {
                    case StreamControlType::START:
                    case StreamControlType::RESUME:
                    case StreamControlType::ACK:
                        copy_out_window_bytes_ += window;
                        copy_out_paused_ = false;
                        break;
                    case StreamControlType::PAUSE:
                        copy_out_paused_ = true;
                        break;
                    case StreamControlType::CANCEL:
                        error = "COPY canceled by client";
                        return false;
                }
                break;
            }
            case MessageType::COPY_FAIL: {
                std::string fail;
                ProtocolCodec::parseCopyFail(msg, fail, nullptr);
                error = fail.empty() ? "COPY failed" : fail;
                return false;
            }
            case MessageType::QUERY_CANCEL:
                error = "COPY canceled by client";
                return false;
            default:
                error = "Unexpected message during COPY OUT";
                return false;
        }
    }
    return true;
}

bool NativeAdapter::sendCopyOutChunk(network::Connection* conn, const uint8_t* data, size_t len,
                                     std::string& error) {
    size_t offset = 0;
    while (offset < len) {
        if (copy_out_window_bytes_ == 0 || copy_out_paused_) {
            if (!waitForCopyOutWindow(conn, error)) {
                return false;
            }
        }

        size_t chunk = len - offset;
        if (copy_out_window_bytes_ > 0) {
            chunk = std::min<size_t>(chunk, copy_out_window_bytes_);
        }

        sendMessage(conn, ProtocolCodec::buildCopyData(data + offset, chunk));
        auto status = flushWriteBuffer(conn);
        if (status != core::Status::OK) {
            error = "Failed to send COPY data";
            return false;
        }

        copy_out_window_bytes_ = (copy_out_window_bytes_ > chunk)
            ? (copy_out_window_bytes_ - static_cast<uint32_t>(chunk))
            : 0;
        copy_total_bytes_ += chunk;
        offset += chunk;
    }
    return true;
}

core::Status NativeAdapter::grantCopyInWindow(network::Connection* conn, uint32_t window_bytes) {
    sendMessage(conn, ProtocolCodec::buildStreamControl(StreamControlType::RESUME,
                                                        window_bytes, 0));
    copy_in_window_bytes_ += window_bytes;
    return flushWriteBuffer(conn);
}

bool NativeAdapter::readCopyInChunk(network::Connection* conn, std::string& out, bool& done,
                                    std::string& error) {
    out.clear();
    done = false;

    while (true) {
        Message msg;
        auto status = receiveMessageBlocking(conn, msg);
        if (status != core::Status::OK) {
            error = "Failed to receive COPY data";
            return false;
        }

        switch (msg.getType()) {
            case MessageType::COPY_DATA: {
                const uint8_t* data = nullptr;
                size_t len = 0;
                if (ProtocolCodec::parseCopyData(msg, &data, &len, nullptr) != core::Status::OK) {
                    error = "Malformed COPY_DATA payload";
                    return false;
                }
                out.assign(reinterpret_cast<const char*>(data), len);
                copy_total_bytes_ += len;

                if (copy_in_window_bytes_ > 0) {
                    if (len >= copy_in_window_bytes_) {
                        copy_in_window_bytes_ = 0;
                    } else {
                        copy_in_window_bytes_ -= static_cast<uint32_t>(len);
                    }
                }
                if (copy_in_window_grant_ > 0 &&
                    copy_in_window_bytes_ <= copy_in_low_watermark_) {
                    grantCopyInWindow(conn, copy_in_window_grant_);
                }
                return true;
            }
            case MessageType::COPY_DONE:
                done = true;
                return true;
            case MessageType::COPY_FAIL: {
                std::string fail;
                ProtocolCodec::parseCopyFail(msg, fail, nullptr);
                error = fail.empty() ? "COPY failed" : fail;
                return false;
            }
            case MessageType::STREAM_CONTROL: {
                StreamControlType control;
                uint32_t window = 0;
                uint32_t timeout_ms = 0;
                if (ProtocolCodec::parseStreamControl(msg, control, window, timeout_ms) != core::Status::OK) {
                    error = "Malformed STREAM_CONTROL message";
                    return false;
                }
                (void)timeout_ms;
                if (control == StreamControlType::CANCEL) {
                    error = "COPY canceled by client";
                    return false;
                }
                // Ignore other control messages during COPY IN.
                break;
            }
            case MessageType::QUERY_CANCEL:
                error = "COPY canceled by client";
                return false;
            default:
                error = "Unexpected message during COPY IN";
                return false;
        }
    }
}

} // namespace protocol
} // namespace scratchbird
