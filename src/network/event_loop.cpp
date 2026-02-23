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
 * Event Loop Implementation
 *
 * ScratchBird Network Layer - Phase 3.1
 *
 * High-performance event loop using epoll (Linux) or kqueue (macOS/BSD).
 */

#include "scratchbird/network/event_loop.h"
#include "scratchbird/core/error_context.h"

#include <algorithm>
#include <cstring>

#ifdef __linux__
    #include <sys/epoll.h>
#endif

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__)
    #include <sys/types.h>
    #include <sys/event.h>
    #include <sys/time.h>
#endif

#ifndef _WIN32
    #include "scratchbird/core/posix_compat.h"
    #include <poll.h>
    #include <fcntl.h>
#endif

namespace scratchbird {
namespace network {

// ============================================================================
// EventSource Implementation
// ============================================================================

EventSource::EventSource(socket_t fd, EventType events, EventCallback callback, void* user_data)
    : fd_(fd), events_(events), callback_(std::move(callback)), user_data_(user_data) {}

EventSource::~EventSource() = default;

EventSource::EventSource(EventSource&& other) noexcept
    : fd_(other.fd_), events_(other.events_),
      callback_(std::move(other.callback_)), user_data_(other.user_data_) {
    other.fd_ = INVALID_SOCKET_VALUE;
}

EventSource& EventSource::operator=(EventSource&& other) noexcept {
    if (this != &other) {
        fd_ = other.fd_;
        events_ = other.events_;
        callback_ = std::move(other.callback_);
        user_data_ = other.user_data_;
        other.fd_ = INVALID_SOCKET_VALUE;
    }
    return *this;
}

void EventSource::invoke(EventType triggered_events) {
    if (callback_) {
        EventData data(fd_, triggered_events, user_data_);
        callback_(data);
    }
}

// ============================================================================
// EventLoop Base Implementation
// ============================================================================

EventLoop::EventLoop(const EventLoopConfig& config) : config_(config) {}

EventLoop::~EventLoop() {
    // Note: Don't call stop() here as it calls wakeup() which is virtual
    // The derived class destructor should call cleanupPlatform() instead
    running_.store(false, std::memory_order_release);

    // Close wakeup pipe
    if (wakeup_read_fd_ != INVALID_SOCKET_VALUE) {
#ifdef _WIN32
        closesocket(wakeup_read_fd_);
        closesocket(wakeup_write_fd_);
#else
        ::close(wakeup_read_fd_);
        ::close(wakeup_write_fd_);
#endif
        wakeup_read_fd_ = INVALID_SOCKET_VALUE;
        wakeup_write_fd_ = INVALID_SOCKET_VALUE;
    }
}

std::unique_ptr<EventLoop> EventLoop::create(core::ErrorContext* ctx) {
    return create(EventLoopConfig(), ctx);
}

std::unique_ptr<EventLoop> EventLoop::create(const EventLoopConfig& config,
                                              core::ErrorContext* ctx) {
#ifdef __linux__
    auto loop = std::unique_ptr<EventLoop>(new EpollEventLoop(config));
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__)
    auto loop = std::unique_ptr<EventLoop>(new KqueueEventLoop(config));
#else
    auto loop = std::unique_ptr<EventLoop>(new PollEventLoop(config));
#endif

    if (auto status = loop->initPlatform(ctx); status != core::Status::OK) {
        return nullptr;
    }

    // Create wakeup pipe
#ifndef _WIN32
    int pipefd[2];
    if (pipe(pipefd) == 0) {
        loop->wakeup_read_fd_ = pipefd[0];
        loop->wakeup_write_fd_ = pipefd[1];

        // Make non-blocking
        fcntl(pipefd[0], F_SETFL, fcntl(pipefd[0], F_GETFL) | O_NONBLOCK);
        fcntl(pipefd[1], F_SETFL, fcntl(pipefd[1], F_GETFL) | O_NONBLOCK);

        // Register wakeup pipe for reading
        loop->addPlatform(pipefd[0], EventType::READ, nullptr);
    }
#endif

    return loop;
}

core::Status EventLoop::add(socket_t fd, EventType events, EventCallback callback,
                            void* user_data, core::ErrorContext* ctx) {
    {
        std::lock_guard<std::mutex> lock(sources_mutex_);
        if (sources_.count(fd)) {
            SET_ERROR_CONTEXT(ctx, core::Status::DUPLICATE_OBJECT, "File descriptor already registered");
            return core::Status::DUPLICATE_OBJECT;
        }
    }

    auto status = addPlatform(fd, events, ctx);
    if (status != core::Status::OK) {
        return status;
    }

    {
        std::lock_guard<std::mutex> lock(sources_mutex_);
        sources_[fd] = std::make_unique<EventSource>(fd, events, std::move(callback), user_data);
    }

    return core::Status::OK;
}

core::Status EventLoop::modify(socket_t fd, EventType events, core::ErrorContext* ctx) {
    {
        std::lock_guard<std::mutex> lock(sources_mutex_);
        auto it = sources_.find(fd);
        if (it == sources_.end()) {
            SET_ERROR_CONTEXT(ctx, core::Status::NOT_FOUND, "File descriptor not registered");
            return core::Status::NOT_FOUND;
        }
        it->second->setEvents(events);
    }

    return modifyPlatform(fd, events, ctx);
}

core::Status EventLoop::remove(socket_t fd, core::ErrorContext* ctx) {
    {
        std::lock_guard<std::mutex> lock(sources_mutex_);
        auto it = sources_.find(fd);
        if (it == sources_.end()) {
            return core::Status::OK;  // Not registered, no-op
        }
        sources_.erase(it);
    }

    return removePlatform(fd, ctx);
}

bool EventLoop::contains(socket_t fd) const {
    std::lock_guard<std::mutex> lock(sources_mutex_);
    return sources_.count(fd) > 0;
}

TimerId EventLoop::addTimer(std::chrono::milliseconds delay, TimerCallback callback) {
    TimerEntry entry;
    entry.id = next_timer_id_.fetch_add(1);
    entry.expires_at = std::chrono::steady_clock::now() + delay;
    entry.interval = std::chrono::milliseconds::zero();
    entry.callback = std::move(callback);

    {
        std::lock_guard<std::mutex> lock(timer_mutex_);
        timers_.push(std::move(entry));
    }

    wakeup();
    return entry.id;
}

TimerId EventLoop::addRepeatingTimer(std::chrono::milliseconds interval, TimerCallback callback) {
    TimerEntry entry;
    entry.id = next_timer_id_.fetch_add(1);
    entry.expires_at = std::chrono::steady_clock::now() + interval;
    entry.interval = interval;
    entry.callback = std::move(callback);

    {
        std::lock_guard<std::mutex> lock(timer_mutex_);
        timers_.push(std::move(entry));
    }

    wakeup();
    return entry.id;
}

bool EventLoop::cancelTimer(TimerId id) {
    std::lock_guard<std::mutex> lock(timer_mutex_);

    // Mark timer as cancelled (we can't actually remove from priority_queue)
    // We'll skip it when it fires
    // Note: This is O(n) but timers are typically few
    std::vector<TimerEntry> temp;
    bool found = false;
    while (!timers_.empty()) {
        auto entry = std::move(const_cast<TimerEntry&>(timers_.top()));
        timers_.pop();
        if (entry.id == id) {
            entry.cancelled = true;
            found = true;
        }
        temp.push_back(std::move(entry));
    }
    for (auto& e : temp) {
        timers_.push(std::move(e));
    }

    return found;
}

int EventLoop::poll(int timeout_ms) {
    if (!running_.load(std::memory_order_acquire)) {
        running_.store(true, std::memory_order_release);
    }

    // Adjust timeout for timers
    int actual_timeout = calculateTimeout(timeout_ms);

    // Poll for events
    std::vector<EventData> events;
    events.reserve(config_.max_events_per_poll);

    int count = pollPlatform(actual_timeout, events);
    if (count < 0) {
        return -1;
    }

    // Process events
    for (const auto& event : events) {
        // Check if this is the wakeup pipe
        if (event.fd == wakeup_read_fd_) {
            // Drain wakeup pipe
            char buf[64];
#ifdef _WIN32
            while (recv(wakeup_read_fd_, buf, static_cast<int>(sizeof(buf)), 0) > 0) {}
#else
            while (::read(wakeup_read_fd_, buf, sizeof(buf)) > 0) {}
#endif
            continue;
        }

        EventSource* source = nullptr;
        {
            std::lock_guard<std::mutex> lock(sources_mutex_);
            auto it = sources_.find(event.fd);
            if (it != sources_.end()) {
                source = it->second.get();
            }
        }

        if (source) {
            source->invoke(event.events);
            events_processed_.fetch_add(1);
        }
    }

    // Process expired timers
    processTimers();

    return static_cast<int>(events.size());
}

core::Status EventLoop::run(core::ErrorContext* ctx) {
    running_.store(true, std::memory_order_release);

    while (running_.load(std::memory_order_acquire)) {
        int result = poll(config_.poll_timeout_ms);
        if (result < 0) {
            SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR, "poll() failed");
            return core::Status::IO_ERROR;
        }
    }

    return core::Status::OK;
}

int EventLoop::runOnce(int timeout_ms) {
    return poll(timeout_ms);
}

void EventLoop::stop() {
    running_.store(false, std::memory_order_release);
    wakeup();
}

void EventLoop::wakeup() {
    wakeupPlatform();
}

size_t EventLoop::size() const {
    std::lock_guard<std::mutex> lock(sources_mutex_);
    return sources_.size();
}

size_t EventLoop::timerCount() const {
    std::lock_guard<std::mutex> lock(timer_mutex_);
    return timers_.size();
}

void EventLoop::processTimers() {
    auto now = std::chrono::steady_clock::now();

    std::vector<TimerEntry> to_fire;
    std::vector<TimerEntry> to_reschedule;

    {
        std::lock_guard<std::mutex> lock(timer_mutex_);
        while (!timers_.empty() && timers_.top().expires_at <= now) {
            auto entry = std::move(const_cast<TimerEntry&>(timers_.top()));
            timers_.pop();

            if (entry.cancelled) {
                continue;
            }

            to_fire.push_back(entry);

            // Reschedule repeating timers
            if (entry.interval.count() > 0) {
                entry.expires_at = now + entry.interval;
                to_reschedule.push_back(std::move(entry));
            }
        }

        for (auto& e : to_reschedule) {
            timers_.push(std::move(e));
        }
    }

    // Fire callbacks outside of lock
    for (auto& entry : to_fire) {
        if (entry.callback) {
            entry.callback(entry.id);
        }
    }
}

int EventLoop::calculateTimeout(int requested_timeout_ms) {
    if (requested_timeout_ms == 0) {
        return 0;  // Polling, no wait
    }

    std::lock_guard<std::mutex> lock(timer_mutex_);
    if (timers_.empty()) {
        return requested_timeout_ms;
    }

    auto now = std::chrono::steady_clock::now();
    auto next_timer = timers_.top().expires_at;
    auto timer_timeout = std::chrono::duration_cast<std::chrono::milliseconds>(next_timer - now);

    if (timer_timeout.count() <= 0) {
        return 0;  // Timer already expired
    }

    if (requested_timeout_ms < 0) {
        return static_cast<int>(timer_timeout.count());
    }

    return std::min(requested_timeout_ms, static_cast<int>(timer_timeout.count()));
}

// ============================================================================
// Linux epoll Implementation
// ============================================================================

#ifdef __linux__

namespace {
uint32_t toEpollEvents(EventType events) {
    uint32_t ep_events = 0;
    if (hasEvent(events, EventType::READ)) ep_events |= EPOLLIN;
    if (hasEvent(events, EventType::WRITE)) ep_events |= EPOLLOUT;
    if (hasEvent(events, EventType::ERROR_EVENT)) ep_events |= EPOLLERR;
    return ep_events;
}

EventType fromEpollEvents(uint32_t ep_events) {
    EventType events = EventType::NONE;
    if (ep_events & EPOLLIN) events |= EventType::READ;
    if (ep_events & EPOLLOUT) events |= EventType::WRITE;
    if (ep_events & EPOLLERR) events |= EventType::ERROR_EVENT;
    if (ep_events & EPOLLHUP) events |= EventType::HANGUP;
    return events;
}
}

EpollEventLoop::EpollEventLoop(const EventLoopConfig& config)
    : EventLoop(config) {}

EpollEventLoop::~EpollEventLoop() {
    cleanupPlatform();
}

core::Status EpollEventLoop::initPlatform(core::ErrorContext* ctx) {
    epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd_ < 0) {
        SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR,
                          ("epoll_create1() failed: " + getSocketErrorString(errno)).c_str());
        return core::Status::IO_ERROR;
    }
    return core::Status::OK;
}

void EpollEventLoop::cleanupPlatform() {
    if (epoll_fd_ >= 0) {
        ::close(epoll_fd_);
        epoll_fd_ = -1;
    }
}

core::Status EpollEventLoop::addPlatform(socket_t fd, EventType events, core::ErrorContext* ctx) {
    struct epoll_event ev;
    ev.events = toEpollEvents(events);
    ev.data.fd = fd;

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) < 0) {
        SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR,
                          ("epoll_ctl(ADD) failed: " + getSocketErrorString(errno)).c_str());
        return core::Status::IO_ERROR;
    }
    return core::Status::OK;
}

core::Status EpollEventLoop::modifyPlatform(socket_t fd, EventType events, core::ErrorContext* ctx) {
    struct epoll_event ev;
    ev.events = toEpollEvents(events);
    ev.data.fd = fd;

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) < 0) {
        SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR,
                          ("epoll_ctl(MOD) failed: " + getSocketErrorString(errno)).c_str());
        return core::Status::IO_ERROR;
    }
    return core::Status::OK;
}

core::Status EpollEventLoop::removePlatform(socket_t fd, core::ErrorContext* ctx) {
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) < 0) {
        // ENOENT is okay - fd may have been closed
        if (errno != ENOENT) {
            SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR,
                              ("epoll_ctl(DEL) failed: " + getSocketErrorString(errno)).c_str());
            return core::Status::IO_ERROR;
        }
    }
    return core::Status::OK;
}

int EpollEventLoop::pollPlatform(int timeout_ms, std::vector<EventData>& events) {
    std::vector<struct epoll_event> ep_events(config_.max_events_per_poll);

    int n = epoll_wait(epoll_fd_, ep_events.data(),
                       static_cast<int>(ep_events.size()), timeout_ms);

    if (n < 0) {
        if (errno == EINTR) return 0;
        return -1;
    }

    for (int i = 0; i < n; ++i) {
        // Use temporary variable to avoid binding to packed field
        socket_t fd = ep_events[i].data.fd;
        events.emplace_back(fd, fromEpollEvents(ep_events[i].events));
    }

    return n;
}

void EpollEventLoop::wakeupPlatform() {
    if (wakeup_write_fd_ != INVALID_SOCKET_VALUE) {
        char c = 'W';
        ssize_t ret;
        do {
            ret = ::write(wakeup_write_fd_, &c, 1);
        } while (ret < 0 && errno == EINTR);
    }
}

#endif // __linux__

// ============================================================================
// BSD kqueue Implementation
// ============================================================================

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__)

KqueueEventLoop::KqueueEventLoop(const EventLoopConfig& config)
    : EventLoop(config) {}

KqueueEventLoop::~KqueueEventLoop() {
    cleanupPlatform();
}

core::Status KqueueEventLoop::initPlatform(core::ErrorContext* ctx) {
    kqueue_fd_ = kqueue();
    if (kqueue_fd_ < 0) {
        SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR,
                          ("kqueue() failed: " + getSocketErrorString(errno)).c_str());
        return core::Status::IO_ERROR;
    }
    return core::Status::OK;
}

void KqueueEventLoop::cleanupPlatform() {
    if (kqueue_fd_ >= 0) {
        ::close(kqueue_fd_);
        kqueue_fd_ = -1;
    }
}

core::Status KqueueEventLoop::addPlatform(socket_t fd, EventType events, core::ErrorContext* ctx) {
    std::vector<struct kevent> changes;

    if (hasEvent(events, EventType::READ)) {
        struct kevent ev;
        EV_SET(&ev, fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, nullptr);
        changes.push_back(ev);
    }

    if (hasEvent(events, EventType::WRITE)) {
        struct kevent ev;
        EV_SET(&ev, fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, nullptr);
        changes.push_back(ev);
    }

    if (!changes.empty()) {
        if (kevent(kqueue_fd_, changes.data(), static_cast<int>(changes.size()),
                   nullptr, 0, nullptr) < 0) {
            SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR,
                              ("kevent(ADD) failed: " + getSocketErrorString(errno)).c_str());
            return core::Status::IO_ERROR;
        }
    }

    return core::Status::OK;
}

core::Status KqueueEventLoop::modifyPlatform(socket_t fd, EventType events, core::ErrorContext* ctx) {
    // kqueue doesn't have modify - delete and re-add
    removePlatform(fd, ctx);
    return addPlatform(fd, events, ctx);
}

core::Status KqueueEventLoop::removePlatform(socket_t fd, core::ErrorContext* ctx) {
    struct kevent changes[2];
    EV_SET(&changes[0], fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
    EV_SET(&changes[1], fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);

    // Ignore errors - fd may already be closed
    kevent(kqueue_fd_, changes, 2, nullptr, 0, nullptr);
    return core::Status::OK;
}

int KqueueEventLoop::pollPlatform(int timeout_ms, std::vector<EventData>& events) {
    std::vector<struct kevent> kq_events(config_.max_events_per_poll);

    struct timespec ts;
    struct timespec* ts_ptr = nullptr;
    if (timeout_ms >= 0) {
        ts.tv_sec = timeout_ms / 1000;
        ts.tv_nsec = (timeout_ms % 1000) * 1000000;
        ts_ptr = &ts;
    }

    int n = kevent(kqueue_fd_, nullptr, 0, kq_events.data(),
                   static_cast<int>(kq_events.size()), ts_ptr);

    if (n < 0) {
        if (errno == EINTR) return 0;
        return -1;
    }

    for (int i = 0; i < n; ++i) {
        EventType ev_type = EventType::NONE;

        if (kq_events[i].filter == EVFILT_READ) {
            ev_type |= EventType::READ;
        }
        if (kq_events[i].filter == EVFILT_WRITE) {
            ev_type |= EventType::WRITE;
        }
        if (kq_events[i].flags & EV_EOF) {
            ev_type |= EventType::HANGUP;
        }
        if (kq_events[i].flags & EV_ERROR) {
            ev_type |= EventType::ERROR_EVENT;
        }

        events.emplace_back(static_cast<socket_t>(kq_events[i].ident), ev_type);
    }

    return n;
}

void KqueueEventLoop::wakeupPlatform() {
    if (wakeup_write_fd_ != INVALID_SOCKET_VALUE) {
        char c = 'W';
        ssize_t ret;
        do {
            ret = ::write(wakeup_write_fd_, &c, 1);
        } while (ret < 0 && errno == EINTR);
    }
}

#endif // __APPLE__ || __FreeBSD__ || __OpenBSD__

// ============================================================================
// Portable poll Implementation (Fallback)
// ============================================================================

PollEventLoop::PollEventLoop(const EventLoopConfig& config)
    : EventLoop(config) {}

PollEventLoop::~PollEventLoop() {
    cleanupPlatform();
}

core::Status PollEventLoop::initPlatform(core::ErrorContext* /*ctx*/) {
    return core::Status::OK;
}

void PollEventLoop::cleanupPlatform() {
    // Nothing to cleanup
}

core::Status PollEventLoop::addPlatform(socket_t /*fd*/, EventType /*events*/,
                                        core::ErrorContext* /*ctx*/) {
    // Poll-based implementation uses sources_ directly
    return core::Status::OK;
}

core::Status PollEventLoop::modifyPlatform(socket_t /*fd*/, EventType /*events*/,
                                           core::ErrorContext* /*ctx*/) {
    return core::Status::OK;
}

core::Status PollEventLoop::removePlatform(socket_t /*fd*/, core::ErrorContext* /*ctx*/) {
    return core::Status::OK;
}

int PollEventLoop::pollPlatform(int timeout_ms, std::vector<EventData>& events) {
#ifdef _WIN32
    // Windows select-based implementation
    fd_set read_fds, write_fds, except_fds;
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_ZERO(&except_fds);

    socket_t max_fd = 0;
    {
        std::lock_guard<std::mutex> lock(sources_mutex_);
        for (const auto& [fd, source] : sources_) {
            if (hasEvent(source->getEvents(), EventType::READ)) FD_SET(fd, &read_fds);
            if (hasEvent(source->getEvents(), EventType::WRITE)) FD_SET(fd, &write_fds);
            FD_SET(fd, &except_fds);
            if (fd > max_fd) max_fd = fd;
        }
    }

    struct timeval tv;
    struct timeval* tv_ptr = nullptr;
    if (timeout_ms >= 0) {
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        tv_ptr = &tv;
    }

    int n = select(static_cast<int>(max_fd) + 1, &read_fds, &write_fds, &except_fds, tv_ptr);
    if (n < 0) return -1;

    {
        std::lock_guard<std::mutex> lock(sources_mutex_);
        for (const auto& [fd, source] : sources_) {
            EventType ev = EventType::NONE;
            if (FD_ISSET(fd, &read_fds)) ev |= EventType::READ;
            if (FD_ISSET(fd, &write_fds)) ev |= EventType::WRITE;
            if (FD_ISSET(fd, &except_fds)) ev |= EventType::ERROR_EVENT;

            if (ev != EventType::NONE) {
                events.emplace_back(fd, ev);
            }
        }
    }

    return n;
#else
    // Unix poll-based implementation
    std::vector<struct pollfd> fds;

    {
        std::lock_guard<std::mutex> lock(sources_mutex_);
        fds.reserve(sources_.size());
        for (const auto& [fd, source] : sources_) {
            struct pollfd pfd;
            pfd.fd = fd;
            pfd.events = 0;
            pfd.revents = 0;

            if (hasEvent(source->getEvents(), EventType::READ)) pfd.events |= POLLIN;
            if (hasEvent(source->getEvents(), EventType::WRITE)) pfd.events |= POLLOUT;

            fds.push_back(pfd);
        }
    }

    int n = ::poll(fds.data(), fds.size(), timeout_ms);
    if (n < 0) {
        if (errno == EINTR) return 0;
        return -1;
    }

    for (const auto& pfd : fds) {
        if (pfd.revents == 0) continue;

        EventType ev = EventType::NONE;
        if (pfd.revents & POLLIN) ev |= EventType::READ;
        if (pfd.revents & POLLOUT) ev |= EventType::WRITE;
        if (pfd.revents & POLLERR) ev |= EventType::ERROR_EVENT;
        if (pfd.revents & POLLHUP) ev |= EventType::HANGUP;

        events.emplace_back(pfd.fd, ev);
    }

    return n;
#endif
}

void PollEventLoop::wakeupPlatform() {
#ifndef _WIN32
    if (wakeup_write_fd_ != INVALID_SOCKET_VALUE) {
        char c = 'W';
        ssize_t ret;
        do {
            ret = ::write(wakeup_write_fd_, &c, 1);
        } while (ret < 0 && errno == EINTR);
    }
#endif
}

} // namespace network
} // namespace scratchbird
