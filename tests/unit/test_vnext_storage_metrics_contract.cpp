#include <gtest/gtest.h>

#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>
#include <unistd.h>
#include <vector>

#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/observability_contract.h"
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

TEST_F(VNextStorageMetricsContractTest, MgaRuntimeMetricsExposeBufferPolicyAndPrefetchCounters)
{
    ErrorContext ctx;

    uint32_t prefetched_page = 0;
    ASSERT_EQ(db_->page_manager()->allocatePage(prefetched_page, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(db_->buffer_pool()->prefetchPages({prefetched_page}, &ctx), Status::OK) << ctx.message;

    void* page_buffer = nullptr;
    ASSERT_EQ(db_->buffer_pool()->pinPage(prefetched_page, &page_buffer, &ctx), Status::OK)
        << ctx.message;
    ASSERT_NE(page_buffer, nullptr);
    ASSERT_EQ(db_->buffer_pool()->unpinPage(prefetched_page, false, &ctx), Status::OK)
        << ctx.message;

    const uint64_t now_ms = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    std::vector<SqlRuntimeMetricRow> rows;
    ASSERT_EQ(SqlObservabilityViewBuilder::buildMgaRuntimeRows(
                  *db_, MetricsRegistry::getInstance(), now_ms, rows),
              Status::OK);

    const auto find_metric = [&rows](const std::string& metric_name,
                                     const std::string& labels_fragment)
        -> std::vector<SqlRuntimeMetricRow>::const_iterator {
        return std::find_if(
            rows.begin(), rows.end(),
            [&](const SqlRuntimeMetricRow& row) {
                return row.metric_name == metric_name &&
                       row.labels_json.find(labels_fragment) != std::string::npos;
            });
    };

    const auto prefetch_total = find_metric("sb_buf_prefetch_pages_total", "\"db\":");
    ASSERT_NE(prefetch_total, rows.end());
    EXPECT_EQ(prefetch_total->value, 1.0);

    const auto prefetch_useful =
        find_metric("sb_buf_prefetch_pages_useful_total", "\"db\":");
    ASSERT_NE(prefetch_useful, rows.end());
    EXPECT_EQ(prefetch_useful->value, 1.0);

    const auto hot_domain =
        find_metric("sb_buf_domain_resident_pages", "\"domain\":\"hot_oltp\"");
    ASSERT_NE(hot_domain, rows.end());
    EXPECT_GE(hot_domain->value, 1.0);

    const auto usefulness =
        find_metric("sb_buf_prefetch_usefulness_pct", "\"db\":");
    ASSERT_NE(usefulness, rows.end());
    EXPECT_GE(usefulness->value, 100.0);
}
