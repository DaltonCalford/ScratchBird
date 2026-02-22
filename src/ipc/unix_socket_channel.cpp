/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0
 */

/**
 * UnixSocketIPCChannel - Unix Domain Socket Implementation
 * 
 * Implements IPCChannel using Unix domain sockets for local
 * inter-process communication.
 * 
 * Features:
 * - Full-duplex communication
 * - Message framing (4-byte length prefix)
 * - Blocking and non-blocking I/O
 * - Timeout support
 * - Error handling
 */

#include "scratchbird/ipc/unix_socket_channel.h"
#include <sys/socket.h>
#include <sys/un.h>
#include "scratchbird/core/posix_compat.h"
#include <fcntl.h>
#include <poll.h>
#include <cstring>
#include <errno.h>

namespace scratchbird {
namespace ipc {

// ============================================================================
// UnixSocketIPCChannel Implementation
// ============================================================================

UnixSocketIPCChannel::UnixSocketIPCChannel()
    : fd_(-1),
      session_id_(0),
      connected_(false) {
}

UnixSocketIPCChannel::~UnixSocketIPCChannel() {
    if (connected_) {
        disconnect(nullptr);
    }
}

core::Status UnixSocketIPCChannel::connect(const std::string& endpoint,
                                          core::ErrorContext* ctx) {
    if (connected_) {
        if (ctx) {
            ctx->set(core::Status::CONNECTION_FAILURE, "Already connected",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::CONNECTION_FAILURE;
    }
    
    // Create socket
    fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd_ < 0) {
        if (ctx) {
            std::string error_msg = "Failed to create socket: " + std::string(strerror(errno));
            ctx->set(core::Status::IO_ERROR, error_msg.c_str(),
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::IO_ERROR;
    }
    
    // Set up address
    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    
    // Handle abstract socket names (Linux) or filesystem paths
    if (endpoint[0] == '@') {
        // Abstract socket (Linux-specific)
        addr.sun_path[0] = '\0';
        std::strncpy(addr.sun_path + 1, endpoint.c_str() + 1, sizeof(addr.sun_path) - 2);
    } else {
        std::strncpy(addr.sun_path, endpoint.c_str(), sizeof(addr.sun_path) - 1);
    }
    
    // Connect
    int result = ::connect(fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    if (result < 0) {
        if (ctx) {
            std::string error_msg = "Failed to connect: " + std::string(strerror(errno));
            ctx->set(core::Status::CONNECTION_FAILURE, error_msg.c_str(),
                    __FILE__, __LINE__, __func__);
        }
        close(fd_);
        fd_ = -1;
        return core::Status::CONNECTION_FAILURE;
    }
    
    endpoint_ = endpoint;
    connected_ = true;
    
    // Receive session ID from server
    uint32_t session_id;
    if (recv(fd_, &session_id, sizeof(session_id), MSG_WAITALL) == sizeof(session_id)) {
        session_id_ = session_id;
    }
    
    return core::Status::OK;
}

core::Status UnixSocketIPCChannel::accept(int listen_fd, core::ErrorContext* ctx) {
    if (connected_) {
        if (ctx) {
            ctx->set(core::Status::CONNECTION_FAILURE, "Already connected",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::CONNECTION_FAILURE;
    }
    
    struct sockaddr_un addr;
    socklen_t addr_len = sizeof(addr);
    
    fd_ = ::accept(listen_fd, reinterpret_cast<struct sockaddr*>(&addr), &addr_len);
    if (fd_ < 0) {
        if (ctx) {
            std::string error_msg = "Failed to accept: " + std::string(strerror(errno));
            ctx->set(core::Status::IO_ERROR, error_msg.c_str(),
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::IO_ERROR;
    }
    
    connected_ = true;
    
    // Generate and send session ID
    static std::atomic<uint32_t> next_session_id_{1};
    session_id_ = next_session_id_++;
    
    ::send(fd_, &session_id_, sizeof(session_id_), 0);
    
    return core::Status::OK;
}

core::Status UnixSocketIPCChannel::disconnect(core::ErrorContext* ctx) {
    (void)ctx;
    
    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }
    
    connected_ = false;
    session_id_ = 0;
    
    return core::Status::OK;
}

bool UnixSocketIPCChannel::isConnected() const {
    return connected_;
}

core::Status UnixSocketIPCChannel::send(const IPCMessage& msg,
                                       core::ErrorContext* ctx) {
    if (!connected_) {
        if (ctx) {
            ctx->set(core::Status::CONNECTION_DOES_NOT_EXIST, "Not connected",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::CONNECTION_DOES_NOT_EXIST;
    }
    
    // Serialize message
    std::vector<uint8_t> buffer;
    
    // Header: type (4) + request_id (4) + payload_len (4)
    buffer.resize(12);
    
    uint32_t type = static_cast<uint32_t>(msg.getType());
    std::memcpy(buffer.data(), &type, 4);
    std::memcpy(buffer.data() + 4, &msg.header.request_id, 4);
    
    uint32_t payload_len = msg.payload.size();
    std::memcpy(buffer.data() + 8, &payload_len, 4);
    
    // Append payload
    buffer.insert(buffer.end(), msg.payload.begin(), msg.payload.end());
    
    // Send with length prefix
    uint32_t total_len = buffer.size();
    
    // Send length
    ssize_t sent = ::send(fd_, &total_len, 4, MSG_NOSIGNAL);
    if (sent != 4) {
        if (ctx) {
            ctx->set(core::Status::IO_ERROR, "Failed to send message length",
                    __FILE__, __LINE__, __func__);
        }
        connected_ = false;
        return core::Status::IO_ERROR;
    }
    
    // Send data
    size_t offset = 0;
    while (offset < buffer.size()) {
        sent = ::send(fd_, buffer.data() + offset, buffer.size() - offset, MSG_NOSIGNAL);
        if (sent < 0) {
            if (errno == EINTR) continue;
            if (ctx) {
                std::string error_msg = "Failed to send message: " + std::string(strerror(errno));
                ctx->set(core::Status::IO_ERROR, error_msg.c_str(),
                        __FILE__, __LINE__, __func__);
            }
            connected_ = false;
            return core::Status::IO_ERROR;
        }
        offset += sent;
    }
    
    return core::Status::OK;
}

core::Status UnixSocketIPCChannel::receive(IPCMessage& msg,
                                          core::ErrorContext* ctx) {
    if (!connected_) {
        if (ctx) {
            ctx->set(core::Status::CONNECTION_DOES_NOT_EXIST, "Not connected",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::CONNECTION_DOES_NOT_EXIST;
    }
    
    // Read length
    uint32_t total_len;
    ssize_t received = recv(fd_, &total_len, 4, MSG_WAITALL);
    
    if (received == 0) {
        connected_ = false;
        return core::Status::CONNECTION_CLOSED;
    }
    
    if (received != 4) {
        if (ctx) {
            ctx->set(core::Status::IO_ERROR, "Failed to receive message length",
                    __FILE__, __LINE__, __func__);
        }
        connected_ = false;
        return core::Status::IO_ERROR;
    }
    
    if (total_len < 12 || total_len > 100 * 1024 * 1024) {  // Max 100MB
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT, "Invalid message size",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::INVALID_ARGUMENT;
    }
    
    // Read message data
    std::vector<uint8_t> buffer(total_len);
    size_t offset = 0;
    
    while (offset < total_len) {
        received = recv(fd_, buffer.data() + offset, total_len - offset, 0);
        if (received == 0) {
            connected_ = false;
            return core::Status::CONNECTION_CLOSED;
        }
        if (received < 0) {
            if (errno == EINTR) continue;
            if (ctx) {
                ctx->set(core::Status::IO_ERROR, "Failed to receive message",
                        __FILE__, __LINE__, __func__);
            }
            connected_ = false;
            return core::Status::IO_ERROR;
        }
        offset += received;
    }
    
    // Parse message
    uint32_t type;
    std::memcpy(&type, buffer.data(), 4);
    msg.setType(static_cast<IPCMessageType>(type));
    
    std::memcpy(&msg.header.request_id, buffer.data() + 4, 4);
    
    uint32_t payload_len;
    std::memcpy(&payload_len, buffer.data() + 8, 4);
    
    msg.payload.assign(buffer.begin() + 12, buffer.begin() + 12 + payload_len);
    
    return core::Status::OK;
}

core::Status UnixSocketIPCChannel::tryReceive(IPCMessage& msg,
                                             uint32_t timeout_ms,
                                             core::ErrorContext* ctx) {
    if (!connected_) {
        if (ctx) {
            ctx->set(core::Status::CONNECTION_DOES_NOT_EXIST, "Not connected",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::CONNECTION_DOES_NOT_EXIST;
    }
    
    // Use poll for timeout
    struct pollfd pfd;
    pfd.fd = fd_;
    pfd.events = POLLIN;
    
    int result = poll(&pfd, 1, timeout_ms);
    
    if (result < 0) {
        if (ctx) {
            ctx->set(core::Status::IO_ERROR, "Poll failed",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::IO_ERROR;
    }
    
    if (result == 0) {
        return core::Status::LOCK_TIMEOUT;
    }
    
    if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
        connected_ = false;
        return core::Status::CONNECTION_CLOSED;
    }
    
    return receive(msg, ctx);
}

std::string UnixSocketIPCChannel::getEndpoint() const {
    return endpoint_;
}

uint32_t UnixSocketIPCChannel::getSessionId() const {
    return session_id_;
}

core::Status UnixSocketIPCChannel::setNonBlocking(bool non_blocking) {
    if (fd_ < 0) {
        return core::Status::CONNECTION_DOES_NOT_EXIST;
    }
    
    int flags = fcntl(fd_, F_GETFL, 0);
    if (flags < 0) {
        return core::Status::IO_ERROR;
    }
    
    if (non_blocking) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    
    if (fcntl(fd_, F_SETFL, flags) < 0) {
        return core::Status::IO_ERROR;
    }
    
    return core::Status::OK;
}

core::Status UnixSocketIPCChannel::setSendTimeout(uint32_t timeout_ms) {
    if (fd_ < 0) {
        return core::Status::CONNECTION_DOES_NOT_EXIST;
    }
    
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    if (setsockopt(fd_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
        return core::Status::IO_ERROR;
    }
    
    return core::Status::OK;
}

core::Status UnixSocketIPCChannel::setReceiveTimeout(uint32_t timeout_ms) {
    if (fd_ < 0) {
        return core::Status::CONNECTION_DOES_NOT_EXIST;
    }
    
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    if (setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        return core::Status::IO_ERROR;
    }
    
    return core::Status::OK;
}

// ============================================================================
// IPCChannelFactory Implementation
// ============================================================================

std::unique_ptr<IPCChannel> IPCChannelFactory::create(IPCChannelType type) {
    switch (type) {
        case IPCChannelType::UNIX_SOCKET:
            return std::make_unique<UnixSocketIPCChannel>();
        
        // TODO: Implement other channel types
        case IPCChannelType::NAMED_PIPE:
        case IPCChannelType::TCP_LOOPBACK:
        case IPCChannelType::SHARED_MEMORY:
        default:
            return nullptr;
    }
}

std::unique_ptr<IPCChannel> IPCChannelFactory::createDefault() {
    return create(getDefaultType());
}

IPCChannelType IPCChannelFactory::getDefaultType() {
    #ifdef _WIN32
        return IPCChannelType::NAMED_PIPE;
    #else
        return IPCChannelType::UNIX_SOCKET;
    #endif
}

bool IPCChannelFactory::isSupported(IPCChannelType type) {
    switch (type) {
        case IPCChannelType::UNIX_SOCKET:
            #ifdef _WIN32
                return false;  // Windows 10 1803+ has AF_UNIX but we don't use it yet
            #else
                return true;
            #endif
        
        case IPCChannelType::NAMED_PIPE:
            #ifdef _WIN32
                return true;
            #else
                return false;
            #endif
        
        case IPCChannelType::TCP_LOOPBACK:
            return true;
        
        case IPCChannelType::SHARED_MEMORY:
            return false;  // Not implemented yet
        
        default:
            return false;
    }
}

} // namespace ipc
} // namespace scratchbird
