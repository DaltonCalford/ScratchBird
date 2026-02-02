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
 * Event Loop
 *
 * ScratchBird Network Layer - Phase 3.1
 *
 * High-performance event loop using epoll (Linux) or kqueue (macOS/BSD).
 * Handles socket I/O events, timers, and signals.
 */

#pragma once

#include "scratchbird/network/socket_types.h"
#include "scratchbird/network/socket.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"

#include <memory>
#include <functional>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <chrono>
#include <queue>

namespace scratchbird {
namespace network {

// Forward declarations
class EventLoop;

// ============================================================================
// Timer
// ============================================================================

/**
 * Timer ID type
 */
using TimerId = uint64_t;
constexpr TimerId INVALID_TIMER_ID = 0;

/**
 * Timer callback
 */
using TimerCallback = std::function<void(TimerId id)>;

/**
 * Timer entry
 */
struct TimerEntry {
    TimerId id;
    std::chrono::steady_clock::time_point expires_at;
    std::chrono::milliseconds interval;  // 0 = one-shot
    TimerCallback callback;
    bool cancelled = false;

    bool operator>(const TimerEntry& other) const {
        return expires_at > other.expires_at;
    }
};

// ============================================================================
// Event Source
// ============================================================================

/**
 * Event source - represents a registered file descriptor
 */
class EventSource {
public:
    EventSource(socket_t fd, EventType events, EventCallback callback, void* user_data = nullptr);
    ~EventSource();

    // Non-copyable, movable
    EventSource(const EventSource&) = delete;
    EventSource& operator=(const EventSource&) = delete;
    EventSource(EventSource&&) noexcept;
    EventSource& operator=(EventSource&&) noexcept;

    socket_t getFd() const { return fd_; }
    EventType getEvents() const { return events_; }
    void* getUserData() const { return user_data_; }

    void setEvents(EventType events) { events_ = events; }
    void setCallback(EventCallback callback) { callback_ = std::move(callback); }
    void setUserData(void* data) { user_data_ = data; }

    void invoke(EventType triggered_events);

private:
    socket_t fd_;
    EventType events_;
    EventCallback callback_;
    void* user_data_;
};

// ============================================================================
// Event Loop
// ============================================================================

/**
 * Event Loop Configuration
 */
struct EventLoopConfig {
    uint32_t max_events_per_poll = 256;     // Maximum events per poll iteration
    int poll_timeout_ms = 100;              // Default poll timeout
    bool enable_timers = true;              // Enable timer support
    bool enable_signals = false;            // Enable signal handling
    std::vector<int> handled_signals;       // Signals to handle (if enabled)
};

/**
 * Event Loop - main I/O multiplexing abstraction
 *
 * Uses epoll on Linux, kqueue on macOS/BSD, and select/poll as fallback.
 *
 * Thread safety: The EventLoop itself is thread-safe for add/remove operations.
 * The poll() method should only be called from a single thread.
 */
class EventLoop {
public:
    /**
     * Create event loop with default configuration
     */
    static std::unique_ptr<EventLoop> create(core::ErrorContext* ctx = nullptr);

    /**
     * Create event loop with custom configuration
     */
    static std::unique_ptr<EventLoop> create(const EventLoopConfig& config,
                                             core::ErrorContext* ctx = nullptr);

    virtual ~EventLoop();

    // Non-copyable
    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    // ========================================================================
    // File Descriptor Registration
    // ========================================================================

    /**
     * Add a file descriptor to the event loop
     *
     * @param fd File descriptor to monitor
     * @param events Events to monitor (READ, WRITE, etc.)
     * @param callback Callback when events occur
     * @param user_data User-provided data passed to callback
     * @param ctx Error context
     * @return Status::OK on success
     */
    core::Status add(socket_t fd, EventType events, EventCallback callback,
                     void* user_data = nullptr, core::ErrorContext* ctx = nullptr);

    /**
     * Modify events for a registered file descriptor
     *
     * @param fd File descriptor
     * @param events New events to monitor
     * @param ctx Error context
     * @return Status::OK on success
     */
    core::Status modify(socket_t fd, EventType events, core::ErrorContext* ctx = nullptr);

    /**
     * Remove a file descriptor from the event loop
     *
     * @param fd File descriptor to remove
     * @param ctx Error context
     * @return Status::OK on success
     */
    core::Status remove(socket_t fd, core::ErrorContext* ctx = nullptr);

    /**
     * Check if a file descriptor is registered
     */
    bool contains(socket_t fd) const;

    // ========================================================================
    // Timers
    // ========================================================================

    /**
     * Add a one-shot timer
     *
     * @param delay Delay before timer fires
     * @param callback Callback when timer fires
     * @return Timer ID (use to cancel)
     */
    TimerId addTimer(std::chrono::milliseconds delay, TimerCallback callback);

    /**
     * Add a repeating timer
     *
     * @param interval Interval between timer fires
     * @param callback Callback when timer fires
     * @return Timer ID (use to cancel)
     */
    TimerId addRepeatingTimer(std::chrono::milliseconds interval, TimerCallback callback);

    /**
     * Cancel a timer
     *
     * @param id Timer ID to cancel
     * @return true if timer was cancelled, false if not found
     */
    bool cancelTimer(TimerId id);

    // ========================================================================
    // Event Processing
    // ========================================================================

    /**
     * Poll for events and dispatch callbacks
     *
     * @param timeout_ms Timeout in milliseconds (-1 = block, 0 = poll)
     * @return Number of events processed, or -1 on error
     */
    int poll(int timeout_ms = -1);

    /**
     * Run the event loop
     *
     * Calls poll() repeatedly until stop() is called.
     *
     * @param ctx Error context
     * @return Status::OK on normal termination
     */
    core::Status run(core::ErrorContext* ctx = nullptr);

    /**
     * Run the event loop for a single iteration
     *
     * @param timeout_ms Timeout in milliseconds
     * @return Number of events processed
     */
    int runOnce(int timeout_ms = 100);

    /**
     * Signal the event loop to stop
     *
     * Thread-safe. Can be called from any thread.
     */
    void stop();

    /**
     * Check if the event loop is running
     */
    bool isRunning() const { return running_.load(std::memory_order_acquire); }

    /**
     * Wake up the event loop from another thread
     *
     * Useful for adding new events or stopping the loop.
     */
    void wakeup();

    // ========================================================================
    // Statistics
    // ========================================================================

    /**
     * Get number of registered file descriptors
     */
    size_t size() const;

    /**
     * Get number of pending timers
     */
    size_t timerCount() const;

    /**
     * Get total events processed since creation
     */
    uint64_t totalEventsProcessed() const { return events_processed_.load(); }

    /**
     * Get configuration
     */
    const EventLoopConfig& getConfig() const { return config_; }

protected:
    EventLoop(const EventLoopConfig& config);

    // Platform-specific methods (implemented in event_loop_*.cpp)
    virtual core::Status initPlatform(core::ErrorContext* ctx) = 0;
    virtual void cleanupPlatform() = 0;
    virtual core::Status addPlatform(socket_t fd, EventType events, core::ErrorContext* ctx) = 0;
    virtual core::Status modifyPlatform(socket_t fd, EventType events, core::ErrorContext* ctx) = 0;
    virtual core::Status removePlatform(socket_t fd, core::ErrorContext* ctx) = 0;
    virtual int pollPlatform(int timeout_ms, std::vector<EventData>& events) = 0;
    virtual void wakeupPlatform() = 0;

    // Process expired timers
    void processTimers();

    // Calculate timeout until next timer
    int calculateTimeout(int requested_timeout_ms);

    EventLoopConfig config_;
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> events_processed_{0};

    // Event sources (protected by mutex)
    mutable std::mutex sources_mutex_;
    std::unordered_map<socket_t, std::unique_ptr<EventSource>> sources_;

    // Timers (protected by timer_mutex)
    mutable std::mutex timer_mutex_;
    std::priority_queue<TimerEntry, std::vector<TimerEntry>, std::greater<TimerEntry>> timers_;
    std::atomic<TimerId> next_timer_id_{1};

    // Wakeup pipe
    socket_t wakeup_read_fd_ = INVALID_SOCKET_VALUE;
    socket_t wakeup_write_fd_ = INVALID_SOCKET_VALUE;
};

// ============================================================================
// Platform-Specific Event Loop Implementations
// ============================================================================

#ifdef __linux__
/**
 * Linux epoll-based event loop
 */
class EpollEventLoop : public EventLoop {
public:
    explicit EpollEventLoop(const EventLoopConfig& config);
    ~EpollEventLoop() override;

protected:
    core::Status initPlatform(core::ErrorContext* ctx) override;
    void cleanupPlatform() override;
    core::Status addPlatform(socket_t fd, EventType events, core::ErrorContext* ctx) override;
    core::Status modifyPlatform(socket_t fd, EventType events, core::ErrorContext* ctx) override;
    core::Status removePlatform(socket_t fd, core::ErrorContext* ctx) override;
    int pollPlatform(int timeout_ms, std::vector<EventData>& events) override;
    void wakeupPlatform() override;

private:
    int epoll_fd_ = -1;
};
#endif

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__)
/**
 * BSD kqueue-based event loop
 */
class KqueueEventLoop : public EventLoop {
public:
    explicit KqueueEventLoop(const EventLoopConfig& config);
    ~KqueueEventLoop() override;

protected:
    core::Status initPlatform(core::ErrorContext* ctx) override;
    void cleanupPlatform() override;
    core::Status addPlatform(socket_t fd, EventType events, core::ErrorContext* ctx) override;
    core::Status modifyPlatform(socket_t fd, EventType events, core::ErrorContext* ctx) override;
    core::Status removePlatform(socket_t fd, core::ErrorContext* ctx) override;
    int pollPlatform(int timeout_ms, std::vector<EventData>& events) override;
    void wakeupPlatform() override;

private:
    int kqueue_fd_ = -1;
};
#endif

/**
 * Portable poll-based event loop (fallback)
 */
class PollEventLoop : public EventLoop {
public:
    explicit PollEventLoop(const EventLoopConfig& config);
    ~PollEventLoop() override;

protected:
    core::Status initPlatform(core::ErrorContext* ctx) override;
    void cleanupPlatform() override;
    core::Status addPlatform(socket_t fd, EventType events, core::ErrorContext* ctx) override;
    core::Status modifyPlatform(socket_t fd, EventType events, core::ErrorContext* ctx) override;
    core::Status removePlatform(socket_t fd, core::ErrorContext* ctx) override;
    int pollPlatform(int timeout_ms, std::vector<EventData>& events) override;
    void wakeupPlatform() override;
};

} // namespace network
} // namespace scratchbird
