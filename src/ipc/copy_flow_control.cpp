/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0
 */

/**
 * COPY Flow Control Implementation
 * 
 * Implements flow control for COPY operations to prevent memory exhaustion
 * and provide backpressure. Uses a credit-based system where the receiver
 * grants credits to the sender.
 * 
 * Features:
 * - Credit-based flow control
 * - Buffer availability tracking
 * - Dynamic window sizing
 * - Per-session flow control
 */

#include "scratchbird/ipc/copy_flow_control.h"
#include <algorithm>

namespace scratchbird {
namespace ipc {

// ============================================================================
// CopyFlowController Implementation
// ============================================================================

CopyFlowController::CopyFlowController(uint32_t session_id,
                                       uint32_t initial_credits,
                                       uint32_t max_window_size)
    : session_id_(session_id),
      credits_(initial_credits),
      max_window_size_(max_window_size),
      buffer_available_(max_window_size),
      total_received_(0),
      total_sent_(0),
      paused_(false) {
}

CopyFlowController::~CopyFlowController() {
}

bool CopyFlowController::canSend(uint32_t bytes) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (paused_) {
        return false;
    }
    
    // Check both credits and buffer availability
    return credits_ > 0 && buffer_available_ >= bytes;
}

bool CopyFlowController::acquireCredits(uint32_t bytes) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (paused_) {
        return false;
    }
    
    if (credits_ == 0 || buffer_available_ < bytes) {
        return false;
    }
    
    // Deduct credits (1 credit = 1 chunk, not bytes)
    if (credits_ > 0) {
        credits_--;
    }
    
    buffer_available_ -= bytes;
    total_sent_ += bytes;
    
    return true;
}

void CopyFlowController::releaseCredits(uint32_t credits, uint32_t buffer_freed) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    credits_ = std::min(credits_ + credits, max_window_size_);
    buffer_available_ = std::min(buffer_available_ + buffer_freed, max_window_size_);
    
    // Notify waiters
    cv_.notify_all();
}

void CopyFlowController::waitForCredits(uint32_t min_credits, uint32_t min_buffer) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    cv_.wait(lock, [this, min_credits, min_buffer]() {
        return credits_ >= min_credits && buffer_available_ >= min_buffer;
    });
}

bool CopyFlowController::waitForCreditsWithTimeout(uint32_t min_credits,
                                                   uint32_t min_buffer,
                                                   uint32_t timeout_ms) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    return cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                       [this, min_credits, min_buffer]() {
                           return credits_ >= min_credits && buffer_available_ >= min_buffer;
                       });
}

void CopyFlowController::updateBufferAvailability(uint32_t buffer_avail) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    buffer_available_ = std::min(buffer_avail, max_window_size_);
    
    if (buffer_available_ > 0) {
        cv_.notify_all();
    }
}

void CopyFlowController::pause() {
    std::lock_guard<std::mutex> lock(mutex_);
    paused_ = true;
}

void CopyFlowController::resume() {
    std::lock_guard<std::mutex> lock(mutex_);
    paused_ = false;
    cv_.notify_all();
}

bool CopyFlowController::isPaused() const {
    return paused_;
}

void CopyFlowController::recordReceived(uint32_t bytes) {
    std::lock_guard<std::mutex> lock(mutex_);
    total_received_ += bytes;
}

CopyFlowController::Stats CopyFlowController::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Stats stats;
    stats.session_id = session_id_;
    stats.credits_available = credits_;
    stats.buffer_available = buffer_available_;
    stats.total_bytes_received = total_received_;
    stats.total_bytes_sent = total_sent_;
    stats.max_window_size = max_window_size_;
    stats.is_paused = paused_;
    
    // Calculate throughput (bytes per second over last window)
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - last_reset_time_).count();
    
    if (elapsed > 0) {
        stats.throughput_bps = (total_received_ + total_sent_) / elapsed;
    } else {
        stats.throughput_bps = 0;
    }
    
    return stats;
}

void CopyFlowController::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    credits_ = max_window_size_ / 2;  // Reset to half window
    buffer_available_ = max_window_size_;
    total_received_ = 0;
    total_sent_ = 0;
    paused_ = false;
    last_reset_time_ = std::chrono::steady_clock::now();
}

// ============================================================================
// CopyFlowControlManager Implementation
// ============================================================================

CopyFlowControlManager& CopyFlowControlManager::instance() {
    static CopyFlowControlManager instance;
    return instance;
}

std::shared_ptr<CopyFlowController> CopyFlowControlManager::createController(
    uint32_t session_id,
    uint32_t initial_credits,
    uint32_t max_window_size) {
    
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    auto controller = std::make_shared<CopyFlowController>(
        session_id, initial_credits, max_window_size);
    
    controllers_[session_id] = controller;
    
    return controller;
}

std::shared_ptr<CopyFlowController> CopyFlowControlManager::getController(
    uint32_t session_id) {
    
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    auto it = controllers_.find(session_id);
    if (it != controllers_.end()) {
        return it->second.lock();
    }
    
    return nullptr;
}

void CopyFlowControlManager::destroyController(uint32_t session_id) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    controllers_.erase(session_id);
}

void CopyFlowFlowControlManager::pauseAll() {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    for (auto& pair : controllers_) {
        auto controller = pair.second.lock();
        if (controller) {
            controller->pause();
        }
    }
}

void CopyFlowControlManager::resumeAll() {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    for (auto& pair : controllers_) {
        auto controller = pair.second.lock();
        if (controller) {
            controller->resume();
        }
    }
}

std::vector<CopyFlowController::Stats> CopyFlowControlManager::getAllStats() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    std::vector<CopyFlowController::Stats> stats;
    
    for (const auto& pair : controllers_) {
        auto controller = pair.second.lock();
        if (controller) {
            stats.push_back(controller->getStats());
        }
    }
    
    return stats;
}

void CopyFlowControlManager::cleanupExpired() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    for (auto it = controllers_.begin(); it != controllers_.end();) {
        if (it->second.expired()) {
            it = controllers_.erase(it);
        } else {
            ++it;
        }
    }
}

// ============================================================================
// Integration with IPCSession
// ============================================================================

core::Status IPCSession::handleCopyDataWithFlowControl(const IPCMessage& msg,
                                                      core::ErrorContext* ctx) {
    // Get or create flow controller for this session
    auto controller = CopyFlowControlManager::instance().getController(id_);
    if (!controller) {
        // Create with default settings: 10 credits, 1MB window
        controller = CopyFlowControlManager::instance().createController(
            id_, 10, 1024 * 1024);
    }
    
    // Extract data from message
    const auto* payload = reinterpret_cast<const IPCCopyDataPayload*>(msg.payload.data());
    uint32_t data_len = msg.payload.size() - sizeof(IPCCopyDataPayload) + 1;
    
    // Check if we can accept more data
    if (!controller->canSend(data_len)) {
        // Send flow control message to pause sender
        IPCMessage control_msg;
        control_msg.type = IPCMessageType::STREAM_CONTROL;
        control_msg.request_id = msg.request_id;
        
        IPCStreamControlPayload control_payload;
        control_payload.credits = 0;
        control_payload.buffer_avail = controller->getStats().buffer_available;
        
        control_msg.payload.resize(sizeof(control_payload));
        std::memcpy(control_msg.payload.data(), &control_payload, sizeof(control_payload));
        
        sendMessage(control_msg, ctx);
        
        // Wait for credits
        if (!controller->waitForCreditsWithTimeout(1, data_len, 30000)) {
            return core::Status::DEADLINE_EXCEEDED;
        }
    }
    
    // Record receipt
    controller->recordReceived(data_len);
    
    // Process the COPY data (forward to handler)
    if (handler_) {
        auto status = handler_->onCopyData(id_, 
                                          reinterpret_cast<const uint8_t*>(payload->data),
                                          data_len, ctx);
        
        if (!status.ok()) {
            return status;
        }
    }
    
    // Send updated flow control
    auto stats = controller->getStats();
    
    IPCMessage control_msg;
    control_msg.type = IPCMessageType::STREAM_CONTROL;
    control_msg.request_id = msg.request_id;
    
    IPCStreamControlPayload control_payload;
    control_payload.credits = stats.credits_available;
    control_payload.buffer_avail = stats.buffer_available;
    
    control_msg.payload.resize(sizeof(control_payload));
    std::memcpy(control_msg.payload.data(), &control_payload, sizeof(control_payload));
    
    return sendMessage(control_msg, ctx);
}

} // namespace ipc
} // namespace scratchbird
