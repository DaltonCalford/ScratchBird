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

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/optimizer/plan_payload.h"
#include "scratchbird/optimizer/query_planner.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/query_compiler_v3.h"
#include "scratchbird/sblr/v3_container.h"
#include "scratchbird/sblr/v3_codec.h"
#include "scratchbird/sblr/v3_opcode_registry.h"
#include "test_helpers.h"

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <algorithm>

using namespace scratchbird;
using namespace scratchbird::core;
using namespace scratchbird::sblr;
namespace sblr_v3 = scratchbird::sblr::v3;
using scratchbird::testing::TestDatabaseFile;

namespace
{
    auto isZeroId(const ID& id) -> bool
    {
        return std::all_of(id.bytes.begin(), id.bytes.end(),
                           [](uint8_t byte) { return byte == 0; });
    }
}

/**
 * Integration test for Query Planner (Phase 1, Task 1.3)
 *
 * Verifies that:
 * 1. Query planner is properly integrated with QueryCompilerV3
 * 2. Optimizer components are initialized
 * 3. Bytecode structure is preserved
 */
class QueryPlannerIntegrationTest : public ::testing::Test
{
protected:
    void TearDown() override
    {
        executor_.reset();
        compiler_.reset();
        ConnectionContext::setCurrent(nullptr);
        connection_ctx_.reset();
        db_.reset();
        db_file_.reset();
    }

    bool createDatabase()
    {
        db_file_ = std::make_unique<TestDatabaseFile>("test_query_planner");

        ErrorContext ctx;
        Status status = Database::create(db_file_->path(), 16384, &ctx);
        if (status != Status::OK)
        {
            return false;
        }

        db_ = std::make_unique<Database>();
        status = db_->open(db_file_->path(), &ctx);
        if (status != Status::OK)
        {
            return false;
        }

        auto* catalog = db_->catalog_manager();
        if (catalog == nullptr)
        {
            return false;
        }

        compiler_ = std::make_unique<QueryCompilerV3>(db_.get());
        executor_ = std::make_unique<Executor>(db_.get());

        CatalogManager::SchemaInfo public_schema_info;
        status = catalog->getSchema("public", public_schema_info, &ctx);
        if (status != Status::OK)
        {
            return false;
        }

        status = db_->connect(connection_ctx_, &ctx);
        if (status != Status::OK)
        {
            return false;
        }
        connection_ctx_->setCurrentSchemaId(public_schema_info.schema_id);
        const auto system_user_id = catalog->getSystemUserId(&ctx);
        if (isZeroId(system_user_id))
        {
            return false;
        }
        connection_ctx_->setCurrentUser(system_user_id, true);
        ConnectionContext::setCurrent(connection_ctx_.get());
        executor_->setConnectionContext(connection_ctx_.get());

        const std::vector<std::string> ddl = {
            "CREATE TABLE users (id INTEGER, name VARCHAR(100), email VARCHAR(100), age INTEGER)",
            "CREATE TABLE products (id INTEGER, name VARCHAR(100), price DOUBLE)",
            "CREATE TABLE test (id INTEGER)",
            "GRANT SELECT ON users TO PUBLIC",
            "GRANT SELECT ON products TO PUBLIC",
            "GRANT SELECT ON test TO PUBLIC"
        };

        for (const auto& sql : ddl)
        {
            auto compile_result = compiler_->compile(sql);
            if (!compile_result.success())
            {
                return false;
            }
            auto exec_result = executor_->execute(compile_result.bytecode());
            if (!exec_result.success())
            {
                return false;
            }
        }

        return true;
    }

    std::vector<uint8_t> compileSQL(const std::string &sql)
    {
        auto compile_result = compiler_->compile(sql);
        if (!compile_result.success())
        {
            last_compile_errors_.clear();
            for (const auto& err : compile_result.errors())
            {
                if (!last_compile_errors_.empty())
                {
                    last_compile_errors_ += "\n";
                }
                last_compile_errors_ += err;
            }
            return {};
        }
        last_compile_errors_.clear();
        return compile_result.bytecode();
    }

    ExecutionResult executeSQL(const std::string &sql)
    {
        auto bytecode = compileSQL(sql);
        if (bytecode.empty())
        {
            return ExecutionResult("Compilation failed: " + last_compile_errors_);
        }
        return executor_->execute(bytecode);
    }

    bool containsOpcode(const std::vector<uint8_t> &bytecode, sblr_v3::Opcode opcode)
    {
        const auto target = static_cast<uint16_t>(opcode);
        std::function<bool(const sblr_v3::Value&)> valueContainsOpcode;
        std::function<bool(const sblr_v3::Instruction&)> instructionContainsOpcode;

        valueContainsOpcode = [&](const sblr_v3::Value& value) -> bool {
            if (const auto* stmt = std::get_if<sblr_v3::Value::InstrPtr>(&value.data)) {
                return *stmt && instructionContainsOpcode(**stmt);
            }
            if (const auto* list = std::get_if<sblr_v3::Value::List>(&value.data)) {
                for (const auto& element : *list) {
                    if (valueContainsOpcode(element)) {
                        return true;
                    }
                }
                return false;
            }
            if (const auto* obj = std::get_if<sblr_v3::Value::Object>(&value.data)) {
                for (const auto& [_, element] : *obj) {
                    if (valueContainsOpcode(element)) {
                        return true;
                    }
                }
                return false;
            }
            return false;
        };

        instructionContainsOpcode = [&](const sblr_v3::Instruction& inst) -> bool {
            if (inst.opcode == target) {
                return true;
            }
            return valueContainsOpcode(inst.payload);
        };

        sblr_v3::Container container;
        std::string err;
        if (!sblr_v3::decodeContainer(bytecode.data(), bytecode.size(), container, err))
        {
            return false;
        }
        size_t offset = 0;
        sblr_v3::DecodeError decode_err;
        while (offset < container.bytecode_stream.size())
        {
            sblr_v3::Instruction inst;
            if (!sblr_v3::decodeInstructionWithSchema(container.bytecode_stream.data(),
                                                      container.bytecode_stream.size(),
                                                      offset,
                                                      inst,
                                                      decode_err) &&
                !sblr_v3::decodeInstruction(container.bytecode_stream.data(),
                                            container.bytecode_stream.size(),
                                            offset,
                                            inst,
                                            decode_err))
            {
                break;
            }
            if (instructionContainsOpcode(inst))
            {
                return true;
            }
        }
        return false;
    }

    bool decodeFirstSelect(const std::vector<uint8_t>& bytecode, sblr_v3::Instruction& out)
    {
        sblr_v3::Container container;
        std::string err;
        if (!sblr_v3::decodeContainer(bytecode.data(), bytecode.size(), container, err))
        {
            return false;
        }
        size_t offset = 0;
        sblr_v3::DecodeError decode_err;
        while (offset < container.bytecode_stream.size())
        {
            sblr_v3::Instruction inst;
            if (!sblr_v3::decodeInstructionWithSchema(container.bytecode_stream.data(),
                                                      container.bytecode_stream.size(),
                                                      offset,
                                                      inst,
                                                      decode_err))
            {
                return false;
            }
            if (static_cast<sblr_v3::Opcode>(inst.opcode) == sblr_v3::Opcode::SBLR3_SELECT)
            {
                out = std::move(inst);
                return true;
            }
            if (static_cast<sblr_v3::Opcode>(inst.opcode) == sblr_v3::Opcode::SBLR3_EXPLAIN_PLAN)
            {
                const auto* obj = std::get_if<sblr_v3::Value::Object>(&inst.payload.data);
                if (!obj)
                {
                    return false;
                }
                auto it_query = obj->find("query");
                if (it_query == obj->end())
                {
                    return false;
                }
                const auto* ptr = std::get_if<sblr_v3::Value::InstrPtr>(&it_query->second.data);
                if (ptr && *ptr &&
                    static_cast<sblr_v3::Opcode>((*ptr)->opcode) == sblr_v3::Opcode::SBLR3_SELECT)
                {
                    out = **ptr;
                    return true;
                }
            }
        }
        return false;
    }

    bool decodeRuntimePlan(const std::vector<uint8_t>& bytecode,
                           scratchbird::optimizer::RuntimePlan& plan_out)
    {
        sblr_v3::Instruction select_inst;
        if (!decodeFirstSelect(bytecode, select_inst))
        {
            return false;
        }
        const auto* obj = std::get_if<sblr_v3::Value::Object>(&select_inst.payload.data);
        if (!obj)
        {
            return false;
        }
        auto it_plan = obj->find("plan");
        if (it_plan == obj->end())
        {
            return false;
        }
        const auto* bytes = std::get_if<sblr_v3::Value::Bytes>(&it_plan->second.data);
        if (!bytes)
        {
            return false;
        }
        std::string err;
        return scratchbird::optimizer::decodeRuntimePlan(*bytes, plan_out, err);
    }

    std::vector<std::string> resultStrings(const ExecutionResult& result)
    {
        std::vector<std::string> lines;
        if (!result.success() || !result.hasResultSet() || result.resultSet() == nullptr)
        {
            return lines;
        }

        auto* rs = result.resultSet();
        for (size_t row = 0; row < rs->rowCount(); ++row)
        {
            lines.push_back(rs->getValue(row, 0).toString());
        }
        return lines;
    }

    std::unique_ptr<TestDatabaseFile> db_file_;
    std::unique_ptr<Database> db_;
    std::unique_ptr<QueryCompilerV3> compiler_;
    std::unique_ptr<Executor> executor_;
    std::unique_ptr<ConnectionContext> connection_ctx_;
    std::string last_compile_errors_;
};

// ===== Basic Integration Tests =====

TEST_F(QueryPlannerIntegrationTest, QueryCompilerV3ProducesBytecode)
{
    ASSERT_TRUE(createDatabase());

    auto bytecode = compileSQL("SELECT * FROM users");
    EXPECT_FALSE(bytecode.empty()) << last_compile_errors_;
    EXPECT_TRUE(containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_SELECT));
    EXPECT_TRUE(containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_SELECT_STAR) ||
                containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_SELECT_TABLE_STAR) ||
                containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_COLUMN_REF));
}

TEST_F(QueryPlannerIntegrationTest, DatabaseHasQueryPlannerComponents)
{
    ASSERT_TRUE(createDatabase());

    EXPECT_NE(db_->statistics_manager(), nullptr);
    scratchbird::optimizer::QueryPlanner planner(
        db_.get(),
        scratchbird::optimizer::CostModel(),
        db_->statistics_manager());
    (void)planner;
}

TEST_F(QueryPlannerIntegrationTest, CompilerRequiresCatalog)
{
    QueryCompilerV3 compiler(nullptr);
    auto result = compiler.compile("SELECT 1");
    EXPECT_FALSE(result.success());
}

// ===== Optimizer Integration Tests =====

TEST_F(QueryPlannerIntegrationTest, SelectGeneratesWithPlanner)
{
    ASSERT_TRUE(createDatabase());

    auto select_bytecode = compileSQL("SELECT * FROM users");
    EXPECT_FALSE(select_bytecode.empty()) << last_compile_errors_;
    EXPECT_TRUE(containsOpcode(select_bytecode, sblr_v3::Opcode::SBLR3_SELECT));
}

TEST_F(QueryPlannerIntegrationTest, OptimizedSelectEmbedsRuntimePlanPayload)
{
    ASSERT_TRUE(createDatabase());

    auto bytecode = compileSQL("SELECT id, name FROM users WHERE id > 10");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    EXPECT_FALSE(plan.plan_hash.empty());
    EXPECT_FALSE(plan.explain_text.empty());
    ASSERT_EQ(plan.relations.size(), 1u);
    EXPECT_EQ(plan.relations.front().table_path, "users");
    EXPECT_FALSE(plan.relations.front().scan_kind.empty());
    EXPECT_FALSE(plan.root.node_type.empty());
}

TEST_F(QueryPlannerIntegrationTest, DisabledOptimizationsDoNotEmbedRuntimePlanPayload)
{
    ASSERT_TRUE(createDatabase());

    compiler_->setOptimizationsEnabled(false);
    auto bytecode = compileSQL("SELECT id FROM users WHERE id = 42");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    EXPECT_FALSE(decodeRuntimePlan(bytecode, plan));
}

TEST_F(QueryPlannerIntegrationTest, RepeatedSelectHitsPlanCache)
{
    ASSERT_TRUE(createDatabase());

    QueryCompilerV3::resetPlanCacheStats();

    auto first = compileSQL("SELECT id FROM users WHERE id = 42");
    ASSERT_FALSE(first.empty()) << last_compile_errors_;
    auto after_first = QueryCompilerV3::planCacheStats();
    EXPECT_EQ(after_first.hits, 0u);
    EXPECT_EQ(after_first.misses, 1u);
    EXPECT_EQ(after_first.inserts, 1u);
    EXPECT_EQ(after_first.entries, 1u);

    auto second = compileSQL("SELECT id FROM users WHERE id = 42");
    ASSERT_FALSE(second.empty()) << last_compile_errors_;
    auto after_second = QueryCompilerV3::planCacheStats();
    EXPECT_EQ(after_second.hits, 1u);
    EXPECT_EQ(after_second.misses, 1u);
    EXPECT_EQ(after_second.inserts, 1u);
    EXPECT_EQ(after_second.entries, 1u);
    EXPECT_EQ(first, second);
}

TEST_F(QueryPlannerIntegrationTest, MutationInvalidatesCachedPlans)
{
    ASSERT_TRUE(createDatabase());

    QueryCompilerV3::resetPlanCacheStats();

    auto first = compileSQL("SELECT id FROM users WHERE id = 42");
    ASSERT_FALSE(first.empty()) << last_compile_errors_;
    auto second = compileSQL("SELECT id FROM users WHERE id = 42");
    ASSERT_FALSE(second.empty()) << last_compile_errors_;
    auto mutation = compileSQL("INSERT INTO users (id, name) VALUES (1, 'alice')");
    ASSERT_FALSE(mutation.empty()) << last_compile_errors_;

    auto after_mutation = QueryCompilerV3::planCacheStats();
    EXPECT_EQ(after_mutation.hits, 1u);
    EXPECT_EQ(after_mutation.misses, 1u);
    EXPECT_EQ(after_mutation.inserts, 1u);
    EXPECT_EQ(after_mutation.invalidations, 1u);
    EXPECT_EQ(after_mutation.entries, 0u);
}

TEST_F(QueryPlannerIntegrationTest, EqualityJoinChoosesHashJoinPlan)
{
    ASSERT_TRUE(createDatabase());

    auto bytecode =
        compileSQL("SELECT users.id FROM users JOIN products ON users.id = products.id");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.join_steps.size(), 1u);
    EXPECT_EQ(plan.join_steps.front().join_type, "INNER");
    EXPECT_EQ(plan.join_steps.front().method, "HASH_JOIN");
    EXPECT_TRUE(plan.join_steps.front().has_hash_keys);
    EXPECT_EQ(plan.join_steps.front().left_hash_key.column_name, "id");
    EXPECT_EQ(plan.join_steps.front().right_hash_key.column_name, "id");
}

TEST_F(QueryPlannerIntegrationTest, FullOuterJoinFailsClosedToNestedLoop)
{
    ASSERT_TRUE(createDatabase());

    auto bytecode =
        compileSQL("SELECT users.id FROM users FULL JOIN products ON users.id = products.id");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.join_steps.size(), 1u);
    EXPECT_EQ(plan.join_steps.front().join_type, "FULL");
    EXPECT_EQ(plan.join_steps.front().method, "NESTED_LOOP");
    EXPECT_FALSE(plan.join_steps.front().has_hash_keys);
}

TEST_F(QueryPlannerIntegrationTest, HashJoinPlanExecutesAndReturnsExpectedRows)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (1, 'alice', 'a@example.com', 30)")
                    .success());
    ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (2, 'bob', 'b@example.com', 32)")
                    .success());
    ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (3, 'carol', 'c@example.com', 34)")
                    .success());
    ASSERT_TRUE(executeSQL("INSERT INTO products (id, name, price) VALUES (1, 'p1', 10.5)").success());
    ASSERT_TRUE(executeSQL("INSERT INTO products (id, name, price) VALUES (2, 'p2', 20.5)").success());
    ASSERT_TRUE(executeSQL("INSERT INTO products (id, name, price) VALUES (4, 'p4', 40.5)").success());

    auto bytecode =
        compileSQL("SELECT users.id FROM users JOIN products ON users.id = products.id");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.join_steps.size(), 1u);
    EXPECT_EQ(plan.join_steps.front().method, "HASH_JOIN");

    auto result =
        executeSQL("SELECT users.id FROM users JOIN products ON users.id = products.id");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto rows = resultStrings(result);
    ASSERT_EQ(rows.size(), 2u);
    std::sort(rows.begin(), rows.end());
    EXPECT_EQ(rows[0], "1");
    EXPECT_EQ(rows[1], "2");
}

TEST_F(QueryPlannerIntegrationTest, ExplainJsonFormatsRuntimePlan)
{
    ASSERT_TRUE(createDatabase());

    auto result = executeSQL("EXPLAIN (FORMAT JSON, ANALYZE, VERBOSE) "
                             "SELECT id FROM users WHERE id = 42");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    const auto lines = resultStrings(result);
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_NE(lines.front().find("\"plan_hash\":\""), std::string::npos);
    EXPECT_NE(lines.front().find("\"options\":["), std::string::npos);
    EXPECT_NE(lines.front().find("\"VERBOSE\""), std::string::npos);
    EXPECT_NE(lines.front().find("\"plan\":"), std::string::npos);
    EXPECT_NE(lines.front().find("\"plan_root\":"), std::string::npos);
    EXPECT_NE(lines.front().find("\"node_type\":\""), std::string::npos);
    EXPECT_NE(lines.front().find("\"analyze\":{\"rows\":"), std::string::npos);
}

TEST_F(QueryPlannerIntegrationTest, ReorderedJoinPayloadKeepsOriginalRelationIndexes)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL("CREATE INDEX idx_users_id ON users (id)").success());
    ASSERT_TRUE(executeSQL("CREATE INDEX idx_products_id ON products (id)").success());
    for (int i = 1; i <= 256; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" + std::to_string(i) +
                               "', 'u" + std::to_string(i) +
                               "@example.com', " + std::to_string(20 + (i % 30)) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("INSERT INTO products (id, name, price) VALUES (42, 'widget', 12.5)")
                    .success());
    ASSERT_TRUE(executeSQL("ANALYZE users").success());
    ASSERT_TRUE(executeSQL("ANALYZE products").success());

    const std::string sql =
        "SELECT users.id FROM users JOIN products ON users.id = products.id "
        "WHERE products.id = 42";
    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 2u);
    ASSERT_EQ(plan.join_steps.size(), 1u);
    EXPECT_EQ(plan.relations.front().source_relation_index, 1u);

    sblr_v3::Instruction select_inst;
    ASSERT_TRUE(decodeFirstSelect(bytecode, select_inst));
    const auto* select_payload = std::get_if<sblr_v3::Value::Object>(&select_inst.payload.data);
    ASSERT_NE(select_payload, nullptr);
    auto from_it = select_payload->find("from");
    ASSERT_NE(from_it, select_payload->end());
    const auto* from_obj = std::get_if<sblr_v3::Value::Object>(&from_it->second.data);
    ASSERT_NE(from_obj, nullptr);
    auto relation_index_it = from_obj->find("source_relation_index");
    ASSERT_NE(relation_index_it, from_obj->end());
    const auto* relation_index = std::get_if<uint64_t>(&relation_index_it->second.data);
    ASSERT_NE(relation_index, nullptr);
    EXPECT_EQ(*relation_index, 1u);

    auto result = executeSQL(sql);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    const auto rows = resultStrings(result);
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows.front(), "42");
}

TEST_F(QueryPlannerIntegrationTest, CoveringIndexPlanUsesIndexOnlyScan)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL("CREATE INDEX idx_users_id_name ON users (id, name)").success());
    for (int i = 1; i <= 1200; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" + std::to_string(i) +
                               "', 'user" + std::to_string(i) +
                               "@example.com', " + std::to_string(20 + (i % 40)) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    auto bytecode = compileSQL("SELECT id, name FROM users WHERE id = 777");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 1u);
    EXPECT_EQ(plan.relations.front().scan_kind, "INDEX_ONLY_SCAN");
    EXPECT_TRUE(plan.relations.front().covering_index);
    EXPECT_TRUE(plan.relations.front().exact_key_lookup);
    EXPECT_EQ(plan.relations.front().index_name, "idx_users_id_name");

    auto result = executeSQL("SELECT id, name FROM users WHERE id = 777");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 1u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "777");
    EXPECT_EQ(result.resultSet()->getValue(0, 1).toString(), "user777");
}

TEST_F(QueryPlannerIntegrationTest, BitmapIndexPlanExecutesExactProbes)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL(
                    "CREATE TABLE search_users (id INTEGER, age INTEGER, city VARCHAR(64), cohort VARCHAR(32))")
                    .success());
    ASSERT_TRUE(executeSQL("GRANT SELECT ON search_users TO PUBLIC").success());
    ASSERT_TRUE(executeSQL("CREATE INDEX idx_search_users_age ON search_users (age)").success());
    ASSERT_TRUE(executeSQL("CREATE INDEX idx_search_users_city ON search_users (city)").success());
    for (int i = 1; i <= 1600; ++i)
    {
        const int age = 20 + (i % 8);
        const std::string city = (i % 4 == 0) ? "Seattle" :
                                 (i % 4 == 1) ? "Austin" :
                                 (i % 4 == 2) ? "Boston" : "Denver";
        ASSERT_TRUE(executeSQL("INSERT INTO search_users (id, age, city, cohort) VALUES (" +
                               std::to_string(i) + ", " + std::to_string(age) + ", '" +
                               city + "', 'c" + std::to_string(i % 5) + "')")
                        .success());
    }
    ASSERT_TRUE(executeSQL(
                    "INSERT INTO search_users (id, age, city, cohort) VALUES (9001, 30, 'Seattle', 'target')")
                    .success());
    ASSERT_TRUE(executeSQL("ANALYZE search_users").success());

    auto bytecode =
        compileSQL("SELECT id FROM search_users WHERE age = 30 AND city = 'Seattle'");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 1u);
    EXPECT_EQ(plan.relations.front().scan_kind, "BITMAP_INDEX_SCAN");
    EXPECT_EQ(plan.relations.front().bitmap_op, "AND");
    EXPECT_TRUE(plan.relations.front().exact_key_lookup);
    ASSERT_GE(plan.relations.front().index_predicates.size(), 2u);

    auto result =
        executeSQL("SELECT id FROM search_users WHERE age = 30 AND city = 'Seattle'");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto rows = resultStrings(result);
    ASSERT_FALSE(rows.empty());
    EXPECT_NE(std::find(rows.begin(), rows.end(), "9001"), rows.end());
}

TEST_F(QueryPlannerIntegrationTest, PassThroughViewIsFlattenedInRuntimePlan)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL("CREATE VIEW v_users_flat AS SELECT * FROM users").success());
    ASSERT_TRUE(executeSQL(
                    "INSERT INTO users (id, name, email, age) VALUES (42, 'flattened', 'f@example.com', 25)")
                    .success());
    ASSERT_TRUE(executeSQL("CREATE INDEX idx_users_id ON users (id)").success());
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    auto bytecode = compileSQL("SELECT id FROM v_users_flat WHERE id = 42");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 1u);
    EXPECT_TRUE(plan.relations.front().flattened_derived);
    EXPECT_FALSE(plan.relations.front().physical_table_path.empty());
    EXPECT_EQ(plan.relations.front().physical_table_path, "users");

    auto result = executeSQL("SELECT id FROM v_users_flat WHERE id = 42");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 1u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "42");
}

TEST_F(QueryPlannerIntegrationTest, RuntimePlanCapturesAggregateWindowSortAndLimitNodes)
{
    ASSERT_TRUE(createDatabase());

    for (int i = 1; i <= 128; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'window" + std::to_string(i) +
                               "', 'window" + std::to_string(i) +
                               "@example.com', " + std::to_string(18 + (i % 10)) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    auto aggregate_bytecode = compileSQL("SELECT COUNT(*) FROM users");
    ASSERT_FALSE(aggregate_bytecode.empty()) << last_compile_errors_;
    scratchbird::optimizer::RuntimePlan aggregate_plan;
    ASSERT_TRUE(decodeRuntimePlan(aggregate_bytecode, aggregate_plan));
    EXPECT_EQ(aggregate_plan.root.node_type, "Aggregate");

    auto window_bytecode =
        compileSQL("SELECT ROW_NUMBER() OVER (ORDER BY id) FROM users");
    ASSERT_FALSE(window_bytecode.empty()) << last_compile_errors_;
    scratchbird::optimizer::RuntimePlan window_plan;
    ASSERT_TRUE(decodeRuntimePlan(window_bytecode, window_plan));
    EXPECT_EQ(window_plan.root.node_type, "Window");

    auto ordered_bytecode =
        compileSQL("SELECT id FROM users ORDER BY id DESC LIMIT 5");
    ASSERT_FALSE(ordered_bytecode.empty()) << last_compile_errors_;
    scratchbird::optimizer::RuntimePlan ordered_plan;
    ASSERT_TRUE(decodeRuntimePlan(ordered_bytecode, ordered_plan));
    EXPECT_EQ(ordered_plan.root.node_type, "Limit");
    ASSERT_EQ(ordered_plan.root.children.size(), 1u);
    EXPECT_EQ(ordered_plan.root.children.front().node_type, "Sort");
}

TEST_F(QueryPlannerIntegrationTest, BytecodeContainsVersionHeader)
{
    ASSERT_TRUE(createDatabase());

    auto bytecode = compileSQL("SELECT * FROM test");
    ASSERT_FALSE(bytecode.empty());
    sblr_v3::Container container;
    std::string err;
    ASSERT_TRUE(sblr_v3::decodeContainer(bytecode.data(), bytecode.size(), container, err))
        << err;
    EXPECT_EQ(std::string(container.header.magic, 4), std::string("SBL3"));
}

TEST_F(QueryPlannerIntegrationTest, SelectWithWhereClause)
{
    ASSERT_TRUE(createDatabase());

    auto bytecode = compileSQL("SELECT id, name FROM users WHERE id > 10");
    EXPECT_FALSE(bytecode.empty()) << last_compile_errors_;
    EXPECT_TRUE(containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_WHERE_CLAUSE));
    EXPECT_TRUE(containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_EXPR_GT));
}

TEST_F(QueryPlannerIntegrationTest, NonSelectStatementsBypassPlanner)
{
    ASSERT_TRUE(createDatabase());

    auto insert_bytecode = compileSQL("INSERT INTO users (id) VALUES (1)");
    EXPECT_FALSE(insert_bytecode.empty()) << last_compile_errors_;
    EXPECT_TRUE(containsOpcode(insert_bytecode, sblr_v3::Opcode::SBLR3_INSERT));

    auto create_bytecode = compileSQL("CREATE TABLE test2 (id INTEGER)");
    EXPECT_FALSE(create_bytecode.empty()) << last_compile_errors_;
    EXPECT_TRUE(containsOpcode(create_bytecode, sblr_v3::Opcode::SBLR3_CREATE_TABLE));
}

// ===== Stress Tests =====

TEST_F(QueryPlannerIntegrationTest, ComplexQueryWithPlanner)
{
    ASSERT_TRUE(createDatabase());

    std::string complex_sql =
        "SELECT id, name, price * 1.1 "
        "FROM products "
        "WHERE price >= 100.0 AND name LIKE '%sale%'";

    auto bytecode = compileSQL(complex_sql);
    EXPECT_FALSE(bytecode.empty()) << last_compile_errors_;
    EXPECT_TRUE(containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_SELECT));
    EXPECT_TRUE(containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_WHERE_CLAUSE));
    EXPECT_TRUE(containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_EXPR_AND));
    EXPECT_TRUE(containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_EXPR_GE));
    EXPECT_TRUE(containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_EXPR_LIKE));
    EXPECT_TRUE(containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_EXPR_MULTIPLY));
}

TEST_F(QueryPlannerIntegrationTest, MultipleSelectStatements)
{
    ASSERT_TRUE(createDatabase());

    std::vector<std::string> queries = {
        "SELECT * FROM users",
        "SELECT id FROM users WHERE id > 10",
        "SELECT name, email FROM users",
        "SELECT COUNT(*) FROM users"
    };

    for (const auto &sql : queries)
    {
        auto bytecode = compileSQL(sql);
        EXPECT_FALSE(bytecode.empty()) << "Failed for query: " << sql << "\n"
                                       << last_compile_errors_;
        EXPECT_TRUE(containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_SELECT))
            << "Missing SELECT opcode for query: " << sql;
    }
}

// ===== Edge Cases =====

TEST_F(QueryPlannerIntegrationTest, EmptySelectList)
{
    ASSERT_TRUE(createDatabase());

    auto bytecode = compileSQL("SELECT * FROM test");
    EXPECT_FALSE(bytecode.empty()) << last_compile_errors_;
    EXPECT_TRUE(containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_SELECT_STAR) ||
                containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_SELECT_TABLE_STAR) ||
                containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_COLUMN_REF));
}

TEST_F(QueryPlannerIntegrationTest, SelectWithoutWhereClause)
{
    ASSERT_TRUE(createDatabase());

    auto bytecode = compileSQL("SELECT id, name FROM users");
    EXPECT_FALSE(bytecode.empty()) << last_compile_errors_;
    EXPECT_TRUE(containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_SELECT));
    EXPECT_FALSE(containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_WHERE_CLAUSE));
}

// ===== Diagnostic Test =====

TEST_F(QueryPlannerIntegrationTest, DiagnosticDisassemblyOutput)
{
    ASSERT_TRUE(createDatabase());

    std::string sql = "SELECT id, name FROM users WHERE id = 42";
    auto bytecode = compileSQL(sql);

    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    EXPECT_TRUE(containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_SELECT));
}
