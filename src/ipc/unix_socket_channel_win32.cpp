/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0
 */

#include "scratchbird/ipc/unix_socket_channel.h"

#ifdef _WIN32
namespace scratchbird {
namespace ipc {

UnixSocketIPCChannel::UnixSocketIPCChannel()
    : fd_(-1),
      session_id_(0),
      connected_(false) {
}

UnixSocketIPCChannel::~UnixSocketIPCChannel() = default;

core::Status UnixSocketIPCChannel::connect(const std::string& endpoint, core::ErrorContext* ctx) {
    (void)endpoint;
    if (ctx) {
        ctx->set(core::Status::NOT_SUPPORTED,
                 "Unix domain sockets are not supported on Windows",
                 __FILE__, __LINE__, __func__);
    }
    return core::Status::NOT_SUPPORTED;
}

core::Status UnixSocketIPCChannel::accept(int listen_fd, core::ErrorContext* ctx) {
    (void)listen_fd;
    if (ctx) {
        ctx->set(core::Status::NOT_SUPPORTED,
                 "Unix domain sockets are not supported on Windows",
                 __FILE__, __LINE__, __func__);
    }
    return core::Status::NOT_SUPPORTED;
}

core::Status UnixSocketIPCChannel::disconnect(core::ErrorContext* ctx) {
    (void)ctx;
    connected_ = false;
    fd_ = -1;
    endpoint_.clear();
    session_id_ = 0;
    return core::Status::OK;
}

bool UnixSocketIPCChannel::isConnected() const {
    return connected_;
}

core::Status UnixSocketIPCChannel::send(const IPCMessage& msg, core::ErrorContext* ctx) {
    (void)msg;
    if (ctx) {
        ctx->set(core::Status::NOT_SUPPORTED,
                 "Unix domain sockets are not supported on Windows",
                 __FILE__, __LINE__, __func__);
    }
    return core::Status::NOT_SUPPORTED;
}

core::Status UnixSocketIPCChannel::receive(IPCMessage& msg, core::ErrorContext* ctx) {
    (void)msg;
    if (ctx) {
        ctx->set(core::Status::NOT_SUPPORTED,
                 "Unix domain sockets are not supported on Windows",
                 __FILE__, __LINE__, __func__);
    }
    return core::Status::NOT_SUPPORTED;
}

core::Status UnixSocketIPCChannel::tryReceive(IPCMessage& msg,
                                              uint32_t timeout_ms,
                                              core::ErrorContext* ctx) {
    (void)msg;
    (void)timeout_ms;
    if (ctx) {
        ctx->set(core::Status::NOT_SUPPORTED,
                 "Unix domain sockets are not supported on Windows",
                 __FILE__, __LINE__, __func__);
    }
    return core::Status::NOT_SUPPORTED;
}

std::string UnixSocketIPCChannel::getEndpoint() const {
    return endpoint_;
}

uint32_t UnixSocketIPCChannel::getSessionId() const {
    return session_id_;
}

core::Status UnixSocketIPCChannel::setNonBlocking(bool non_blocking) {
    (void)non_blocking;
    return core::Status::NOT_SUPPORTED;
}

core::Status UnixSocketIPCChannel::setSendTimeout(uint32_t timeout_ms) {
    (void)timeout_ms;
    return core::Status::NOT_SUPPORTED;
}

core::Status UnixSocketIPCChannel::setReceiveTimeout(uint32_t timeout_ms) {
    (void)timeout_ms;
    return core::Status::NOT_SUPPORTED;
}

std::unique_ptr<IPCChannel> IPCChannelFactory::create(IPCChannelType type) {
    switch (type) {
        case IPCChannelType::UNIX_SOCKET:
            return nullptr;
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
    return IPCChannelType::NAMED_PIPE;
}

bool IPCChannelFactory::isSupported(IPCChannelType type) {
    switch (type) {
        case IPCChannelType::UNIX_SOCKET:
            return false;
        case IPCChannelType::NAMED_PIPE:
            return true;
        case IPCChannelType::TCP_LOOPBACK:
            return true;
        case IPCChannelType::SHARED_MEMORY:
        default:
            return false;
    }
}

} // namespace ipc
} // namespace scratchbird
#endif
