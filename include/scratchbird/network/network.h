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
 * ScratchBird Network Layer
 *
 * Phase 3.1 - Network Infrastructure
 *
 * This is the main header for the network layer, providing all necessary
 * components for TCP/Unix socket servers and client connections.
 *
 * Components:
 * - socket_types.h   - Common types, enums, and constants
 * - socket.h         - Platform-independent socket abstraction
 * - event_loop.h     - I/O event multiplexing (epoll/kqueue)
 * - thread_pool.h    - Worker thread pool
 * - connection_handler.h - Connection lifecycle and protocol routing
 */

#pragma once

// Core types and constants
#include "scratchbird/network/socket_types.h"

// Socket abstraction
#include "scratchbird/network/socket.h"

// Event loop
#include "scratchbird/network/event_loop.h"

// Thread pool
#include "scratchbird/network/thread_pool.h"

// Connection management
#include "scratchbird/network/connection_handler.h"

namespace scratchbird {
namespace network {

/**
 * Initialize the network subsystem
 *
 * Must be called before using any network functions.
 * On Windows, initializes Winsock. On other platforms, this is a no-op.
 *
 * @return true on success
 */
bool initNetwork();

/**
 * Cleanup the network subsystem
 *
 * Should be called when the application exits.
 */
void cleanupNetwork();

/**
 * Check if network subsystem is initialized
 */
bool isNetworkInitialized();

/**
 * Network initialization guard
 *
 * RAII helper for network initialization/cleanup.
 * Creates one in main() to ensure proper initialization.
 */
class NetworkInitGuard {
public:
    NetworkInitGuard() : initialized_(initNetwork()) {}
    ~NetworkInitGuard() { if (initialized_) cleanupNetwork(); }

    bool isInitialized() const { return initialized_; }

    // Non-copyable
    NetworkInitGuard(const NetworkInitGuard&) = delete;
    NetworkInitGuard& operator=(const NetworkInitGuard&) = delete;

private:
    bool initialized_;
};

} // namespace network
} // namespace scratchbird
