/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/clock_control.h"

#include <gtest/gtest.h>

namespace
{

TEST(ClockControlTest, MonotonicNowAndSleepAdvanceTime)
{
    auto clock = scratchbird::core::createDefaultClockControl();
    ASSERT_NE(clock, nullptr);

    auto before = clock->monotonicNow();
    clock->sleepFor(std::chrono::milliseconds(5));
    auto after = clock->monotonicNow();

    EXPECT_GE(after, before);
}

TEST(ClockControlTest, RealtimeNowMillisecondsIsNonZero)
{
    auto clock = scratchbird::core::createDefaultClockControl();
    ASSERT_NE(clock, nullptr);

    uint64_t now_ms = clock->realtimeNowMs();
    EXPECT_GT(now_ms, 0u);
}

} // namespace
