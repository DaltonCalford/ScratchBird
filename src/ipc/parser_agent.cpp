/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0
 */

#include "scratchbird/ipc/parser_agent.h"

// Section 32 invariant: parser_agent owns the parser-side runtime surface in
// the external-agent topology. That ownership does not collapse into engine
// execution ownership or a general client-facing runtime contract.

#include <cstring>
#include <chrono>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include "scratchbird/core/posix_compat.h"
#else
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "scratchbird/core/posix_compat.h"
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#endif
#include "scratchbird/core/socket_call_compat.h"


namespace scratchbird {
namespace ipc {

namespace {

std::string deriveParserIpcEndpoint(const std::string& engine_endpoint) {
    if (engine_endpoint.empty()) {
        return std::string();
    }
    return engine_endpoint + ".parser_v1";
}

}

// ============================================================================
// ParserAgent Base Class Implementation
// ============================================================================

ParserAgent::ParserAgent(const ParserAgentConfig& config)
    : config_(config) {
}

ParserAgent::~ParserAgent() {
    stop();
}

core::Status ParserAgent::runAcceptedClient(int client_fd, core::ErrorContext* ctx) {
    return handleClient(client_fd, ctx);
}

core::Status ParserAgent::start(core::ErrorContext* ctx) {
    if (running_) {
        return core::Status::OK;
    }
    
    auto status = setupListener(ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    running_ = true;
    
    // Start accept thread
    accept_thread_ = std::thread(&ParserAgent::acceptLoop, this);
    
    // Start I/O threads
    for (uint32_t i = 0; i < config_.io_threads; i++) {
        io_threads_.emplace_back(&ParserAgent::ioLoop, this);
    }
    
    return core::Status::OK;
}

core::Status ParserAgent::stop(core::ErrorContext* ctx) {
    (void)ctx;
    if (!running_) {
        return core::Status::OK;
    }
    
    running_ = false;
    
    // Close listener
    if (listen_fd_ >= 0) {
#if defined(__linux__) || defined(__APPLE__)
        ::close(listen_fd_);
        listen_fd_ = -1;
#endif
    }
    
    // Stop accept thread
    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }
    
    // Stop I/O threads
    for (auto& thread : io_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    io_threads_.clear();
    
    // Close all connections
    {
        std::unique_lock<std::shared_mutex> lock(connections_mutex_);
        for (auto& [id, conn] : connections_) {
            if (conn->socket_fd >= 0) {
#if defined(__linux__) || defined(__APPLE__)
                ::close(conn->socket_fd);
#endif
            }
        }
        connections_.clear();
    }
    
    // Close IPC channels
    {
        std::lock_guard<std::mutex> lock(ipc_pool_mutex_);
        ipc_channels_.clear();
    }
    
    return core::Status::OK;
}

ParserAgent::Stats ParserAgent::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    Stats s = stats_;
    
    std::shared_lock<std::shared_mutex> conn_lock(connections_mutex_);
    s.active_connections = static_cast<uint32_t>(connections_.size());
    
    return s;
}

core::Status ParserAgent::setupListener(core::ErrorContext* ctx) {
#if defined(__linux__) || defined(__APPLE__)
    // Determine socket type from endpoint
    if (config_.listen_endpoint.find('/') == 0) {
        // Unix domain socket
        listen_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
        if (listen_fd_ < 0) {
            if (ctx) {
                ctx->set(core::Status::IO_ERROR, "Failed to create Unix socket",
                        __FILE__, __LINE__, __func__);
            }
            return core::Status::IO_ERROR;
        }
        
        // Remove old socket file
        unlink(config_.listen_endpoint.c_str());
        
        struct sockaddr_un addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        std::strncpy(addr.sun_path, config_.listen_endpoint.c_str(),
                    sizeof(addr.sun_path) - 1);
        
        if (bind(listen_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            if (ctx) {
                ctx->set(core::Status::IO_ERROR, "Failed to bind Unix socket",
                        __FILE__, __LINE__, __func__);
            }
            ::close(listen_fd_);
            listen_fd_ = -1;
            return core::Status::IO_ERROR;
        }
    } else {
        // TCP socket
        listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd_ < 0) {
            if (ctx) {
                ctx->set(core::Status::IO_ERROR, "Failed to create TCP socket",
                        __FILE__, __LINE__, __func__);
            }
            return core::Status::IO_ERROR;
        }
        
        // Parse host:port
        size_t colon = config_.listen_endpoint.find(':');
        std::string host = config_.listen_endpoint.substr(0, colon);
        uint16_t port = 5433;  // Default
        if (colon != std::string::npos) {
            port = static_cast<uint16_t>(std::stoi(
                config_.listen_endpoint.substr(colon + 1)));
        }
        
        int opt = 1;
        sb_socket_setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, host.c_str(), &addr.sin_addr);
        
        if (bind(listen_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            if (ctx) {
                ctx->set(core::Status::IO_ERROR, "Failed to bind TCP socket",
                        __FILE__, __LINE__, __func__);
            }
            ::close(listen_fd_);
            listen_fd_ = -1;
            return core::Status::IO_ERROR;
        }
    }
    
    if (listen(listen_fd_, 128) < 0) {
        if (ctx) {
            ctx->set(core::Status::IO_ERROR, "Failed to listen on socket",
                    __FILE__, __LINE__, __func__);
        }
        ::close(listen_fd_);
        listen_fd_ = -1;
        return core::Status::IO_ERROR;
    }
    
    // Set non-blocking
    int flags = fcntl(listen_fd_, F_GETFL, 0);
    fcntl(listen_fd_, F_SETFL, flags | O_NONBLOCK);
    
    return core::Status::OK;
#else
    (void)ctx;
    return core::Status::NOT_IMPLEMENTED;
#endif
}

void ParserAgent::acceptLoop() {
    while (running_) {
#if defined(__linux__) || defined(__APPLE__)
        struct sockaddr_storage client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_fd = accept(listen_fd_, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No pending connections, sleep briefly
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            if (errno == EINTR) continue;
            if (!running_) break;
            continue;
        }
        
        // Check max connections
        {
            std::shared_lock<std::shared_mutex> lock(connections_mutex_);
            if (connections_.size() >= config_.max_connections) {
                ::close(client_fd);
                continue;
            }
        }
        
        // Handle the client
        core::ErrorContext ctx;
        auto status = handleClient(client_fd, &ctx);
        if (status != core::Status::OK) {
            ::close(client_fd);
        }
        
        updateStats([](Stats& s) { s.connections_accepted++; });
#else
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
#endif
    }
}

void ParserAgent::ioLoop() {
    while (running_) {
        // Process I/O for active connections
        // This is handled by individual connection threads in subclasses
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void ParserAgent::disconnectClient(uint32_t client_id) {
    std::unique_ptr<IPCChannel> ipc_channel;
    {
        std::unique_lock<std::shared_mutex> lock(connections_mutex_);
        auto it = connections_.find(client_id);
        if (it != connections_.end()) {
#if defined(__linux__) || defined(__APPLE__)
            if (it->second->socket_fd >= 0) {
                ::close(it->second->socket_fd);
            }
#endif
            ipc_channel = std::move(it->second->ipc_channel);
            connections_.erase(it);
        }
    }

    if (ipc_channel) {
        releaseIPCChannel(std::move(ipc_channel));
    }

    updateStats([](Stats& s) {
        s.connections_closed++;
        s.active_connections--;
    });
}

core::Status ParserAgent::sendToEngine(uint32_t client_id, const IPCMessage& msg,
                                      core::ErrorContext* ctx) {
    // Get client connection
    std::shared_lock<std::shared_mutex> lock(connections_mutex_);
    auto it = connections_.find(client_id);
    if (it == connections_.end()) {
        if (ctx) {
            ctx->set(core::Status::NOT_FOUND, "Client not found",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::NOT_FOUND;
    }
    
    ClientConnection* client = it->second.get();
    if (!client->ipc_channel) {
        if (ctx) {
            ctx->set(core::Status::NOT_FOUND, "No IPC channel for client",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::NOT_FOUND;
    }
    
    // Use IPCChannel API directly
    return client->ipc_channel->send(msg, ctx);
}

core::Status ParserAgent::sendCompiledQueryToEngine(uint32_t client_id,
                                                    uint32_t request_id,
                                                    const std::vector<uint8_t>& bytecode,
                                                    const std::string& original_sql,
                                                    core::ErrorContext* ctx) {
    IPCMessage msg;
    msg.setType(IPCMessageType::COMPILED_QUERY);
    msg.header.request_id = request_id;

    IPCCompiledQueryPayload payload{};
    payload.original_sql_length = static_cast<uint32_t>(original_sql.size());
    payload.bytecode_length = static_cast<uint32_t>(bytecode.size());

    msg.payload.resize(sizeof(payload) + original_sql.size() + bytecode.size());
    std::memcpy(msg.payload.data(), &payload, sizeof(payload));
    if (!original_sql.empty()) {
        std::memcpy(msg.payload.data() + sizeof(payload),
                    original_sql.data(),
                    original_sql.size());
    }
    if (!bytecode.empty()) {
        std::memcpy(msg.payload.data() + sizeof(payload) + original_sql.size(),
                    bytecode.data(),
                    bytecode.size());
    }

    return sendToEngine(client_id, msg, ctx);
}

core::Status ParserAgent::sendCompiledParseToEngine(uint32_t client_id,
                                                    uint32_t request_id,
                                                    const std::string& stmt_name,
                                                    const std::vector<uint8_t>& bytecode,
                                                    const std::string& original_sql,
                                                    core::ErrorContext* ctx) {
    IPCMessage msg;
    msg.setType(IPCMessageType::COMPILED_PARSE);
    msg.header.request_id = request_id;

    IPCCompiledParsePayload payload{};
    std::strncpy(payload.stmt_name, stmt_name.c_str(), sizeof(payload.stmt_name) - 1);
    payload.stmt_name[sizeof(payload.stmt_name) - 1] = '\0';
    payload.original_sql_length = static_cast<uint32_t>(original_sql.size());
    payload.bytecode_length = static_cast<uint32_t>(bytecode.size());

    msg.payload.resize(sizeof(payload) + original_sql.size() + bytecode.size());
    std::memcpy(msg.payload.data(), &payload, sizeof(payload));
    if (!original_sql.empty()) {
        std::memcpy(msg.payload.data() + sizeof(payload),
                    original_sql.data(),
                    original_sql.size());
    }
    if (!bytecode.empty()) {
        std::memcpy(msg.payload.data() + sizeof(payload) + original_sql.size(),
                    bytecode.data(),
                    bytecode.size());
    }

    return sendToEngine(client_id, msg, ctx);
}

core::Status ParserAgent::receiveFromEngine(uint32_t client_id, IPCMessage& msg,
                                           core::ErrorContext* ctx,
                                           uint32_t timeout_ms) {
    // Get client connection
    std::shared_lock<std::shared_mutex> lock(connections_mutex_);
    auto it = connections_.find(client_id);
    if (it == connections_.end()) {
        if (ctx) {
            ctx->set(core::Status::NOT_FOUND, "Client not found",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::NOT_FOUND;
    }
    
    ClientConnection* client = it->second.get();
    if (!client->ipc_channel) {
        if (ctx) {
            ctx->set(core::Status::NOT_FOUND, "No IPC channel for client",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::NOT_FOUND;
    }
    
    // Honor timeout when requested, otherwise use blocking receive.
    if (timeout_ms > 0) {
        return client->ipc_channel->tryReceive(msg, timeout_ms, ctx);
    }
    return client->ipc_channel->receive(msg, ctx);
}

std::unique_ptr<IPCChannel> ParserAgent::acquireIPCChannel() {
    std::lock_guard<std::mutex> lock(ipc_pool_mutex_);
    if (!ipc_channels_.empty()) {
        auto channel = std::move(ipc_channels_.back());
        ipc_channels_.pop_back();
        return channel;
    }

    if (config_.ipc_endpoint.empty()) {
        return nullptr;
    }

    auto channel = IPCChannelFactory::createDefault();
    if (!channel) {
        return nullptr;
    }

    core::ErrorContext ctx;
    const std::string parser_endpoint = deriveParserIpcEndpoint(config_.ipc_endpoint);
    if (!parser_endpoint.empty() &&
        channel->connect(parser_endpoint, &ctx) == core::Status::OK) {
        return channel;
    }

    core::ErrorContext fallback_ctx;
    if (channel->connect(config_.ipc_endpoint, &fallback_ctx) != core::Status::OK) {
        return nullptr;
    }

    return channel;
}

void ParserAgent::releaseIPCChannel(std::unique_ptr<IPCChannel> channel) {
    if (!channel) return;
    std::lock_guard<std::mutex> lock(ipc_pool_mutex_);
    ipc_channels_.push_back(std::move(channel));
}

uint64_t ParserAgent::getCurrentTimeMs() const {
    using namespace std::chrono;
    return duration_cast<milliseconds>(
        steady_clock::now().time_since_epoch()).count();
}

void ParserAgent::updateStats(const std::function<void(Stats&)>& updater) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    updater(stats_);
}

// ============================================================================
// NativeSBParserAgent Implementation
// ============================================================================

NativeSBParserAgent::NativeSBParserAgent(const ParserAgentConfig& config)
    : ParserAgent(config) {
}

NativeSBParserAgent::~NativeSBParserAgent() {
    stop();
}

core::Status NativeSBParserAgent::handleClient(int client_fd, core::ErrorContext* ctx) {
    // Create client connection record
    uint32_t client_id = next_client_id_++;
    auto client = std::make_unique<ClientConnection>();
    client->client_id = client_id;
    client->socket_fd = client_fd;
    client->connect_time_ms = getCurrentTimeMs();
    client->last_activity_ms = client->connect_time_ms;
    
    {
        std::unique_lock<std::shared_mutex> lock(connections_mutex_);
        connections_[client_id] = std::move(client);
    }
    
    updateStats([](Stats& s) { s.active_connections++; });
    
    // Handle SBWP startup
    auto it = connections_.find(client_id);
    if (it != connections_.end()) {
        auto status = handleStartup(*it->second, ctx);
        if (status != core::Status::OK) {
            disconnectClient(client_id);
            return status;
        }
    }
    
    // Client is now handled by its own thread or async I/O
    // For now, we keep it simple with blocking reads in a thread
    std::thread client_thread([this, client_id]() {
        auto it = connections_.find(client_id);
        if (it == connections_.end()) return;
        
        auto& client = *it->second;
        while (running_ && client.socket_fd >= 0) {
            // Read SBWP message header
            uint8_t header[5];
            ssize_t n = sb_socket_recv(client.socket_fd, header, 5, MSG_WAITALL);
            if (n <= 0) break;
            
            uint8_t msg_type = header[0];
            uint32_t msg_len = (header[1] << 24) | (header[2] << 16) |
                              (header[3] << 8) | header[4];
            
            std::vector<uint8_t> payload;
            if (msg_len > 0) {
                payload.resize(msg_len);
                n = sb_socket_recv(client.socket_fd, payload.data(), msg_len, MSG_WAITALL);
                if (n <= 0) break;
            }
            
            // Process message
            core::ErrorContext ctx;
            core::Status status = core::Status::OK;
            
            switch (msg_type) {
                case 0x10: { // SIMPLE_QUERY
                    std::string sql(reinterpret_cast<char*>(payload.data()));
                    status = handleQuery(client, sql, &ctx);
                    break;
                }
                case 0x11: { // PARSE
                    // Parse name\0sql\0 format
                    const char* data = reinterpret_cast<char*>(payload.data());
                    std::string stmt_name(data);
                    std::string sql(data + stmt_name.length() + 1);
                    status = handleParse(client, stmt_name, sql, &ctx);
                    break;
                }
                case 0x12: { // BIND
                    status = handleBind(client, "", "", &ctx);
                    break;
                }
                case 0x13: { // EXECUTE
                    status = handleExecute(client, "", 0, &ctx);
                    break;
                }
                case 0x15: { // CLOSE
                    status = handleClose(client, 'S', "", &ctx);
                    break;
                }
                case 0x16: { // SYNC
                    status = handleSync(client, &ctx);
                    break;
                }
                case 0x62: { // TERMINATE
                    handleTerminate(client, &ctx);
                    return;
                }
                default:
                    status = sendError(client, "0A000", "Unknown message type");
                    break;
            }
            
            if (status != core::Status::OK && status != core::Status::NOT_IMPLEMENTED) {
                break;
            }
            
            client.last_activity_ms = getCurrentTimeMs();
            updateStats([msg_len](Stats& s) {
                s.messages_received++;
                s.bytes_received += msg_len + 5;
            });
        }
        
        disconnectClient(client_id);
    });
    
    client_thread.detach();
    return core::Status::OK;
}

core::Status NativeSBParserAgent::handleStartup(ClientConnection& client, core::ErrorContext* ctx) {
    (void)ctx;
    // Read startup message
    uint8_t version[2];
    ssize_t n = sb_socket_recv(client.socket_fd, version, 2, MSG_WAITALL);
    if (n != 2) {
        return core::Status::CONNECTION_FAILURE;
    }
    
    uint16_t proto_ver = (version[0] << 8) | version[1];
    (void)proto_ver;
    
    // Read SSL mode
    uint8_t ssl_mode;
    n = sb_socket_recv(client.socket_fd, &ssl_mode, 1, MSG_WAITALL);
    if (n != 1) {
        return core::Status::CONNECTION_FAILURE;
    }
    
    // Read parameters until null
    char param_buf[1024];
    size_t pos = 0;
    while (pos < sizeof(param_buf) - 1) {
        n = sb_socket_recv(client.socket_fd, &param_buf[pos], 1, MSG_WAITALL);
        if (n != 1) break;
        if (param_buf[pos] == 0 && pos > 0 && param_buf[pos-1] == 0) {
            // Two consecutive nulls = end of parameters
            break;
        }
        pos++;
    }
    
    // Parse parameters
    const char* p = param_buf;
    while (*p && p < param_buf + pos) {
        std::string key(p);
        p += key.length() + 1;
        std::string value(p);
        p += value.length() + 1;
        
        if (key == "user") client.user = value;
        else if (key == "database") client.database = value;
    }
    
    // Send READY
    return sendReady(client, IPC_FEATURE_PREPARED_STATEMENTS | 
                           IPC_FEATURE_COPY_STREAMING | 
                           IPC_FEATURE_CANCEL);
}

core::Status NativeSBParserAgent::sendReady(ClientConnection& client, uint32_t features) {
    std::vector<uint8_t> response;
    response.push_back(0x04); // READY
    
    uint32_t len = 12; // session_id + features + version
    response.push_back((len >> 24) & 0xFF);
    response.push_back((len >> 16) & 0xFF);
    response.push_back((len >> 8) & 0xFF);
    response.push_back(len & 0xFF);
    
    // Session ID
    client.session_id = client.client_id;
    response.push_back((client.session_id >> 24) & 0xFF);
    response.push_back((client.session_id >> 16) & 0xFF);
    response.push_back((client.session_id >> 8) & 0xFF);
    response.push_back(client.session_id & 0xFF);
    
    // Features
    response.push_back((features >> 24) & 0xFF);
    response.push_back((features >> 16) & 0xFF);
    response.push_back((features >> 8) & 0xFF);
    response.push_back(features & 0xFF);
    
    // Version string placeholder
    response.push_back(0);
    response.push_back(0);
    response.push_back(0);
    response.push_back(0);
    
    ssize_t n = sb_socket_send(client.socket_fd, response.data(), response.size(), 0);
    if (n < 0) {
        return core::Status::IO_ERROR;
    }
    
    client.authenticated = true;
    return core::Status::OK;
}

core::Status NativeSBParserAgent::handleQuery(ClientConnection& client, 
                                             const std::string& sql,
                                             core::ErrorContext* ctx) {
    // Create IPC SIMPLE_QUERY message
    IPCMessage msg;
    msg.setType(IPCMessageType::SIMPLE_QUERY);
    msg.header.request_id = client.session_id;
    
    IPCSimpleQueryPayload payload;
    payload.query_length = static_cast<uint32_t>(sql.length());
    
    msg.payload.resize(sizeof(payload) + sql.length());
    std::memcpy(msg.payload.data(), &payload, sizeof(payload));
    std::memcpy(msg.payload.data() + sizeof(payload), sql.data(), sql.length());
    
    // Send to engine
    auto status = sendToEngine(client.client_id, msg, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    // Receive response
    IPCMessage response;
    status = receiveFromEngine(client.client_id, response, ctx, 30000);
    if (status != core::Status::OK) {
        return status;
    }
    
    // Forward response to client
    return forwardResponseToClient(client, response, ctx);
}

core::Status NativeSBParserAgent::handleParse(ClientConnection& client,
                                             const std::string& stmt_name,
                                             const std::string& sql,
                                             core::ErrorContext* ctx) {
    // Create IPC PARSE message
    IPCMessage msg;
    msg.setType(IPCMessageType::PARSE);
    msg.header.request_id = client.session_id;
    
    IPCParsePayload payload;
    std::strncpy(payload.stmt_name, stmt_name.c_str(), sizeof(payload.stmt_name) - 1);
    payload.stmt_name[sizeof(payload.stmt_name) - 1] = '\0';
    std::strncpy(payload.sql, sql.c_str(), sizeof(payload.sql) - 1);
    payload.sql[sizeof(payload.sql) - 1] = '\0';
    // param_types array is already zero-initialized
    
    msg.payload.resize(sizeof(payload));
    std::memcpy(msg.payload.data(), &payload, sizeof(payload));
    
    // Send to engine
    auto status = sendToEngine(client.client_id, msg, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    // Wait for PARSE_COMPLETE
    IPCMessage response;
    status = receiveFromEngine(client.client_id, response, ctx, 30000);
    if (status != core::Status::OK) {
        return status;
    }
    
    if (response.getType() == IPCMessageType::PARSE_COMPLETE) {
        return sendParseComplete(client);
    } else if (response.getType() == IPCMessageType::ERROR_RESPONSE) {
        return forwardResponseToClient(client, response, ctx);
    }
    
    return core::Status::OK;
}

core::Status NativeSBParserAgent::handleBind(ClientConnection& client,
                                            const std::string& portal_name,
                                            const std::string& stmt_name,
                                            core::ErrorContext* ctx) {
    // Create IPC BIND message
    IPCMessage msg;
    msg.setType(IPCMessageType::BIND);
    msg.header.request_id = client.session_id;
    
    IPCBindPayload payload;
    std::strncpy(payload.portal_name, portal_name.c_str(), sizeof(payload.portal_name) - 1);
    payload.portal_name[sizeof(payload.portal_name) - 1] = '\0';
    std::strncpy(payload.stmt_name, stmt_name.c_str(), sizeof(payload.stmt_name) - 1);
    payload.stmt_name[sizeof(payload.stmt_name) - 1] = '\0';
    payload.num_params = 0;
    
    msg.payload.resize(sizeof(payload));
    std::memcpy(msg.payload.data(), &payload, sizeof(payload));
    
    // Send to engine
    auto status = sendToEngine(client.client_id, msg, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    // Wait for BIND_COMPLETE
    IPCMessage response;
    status = receiveFromEngine(client.client_id, response, ctx, 30000);
    if (status != core::Status::OK) {
        return status;
    }
    
    if (response.getType() == IPCMessageType::BIND_COMPLETE) {
        return sendBindComplete(client);
    } else if (response.getType() == IPCMessageType::ERROR_RESPONSE) {
        return forwardResponseToClient(client, response, ctx);
    }
    
    return core::Status::OK;
}

core::Status NativeSBParserAgent::handleExecute(ClientConnection& client,
                                               const std::string& portal_name,
                                               uint32_t max_rows,
                                               core::ErrorContext* ctx) {
    // Create IPC EXECUTE message
    IPCMessage msg;
    msg.setType(IPCMessageType::EXECUTE);
    msg.header.request_id = client.session_id;
    
    IPCExecutePayload payload;
    std::strncpy(payload.portal_name, portal_name.c_str(), sizeof(payload.portal_name) - 1);
    payload.portal_name[sizeof(payload.portal_name) - 1] = '\0';
    payload.max_rows = max_rows;
    
    msg.payload.resize(sizeof(payload));
    std::memcpy(msg.payload.data(), &payload, sizeof(payload));
    
    // Send to engine
    auto status = sendToEngine(client.client_id, msg, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    // Receive and forward responses until COMMAND_COMPLETE or ERROR
    bool done = false;
    while (!done) {
        IPCMessage response;
        status = receiveFromEngine(client.client_id, response, ctx, 30000);
        if (status != core::Status::OK) {
            return status;
        }
        
        status = forwardResponseToClient(client, response, ctx);
        if (status != core::Status::OK) {
            return status;
        }
        
        if (response.getType() == IPCMessageType::COMMAND_COMPLETE ||
            response.getType() == IPCMessageType::ERROR_RESPONSE) {
            done = true;
        }
    }
    
    return core::Status::OK;
}

core::Status NativeSBParserAgent::handleClose(ClientConnection& client, char type,
                                             const std::string& name,
                                             core::ErrorContext* ctx) {
    // Create IPC CLOSE message
    IPCMessage msg;
    msg.setType(IPCMessageType::CLOSE);
    msg.header.request_id = client.session_id;
    
    IPCClosePayload payload;
    payload.type = type;
    std::strncpy(payload.name, name.c_str(), sizeof(payload.name) - 1);
    payload.name[sizeof(payload.name) - 1] = '\0';
    
    msg.payload.resize(sizeof(payload));
    std::memcpy(msg.payload.data(), &payload, sizeof(payload));
    
    // Send to engine
    auto status = sendToEngine(client.client_id, msg, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    // Wait for CLOSE_COMPLETE
    IPCMessage response;
    status = receiveFromEngine(client.client_id, response, ctx, 30000);
    if (status != core::Status::OK) {
        return status;
    }
    
    if (response.getType() == IPCMessageType::CLOSE_COMPLETE) {
        return sendCloseComplete(client);
    } else if (response.getType() == IPCMessageType::ERROR_RESPONSE) {
        return forwardResponseToClient(client, response, ctx);
    }
    
    return core::Status::OK;
}

core::Status NativeSBParserAgent::handleSync(ClientConnection& client, core::ErrorContext* ctx) {
    (void)client;
    (void)ctx;
    return core::Status::OK;
}

core::Status NativeSBParserAgent::handleTerminate(ClientConnection& client, 
                                                 core::ErrorContext* ctx) {
    (void)ctx;
    disconnectClient(client.client_id);
    return core::Status::OK;
}

core::Status NativeSBParserAgent::sendCommandComplete(ClientConnection& client,
                                                     const std::string& tag) {
    std::vector<uint8_t> response;
    response.push_back(0x22); // COMMAND_COMPLETE
    
    uint32_t len = tag.length() + 1;
    response.push_back((len >> 24) & 0xFF);
    response.push_back((len >> 16) & 0xFF);
    response.push_back((len >> 8) & 0xFF);
    response.push_back(len & 0xFF);
    
    response.insert(response.end(), tag.begin(), tag.end());
    response.push_back(0);
    
    ssize_t n = sb_socket_send(client.socket_fd, response.data(), response.size(), 0);
    if (n < 0) {
        return core::Status::IO_ERROR;
    }
    return core::Status::OK;
}

core::Status NativeSBParserAgent::sendError(ClientConnection& client,
                                           const char* sqlstate,
                                           const std::string& message) {
    std::vector<uint8_t> response;
    response.push_back(0x05); // ERROR
    
    uint32_t len = 6 + message.length() + 1;
    response.push_back((len >> 24) & 0xFF);
    response.push_back((len >> 16) & 0xFF);
    response.push_back((len >> 8) & 0xFF);
    response.push_back(len & 0xFF);
    
    // SQLSTATE
    response.insert(response.end(), sqlstate, sqlstate + 5);
    response.push_back(0);
    
    // Message
    response.insert(response.end(), message.begin(), message.end());
    response.push_back(0);
    
    ssize_t n = sb_socket_send(client.socket_fd, response.data(), response.size(), 0);
    if (n < 0) {
        return core::Status::IO_ERROR;
    }
    return core::Status::OK;
}

core::Status NativeSBParserAgent::sendRowDescription(ClientConnection& client,
                                                    const std::vector<IPCFieldDesc>& fields) {
    // SBWP ROW_DESCRIPTION: type(1) + length(4) + num_fields(2) + fields[]
    std::vector<uint8_t> response;
    response.push_back(0x20); // ROW_DESCRIPTION
    
    // Calculate total length
    uint32_t len = 2; // num_fields
    for (const auto& field : fields) {
        len += 2 + std::strlen(field.name) + 1 + 4; // namelen + name + null + type_oid
    }
    
    response.push_back((len >> 24) & 0xFF);
    response.push_back((len >> 16) & 0xFF);
    response.push_back((len >> 8) & 0xFF);
    response.push_back(len & 0xFF);
    
    // Number of fields
    uint16_t num_fields = static_cast<uint16_t>(fields.size());
    response.push_back((num_fields >> 8) & 0xFF);
    response.push_back(num_fields & 0xFF);
    
    // Fields
    for (const auto& field : fields) {
        // Name length (2 bytes) + name + null
        uint16_t name_len = static_cast<uint16_t>(std::strlen(field.name));
        response.push_back((name_len >> 8) & 0xFF);
        response.push_back(name_len & 0xFF);
        response.insert(response.end(), field.name, field.name + name_len);
        response.push_back(0);
        
        // Type OID (4 bytes)
        response.push_back((field.type_oid >> 24) & 0xFF);
        response.push_back((field.type_oid >> 16) & 0xFF);
        response.push_back((field.type_oid >> 8) & 0xFF);
        response.push_back(field.type_oid & 0xFF);
    }
    
    ssize_t n = sb_socket_send(client.socket_fd, response.data(), response.size(), 0);
    if (n < 0) {
        return core::Status::IO_ERROR;
    }
    return core::Status::OK;
}

core::Status NativeSBParserAgent::sendDataRow(ClientConnection& client,
                                             const std::vector<std::optional<std::string>>& values) {
    // SBWP DATA_ROW: type(1) + length(4) + num_fields(2) + field_lengths[] + data[]
    std::vector<uint8_t> response;
    response.push_back(0x21); // DATA_ROW
    
    // Calculate total length
    uint32_t len = 2; // num_fields
    for (const auto& val : values) {
        len += 4; // field length (int32)
        if (val) {
            len += val->length();
        }
    }
    
    response.push_back((len >> 24) & 0xFF);
    response.push_back((len >> 16) & 0xFF);
    response.push_back((len >> 8) & 0xFF);
    response.push_back(len & 0xFF);
    
    // Number of fields
    uint16_t num_fields = static_cast<uint16_t>(values.size());
    response.push_back((num_fields >> 8) & 0xFF);
    response.push_back(num_fields & 0xFF);
    
    // Field data
    for (const auto& val : values) {
        if (val) {
            // Length (4 bytes, signed)
            int32_t field_len = static_cast<int32_t>(val->length());
            response.push_back((field_len >> 24) & 0xFF);
            response.push_back((field_len >> 16) & 0xFF);
            response.push_back((field_len >> 8) & 0xFF);
            response.push_back(field_len & 0xFF);
            // Data
            response.insert(response.end(), val->begin(), val->end());
        } else {
            // NULL: length = -1
            response.push_back(0xFF); response.push_back(0xFF);
            response.push_back(0xFF); response.push_back(0xFF);
        }
    }
    
    ssize_t n = sb_socket_send(client.socket_fd, response.data(), response.size(), 0);
    if (n < 0) {
        return core::Status::IO_ERROR;
    }
    return core::Status::OK;
}

core::Status NativeSBParserAgent::sendNotice(ClientConnection& client, 
                                            const std::string& message) {
    // SBWP NOTICE: type(1) + length(4) + message + null
    std::vector<uint8_t> response;
    response.push_back(0x06); // NOTICE
    
    uint32_t len = message.length() + 1;
    response.push_back((len >> 24) & 0xFF);
    response.push_back((len >> 16) & 0xFF);
    response.push_back((len >> 8) & 0xFF);
    response.push_back(len & 0xFF);
    
    response.insert(response.end(), message.begin(), message.end());
    response.push_back(0);
    
    ssize_t n = sb_socket_send(client.socket_fd, response.data(), response.size(), 0);
    if (n < 0) {
        return core::Status::IO_ERROR;
    }
    return core::Status::OK;
}

core::Status NativeSBParserAgent::handleSSLRequest(ClientConnection& client, 
                                                  core::ErrorContext* ctx) {
    (void)ctx;
    // SBWP SSL negotiation: Send 'N' for "SSL not supported" (or 'S' for supported)
    // For now, we don't support SSL in the native parser
    char response = 'N';
    ssize_t n = sb_socket_send(client.socket_fd, &response, 1, 0);
    if (n < 0) {
        return core::Status::IO_ERROR;
    }
    return core::Status::OK;
}

core::Status NativeSBParserAgent::handleAuth(ClientConnection& client, 
                                            const std::string& method,
                                            const std::vector<uint8_t>& data,
                                            core::ErrorContext* ctx) {
    (void)client;
    (void)method;
    (void)data;
    (void)ctx;
    // Authentication is handled at the connection level
    // For now, just return OK (authentication already done during startup)
    return core::Status::OK;
}

// ============================================================================
// EmulatedParserAgent Implementation
// ============================================================================

EmulatedParserAgent::EmulatedParserAgent(const ParserAgentConfig& config,
                                         const std::string& target_protocol)
    : ParserAgent(config), target_protocol_(target_protocol) {
}

core::Status NativeSBParserAgent::sendParseComplete(ClientConnection& client) {
    std::vector<uint8_t> response;
    response.push_back(0x31); // PARSE_COMPLETE
    response.push_back(0); response.push_back(0); response.push_back(0); response.push_back(4);
    
    ssize_t n = sb_socket_send(client.socket_fd, response.data(), response.size(), 0);
    if (n < 0) {
        return core::Status::IO_ERROR;
    }
    return core::Status::OK;
}

core::Status NativeSBParserAgent::sendBindComplete(ClientConnection& client) {
    std::vector<uint8_t> response;
    response.push_back(0x32); // BIND_COMPLETE
    response.push_back(0); response.push_back(0); response.push_back(0); response.push_back(4);
    
    ssize_t n = sb_socket_send(client.socket_fd, response.data(), response.size(), 0);
    if (n < 0) {
        return core::Status::IO_ERROR;
    }
    return core::Status::OK;
}

core::Status NativeSBParserAgent::sendCloseComplete(ClientConnection& client) {
    std::vector<uint8_t> response;
    response.push_back(0x33); // CLOSE_COMPLETE
    response.push_back(0); response.push_back(0); response.push_back(0); response.push_back(4);
    
    ssize_t n = sb_socket_send(client.socket_fd, response.data(), response.size(), 0);
    if (n < 0) {
        return core::Status::IO_ERROR;
    }
    return core::Status::OK;
}

core::Status NativeSBParserAgent::forwardResponseToClient(ClientConnection& client,
                                                         const IPCMessage& response,
                                                         core::ErrorContext* ctx) {
    (void)ctx;
    
    switch (response.getType()) {
        case IPCMessageType::ROW_DESCRIPTION: {
            // Parse fields from response
            std::vector<IPCFieldDesc> fields;
            auto* payload = response.getPayload<IPCRowDescriptionPayload>();
            if (payload) {
                size_t offset = sizeof(IPCRowDescriptionPayload);
                const uint8_t* data = response.payload.data();
                size_t payload_size = response.payload.size();
                
                for (uint16_t i = 0; i < payload->num_fields && offset + sizeof(IPCFieldDesc) <= payload_size; i++) {
                    IPCFieldDesc field;
                    std::memcpy(&field, data + offset, sizeof(IPCFieldDesc));
                    fields.push_back(field);
                    offset += sizeof(IPCFieldDesc);
                }
            }
            return sendRowDescription(client, fields);
        }
        
        case IPCMessageType::DATA_ROW: {
            // Parse values from response
            std::vector<std::optional<std::string>> values;
            auto* payload = response.getPayload<IPCDataRowPayload>();
            if (payload) {
                size_t offset = sizeof(IPCDataRowPayload);
                const uint8_t* data = response.payload.data();
                size_t payload_size = response.payload.size();
                
                for (uint16_t i = 0; i < payload->num_fields && offset + sizeof(int32_t) <= payload_size; i++) {
                    int32_t len;
                    std::memcpy(&len, data + offset, sizeof(int32_t));
                    offset += sizeof(int32_t);
                    
                    if (len < 0) {
                        values.push_back(std::nullopt);
                    } else if (offset + len <= payload_size) {
                        values.push_back(std::string(reinterpret_cast<const char*>(data + offset), len));
                        offset += len;
                    } else {
                        values.push_back(std::nullopt);
                        break;
                    }
                }
            }
            return sendDataRow(client, values);
        }
        
        case IPCMessageType::COMMAND_COMPLETE: {
            auto* payload = response.getPayload<IPCCommandCompletePayload>();
            if (payload) {
                return sendCommandComplete(client, payload->tag);
            }
            return sendCommandComplete(client, "OK");
        }
        
        case IPCMessageType::ERROR_RESPONSE: {
            auto* payload = response.getPayload<IPCErrorPayload>();
            if (payload) {
                return sendError(client, payload->sqlstate, payload->message);
            }
            return sendError(client, "XX000", "Unknown error");
        }
        
        case IPCMessageType::READY_FOR_QUERY: {
            return sendReady(client, 0);
        }
        
        case IPCMessageType::PARSE_COMPLETE: {
            return sendParseComplete(client);
        }
        
        case IPCMessageType::BIND_COMPLETE: {
            return sendBindComplete(client);
        }
        
        case IPCMessageType::CLOSE_COMPLETE: {
            return sendCloseComplete(client);
        }
        
        default: {
            // Unknown message type - ignore
            return core::Status::OK;
        }
    }
}

EmulatedParserAgent::~EmulatedParserAgent() {
}

// Full implementations for PostgreSQLParserAgent, MySQLParserAgent, and 
// FirebirdParserAgent are in their respective source files:
//   - src/ipc/postgresql_parser_agent.cpp
//   - src/ipc/mysql_parser_agent.cpp  
//   - src/ipc/firebird_parser_agent.cpp

} // namespace ipc
} // namespace scratchbird
