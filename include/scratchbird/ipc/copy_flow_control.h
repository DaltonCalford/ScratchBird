/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0
 */
#pragma once

/**
 * COPY Flow Control
 * 
 * Implements credit-based flow control for COPY operations to prevent
 * memory exhaustion and provide backpressure.
 */

#include "scratchbird/ipc/ipc_server.h"
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <atomic>

namespace scratchbird {
namespace ipc {

/**
 * Per-session flow controller for COPY operations
 */
class CopyFlowController {
public:
    struct Stats {
        uint32_t session_id = 0;
        uint32_t credits_available = 0;
        uint32_t buffer_available = 0;
        uint64_t total_bytes_received = 0;
        uint64_t total_bytes_sent = 0;
        uint32_t max_window_size = 0;
        uint64_t throughput_bps = 0;
        bool is_paused = false;
    };

    /**
     * Create flow controller
     * 
     * @param session_id Session ID
     * @param initial_credits Initial credit count
     * @param max_window_size Maximum window size in bytes
     */
    CopyFlowController(uint32_t session_id,
                      uint32_t initial_credits,
                      uint32_t max_window_size);
    
    ~CopyFlowController();

    // Non-copyable
    CopyFlowController(const CopyFlowController&) = delete;
    CopyFlowController& operator=(const CopyFlowController&) = delete;

    /**
     * Check if sender can send data
     */
    bool canSend(uint32_t bytes);
    
    /**
     * Acquire credits to send data
     * @return true if credits acquired
     */
    bool acquireCredits(uint32_t bytes);
    
    /**
     * Release credits back to sender
     */
    void releaseCredits(uint32_t credits, uint32_t buffer_freed);
    
    /**
     * Wait for credits to become available
     */
    void waitForCredits(uint32_t min_credits, uint32_t min_buffer);
    
    /**
     * Wait for credits with timeout
     * @return true if credits available, false if timeout
     */
    bool waitForCreditsWithTimeout(uint32_t min_credits,
                                   uint32_t min_buffer,
                                   uint32_t timeout_ms);
    
    /**
     * Update buffer availability
     */
    void updateBufferAvailability(uint32_t buffer_avail);
    
    /**
     * Pause data flow
     */
    void pause();
    
    /**
     * Resume data flow
     */
    void resume();
    
    /**
     * Check if flow is paused
     */
    bool isPaused() const;
    
    /**
     * Record received bytes
     */
    void recordReceived(uint32_t bytes);
    
    /**
     * Get statistics
     */
    Stats getStats() const;
    
    /**
     * Reset controller state
     */
    void reset();

private:
    uint32_t session_id_;
    
    // Credit-based flow control
    uint32_t credits_;
    uint32_t max_window_size_;
    
    // Buffer tracking
    uint32_t buffer_available_;
    
    // Statistics
    uint64_t total_received_;
    uint64_t total_sent_;
    
    // Control
    bool paused_;
    std::chrono::steady_clock::time_point last_reset_time_;
    
    // Synchronization
    mutable std::mutex mutex_;
    std::condition_variable cv_;
};

/**
 * Manager for all COPY flow controllers
 */
class CopyFlowControlManager {
public:
    static CopyFlowControlManager& instance();

    /**
     * Create flow controller for session
     */
    std::shared_ptr<CopyFlowController> createController(
        uint32_t session_id,
        uint32_t initial_credits = 10,
        uint32_t max_window_size = 1024 * 1024);
    
    /**
     * Get existing controller
     */
    std::shared_ptr<CopyFlowController> getController(uint32_t session_id);
    
    /**
     * Destroy controller
     */
    void destroyController(uint32_t session_id);
    
    /**
     * Pause all COPY operations
     */
    void pauseAll();
    
    /**
     * Resume all COPY operations
     */
    void resumeAll();
    
    /**
     * Get stats for all controllers
     */
    std::vector<CopyFlowController::Stats> getAllStats() const;
    
    /**
     * Clean up expired controllers
     */
    void cleanupExpired();

private:
    CopyFlowControlManager() = default;
    
    mutable std::shared_mutex mutex_;
    std::unordered_map<uint32_t, std::weak_ptr<CopyFlowController>> controllers_;
};

/**
 * IPCSession extension for COPY flow control
 */
class IPCSessionWithFlowControl : public IPCSession {
public:
    using IPCSession::IPCSession;
    
    /**
     * Handle COPY data with flow control
     */
    core::Status handleCopyDataWithFlowControl(const IPCMessage& msg,
                                              core::ErrorContext* ctx);
};

} // namespace ipc
} // namespace scratchbird
