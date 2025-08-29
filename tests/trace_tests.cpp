#include "scratchbird/trace/trace.h"

#include <gtest/gtest.h>
#include <thread>

using scratchbird::trace::TraceController;
using scratchbird::trace::TraceProfileConfig;
using scratchbird::trace::TraceSpan;

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

TEST(TraceTests, StartStopProfile)
{
    auto& ctl = TraceController::instance();
    EXPECT_FALSE(ctl.is_running());

    TraceProfileConfig cfg;
    cfg.name = "default";
    cfg.sample_ratio = 1.0; // sample all for test
    cfg.buffer_capacity = 128;
    ctl.start_profile(cfg);
    EXPECT_TRUE(ctl.is_running());

    {
        TraceSpan span("test", "outer");
        {
            TraceSpan inner("test", "inner", span.id());
        }
    }

    auto events = ctl.collect();
    EXPECT_GE(events.size(), 2u); // at least begin+end

    ctl.stop_profile();
    EXPECT_FALSE(ctl.is_running());
}

TEST(TraceTests, Sampling)
{
    auto& ctl = TraceController::instance();
    TraceProfileConfig cfg;
    cfg.name = "low";
    cfg.sample_ratio = 0.0; // sample none
    cfg.buffer_capacity = 64;
    ctl.start_profile(cfg);

    {
        TraceSpan span("test", "nosample");
    }

    auto events = ctl.collect();
    EXPECT_EQ(events.size(), 0u);
    ctl.stop_profile();
}
