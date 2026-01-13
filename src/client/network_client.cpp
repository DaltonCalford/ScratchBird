/**
 * ScratchBird Network Client (libscratchbird)
 *
 * Alpha: Native protocol over network listener (parser bridge required).
 */

#include "scratchbird/client/network_client.h"

#include <algorithm>
#include <cstring>
#include <iostream>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace scratchbird {
namespace client {

namespace {
uint32_t getProcessId() {
#ifdef _WIN32
    return static_cast<uint32_t>(_getpid());
#else
    return static_cast<uint32_t>(getpid());
#endif
}

core::Status mapQueryError(const protocol::Message& response,
                           std::string& message,
                           core::ErrorContext* ctx) {
    uint32_t error_code = 0;
    std::string sqlstate;
    std::string detail;
    std::string hint;
    protocol::ProtocolCodec::parseQueryError(response, error_code, sqlstate, message, detail, hint, ctx);
    if (!detail.empty()) {
        message += " (" + detail + ")";
    }
    return static_cast<core::Status>(error_code);
}

} // namespace

NetworkClient::NetworkClient() = default;
NetworkClient::~NetworkClient() = default;

core::Status NetworkClient::connect(const NetworkClientConfig& config,
                                    core::ErrorContext* ctx) {
    config_ = config;
    last_error_.clear();

    if (!network::isNetworkInitialized()) {
        if (!network::initNetwork()) {
            last_error_ = "Failed to initialize network subsystem";
            return core::Status::INTERNAL_ERROR;
        }
    }

    network::NetworkAddress address;
    address.family = network::AddressFamily::IPV4;
    address.host = config_.host;
    address.port = config_.port;

    network::SocketOptions options;
    options.connect_timeout_ms = config_.connect_timeout_ms;
    options.read_timeout_ms = config_.read_timeout_ms;
    options.write_timeout_ms = config_.write_timeout_ms;

    socket_ = network::Socket::connect(address, options, ctx);
    if (!socket_) {
        last_error_ = "Failed to connect to network listener";
        return core::Status::CONNECTION_FAILURE;
    }

    bool require_tls = config_.ssl_mode == network::SSLMode::REQUIRE ||
                       config_.ssl_mode == network::SSLMode::VERIFY_CA ||
                       config_.ssl_mode == network::SSLMode::VERIFY_FULL;

    if (config_.ssl_mode != network::SSLMode::DISABLED) {
        security::TLSClientConfig tls_cfg;
        tls_cfg.enabled = true;
        tls_cfg.min_version = security::TLSVersion::TLS_1_3;
        tls_cfg.max_version = security::TLSVersion::TLS_1_3;
        tls_cfg.cert_file = config_.ssl_cert;
        tls_cfg.key_file = config_.ssl_key;
        tls_cfg.ca_file = config_.ssl_root_cert;
        tls_cfg.use_system_ca = config_.ssl_root_cert.empty();

        if (config_.ssl_mode == network::SSLMode::VERIFY_CA ||
            config_.ssl_mode == network::SSLMode::VERIFY_FULL) {
            tls_cfg.verify_server = true;
            if (config_.ssl_mode == network::SSLMode::VERIFY_FULL) {
                tls_cfg.expected_hostname = config_.host;
            }
        } else {
            tls_cfg.verify_server = false;
        }

        tls_ctx_ = security::TLSContext::createClient(tls_cfg, ctx);
        if (!tls_ctx_) {
            last_error_ = ctx ? ctx->message : "Failed to initialize TLS context";
            if (require_tls) {
                return core::Status::CONNECTION_FAILURE;
            }
        } else {
            tls_conn_ = std::make_unique<security::TLSConnection>(*tls_ctx_);
            auto status = tls_conn_->setFd(socket_->getFd());
            if (status == core::Status::OK) {
                tls_conn_->setSNIHostname(config_.host);
                status = tls_conn_->connect();
            }

            if (status != core::Status::OK) {
                last_error_ = tls_conn_ ? tls_conn_->getLastErrorMessage() : "TLS handshake failed";
                tls_conn_.reset();
                tls_ctx_.reset();
                if (require_tls) {
                    return status;
                }
            } else {
                tls_active_ = true;
            }
        }
    }

    // CONNECT_REQUEST
    auto connect_msg = protocol::ProtocolCodec::buildConnectRequest(
        config_.database,
        config_.application_name,
        getProcessId()
    );

    auto status = sendMessage(connect_msg, ctx);
    if (status != core::Status::OK) {
        last_error_ = "Failed to send CONNECT_REQUEST";
        return status;
    }

    protocol::Message response;
    status = receiveMessage(response, ctx);
    if (status != core::Status::OK) {
        last_error_ = "Failed to receive CONNECT_RESPONSE";
        return status;
    }

    if (response.getType() != protocol::MessageType::CONNECT_RESPONSE) {
        last_error_ = "Unexpected response to CONNECT_REQUEST";
        return core::Status::PROTOCOL_VIOLATION;
    }

    bool success = false;
    std::string error_msg;
    status = protocol::ProtocolCodec::parseConnectResponse(
        response, success, session_id_, error_msg, ctx
    );
    if (status != core::Status::OK || !success) {
        last_error_ = error_msg.empty() ? "Connection refused" : error_msg;
        return core::Status::CONNECTION_FAILURE;
    }

    if (!config_.username.empty()) {
        auto auth_msg = protocol::ProtocolCodec::buildAuthRequest(
            session_id_,
            config_.username,
            config_.password
        );
        status = sendMessage(auth_msg, ctx);
        if (status != core::Status::OK) {
            last_error_ = "Failed to send AUTH_REQUEST";
            return status;
        }

        status = receiveMessage(response, ctx);
        if (status != core::Status::OK) {
            last_error_ = "Failed to receive AUTH_RESPONSE";
            return status;
        }

        if (response.getType() != protocol::MessageType::AUTH_RESPONSE) {
            last_error_ = "Unexpected response to AUTH_REQUEST";
            return core::Status::PROTOCOL_VIOLATION;
        }

        uint32_t user_id = 0;
        status = protocol::ProtocolCodec::parseAuthResponse(
            response, success, user_id, error_msg, ctx
        );
        if (status != core::Status::OK || !success) {
            last_error_ = error_msg.empty() ? "Authentication failed" : error_msg;
            return core::Status::INVALID_PASSWORD;
        }
    }

    connected_ = true;
    in_transaction_ = false;
    return core::Status::OK;
}

void NetworkClient::disconnect() {
    if (!socket_) {
        connected_ = false;
        return;
    }

    protocol::Message msg = protocol::ProtocolCodec::buildDisconnect();
    sendMessage(msg, nullptr);

    if (tls_conn_) {
        tls_conn_->shutdown();
    }
    tls_conn_.reset();
    tls_ctx_.reset();
    tls_active_ = false;

    socket_->close();
    socket_.reset();
    connected_ = false;
    in_transaction_ = false;
}

bool NetworkClient::isConnected() const {
    return connected_ && socket_ && socket_->isOpen();
}

core::Status NetworkClient::executeQuery(const std::string& sql,
                                         NetworkResultSet& results,
                                         core::ErrorContext* ctx) {
    results.columns.clear();
    results.rows.clear();
    results.rows_affected = 0;
    results.command_tag.clear();

    if (!isConnected()) {
        last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }

    auto query_msg = protocol::ProtocolCodec::buildQuery(session_id_, sql, 0);
    auto status = sendMessage(query_msg, ctx);
    if (status != core::Status::OK) {
        last_error_ = "Failed to send QUERY";
        return status;
    }

    const uint32_t kCopyWindow = 65536;

    auto handle_copy_out = [&]() -> core::Status {
        std::ostream* out = copy_output_stream_ ? copy_output_stream_ : &std::cout;
        uint32_t window = 0;
        bool stream_ready = false;

        while (true) {
            protocol::Message response;
            auto status = receiveMessage(response, ctx);
            if (status != core::Status::OK) {
                last_error_ = "Failed to receive COPY OUT response";
                return status;
            }

            switch (response.getType()) {
                case protocol::MessageType::STREAM_READY: {
                    stream_ready = true;
                    window = kCopyWindow;
                    auto ctrl = protocol::ProtocolCodec::buildStreamControl(
                        protocol::StreamControlType::START, window, 0);
                    status = sendMessage(ctrl, ctx);
                    if (status != core::Status::OK) {
                        last_error_ = "Failed to send STREAM_CONTROL";
                        return status;
                    }
                    break;
                }
                case protocol::MessageType::COPY_DATA: {
                    const uint8_t* data = nullptr;
                    size_t len = 0;
                    protocol::ProtocolCodec::parseCopyData(response, &data, &len, ctx);
                    if (len > 0) {
                        out->write(reinterpret_cast<const char*>(data),
                                   static_cast<std::streamsize>(len));
                        if (!(*out)) {
                            last_error_ = "COPY OUT write failed";
                            return core::Status::IO_ERROR;
                        }
                    }
                    if (window > 0) {
                        if (len >= window) {
                            window = 0;
                        } else {
                            window -= static_cast<uint32_t>(len);
                        }
                    }
                    if (stream_ready && window == 0) {
                        window = kCopyWindow;
                        auto ctrl = protocol::ProtocolCodec::buildStreamControl(
                            protocol::StreamControlType::ACK, window, 0);
                        status = sendMessage(ctrl, ctx);
                        if (status != core::Status::OK) {
                            last_error_ = "Failed to send STREAM_CONTROL ACK";
                            return status;
                        }
                    }
                    break;
                }
                case protocol::MessageType::COPY_DONE:
                    return core::Status::OK;

                case protocol::MessageType::COPY_FAIL: {
                    std::string message;
                    protocol::ProtocolCodec::parseCopyFail(response, message, ctx);
                    last_error_ = message.empty() ? "COPY OUT failed" : message;
                    return core::Status::INTERNAL_ERROR;
                }
                case protocol::MessageType::QUERY_ERROR: {
                    std::string message;
                    status = mapQueryError(response, message, ctx);
                    last_error_ = message;
                    return status;
                }
                case protocol::MessageType::STREAM_END:
                    break;

                default:
                    break;
            }
        }
    };

    auto handle_copy_in = [&]() -> core::Status {
        std::istream* in = copy_input_stream_ ? copy_input_stream_ : &std::cin;
        uint32_t window = 0;
        bool done = false;

        while (!done) {
            if (window == 0) {
                protocol::Message response;
                auto status = receiveMessage(response, ctx);
                if (status != core::Status::OK) {
                    last_error_ = "Failed to receive COPY IN control";
                    return status;
                }

                switch (response.getType()) {
                    case protocol::MessageType::STREAM_READY:
                        break;
                    case protocol::MessageType::STREAM_CONTROL: {
                        protocol::StreamControlType control;
                        uint32_t new_window = 0;
                        uint32_t timeout_ms = 0;
                        status = protocol::ProtocolCodec::parseStreamControl(
                            response, control, new_window, timeout_ms, ctx);
                        if (status != core::Status::OK) {
                            last_error_ = "Malformed STREAM_CONTROL";
                            return status;
                        }
                        (void)timeout_ms;
                        if (control == protocol::StreamControlType::PAUSE) {
                            break;
                        }
                        if (control == protocol::StreamControlType::CANCEL) {
                            last_error_ = "COPY IN canceled by server";
                            return core::Status::CANCELLED;
                        }
                        window += new_window;
                        break;
                    }
                    case protocol::MessageType::COPY_FAIL: {
                        std::string message;
                        protocol::ProtocolCodec::parseCopyFail(response, message, ctx);
                        last_error_ = message.empty() ? "COPY IN failed" : message;
                        return core::Status::INTERNAL_ERROR;
                    }
                    case protocol::MessageType::QUERY_ERROR: {
                        std::string message;
                        status = mapQueryError(response, message, ctx);
                        last_error_ = message;
                        return status;
                    }
                    default:
                        break;
                }
                if (window == 0) {
                    continue;
                }
            }

            size_t to_read = std::min<size_t>(window, 16384);
            std::string buffer(to_read, '\0');
            in->read(buffer.data(), static_cast<std::streamsize>(to_read));
            std::streamsize got = in->gcount();

            if (got > 0) {
                auto msg = protocol::ProtocolCodec::buildCopyData(
                    reinterpret_cast<const uint8_t*>(buffer.data()),
                    static_cast<size_t>(got));
                auto status = sendMessage(msg, ctx);
                if (status != core::Status::OK) {
                    last_error_ = "Failed to send COPY_DATA";
                    return status;
                }
                if (got >= static_cast<std::streamsize>(window)) {
                    window = 0;
                } else {
                    window -= static_cast<uint32_t>(got);
                }
            } else {
                auto msg = protocol::ProtocolCodec::buildCopyDone();
                auto status = sendMessage(msg, ctx);
                if (status != core::Status::OK) {
                    last_error_ = "Failed to send COPY_DONE";
                    return status;
                }
                done = true;
            }
        }
        return core::Status::OK;
    };

    while (true) {
        protocol::Message response;
        status = receiveMessage(response, ctx);
        if (status != core::Status::OK) {
            last_error_ = "Failed to receive response";
            return status;
        }

        switch (response.getType()) {
            case protocol::MessageType::QUERY_ERROR: {
                std::string message;
                status = mapQueryError(response, message, ctx);
                last_error_ = message;
                return status;
            }
            case protocol::MessageType::ROW_DESCRIPTION: {
                std::vector<protocol::ProtocolCodec::ColumnInfo> cols;
                protocol::ProtocolCodec::parseRowDescription(response, cols, ctx);
                results.columns.clear();
                results.columns.reserve(cols.size());
                for (const auto& col : cols) {
                    NetworkColumn out;
                    out.name = col.name;
                    out.type = col.type;
                    out.type_modifier = col.type_modifier;
                    results.columns.push_back(std::move(out));
                }
                break;
            }
            case protocol::MessageType::ROW_DATA: {
                std::vector<protocol::ProtocolCodec::ColumnValue> values;
                protocol::ProtocolCodec::parseRowData(response, values, ctx);
                results.rows.push_back(std::move(values));
                break;
            }
            case protocol::MessageType::COMMAND_COMPLETE: {
                std::string tag;
                int64_t rows_affected = 0;
                protocol::ProtocolCodec::parseCommandComplete(
                    response, tag, rows_affected, ctx
                );
                results.command_tag = tag;
                results.rows_affected = rows_affected;
                break;
            }
            case protocol::MessageType::END_OF_RESULTS:
                return core::Status::OK;

            case protocol::MessageType::COPY_IN_RESPONSE: {
                status = handle_copy_in();
                if (status != core::Status::OK) {
                    return status;
                }
                break;
            }
            case protocol::MessageType::COPY_OUT_RESPONSE: {
                status = handle_copy_out();
                if (status != core::Status::OK) {
                    return status;
                }
                break;
            }
            case protocol::MessageType::COPY_FAIL: {
                std::string message;
                protocol::ProtocolCodec::parseCopyFail(response, message, ctx);
                last_error_ = message.empty() ? "COPY failed" : message;
                return core::Status::INTERNAL_ERROR;
            }
            case protocol::MessageType::STREAM_END:
            case protocol::MessageType::STREAM_READY:
                break;

            case protocol::MessageType::TRANSACTION_STATUS: {
                if (response.getPayloadSize() >= sizeof(protocol::TransactionStatusPayload)) {
                    const auto* ts_payload = reinterpret_cast<const protocol::TransactionStatusPayload*>(
                        response.getPayload());
                    in_transaction_ = (ts_payload->status == 1);
                }
                break;
            }
            default:
                break;
        }
    }
}

core::Status NetworkClient::beginTransaction(core::ErrorContext* ctx) {
    auto msg = protocol::ProtocolCodec::buildBeginTransaction(session_id_, 0, false);
    auto status = sendMessage(msg, ctx);
    if (status != core::Status::OK) {
        last_error_ = "Failed to send BEGIN";
        return status;
    }

    protocol::Message response;
    status = receiveMessage(response, ctx);
    if (status != core::Status::OK) {
        last_error_ = "Failed to receive BEGIN response";
        return status;
    }

    if (response.getType() == protocol::MessageType::QUERY_ERROR) {
        std::string message;
        status = mapQueryError(response, message, ctx);
        last_error_ = message;
        return status;
    }

    in_transaction_ = true;
    return core::Status::OK;
}

core::Status NetworkClient::commit(core::ErrorContext* ctx) {
    auto msg = protocol::ProtocolCodec::buildCommit(session_id_);
    auto status = sendMessage(msg, ctx);
    if (status != core::Status::OK) {
        last_error_ = "Failed to send COMMIT";
        return status;
    }

    protocol::Message response;
    status = receiveMessage(response, ctx);
    if (status != core::Status::OK) {
        last_error_ = "Failed to receive COMMIT response";
        return status;
    }

    if (response.getType() == protocol::MessageType::QUERY_ERROR) {
        std::string message;
        status = mapQueryError(response, message, ctx);
        last_error_ = message;
        return status;
    }

    in_transaction_ = false;
    return core::Status::OK;
}

core::Status NetworkClient::rollback(core::ErrorContext* ctx) {
    auto msg = protocol::ProtocolCodec::buildRollback(session_id_);
    auto status = sendMessage(msg, ctx);
    if (status != core::Status::OK) {
        last_error_ = "Failed to send ROLLBACK";
        return status;
    }

    protocol::Message response;
    status = receiveMessage(response, ctx);
    if (status != core::Status::OK) {
        last_error_ = "Failed to receive ROLLBACK response";
        return status;
    }

    if (response.getType() == protocol::MessageType::QUERY_ERROR) {
        std::string message;
        status = mapQueryError(response, message, ctx);
        last_error_ = message;
        return status;
    }

    in_transaction_ = false;
    return core::Status::OK;
}

core::Status NetworkClient::sendMessage(const protocol::Message& msg,
                                        core::ErrorContext* ctx) {
    if (!socket_ || !socket_->isOpen()) {
        SET_ERROR_CONTEXT(ctx, core::Status::CONNECTION_FAILURE, "Connection closed");
        return core::Status::CONNECTION_FAILURE;
    }

    std::vector<uint8_t> buffer;
    auto status = msg.serialize(buffer);
    if (status != core::Status::OK) {
        return status;
    }

    if (tls_active_ && tls_conn_) {
        size_t offset = 0;
        while (offset < buffer.size()) {
            int written = tls_conn_->write(buffer.data() + offset,
                                           static_cast<int>(buffer.size() - offset));
            if (written <= 0) {
                SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR, "TLS write failed");
                return core::Status::IO_ERROR;
            }
            offset += static_cast<size_t>(written);
        }
        return core::Status::OK;
    }

    return socket_->writeExact(buffer.data(), buffer.size(), ctx);
}

core::Status NetworkClient::receiveMessage(protocol::Message& msg,
                                           core::ErrorContext* ctx) {
    if (!socket_ || !socket_->isOpen()) {
        SET_ERROR_CONTEXT(ctx, core::Status::CONNECTION_FAILURE, "Connection closed");
        return core::Status::CONNECTION_FAILURE;
    }

    uint8_t header_buf[sizeof(protocol::MessageHeader)];
    core::Status status;
    if (tls_active_ && tls_conn_) {
        size_t offset = 0;
        while (offset < sizeof(header_buf)) {
            int read = tls_conn_->read(header_buf + offset,
                                       static_cast<int>(sizeof(header_buf) - offset));
            if (read <= 0) {
                SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR, "TLS read failed");
                return core::Status::IO_ERROR;
            }
            offset += static_cast<size_t>(read);
        }
        status = core::Status::OK;
    } else {
        status = socket_->readExact(header_buf, sizeof(header_buf), ctx);
    }
    if (status != core::Status::OK) {
        return status;
    }

    protocol::MessageHeader header;
    status = protocol::Message::parseHeader(header_buf, header, ctx);
    if (status != core::Status::OK) {
        return status;
    }

    msg = protocol::Message(static_cast<protocol::MessageType>(header.type));
    msg.setFlags(header.flags);

    if (header.payload_length > 0) {
        std::vector<uint8_t> payload_buf(header.payload_length);
        if (tls_active_ && tls_conn_) {
            size_t offset = 0;
            while (offset < payload_buf.size()) {
                int read = tls_conn_->read(payload_buf.data() + offset,
                                           static_cast<int>(payload_buf.size() - offset));
                if (read <= 0) {
                    SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR, "TLS read failed");
                    return core::Status::IO_ERROR;
                }
                offset += static_cast<size_t>(read);
            }
            status = core::Status::OK;
        } else {
            status = socket_->readExact(payload_buf.data(), header.payload_length, ctx);
        }
        if (status != core::Status::OK) {
            return status;
        }
        msg.setPayload(payload_buf.data(), header.payload_length);
    }

    return core::Status::OK;
}

} // namespace client
} // namespace scratchbird
