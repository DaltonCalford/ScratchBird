/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */

#include <gtest/gtest.h>

#include <thread>
#include <string>
#include <vector>

#include "scratchbird/core/login_attempt_tracker.h"

using namespace scratchbird::core;

namespace {

class AuthRateLimitStressTest : public ::testing::Test {
protected:
    void SetUp() override {
        policy_.max_attempts = 5;
        policy_.reset_window_ms = 60000;
        policy_.base_lockout_ms = 30000;
        policy_.exponential_backoff = true;
        policy_.max_lockout_multiplier = 8;
    }

    LockoutPolicy policy_{};
};

TEST_F(AuthRateLimitStressTest, ConcurrentFailureBurstLocksSingleAccount) {
    LoginAttemptTracker tracker(policy_);

    static constexpr int kThreads = 32;
    static constexpr int kAttemptsPerThread = 128;

    std::vector<std::thread> workers;
    workers.reserve(kThreads);

    for (int i = 0; i < kThreads; ++i) {
        workers.emplace_back([&tracker]() {
            for (int j = 0; j < kAttemptsPerThread; ++j) {
                tracker.recordFailedAttempt("hot_account");
            }
        });
    }

    for (auto& worker : workers) {
        worker.join();
    }

    EXPECT_TRUE(tracker.isAccountLocked("hot_account"));
    EXPECT_GT(tracker.getLockoutTimeRemaining("hot_account"), 0u);
    EXPECT_GE(tracker.getTrackedAccountCount(), 1u);
}

TEST_F(AuthRateLimitStressTest, ParallelFailureBurstsRemainIsolatedPerAccount) {
    LoginAttemptTracker tracker(policy_);

    static constexpr int kAccounts = 24;

    std::vector<std::thread> workers;
    workers.reserve(kAccounts);

    for (int i = 0; i < kAccounts; ++i) {
        workers.emplace_back([&tracker, i]() {
            const std::string username = "account_" + std::to_string(i);
            for (uint32_t attempt = 0; attempt < 5; ++attempt) {
                tracker.recordFailedAttempt(username);
            }
        });
    }

    for (auto& worker : workers) {
        worker.join();
    }

    for (int i = 0; i < kAccounts; ++i) {
        const std::string username = "account_" + std::to_string(i);
        EXPECT_TRUE(tracker.isAccountLocked(username)) << username;
    }

    EXPECT_EQ(tracker.getTrackedAccountCount(), static_cast<size_t>(kAccounts));
}

}  // namespace
