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
 * Connection Handler Implementation
 *
 * Network-side connection lifecycle manager.
 *
 * Manages accepted sockets, per-connection state, and routing into protocol
 * handlers/listener front-door code.
 */

#include "scratchbird/network/connection_handler.h"
#include "scratchbird/core/error_context.h"

#include <algorithm>

namespace scratchbird {
namespace network {

// ============================================================================
// Connection Implementation
// ============================================================================

Connection::Connection(std::unique_ptr<Socket> socket, ConnectionId id)
    : id_(id), socket_(std::move(socket)),
      connected_at_(std::chrono::steady_clock::now()),
      last_activity_(connected_at_), last_read_(connected_at_), last_write_(connected_at_) {
    read_buffer_.reserve(DEFAULT_RECV_BUFFER_SIZE);
    write_buffer_.reserve(DEFAULT_SEND_BUFFER_SIZE);
}

Connection::~Connection() {
    close(CloseReason::NORMAL);
}

Connection::Connection(Connection&& other) noexcept
    : id_(other.id_), socket_(std::move(other.socket_)),
      state_(other.state_.load()), protocol_(other.protocol_),
      close_reason_(other.close_reason_),
      read_buffer_(std::move(other.read_buffer_)),
      write_buffer_(std::move(other.write_buffer_)),
      read_offset_(other.read_offset_), write_offset_(other.write_offset_),
      username_(std::move(other.username_)), database_(std::move(other.database_)),
      application_name_(std::move(other.application_name_)),
      read_timeout_(other.read_timeout_), write_timeout_(other.write_timeout_),
      idle_timeout_(other.idle_timeout_),
      connected_at_(other.connected_at_), last_activity_(other.last_activity_),
      last_read_(other.last_read_), last_write_(other.last_write_) {
    other.id_ = INVALID_CONNECTION_ID;
}

Connection& Connection::operator=(Connection&& other) noexcept {
    if (this != &other) {
        close();
        id_ = other.id_;
        socket_ = std::move(other.socket_);
        state_.store(other.state_.load());
        protocol_ = other.protocol_;
        close_reason_ = other.close_reason_;
        read_buffer_ = std::move(other.read_buffer_);
        write_buffer_ = std::move(other.write_buffer_);
        read_offset_ = other.read_offset_;
        write_offset_ = other.write_offset_;
        username_ = std::move(other.username_);
        database_ = std::move(other.database_);
        application_name_ = std::move(other.application_name_);
        read_timeout_ = other.read_timeout_;
        write_timeout_ = other.write_timeout_;
        idle_timeout_ = other.idle_timeout_;
        connected_at_ = other.connected_at_;
        last_activity_ = other.last_activity_;
        last_read_ = other.last_read_;
        last_write_ = other.last_write_;
        other.id_ = INVALID_CONNECTION_ID;
    }
    return *this;
}

ssize_t Connection::readIntoBuffer() {
    if (!socket_ || !socket_->isOpen()) {
        return -1;
    }

    size_t bytes_read = 0;
    const size_t old_size = read_buffer_.size();
    read_buffer_.resize(old_size + SOCKET_READ_CHUNK);
    auto status = socket_->read(read_buffer_.data() + old_size,
                                SOCKET_READ_CHUNK, &bytes_read);

    if (status != core::Status::OK) {
        if (status == core::Status::CONNECTION_FAILURE) {
            state_.store(ConnectionState::CLOSING, std::memory_order_release);
            return 0;
        }
        return -1;
    }

    if (bytes_read == 0) {
        return 0;  // Would block
    }

    // Trim buffer to actual data size
    read_buffer_.resize(old_size + bytes_read);

    stats_.bytes_received += bytes_read;
    last_read_ = last_activity_ = std::chrono::steady_clock::now();

    return static_cast<ssize_t>(bytes_read);
}

ssize_t Connection::writeFromBuffer() {
    if (!socket_ || !socket_->isOpen()) {
        return -1;
    }

    if (write_offset_ >= write_buffer_.size()) {
        return 0;  // Nothing to write
    }

    size_t bytes_written;
    auto status = socket_->write(write_buffer_.data() + write_offset_,
                                 write_buffer_.size() - write_offset_,
                                 &bytes_written);

    if (status != core::Status::OK) {
        return -1;
    }

    if (bytes_written == 0) {
        return 0;  // Would block
    }

    write_offset_ += bytes_written;
    stats_.bytes_sent += bytes_written;
    last_write_ = last_activity_ = std::chrono::steady_clock::now();

    // Clear buffer if fully written
    if (write_offset_ >= write_buffer_.size()) {
        write_buffer_.clear();
        write_offset_ = 0;
    }

    return static_cast<ssize_t>(bytes_written);
}

void Connection::consumeReadBuffer(size_t bytes) {
    if (bytes >= read_buffer_.size() - read_offset_) {
        read_buffer_.clear();
        read_offset_ = 0;
        return;
    }

    read_buffer_.erase(read_buffer_.begin() + read_offset_,
                       read_buffer_.begin() + read_offset_ + bytes);
    read_offset_ = 0;
}

void Connection::appendToWriteBuffer(const void* data, size_t size) {
    const uint8_t* ptr = static_cast<const uint8_t*>(data);
    write_buffer_.insert(write_buffer_.end(), ptr, ptr + size);
}

bool Connection::isReadTimedOut() const {
    if (read_timeout_.count() == 0) return false;
    auto elapsed = std::chrono::steady_clock::now() - last_read_;
    return elapsed > read_timeout_;
}

bool Connection::isWriteTimedOut() const {
    if (write_timeout_.count() == 0) return false;
    if (write_buffer_.empty()) return false;
    auto elapsed = std::chrono::steady_clock::now() - last_write_;
    return elapsed > write_timeout_;
}

bool Connection::isIdleTimedOut() const {
    if (idle_timeout_.count() == 0) return false;
    auto elapsed = std::chrono::steady_clock::now() - last_activity_;
    return elapsed > idle_timeout_;
}

void Connection::close(CloseReason reason) {
    if (getState() == ConnectionState::CLOSED) {
        return;
    }

    close_reason_ = reason;
    state_.store(ConnectionState::CLOSING, std::memory_order_release);

    if (socket_) {
        socket_->close();
    }

    state_.store(ConnectionState::CLOSED, std::memory_order_release);
}

// ============================================================================
// Connection Manager Implementation
// ============================================================================

ConnectionManager::ConnectionManager(EventLoop* event_loop, ThreadPool* thread_pool,
                                     const ConnectionManagerConfig& config)
    : event_loop_(event_loop), thread_pool_(thread_pool), config_(config) {}

ConnectionManager::~ConnectionManager() {
    closeAllConnections(CloseReason::SERVER_SHUTDOWN);
}

std::unique_ptr<ConnectionManager> ConnectionManager::create(
    EventLoop* event_loop,
    ThreadPool* thread_pool,
    const ConnectionManagerConfig& config,
    core::ErrorContext* ctx) {
    if (config.fixed_protocol == ProtocolType::AUTO_DETECT) {
        if (ctx) {
            SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT,
                              "ConnectionManager fixed_protocol must not be AUTO_DETECT");
        }
        return nullptr;
    }

    if (std::find(config.allowed_protocols.begin(),
                  config.allowed_protocols.end(),
                  config.fixed_protocol) == config.allowed_protocols.end()) {
        if (ctx) {
            SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT,
                              "ConnectionManager fixed_protocol is not in allowed_protocols");
        }
        return nullptr;
    }

    return std::unique_ptr<ConnectionManager>(
        new ConnectionManager(event_loop, thread_pool, config));
}

ConnectionId ConnectionManager::acceptConnection(std::unique_ptr<Socket> socket) {
    // Check connection limit
    if (isAtLimit()) {
        stats_.errors.fetch_add(1);
        return INVALID_CONNECTION_ID;
    }

    // Generate unique ID
    ConnectionId id = generateConnectionId();

    // Create connection
    auto conn = std::make_unique<Connection>(std::move(socket), id);

    // Set to non-blocking for event loop
    if (conn->getSocket()) {
        conn->getSocket()->setNonBlocking(true);
    }

    // Register with event loop
    if (event_loop_) {
        socket_t fd = conn->getFd();
        event_loop_->add(fd, EventType::READ, [this, id](const EventData& event) {
            handleEvent(id, event.events);
        });
    }

    // Store connection
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        connections_[id] = std::move(conn);
    }

    connection_count_.fetch_add(1);
    stats_.total_connections.fetch_add(1);
    stats_.active_connections.fetch_add(1);

    // Fire event
    fireEvent(ConnectionEvent(id, ConnectionEventType::CONNECTED));

    // Handle new connection
    handleNewConnection(getConnection(id));

    return id;
}

Connection* ConnectionManager::getConnection(ConnectionId id) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    auto it = connections_.find(id);
    if (it != connections_.end()) {
        return it->second.get();
    }
    return nullptr;
}

void ConnectionManager::closeConnection(ConnectionId id, CloseReason reason) {
    std::unique_ptr<Connection> conn;

    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        auto it = connections_.find(id);
        if (it == connections_.end()) {
            return;
        }

        conn = std::move(it->second);
        connections_.erase(it);
    }

    // Remove from event loop
    if (event_loop_ && conn) {
        event_loop_->remove(conn->getFd());
    }

    if (conn) {
        if (auto handler = getProtocolHandler(conn->getProtocol())) {
            handler->onConnectionClosed(conn.get(), reason);
        }
    }

    // Close connection
    if (conn) {
        conn->close(reason);
    }

    connection_count_.fetch_sub(1);
    stats_.active_connections.fetch_sub(1);

    // Fire event
    fireEvent(ConnectionEvent(id, ConnectionEventType::DISCONNECTED,
                              closeReasonToString(reason)));
}

void ConnectionManager::closeAllConnections(CloseReason reason) {
    std::vector<ConnectionId> ids;
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        ids.reserve(connections_.size());
        for (const auto& [id, conn] : connections_) {
            ids.push_back(id);
        }
    }

    for (auto id : ids) {
        closeConnection(id, reason);
    }
}

void ConnectionManager::handleEvent(ConnectionId id, EventType events) {
    Connection* conn = getConnection(id);
    if (!conn) {
        return;
    }

    if (hasEvent(events, EventType::ERROR_EVENT) || hasEvent(events, EventType::HANGUP)) {
        closeConnection(id, CloseReason::IO_ERROR);
        return;
    }

    if (hasEvent(events, EventType::READ)) {
        ssize_t bytes = conn->readIntoBuffer();
        if (bytes < 0) {
            closeConnection(id, CloseReason::IO_ERROR);
            return;
        }
        if (bytes == 0) {
            closeConnection(id, CloseReason::CLIENT_DISCONNECT);
            return;
        }

        if (bytes > 0) {
            fireEvent(ConnectionEvent(id, ConnectionEventType::DATA_RECEIVED));
            handleData(conn);
        }
    }

    if (hasEvent(events, EventType::WRITE)) {
        ssize_t bytes = conn->writeFromBuffer();
        if (bytes < 0) {
            closeConnection(id, CloseReason::IO_ERROR);
            return;
        }

        if (bytes > 0) {
            fireEvent(ConnectionEvent(id, ConnectionEventType::DATA_SENT));
        }

        // Update event loop interest if no more data to write
        if (!conn->hasPendingWrites() && event_loop_) {
            event_loop_->modify(conn->getFd(), EventType::READ);
        }
    }
}

void ConnectionManager::processPendingIO() {
    std::vector<ConnectionId> ids;
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        ids.reserve(connections_.size());
        for (const auto& [id, conn] : connections_) {
            ids.push_back(id);
        }
    }

    for (auto id : ids) {
        Connection* conn = getConnection(id);
        if (!conn) continue;

        // Try to write pending data
        if (conn->hasPendingWrites()) {
            conn->writeFromBuffer();
        }
    }
}

void ConnectionManager::checkTimeouts() {
    auto now = std::chrono::steady_clock::now();

    std::vector<ConnectionId> timed_out;

    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        for (const auto& [id, conn] : connections_) {
            // Check authentication timeout
            if (conn->getState() == ConnectionState::AUTHENTICATING) {
                auto elapsed = now - conn->getConnectedAt();
                if (elapsed > config_.auth_timeout) {
                    timed_out.push_back(id);
                    stats_.timeouts.fetch_add(1);
                    continue;
                }
            }

            // Check idle timeout
            if (conn->isIdleTimedOut()) {
                timed_out.push_back(id);
                stats_.timeouts.fetch_add(1);
            }
        }
    }

    for (auto id : timed_out) {
        fireEvent(ConnectionEvent(id, ConnectionEventType::TIMEOUT));
        closeConnection(id, CloseReason::TIMEOUT);
    }
}

void ConnectionManager::registerProtocolHandler(ProtocolType protocol,
                                                 std::shared_ptr<ProtocolHandler> handler) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    protocol_handlers_[protocol] = std::move(handler);
}

std::shared_ptr<ProtocolHandler> ConnectionManager::getProtocolHandler(ProtocolType protocol) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    auto it = protocol_handlers_.find(protocol);
    if (it != protocol_handlers_.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<ConnectionId> ConnectionManager::getConnectionIds() const {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    std::vector<ConnectionId> ids;
    ids.reserve(connections_.size());
    for (const auto& [id, conn] : connections_) {
        ids.push_back(id);
    }
    return ids;
}

void ConnectionManager::handleNewConnection(Connection* conn) {
    if (!conn) return;

    // Listener ports are single-protocol by design: fixed protocol is mandatory.
    if (config_.fixed_protocol == ProtocolType::AUTO_DETECT) {
        conn->close(CloseReason::PROTOCOL_ERROR);
        return;
    }

    if (!isProtocolAllowed(config_.fixed_protocol)) {
        conn->close(CloseReason::PROTOCOL_ERROR);
        return;
    }

    conn->setProtocol(config_.fixed_protocol);
    if (auto handler = getProtocolHandler(config_.fixed_protocol)) {
        handler->initializeConnection(conn);
    }
    conn->setState(ConnectionState::AUTHENTICATING);
}

void ConnectionManager::handleAuthentication(Connection* conn) {
    if (!conn) return;

    auto handler = getProtocolHandler(conn->getProtocol());
    if (!handler) {
        conn->close(CloseReason::INTERNAL_ERROR);
        return;
    }

    auto status = handler->handleAuthentication(conn);
    if (status != core::Status::OK) {
        stats_.auth_failures.fetch_add(1);
        fireEvent(ConnectionEvent(conn->getId(), ConnectionEventType::ERROR_EVENT,
                                  "Authentication failed"));
        conn->close(CloseReason::AUTH_FAILURE);
        return;
    }

    if (conn->getState() == ConnectionState::AUTHENTICATED) {
        fireEvent(ConnectionEvent(conn->getId(), ConnectionEventType::AUTHENTICATED));
        conn->setState(ConnectionState::READY);
        handler->sendReady(conn);
    }
}

void ConnectionManager::handleReady(Connection* conn) {
    if (!conn) return;

    // Connection is ready for queries - handled by protocol handler
}

void ConnectionManager::handleData(Connection* conn) {
    if (!conn) return;

    switch (conn->getState()) {
        case ConnectionState::NEW: {
            handleNewConnection(conn);
            if (conn->getState() == ConnectionState::AUTHENTICATING) {
                handleAuthentication(conn);
            }
            break;
        }

        case ConnectionState::PROTOCOL_DETECTION:
            conn->close(CloseReason::PROTOCOL_ERROR);
            break;

        case ConnectionState::AUTHENTICATING:
            handleAuthentication(conn);
            break;

        case ConnectionState::READY:
        case ConnectionState::PROCESSING: {
            auto handler = getProtocolHandler(conn->getProtocol());
            if (handler) {
                auto status = handler->handleData(conn);
                if (status != core::Status::OK) {
                    conn->close(CloseReason::PROTOCOL_ERROR);
                }
            }
            break;
        }

        default:
            break;
    }

    // Update event loop interest if we have pending writes
    if (conn->hasPendingWrites() && event_loop_) {
        event_loop_->modify(conn->getFd(), EventType::READ | EventType::WRITE);
    }
}

bool ConnectionManager::isProtocolAllowed(ProtocolType protocol) const {
    const auto& allowed = config_.allowed_protocols;
    return std::find(allowed.begin(), allowed.end(), protocol) != allowed.end();
}

void ConnectionManager::fireEvent(const ConnectionEvent& event) {
    if (event_callback_) {
        event_callback_(event);
    }
}

ConnectionId ConnectionManager::generateConnectionId() {
    return next_connection_id_.fetch_add(1);
}

} // namespace network
} // namespace scratchbird
