// Copyright (c) ScratchBird Project
// SPDX-License-Identifier: Apache-2.0

#include "scratchbird/engine/prepared_statement_cache.h"

#include <chrono>
#include <fstream>
#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <vector>

using namespace scratchbird::engine;

// Forward declaration for PreparedStatement (will be implemented later)
namespace scratchbird::engine
{
    class PreparedStatement
    {
      public:
        virtual ~PreparedStatement() = default;
    };
} // namespace scratchbird::engine

// Mock PreparedStatement class for testing
class MockPreparedStatement : public scratchbird::engine::PreparedStatement
{
  public:
    std::string sql_text;
    std::vector<ParameterDescriptor> parameters;
    std::vector<ColumnMetadata> columns;

    MockPreparedStatement(const std::string& sql) : sql_text(sql) {}

    void add_parameter(const std::string& name, const std::string& type, std::uint32_t ordinal)
    {
        parameters.emplace_back(name, type, ordinal);
    }

    void add_column(const std::string& name, const std::string& type, std::uint32_t ordinal)
    {
        columns.emplace_back(name, type, ordinal);
    }
};

class PreparedStatementCacheTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // Create test configuration
        config_.max_statements = 10;
        config_.max_memory_bytes = 1024 * 1024; // 1 MB
        config_.entry_ttl_seconds = 300;        // 5 minutes
        config_.cleanup_interval_seconds = 5;   // 5 seconds
        config_.enabled = true;

        // Create prepared statement cache
        stmt_cache_ = std::make_unique<PreparedStatementCache>(config_);
        ASSERT_TRUE(stmt_cache_->initialize());

        // Create test prepared statements
        test_stmt1_ = std::make_shared<MockPreparedStatement>("SELECT * FROM users WHERE id = ?");
        test_stmt1_->add_parameter("id", "INTEGER", 1);
        test_stmt1_->add_column("id", "INTEGER", 0);
        test_stmt1_->add_column("name", "VARCHAR", 1);
        test_stmt1_->add_column("email", "VARCHAR", 2);

        test_stmt2_ =
            std::make_shared<MockPreparedStatement>("SELECT name FROM products WHERE price > ?");
        test_stmt2_->add_parameter("price", "DECIMAL", 1);
        test_stmt2_->add_column("name", "VARCHAR", 0);

        test_stmt3_ = std::make_shared<MockPreparedStatement>(
            "INSERT INTO orders (user_id, total) VALUES (?, ?)");
        test_stmt3_->add_parameter("user_id", "INTEGER", 1);
        test_stmt3_->add_parameter("total", "DECIMAL", 2);

        // Create test metadata
        metadata1_.statement_type = "SELECT";
        metadata1_.read_only = true;
        metadata1_.returns_results = true;
        metadata1_.complexity = StatementMetadata::Complexity::SIMPLE;
        metadata1_.parameters = test_stmt1_->parameters;
        metadata1_.columns = test_stmt1_->columns;

        metadata2_.statement_type = "SELECT";
        metadata2_.read_only = true;
        metadata2_.returns_results = true;
        metadata2_.complexity = StatementMetadata::Complexity::SIMPLE;
        metadata2_.parameters = test_stmt2_->parameters;
        metadata2_.columns = test_stmt2_->columns;

        metadata3_.statement_type = "INSERT";
        metadata3_.read_only = false;
        metadata3_.returns_results = false;
        metadata3_.complexity = StatementMetadata::Complexity::MODERATE;
        metadata3_.parameters = test_stmt3_->parameters;
    }

    void TearDown() override
    {
        if (stmt_cache_) {
            stmt_cache_->shutdown();
        }
    }

    PreparedStatementCacheConfig config_;
    std::unique_ptr<PreparedStatementCache> stmt_cache_;

    std::shared_ptr<MockPreparedStatement> test_stmt1_;
    std::shared_ptr<MockPreparedStatement> test_stmt2_;
    std::shared_ptr<MockPreparedStatement> test_stmt3_;

    StatementMetadata metadata1_;
    StatementMetadata metadata2_;
    StatementMetadata metadata3_;
};

TEST_F(PreparedStatementCacheTest, PreparedStatementKeyConstruction)
{
    PreparedStatementKey key1("SELECT * FROM users WHERE id = ?");
    EXPECT_FALSE(key1.sql_text.empty());
    EXPECT_EQ(key1.sql_text, "SELECT * FROM users WHERE id = ?");

    PreparedStatementKey key2("SELECT * FROM users WHERE id = ?", "testdb", "public", "testuser");
    EXPECT_EQ(key2.database_name, "testdb");
    EXPECT_EQ(key2.schema_name, "public");
    EXPECT_EQ(key2.user_name, "testuser");

    // Test equality
    PreparedStatementKey key3("SELECT * FROM users WHERE id = ?");
    EXPECT_EQ(key1, key3);
    EXPECT_NE(key1, key2);

    // Test hash
    EXPECT_EQ(key1.hash(), key3.hash());
    EXPECT_NE(key1.hash(), key2.hash());
}

TEST_F(PreparedStatementCacheTest, SQLNormalization)
{
    std::string sql1 = "  select   *  from   users  where  id = ? ";
    std::string sql2 = "SELECT * FROM users WHERE id = ?";

    std::string normalized1 = PreparedStatementKey::normalize_sql(sql1);
    std::string normalized2 = PreparedStatementKey::normalize_sql(sql2);

    EXPECT_EQ(normalized1, normalized2);
    EXPECT_TRUE(normalized1.find("SELECT") != std::string::npos);
}

TEST_F(PreparedStatementCacheTest, PreparedStatementStats)
{
    PreparedStatementStats stats;

    // Test initial state
    EXPECT_EQ(stats.execution_count.load(), 0);
    EXPECT_EQ(stats.total_execution_time_us.load(), 0);
    EXPECT_EQ(stats.success_ratio.load(), 1.0);

    // Test execution statistics update
    stats.update_execution_stats(1000, 100, true);
    EXPECT_EQ(stats.execution_count.load(), 1);
    EXPECT_EQ(stats.total_execution_time_us.load(), 1000);
    EXPECT_EQ(stats.total_rows_processed.load(), 100);
    EXPECT_EQ(stats.success_ratio.load(), 1.0);

    // Test preparation statistics
    stats.update_preparation_stats(500);
    EXPECT_EQ(stats.total_preparation_time_us.load(), 500);

    // Test cache hit recording
    stats.record_cache_hit();
    EXPECT_EQ(stats.cache_hit_count.load(), 1);

    // Test reset
    stats.reset();
    EXPECT_EQ(stats.execution_count.load(), 0);
    EXPECT_EQ(stats.cache_hit_count.load(), 0);
}

TEST_F(PreparedStatementCacheTest, CachedPreparedStatementCreation)
{
    PreparedStatementKey key("SELECT * FROM users WHERE id = ?", "testdb", "public", "testuser");

    auto cached_stmt = std::make_shared<CachedPreparedStatement>(key, test_stmt1_, metadata1_);

    EXPECT_EQ(cached_stmt->get_key(), key);
    EXPECT_EQ(cached_stmt->get_statement(), test_stmt1_);
    EXPECT_EQ(cached_stmt->get_metadata().statement_type, "SELECT");
    EXPECT_TRUE(cached_stmt->is_valid());
    EXPECT_GT(cached_stmt->get_memory_footprint(), 0);

    // Test access tracking
    auto initial_count = cached_stmt->get_access_count();
    cached_stmt->record_access();
    EXPECT_EQ(cached_stmt->get_access_count(), initial_count + 1);
}

TEST_F(PreparedStatementCacheTest, BasicCacheOperations)
{
    PreparedStatementKey key1("SELECT * FROM users WHERE id = ?");
    PreparedStatementKey key2("SELECT name FROM products WHERE price > ?");

    // Test insertion
    EXPECT_TRUE(stmt_cache_->insert_statement(key1, test_stmt1_, metadata1_));
    EXPECT_TRUE(stmt_cache_->insert_statement(key2, test_stmt2_, metadata2_));

    // Test lookup
    auto cached_stmt1 = stmt_cache_->lookup_statement(key1);
    ASSERT_NE(cached_stmt1, nullptr);
    EXPECT_EQ(cached_stmt1->get_statement(), test_stmt1_);

    auto cached_stmt2 = stmt_cache_->lookup_statement(key2);
    ASSERT_NE(cached_stmt2, nullptr);
    EXPECT_EQ(cached_stmt2->get_statement(), test_stmt2_);

    // Test cache miss
    PreparedStatementKey key3("SELECT * FROM unknown_table");
    auto cached_stmt3 = stmt_cache_->lookup_statement(key3);
    EXPECT_EQ(cached_stmt3, nullptr);
}

TEST_F(PreparedStatementCacheTest, CacheStatistics)
{
    PreparedStatementKey key1("SELECT * FROM users WHERE id = ?");
    PreparedStatementKey key2("SELECT name FROM products WHERE price > ?");
    PreparedStatementKey key3("SELECT * FROM unknown_table");

    // Insert statements
    stmt_cache_->insert_statement(key1, test_stmt1_, metadata1_);
    stmt_cache_->insert_statement(key2, test_stmt2_, metadata2_);

    // Generate hits and misses
    stmt_cache_->lookup_statement(key1); // Hit
    stmt_cache_->lookup_statement(key2); // Hit
    stmt_cache_->lookup_statement(key3); // Miss

    auto stats = stmt_cache_->get_statistics();
    EXPECT_EQ(stats.cache_hits.load(), 2);
    EXPECT_EQ(stats.cache_misses.load(), 1);
    EXPECT_EQ(stats.total_statements.load(), 2);
    EXPECT_EQ(stats.active_statements.load(), 2);
    EXPECT_DOUBLE_EQ(stats.get_hit_ratio(), 2.0 / 3.0);
}

TEST_F(PreparedStatementCacheTest, CacheEviction)
{
    // Fill cache to capacity
    std::vector<PreparedStatementKey> keys;
    std::vector<std::shared_ptr<MockPreparedStatement>> statements;
    std::vector<StatementMetadata> metadatas;

    for (int i = 0; i < 15; ++i) { // More than max_statements (10)
        std::string sql = "SELECT * FROM table" + std::to_string(i) + " WHERE id = ?";
        keys.emplace_back(sql);
        statements.push_back(std::make_shared<MockPreparedStatement>(sql));

        StatementMetadata metadata;
        metadata.statement_type = "SELECT";
        metadata.read_only = true;
        metadatas.push_back(metadata);

        stmt_cache_->insert_statement(keys[i], statements[i], metadatas[i]);
    }

    auto stats = stmt_cache_->get_statistics();
    EXPECT_LE(stats.active_statements.load(), config_.max_statements);
    EXPECT_GT(stats.cache_evictions.load(), 0);
}

TEST_F(PreparedStatementCacheTest, StatementInvalidation)
{
    PreparedStatementKey key1("SELECT * FROM users WHERE id = ?");
    PreparedStatementKey key2("SELECT * FROM users WHERE name = ?");
    PreparedStatementKey key3("SELECT * FROM products WHERE price > ?");

    // Insert statements
    stmt_cache_->insert_statement(key1, test_stmt1_, metadata1_);
    stmt_cache_->insert_statement(key2, test_stmt2_, metadata2_);
    stmt_cache_->insert_statement(key3, test_stmt3_, metadata3_);

    // Invalidate statements matching pattern
    auto invalidated_count = stmt_cache_->invalidate_statements("users");
    EXPECT_EQ(invalidated_count, 2);

    // Check that invalidated statements are no longer valid
    auto cached_stmt1 = stmt_cache_->lookup_statement(key1);
    auto cached_stmt2 = stmt_cache_->lookup_statement(key2);
    auto cached_stmt3 = stmt_cache_->lookup_statement(key3);

    // Statements should still be in cache but marked invalid
    ASSERT_NE(cached_stmt1, nullptr);
    ASSERT_NE(cached_stmt2, nullptr);
    ASSERT_NE(cached_stmt3, nullptr);

    EXPECT_FALSE(cached_stmt1->is_valid());
    EXPECT_FALSE(cached_stmt2->is_valid());
    EXPECT_TRUE(cached_stmt3->is_valid());
}

TEST_F(PreparedStatementCacheTest, ConfigurationManagement)
{
    auto original_config = stmt_cache_->get_config();
    EXPECT_EQ(original_config.max_statements, 10);

    // Update configuration
    PreparedStatementCacheConfig new_config = original_config;
    new_config.max_statements = 20;
    new_config.max_memory_bytes = 2 * 1024 * 1024; // 2 MB

    EXPECT_TRUE(stmt_cache_->update_config(new_config));

    auto updated_config = stmt_cache_->get_config();
    EXPECT_EQ(updated_config.max_statements, 20);
    EXPECT_EQ(updated_config.max_memory_bytes, 2 * 1024 * 1024);
}

TEST_F(PreparedStatementCacheTest, MemoryManagement)
{
    PreparedStatementKey key("SELECT * FROM users WHERE id = ?");

    auto initial_memory = stmt_cache_->get_memory_usage();

    // Insert statement
    stmt_cache_->insert_statement(key, test_stmt1_, metadata1_);

    auto after_insert_memory = stmt_cache_->get_memory_usage();
    EXPECT_GT(after_insert_memory, initial_memory);

    // Test capacity utilization
    auto utilization = stmt_cache_->get_capacity_utilization();
    EXPECT_GT(utilization, 0.0);
    EXPECT_LT(utilization, 1.0);

    // Remove statement
    stmt_cache_->remove_statement(key);

    auto after_remove_memory = stmt_cache_->get_memory_usage();
    EXPECT_LE(after_remove_memory, after_insert_memory);
}

TEST_F(PreparedStatementCacheTest, CacheIntrospection)
{
    PreparedStatementKey key1("SELECT * FROM users WHERE id = ?");
    PreparedStatementKey key2("SELECT name FROM products WHERE price > ?");

    // Insert statements
    stmt_cache_->insert_statement(key1, test_stmt1_, metadata1_);
    stmt_cache_->insert_statement(key2, test_stmt2_, metadata2_);

    // Get all keys
    auto all_keys = stmt_cache_->get_all_statement_keys();
    EXPECT_EQ(all_keys.size(), 2);

    // Test contains both keys (order may vary)
    bool found_key1 = false, found_key2 = false;
    for (const auto& key : all_keys) {
        if (key == key1)
            found_key1 = true;
        if (key == key2)
            found_key2 = true;
    }
    EXPECT_TRUE(found_key1);
    EXPECT_TRUE(found_key2);

    // Get statement details
    auto stmt_details = stmt_cache_->get_statement_details(key1);
    ASSERT_NE(stmt_details, nullptr);
    EXPECT_EQ(stmt_details->get_key(), key1);
}

TEST_F(PreparedStatementCacheTest, PerformanceReport)
{
    PreparedStatementKey key("SELECT * FROM users WHERE id = ?");
    stmt_cache_->insert_statement(key, test_stmt1_, metadata1_);

    auto report = stmt_cache_->generate_performance_report();
    EXPECT_FALSE(report.empty());
    EXPECT_TRUE(report.find("Prepared Statement Cache Performance Report") != std::string::npos);
    EXPECT_TRUE(report.find("Cache Hits:") != std::string::npos);
    EXPECT_TRUE(report.find("Hit Ratio:") != std::string::npos);
}

TEST_F(PreparedStatementCacheTest, StatisticsExport)
{
    PreparedStatementKey key("SELECT * FROM users WHERE id = ?");
    stmt_cache_->insert_statement(key, test_stmt1_, metadata1_);

    std::string export_file = "/tmp/test_stmt_cache_stats.txt";
    EXPECT_TRUE(stmt_cache_->export_statistics(export_file));

    // Verify file exists and has content
    std::ifstream file(export_file);
    EXPECT_TRUE(file.is_open());

    std::string content;
    std::getline(file, content);
    EXPECT_FALSE(content.empty());

    file.close();
    std::remove(export_file.c_str());
}

TEST_F(PreparedStatementCacheTest, ConcurrentAccess)
{
    const int num_threads = 4;
    const int statements_per_thread = 5;

    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, t, statements_per_thread, &success_count]() {
            for (int i = 0; i < statements_per_thread; ++i) {
                std::string sql = "SELECT * FROM table" + std::to_string(t) + "_" +
                                  std::to_string(i) + " WHERE id = ?";
                PreparedStatementKey key(sql);
                auto stmt = std::make_shared<MockPreparedStatement>(sql);
                StatementMetadata metadata;
                metadata.statement_type = "SELECT";

                if (stmt_cache_->insert_statement(key, stmt, metadata)) {
                    auto cached = stmt_cache_->lookup_statement(key);
                    if (cached != nullptr) {
                        success_count.fetch_add(1);
                    }
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_GT(success_count.load(), 0);
    auto stats = stmt_cache_->get_statistics();
    EXPECT_GT(stats.total_statements.load(), 0);
}

TEST_F(PreparedStatementCacheTest, CacheConfigValidation)
{
    PreparedStatementCacheConfig valid_config;
    EXPECT_TRUE(valid_config.is_valid());
    EXPECT_TRUE(valid_config.validate().empty());

    PreparedStatementCacheConfig invalid_config;
    invalid_config.max_statements = 0; // Invalid
    EXPECT_FALSE(invalid_config.is_valid());
    EXPECT_FALSE(invalid_config.validate().empty());
}
