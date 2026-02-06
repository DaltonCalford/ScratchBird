/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0
 */
#pragma once

/**
 * UnixSocketIPCChannel - Unix Domain Socket Implementation
 * 
 * Implements IPCChannel using Unix domain sockets for local
 * inter-process communication.
 */

#include "scratchbird/ipc/ipc_contract_v1_1.h"
#include <atomic>

namespace scratchbird {
namespace ipc {

/**
 * Unix domain socket IPC channel implementation
 */
class UnixSocketIPCChannel : public IPCChannel {
public:
    UnixSocketIPCChannel();
    ~UnixSocketIPCChannel() override;

    // Non-copyable
    UnixSocketIPCChannel(const UnixSocketIPCChannel&) = delete;
    UnixSocketIPCChannel& operator=(const UnixSocketIPCChannel&) = delete;

    // ========================================================================
    // IPCChannel Interface
    // ========================================================================
    
    core::Status connect(const std::string& endpoint,
                        core::ErrorContext* ctx = nullptr) override;
    
    core::Status disconnect(core::ErrorContext* ctx = nullptr) override;
    
    bool isConnected() const override;
    
    core::Status send(const IPCMessage& msg,
                     core::ErrorContext* ctx = nullptr) override;
    
    core::Status receive(IPCMessage& msg,
                        core::ErrorContext* ctx = nullptr) override;
    
    core::Status tryReceive(IPCMessage& msg,
                           uint32_t timeout_ms,
                           core::ErrorContext* ctx = nullptr) override;
    
    std::string getEndpoint() const override;
    
    uint32_t getSessionId() const override;

    // ========================================================================
    // Server-side Methods
    // ========================================================================
    
    /**
     * Accept a connection from a listening socket
     */
    core::Status accept(int listen_fd, core::ErrorContext* ctx = nullptr);

    // ========================================================================
    // Configuration
    // ========================================================================
    
    /**
     * Set non-blocking mode
     */
    core::Status setNonBlocking(bool non_blocking);
    
    /**
     * Set send timeout
     */
    core::Status setSendTimeout(uint32_t timeout_ms);
    
    /**
     * Set receive timeout
     */
    core::Status setReceiveTimeout(uint32_t timeout_ms);
    
    /**
     * Get underlying file descriptor
     */
    int getFileDescriptor() const { return fd_; }

private:
    int fd_;
    std::string endpoint_;
    uint32_t session_id_;
    std::atomic<bool> connected_;
};

} // namespace ipc
} // namespace scratchbird
