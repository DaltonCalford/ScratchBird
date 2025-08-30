// Copyright (c) ScratchBird Project
// SPDX-License-Identifier: Apache-2.0

#include "scratchbird/engine/plan_cache.h"
#include "scratchbird/engine/query_planner.h"

#include <chrono>
#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <vector>

using namespace scratchbird::engine;

// Mock QueryPlan class for testing
class MockQueryPlan : public QueryPlan
{
  public:
    std::string query_text;
    double estimated_cost{100.0};

    MockQueryPlan(const std::string& query) : query_text(query)
    {
        // Initialize base QueryPlan with a simple root node
        root = std::make_unique<PlanNode>("MockNode", query);
        root->estimated_cost = estimated_cost;
    }
};

class PlanCacheTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // Create test configuration
        config_.max_plans = 10;
        config_.max_memory_bytes = 1024 * 1024; // 1 MB
        config_.entry_ttl_seconds = 300;        // 5 minutes
        config_.cleanup_interval_seconds = 5;   // 5 seconds
        config_.enabled = true;

        // Create plan cache
        plan_cache_ = std::make_unique<PlanCache>(config_);
        ASSERT_TRUE(plan_cache_->initialize());

        // Create test plans
        test_plan1_ = std::make_shared<MockQueryPlan>("SELECT * FROM users WHERE id = ?");
        test_plan2_ = std::make_shared<MockQueryPlan>("SELECT name FROM products WHERE price > ?");
        test_plan3_ =
            std::make_shared<MockQueryPlan>("INSERT INTO orders (user_id, total) VALUES (?, ?)");
    }

    void TearDown() override
    {
        if (plan_cache_) {
            plan_cache_->shutdown();
        }
    }

    PlanCacheConfig config_;
    std::unique_ptr<PlanCache> plan_cache_;
    std::shared_ptr<MockQueryPlan> test_plan1_;
    std::shared_ptr<MockQueryPlan> test_plan2_;
    std::shared_ptr<MockQueryPlan> test_plan3_;
};

TEST_F(PlanCacheTest, PlanKeyConstruction)
{
    // Test basic key construction
    PlanKey key1("SELECT * FROM users");
    EXPECT_FALSE(key1.query_text.empty());
    EXPECT_NE(key1.hash(), 0);

    // Test key with database and schema
    PlanKey key2("SELECT * FROM users", "test_db", "public");
    EXPECT_EQ(key2.database_name, "test_db");
    EXPECT_EQ(key2.schema_name, "public");
    EXPECT_NE(key2.hash(), key1.hash());

    // Test key equality
    PlanKey key3("SELECT * FROM users");
    EXPECT_TRUE(key1 == key3);
    EXPECT_FALSE(key1 == key2);

    // Test query normalization
    PlanKey key4("select   * from   users");
    PlanKey key5("SELECT * FROM USERS");
    EXPECT_EQ(key4.query_text, key5.query_text);
}

TEST_F(PlanCacheTest, ExecutionStatistics)
{
    ExecutionStatistics stats;

    // Initial state
    EXPECT_EQ(stats.execution_count.load(), 0);
    EXPECT_EQ(stats.total_execution_time_us.load(), 0);
    EXPECT_EQ(stats.avg_execution_time_us.load(), 0);

    // Update statistics
    stats.update_execution_stats(1000, 100); // 1ms, 100 rows
    EXPECT_EQ(stats.execution_count.load(), 1);
    EXPECT_EQ(stats.total_execution_time_us.load(), 1000);
    EXPECT_EQ(stats.avg_execution_time_us.load(), 1000);
    EXPECT_EQ(stats.min_execution_time_us.load(), 1000);
    EXPECT_EQ(stats.max_execution_time_us.load(), 1000);

    // Update with different timing
    stats.update_execution_stats(2000, 200);
    EXPECT_EQ(stats.execution_count.load(), 2);
    EXPECT_EQ(stats.total_execution_time_us.load(), 3000);
    EXPECT_EQ(stats.avg_execution_time_us.load(), 1500);
    EXPECT_EQ(stats.min_execution_time_us.load(), 1000);
    EXPECT_EQ(stats.max_execution_time_us.load(), 2000);

    // Test reset
    stats.reset();
    EXPECT_EQ(stats.execution_count.load(), 0);
    EXPECT_EQ(stats.total_execution_time_us.load(), 0);
}

TEST_F(PlanCacheTest, CachedPlanCreation)
{
    PlanKey key("SELECT * FROM users WHERE id = ?");

    // Create cached plan
    auto cached_plan = std::make_shared<CachedPlan>(key, test_plan1_, CachedPlanType::GENERIC_PLAN);

    EXPECT_EQ(cached_plan->get_key(), key);
    EXPECT_EQ(cached_plan->get_plan(), test_plan1_);
    EXPECT_EQ(cached_plan->get_type(), CachedPlanType::GENERIC_PLAN);
    EXPECT_TRUE(cached_plan->is_valid());
    EXPECT_EQ(cached_plan->get_access_count(), 0);

    // Test access recording
    cached_plan->record_access();
    EXPECT_EQ(cached_plan->get_access_count(), 1);

    // Test flags
    EXPECT_FALSE(cached_plan->has_flags(CachedPlanFlags::PINNED));
    cached_plan->set_flags(CachedPlanFlags::PINNED);
    EXPECT_TRUE(cached_plan->has_flags(CachedPlanFlags::PINNED));

    cached_plan->add_flags(CachedPlanFlags::READ_ONLY);
    EXPECT_TRUE(cached_plan->has_flags(CachedPlanFlags::PINNED));
    EXPECT_TRUE(cached_plan->has_flags(CachedPlanFlags::READ_ONLY));

    cached_plan->remove_flags(CachedPlanFlags::PINNED);
    EXPECT_FALSE(cached_plan->has_flags(CachedPlanFlags::PINNED));
    EXPECT_TRUE(cached_plan->has_flags(CachedPlanFlags::READ_ONLY));

    // Test invalidation
    cached_plan->invalidate();
    EXPECT_FALSE(cached_plan->is_valid());
}

TEST_F(PlanCacheTest, BasicCacheOperations)
{
    PlanKey key1("SELECT * FROM users WHERE id = ?");
    PlanKey key2("SELECT name FROM products WHERE price > ?");

    // Test insertion
    EXPECT_TRUE(plan_cache_->insert_plan(key1, test_plan1_));
    EXPECT_TRUE(plan_cache_->insert_plan(key2, test_plan2_));

    // Test lookup - should find plans
    auto cached_plan1 = plan_cache_->lookup_plan(key1);
    ASSERT_TRUE(cached_plan1 != nullptr);
    EXPECT_EQ(cached_plan1->get_plan(), test_plan1_);

    auto cached_plan2 = plan_cache_->lookup_plan(key2);
    ASSERT_TRUE(cached_plan2 != nullptr);
    EXPECT_EQ(cached_plan2->get_plan(), test_plan2_);

    // Test lookup for non-existent plan
    PlanKey key3("SELECT * FROM orders");
    auto cached_plan3 = plan_cache_->lookup_plan(key3);
    EXPECT_TRUE(cached_plan3 == nullptr);

    // Test removal
    EXPECT_TRUE(plan_cache_->remove_plan(key1));
    cached_plan1 = plan_cache_->lookup_plan(key1);
    EXPECT_TRUE(cached_plan1 == nullptr);

    // Test removing non-existent plan
    EXPECT_FALSE(plan_cache_->remove_plan(key3));
}

TEST_F(PlanCacheTest, CacheStatistics)
{
    PlanKey key1("SELECT * FROM users WHERE id = ?");
    PlanKey key2("SELECT name FROM products WHERE price > ?");
    PlanKey key3("SELECT * FROM orders");

    // Initial statistics
    auto stats = plan_cache_->get_statistics();
    EXPECT_EQ(stats.cache_hits.load(), 0);
    EXPECT_EQ(stats.cache_misses.load(), 0);
    EXPECT_EQ(stats.total_plans.load(), 0);

    // Insert plans and check statistics
    plan_cache_->insert_plan(key1, test_plan1_);
    plan_cache_->insert_plan(key2, test_plan2_);

    stats = plan_cache_->get_statistics();
    EXPECT_EQ(stats.total_plans.load(), 2);
    EXPECT_EQ(stats.generic_plans.load(), 2);

    // Test cache hits and misses
    auto cached_plan = plan_cache_->lookup_plan(key1); // Hit
    EXPECT_TRUE(cached_plan != nullptr);

    cached_plan = plan_cache_->lookup_plan(key3); // Miss
    EXPECT_TRUE(cached_plan == nullptr);

    stats = plan_cache_->get_statistics();
    EXPECT_EQ(stats.cache_hits.load(), 1);
    EXPECT_EQ(stats.cache_misses.load(), 1);
    EXPECT_DOUBLE_EQ(stats.get_hit_ratio(), 0.5);

    // Test statistics reset
    plan_cache_->reset_statistics();
    stats = plan_cache_->get_statistics();
    EXPECT_EQ(stats.cache_hits.load(), 0);
    EXPECT_EQ(stats.cache_misses.load(), 0);
}

TEST_F(PlanCacheTest, CacheEviction)
{
    // Set small cache limit for testing
    PlanCacheConfig small_config = config_;
    small_config.max_plans = 3;
    plan_cache_->update_config(small_config);

    // Insert plans up to limit
    PlanKey key1("SELECT * FROM users WHERE id = 1");
    PlanKey key2("SELECT * FROM users WHERE id = 2");
    PlanKey key3("SELECT * FROM users WHERE id = 3");
    PlanKey key4("SELECT * FROM users WHERE id = 4");

    EXPECT_TRUE(plan_cache_->insert_plan(key1, test_plan1_));
    EXPECT_TRUE(plan_cache_->insert_plan(key2, test_plan2_));
    EXPECT_TRUE(plan_cache_->insert_plan(key3, test_plan3_));

    auto stats = plan_cache_->get_statistics();
    EXPECT_EQ(stats.total_plans.load(), 3);

    // Insert another plan - should trigger eviction
    EXPECT_TRUE(plan_cache_->insert_plan(key4, test_plan1_));

    // Should still have 3 plans (or fewer if eviction occurred)
    stats = plan_cache_->get_statistics();
    EXPECT_LE(stats.total_plans.load(), 3);

    // Verify that the newest plan is in cache
    auto cached_plan4 = plan_cache_->lookup_plan(key4);
    EXPECT_TRUE(cached_plan4 != nullptr);

    // Test manual eviction
    auto evicted_count = plan_cache_->evict_plans(1);
    EXPECT_GT(evicted_count, 0);

    stats = plan_cache_->get_statistics();
    EXPECT_GT(stats.cache_evictions.load(), 0);
}

TEST_F(PlanCacheTest, PlanInvalidation)
{
    PlanKey key1("SELECT * FROM users WHERE id = ?");
    PlanKey key2("SELECT * FROM products WHERE price > ?");
    PlanKey key3("SELECT * FROM orders WHERE user_id = ?");

    // Insert plans
    plan_cache_->insert_plan(key1, test_plan1_);
    plan_cache_->insert_plan(key2, test_plan2_);
    plan_cache_->insert_plan(key3, test_plan3_);

    // Test pattern-based invalidation
    auto invalidated_count = plan_cache_->invalidate_plans(".*users.*");
    EXPECT_GT(invalidated_count, 0);

    // Check that user-related plans are invalidated
    auto cached_plan1 = plan_cache_->lookup_plan(key1);
    if (cached_plan1) {
        EXPECT_FALSE(cached_plan1->is_valid());
    }

    // Test schema-based invalidation
    PlanKey key_with_schema("SELECT * FROM test_table", "test_db", "public");
    plan_cache_->insert_plan(key_with_schema, test_plan1_);

    invalidated_count = plan_cache_->invalidate_plans_by_schema("test_db", "public");
    EXPECT_GT(invalidated_count, 0);

    auto stats = plan_cache_->get_statistics();
    EXPECT_GT(stats.cache_invalidations.load(), 0);
}

TEST_F(PlanCacheTest, ConfigurationManagement)
{
    // Test getting current config
    auto current_config = plan_cache_->get_config();
    EXPECT_EQ(current_config.max_plans, config_.max_plans);
    EXPECT_EQ(current_config.max_memory_bytes, config_.max_memory_bytes);
    EXPECT_TRUE(current_config.enabled);

    // Test updating config
    PlanCacheConfig new_config = current_config;
    new_config.max_plans = 20;
    new_config.entry_ttl_seconds = 600;

    EXPECT_TRUE(plan_cache_->update_config(new_config));

    auto updated_config = plan_cache_->get_config();
    EXPECT_EQ(updated_config.max_plans, 20);
    EXPECT_EQ(updated_config.entry_ttl_seconds, 600);

    // Test invalid config
    PlanCacheConfig invalid_config;
    invalid_config.max_plans = 0; // Invalid
    EXPECT_FALSE(plan_cache_->update_config(invalid_config));
}

TEST_F(PlanCacheTest, MemoryManagement)
{
    PlanKey key1("SELECT * FROM users WHERE id = ?");
    PlanKey key2("SELECT name FROM products WHERE price > ?");

    // Insert plans
    plan_cache_->insert_plan(key1, test_plan1_);
    plan_cache_->insert_plan(key2, test_plan2_);

    // Check memory usage
    auto memory_usage = plan_cache_->get_memory_usage();
    EXPECT_GT(memory_usage, 0);

    // Check capacity utilization
    auto utilization = plan_cache_->get_capacity_utilization();
    EXPECT_GT(utilization, 0.0);
    EXPECT_LE(utilization, 1.0);

    // Test garbage collection
    plan_cache_->garbage_collect();

    // Memory usage should remain the same or decrease
    auto new_memory_usage = plan_cache_->get_memory_usage();
    EXPECT_LE(new_memory_usage, memory_usage);
}

TEST_F(PlanCacheTest, PlanCacheIntrospection)
{
    PlanKey key1("SELECT * FROM users WHERE id = ?");
    PlanKey key2("SELECT name FROM products WHERE price > ?");

    // Insert plans
    plan_cache_->insert_plan(key1, test_plan1_, CachedPlanType::GENERIC_PLAN);
    plan_cache_->insert_plan(key2, test_plan2_, CachedPlanType::SPECIFIC_PLAN);

    // Test getting all plan keys
    auto all_keys = plan_cache_->get_all_plan_keys();
    EXPECT_EQ(all_keys.size(), 2);

    // Test getting plan details
    auto plan_details1 = plan_cache_->get_plan_details(key1);
    ASSERT_TRUE(plan_details1 != nullptr);
    EXPECT_EQ(plan_details1->get_type(), CachedPlanType::GENERIC_PLAN);

    auto plan_details2 = plan_cache_->get_plan_details(key2);
    ASSERT_TRUE(plan_details2 != nullptr);
    EXPECT_EQ(plan_details2->get_type(), CachedPlanType::SPECIFIC_PLAN);

    // Test getting plan details for non-existent key
    PlanKey non_existent_key("SELECT * FROM orders");
    auto non_existent_plan = plan_cache_->get_plan_details(non_existent_key);
    EXPECT_TRUE(non_existent_plan == nullptr);
}

TEST_F(PlanCacheTest, PerformanceReport)
{
    PlanKey key1("SELECT * FROM users WHERE id = ?");
    PlanKey key2("SELECT name FROM products WHERE price > ?");

    // Insert plans and perform some operations
    plan_cache_->insert_plan(key1, test_plan1_);
    plan_cache_->insert_plan(key2, test_plan2_);

    plan_cache_->lookup_plan(key1);                            // Hit
    plan_cache_->lookup_plan(PlanKey("SELECT * FROM orders")); // Miss

    // Generate performance report
    auto report = plan_cache_->generate_performance_report();

    EXPECT_FALSE(report.empty());
    EXPECT_NE(report.find("Plan Cache Performance Report"), std::string::npos);
    EXPECT_NE(report.find("Total Plans:"), std::string::npos);
    EXPECT_NE(report.find("Hit Ratio:"), std::string::npos);
    EXPECT_NE(report.find("Memory Usage:"), std::string::npos);
}

TEST_F(PlanCacheTest, DisabledCache)
{
    // Create cache with disabled configuration
    PlanCacheConfig disabled_config = config_;
    disabled_config.enabled = false;

    auto disabled_cache = std::make_unique<PlanCache>(disabled_config);
    ASSERT_TRUE(disabled_cache->initialize());

    PlanKey key("SELECT * FROM users WHERE id = ?");

    // Insert should fail when disabled
    EXPECT_FALSE(disabled_cache->insert_plan(key, test_plan1_));

    // Lookup should return nullptr and record miss
    auto cached_plan = disabled_cache->lookup_plan(key);
    EXPECT_TRUE(cached_plan == nullptr);

    auto stats = disabled_cache->get_statistics();
    EXPECT_EQ(stats.cache_misses.load(), 1);

    disabled_cache->shutdown();
}

TEST_F(PlanCacheTest, ClearCache)
{
    PlanKey key1("SELECT * FROM users WHERE id = ?");
    PlanKey key2("SELECT name FROM products WHERE price > ?");

    // Insert plans
    plan_cache_->insert_plan(key1, test_plan1_);
    plan_cache_->insert_plan(key2, test_plan2_);

    auto stats = plan_cache_->get_statistics();
    EXPECT_EQ(stats.total_plans.load(), 2);

    // Clear cache
    plan_cache_->clear();

    stats = plan_cache_->get_statistics();
    EXPECT_EQ(stats.total_plans.load(), 0);
    EXPECT_EQ(stats.memory_usage_bytes.load(), 0);

    // Verify plans are gone
    auto cached_plan1 = plan_cache_->lookup_plan(key1);
    EXPECT_TRUE(cached_plan1 == nullptr);

    auto cached_plan2 = plan_cache_->lookup_plan(key2);
    EXPECT_TRUE(cached_plan2 == nullptr);
}

TEST_F(PlanCacheTest, ThreadSafety)
{
    const int NUM_THREADS = 4;
    const int OPERATIONS_PER_THREAD = 100;
    std::vector<std::thread> threads;

    // Start multiple threads performing cache operations
    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([this, t, OPERATIONS_PER_THREAD]() {
            for (int i = 0; i < OPERATIONS_PER_THREAD; ++i) {
                std::string query =
                    "SELECT * FROM table" + std::to_string(t) + " WHERE id = " + std::to_string(i);
                PlanKey key(query);

                // Insert plan
                plan_cache_->insert_plan(key, test_plan1_);

                // Lookup plan
                auto cached_plan = plan_cache_->lookup_plan(key);

                // Occasionally remove plan
                if (i % 10 == 0) {
                    plan_cache_->remove_plan(key);
                }
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // Should complete without crashes or hangs
    auto stats = plan_cache_->get_statistics();
    EXPECT_GT(stats.cache_hits.load() + stats.cache_misses.load(), 0);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
