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
 * Unit Tests for Login Attempt Tracking & Account Lockout
 *
 * P0-2: Failed Login Tracking & Account Lockout (Security Phase 3.5)
 * Tests brute-force protection and account lockout logic (CWE-307)
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include "scratchbird/core/login_attempt_tracker.h"

using namespace scratchbird::core;

/**
 * Test Fixture for Login Attempt Tracker
 */
class LoginAttemptTrackerTest : public ::testing::Test {
protected:
    LockoutPolicy fast_policy_;

    void SetUp() override {
        // Use fast timeouts for testing
        fast_policy_.max_attempts = 3;           // Lock after 3 attempts
        fast_policy_.reset_window_ms = 1000;     // 1 second reset window
        fast_policy_.base_lockout_ms = 500;      // 500ms base lockout
        fast_policy_.exponential_backoff = true;
        fast_policy_.max_lockout_multiplier = 8;
    }

    void TearDown() override {
    }

    // Helper: Sleep for milliseconds
    void sleepMs(uint64_t ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
};

// ===== Basic Functionality Tests =====

TEST_F(LoginAttemptTrackerTest, InitialStateNoAttempts) {
    LoginAttemptTracker tracker(fast_policy_);

    EXPECT_FALSE(tracker.isAccountLocked("user1"));
    EXPECT_EQ(0, tracker.getFailedAttemptCount("user1"));
    EXPECT_EQ(0, tracker.getLockoutTimeRemaining("user1"));
}

TEST_F(LoginAttemptTrackerTest, RecordSingleFailedAttempt) {
    LoginAttemptTracker tracker(fast_policy_);

    tracker.recordFailedAttempt("user1");

    EXPECT_FALSE(tracker.isAccountLocked("user1"));
    EXPECT_EQ(1, tracker.getFailedAttemptCount("user1"));
}

TEST_F(LoginAttemptTrackerTest, RecordMultipleFailedAttempts) {
    LoginAttemptTracker tracker(fast_policy_);

    tracker.recordFailedAttempt("user1");
    tracker.recordFailedAttempt("user1");

    EXPECT_FALSE(tracker.isAccountLocked("user1"));
    EXPECT_EQ(2, tracker.getFailedAttemptCount("user1"));
}

TEST_F(LoginAttemptTrackerTest, AccountLockoutAfterMaxAttempts) {
    LoginAttemptTracker tracker(fast_policy_);

    // Record max_attempts (3) failed attempts
    tracker.recordFailedAttempt("user1");
    tracker.recordFailedAttempt("user1");
    tracker.recordFailedAttempt("user1");

    // Should now be locked
    EXPECT_TRUE(tracker.isAccountLocked("user1"));
    EXPECT_GT(tracker.getLockoutTimeRemaining("user1"), 0);

    // Attempt count should reset after lockout
    EXPECT_EQ(0, tracker.getFailedAttemptCount("user1"));
}

TEST_F(LoginAttemptTrackerTest, SuccessfulLoginClearsAttempts) {
    LoginAttemptTracker tracker(fast_policy_);

    tracker.recordFailedAttempt("user1");
    tracker.recordFailedAttempt("user1");
    EXPECT_EQ(2, tracker.getFailedAttemptCount("user1"));

    // Successful login should clear attempts
    tracker.recordSuccessfulLogin("user1");

    EXPECT_EQ(0, tracker.getFailedAttemptCount("user1"));
    EXPECT_FALSE(tracker.isAccountLocked("user1"));
}

// ===== Account Lockout Tests =====

TEST_F(LoginAttemptTrackerTest, LockoutPreventsLogin) {
    LoginAttemptTracker tracker(fast_policy_);

    // Trigger lockout
    tracker.recordFailedAttempt("user1");
    tracker.recordFailedAttempt("user1");
    tracker.recordFailedAttempt("user1");

    EXPECT_TRUE(tracker.isAccountLocked("user1"));
}

TEST_F(LoginAttemptTrackerTest, LockoutExpiresAfterDuration) {
    LoginAttemptTracker tracker(fast_policy_);

    // Trigger lockout
    tracker.recordFailedAttempt("user1");
    tracker.recordFailedAttempt("user1");
    tracker.recordFailedAttempt("user1");

    EXPECT_TRUE(tracker.isAccountLocked("user1"));

    // Wait for lockout to expire (base_lockout_ms = 500ms)
    sleepMs(600);

    // Should no longer be locked
    EXPECT_FALSE(tracker.isAccountLocked("user1"));
    EXPECT_EQ(0, tracker.getLockoutTimeRemaining("user1"));
}

TEST_F(LoginAttemptTrackerTest, LockoutTimeRemaining) {
    LoginAttemptTracker tracker(fast_policy_);

    // Trigger lockout
    tracker.recordFailedAttempt("user1");
    tracker.recordFailedAttempt("user1");
    tracker.recordFailedAttempt("user1");

    uint64_t remaining = tracker.getLockoutTimeRemaining("user1");
    EXPECT_GT(remaining, 0);
    EXPECT_LE(remaining, fast_policy_.base_lockout_ms);

    // Wait a bit
    sleepMs(100);

    // Remaining time should decrease
    uint64_t remaining2 = tracker.getLockoutTimeRemaining("user1");
    EXPECT_LT(remaining2, remaining);
}

// ===== Exponential Backoff Tests =====

TEST_F(LoginAttemptTrackerTest, ExponentialBackoffIncreasesLockout) {
    LoginAttemptTracker tracker(fast_policy_);

    // First lockout
    tracker.recordFailedAttempt("user1");
    tracker.recordFailedAttempt("user1");
    tracker.recordFailedAttempt("user1");
    EXPECT_TRUE(tracker.isAccountLocked("user1"));

    uint64_t first_lockout = tracker.getLockoutTimeRemaining("user1");

    // Wait for lockout to expire
    sleepMs(600);
    EXPECT_FALSE(tracker.isAccountLocked("user1"));

    // Second lockout (should be longer)
    tracker.recordFailedAttempt("user1");
    tracker.recordFailedAttempt("user1");
    tracker.recordFailedAttempt("user1");
    EXPECT_TRUE(tracker.isAccountLocked("user1"));

    uint64_t second_lockout = tracker.getLockoutTimeRemaining("user1");

    // Second lockout should be longer (2x with exponential backoff)
    EXPECT_GT(second_lockout, first_lockout);
}

TEST_F(LoginAttemptTrackerTest, ExponentialBackoffMaxMultiplier) {
    LockoutPolicy policy = fast_policy_;
    policy.max_lockout_multiplier = 4;
    LoginAttemptTracker tracker(policy);

    // Trigger multiple lockouts
    // Expected runtime: ~20-25 seconds (10 iterations x ~2.1s sleep each).
    for (int i = 0; i < 10; i++) {
        // Wait for previous lockout to expire
        sleepMs(policy.base_lockout_ms * policy.max_lockout_multiplier + 100);

        // Trigger new lockout
        tracker.recordFailedAttempt("user1");
        tracker.recordFailedAttempt("user1");
        tracker.recordFailedAttempt("user1");
    }

    // Lockout should cap at max_lockout_multiplier
    uint64_t remaining = tracker.getLockoutTimeRemaining("user1");
    EXPECT_LE(remaining, policy.base_lockout_ms * policy.max_lockout_multiplier);
}

TEST_F(LoginAttemptTrackerTest, NoExponentialBackoffWhenDisabled) {
    LockoutPolicy policy = fast_policy_;
    policy.exponential_backoff = false;
    LoginAttemptTracker tracker(policy);

    // First lockout
    tracker.recordFailedAttempt("user1");
    tracker.recordFailedAttempt("user1");
    tracker.recordFailedAttempt("user1");
    uint64_t first_lockout = tracker.getLockoutTimeRemaining("user1");

    // Wait for lockout to expire
    sleepMs(600);

    // Second lockout
    tracker.recordFailedAttempt("user1");
    tracker.recordFailedAttempt("user1");
    tracker.recordFailedAttempt("user1");
    uint64_t second_lockout = tracker.getLockoutTimeRemaining("user1");

    // Should be approximately the same (no exponential backoff)
    int64_t diff = std::abs(static_cast<int64_t>(second_lockout) -
                             static_cast<int64_t>(first_lockout));
    EXPECT_LT(diff, 100);  // Allow 100ms tolerance
}

// ===== Reset Window Tests =====

TEST_F(LoginAttemptTrackerTest, AttemptsResetAfterWindow) {
    LoginAttemptTracker tracker(fast_policy_);

    tracker.recordFailedAttempt("user1");
    tracker.recordFailedAttempt("user1");
    EXPECT_EQ(2, tracker.getFailedAttemptCount("user1"));

    // Wait for reset window to expire
    sleepMs(fast_policy_.reset_window_ms + 100);

    // Attempts should be reset
    EXPECT_EQ(0, tracker.getFailedAttemptCount("user1"));
}

TEST_F(LoginAttemptTrackerTest, AttemptsNotResetBeforeWindow) {
    LoginAttemptTracker tracker(fast_policy_);

    tracker.recordFailedAttempt("user1");
    sleepMs(100);  // Wait a bit but not enough
    tracker.recordFailedAttempt("user1");

    EXPECT_EQ(2, tracker.getFailedAttemptCount("user1"));
}

// ===== Multiple Users Tests =====

TEST_F(LoginAttemptTrackerTest, MultipleUsersIndependent) {
    LoginAttemptTracker tracker(fast_policy_);

    tracker.recordFailedAttempt("user1");
    tracker.recordFailedAttempt("user1");

    tracker.recordFailedAttempt("user2");

    EXPECT_EQ(2, tracker.getFailedAttemptCount("user1"));
    EXPECT_EQ(1, tracker.getFailedAttemptCount("user2"));
}

TEST_F(LoginAttemptTrackerTest, OneLockoutDoesNotAffectOthers) {
    LoginAttemptTracker tracker(fast_policy_);

    // Lock user1
    tracker.recordFailedAttempt("user1");
    tracker.recordFailedAttempt("user1");
    tracker.recordFailedAttempt("user1");

    // user2 should not be affected
    EXPECT_TRUE(tracker.isAccountLocked("user1"));
    EXPECT_FALSE(tracker.isAccountLocked("user2"));
}

// ===== Admin Functions Tests =====

TEST_F(LoginAttemptTrackerTest, ClearAttemptsResetsCounter) {
    LoginAttemptTracker tracker(fast_policy_);

    tracker.recordFailedAttempt("user1");
    tracker.recordFailedAttempt("user1");
    EXPECT_EQ(2, tracker.getFailedAttemptCount("user1"));

    tracker.clearAttempts("user1");

    EXPECT_EQ(0, tracker.getFailedAttemptCount("user1"));
}

TEST_F(LoginAttemptTrackerTest, ClearAttemptsUnlocksAccount) {
    LoginAttemptTracker tracker(fast_policy_);

    // Lock account
    tracker.recordFailedAttempt("user1");
    tracker.recordFailedAttempt("user1");
    tracker.recordFailedAttempt("user1");
    EXPECT_TRUE(tracker.isAccountLocked("user1"));

    // Admin unlocks
    tracker.clearAttempts("user1");

    EXPECT_FALSE(tracker.isAccountLocked("user1"));
    EXPECT_EQ(0, tracker.getLockoutTimeRemaining("user1"));
}

TEST_F(LoginAttemptTrackerTest, GetTrackedAccountCount) {
    LoginAttemptTracker tracker(fast_policy_);

    EXPECT_EQ(0, tracker.getTrackedAccountCount());

    tracker.recordFailedAttempt("user1");
    EXPECT_EQ(1, tracker.getTrackedAccountCount());

    tracker.recordFailedAttempt("user2");
    EXPECT_EQ(2, tracker.getTrackedAccountCount());

    tracker.clearAttempts("user1");
    EXPECT_EQ(1, tracker.getTrackedAccountCount());
}

// ===== Cleanup Tests =====

TEST_F(LoginAttemptTrackerTest, CleanupExpiredEntries) {
    LoginAttemptTracker tracker(fast_policy_);

    // Create some failed attempts
    tracker.recordFailedAttempt("user1");
    tracker.recordFailedAttempt("user2");
    EXPECT_EQ(2, tracker.getTrackedAccountCount());

    // Wait for reset window
    sleepMs(fast_policy_.reset_window_ms + 100);

    // Cleanup should remove expired entries
    tracker.cleanupExpiredEntries();

    EXPECT_EQ(0, tracker.getTrackedAccountCount());
}

TEST_F(LoginAttemptTrackerTest, CleanupDoesNotRemoveActive) {
    LoginAttemptTracker tracker(fast_policy_);

    // Create locked account
    tracker.recordFailedAttempt("user1");
    tracker.recordFailedAttempt("user1");
    tracker.recordFailedAttempt("user1");
    EXPECT_TRUE(tracker.isAccountLocked("user1"));

    // Cleanup should not remove locked account
    tracker.cleanupExpiredEntries();

    EXPECT_EQ(1, tracker.getTrackedAccountCount());
    EXPECT_TRUE(tracker.isAccountLocked("user1"));
}

// ===== Edge Cases Tests =====

TEST_F(LoginAttemptTrackerTest, EmptyUsername) {
    LoginAttemptTracker tracker(fast_policy_);

    // Empty username should be handled gracefully
    tracker.recordFailedAttempt("");
    EXPECT_EQ(1, tracker.getFailedAttemptCount(""));
}

TEST_F(LoginAttemptTrackerTest, VeryLongUsername) {
    LoginAttemptTracker tracker(fast_policy_);

    std::string long_username(10000, 'a');
    tracker.recordFailedAttempt(long_username);
    EXPECT_EQ(1, tracker.getFailedAttemptCount(long_username));
}

TEST_F(LoginAttemptTrackerTest, SpecialCharactersInUsername) {
    LoginAttemptTracker tracker(fast_policy_);

    tracker.recordFailedAttempt("user@domain.com");
    tracker.recordFailedAttempt("user!#$%");
    tracker.recordFailedAttempt("用户");  // Chinese characters

    EXPECT_EQ(1, tracker.getFailedAttemptCount("user@domain.com"));
    EXPECT_EQ(1, tracker.getFailedAttemptCount("user!#$%"));
    EXPECT_EQ(1, tracker.getFailedAttemptCount("用户"));
}

TEST_F(LoginAttemptTrackerTest, ZeroMaxAttempts) {
    LockoutPolicy policy = fast_policy_;
    policy.max_attempts = 0;
    LoginAttemptTracker tracker(policy);

    // Should lock immediately (though this is an unusual config)
    // Implementation should handle gracefully
}

TEST_F(LoginAttemptTrackerTest, VeryHighMaxAttempts) {
    LockoutPolicy policy = fast_policy_;
    policy.max_attempts = 1000000;
    LoginAttemptTracker tracker(policy);

    // Should not lock with normal attempts
    for (int i = 0; i < 100; i++) {
        tracker.recordFailedAttempt("user1");
    }

    EXPECT_FALSE(tracker.isAccountLocked("user1"));
    EXPECT_EQ(100, tracker.getFailedAttemptCount("user1"));
}

// ===== Thread Safety Tests =====

TEST_F(LoginAttemptTrackerTest, ConcurrentFailedAttempts) {
    LoginAttemptTracker tracker(fast_policy_);

    // Multiple threads recording failed attempts
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; i++) {
        threads.emplace_back([&tracker]() {
            for (int j = 0; j < 10; j++) {
                tracker.recordFailedAttempt("user1");
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Some attempts may have triggered lockouts, but total tracked should make sense
    // (Not exactly 100 due to lockout resets)
    EXPECT_GT(tracker.getTrackedAccountCount(), 0);
}

TEST_F(LoginAttemptTrackerTest, ConcurrentDifferentUsers) {
    LoginAttemptTracker tracker(fast_policy_);

    // Multiple threads working with different users
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; i++) {
        threads.emplace_back([&tracker, i]() {
            std::string username = "user" + std::to_string(i);
            tracker.recordFailedAttempt(username);
            tracker.recordFailedAttempt(username);
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(10, tracker.getTrackedAccountCount());
}

// ===== Integration Test: Realistic Brute Force Scenario =====

TEST_F(LoginAttemptTrackerTest, RealisticBruteForceScenario) {
    // Use realistic policy
    LockoutPolicy policy;
    policy.max_attempts = 5;
    policy.reset_window_ms = 3600000;  // 1 hour
    policy.base_lockout_ms = 900000;   // 15 minutes
    policy.exponential_backoff = true;
    policy.max_lockout_multiplier = 8;

    LoginAttemptTracker tracker(policy);

    // Attacker tries 5 times
    for (int i = 0; i < 5; i++) {
        tracker.recordFailedAttempt("admin");
    }

    // Account should be locked
    EXPECT_TRUE(tracker.isAccountLocked("admin"));

    // Lockout should be ~15 minutes
    uint64_t remaining = tracker.getLockoutTimeRemaining("admin");
    EXPECT_GT(remaining, 800000);  // At least 13+ minutes
    EXPECT_LE(remaining, 900000);  // At most 15 minutes
}

TEST_F(LoginAttemptTrackerTest, LegitimateUserRecovery) {
    LoginAttemptTracker tracker(fast_policy_);

    // User makes 2 failed attempts
    tracker.recordFailedAttempt("user1");
    tracker.recordFailedAttempt("user1");
    EXPECT_EQ(2, tracker.getFailedAttemptCount("user1"));

    // User remembers password and logs in
    tracker.recordSuccessfulLogin("user1");

    // Counter should be reset
    EXPECT_EQ(0, tracker.getFailedAttemptCount("user1"));
    EXPECT_FALSE(tracker.isAccountLocked("user1"));

    // User can continue normally
    tracker.recordSuccessfulLogin("user1");
    EXPECT_FALSE(tracker.isAccountLocked("user1"));
}
