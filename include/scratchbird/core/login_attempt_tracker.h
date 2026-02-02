/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include <cstdint>

namespace scratchbird {
namespace core {

/**
 * Failed Login Attempts Tracking
 *
 * Tracks failed login attempts per user account for brute-force protection.
 *
 * P0-2: Failed Login Tracking & Account Lockout (Security Phase 3.5)
 * Addresses CWE-307: Improper Restriction of Excessive Authentication Attempts
 */
struct FailedAttempts {
    uint32_t count = 0;              // Number of failed attempts
    uint64_t first_attempt_time = 0; // Unix timestamp (ms) of first failure
    uint64_t last_attempt_time = 0;  // Unix timestamp (ms) of last failure
    uint64_t lockout_until = 0;      // Unix timestamp (ms), 0 = not locked
    uint32_t lockout_count = 0;      // Number of times account has been locked
};

/**
 * Account Lockout Policy Configuration
 *
 * Defines rules for account lockout after failed login attempts.
 */
struct LockoutPolicy {
    uint32_t max_attempts = 5;           // Max failed attempts before lockout
    uint64_t reset_window_ms = 3600000;  // 1 hour - reset counter after this period
    uint64_t base_lockout_ms = 900000;   // 15 minutes - first lockout duration
    bool exponential_backoff = true;     // Double lockout time on repeated failures
    uint32_t max_lockout_multiplier = 8; // Max multiplier (15min * 8 = 2 hours)
};

/**
 * Login Attempt Tracker
 *
 * Thread-safe tracker for failed login attempts and account lockouts.
 * Implements exponential backoff to discourage brute-force attacks.
 *
 * Features:
 * - Per-user failed attempt counting
 * - Automatic account lockout after threshold
 * - Exponential backoff for repeated lockouts
 * - Automatic reset after time window
 * - Thread-safe operations
 *
 * Example Usage:
 * ```cpp
 * LoginAttemptTracker tracker;
 *
 * // Check if account is locked
 * if (tracker.isAccountLocked("admin")) {
 *     return "Account locked, try again later";
 * }
 *
 * // Failed authentication
 * tracker.recordFailedAttempt("admin");
 *
 * // Successful authentication
 * tracker.recordSuccessfulLogin("admin");
 * ```
 */
class LoginAttemptTracker {
public:
    /**
     * Constructor
     *
     * @param policy Lockout policy configuration
     */
    explicit LoginAttemptTracker(const LockoutPolicy& policy = LockoutPolicy());

    /**
     * Check if account is currently locked
     *
     * @param username Username to check
     * @return true if account is locked, false otherwise
     */
    bool isAccountLocked(const std::string& username);

    /**
     * Record a failed login attempt
     *
     * Increments failure counter and potentially triggers lockout.
     *
     * @param username Username that failed authentication
     */
    void recordFailedAttempt(const std::string& username);

    /**
     * Record successful login
     *
     * Resets failure counter for the account.
     *
     * @param username Username that successfully authenticated
     */
    void recordSuccessfulLogin(const std::string& username);

    /**
     * Get lockout time remaining
     *
     * @param username Username to check
     * @return Milliseconds until unlock, 0 if not locked
     */
    uint64_t getLockoutTimeRemaining(const std::string& username);

    /**
     * Get failed attempt count
     *
     * @param username Username to check
     * @return Number of recent failed attempts
     */
    uint32_t getFailedAttemptCount(const std::string& username);

    /**
     * Clear all tracking for an account
     *
     * Used by administrators to manually unlock accounts.
     *
     * @param username Username to clear
     */
    void clearAttempts(const std::string& username);

    /**
     * Cleanup expired entries
     *
     * Removes tracking data for accounts that are no longer
     * locked and have exceeded the reset window.
     *
     * Should be called periodically for maintenance.
     */
    void cleanupExpiredEntries();

    /**
     * Get total number of tracked accounts
     *
     * @return Number of accounts with tracking data
     */
    size_t getTrackedAccountCount() const;

private:
    LockoutPolicy policy_;
    std::unordered_map<std::string, FailedAttempts> attempts_;
    mutable std::mutex mutex_;

    /**
     * Get current time in milliseconds since epoch
     */
    uint64_t getCurrentTimeMs() const;

    /**
     * Check if tracking entry should be reset
     *
     * @param attempts Failed attempts record
     * @return true if reset window has elapsed
     */
    bool shouldReset(const FailedAttempts& attempts) const;

    /**
     * Calculate lockout duration based on attempt history
     *
     * @param lockout_count Number of previous lockouts
     * @return Lockout duration in milliseconds
     */
    uint64_t calculateLockoutDuration(uint32_t lockout_count) const;
};

}  // namespace core
}  // namespace scratchbird
