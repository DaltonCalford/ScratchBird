/**
 * Control Plane Protocol Implementation
 */

#include "scratchbird/network/control_plane.h"

#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#ifndef _WIN32
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>
#endif

namespace scratchbird::network {

namespace {

constexpr size_t CONTROL_PLANE_HEADER_SIZE = 28;
constexpr size_t CONTROL_PLANE_MAX_MESSAGE = 1024;

void appendU16(std::vector<uint8_t>& out, uint16_t value) {
    out.push_back(static_cast<uint8_t>(value & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
}

void appendU32(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>(value & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

void appendU64(std::vector<uint8_t>& out, uint64_t value) {
    for (int i = 0; i < 8; ++i) {
        out.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
    }
}

uint16_t readU16(const uint8_t* data) {
    return static_cast<uint16_t>(data[0] | (data[1] << 8));
}

uint32_t readU32(const uint8_t* data) {
    return static_cast<uint32_t>(data[0])
        | (static_cast<uint32_t>(data[1]) << 8)
        | (static_cast<uint32_t>(data[2]) << 16)
        | (static_cast<uint32_t>(data[3]) << 24);
}

uint64_t readU64(const uint8_t* data) {
    uint64_t value = 0;
    for (int i = 7; i >= 0; --i) {
        value = (value << 8) | data[i];
    }
    return value;
}

}  // namespace

bool encodeControlPlaneHeader(const ControlPlaneHeader& header, std::vector<uint8_t>& out) {
    out.clear();
    out.reserve(24);
    appendU32(out, header.magic);
    appendU16(out, header.version);
    appendU16(out, header.message_type);
    appendU16(out, header.flags);
    appendU16(out, header.reserved);
    appendU64(out, header.request_id);
    appendU64(out, header.payload_len);
    return true;
}

bool decodeControlPlaneHeader(const uint8_t* data, size_t len, ControlPlaneHeader& header) {
    if (len < 24 || data == nullptr) {
        return false;
    }
    header.magic = readU32(data);
    header.version = readU16(data + 4);
    header.message_type = readU16(data + 6);
    header.flags = readU16(data + 8);
    header.reserved = readU16(data + 10);
    header.request_id = readU64(data + 12);
    header.payload_len = readU64(data + 20);
    return true;
}

core::Status sendControlPlaneMessage(Socket& socket,
                                     const ControlPlaneMessage& message,
                                     socket_t send_fd,
                                     uint32_t target_pid,
                                     core::ErrorContext* ctx) {
    ControlPlaneHeader header = message.header;
    header.payload_len = message.payload.size();
    if (send_fd != INVALID_SOCKET_VALUE) {
        header.flags |= CONTROL_PLANE_FLAG_HAS_HANDLE;
    }

    std::vector<uint8_t> buffer;
    encodeControlPlaneHeader(header, buffer);
    buffer.insert(buffer.end(), message.payload.begin(), message.payload.end());

#ifdef _WIN32
    if (send_fd != INVALID_SOCKET_VALUE) {
        if (target_pid == 0) {
            SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT,
                              "Target PID required for WSADuplicateSocket");
            return core::Status::INVALID_ARGUMENT;
        }
        WSAPROTOCOL_INFO info{};
        if (WSADuplicateSocket(send_fd, target_pid, &info) != 0) {
            SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR, "WSADuplicateSocket failed");
            return core::Status::IO_ERROR;
        }
        auto info_bytes = reinterpret_cast<const uint8_t*>(&info);
        buffer.insert(buffer.end(), info_bytes, info_bytes + sizeof(info));
    }
    return socket.writeExact(buffer.data(), buffer.size(), ctx);
#else
    struct msghdr msg{};
    struct iovec iov{};
    iov.iov_base = buffer.data();
    iov.iov_len = buffer.size();
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    char control[CMSG_SPACE(sizeof(int))];
    if (send_fd != INVALID_SOCKET_VALUE) {
        msg.msg_control = control;
        msg.msg_controllen = sizeof(control);
        std::memset(control, 0, sizeof(control));
        struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int));
        std::memcpy(CMSG_DATA(cmsg), &send_fd, sizeof(int));
    }

    ssize_t sent = ::sendmsg(socket.getFd(), &msg, 0);
    if (sent < 0 || static_cast<size_t>(sent) != buffer.size()) {
        SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR, "sendmsg failed");
        return core::Status::IO_ERROR;
    }
    return core::Status::OK;
#endif
}

core::Status receiveControlPlaneMessage(Socket& socket,
                                        ControlPlaneMessage& message,
                                        socket_t* recv_fd,
                                        core::ErrorContext* ctx) {
    if (recv_fd) {
        *recv_fd = INVALID_SOCKET_VALUE;
    }

#ifdef _WIN32
    std::vector<uint8_t> header_buf(CONTROL_PLANE_HEADER_SIZE);
    core::Status status = socket.readExact(header_buf.data(), header_buf.size(), ctx);
    if (status != core::Status::OK) {
        return status;
    }
    if (!decodeControlPlaneHeader(header_buf.data(), header_buf.size(), message.header)) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION, "Invalid control header");
        return core::Status::PROTOCOL_VIOLATION;
    }
    if (message.header.payload_len > CONTROL_PLANE_MAX_MESSAGE) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION, "Control payload too large");
        return core::Status::PROTOCOL_VIOLATION;
    }
    message.payload.resize(static_cast<size_t>(message.header.payload_len));
    if (!message.payload.empty()) {
        status = socket.readExact(message.payload.data(), message.payload.size(), ctx);
        if (status != core::Status::OK) {
            return status;
        }
    }
    if ((message.header.flags & CONTROL_PLANE_FLAG_HAS_HANDLE) != 0) {
        WSAPROTOCOL_INFO info{};
        status = socket.readExact(&info, sizeof(info), ctx);
        if (status != core::Status::OK) {
            return status;
        }
        SOCKET dup = WSASocket(FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO,
                               &info, 0, 0);
        if (dup == INVALID_SOCKET_VALUE) {
            SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR, "WSASocket failed");
            return core::Status::IO_ERROR;
        }
        if (recv_fd) {
            *recv_fd = dup;
        } else {
            closesocket(dup);
        }
    }
    return core::Status::OK;
#else
    std::vector<uint8_t> header_buf(CONTROL_PLANE_HEADER_SIZE);
    struct msghdr msg{};
    struct iovec iov{};
    iov.iov_base = header_buf.data();
    iov.iov_len = header_buf.size();
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    char control[CMSG_SPACE(sizeof(int))];
    msg.msg_control = control;
    msg.msg_controllen = sizeof(control);

    ssize_t received = ::recvmsg(socket.getFd(), &msg, MSG_WAITALL);
    if (received <= 0) {
        SET_ERROR_CONTEXT(ctx, core::Status::CONNECTION_FAILURE, "recvmsg failed");
        return core::Status::CONNECTION_FAILURE;
    }
    if (static_cast<size_t>(received) < CONTROL_PLANE_HEADER_SIZE) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION, "Short control header");
        return core::Status::PROTOCOL_VIOLATION;
    }

    for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
         cmsg != nullptr;
         cmsg = CMSG_NXTHDR(&msg, cmsg)) {
        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
            if (recv_fd) {
                std::memcpy(recv_fd, CMSG_DATA(cmsg), sizeof(int));
            }
        }
    }

    if (!decodeControlPlaneHeader(header_buf.data(), header_buf.size(), message.header)) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION, "Invalid control header");
        return core::Status::PROTOCOL_VIOLATION;
    }

    if (message.header.payload_len > CONTROL_PLANE_MAX_MESSAGE) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION, "Control payload too large");
        return core::Status::PROTOCOL_VIOLATION;
    }

    message.payload.resize(static_cast<size_t>(message.header.payload_len));
    if (!message.payload.empty()) {
        core::Status status = socket.readExact(message.payload.data(), message.payload.size(), ctx);
        if (status != core::Status::OK) {
            return status;
        }
    }
    return core::Status::OK;
#endif
}

ControlPlaneServer::~ControlPlaneServer() {
    stop();
}

core::Status ControlPlaneServer::start(const std::string& path, core::ErrorContext* ctx) {
    if (path.empty()) {
        SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT, "Control socket path required");
        return core::Status::INVALID_ARGUMENT;
    }

#ifdef _WIN32
    (void)path;
    SET_ERROR_CONTEXT(ctx, core::Status::NOT_IMPLEMENTED,
                      "Control-plane sockets not implemented on Windows");
    return core::Status::NOT_IMPLEMENTED;
#else
    path_ = path;

    // Remove any stale socket file
    ::unlink(path.c_str());

    auto socket = Socket::create(AddressFamily::UNIX, SocketType::STREAM, ctx);
    if (!socket) {
        return core::Status::IO_ERROR;
    }

    NetworkAddress address;
    address.family = AddressFamily::UNIX;
    address.path = path;

    core::Status status = socket->bind(address, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    status = socket->listen(DEFAULT_LISTEN_BACKLOG, ctx);
    if (status != core::Status::OK) {
        return status;
    }

    listener_ = std::move(socket);
    return core::Status::OK;
#endif
}

void ControlPlaneServer::stop() {
    if (listener_) {
        listener_->close();
        listener_.reset();
    }
#ifndef _WIN32
    if (!path_.empty()) {
        ::unlink(path_.c_str());
    }
#endif
    path_.clear();
}

std::unique_ptr<Socket> ControlPlaneServer::accept(core::ErrorContext* ctx) {
    if (!listener_) {
        SET_ERROR_CONTEXT(ctx, core::Status::CONNECTION_FAILURE, "Control-plane listener not running");
        return nullptr;
    }
    return listener_->accept(nullptr, ctx);
}

}  // namespace scratchbird::network
