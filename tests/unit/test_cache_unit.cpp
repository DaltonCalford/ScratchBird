/**
 * Cache unit tests (statement/result/translation cache keying, invalidation, eviction).
 */

#include <gtest/gtest.h>
#include "scratchbird/pool/statement_cache.h"
#include "scratchbird/pool/result_cache.h"
#include "scratchbird/protocol/translation_cache.h"

using namespace scratchbird::pool;
using namespace scratchbird::protocol;

TEST(CacheUnitTest, StatementCacheKeyingAndInvalidation) {
    StatementCacheConfig config;
    config.max_statements = 8;
    config.min_statement_size = 1;

    DatabaseStatementCache cache("test_db", config);

    StatementMetadata meta;
    meta.statement_type = StatementType::SELECT;
    meta.referenced_tables = {"t1"};
    meta.parameter_types = {23};
    meta.parameter_count = 1;
    meta.schema_version_id = 42;
    meta.privilege_signature = "user:1";

    auto statement = std::make_shared<CachedStatement>(
        "SELECT * FROM t1 WHERE id = ?", meta);
    ASSERT_TRUE(cache.put(statement));

    auto hit = cache.get("SELECT * FROM t1 WHERE id = ?",
                         meta.parameter_types,
                         meta.schema_version_id,
                         meta.privilege_signature);
    EXPECT_NE(hit, nullptr);

    auto miss_schema = cache.get("SELECT * FROM t1 WHERE id = ?",
                                 meta.parameter_types,
                                 43,
                                 meta.privilege_signature);
    EXPECT_EQ(miss_schema, nullptr);

    auto miss_priv = cache.get("SELECT * FROM t1 WHERE id = ?",
                               meta.parameter_types,
                               meta.schema_version_id,
                               "user:2");
    EXPECT_EQ(miss_priv, nullptr);

    EXPECT_EQ(cache.invalidate_by_table("t1"), 1u);
    auto invalidated = cache.get("SELECT * FROM t1 WHERE id = ?",
                                 meta.parameter_types,
                                 meta.schema_version_id,
                                 meta.privilege_signature);
    ASSERT_NE(invalidated, nullptr);
    EXPECT_EQ(invalidated->state(), CacheEntryState::INVALID);
}

TEST(CacheUnitTest, ResultCacheKeyingInvalidationAndEviction) {
    ResultCacheConfig config;
    config.max_entries = 1;
    config.min_rows_to_cache = 0;
    config.max_rows_to_cache = 100;

    DatabaseResultCache cache("test_db", config);

    ResultMetadata meta;
    meta.sql = "SELECT * FROM t1";
    meta.referenced_tables = {"t1"};
    meta.schema_version_id = 10;
    meta.privilege_signature = "u1";

    auto result = std::make_shared<CachedResult>(meta.sql, meta);
    result->add_row({static_cast<int64_t>(1)});
    ASSERT_TRUE(cache.put(result));

    auto hit = cache.get(meta.sql, {}, meta.schema_version_id, meta.privilege_signature);
    EXPECT_NE(hit, nullptr);

    auto miss_schema = cache.get(meta.sql, {}, 11, meta.privilege_signature);
    EXPECT_EQ(miss_schema, nullptr);

    auto miss_priv = cache.get(meta.sql, {}, meta.schema_version_id, "u2");
    EXPECT_EQ(miss_priv, nullptr);

    ResultMetadata meta2;
    meta2.sql = "SELECT * FROM t2";
    meta2.referenced_tables = {"t2"};
    meta2.schema_version_id = 10;
    meta2.privilege_signature = "u1";

    auto result2 = std::make_shared<CachedResult>(meta2.sql, meta2);
    result2->add_row({static_cast<int64_t>(2)});
    ASSERT_TRUE(cache.put(result2));

    auto evicted = cache.get(meta.sql, {}, meta.schema_version_id, meta.privilege_signature);
    EXPECT_EQ(evicted, nullptr);
    EXPECT_EQ(cache.statistics().eviction_count, 1u);

    EXPECT_EQ(cache.invalidate_by_table("t2"), 1u);
    auto invalidated = cache.get(meta2.sql, {}, meta2.schema_version_id, meta2.privilege_signature);
    EXPECT_EQ(invalidated, nullptr);
}

TEST(CacheUnitTest, TranslationCacheKeyingAndEviction) {
    TranslationCacheConfig config;
    config.max_entries = 1;
    config.max_bytes = 1024 * 1024;
    config.ttl = std::chrono::seconds(300);
    config.enabled = true;

    TranslationCache cache(config);
    cache.resetStats();

    std::vector<uint8_t> bytecode = {1, 2, 3};
    cache.put("postgresql", "SELECT 1", 1, "u1", bytecode);

    std::vector<uint8_t> out;
    EXPECT_TRUE(cache.get("postgresql", "SELECT 1", 1, "u1", out));
    EXPECT_EQ(out, bytecode);

    std::vector<uint8_t> out_miss;
    EXPECT_FALSE(cache.get("postgresql", "SELECT 1", 2, "u1", out_miss));

    cache.put("postgresql", "SELECT 2", 1, "u1", std::vector<uint8_t>{9});
    auto stats = cache.stats();
    EXPECT_EQ(stats.evictions, 1u);
    EXPECT_EQ(stats.current_entries, 1u);
}
