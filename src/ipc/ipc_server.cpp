/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0
 */

#include "scratchbird/ipc/ipc_server.h"
#include "scratchbird/ipc/unix_socket_channel.h"
#include "scratchbird/ipc/copy_flow_control.h"

#include <cstring>
#include <chrono>
#include <algorithm>

#if defined(__linux__) || defined(__APPLE__)
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#endif

namespace scratchbird {
namespace ipc {

// ============================================================================
// IPCServerConfig Implementation
// ============================================================================

IPCServerConfig::IPCServerConfig() {
#if defined(__linux__) || defined(__APPLE__)
    endpoint = "/tmp/scratchbird_ipc.sock";
#elif defined(_WIN32)
    endpoint = "\\\\.\\pipe\\ScratchBirdIPC";
#else
    endpoint = "127.0.0.1:5434";
#endif
}

// ============================================================================
// IPCSession Implementation
// ============================================================================

IPCSession::IPCSession(uint32_t id, std::unique_ptr<IPCChannel> channel,
                       IPCSessionHandler* handler)
    : id_(id), channel_(std::move(channel)), handler_(handler) {
    updateActivity();
}

IPCSession::~IPCSession() {
    stop();
}

core::Status IPCSession::start(core::ErrorContext* ctx) {
    state_ = SessionState::INITIALIZING;
    stats_.start_time_ms = getCurrentTimeMs();
    return core::Status::OK;
}

core::Status IPCSession::stop(core::ErrorContext* ctx) {
    state_ = SessionState::CLOSING;
    
    if (channel_) {
        channel_->disconnect(ctx);
    }
    
    state_ = SessionState::CLOSED;
    return core::Status::OK;
}

void IPCSession::shutdown() {
    std::lock_guard<std::mutex> lock(io_mutex_);
    if (channel_) {
        channel_->disconnect();
    }
    state_ = SessionState::CLOSED;
}

core::Status IPCSession::handleMessage(const IPCMessage& msg, core::ErrorContext* ctx) {
    updateActivity();
    stats_.messages_received++;
    
    switch (msg.getType()) {
        case IPCMessageType::STARTUP:
            return handleStartup(msg, ctx);
        case IPCMessageType::FEATURE_NEGOTIATE:
            return handleFeatureNegotiate(msg, ctx);
        case IPCMessageType::SIMPLE_QUERY:
            return handleSimpleQuery(msg, ctx);
        case IPCMessageType::PARSE:
            return handleParse(msg, ctx);
        case IPCMessageType::BIND:
            return handleBind(msg, ctx);
        case IPCMessageType::EXECUTE:
            return handleExecute(msg, ctx);
        case IPCMessageType::CLOSE:
            return handleClose(msg, ctx);
        case IPCMessageType::SYNC:
            return handleSync(msg, ctx);
        case IPCMessageType::TXN_BEGIN:
            return handleTxnBegin(msg, ctx);
        case IPCMessageType::TXN_COMMIT:
            return handleTxnCommit(msg, ctx);
        case IPCMessageType::TXN_ROLLBACK:
            return handleTxnRollback(msg, ctx);
        case IPCMessageType::SAVEPOINT:
            return handleSavepoint(msg, ctx);
        case IPCMessageType::COPY_DATA:
            return handleCopyData(msg, ctx);
        case IPCMessageType::COPY_DONE:
            return handleCopyDone(msg, ctx);
        case IPCMessageType::COPY_FAIL:
            return handleCopyFail(msg, ctx);
        case IPCMessageType::CANCEL_REQUEST:
            return handleCancelRequest(msg, ctx);
        case IPCMessageType::PING:
            return handlePing(msg, ctx);
        case IPCMessageType::TERMINATE:
            return handleTerminate(msg, ctx);
        case IPCMessageType::STREAM_CONTROL:
            return handleStreamControl(msg, ctx);
        default:
            if (ctx) {
                ctx->set(core::Status::NOT_SUPPORTED, "Unknown message type",
                        __FILE__, __LINE__, __func__);
            }
            return core::Status::NOT_SUPPORTED;
    }
}

core::Status IPCSession::sendMessage(const IPCMessage& msg, core::ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(io_mutex_);
    
    if (!channel_ || !channel_->isConnected()) {
        if (ctx) {
            ctx->set(core::Status::CONNECTION_FAILURE, "Channel not connected",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::CONNECTION_FAILURE;
    }
    
    auto status = channel_->send(msg, ctx);
    if (status == core::Status::OK) {
        stats_.messages_sent++;
    }
    return status;
}

SessionStats IPCSession::getStats() const {
    return stats_;
}

void IPCSession::updateActivity() {
    last_activity_ms_ = getCurrentTimeMs();
    stats_.last_activity_ms = last_activity_ms_;
}

bool IPCSession::isIdle() const {
    return state_ == SessionState::ACTIVE;
}

void IPCSession::clearQueryContext() {
    query_ctx_ = QueryContext();
}

uint64_t IPCSession::getCurrentTimeMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// Message handlers
core::Status IPCSession::handleStartup(const IPCMessage& msg, core::ErrorContext* ctx) {
    auto* payload = msg.getPayload<IPCStartupPayload>();
    if (!payload) {
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT, "Invalid STARTUP payload",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::INVALID_ARGUMENT;
    }
    
    state_ = SessionState::NEGOTIATING;
    
    auto status = handler_->onAttach(id_, *payload, ctx);
    if (status != core::Status::OK) {
        state_ = SessionState::INITIALIZING;
        return status;
    }
    
    // Send READY response
    uint32_t features = IPC_FEATURE_PREPARED_STATEMENTS |
                       IPC_FEATURE_COPY_STREAMING |
                       IPC_FEATURE_CANCEL |
                       IPC_FEATURE_BINARY_RESULTS;
    
    status = handler_->sendReady(id_, features);
    if (status == core::Status::OK) {
        state_ = SessionState::ACTIVE;
    }
    
    return status;
}

core::Status IPCSession::handleFeatureNegotiate(const IPCMessage& msg, core::ErrorContext* ctx) {
    // Feature negotiation already done in STARTUP
    // Send back agreed features
    IPCMessage response(IPCMessageType::FEATURE_NEGOTIATE, id_);
    auto* payload = response.getPayload<IPCFeaturePayload>();
    if (payload) {
        payload->client_features = 0;
        payload->server_features = IPC_FEATURE_PREPARED_STATEMENTS |
                                   IPC_FEATURE_COPY_STREAMING |
                                   IPC_FEATURE_CANCEL |
                                   IPC_FEATURE_BINARY_RESULTS;
        payload->agreed_features = payload->server_features;
    }
    return sendMessage(response, ctx);
}

core::Status IPCSession::handleSimpleQuery(const IPCMessage& msg, core::ErrorContext* ctx) {
    auto* payload = msg.getPayload<IPCSimpleQueryPayload>();
    if (!payload) {
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT, "Invalid SIMPLE_QUERY payload",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::INVALID_ARGUMENT;
    }
    
    state_ = SessionState::EXECUTING;
    
    // Extract SQL from payload (after the header)
    std::string sql;
    if (payload && payload->query_length > 0) {
        const char* sql_data = reinterpret_cast<const char*>(msg.payload.data() + sizeof(IPCSimpleQueryPayload));
        sql.assign(sql_data, payload->query_length);
    }
    
    QueryContext qctx;
    qctx.sql = sql;
    qctx.request_id = msg.header.request_id;
    qctx.start_time = std::chrono::steady_clock::now();
    setQueryContext(qctx);
    
    auto status = handler_->onSimpleQuery(id_, sql, ctx);
    
    clearQueryContext();
    state_ = SessionState::ACTIVE;
    
    if (status == core::Status::OK) {
        stats_.queries_executed++;
    } else {
        stats_.errors++;
    }
    
    return status;
}

core::Status IPCSession::handleParse(const IPCMessage& msg, core::ErrorContext* ctx) {
    auto* payload = msg.getPayload<IPCParsePayload>();
    if (!payload) {
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT, "Invalid PARSE payload",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::INVALID_ARGUMENT;
    }
    
    auto status = handler_->onParse(id_, payload->stmt_name, payload->sql, ctx);
    if (status == core::Status::OK) {
        status = handler_->sendParseComplete(id_);
    }
    return status;
}

core::Status IPCSession::handleBind(const IPCMessage& msg, core::ErrorContext* ctx) {
    auto* payload = msg.getPayload<IPCBindPayload>();
    if (!payload) {
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT, "Invalid BIND payload",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::INVALID_ARGUMENT;
    }
    
    auto status = handler_->onBind(id_, payload->portal_name, payload->stmt_name, ctx);
    if (status == core::Status::OK) {
        status = handler_->sendBindComplete(id_);
    }
    return status;
}

core::Status IPCSession::handleExecute(const IPCMessage& msg, core::ErrorContext* ctx) {
    auto* payload = msg.getPayload<IPCExecutePayload>();
    if (!payload) {
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT, "Invalid EXECUTE payload",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::INVALID_ARGUMENT;
    }
    
    state_ = SessionState::EXECUTING;
    
    QueryContext qctx;
    qctx.portal_name = payload->portal_name;
    qctx.max_rows = payload->max_rows;
    qctx.request_id = msg.header.request_id;
    qctx.start_time = std::chrono::steady_clock::now();
    setQueryContext(qctx);
    
    auto status = handler_->onExecute(id_, payload->portal_name, payload->max_rows, ctx);
    
    clearQueryContext();
    state_ = SessionState::ACTIVE;
    
    return status;
}

core::Status IPCSession::handleClose(const IPCMessage& msg, core::ErrorContext* ctx) {
    auto* payload = msg.getPayload<IPCClosePayload>();
    if (!payload) {
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT, "Invalid CLOSE payload",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::INVALID_ARGUMENT;
    }
    
    auto status = handler_->onClose(id_, payload->type, payload->name, ctx);
    if (status == core::Status::OK) {
        status = handler_->sendCloseComplete(id_);
    }
    return status;
}

core::Status IPCSession::handleSync(const IPCMessage& msg, core::ErrorContext* ctx) {
    (void)msg;
    return handler_->onSync(id_, ctx);
}

core::Status IPCSession::handleTxnBegin(const IPCMessage& msg, core::ErrorContext* ctx) {
    (void)msg;
    auto status = handler_->onBegin(id_, ctx);
    if (status == core::Status::OK) {
        stats_.transactions_started++;
        status = handler_->sendTxnComplete(id_);
    }
    return status;
}

core::Status IPCSession::handleTxnCommit(const IPCMessage& msg, core::ErrorContext* ctx) {
    (void)msg;
    auto status = handler_->onCommit(id_, ctx);
    if (status == core::Status::OK) {
        status = handler_->sendTxnComplete(id_);
    }
    return status;
}

core::Status IPCSession::handleTxnRollback(const IPCMessage& msg, core::ErrorContext* ctx) {
    (void)msg;
    auto status = handler_->onRollback(id_, ctx);
    if (status == core::Status::OK) {
        status = handler_->sendTxnComplete(id_);
    }
    return status;
}

core::Status IPCSession::handleSavepoint(const IPCMessage& msg, core::ErrorContext* ctx) {
    // Extract savepoint name from payload
    if (msg.payload.empty()) {
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT, "SAVEPOINT requires name",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::INVALID_ARGUMENT;
    }
    
    std::string name(reinterpret_cast<const char*>(msg.payload.data()));
    auto status = handler_->onSavepoint(id_, name, ctx);
    if (status == core::Status::OK) {
        status = handler_->sendTxnComplete(id_);
    }
    return status;
}

core::Status IPCSession::handleCopyData(const IPCMessage& msg, core::ErrorContext* ctx) {
    if (state_ != SessionState::COPY_IN) {
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT, "Not in COPY IN mode",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::INVALID_ARGUMENT;
    }
    
    auto* payload = msg.getPayload<IPCCopyDataPayload>();
    if (!payload) {
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT, "Invalid COPY_DATA payload",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::INVALID_ARGUMENT;
    }
    
    size_t data_offset = sizeof(IPCCopyDataPayload);
    if (msg.payload.size() < data_offset + payload->length) {
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT, "COPY_DATA length mismatch",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::INVALID_ARGUMENT;
    }
    
    // Flow control: Get or create flow controller for this session
    auto controller = CopyFlowControlManager::instance().getController(id_);
    if (!controller) {
        // Create with default settings: 10 credits, 1MB window
        controller = CopyFlowControlManager::instance().createController(
            id_, 10, 1024 * 1024);
    }
    
    // Check if we can accept more data (have credits)
    if (!controller->canSend(payload->length)) {
        // Send flow control pause
        IPCMessage control_msg;
        control_msg.setType(IPCMessageType::STREAM_CONTROL);
        control_msg.header.request_id = msg.header.request_id;
        
        IPCStreamControlPayload control_payload;
        control_payload.credits = 0;
        control_payload.buffer_avail = controller->getStats().buffer_available;
        
        control_msg.payload.resize(sizeof(control_payload));
        std::memcpy(control_msg.payload.data(), &control_payload, sizeof(control_payload));
        
        sendMessage(control_msg, ctx);
        
        // Wait for credits
        if (!controller->waitForCreditsWithTimeout(1, payload->length, 30000)) {
            if (ctx) {
                ctx->set(core::Status::LOCK_TIMEOUT,
                        "COPY flow control timeout waiting for credits",
                        __FILE__, __LINE__, __func__);
            }
            return core::Status::LOCK_TIMEOUT;
        }
    }
    
    // Acquire credits (decrements credit count)
    if (!controller->acquireCredits(payload->length)) {
        if (ctx) {
            ctx->set(core::Status::LOCK_TIMEOUT,
                    "Failed to acquire credits for COPY data",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::LOCK_TIMEOUT;
    }
    
    // Record receipt
    controller->recordReceived(payload->length);
    
    // Process the COPY data
    const uint8_t* data = msg.payload.data() + data_offset;
    auto status = handler_->onCopyData(id_, data, payload->length, ctx);
    
    if (status != core::Status::OK) {
        return status;
    }
    
    // Send updated flow control
    auto stats = controller->getStats();
    
    IPCMessage control_msg;
    control_msg.setType(IPCMessageType::STREAM_CONTROL);
    control_msg.header.request_id = msg.header.request_id;
    
    IPCStreamControlPayload control_payload;
    control_payload.credits = stats.credits_available;
    control_payload.buffer_avail = stats.buffer_available;
    
    control_msg.payload.resize(sizeof(control_payload));
    std::memcpy(control_msg.payload.data(), &control_payload, sizeof(control_payload));
    
    return sendMessage(control_msg, ctx);
}

core::Status IPCSession::handleCopyDone(const IPCMessage& msg, core::ErrorContext* ctx) {
    (void)msg;
    auto status = handler_->onCopyDone(id_, ctx);
    if (status == core::Status::OK) {
        state_ = SessionState::ACTIVE;
        status = handler_->sendCopyComplete(id_);
    }
    return status;
}

core::Status IPCSession::handleCopyFail(const IPCMessage& msg, core::ErrorContext* ctx) {
    std::string reason;
    if (!msg.payload.empty()) {
        reason = reinterpret_cast<const char*>(msg.payload.data());
    }
    
    auto status = handler_->onCopyFail(id_, reason, ctx);
    state_ = SessionState::ACTIVE;
    return status;
}

core::Status IPCSession::handleCancelRequest(const IPCMessage& msg, core::ErrorContext* ctx) {
    (void)msg;
    cancelQuery();
    return handler_->onCancel(id_, ctx);
}

core::Status IPCSession::handlePing(const IPCMessage& msg, core::ErrorContext* ctx) {
    (void)msg;
    IPCMessage response(IPCMessageType::PONG, id_);
    return sendMessage(response, ctx);
}

core::Status IPCSession::handleTerminate(const IPCMessage& msg, core::ErrorContext* ctx) {
    (void)msg;
    return stop(ctx);
}

core::Status IPCSession::handleStreamControl(const IPCMessage& msg, core::ErrorContext* ctx) {
    if (msg.payload.size() < sizeof(IPCStreamControlPayload)) {
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT,
                    "Invalid STREAM_CONTROL payload",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::INVALID_ARGUMENT;
    }
    
    const auto* payload = reinterpret_cast<const IPCStreamControlPayload*>(
        msg.payload.data()
    );
    
    {
        std::lock_guard<std::mutex> lock(flow_control_.mutex);
        flow_control_.credits = payload->credits;
        flow_control_.buffer_avail = payload->buffer_avail;
        flow_control_.last_update = std::chrono::steady_clock::now();
    }
    flow_control_.cv.notify_all();
    
    return core::Status::OK;
}

// ============================================================================
// IPCServer Implementation
// ============================================================================

IPCServer::IPCServer(const IPCServerConfig& config,
                     std::unique_ptr<IPCSessionHandler> handler)
    : config_(config), handler_(std::move(handler)) {
}

IPCServer::~IPCServer() {
    stop();
}

core::Status IPCServer::start(core::ErrorContext* ctx) {
    if (running_) {
        return core::Status::OK;
    }
    
    // Setup listener
    auto status = setupListener(ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    // Create workers
    for (uint32_t i = 0; i < config_.max_workers; i++) {
        auto worker = std::make_unique<IPCWorker>(i, this);
        workers_.push_back(std::move(worker));
    }
    
    // Start workers
    shutdown_ = false;
    running_ = true;
    
    for (auto& worker : workers_) {
        worker->start();
    }
    
    // Start accept thread
    accept_thread_ = std::thread(&IPCServer::acceptLoop, this);
    
    return core::Status::OK;
}

core::Status IPCServer::stop(core::ErrorContext* ctx) {
    (void)ctx;
    if (!running_) {
        return core::Status::OK;
    }
    
    running_ = false;
    shutdown_ = true;
    queue_cv_.notify_all();
    
    // Stop accept thread
    if (accept_thread_.joinable()) {
#if defined(__linux__) || defined(__APPLE__)
        if (listen_fd_ >= 0) {
            ::close(listen_fd_);
            listen_fd_ = -1;
        }
#endif
        accept_thread_.join();
    }
    
    // Stop workers
    for (auto& worker : workers_) {
        worker->stop();
    }
    
    // Close all sessions
    {
        std::unique_lock<std::shared_mutex> lock(sessions_mutex_);
        for (auto& [id, session] : sessions_) {
            session->stop(nullptr);
        }
        sessions_.clear();
    }
    
    return core::Status::OK;
}

core::Status IPCServer::broadcast(const IPCMessage& msg, core::ErrorContext* ctx) {
    std::shared_lock<std::shared_mutex> lock(sessions_mutex_);
    
    core::Status last_status = core::Status::OK;
    for (auto& [id, session] : sessions_) {
        auto status = session->sendMessage(msg, ctx);
        if (status != core::Status::OK) {
            last_status = status;
        }
    }
    
    return last_status;
}

core::Status IPCServer::sendTo(uint32_t session_id, const IPCMessage& msg,
                              core::ErrorContext* ctx) {
    std::shared_lock<std::shared_mutex> lock(sessions_mutex_);
    
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT, "Session not found",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::INVALID_ARGUMENT;
    }
    
    return it->second->sendMessage(msg, ctx);
}

core::Status IPCServer::closeSession(uint32_t session_id, core::ErrorContext* ctx) {
    destroySession(session_id);
    return core::Status::OK;
}

IPCServer::ServerStats IPCServer::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    ServerStats s = stats_;
    
    std::shared_lock<std::shared_mutex> session_lock(sessions_mutex_);
    s.active_sessions = static_cast<uint32_t>(sessions_.size());
    
    return s;
}

core::Status IPCServer::setupListener(core::ErrorContext* ctx) {
#if defined(__linux__) || defined(__APPLE__)
    // Unix domain socket
    listen_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        if (ctx) {
            ctx->set(core::Status::IO_ERROR, "Failed to create socket",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::IO_ERROR;
    }
    
    // Remove old socket file if exists
    unlink(config_.endpoint.c_str());
    
    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, config_.endpoint.c_str(), sizeof(addr.sun_path) - 1);
    
    if (bind(listen_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        if (ctx) {
            ctx->set(core::Status::IO_ERROR, "Failed to bind socket",
                    __FILE__, __LINE__, __func__);
        }
        ::close(listen_fd_);
        listen_fd_ = -1;
        return core::Status::IO_ERROR;
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
    
    return core::Status::OK;
#else
    // TCP fallback for other platforms
    (void)ctx;
    return core::Status::NOT_IMPLEMENTED;
#endif
}

void IPCServer::acceptLoop() {
    while (running_) {
#if defined(__linux__) || defined(__APPLE__)
        struct sockaddr_un client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_fd = accept(listen_fd_, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            if (!running_) break;
            continue;
        }
        
        // Create channel and accept connection
        auto channel = std::make_unique<UnixSocketIPCChannel>();
        core::ErrorContext ctx;
        if (channel->accept(client_fd, &ctx) != core::Status::OK) {
            ::close(client_fd);
            continue;
        }
        
        // Create session
        uint32_t session_id = createSession(std::move(channel));
        
        // Start reader thread for this session
        std::thread([this, session_id]() { sessionReadLoop(session_id); }).detach();
        
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.active_sessions++;
            stats_.total_sessions++;
        }
#else
        // Not implemented on other platforms
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
#endif
    }
}

void IPCServer::workerLoop(IPCWorker* worker) {
    (void)worker;
    while (!shutdown_) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        queue_cv_.wait(lock, [this] { return !message_queue_.empty() || shutdown_; });
        
        if (shutdown_) break;
        
        if (!message_queue_.empty()) {
            auto queued = std::move(message_queue_.front());
            message_queue_.pop();
            lock.unlock();
            
            // Process message
            std::shared_lock<std::shared_mutex> session_lock(sessions_mutex_);
            auto it = sessions_.find(queued.session_id);
            if (it != sessions_.end()) {
                session_lock.unlock();
                core::ErrorContext ctx;
                it->second->handleMessage(queued.message, &ctx);
            }
        }
    }
}

void IPCServer::cleanupIdleSessions() {
    std::unique_lock<std::shared_mutex> lock(sessions_mutex_);
    
    for (auto it = sessions_.begin(); it != sessions_.end();) {
        if (!it->second->isActive()) {
            it = sessions_.erase(it);
        } else {
            ++it;
        }
    }
}

uint32_t IPCServer::createSession(std::unique_ptr<IPCChannel> channel) {
    uint32_t id = next_session_id_++;
    
    auto session = std::make_shared<IPCSession>(id, std::move(channel), handler_.get());
    
    {
        std::unique_lock<std::shared_mutex> lock(sessions_mutex_);
        sessions_[id] = std::move(session);
    }
    
    return id;
}

void IPCServer::destroySession(uint32_t session_id) {
    std::shared_ptr<IPCSession> session;
    {
        std::shared_lock<std::shared_mutex> lock(sessions_mutex_);
        auto it = sessions_.find(session_id);
        if (it != sessions_.end()) {
            session = it->second;
        }
    }
    
    if (session) {
        session->setState(SessionState::CLOSING);
        session->shutdown();
        
        std::unique_lock<std::shared_mutex> lock(sessions_mutex_);
        sessions_.erase(session_id);
        
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.active_sessions--;
    }
}

std::shared_ptr<IPCSession> IPCServer::getSession(uint32_t session_id) {
    std::shared_lock<std::shared_mutex> lock(sessions_mutex_);
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
        return it->second;
    }
    return nullptr;
}

void IPCServer::enqueueMessage(uint32_t session_id, IPCMessage&& msg) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        message_queue_.push({session_id, std::move(msg)});
    }
    queue_cv_.notify_one();
}

void IPCServer::sessionReadLoop(uint32_t session_id) {
    std::shared_ptr<IPCSession> session;
    {
        std::shared_lock<std::shared_mutex> lock(sessions_mutex_);
        auto it = sessions_.find(session_id);
        if (it != sessions_.end()) {
            session = it->second;
        }
    }

    if (!session || !session->isActive()) return;

    core::ErrorContext ctx;
    while (running_ && session->isActive()) {
        IPCMessage msg;
        
        // Lock during receive (channel is not thread-safe)
        {
            std::lock_guard<std::mutex> io_lock(session->getIOMutex());
            auto status = session->getChannel()->receive(msg, &ctx);
            if (status != core::Status::OK) {
                break; // disconnect or error
            }
        }
        
        enqueueMessage(session_id, std::move(msg));
    }

    // Cleanup on disconnect
    destroySession(session_id);
}

// ============================================================================
// IPCWorker Implementation
// ============================================================================

IPCWorker::IPCWorker(uint32_t id, IPCServer* server)
    : id_(id), server_(server) {
}

IPCWorker::~IPCWorker() {
    stop();
}

void IPCWorker::start() {
    if (running_) return;
    running_ = true;
    thread_ = std::thread(&IPCServer::workerLoop, server_, this);
}

void IPCWorker::stop() {
    if (!running_) return;
    running_ = false;
    if (thread_.joinable()) {
        thread_.join();
    }
}

} // namespace ipc
} // namespace scratchbird
