#include <gtest/gtest.h>

#include <cstdio>
#include <memory>
#include <string>
#include <unistd.h>
#include <vector>

#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/telemetry.h"

using namespace scratchbird::core;

namespace
{
auto metricCounterValue(const std::string& metric_name,
                        const std::vector<std::string>& labels) -> double
{
    auto* metric = MetricsRegistry::getInstance().get(metric_name);
    if (metric == nullptr)
    {
        return 0.0;
    }
    auto* counter = dynamic_cast<Counter*>(metric);
    if (counter == nullptr)
    {
        return 0.0;
    }
    return counter->get(labels);
}
} // namespace

class VNextStorageMetricsContractTest : public ::testing::Test
{
protected:
    std::string db_path_;
    std::unique_ptr<Database> db_;

    void SetUp() override
    {
        db_path_ = "/tmp/test_vnext_storage_metrics_contract_" + std::to_string(getpid()) + ".db";
        std::remove(db_path_.c_str());

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(db_path_, &ctx), Status::OK) << ctx.message;
    }

    void TearDown() override
    {
        if (db_)
        {
            db_->close();
            db_.reset();
        }
        std::remove(db_path_.c_str());
    }
};

TEST_F(VNextStorageMetricsContractTest, BufferPoolEventsEmitCanonicalVNextStorageCounters)
{
    ErrorContext ctx;
    const std::string metric = "scratchbird_vnext_storage_events_total";

    const double hit_before = metricCounterValue(metric, {"buffer_pool_pin", "hit", "NONE"});
    const double miss_before = metricCounterValue(metric, {"buffer_pool_pin", "miss", "NONE"});
    const double read_before = metricCounterValue(metric, {"buffer_pool_io_read", "ok", "NONE"});

    uint32_t page_id = 0;
    ASSERT_EQ(db_->page_manager()->allocatePage(page_id, &ctx), Status::OK) << ctx.message;

    void* page_buffer = nullptr;
    ASSERT_EQ(db_->buffer_pool()->pinPage(page_id, &page_buffer, &ctx), Status::OK) << ctx.message;
    ASSERT_NE(page_buffer, nullptr);
    ASSERT_EQ(db_->buffer_pool()->unpinPage(page_id, false, &ctx), Status::OK) << ctx.message;

    page_buffer = nullptr;
    ASSERT_EQ(db_->buffer_pool()->pinPage(page_id, &page_buffer, &ctx), Status::OK) << ctx.message;
    ASSERT_NE(page_buffer, nullptr);
    ASSERT_EQ(db_->buffer_pool()->unpinPage(page_id, false, &ctx), Status::OK) << ctx.message;

    const double hit_after = metricCounterValue(metric, {"buffer_pool_pin", "hit", "NONE"});
    const double miss_after = metricCounterValue(metric, {"buffer_pool_pin", "miss", "NONE"});
    const double read_after = metricCounterValue(metric, {"buffer_pool_io_read", "ok", "NONE"});

    EXPECT_GE(hit_after - hit_before, 1.0);
    EXPECT_GE(miss_after - miss_before, 1.0);
    EXPECT_GE(read_after - read_before, 1.0);
}
