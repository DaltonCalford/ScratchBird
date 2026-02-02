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
 * Control Plane Protocol (Listener <-> Parser)
 *
 * Defines message framing and a minimal control-plane server scaffold.
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/network/socket.h"
#include "scratchbird/network/socket_types.h"

namespace scratchbird::network {

constexpr uint32_t CONTROL_PLANE_MAGIC = 0x54434253; // "SBCT" little-endian
constexpr uint16_t CONTROL_PLANE_VERSION = 1;
constexpr uint16_t CONTROL_PLANE_FLAG_HAS_HANDLE = 0x0001;

enum class ControlPlaneMessageType : uint16_t {
    HELLO = 0x0001,
    HELLO_ACK = 0x0002,
    SPAWN_REQUEST = 0x0010,
    SPAWN_READY = 0x0011,
    HANDOFF_SOCKET = 0x0020,
    HANDOFF_ACK = 0x0021,
    HEALTH_CHECK = 0x0030,
    HEALTH_REPORT = 0x0031,
    POOL_STATS = 0x0040,
    RECYCLE = 0x0050,
    SHUTDOWN = 0x0051,
    ERROR = 0x00FF
};

struct ControlPlaneHeader {
    uint32_t magic = CONTROL_PLANE_MAGIC;
    uint16_t version = CONTROL_PLANE_VERSION;
    uint16_t message_type = 0;
    uint16_t flags = 0;
    uint16_t reserved = 0;
    uint64_t request_id = 0;
    uint64_t payload_len = 0;
};

struct ControlPlaneMessage {
    ControlPlaneHeader header;
    std::vector<uint8_t> payload;
};

bool encodeControlPlaneHeader(const ControlPlaneHeader& header, std::vector<uint8_t>& out);
bool decodeControlPlaneHeader(const uint8_t* data, size_t len, ControlPlaneHeader& header);

core::Status sendControlPlaneMessage(Socket& socket,
                                     const ControlPlaneMessage& message,
                                     socket_t send_fd = INVALID_SOCKET_VALUE,
                                     uint32_t target_pid = 0,
                                     core::ErrorContext* ctx = nullptr);
core::Status receiveControlPlaneMessage(Socket& socket,
                                        ControlPlaneMessage& message,
                                        socket_t* recv_fd = nullptr,
                                        core::ErrorContext* ctx = nullptr);

class ControlPlaneServer {
public:
    ControlPlaneServer() = default;
    ~ControlPlaneServer();

    core::Status start(const std::string& path, core::ErrorContext* ctx = nullptr);
    void stop();

    std::unique_ptr<Socket> accept(core::ErrorContext* ctx = nullptr);

    bool isRunning() const { return listener_ != nullptr; }
    const std::string& path() const { return path_; }

private:
    std::unique_ptr<Socket> listener_;
    std::string path_;
};

}  // namespace scratchbird::network
