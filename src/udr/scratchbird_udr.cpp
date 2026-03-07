/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0
 */
/**
 * @file scratchbird_udr.cpp
 * @brief Outbound ScratchBird-to-ScratchBird bridge connector implementation.
 */

#include "scratchbird/udr/scratchbird_udr.h"
#include "scratchbird/protocol/sbwp_protocol.h"

#include <cstring>
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <arpa/inet.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <netdb.h>
#endif
#include "scratchbird/core/posix_compat.h"
#include <fcntl.h>
#include <openssl/ssl.h>

namespace scratchbird {
namespace udr {

// ============================================================================
// ScratchBirdConnection Implementation
// ============================================================================

ScratchBirdConnection::ScratchBirdConnection() = default;

ScratchBirdConnection::~ScratchBirdConnection() {
    close();
}

bool ScratchBirdConnection::isOpen() const {
    return socket_fd_ >= 0;
}

bool ScratchBirdConnection::isValid() const {
    return isOpen() && process_id_ != 0;
}

core::Status ScratchBirdConnection::ping(core::ErrorContext* ctx) {
    if (!isOpen()) {
        if (ctx) {
            ctx->set(core::Status::CONNECTION_FAILURE, "Connection is closed",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::CONNECTION_FAILURE;
    }
    
    // Send PING, expect PONG
    SBWPMessage ping(sbwp::MessageType::PING, {});
    auto status = sendMessage(ping, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    SBWPMessage response;
    status = readMessage(response, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    if (response.type != sbwp::MessageType::PONG) {
        if (ctx) {
            ctx->set(core::Status::PROTOCOL_VIOLATION, "Expected PONG",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::PROTOCOL_VIOLATION;
    }
    
    return core::Status::OK;
}

void ScratchBirdConnection::close() {
    if (socket_fd_ >= 0) {
        sendTerminate(nullptr);
        cleanupSSL();
        cleanupSocket();
    }
    process_id_ = 0;
    secret_key_ = 0;
}

std::string ScratchBirdConnection::getRemoteAddress() const {
    return host_ + ":" + std::to_string(port_);
}

std::string ScratchBirdConnection::getRemoteVersion() const {
    return "SBWP " + std::to_string(protocol_version_ >> 8) + "." +
           std::to_string(protocol_version_ & 0xFF);
}

core::Status ScratchBirdConnection::connect(const std::string& host, uint16_t port,
                                           const std::string& database,
                                           const std::string& user,
                                           const std::string& password,
                                           const std::string& ssl_mode,
                                           core::ErrorContext* ctx) {
    host_ = host;
    port_ = port;
    database_ = database;
    user_ = user;
    password_ = password;
    ssl_mode_ = ssl_mode.empty() ? "prefer" : ssl_mode;
    
    // Resolve hostname
    struct hostent* server = gethostbyname(host.c_str());
    if (!server) {
        if (ctx) {
            ctx->set(core::Status::CONNECTION_FAILURE, "Failed to resolve hostname",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::CONNECTION_FAILURE;
    }
    
    // Create socket
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
        if (ctx) {
            ctx->set(core::Status::IO_ERROR, "Failed to create socket",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::IO_ERROR;
    }
    
    // Connect
    struct sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    std::memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    
    if (::connect(socket_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        cleanupSocket();
        if (ctx) {
            ctx->set(core::Status::CONNECTION_FAILURE, "Failed to connect to server",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::CONNECTION_FAILURE;
    }
    
    // Perform startup handshake
    auto status = startup(ctx);
    if (status != core::Status::OK) {
        cleanupSocket();
        return status;
    }
    
    return core::Status::OK;
}

core::Status ScratchBirdConnection::startup(core::ErrorContext* ctx) {
    // Build startup message
    std::vector<uint8_t> payload;
    
    // Protocol version (2 bytes, big-endian)
    payload.push_back((sbwp::CURRENT_VERSION >> 8) & 0xFF);
    payload.push_back(sbwp::CURRENT_VERSION & 0xFF);
    
    // SSL mode (1 byte)
    uint8_t ssl_code = sbwp::SSL_MODE_PREFER;
    if (ssl_mode_ == "disable") ssl_code = sbwp::SSL_MODE_DISABLE;
    else if (ssl_mode_ == "prefer") ssl_code = sbwp::SSL_MODE_PREFER;
    else if (ssl_mode_ == "require") ssl_code = sbwp::SSL_MODE_REQUIRE;
    else if (ssl_mode_ == "verify-ca") ssl_code = sbwp::SSL_MODE_VERIFY_CA;
    else if (ssl_mode_ == "verify-full") ssl_code = sbwp::SSL_MODE_VERIFY_FULL;
    payload.push_back(ssl_code);
    
    // Startup parameters (null-terminated key-value pairs)
    auto addParam = [&payload](const char* key, const char* value) {
        payload.insert(payload.end(), key, key + std::strlen(key) + 1);
        payload.insert(payload.end(), value, value + std::strlen(value) + 1);
    };
    
    addParam("user", user_.c_str());
    if (!database_.empty()) {
        addParam("database", database_.c_str());
    }
    addParam("client_encoding", "UTF8");
    addParam("application_name", "ScratchBirdUDR");
    
    // Null terminator
    payload.push_back(0);
    
    // Send STARTUP
    SBWPMessage msg(sbwp::MessageType::STARTUP, payload);
    auto status = sendMessage(msg, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    // Read response
    SBWPMessage response;
    status = readMessage(response, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    switch (response.type) {
        case sbwp::MessageType::READY:
            // Connection ready
            if (response.payload.size() >= 4) {
                process_id_ = (response.payload[0] << 24) |
                             (response.payload[1] << 16) |
                             (response.payload[2] << 8) |
                             response.payload[3];
            }
            if (response.payload.size() >= 8) {
                secret_key_ = (response.payload[4] << 24) |
                             (response.payload[5] << 16) |
                             (response.payload[6] << 8) |
                             response.payload[7];
            }
            return core::Status::OK;
            
        case sbwp::MessageType::SSL_REQUEST:
            // Server wants SSL
            if (ssl_mode_ == "disable") {
                if (ctx) {
                    ctx->set(core::Status::CONNECTION_FAILURE,
                            "Server requires SSL but client disabled it",
                            __FILE__, __LINE__, __func__);
                }
                return core::Status::CONNECTION_FAILURE;
            }
            status = negotiateSSL(ctx);
            if (status != core::Status::OK) {
                return status;
            }
            // Retry startup over SSL
            return startup(ctx);
            
        case sbwp::MessageType::AUTHENTICATE:
            // Authentication required
            return authenticate(ctx);
            
        case sbwp::MessageType::ERROR_MESSAGE:
            // Error response
            if (ctx && response.payload.size() >= 2) {
                uint16_t error_code = (response.payload[0] << 8) | response.payload[1];
                ctx->set(core::Status::CONNECTION_FAILURE,
                        ("Server error: " + std::to_string(error_code)).c_str(),
                        __FILE__, __LINE__, __func__);
            }
            return core::Status::CONNECTION_FAILURE;
            
        default:
            if (ctx) {
                ctx->set(core::Status::PROTOCOL_VIOLATION,
                        "Unexpected message during startup",
                        __FILE__, __LINE__, __func__);
            }
            return core::Status::PROTOCOL_VIOLATION;
    }
}

core::Status ScratchBirdConnection::negotiateSSL(core::ErrorContext* ctx) {
    // Send SSL request
    std::vector<uint8_t> payload;
    payload.push_back(sbwp::SSL_MODE_REQUIRE);  // We're requesting SSL
    
    SBWPMessage msg(sbwp::MessageType::SSL_REQUEST, payload);
    auto status = sendMessage(msg, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    // Read SSL response
    SBWPMessage response;
    status = readMessage(response, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    if (response.type != sbwp::MessageType::READY) {
        if (ctx) {
            ctx->set(core::Status::CONNECTION_FAILURE, "SSL negotiation failed",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::CONNECTION_FAILURE;
    }
    
    // Initialize SSL
    SSL_CTX* ssl_ctx = SSL_CTX_new(TLS_client_method());
    if (!ssl_ctx) {
        if (ctx) {
            ctx->set(core::Status::INTERNAL_ERROR, "Failed to create SSL context",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::INTERNAL_ERROR;
    }
    
    // Set verify mode based on ssl_mode_
    if (ssl_mode_ == "verify-ca" || ssl_mode_ == "verify-full") {
        SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER, nullptr);
    } else {
        SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_NONE, nullptr);
    }
    
    SSL* ssl = SSL_new(ssl_ctx);
    if (!ssl) {
        SSL_CTX_free(ssl_ctx);
        if (ctx) {
            ctx->set(core::Status::INTERNAL_ERROR, "Failed to create SSL",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::INTERNAL_ERROR;
    }
    
    SSL_set_fd(ssl, socket_fd_);
    
    if (SSL_connect(ssl) <= 0) {
        SSL_free(ssl);
        SSL_CTX_free(ssl_ctx);
        if (ctx) {
            ctx->set(core::Status::CONNECTION_FAILURE, "SSL handshake failed",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::CONNECTION_FAILURE;
    }
    
    ssl_ = ssl;
    ssl_ctx_ = ssl_ctx;
    ssl_enabled_ = true;
    
    return core::Status::OK;
}

core::Status ScratchBirdConnection::authenticate(core::ErrorContext* ctx) {
    // Send password authentication
    std::vector<uint8_t> payload;
    
    // Auth method (1 byte)
    payload.push_back(sbwp::AUTH_PASSWORD);
    
    // Password (null-terminated)
    payload.insert(payload.end(), password_.begin(), password_.end());
    payload.push_back(0);
    
    SBWPMessage msg(sbwp::MessageType::AUTHENTICATE, payload);
    auto status = sendMessage(msg, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    // Read response
    SBWPMessage response;
    status = readMessage(response, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    if (response.type == sbwp::MessageType::READY) {
        if (response.payload.size() >= 8) {
            process_id_ = (response.payload[0] << 24) |
                         (response.payload[1] << 16) |
                         (response.payload[2] << 8) |
                         response.payload[3];
            secret_key_ = (response.payload[4] << 24) |
                         (response.payload[5] << 16) |
                         (response.payload[6] << 8) |
                         response.payload[7];
        }
        return core::Status::OK;
    }
    
    if (response.type == sbwp::MessageType::ERROR_MESSAGE) {
        if (ctx) {
            ctx->set(core::Status::INVALID_PASSWORD, "Authentication failed",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::INVALID_PASSWORD;
    }
    
    if (ctx) {
        ctx->set(core::Status::PROTOCOL_VIOLATION, "Unexpected auth response",
                __FILE__, __LINE__, __func__);
    }
    return core::Status::PROTOCOL_VIOLATION;
}

core::Status ScratchBirdConnection::sendSimpleQuery(const std::string& sql,
                                                   core::ErrorContext* ctx) {
    std::vector<uint8_t> payload(sql.begin(), sql.end());
    payload.push_back(0);  // Null terminator
    
    SBWPMessage msg(sbwp::MessageType::SIMPLE_QUERY, payload);
    return sendMessage(msg, ctx);
}

core::Status ScratchBirdConnection::sendParse(const std::string& name,
                                             const std::string& sql,
                                             core::ErrorContext* ctx) {
    std::vector<uint8_t> payload;
    
    // Statement name (null-terminated)
    payload.insert(payload.end(), name.begin(), name.end());
    payload.push_back(0);
    
    // SQL (null-terminated)
    payload.insert(payload.end(), sql.begin(), sql.end());
    payload.push_back(0);
    
    // Parameter types (count + OIDs) - empty for now
    payload.push_back(0);
    payload.push_back(0);
    
    SBWPMessage msg(sbwp::MessageType::PARSE, payload);
    return sendMessage(msg, ctx);
}

core::Status ScratchBirdConnection::sendBind(const std::string& portal,
                                            const std::string& stmt,
                                            const std::vector<RemoteValue>& params,
                                            core::ErrorContext* ctx) {
    (void)params;
    std::vector<uint8_t> payload;
    
    // Portal name (null-terminated)
    payload.insert(payload.end(), portal.begin(), portal.end());
    payload.push_back(0);
    
    // Statement name (null-terminated)
    payload.insert(payload.end(), stmt.begin(), stmt.end());
    payload.push_back(0);
    
    // Parameter format codes and values would go here
    // For now, send empty
    payload.push_back(0);
    payload.push_back(0);
    
    SBWPMessage msg(sbwp::MessageType::BIND, payload);
    return sendMessage(msg, ctx);
}

core::Status ScratchBirdConnection::sendExecute(const std::string& portal,
                                               uint32_t max_rows,
                                               core::ErrorContext* ctx) {
    std::vector<uint8_t> payload;
    
    // Portal name (null-terminated)
    payload.insert(payload.end(), portal.begin(), portal.end());
    payload.push_back(0);
    
    // Max rows (4 bytes, big-endian)
    payload.push_back((max_rows >> 24) & 0xFF);
    payload.push_back((max_rows >> 16) & 0xFF);
    payload.push_back((max_rows >> 8) & 0xFF);
    payload.push_back(max_rows & 0xFF);
    
    SBWPMessage msg(sbwp::MessageType::EXECUTE, payload);
    return sendMessage(msg, ctx);
}

core::Status ScratchBirdConnection::sendSync(core::ErrorContext* ctx) {
    SBWPMessage msg(sbwp::MessageType::SYNC, {});
    return sendMessage(msg, ctx);
}

core::Status ScratchBirdConnection::sendTerminate(core::ErrorContext* ctx) {
    SBWPMessage msg(sbwp::MessageType::TERMINATE, {});
    return sendMessage(msg, ctx);
}

core::Status ScratchBirdConnection::sendBegin(core::ErrorContext* ctx) {
    SBWPMessage msg(sbwp::MessageType::BEGIN, {});
    return sendMessage(msg, ctx);
}

core::Status ScratchBirdConnection::sendCommit(core::ErrorContext* ctx) {
    SBWPMessage msg(sbwp::MessageType::COMMIT, {});
    return sendMessage(msg, ctx);
}

core::Status ScratchBirdConnection::sendRollback(core::ErrorContext* ctx) {
    SBWPMessage msg(sbwp::MessageType::ROLLBACK, {});
    return sendMessage(msg, ctx);
}

core::Status ScratchBirdConnection::sendSavepoint(const std::string& name,
                                                 core::ErrorContext* ctx) {
    std::vector<uint8_t> payload(name.begin(), name.end());
    payload.push_back(0);
    
    SBWPMessage msg(sbwp::MessageType::SAVEPOINT, payload);
    return sendMessage(msg, ctx);
}

core::Status ScratchBirdConnection::sendCopyData(const uint8_t* data, size_t len,
                                                core::ErrorContext* ctx) {
    std::vector<uint8_t> payload(data, data + len);
    
    SBWPMessage msg(sbwp::MessageType::COPY_DATA, payload);
    return sendMessage(msg, ctx);
}

core::Status ScratchBirdConnection::sendCopyDone(core::ErrorContext* ctx) {
    SBWPMessage msg(sbwp::MessageType::COPY_DONE, {});
    return sendMessage(msg, ctx);
}

core::Status ScratchBirdConnection::sendCopyFail(const std::string& reason,
                                                core::ErrorContext* ctx) {
    std::vector<uint8_t> payload(reason.begin(), reason.end());
    payload.push_back(0);
    
    SBWPMessage msg(sbwp::MessageType::COPY_FAIL, payload);
    return sendMessage(msg, ctx);
}

core::Status ScratchBirdConnection::sendMessage(const SBWPMessage& msg,
                                               core::ErrorContext* ctx) {
    // Build frame: [type:1][length:4][payload:N]
    std::vector<uint8_t> frame;
    frame.push_back(static_cast<uint8_t>(msg.type));
    
    uint32_t len = msg.length;
    frame.push_back((len >> 24) & 0xFF);
    frame.push_back((len >> 16) & 0xFF);
    frame.push_back((len >> 8) & 0xFF);
    frame.push_back(len & 0xFF);
    
    if (!msg.payload.empty()) {
        frame.insert(frame.end(), msg.payload.begin(), msg.payload.end());
    }
    
    return writeExactly(frame.data(), frame.size(), ctx);
}

core::Status ScratchBirdConnection::readMessage(SBWPMessage& msg, core::ErrorContext* ctx) {
    // Read header: [type:1][length:4]
    uint8_t header[5];
    auto status = readExactly(header, 5, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    msg.type = static_cast<sbwp::MessageType>(header[0]);
    msg.length = (header[1] << 24) | (header[2] << 16) | (header[3] << 8) | header[4];
    
    if (msg.length > sbwp::MAX_MESSAGE_SIZE) {
        if (ctx) {
            ctx->set(core::Status::PROTOCOL_VIOLATION, "Message too large",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::PROTOCOL_VIOLATION;
    }
    
    if (msg.length > 0) {
        msg.payload.resize(msg.length);
        status = readExactly(msg.payload.data(), msg.length, ctx);
        if (status != core::Status::OK) {
            return status;
        }
    } else {
        msg.payload.clear();
    }
    
    return core::Status::OK;
}

core::Status ScratchBirdConnection::readExactly(void* buffer, size_t len,
                                               core::ErrorContext* ctx) {
    uint8_t* ptr = (uint8_t*)buffer;
    size_t remaining = len;
    
    while (remaining > 0) {
        ssize_t n;
        if (ssl_enabled_ && ssl_) {
            n = SSL_read((SSL*)ssl_, ptr, remaining);
        } else {
            n = read(socket_fd_, ptr, remaining);
        }
        
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                if (ctx) {
                    ctx->set(core::Status::LOCK_TIMEOUT, "Read timeout",
                            __FILE__, __LINE__, __func__);
                }
                return core::Status::LOCK_TIMEOUT;
            }
            if (ctx) {
                ctx->set(core::Status::IO_ERROR, "Read error",
                        __FILE__, __LINE__, __func__);
            }
            return core::Status::IO_ERROR;
        }
        
        if (n == 0) {
            if (ctx) {
                ctx->set(core::Status::CONNECTION_FAILURE, "Connection closed",
                        __FILE__, __LINE__, __func__);
            }
            return core::Status::CONNECTION_FAILURE;
        }
        
        ptr += n;
        remaining -= n;
    }
    
    return core::Status::OK;
}

core::Status ScratchBirdConnection::writeExactly(const void* buffer, size_t len,
                                                core::ErrorContext* ctx) {
    const uint8_t* ptr = (const uint8_t*)buffer;
    size_t remaining = len;
    
    while (remaining > 0) {
        ssize_t n;
        if (ssl_enabled_ && ssl_) {
            n = SSL_write((SSL*)ssl_, ptr, remaining);
        } else {
            n = write(socket_fd_, ptr, remaining);
        }
        
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                if (ctx) {
                    ctx->set(core::Status::LOCK_TIMEOUT, "Write timeout",
                            __FILE__, __LINE__, __func__);
                }
                return core::Status::LOCK_TIMEOUT;
            }
            if (ctx) {
                ctx->set(core::Status::IO_ERROR, "Write error",
                        __FILE__, __LINE__, __func__);
            }
            return core::Status::IO_ERROR;
        }
        
        ptr += n;
        remaining -= n;
    }
    
    return core::Status::OK;
}

void ScratchBirdConnection::cleanupSSL() {
    if (ssl_) {
        SSL_shutdown((SSL*)ssl_);
        SSL_free((SSL*)ssl_);
        ssl_ = nullptr;
    }
    if (ssl_ctx_) {
        SSL_CTX_free((SSL_CTX*)ssl_ctx_);
        ssl_ctx_ = nullptr;
    }
    ssl_enabled_ = false;
}

void ScratchBirdConnection::cleanupSocket() {
    if (socket_fd_ >= 0) {
        ::close(socket_fd_);
        socket_fd_ = -1;
    }
}

// Row reading implementations
core::Status ScratchBirdConnection::readRowDescription(std::vector<SBWPField>& fields,
                                                      core::ErrorContext* ctx) {
    fields.clear();
    
    // Read message
    SBWPMessage msg;
    
    auto status = readMessage(msg, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    if (msg.type != sbwp::MessageType::ROW_DESCRIPTION) {
        if (msg.type == sbwp::MessageType::ERROR_MESSAGE) {
            return core::Status::INTERNAL_ERROR;  // Error handled by caller
        }
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT, 
                    "Expected RowDescription, got different message type",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::INVALID_ARGUMENT;
    }
    
    if (msg.payload.size() < 2) {
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT, "Invalid RowDescription message",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::INVALID_ARGUMENT;
    }
    
    // Parse field count
    uint16_t field_count = ntohs(*reinterpret_cast<const uint16_t*>(msg.payload.data()));
    size_t offset = 2;
    
    for (uint16_t i = 0; i < field_count; i++) {
        SBWPField field;
        
        // Read field name (null-terminated)
        const char* name_ptr = reinterpret_cast<const char*>(msg.payload.data() + offset);
        field.name = name_ptr;
        offset += field.name.size() + 1;
        
        // Read remaining fixed fields
        if (offset + 20 > msg.payload.size()) break;
        
        field.table_oid = ntohl(*reinterpret_cast<const uint32_t*>(msg.payload.data() + offset));
        offset += 4;
        
        field.column_number = ntohs(*reinterpret_cast<const uint16_t*>(msg.payload.data() + offset));
        offset += 2;
        
        field.type_oid = ntohl(*reinterpret_cast<const uint32_t*>(msg.payload.data() + offset));
        offset += 4;
        
        field.type_size = ntohs(*reinterpret_cast<const uint16_t*>(msg.payload.data() + offset));
        offset += 2;
        
        field.type_modifier = ntohl(*reinterpret_cast<const uint32_t*>(msg.payload.data() + offset));
        offset += 4;
        
        field.format_code = ntohs(*reinterpret_cast<const uint16_t*>(msg.payload.data() + offset));
        offset += 2;
        
        fields.push_back(field);
    }
    
    return core::Status::OK;
}

core::Status ScratchBirdConnection::readDataRow(SBWPRow& row,
                                               const std::vector<SBWPField>& fields,
                                               core::ErrorContext* ctx) {
    row.fields.clear();
    row.fields.resize(fields.size());
    
    // Read message
    SBWPMessage msg;
    
    auto status = readMessage(msg, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    if (msg.type == sbwp::MessageType::COMMAND_COMPLETE) {
        // No more rows
        return core::Status::NOT_FOUND;  // Use NOT_FOUND to indicate end of results
    }
    
    if (msg.type != sbwp::MessageType::DATA_ROW) {
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT, 
                    "Expected DataRow, got different message type",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::INVALID_ARGUMENT;
    }
    
    if (msg.payload.size() < 2) {
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT, "Invalid DataRow message",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::INVALID_ARGUMENT;
    }
    
    // Parse field count
    uint16_t field_count = ntohs(*reinterpret_cast<const uint16_t*>(msg.payload.data()));
    if (field_count != fields.size()) {
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT, "Field count mismatch",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::INVALID_ARGUMENT;
    }
    
    size_t offset = 2;
    
    for (uint16_t i = 0; i < field_count; i++) {
        if (offset + 4 > msg.payload.size()) break;
        
        int32_t field_len = ntohl(*reinterpret_cast<const int32_t*>(msg.payload.data() + offset));
        offset += 4;
        
        if (field_len < 0) {
            // NULL value
            row.fields[i] = std::nullopt;
        } else {
            if (offset + field_len > msg.payload.size()) break;
            row.fields[i] = std::vector<uint8_t>(msg.payload.data() + offset, 
                                                  msg.payload.data() + offset + field_len);
            offset += field_len;
        }
    }
    
    return core::Status::OK;
}

// ============================================================================
// ScratchBirdConnectionFactory Implementation
// ============================================================================

ScratchBirdConnectionFactory::ScratchBirdConnectionFactory(const UDRServerConfig& config)
    : config_(config) {
}

std::unique_ptr<PooledConnection> ScratchBirdConnectionFactory::createConnection(
    core::ErrorContext* ctx) {
    
    auto conn = std::make_unique<ScratchBirdConnection>();
    
    auto status = conn->connect(
        config_.host,
        config_.port ? config_.port : 5433,  // Default SB port
        config_.database,
        config_.user,
        config_.password,
        config_.ssl_mode,
        ctx
    );
    
    if (status != core::Status::OK) {
        return nullptr;
    }
    
    return conn;
}

bool ScratchBirdConnectionFactory::validateConnection(PooledConnection* conn,
                                                     core::ErrorContext* ctx) {
    if (!conn) return false;
    
    auto* sb_conn = dynamic_cast<ScratchBirdConnection*>(conn);
    if (!sb_conn) return false;
    
    return sb_conn->ping(ctx) == core::Status::OK;
}

void ScratchBirdConnectionFactory::destroyConnection(PooledConnection* conn) {
    if (conn) {
        conn->close();
        delete conn;
    }
}

// ============================================================================
// ScratchBirdUDRConnector Implementation
// ============================================================================

ScratchBirdUDRConnector::ScratchBirdUDRConnector() = default;

ScratchBirdUDRConnector::~ScratchBirdUDRConnector() {
    shutdown();
}

core::Status ScratchBirdUDRConnector::initialize(const UDRServerConfig& config,
                                                core::ErrorContext* ctx) {
    config_ = config;
    
    auto factory = std::make_unique<ScratchBirdConnectionFactory>(config);
    
    ConnectionPoolConfig pool_config;
    pool_config.min_size = config.pool_min_size;
    pool_config.max_size = config.pool_max_size;
    pool_config.initial_size = config.pool_min_size;
    pool_config.connection_timeout_ms = config.pool_connection_timeout_ms;
    pool_config.idle_timeout_ms = config.pool_max_idle_ms;
    pool_config.health_check_interval_ms = config.pool_health_check_interval_ms;
    
    pool_ = std::make_unique<ConnectionPool>(
        "sb_" + config.host + "_" + config.database,
        std::move(factory),
        pool_config
    );
    
    return pool_->initialize(ctx);
}

core::Status ScratchBirdUDRConnector::shutdown(core::ErrorContext* ctx) {
    (void)ctx;
    if (pool_) {
        pool_->shutdown();
        pool_.reset();
    }
    return core::Status::OK;
}

bool ScratchBirdUDRConnector::isConnected() const {
    return pool_ && pool_->isRunning();
}

core::Status ScratchBirdUDRConnector::ping(core::ErrorContext* ctx) {
    if (!pool_) {
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT, "Connector not initialized",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::INVALID_ARGUMENT;
    }
    
    auto conn = acquireConnection(ctx);
    if (!conn) {
        return core::Status::CONNECTION_FAILURE;
    }
    
    auto status = conn->ping(ctx);
    releaseConnection(std::move(conn));
    
    return status;
}

core::Status ScratchBirdUDRConnector::reconnect(core::ErrorContext* ctx) {
    if (pool_) {
        pool_->invalidateAll();
        return pool_->ensureMinimumConnections(ctx);
    }
    return core::Status::INVALID_ARGUMENT;
}

core::Status ScratchBirdUDRConnector::executeQuery(const std::string& sql,
                                                  RemoteResultSet& result,
                                                  core::ErrorContext* ctx) {
    (void)result;
    auto conn = acquireConnection(ctx);
    if (!conn) {
        return core::Status::CONNECTION_FAILURE;
    }
    
    auto status = conn->sendSimpleQuery(sql, ctx);
    releaseConnection(std::move(conn));
    
    return status;
}

core::Status ScratchBirdUDRConnector::executeCommand(const std::string& sql,
                                                    uint64_t& rows_affected,
                                                    core::ErrorContext* ctx) {
    (void)rows_affected;
    RemoteResultSet result;
    return executeQuery(sql, result, ctx);
}

core::Status ScratchBirdUDRConnector::prepareStatement(const std::string& name,
                                                      const std::string& sql,
                                                      std::vector<uint32_t>& param_types,
                                                      core::ErrorContext* ctx) {
    auto conn = acquireConnection(ctx);
    if (!conn) {
        return core::Status::CONNECTION_FAILURE;
    }
    
    auto status = conn->sendParse(name, sql, ctx);
    if (status == core::Status::OK) {
        std::lock_guard<std::mutex> lock(prepared_mutex_);
        prepared_statements_[name] = param_types;
    }
    
    releaseConnection(std::move(conn));
    return status;
}

core::Status ScratchBirdUDRConnector::executePrepared(const std::string& name,
                                                     const std::vector<RemoteValue>& params,
                                                     RemoteResultSet& result,
                                                     core::ErrorContext* ctx) {
    (void)params;
    (void)result;
    {
        std::lock_guard<std::mutex> lock(prepared_mutex_);
        if (prepared_statements_.find(name) == prepared_statements_.end()) {
            if (ctx) {
                ctx->set(core::Status::INVALID_ARGUMENT, "Unknown prepared statement",
                        __FILE__, __LINE__, __func__);
            }
            return core::Status::INVALID_ARGUMENT;
        }
    }
    
    auto conn = acquireConnection(ctx);
    if (!conn) {
        return core::Status::CONNECTION_FAILURE;
    }
    
    // Bind parameters
    auto status = conn->sendBind("", name, params, ctx);
    if (status != core::Status::OK) {
        releaseConnection(std::move(conn));
        return status;
    }
    
    // Execute
    status = conn->sendExecute("", 0, ctx);
    if (status != core::Status::OK) {
        releaseConnection(std::move(conn));
        return status;
    }
    
    // Sync
    status = conn->sendSync(ctx);
    
    releaseConnection(std::move(conn));
    return status;
}

core::Status ScratchBirdUDRConnector::closeStatement(const std::string& name,
                                                    core::ErrorContext* ctx) {
    (void)ctx;
    std::lock_guard<std::mutex> lock(prepared_mutex_);
    prepared_statements_.erase(name);
    return core::Status::OK;
}

core::Status ScratchBirdUDRConnector::beginTransaction(core::ErrorContext* ctx) {
    auto conn = acquireConnection(ctx);
    if (!conn) {
        return core::Status::CONNECTION_FAILURE;
    }
    
    auto status = conn->sendBegin(ctx);
    releaseConnection(std::move(conn));
    
    return status;
}

core::Status ScratchBirdUDRConnector::commitTransaction(core::ErrorContext* ctx) {
    auto conn = acquireConnection(ctx);
    if (!conn) {
        return core::Status::CONNECTION_FAILURE;
    }
    
    auto status = conn->sendCommit(ctx);
    releaseConnection(std::move(conn));
    
    return status;
}

core::Status ScratchBirdUDRConnector::rollbackTransaction(core::ErrorContext* ctx) {
    auto conn = acquireConnection(ctx);
    if (!conn) {
        return core::Status::CONNECTION_FAILURE;
    }
    
    auto status = conn->sendRollback(ctx);
    releaseConnection(std::move(conn));
    
    return status;
}

core::Status ScratchBirdUDRConnector::savepoint(const std::string& name,
                                               core::ErrorContext* ctx) {
    auto conn = acquireConnection(ctx);
    if (!conn) {
        return core::Status::CONNECTION_FAILURE;
    }
    
    auto status = conn->sendSavepoint(name, ctx);
    releaseConnection(std::move(conn));
    
    return status;
}

core::Status ScratchBirdUDRConnector::rollbackToSavepoint(const std::string& name,
                                                         core::ErrorContext* ctx) {
    auto conn = acquireConnection(ctx);
    if (!conn) {
        return core::Status::CONNECTION_FAILURE;
    }
    
    // Send ROLLBACK TO SAVEPOINT via simple query
    std::string sql = "ROLLBACK TO SAVEPOINT " + name;
    auto status = conn->sendSimpleQuery(sql, ctx);
    
    if (status == core::Status::OK) {
        // Read response to complete the protocol exchange
        SBWPMessage response;
        status = conn->readMessage(response, ctx);
        if (status == core::Status::OK) {
            if (response.type == sbwp::MessageType::ERROR_MESSAGE) {
                if (ctx && response.payload.size() >= 2) {
                    uint16_t error_code = (response.payload[0] << 8) | response.payload[1];
                    ctx->set(core::Status::INTERNAL_ERROR,
                            ("Rollback to savepoint failed: " + std::to_string(error_code)).c_str(),
                            __FILE__, __LINE__, __func__);
                }
                status = core::Status::INTERNAL_ERROR;
            }
            // COMMAND_COMPLETE or EMPTY_QUERY_RESPONSE are expected
        }
    }
    
    releaseConnection(std::move(conn));
    return status;
}

std::unique_ptr<ScratchBirdConnection> ScratchBirdUDRConnector::acquireConnection(
    core::ErrorContext* ctx) {
    
    if (!pool_) {
        return nullptr;
    }
    
    auto conn = pool_->acquire(ctx);
    if (!conn) {
        return nullptr;
    }
    
    auto* sb_conn = dynamic_cast<ScratchBirdConnection*>(conn.get());
    if (!sb_conn) {
        pool_->release(std::move(conn));
        return nullptr;
    }
    
    conn.release();
    return std::unique_ptr<ScratchBirdConnection>(sb_conn);
}

void ScratchBirdUDRConnector::releaseConnection(
    std::unique_ptr<ScratchBirdConnection> conn) {
    
    if (pool_ && conn) {
        pool_->release(std::unique_ptr<PooledConnection>(conn.release()));
    }
}

std::string ScratchBirdUDRConnector::getVersion() const {
    return "1.0.0";
}

std::string ScratchBirdUDRConnector::getRemoteVersion() const {
    return remote_version_;
}

std::vector<std::string> ScratchBirdUDRConnector::getSupportedFeatures() const {
    return {
        "simple_query",
        "prepared_statements",
        "extended_query",
        "transactions",
        "savepoints",
        "rollback_to_savepoint",
        "cursors",
        "scrollable_cursors",
        "schema_introspection",
        "table_info",
        "procedure_info",
        "list_tables",
        "list_procedures",
        "copy_streaming",
        "copy_in",
        "copy_out",
        "ssl",
        "notifications",
        "cancel",
        "type_mapping"
    };
}

// ============================================================================
// Cursor Operations
// ============================================================================

core::Status ScratchBirdUDRConnector::declareCursor(const std::string& cursor_name,
                                                     const std::string& query,
                                                     bool scrollable,
                                                     core::ErrorContext* ctx) {
    auto conn = acquireConnection(ctx);
    if (!conn) {
        return core::Status::CONNECTION_FAILURE;
    }
    
    // Build DECLARE CURSOR SQL
    std::string sql = "DECLARE " + cursor_name;
    if (scrollable) {
        sql += " SCROLL";
    }
    sql += " CURSOR FOR " + query;
    
    auto status = conn->sendSimpleQuery(sql, ctx);
    if (status != core::Status::OK) {
        releaseConnection(std::move(conn));
        return status;
    }
    
    // Read and process response
    SBWPMessage response;
    while (true) {
        status = conn->readMessage(response, ctx);
        if (status != core::Status::OK) {
            releaseConnection(std::move(conn));
            return status;
        }
        
        if (response.type == sbwp::MessageType::ERROR_MESSAGE) {
            if (ctx && response.payload.size() >= 2) {
                uint16_t error_code = (response.payload[0] << 8) | response.payload[1];
                ctx->set(core::Status::INTERNAL_ERROR,
                        ("Declare cursor failed: " + std::to_string(error_code)).c_str(),
                        __FILE__, __LINE__, __func__);
            }
            releaseConnection(std::move(conn));
            return core::Status::INTERNAL_ERROR;
        }
        
        if (response.type == sbwp::MessageType::COMMAND_COMPLETE ||
            response.type == sbwp::MessageType::EMPTY_QUERY_RESPONSE) {
            break;
        }
        
        // Continue reading until command complete
        if (response.type == sbwp::MessageType::READY) {
            break;
        }
    }
    
    // Track active cursor
    active_cursor_ = cursor_name;
    
    releaseConnection(std::move(conn));
    return core::Status::OK;
}

core::Status ScratchBirdUDRConnector::fetchCursor(const std::string& cursor_name,
                                                   uint32_t count,
                                                   RemoteResultSet& result,
                                                   core::ErrorContext* ctx) {
    auto conn = acquireConnection(ctx);
    if (!conn) {
        return core::Status::CONNECTION_FAILURE;
    }
    
    // Build FETCH SQL
    std::string sql = "FETCH ";
    if (count == 0) {
        sql += "ALL";
    } else {
        sql += std::to_string(count);
    }
    sql += " FROM " + cursor_name;
    
    auto status = conn->sendSimpleQuery(sql, ctx);
    if (status != core::Status::OK) {
        releaseConnection(std::move(conn));
        return status;
    }
    
    // Read result set
    SBWPMessage response;
    std::vector<SBWPField> fields;
    bool row_desc_received = false;
    
    while (true) {
        status = conn->readMessage(response, ctx);
        if (status != core::Status::OK) {
            releaseConnection(std::move(conn));
            return status;
        }
        
        switch (response.type) {
            case sbwp::MessageType::ROW_DESCRIPTION: {
                // Parse row description
                fields.clear();
                if (response.payload.size() >= 2) {
                    uint16_t field_count = (response.payload[0] << 8) | response.payload[1];
                    size_t offset = 2;
                    for (uint16_t i = 0; i < field_count && offset < response.payload.size(); ++i) {
                        SBWPField field;
                        // Parse field name (null-terminated)
                        if (offset < response.payload.size()) {
                            field.name = reinterpret_cast<const char*>(response.payload.data() + offset);
                            offset += field.name.length() + 1;
                        }
                        // Parse type OID (4 bytes)
                        if (offset + 4 <= response.payload.size()) {
                            field.type_oid = (response.payload[offset] << 24) |
                                            (response.payload[offset + 1] << 16) |
                                            (response.payload[offset + 2] << 8) |
                                            response.payload[offset + 3];
                            offset += 4;
                        }
                        // Parse format code (2 bytes)
                        if (offset + 2 <= response.payload.size()) {
                            field.format_code = (response.payload[offset] << 8) | response.payload[offset + 1];
                            offset += 2;
                        }
                        fields.push_back(field);
                        
                        // Add to result columns
                        RemoteColumn col;
                        col.name = field.name;
                        col.type_oid = field.type_oid;
                        col.nullable = true;
                        result.columns.push_back(col);
                    }
                }
                row_desc_received = true;
                break;
            }
            
            case sbwp::MessageType::DATA_ROW: {
                if (!row_desc_received) {
                    if (ctx) {
                        ctx->set(core::Status::PROTOCOL_VIOLATION,
                                "Data row received without row description",
                                __FILE__, __LINE__, __func__);
                    }
                    releaseConnection(std::move(conn));
                    return core::Status::PROTOCOL_VIOLATION;
                }
                
                // Parse data row
                if (response.payload.size() >= 2) {
                    uint16_t field_count = (response.payload[0] << 8) | response.payload[1];
                    RemoteRow row;
                    size_t offset = 2;
                    
                    for (uint16_t i = 0; i < field_count && i < fields.size() && offset < response.payload.size(); ++i) {
                        RemoteValue value;
                        
                        // Check for null (special length indicator)
                        if (offset + 4 <= response.payload.size()) {
                            int32_t len = (response.payload[offset] << 24) |
                                         (response.payload[offset + 1] << 16) |
                                         (response.payload[offset + 2] << 8) |
                                         response.payload[offset + 3];
                            offset += 4;
                            
                            if (len < 0) {
                                // NULL value
                                value.is_null = true;
                            } else if (len > 0 && offset + static_cast<size_t>(len) <= response.payload.size()) {
                                value.data.assign(response.payload.begin() + offset,
                                                 response.payload.begin() + offset + len);
                                offset += len;
                            }
                        }
                        value.type_oid = fields[i].type_oid;
                        row.values.push_back(value);
                    }
                    result.rows.push_back(row);
                }
                break;
            }
            
            case sbwp::MessageType::COMMAND_COMPLETE: {
                // Parse command tag if present
                if (!response.payload.empty()) {
                    result.command_tag = std::string(reinterpret_cast<const char*>(response.payload.data()));
                }
                break;
            }
            
            case sbwp::MessageType::EMPTY_QUERY_RESPONSE: {
                result.has_more = false;
                break;
            }
            
            case sbwp::MessageType::ERROR_MESSAGE: {
                if (ctx && response.payload.size() >= 2) {
                    uint16_t error_code = (response.payload[0] << 8) | response.payload[1];
                    ctx->set(core::Status::INTERNAL_ERROR,
                            ("Fetch cursor failed: " + std::to_string(error_code)).c_str(),
                            __FILE__, __LINE__, __func__);
                }
                releaseConnection(std::move(conn));
                return core::Status::INTERNAL_ERROR;
            }
            
            case sbwp::MessageType::READY: {
                result.has_more = false;
                releaseConnection(std::move(conn));
                return core::Status::OK;
            }
            
            default:
                // Ignore other message types
                break;
        }
    }
    
    releaseConnection(std::move(conn));
    return core::Status::OK;
}

core::Status ScratchBirdUDRConnector::closeCursor(const std::string& cursor_name,
                                                   core::ErrorContext* ctx) {
    auto conn = acquireConnection(ctx);
    if (!conn) {
        return core::Status::CONNECTION_FAILURE;
    }
    
    // Build CLOSE SQL
    std::string sql = "CLOSE " + cursor_name;
    
    auto status = conn->sendSimpleQuery(sql, ctx);
    if (status != core::Status::OK) {
        releaseConnection(std::move(conn));
        return status;
    }
    
    // Read response
    SBWPMessage response;
    while (true) {
        status = conn->readMessage(response, ctx);
        if (status != core::Status::OK) {
            releaseConnection(std::move(conn));
            return status;
        }
        
        if (response.type == sbwp::MessageType::ERROR_MESSAGE) {
            if (ctx && response.payload.size() >= 2) {
                uint16_t error_code = (response.payload[0] << 8) | response.payload[1];
                ctx->set(core::Status::INTERNAL_ERROR,
                        ("Close cursor failed: " + std::to_string(error_code)).c_str(),
                        __FILE__, __LINE__, __func__);
            }
            releaseConnection(std::move(conn));
            return core::Status::INTERNAL_ERROR;
        }
        
        if (response.type == sbwp::MessageType::COMMAND_COMPLETE ||
            response.type == sbwp::MessageType::READY) {
            break;
        }
    }
    
    // Clear active cursor if this was it
    if (active_cursor_ == cursor_name) {
        active_cursor_.clear();
    }
    
    releaseConnection(std::move(conn));
    return core::Status::OK;
}

// ============================================================================
// Schema Introspection
// ============================================================================

core::Status ScratchBirdUDRConnector::getTableInfo(const std::string& schema,
                                                    const std::string& table,
                                                    RemoteTableInfo& info,
                                                    core::ErrorContext* ctx) {
    auto conn = acquireConnection(ctx);
    if (!conn) {
        return core::Status::CONNECTION_FAILURE;
    }
    
    // Query information_schema.columns
    std::string sql = 
        "SELECT column_name, data_type, character_maximum_length, numeric_precision, "
        "numeric_scale, is_nullable, column_default "
        "FROM information_schema.columns "
        "WHERE table_schema = '" + schema + "' "
        "AND table_name = '" + table + "' "
        "ORDER BY ordinal_position";
    
    auto status = conn->sendSimpleQuery(sql, ctx);
    if (status != core::Status::OK) {
        releaseConnection(std::move(conn));
        return status;
    }
    
    // Parse result
    SBWPMessage response;
    std::vector<SBWPField> fields;
    bool in_results = false;
    
    info.remote_schema = schema;
    info.remote_name = table;
    info.columns.clear();
    
    while (true) {
        status = conn->readMessage(response, ctx);
        if (status != core::Status::OK) {
            releaseConnection(std::move(conn));
            return status;
        }
        
        switch (response.type) {
            case sbwp::MessageType::ROW_DESCRIPTION: {
                fields.clear();
                if (response.payload.size() >= 2) {
                    uint16_t field_count = (response.payload[0] << 8) | response.payload[1];
                    size_t offset = 2;
                    for (uint16_t i = 0; i < field_count && offset < response.payload.size(); ++i) {
                        SBWPField field;
                        if (offset < response.payload.size()) {
                            field.name = reinterpret_cast<const char*>(response.payload.data() + offset);
                            offset += field.name.length() + 1;
                        }
                        if (offset + 4 <= response.payload.size()) {
                            field.type_oid = (response.payload[offset] << 24) |
                                            (response.payload[offset + 1] << 16) |
                                            (response.payload[offset + 2] << 8) |
                                            response.payload[offset + 3];
                            offset += 4;
                        }
                        fields.push_back(field);
                    }
                }
                in_results = true;
                break;
            }
            
            case sbwp::MessageType::DATA_ROW: {
                if (!in_results) {
                    break;
                }
                
                RemoteColumn col;
                if (response.payload.size() >= 2) {
                    uint16_t field_count = (response.payload[0] << 8) | response.payload[1];
                    size_t offset = 2;
                    
                    for (uint16_t i = 0; i < field_count && offset < response.payload.size(); ++i) {
                        if (offset + 4 > response.payload.size()) break;
                        
                        int32_t len = (response.payload[offset] << 24) |
                                     (response.payload[offset + 1] << 16) |
                                     (response.payload[offset + 2] << 8) |
                                     response.payload[offset + 3];
                        offset += 4;
                        
                        std::string value;
                        if (len > 0 && offset + static_cast<size_t>(len) <= response.payload.size()) {
                            value.assign(reinterpret_cast<const char*>(response.payload.data() + offset), len);
                            offset += len;
                        }
                        
                        // Map fields to RemoteColumn
                        if (i < fields.size()) {
                            const std::string& field_name = fields[i].name;
                            if (field_name == "column_name") {
                                col.name = value;
                            } else if (field_name == "data_type") {
                                col.type_name = value;
                            } else if (field_name == "is_nullable") {
                                col.nullable = (value == "YES");
                            } else if (field_name == "column_default") {
                                col.default_value = value;
                            }
                        }
                    }
                    info.columns.push_back(col);
                }
                break;
            }
            
            case sbwp::MessageType::COMMAND_COMPLETE:
            case sbwp::MessageType::READY:
                releaseConnection(std::move(conn));
                return core::Status::OK;
                
            case sbwp::MessageType::ERROR_MESSAGE:
                if (ctx && response.payload.size() >= 2) {
                    uint16_t error_code = (response.payload[0] << 8) | response.payload[1];
                    ctx->set(core::Status::INTERNAL_ERROR,
                            ("Get table info failed: " + std::to_string(error_code)).c_str(),
                            __FILE__, __LINE__, __func__);
                }
                releaseConnection(std::move(conn));
                return core::Status::INTERNAL_ERROR;
                
            default:
                break;
        }
    }
    
    releaseConnection(std::move(conn));
    return core::Status::OK;
}

core::Status ScratchBirdUDRConnector::listTables(const std::string& schema,
                                                  std::vector<std::string>& tables,
                                                  core::ErrorContext* ctx) {
    auto conn = acquireConnection(ctx);
    if (!conn) {
        return core::Status::CONNECTION_FAILURE;
    }
    
    std::string sql = 
        "SELECT table_name FROM information_schema.tables "
        "WHERE table_schema = '" + schema + "' "
        "AND table_type = 'BASE TABLE' "
        "ORDER BY table_name";
    
    auto status = conn->sendSimpleQuery(sql, ctx);
    if (status != core::Status::OK) {
        releaseConnection(std::move(conn));
        return status;
    }
    
    SBWPMessage response;
    tables.clear();
    
    while (true) {
        status = conn->readMessage(response, ctx);
        if (status != core::Status::OK) {
            releaseConnection(std::move(conn));
            return status;
        }
        
        switch (response.type) {
            case sbwp::MessageType::DATA_ROW: {
                if (response.payload.size() >= 6) {
                    // Skip field count (2 bytes) and length (4 bytes)
                    int32_t len = (response.payload[2] << 24) |
                                 (response.payload[3] << 16) |
                                 (response.payload[4] << 8) |
                                 response.payload[5];
                    if (len > 0 && 6 + len <= response.payload.size()) {
                        std::string table_name(reinterpret_cast<const char*>(response.payload.data() + 6), len);
                        tables.push_back(table_name);
                    }
                }
                break;
            }
            
            case sbwp::MessageType::COMMAND_COMPLETE:
            case sbwp::MessageType::READY:
                releaseConnection(std::move(conn));
                return core::Status::OK;
                
            case sbwp::MessageType::ERROR_MESSAGE:
                if (ctx && response.payload.size() >= 2) {
                    uint16_t error_code = (response.payload[0] << 8) | response.payload[1];
                    ctx->set(core::Status::INTERNAL_ERROR,
                            ("List tables failed: " + std::to_string(error_code)).c_str(),
                            __FILE__, __LINE__, __func__);
                }
                releaseConnection(std::move(conn));
                return core::Status::INTERNAL_ERROR;
                
            default:
                break;
        }
    }
    
    releaseConnection(std::move(conn));
    return core::Status::OK;
}

core::Status ScratchBirdUDRConnector::getProcedureInfo(const std::string& schema,
                                                        const std::string& procedure,
                                                        RemoteProcedureInfo& info,
                                                        core::ErrorContext* ctx) {
    auto conn = acquireConnection(ctx);
    if (!conn) {
        return core::Status::CONNECTION_FAILURE;
    }
    
    // Query information_schema.routines and parameters
    std::string sql = 
        "SELECT routine_name, parameter_name, parameter_mode, data_type "
        "FROM information_schema.parameters "
        "WHERE specific_schema = '" + schema + "' "
        "AND specific_name = '" + procedure + "' "
        "ORDER BY ordinal_position";
    
    auto status = conn->sendSimpleQuery(sql, ctx);
    if (status != core::Status::OK) {
        releaseConnection(std::move(conn));
        return status;
    }
    
    info.remote_schema = schema;
    info.remote_name = procedure;
    info.input_params.clear();
    info.output_params.clear();
    
    SBWPMessage response;
    while (true) {
        status = conn->readMessage(response, ctx);
        if (status != core::Status::OK) {
            releaseConnection(std::move(conn));
            return status;
        }
        
        switch (response.type) {
            case sbwp::MessageType::DATA_ROW: {
                // Parse row and populate procedure info
                // Simplified parsing - in real implementation would parse all fields
                RemoteColumn param;
                param.name = "param";  // Would be parsed from row data
                info.input_params.push_back(param);
                break;
            }
            
            case sbwp::MessageType::COMMAND_COMPLETE:
            case sbwp::MessageType::READY:
                releaseConnection(std::move(conn));
                return core::Status::OK;
                
            case sbwp::MessageType::ERROR_MESSAGE:
                if (ctx && response.payload.size() >= 2) {
                    uint16_t error_code = (response.payload[0] << 8) | response.payload[1];
                    ctx->set(core::Status::INTERNAL_ERROR,
                            ("Get procedure info failed: " + std::to_string(error_code)).c_str(),
                            __FILE__, __LINE__, __func__);
                }
                releaseConnection(std::move(conn));
                return core::Status::INTERNAL_ERROR;
                
            default:
                break;
        }
    }
    
    releaseConnection(std::move(conn));
    return core::Status::OK;
}

core::Status ScratchBirdUDRConnector::listProcedures(const std::string& schema,
                                                      std::vector<std::string>& procedures,
                                                      core::ErrorContext* ctx) {
    auto conn = acquireConnection(ctx);
    if (!conn) {
        return core::Status::CONNECTION_FAILURE;
    }
    
    std::string sql = 
        "SELECT routine_name FROM information_schema.routines "
        "WHERE specific_schema = '" + schema + "' "
        "AND routine_type = 'PROCEDURE' "
        "ORDER BY routine_name";
    
    auto status = conn->sendSimpleQuery(sql, ctx);
    if (status != core::Status::OK) {
        releaseConnection(std::move(conn));
        return status;
    }
    
    SBWPMessage response;
    procedures.clear();
    
    while (true) {
        status = conn->readMessage(response, ctx);
        if (status != core::Status::OK) {
            releaseConnection(std::move(conn));
            return status;
        }
        
        switch (response.type) {
            case sbwp::MessageType::DATA_ROW: {
                if (response.payload.size() >= 6) {
                    int32_t len = (response.payload[2] << 24) |
                                 (response.payload[3] << 16) |
                                 (response.payload[4] << 8) |
                                 response.payload[5];
                    if (len > 0 && 6 + len <= response.payload.size()) {
                        std::string proc_name(reinterpret_cast<const char*>(response.payload.data() + 6), len);
                        procedures.push_back(proc_name);
                    }
                }
                break;
            }
            
            case sbwp::MessageType::COMMAND_COMPLETE:
            case sbwp::MessageType::READY:
                releaseConnection(std::move(conn));
                return core::Status::OK;
                
            case sbwp::MessageType::ERROR_MESSAGE:
                if (ctx && response.payload.size() >= 2) {
                    uint16_t error_code = (response.payload[0] << 8) | response.payload[1];
                    ctx->set(core::Status::INTERNAL_ERROR,
                            ("List procedures failed: " + std::to_string(error_code)).c_str(),
                            __FILE__, __LINE__, __func__);
                }
                releaseConnection(std::move(conn));
                return core::Status::INTERNAL_ERROR;
                
            default:
                break;
        }
    }
    
    releaseConnection(std::move(conn));
    return core::Status::OK;
}

// ============================================================================
// COPY Operations
// ============================================================================

core::Status ScratchBirdUDRConnector::startCopyIn(const std::string& table,
                                                   const std::vector<std::string>& columns,
                                                   core::ErrorContext* ctx) {
    auto conn = acquireConnection(ctx);
    if (!conn) {
        return core::Status::CONNECTION_FAILURE;
    }
    
    // Build COPY SQL
    std::string sql = "COPY " + table;
    if (!columns.empty()) {
        sql += "(";
        for (size_t i = 0; i < columns.size(); ++i) {
            if (i > 0) sql += ", ";
            sql += columns[i];
        }
        sql += ")";
    }
    sql += " FROM STDIN";
    
    auto status = conn->sendSimpleQuery(sql, ctx);
    if (status != core::Status::OK) {
        releaseConnection(std::move(conn));
        return status;
    }
    
    // Expect COPY_IN_REQUEST response
    SBWPMessage response;
    status = conn->readMessage(response, ctx);
    if (status != core::Status::OK) {
        releaseConnection(std::move(conn));
        return status;
    }
    
    if (response.type == sbwp::MessageType::COPY_IN_REQUEST) {
        in_copy_in_ = true;
        copy_conn_ = conn.release();
        return core::Status::OK;
    }
    
    if (response.type == sbwp::MessageType::ERROR_MESSAGE && ctx && response.payload.size() >= 2) {
        uint16_t error_code = (response.payload[0] << 8) | response.payload[1];
        ctx->set(core::Status::INTERNAL_ERROR,
                ("Start copy in failed: " + std::to_string(error_code)).c_str(),
                __FILE__, __LINE__, __func__);
    }
    
    releaseConnection(std::move(conn));
    return core::Status::INTERNAL_ERROR;
}

core::Status ScratchBirdUDRConnector::sendCopyData(const uint8_t* data, size_t len,
                                                    core::ErrorContext* ctx) {
    if (!in_copy_in_ || !copy_conn_) {
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT, "Not in COPY IN mode",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::INVALID_ARGUMENT;
    }
    
    return copy_conn_->sendCopyData(data, len, ctx);
}

core::Status ScratchBirdUDRConnector::endCopyIn(uint64_t& rows_inserted,
                                                 core::ErrorContext* ctx) {
    if (!in_copy_in_ || !copy_conn_) {
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT, "Not in COPY IN mode",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::INVALID_ARGUMENT;
    }
    
    // Send COPY_DONE
    auto status = copy_conn_->sendCopyDone(ctx);
    if (status != core::Status::OK) {
        releaseConnection(std::unique_ptr<ScratchBirdConnection>(copy_conn_));
        copy_conn_ = nullptr;
        in_copy_in_ = false;
        return status;
    }
    
    // Read CommandComplete
    SBWPMessage response;
    while (true) {
        status = copy_conn_->readMessage(response, ctx);
        if (status != core::Status::OK) {
            releaseConnection(std::unique_ptr<ScratchBirdConnection>(copy_conn_));
            copy_conn_ = nullptr;
            in_copy_in_ = false;
            return status;
        }
        
        if (response.type == sbwp::MessageType::COMMAND_COMPLETE) {
            // Parse row count if available
            if (!response.payload.empty()) {
                // Parse command tag like "COPY <count>"
                std::string tag(reinterpret_cast<const char*>(response.payload.data()));
                // Extract count from tag
                size_t pos = tag.find_last_of(' ');
                if (pos != std::string::npos) {
                    try {
                        rows_inserted = std::stoull(tag.substr(pos + 1));
                    } catch (...) {
                        rows_inserted = 0;
                    }
                }
            }
            break;
        }
        
        if (response.type == sbwp::MessageType::ERROR_MESSAGE) {
            releaseConnection(std::unique_ptr<ScratchBirdConnection>(copy_conn_));
            copy_conn_ = nullptr;
            in_copy_in_ = false;
            return core::Status::INTERNAL_ERROR;
        }
        
        if (response.type == sbwp::MessageType::READY) {
            break;
        }
    }
    
    releaseConnection(std::unique_ptr<ScratchBirdConnection>(copy_conn_));
    copy_conn_ = nullptr;
    in_copy_in_ = false;
    return core::Status::OK;
}

core::Status ScratchBirdUDRConnector::startCopyOut(const std::string& query,
                                                    core::ErrorContext* ctx) {
    auto conn = acquireConnection(ctx);
    if (!conn) {
        return core::Status::CONNECTION_FAILURE;
    }
    
    std::string sql = "COPY (" + query + ") TO STDOUT";
    
    auto status = conn->sendSimpleQuery(sql, ctx);
    if (status != core::Status::OK) {
        releaseConnection(std::move(conn));
        return status;
    }
    
    // Expect COPY_OUT_RESPONSE
    SBWPMessage response;
    status = conn->readMessage(response, ctx);
    if (status != core::Status::OK) {
        releaseConnection(std::move(conn));
        return status;
    }
    
    if (response.type == sbwp::MessageType::COPY_OUT_RESPONSE) {
        in_copy_out_ = true;
        copy_conn_ = conn.release();
        return core::Status::OK;
    }
    
    if (response.type == sbwp::MessageType::ERROR_MESSAGE && ctx && response.payload.size() >= 2) {
        uint16_t error_code = (response.payload[0] << 8) | response.payload[1];
        ctx->set(core::Status::INTERNAL_ERROR,
                ("Start copy out failed: " + std::to_string(error_code)).c_str(),
                __FILE__, __LINE__, __func__);
    }
    
    releaseConnection(std::move(conn));
    return core::Status::INTERNAL_ERROR;
}

core::Status ScratchBirdUDRConnector::receiveCopyData(std::vector<uint8_t>& data,
                                                       bool& done,
                                                       core::ErrorContext* ctx) {
    data.clear();
    done = false;
    
    if (!in_copy_out_ || !copy_conn_) {
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT, "Not in COPY OUT mode",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::INVALID_ARGUMENT;
    }
    
    SBWPMessage response;
    auto status = copy_conn_->readMessage(response, ctx);
    if (status != core::Status::OK) {
        releaseConnection(std::unique_ptr<ScratchBirdConnection>(copy_conn_));
        copy_conn_ = nullptr;
        in_copy_out_ = false;
        return status;
    }
    
    if (response.type == sbwp::MessageType::COPY_DATA) {
        data = response.payload;
        done = false;
        return core::Status::OK;
    }
    
    if (response.type == sbwp::MessageType::COPY_DONE) {
        done = true;
        // Read CommandComplete and Ready
        while (true) {
            status = copy_conn_->readMessage(response, ctx);
            if (status != core::Status::OK || response.type == sbwp::MessageType::READY) {
                break;
            }
        }
        releaseConnection(std::unique_ptr<ScratchBirdConnection>(copy_conn_));
        copy_conn_ = nullptr;
        in_copy_out_ = false;
        return core::Status::OK;
    }
    
    if (response.type == sbwp::MessageType::ERROR_MESSAGE) {
        releaseConnection(std::unique_ptr<ScratchBirdConnection>(copy_conn_));
        copy_conn_ = nullptr;
        in_copy_out_ = false;
        return core::Status::INTERNAL_ERROR;
    }
    
    return core::Status::OK;
}

// ============================================================================
// Type Mapping
// ============================================================================

uint32_t ScratchBirdUDRConnector::mapTypeToSBWP(core::DataType type) const {
    using namespace scratchbird::protocol::sbwp;
    
    switch (type) {
        case core::DataType::BOOLEAN: return kOidBool;
        case core::DataType::INT8: return kOidChar;
        case core::DataType::INT16: return kOidInt2;
        case core::DataType::INT32: return kOidInt4;
        case core::DataType::INT64: return kOidInt8;
        case core::DataType::FLOAT32: return kOidFloat4;
        case core::DataType::FLOAT64: return kOidFloat8;
        case core::DataType::DECIMAL: return kOidNumeric;
        case core::DataType::MONEY: return kOidMoney;
        case core::DataType::CHAR: return kOidChar;
        case core::DataType::VARCHAR: return kOidVarchar;
        case core::DataType::TEXT: return kOidText;
        case core::DataType::BYTEA: return kOidBytea;
        case core::DataType::BLOB: return kOidBytea;
        case core::DataType::DATE: return kOidDate;
        case core::DataType::TIME: return kOidTime;
        case core::DataType::TIMESTAMP: return kOidTimestamp;
        case core::DataType::INTERVAL: return kOidInterval;
        case core::DataType::UUID: return kOidUuid;
        case core::DataType::JSON: return kOidJson;
        case core::DataType::JSONB: return kOidJsonb;
        case core::DataType::XML: return kOidXml;
        case core::DataType::POINT: return kOidPoint;
        case core::DataType::POLYGON: return kOidPolygon;
        case core::DataType::INET: return kOidInet;
        case core::DataType::CIDR: return kOidCidr;
        case core::DataType::MACADDR: return kOidMacaddr;
        case core::DataType::MACADDR8: return kOidMacaddr8;
        case core::DataType::TSVECTOR: return kOidTsVector;
        case core::DataType::TSQUERY: return kOidTsQuery;
        case core::DataType::INT4RANGE: return kOidInt4Range;
        case core::DataType::INT8RANGE: return kOidInt8Range;
        case core::DataType::NUMRANGE: return kOidNumRange;
        case core::DataType::TSRANGE: return kOidTsRange;
        case core::DataType::TSTZRANGE: return kOidTstzRange;
        case core::DataType::DATERANGE: return kOidDateRange;
        case core::DataType::VECTOR: return kOidSbVector;
        default: return kOidText;  // Default to text for unknown types
    }
}

core::DataType ScratchBirdUDRConnector::mapTypeFromSBWP(uint32_t oid) const {
    using namespace scratchbird::protocol::sbwp;
    
    switch (oid) {
        case kOidBool: return core::DataType::BOOLEAN;
        case kOidBytea: return core::DataType::BYTEA;
        case kOidChar: return core::DataType::CHAR;
        case kOidInt8: return core::DataType::INT64;
        case kOidInt2: return core::DataType::INT16;
        case kOidInt4: return core::DataType::INT32;
        case kOidText: return core::DataType::TEXT;
        case kOidJson: return core::DataType::JSON;
        case kOidXml: return core::DataType::XML;
        case kOidPoint: return core::DataType::POINT;
        case kOidLseg: return core::DataType::LINESTRING;
        case kOidPath: return core::DataType::LINESTRING;
        case kOidBox: return core::DataType::POLYGON;
        case kOidPolygon: return core::DataType::POLYGON;
        case kOidLine: return core::DataType::LINESTRING;
        case kOidFloat4: return core::DataType::FLOAT32;
        case kOidFloat8: return core::DataType::FLOAT64;
        case kOidCircle: return core::DataType::POINT;  // Approximation
        case kOidMoney: return core::DataType::MONEY;
        case kOidMacaddr: return core::DataType::MACADDR;
        case kOidCidr: return core::DataType::CIDR;
        case kOidInet: return core::DataType::INET;
        case kOidMacaddr8: return core::DataType::MACADDR8;
        case kOidBpChar: return core::DataType::CHAR;
        case kOidVarchar: return core::DataType::VARCHAR;
        case kOidDate: return core::DataType::DATE;
        case kOidTime: return core::DataType::TIME;
        case kOidTimestamp: return core::DataType::TIMESTAMP;
        case kOidTimestamptz: return core::DataType::TIMESTAMP;
        case kOidInterval: return core::DataType::INTERVAL;
        case kOidTimetz: return core::DataType::TIME;
        case kOidNumeric: return core::DataType::DECIMAL;
        case kOidUuid: return core::DataType::UUID;
        case kOidJsonb: return core::DataType::JSONB;
        case kOidInt4Range: return core::DataType::INT4RANGE;
        case kOidNumRange: return core::DataType::NUMRANGE;
        case kOidTsRange: return core::DataType::TSRANGE;
        case kOidTstzRange: return core::DataType::TSTZRANGE;
        case kOidDateRange: return core::DataType::DATERANGE;
        case kOidInt8Range: return core::DataType::INT8RANGE;
        case kOidTsVector: return core::DataType::TSVECTOR;
        case kOidTsQuery: return core::DataType::TSQUERY;
        case kOidSbVector: return core::DataType::VECTOR;
        default: return core::DataType::UNKNOWN;
    }
}

} // namespace udr
} // namespace scratchbird
