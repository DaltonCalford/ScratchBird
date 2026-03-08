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

#include "scratchbird/optimizer/query_profiler.h"

using scratchbird::optimizer::QueryProfile;
using scratchbird::optimizer::QueryProfiler;
using scratchbird::optimizer::RegressionPolicy;
using scratchbird::optimizer::RegressionSeverity;

namespace {

class QueryProfilerTest : public ::testing::Test {
protected:
    void SetUp() override {
        profiler_.setEnabled(true);
        profiler_.clearProfiles();
        profiler_.clearProtectedWorkloads();
        profiler_.setRegressionPolicy(RegressionPolicy{});
    }

    void TearDown() override {
        profiler_.clearProfiles();
        profiler_.clearProtectedWorkloads();
        profiler_.setRegressionPolicy(RegressionPolicy{});
        profiler_.setEnabled(false);
    }

    std::shared_ptr<QueryProfile> makeProfile(const std::string& sql,
                                              uint64_t execution_time_us,
                                              uint64_t planning_time_us = 10) {
        auto profile = profiler_.beginProfile(sql);
        if (profile) {
            profile->setPlanningTime(planning_time_us);
            profile->setExecutionTime(execution_time_us);
        }
        return profile;
    }

    QueryProfiler& profiler_ = QueryProfiler::getInstance();
};

TEST_F(QueryProfilerTest, StoresHistoryByNormalizedFingerprint) {
    auto first = makeProfile("SELECT * FROM t WHERE id = 1", 100);
    ASSERT_NE(first, nullptr);
    profiler_.endProfile(first);

    auto second = makeProfile(" select  *  from t where id = 2 ", 120);
    ASSERT_NE(second, nullptr);
    profiler_.endProfile(second);

    const auto fp1 = profiler_.fingerprintQuery("SELECT * FROM t WHERE id = 1");
    const auto fp2 = profiler_.fingerprintQuery("select * from t where id = 2");
    EXPECT_EQ(fp1, fp2);

    const auto history = profiler_.getHistoryForQuery("SELECT * FROM t WHERE id = 999");
    ASSERT_EQ(history.size(), 2u);
    EXPECT_EQ(history[0].fingerprint, fp1);
    EXPECT_EQ(history[1].fingerprint, fp1);
    EXPECT_EQ(history[0].sequence, 1u);
    EXPECT_EQ(history[1].sequence, 2u);
}

TEST_F(QueryProfilerTest, DetectsRegressionForProtectedWorkload) {
    RegressionPolicy policy;
    policy.enabled = true;
    policy.max_execution_time_ratio = 1.5;
    policy.min_baseline_samples = 3;
    profiler_.setRegressionPolicy(policy);
    profiler_.markProtectedWorkload("SELECT * FROM t WHERE id = ?");

    for (uint64_t us : {100u, 105u, 95u}) {
        auto profile = makeProfile("SELECT * FROM t WHERE id = 1", us);
        ASSERT_NE(profile, nullptr);
        profiler_.endProfile(profile);
    }

    auto slow = makeProfile("SELECT * FROM t WHERE id = 42", 260);
    ASSERT_NE(slow, nullptr);
    profiler_.endProfile(slow);

    auto latest = profiler_.latestRegressionForQuery("SELECT * FROM t WHERE id = 99");
    ASSERT_TRUE(latest.has_value());
    EXPECT_TRUE(latest->available);
    EXPECT_TRUE(latest->protected_workload);
    EXPECT_TRUE(latest->regression_detected);
    EXPECT_EQ(latest->severity, RegressionSeverity::MAJOR);
    EXPECT_GT(latest->execution_time_ratio, 1.5);
    EXPECT_EQ(latest->baseline_sample_count, 3u);

    const auto recent = profiler_.getRecentProfiles(1);
    ASSERT_EQ(recent.size(), 1u);
    EXPECT_TRUE(recent.back()->protectedWorkload());
    EXPECT_TRUE(recent.back()->regressionSignal().regression_detected);
}

TEST_F(QueryProfilerTest, LeavesUnprotectedWorkloadsOutOfRegressionGate) {
    RegressionPolicy policy;
    policy.enabled = true;
    policy.max_execution_time_ratio = 1.1;
    policy.min_baseline_samples = 2;
    profiler_.setRegressionPolicy(policy);

    auto first = makeProfile("SELECT * FROM t WHERE id = 1", 100);
    auto second = makeProfile("SELECT * FROM t WHERE id = 2", 100);
    auto third = makeProfile("SELECT * FROM t WHERE id = 3", 250);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    ASSERT_NE(third, nullptr);
    profiler_.endProfile(first);
    profiler_.endProfile(second);
    profiler_.endProfile(third);

    auto latest = profiler_.latestRegressionForQuery("SELECT * FROM t WHERE id = 4");
    ASSERT_TRUE(latest.has_value());
    EXPECT_FALSE(latest->available);
    EXPECT_FALSE(latest->protected_workload);
    EXPECT_FALSE(latest->regression_detected);
    EXPECT_EQ(latest->severity, RegressionSeverity::NONE);
}

} // namespace
