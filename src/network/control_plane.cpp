/**
 * Control Plane Protocol Implementation
 */

#include "scratchbird/network/control_plane.h"

#include <cstring>

#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace scratchbird::network {

namespace {

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
