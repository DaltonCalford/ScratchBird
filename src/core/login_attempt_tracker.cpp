/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/login_attempt_tracker.h"
#include <chrono>
#include <algorithm>

namespace scratchbird {
namespace core {

LoginAttemptTracker::LoginAttemptTracker(const LockoutPolicy& policy)
    : policy_(policy) {
}

uint64_t LoginAttemptTracker::getCurrentTimeMs() const {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

bool LoginAttemptTracker::shouldReset(const FailedAttempts& attempts) const {
    uint64_t now = getCurrentTimeMs();

    // If locked, don't reset until lockout expires
    if (attempts.lockout_until > now) {
        return false;
    }

    // Reset if enough time has passed since first attempt
    if (now - attempts.first_attempt_time > policy_.reset_window_ms) {
        return true;
    }

    return false;
}

uint64_t LoginAttemptTracker::calculateLockoutDuration(uint32_t lockout_count) const {
    if (!policy_.exponential_backoff) {
        return policy_.base_lockout_ms;
    }

    // Exponential backoff: base * 2^lockout_count
    // With max multiplier cap
    uint32_t multiplier = std::min(
        static_cast<uint32_t>(1 << lockout_count),  // 2^lockout_count
        policy_.max_lockout_multiplier
    );

    return policy_.base_lockout_ms * multiplier;
}

bool LoginAttemptTracker::isAccountLocked(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = attempts_.find(username);
    if (it == attempts_.end()) {
        return false;  // No tracking data = not locked
    }

    FailedAttempts& attempts = it->second;
    uint64_t now = getCurrentTimeMs();

    // Check if lockout has expired
    if (attempts.lockout_until > 0 && now >= attempts.lockout_until) {
        // Lockout expired - clear it but keep attempt count for exponential backoff
        attempts.lockout_until = 0;
        return false;
    }

    return attempts.lockout_until > 0;
}

void LoginAttemptTracker::recordFailedAttempt(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);

    uint64_t now = getCurrentTimeMs();

    // Get or create tracking entry
    FailedAttempts& attempts = attempts_[username];

    // Check if we should reset the counter
    if (shouldReset(attempts)) {
        // Reset to fresh state
        attempts.count = 0;
        attempts.first_attempt_time = now;
        attempts.last_attempt_time = now;
        attempts.lockout_until = 0;
        // Keep lockout_count for exponential backoff history
    }

    // If this is the first attempt in the window
    if (attempts.count == 0) {
        attempts.first_attempt_time = now;
    }

    // Record the failed attempt
    attempts.count++;
    attempts.last_attempt_time = now;

    // Check if we should trigger lockout
    if (attempts.count >= policy_.max_attempts) {
        // Calculate lockout duration with exponential backoff
        uint64_t lockout_duration = calculateLockoutDuration(attempts.lockout_count);
        attempts.lockout_until = now + lockout_duration;
        attempts.lockout_count++;

        // Reset attempt count (will start fresh after lockout)
        attempts.count = 0;
    }
}

void LoginAttemptTracker::recordSuccessfulLogin(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Clear all tracking data on successful login
    auto it = attempts_.find(username);
    if (it != attempts_.end()) {
        attempts_.erase(it);
    }
}

uint64_t LoginAttemptTracker::getLockoutTimeRemaining(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = attempts_.find(username);
    if (it == attempts_.end()) {
        return 0;
    }

    FailedAttempts& attempts = it->second;
    uint64_t now = getCurrentTimeMs();

    if (attempts.lockout_until == 0 || now >= attempts.lockout_until) {
        return 0;  // Not locked or lockout expired
    }

    return attempts.lockout_until - now;
}

uint32_t LoginAttemptTracker::getFailedAttemptCount(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = attempts_.find(username);
    if (it == attempts_.end()) {
        return 0;
    }

    const FailedAttempts& attempts = it->second;

    // Return 0 if should be reset
    if (shouldReset(attempts)) {
        return 0;
    }

    return attempts.count;
}

void LoginAttemptTracker::clearAttempts(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = attempts_.find(username);
    if (it != attempts_.end()) {
        attempts_.erase(it);
    }
}

void LoginAttemptTracker::cleanupExpiredEntries() {
    std::lock_guard<std::mutex> lock(mutex_);

    uint64_t now = getCurrentTimeMs();

    // Remove entries that are no longer relevant
    auto it = attempts_.begin();
    while (it != attempts_.end()) {
        const FailedAttempts& attempts = it->second;

        // Remove if:
        // 1. Not locked AND
        // 2. Reset window has elapsed since last attempt
        bool should_remove =
            (attempts.lockout_until == 0 || now >= attempts.lockout_until) &&
            (now - attempts.last_attempt_time > policy_.reset_window_ms);

        if (should_remove) {
            it = attempts_.erase(it);
        } else {
            ++it;
        }
    }
}

size_t LoginAttemptTracker::getTrackedAccountCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return attempts_.size();
}

}  // namespace core
}  // namespace scratchbird
