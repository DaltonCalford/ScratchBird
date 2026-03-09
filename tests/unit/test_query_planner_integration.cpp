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
#include "scratchbird/optimizer/query_profiler.h"
#include "scratchbird/optimizer/query_planner.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/query_compiler_v3.h"
#include "scratchbird/sblr/v3_container.h"
#include "scratchbird/sblr/v3_codec.h"
#include "scratchbird/sblr/v3_opcode_registry.h"
#include "test_helpers.h"

#include <memory>
#include <nlohmann/json.hpp>
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
        optimizer::QueryProfiler::getInstance().clearCardinalityFeedback();
        optimizer::QueryProfiler::getInstance().clearProfiles();
        executor_.reset();
        compiler_.reset();
        ConnectionContext::setCurrent(nullptr);
        connection_ctx_.reset();
        db_.reset();
        db_file_.reset();
    }

    bool createDatabase()
    {
        optimizer::QueryProfiler::getInstance().clearCardinalityFeedback();
        optimizer::QueryProfiler::getInstance().clearProfiles();
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
            "CREATE TABLE orders (id INTEGER, user_id INTEGER, amount DOUBLE)",
            "CREATE TABLE test (id INTEGER)",
            "GRANT SELECT ON users TO PUBLIC",
            "GRANT SELECT ON products TO PUBLIC",
            "GRANT SELECT ON orders TO PUBLIC",
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

    QueryCompilerV3::CompileResult compileSQLWithParameters(
        const std::string& sql,
        const optimizer::ParameterBindings& bindings)
    {
        auto compile_result = compiler_->compileWithParameters(sql, bindings);
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
        }
        else
        {
            last_compile_errors_.clear();
        }
        return compile_result;
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

    ExecutionResult executeBytecode(const std::vector<uint8_t>& bytecode)
    {
        if (bytecode.empty())
        {
            return ExecutionResult("Bytecode payload is empty");
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

    const scratchbird::optimizer::RuntimePlanControlEntry* findOptimizerControl(
        const scratchbird::optimizer::RuntimePlan& plan,
        const std::string& name)
    {
        for (const auto& entry : plan.optimizer_controls)
        {
            if (entry.name == name)
            {
                return &entry;
            }
        }
        return nullptr;
    }

    nlohmann::json loadTableMetadataJson(const std::string& table_name)
    {
        nlohmann::json metadata = nlohmann::json::object();
        if (db_ == nullptr)
        {
            ADD_FAILURE() << "Database is not initialized";
            return metadata;
        }
        auto* catalog = db_->catalog_manager();
        if (catalog == nullptr)
        {
            ADD_FAILURE() << "Catalog manager is not available";
            return metadata;
        }

        ErrorContext ctx;
        CatalogManager::SchemaInfo public_schema;
        if (catalog->getSchema("public", public_schema, &ctx) != Status::OK)
        {
            ADD_FAILURE() << "Failed to resolve public schema: " << ctx.message;
            return metadata;
        }

        CatalogManager::TableInfo table_info;
        if (catalog->getTable(public_schema.schema_id, table_name, table_info, &ctx) != Status::OK)
        {
            ADD_FAILURE() << "Failed to load table metadata row: " << ctx.message;
            return metadata;
        }
        if (isZeroId(table_info.storage_params_oid))
        {
            ADD_FAILURE() << "Table storage_params_oid was empty for " << table_name;
            return metadata;
        }

        std::string params;
        if (catalog->loadStringFromToast(table_info.storage_params_oid, 0, params, &ctx) != Status::OK)
        {
            ADD_FAILURE() << "Failed to load table metadata TOAST payload: " << ctx.message;
            return metadata;
        }
        if (params.empty())
        {
            ADD_FAILURE() << "Table metadata payload was empty for " << table_name;
            return metadata;
        }

        try
        {
            metadata = nlohmann::json::parse(params);
        }
        catch (const std::exception& ex)
        {
            ADD_FAILURE() << "Failed to parse metadata JSON: " << ex.what();
        }
        return metadata;
    }

    bool findFirstOpcode(const sblr_v3::Instruction& root,
                         sblr_v3::Opcode target,
                         sblr_v3::Instruction& out)
    {
        std::function<bool(const sblr_v3::Instruction&)> visit_inst;
        std::function<bool(const sblr_v3::Value&)> visit_value;

        visit_value = [&](const sblr_v3::Value& value) -> bool {
            if (const auto* ptr = std::get_if<sblr_v3::Value::InstrPtr>(&value.data))
            {
                return ptr != nullptr && *ptr != nullptr && visit_inst(**ptr);
            }
            if (const auto* list = std::get_if<sblr_v3::Value::List>(&value.data))
            {
                for (const auto& entry : *list)
                {
                    if (visit_value(entry))
                    {
                        return true;
                    }
                }
                return false;
            }
            if (const auto* obj = std::get_if<sblr_v3::Value::Object>(&value.data))
            {
                for (const auto& [_, entry] : *obj)
                {
                    if (visit_value(entry))
                    {
                        return true;
                    }
                }
            }
            return false;
        };

        visit_inst = [&](const sblr_v3::Instruction& inst) -> bool {
            if (static_cast<sblr_v3::Opcode>(inst.opcode) == target)
            {
                out = inst;
                return true;
            }
            return visit_value(inst.payload);
        };

        return visit_inst(root);
    }

    std::vector<std::pair<std::string, std::string>> collectColumnRefs(
        const sblr_v3::Instruction& root)
    {
        std::vector<std::pair<std::string, std::string>> refs;
        std::function<void(const sblr_v3::Instruction&)> visit_inst;
        std::function<void(const sblr_v3::Value&)> visit_value;

        visit_value = [&](const sblr_v3::Value& value) {
            if (const auto* ptr = std::get_if<sblr_v3::Value::InstrPtr>(&value.data))
            {
                if (ptr != nullptr && *ptr != nullptr)
                {
                    visit_inst(**ptr);
                }
                return;
            }
            if (const auto* list = std::get_if<sblr_v3::Value::List>(&value.data))
            {
                for (const auto& entry : *list)
                {
                    visit_value(entry);
                }
                return;
            }
            if (const auto* obj = std::get_if<sblr_v3::Value::Object>(&value.data))
            {
                for (const auto& [_, entry] : *obj)
                {
                    visit_value(entry);
                }
            }
        };

        visit_inst = [&](const sblr_v3::Instruction& inst) {
            if (static_cast<sblr_v3::Opcode>(inst.opcode) == sblr_v3::Opcode::SBLR3_COLUMN_REF)
            {
                const auto* obj = std::get_if<sblr_v3::Value::Object>(&inst.payload.data);
                if (obj != nullptr)
                {
                    std::string column_name;
                    std::string qualifier;
                    auto col_it = obj->find("column");
                    if (col_it != obj->end())
                    {
                        if (const auto* col = std::get_if<std::string>(&col_it->second.data))
                        {
                            column_name = *col;
                        }
                    }
                    auto path_it = obj->find("path");
                    if (path_it != obj->end())
                    {
                        if (const auto* path =
                                std::get_if<sblr_v3::Value::List>(&path_it->second.data))
                        {
                            for (auto it = path->rbegin(); it != path->rend(); ++it)
                            {
                                if (const auto* part = std::get_if<std::string>(&it->data))
                                {
                                    if (!part->empty())
                                    {
                                        qualifier = *part;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    refs.emplace_back(std::move(qualifier), std::move(column_name));
                }
            }
            visit_value(inst.payload);
        };

        visit_inst(root);
        return refs;
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
    EXPECT_EQ(plan.cache_mode, "GENERIC");
    EXPECT_FALSE(plan.plan_profile_signature.empty());
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

TEST_F(QueryPlannerIntegrationTest, ParameterizedSelectUsesCustomPlanProfileAndHitsBucketedCache)
{
    ASSERT_TRUE(createDatabase());
    ASSERT_TRUE(executeSQL("CREATE INDEX idx_users_id ON users (id)").success());

    for (int i = 1; i <= 256; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'u" + std::to_string(i) +
                               "', 'u" + std::to_string(i) + "@x', " +
                               std::to_string(20 + (i % 10)) + ")")
                        .success());
    }

    CatalogManager::TableInfo table_info;
    ErrorContext stats_ctx;
    ASSERT_EQ(db_->catalog_manager()->getTable(connection_ctx_->getCurrentSchemaId(),
                                               "users",
                                               table_info,
                                               &stats_ctx),
              Status::OK)
        << stats_ctx.message;
    ASSERT_EQ(db_->statistics_manager()->analyzeTable(table_info.table_id, 1.0, &stats_ctx),
              Status::OK)
        << stats_ctx.message;

    optimizer::ParameterBindings bindings;
    bindings.positional.push_back({false, "5"});

    QueryCompilerV3::resetPlanCacheStats();

    auto first = compileSQLWithParameters("SELECT id FROM users WHERE id < $1", bindings);
    ASSERT_TRUE(first.success()) << last_compile_errors_;
    EXPECT_EQ(first.planProfile().mode,
              scratchbird::sblr::detail::QueryCompilerV3PlanProfileMode::CUSTOM);
    EXPECT_TRUE(first.planProfile().parameter_sensitive);
    EXPECT_NE(first.planProfile().signature.find("CUSTOM:"), std::string::npos);
    scratchbird::optimizer::RuntimePlan custom_plan;
    ASSERT_TRUE(decodeRuntimePlan(first.bytecode(), custom_plan));
    EXPECT_EQ(custom_plan.cache_mode, "CUSTOM");
    EXPECT_TRUE(custom_plan.parameter_sensitive);
    EXPECT_EQ(custom_plan.plan_profile_signature, first.planProfile().signature);
    EXPECT_FALSE(custom_plan.considered_paths.empty());
    EXPECT_FALSE(custom_plan.statistics_provenance.empty());

    auto after_first = QueryCompilerV3::planCacheStats();
    EXPECT_EQ(after_first.hits, 0u);
    EXPECT_EQ(after_first.misses, 1u);
    EXPECT_EQ(after_first.inserts, 1u);

    auto second = compileSQLWithParameters("SELECT id FROM users WHERE id < $1", bindings);
    ASSERT_TRUE(second.success()) << last_compile_errors_;
    EXPECT_EQ(first.planProfile().signature, second.planProfile().signature);

    auto after_second = QueryCompilerV3::planCacheStats();
    EXPECT_EQ(after_second.hits, 1u);
    EXPECT_EQ(after_second.misses, 1u);
    EXPECT_EQ(after_second.inserts, 1u);
}

TEST_F(QueryPlannerIntegrationTest, ParameterizedRangeBucketsChangeWithBindingSelectivity)
{
    ASSERT_TRUE(createDatabase());
    ASSERT_TRUE(executeSQL("CREATE INDEX idx_users_id ON users (id)").success());

    for (int i = 1; i <= 256; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'u" + std::to_string(i) +
                               "', 'u" + std::to_string(i) + "@x', " +
                               std::to_string(20 + (i % 10)) + ")")
                        .success());
    }

    CatalogManager::TableInfo table_info;
    ErrorContext stats_ctx;
    ASSERT_EQ(db_->catalog_manager()->getTable(connection_ctx_->getCurrentSchemaId(),
                                               "users",
                                               table_info,
                                               &stats_ctx),
              Status::OK)
        << stats_ctx.message;
    ASSERT_EQ(db_->statistics_manager()->analyzeTable(table_info.table_id, 1.0, &stats_ctx),
              Status::OK)
        << stats_ctx.message;

    optimizer::ParameterBindings selective;
    selective.positional.push_back({false, "5"});
    optimizer::ParameterBindings broad;
    broad.positional.push_back({false, "250"});

    auto selective_compile =
        compileSQLWithParameters("SELECT id FROM users WHERE id < $1", selective);
    ASSERT_TRUE(selective_compile.success()) << last_compile_errors_;
    auto broad_compile =
        compileSQLWithParameters("SELECT id FROM users WHERE id < $1", broad);
    ASSERT_TRUE(broad_compile.success()) << last_compile_errors_;

    EXPECT_NE(selective_compile.planProfile().selectivity_bucket_signature,
              broad_compile.planProfile().selectivity_bucket_signature);
    EXPECT_NE(selective_compile.planProfile().signature,
              broad_compile.planProfile().signature);

    scratchbird::optimizer::RuntimePlan selective_plan;
    scratchbird::optimizer::RuntimePlan broad_plan;
    ASSERT_TRUE(decodeRuntimePlan(selective_compile.bytecode(), selective_plan));
    ASSERT_TRUE(decodeRuntimePlan(broad_compile.bytecode(), broad_plan));
    ASSERT_FALSE(selective_plan.relations.empty());
    ASSERT_FALSE(broad_plan.relations.empty());
    EXPECT_EQ(selective_plan.cache_mode, "CUSTOM");
    EXPECT_TRUE(selective_plan.parameter_sensitive);
    EXPECT_EQ(selective_plan.selectivity_bucket_signature,
              selective_compile.planProfile().selectivity_bucket_signature);
    EXPECT_FALSE(selective_plan.considered_paths.empty());
    EXPECT_FALSE(selective_plan.statistics_provenance.empty());
    EXPECT_LT(selective_plan.relations.front().estimated_rows,
              broad_plan.relations.front().estimated_rows);
}

TEST_F(QueryPlannerIntegrationTest, PlanProfileDirectiveForcesGenericParameterizedPlan)
{
    ASSERT_TRUE(createDatabase());
    ASSERT_TRUE(executeSQL("CREATE INDEX idx_users_id ON users (id)").success());

    for (int i = 1; i <= 128; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'u" + std::to_string(i) +
                               "', 'u" + std::to_string(i) + "@x', " +
                               std::to_string(20 + (i % 10)) + ")")
                        .success());
    }

    connection_ctx_->setSessionVariable("OPTIMIZER.PLAN_DIRECTIVES",
                                        "PLAN_PROFILE=GENERIC");

    optimizer::ParameterBindings bindings;
    bindings.positional.push_back({false, "5"});

    auto compile_result =
        compileSQLWithParameters("SELECT id FROM users WHERE id < $1", bindings);
    ASSERT_TRUE(compile_result.success()) << last_compile_errors_;
    EXPECT_EQ(compile_result.planProfile().mode,
              scratchbird::sblr::detail::QueryCompilerV3PlanProfileMode::GENERIC);
    EXPECT_FALSE(compile_result.planProfile().parameter_sensitive);

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(compile_result.bytecode(), plan));
    EXPECT_EQ(plan.cache_mode, "GENERIC");
    EXPECT_FALSE(plan.parameter_sensitive);
    const auto* plan_profile = findOptimizerControl(plan, "PLAN_PROFILE");
    ASSERT_NE(plan_profile, nullptr);
    EXPECT_EQ(plan_profile->value, "GENERIC");
    EXPECT_EQ(plan_profile->source, "DIRECTIVE");
}

TEST_F(QueryPlannerIntegrationTest, CardinalityFeedbackBypassesStaleCacheAndRebuildsPlan)
{
    ASSERT_TRUE(createDatabase());

    for (int i = 1; i <= 4; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'u" + std::to_string(i) +
                               "', 'u" + std::to_string(i) + "@x', 30)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    QueryCompilerV3::resetPlanCacheStats();

    const std::string sql = "SELECT id FROM users WHERE id > 0";
    auto stale_bytecode = compileSQL(sql);
    ASSERT_FALSE(stale_bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan stale_plan;
    ASSERT_TRUE(decodeRuntimePlan(stale_bytecode, stale_plan));
    ASSERT_GT(stale_plan.root.estimated_rows, 0u);

    for (int i = 5; i <= 200; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'u" + std::to_string(i) +
                               "', 'u" + std::to_string(i) + "@x', 30)")
                        .success());
    }

    auto stale_result = executeBytecode(stale_bytecode);
    ASSERT_TRUE(stale_result.success()) << stale_result.error();
    ASSERT_TRUE(stale_result.hasResultSet());
    ASSERT_EQ(stale_result.resultSet()->rowCount(), 200u);

    const auto feedback_key =
        std::to_string(sblr_v3::stableHash64(
            optimizer::QueryProfiler::getInstance().fingerprintQuery(sql)));
    auto feedback =
        optimizer::QueryProfiler::getInstance().latestCardinalityFeedback(feedback_key);
    ASSERT_TRUE(feedback.has_value());
    EXPECT_TRUE(feedback->available);
    EXPECT_TRUE(feedback->replan_required);
    EXPECT_EQ(feedback->last_estimated_rows, stale_plan.root.estimated_rows);
    EXPECT_EQ(feedback->last_actual_rows, 200u);

    auto refreshed_bytecode = compileSQL(sql);
    ASSERT_FALSE(refreshed_bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan refreshed_plan;
    ASSERT_TRUE(decodeRuntimePlan(refreshed_bytecode, refreshed_plan));
    EXPECT_TRUE(refreshed_plan.adaptive_feedback.available);
    EXPECT_FALSE(refreshed_plan.adaptive_feedback.replan_required);
    EXPECT_TRUE(refreshed_plan.adaptive_feedback.stats_refresh_applied);
    EXPECT_EQ(refreshed_plan.adaptive_feedback.observation_count, 1u);
    EXPECT_EQ(refreshed_plan.adaptive_feedback.last_actual_rows, 200u);
    EXPECT_NE(refreshed_plan.root.estimated_rows, stale_plan.root.estimated_rows);

    auto adaptive_it = std::find_if(
        refreshed_plan.statistics_provenance.begin(),
        refreshed_plan.statistics_provenance.end(),
        [](const scratchbird::optimizer::RuntimePlanStatisticsProvenance& entry) {
            return entry.source == "ADAPTIVE_CARDINALITY_FEEDBACK";
        });
    EXPECT_NE(adaptive_it, refreshed_plan.statistics_provenance.end());

    const auto cache_stats = QueryCompilerV3::planCacheStats();
    EXPECT_EQ(cache_stats.hits, 0u);
    EXPECT_EQ(cache_stats.misses, 1u);
    EXPECT_EQ(cache_stats.inserts, 2u);
    EXPECT_GE(cache_stats.invalidations, 1u);
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

TEST_F(QueryPlannerIntegrationTest, LeftJoinCarriesFormalLegalityMetadata)
{
    ASSERT_TRUE(createDatabase());

    auto bytecode =
        compileSQL("SELECT users.id FROM users LEFT JOIN products ON users.id = products.id");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.join_steps.size(), 1u);
    EXPECT_EQ(plan.join_steps.front().join_type, "LEFT");
    EXPECT_EQ(plan.join_steps.front().legality_class, "LEFT_OUTER_BARRIER");
    EXPECT_FALSE(plan.join_steps.front().reorderable);
    EXPECT_TRUE(plan.join_steps.front().preserves_left_rows);
    EXPECT_FALSE(plan.join_steps.front().preserves_right_rows);
    EXPECT_FALSE(plan.join_steps.front().null_introduces_left);
    EXPECT_TRUE(plan.join_steps.front().null_introduces_right);
    EXPECT_TRUE(plan.join_steps.front().requires_original_order);
}

TEST_F(QueryPlannerIntegrationTest, UsingJoinMarksReorderBarrierAndFailsClosedToNestedLoop)
{
    ASSERT_TRUE(createDatabase());

    auto bytecode =
        compileSQL("SELECT users.id FROM users JOIN products USING (id)");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.join_steps.size(), 1u);
    EXPECT_EQ(plan.join_steps.front().join_type, "INNER");
    EXPECT_EQ(plan.join_steps.front().legality_class, "USING_BARRIER");
    EXPECT_FALSE(plan.join_steps.front().reorderable);
    EXPECT_TRUE(plan.join_steps.front().requires_original_order);
    EXPECT_EQ(plan.join_steps.front().method, "NESTED_LOOP");
    EXPECT_FALSE(plan.join_steps.front().has_hash_keys);
}

TEST_F(QueryPlannerIntegrationTest, MixedOuterAndInnerJoinKeepsSyntacticRelationOrder)
{
    ASSERT_TRUE(createDatabase());

    auto bytecode = compileSQL(
        "SELECT users.id "
        "FROM users "
        "LEFT JOIN products ON users.id = products.id "
        "JOIN test ON test.id = products.id");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 3u);
    ASSERT_EQ(plan.join_steps.size(), 2u);
    EXPECT_EQ(plan.join_steps[0].source_join_index, 0u);
    EXPECT_EQ(plan.join_steps[1].source_join_index, 1u);
    EXPECT_EQ(plan.join_steps[0].legality_class, "LEFT_OUTER_BARRIER");
    EXPECT_FALSE(plan.join_steps[0].reorderable);
    EXPECT_EQ(plan.join_steps[1].join_type, "INNER");
}

TEST_F(QueryPlannerIntegrationTest, DisconnectedJoinGraphKeepsExplicitCrossJoinStep)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL(
                    "INSERT INTO users (id, name, email, age) VALUES (1, 'alice', 'a@example.com', 30)")
                    .success());
    ASSERT_TRUE(executeSQL("INSERT INTO products (id, name, price) VALUES (1, 'p1', 10.5)").success());
    ASSERT_TRUE(executeSQL("INSERT INTO test (id) VALUES (10)").success());
    ASSERT_TRUE(executeSQL("INSERT INTO test (id) VALUES (20)").success());

    const std::string sql =
        "SELECT users.id FROM users JOIN products ON users.id = products.id, test";
    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.join_steps.size(), 2u);

    auto cross_it = std::find_if(plan.join_steps.begin(),
                                 plan.join_steps.end(),
                                 [](const scratchbird::optimizer::RuntimePlanJoinStep &step) {
                                     return step.join_type == "CROSS";
                                 });
    ASSERT_NE(cross_it, plan.join_steps.end());
    EXPECT_EQ(cross_it->method, "NESTED_LOOP");

    auto result = executeSQL(sql);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto rows = resultStrings(result);
    ASSERT_EQ(rows.size(), 2u);
    EXPECT_EQ(rows[0], "1");
    EXPECT_EQ(rows[1], "1");
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

TEST_F(QueryPlannerIntegrationTest, HashJoinRuntimePlanTracksMemoryBudgetAndSpillMetadata)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("WORK_MEM", "64KB");

    for (int i = 1; i <= 1600; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" + std::to_string(i) +
                               "', 'u" + std::to_string(i) +
                               "@example.com', " + std::to_string(20 + (i % 50)) + ")")
                        .success());
        ASSERT_TRUE(executeSQL("INSERT INTO products (id, name, price) VALUES (" +
                               std::to_string(i) + ", 'p" + std::to_string(i) +
                               "', " + std::to_string(10 + (i % 100)) + ".0)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());
    ASSERT_TRUE(executeSQL("ANALYZE products").success());

    auto bytecode =
        compileSQL("SELECT users.id FROM users JOIN products ON users.id = products.id");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.join_steps.size(), 1u);
    EXPECT_EQ(plan.join_steps.front().method, "HASH_JOIN");
    EXPECT_TRUE(plan.join_steps.front().spill_expected);
    EXPECT_GT(plan.join_steps.front().spill_passes, 0u);
    EXPECT_GT(plan.join_steps.front().estimated_memory_bytes, 0u);
    EXPECT_GT(plan.join_steps.front().memory_budget_bytes, 0u);
    EXPECT_EQ(plan.join_steps.front().spill_policy, "ALLOW");
    EXPECT_EQ(plan.root.node_type, "HashJoin");
    EXPECT_TRUE(plan.root.spill_expected);
    EXPECT_GT(plan.root.memory_budget_bytes, 0u);
    const auto* work_mem = findOptimizerControl(plan, "WORK_MEM");
    ASSERT_NE(work_mem, nullptr);
    EXPECT_EQ(work_mem->source, "SESSION");
    const auto* spill_policy = findOptimizerControl(plan, "SPILL_POLICY");
    ASSERT_NE(spill_policy, nullptr);
    EXPECT_EQ(spill_policy->value, "ALLOW");
}

TEST_F(QueryPlannerIntegrationTest, JoinMethodControlForcesNestedLoopAndChangesCacheIdentity)
{
    ASSERT_TRUE(createDatabase());

    for (int i = 1; i <= 256; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" + std::to_string(i) +
                               "', 'u" + std::to_string(i) +
                               "@example.com', " + std::to_string(20 + (i % 50)) + ")")
                        .success());
        ASSERT_TRUE(executeSQL("INSERT INTO products (id, name, price) VALUES (" +
                               std::to_string(i) + ", 'p" + std::to_string(i) +
                               "', " + std::to_string(10 + (i % 100)) + ".0)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());
    ASSERT_TRUE(executeSQL("ANALYZE products").success());

    const std::string sql =
        "SELECT users.id FROM users JOIN products ON users.id = products.id";
    QueryCompilerV3::resetPlanCacheStats();

    auto default_bytecode = compileSQL(sql);
    ASSERT_FALSE(default_bytecode.empty()) << last_compile_errors_;
    auto after_default = QueryCompilerV3::planCacheStats();
    EXPECT_EQ(after_default.misses, 1u);

    scratchbird::optimizer::RuntimePlan default_plan;
    ASSERT_TRUE(decodeRuntimePlan(default_bytecode, default_plan));

    connection_ctx_->setSessionVariable("OPTIMIZER.JOIN_METHOD", "NESTED_LOOP");

    auto forced_bytecode = compileSQL(sql);
    ASSERT_FALSE(forced_bytecode.empty()) << last_compile_errors_;
    auto after_forced = QueryCompilerV3::planCacheStats();
    EXPECT_EQ(after_forced.misses, 2u);

    scratchbird::optimizer::RuntimePlan forced_plan;
    ASSERT_TRUE(decodeRuntimePlan(forced_bytecode, forced_plan));
    ASSERT_EQ(forced_plan.join_steps.size(), 1u);
    EXPECT_EQ(forced_plan.join_steps.front().method, "NESTED_LOOP");
    const auto* join_method = findOptimizerControl(forced_plan, "JOIN_METHOD");
    ASSERT_NE(join_method, nullptr);
    EXPECT_EQ(join_method->value, "NESTED_LOOP");
    EXPECT_EQ(join_method->source, "SESSION");
    EXPECT_NE(default_plan.plan_hash, forced_plan.plan_hash);
}

TEST_F(QueryPlannerIntegrationTest, UnsupportedOptimizerDirectiveFailsClosed)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("OPTIMIZER.PLAN_DIRECTIVES", "FOO=BAR");
    auto bytecode =
        compileSQL("SELECT users.id FROM users JOIN products ON users.id = products.id");
    EXPECT_TRUE(bytecode.empty());
    EXPECT_NE(last_compile_errors_.find("Unsupported optimizer directive"),
              std::string::npos);
}

TEST_F(QueryPlannerIntegrationTest, SpillPolicyDisallowRejectsSpilledHashJoin)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("WORK_MEM", "64KB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "DISALLOW");

    for (int i = 1; i <= 1600; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" + std::to_string(i) +
                               "', 'u" + std::to_string(i) +
                               "@example.com', " + std::to_string(20 + (i % 50)) + ")")
                        .success());
        ASSERT_TRUE(executeSQL("INSERT INTO products (id, name, price) VALUES (" +
                               std::to_string(i) + ", 'p" + std::to_string(i) +
                               "', " + std::to_string(10 + (i % 100)) + ".0)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());
    ASSERT_TRUE(executeSQL("ANALYZE products").success());

    auto bytecode =
        compileSQL("SELECT users.id FROM users JOIN products ON users.id = products.id");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.join_steps.size(), 1u);
    EXPECT_EQ(plan.join_steps.front().method, "NESTED_LOOP");

    bool found_hash_rejection = false;
    for (const auto& entry : plan.rejected_paths)
    {
        if (entry.candidate == "HASH_JOIN" &&
            entry.reason.find("spill policy disallows") != std::string::npos)
        {
            found_hash_rejection = true;
            break;
        }
    }
    EXPECT_TRUE(found_hash_rejection);
}

TEST_F(QueryPlannerIntegrationTest, MergeJoinPlanExecutesAndPreservesRuntimeMetadata)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL("CREATE INDEX idx_users_id ON users (id)").success());
    ASSERT_TRUE(executeSQL("CREATE INDEX idx_products_id ON products (id)").success());

    for (int i = 1; i <= 4; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" + std::to_string(i) +
                               "', 'u" + std::to_string(i) +
                               "@example.com', " + std::to_string(20 + i) + ")")
                        .success());
        ASSERT_TRUE(executeSQL("INSERT INTO products (id, name, price) VALUES (" +
                               std::to_string(i) + ", 'p" + std::to_string(i) +
                               "', " + std::to_string(10 + i) + ".0)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());
    ASSERT_TRUE(executeSQL("ANALYZE products").success());

    const std::string sql =
        "SELECT users.id "
        "FROM users JOIN products ON users.id = products.id "
        "WHERE users.id = 2 AND products.id = 2";
    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.join_steps.size(), 1u);
    EXPECT_EQ(plan.join_steps.front().method, "MERGE_JOIN");
    EXPECT_TRUE(plan.join_steps.front().has_merge_keys);
    EXPECT_EQ(plan.join_steps.front().left_merge_key.column_name, "id");
    EXPECT_EQ(plan.join_steps.front().right_merge_key.column_name, "id");
    EXPECT_EQ(plan.root.node_type, "MergeJoin");
    EXPECT_NE(plan.root.detail_text.find("presorted"), std::string::npos);

    auto result = executeSQL(sql);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    const auto rows = resultStrings(result);
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows.front(), "2");
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
    EXPECT_NE(lines.front().find("\"cache_mode\":\"GENERIC\""), std::string::npos);
    EXPECT_NE(lines.front().find("\"plan_profile_signature\":\""), std::string::npos);
    EXPECT_NE(lines.front().find("\"optimizer_trace\":"), std::string::npos);
    EXPECT_NE(lines.front().find("\"considered_paths\":"), std::string::npos);
    EXPECT_NE(lines.front().find("\"rejected_paths\":"), std::string::npos);
    EXPECT_NE(lines.front().find("\"statistics_provenance\":"), std::string::npos);
    EXPECT_NE(lines.front().find("\"adaptive_feedback\":"), std::string::npos);
    EXPECT_NE(lines.front().find("\"analyze\":{\"rows\":"), std::string::npos);
}

TEST_F(QueryPlannerIntegrationTest, ExplainJsonIncludesOperatorMemoryAndSpillMetadata)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("WORK_MEM", "64KB");
    for (int i = 1; i <= 2000; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" + std::to_string(i) +
                               "', 'u" + std::to_string(i) +
                               "@example.com', " + std::to_string(20 + (i % 50)) + ")")
                        .success());
    }

    auto result = executeSQL("EXPLAIN (FORMAT JSON) "
                             "SELECT name FROM users ORDER BY name");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    const auto lines = resultStrings(result);
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_NE(lines.front().find("\"estimated_memory_bytes\":"), std::string::npos);
    EXPECT_NE(lines.front().find("\"memory_budget_bytes\":"), std::string::npos);
    EXPECT_NE(lines.front().find("\"spill_expected\":true"), std::string::npos);
    EXPECT_NE(lines.front().find("\"spill_policy\":\"ALLOW\""), std::string::npos);
    EXPECT_NE(lines.front().find("\"optimizer_controls\":"), std::string::npos);
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

TEST_F(QueryPlannerIntegrationTest, LateralJoinUsesParameterizedNestedLoopPath)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL(
                    "INSERT INTO users (id, name, email, age) VALUES (1, 'alice', 'a@example.com', 30)")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "INSERT INTO users (id, name, email, age) VALUES (2, 'bob', 'b@example.com', 31)")
                    .success());
    ASSERT_TRUE(executeSQL("INSERT INTO orders (id, user_id, amount) VALUES (10, 1, 50.0)")
                    .success());

    const std::string sql =
        "SELECT u.id, o.order_id "
        "FROM users AS u "
        "LEFT JOIN LATERAL "
        "(SELECT id AS order_id FROM orders WHERE orders.user_id = u.id) AS o "
        "ON TRUE "
        "ORDER BY u.id";

    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 2u);
    EXPECT_TRUE(plan.relations[1].lateral);
    EXPECT_TRUE(plan.relations[1].parameterized);
    ASSERT_EQ(plan.join_steps.size(), 1u);
    EXPECT_EQ(plan.join_steps[0].method, "NESTED_LOOP");

    auto result = executeSQL(sql);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 2u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "1");
    EXPECT_EQ(result.resultSet()->getValue(0, 1).toString(), "10");
    EXPECT_EQ(result.resultSet()->getValue(1, 0).toString(), "2");
    EXPECT_TRUE(result.resultSet()->getValue(1, 1).isNull());
}

TEST_F(QueryPlannerIntegrationTest, CorrelatedExistsAndNotExistsUseOuterRowBindings)
{
    ASSERT_TRUE(createDatabase());

    for (int id : {1, 2, 3})
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(id) + ", 'u" + std::to_string(id) +
                               "', 'u" + std::to_string(id) + "@example.com', 30)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("INSERT INTO orders (id, user_id, amount) VALUES (100, 1, 25.0)")
                    .success());
    ASSERT_TRUE(executeSQL("INSERT INTO orders (id, user_id, amount) VALUES (101, 3, 30.0)")
                    .success());

    auto exists_result = executeSQL(
        "SELECT u.id FROM users AS u "
        "WHERE EXISTS (SELECT 1 FROM orders WHERE orders.user_id = u.id) "
        "ORDER BY u.id");
    ASSERT_TRUE(exists_result.success()) << exists_result.error();
    ASSERT_TRUE(exists_result.hasResultSet());
    ASSERT_EQ(exists_result.resultSet()->rowCount(), 2u);
    EXPECT_EQ(exists_result.resultSet()->getValue(0, 0).toString(), "1");
    EXPECT_EQ(exists_result.resultSet()->getValue(1, 0).toString(), "3");

    auto not_exists_result = executeSQL(
        "SELECT u.id FROM users AS u "
        "WHERE NOT EXISTS (SELECT 1 FROM orders WHERE orders.user_id = u.id) "
        "ORDER BY u.id");
    ASSERT_TRUE(not_exists_result.success()) << not_exists_result.error();
    ASSERT_TRUE(not_exists_result.hasResultSet());
    ASSERT_EQ(not_exists_result.resultSet()->rowCount(), 1u);
    EXPECT_EQ(not_exists_result.resultSet()->getValue(0, 0).toString(), "2");
}

TEST_F(QueryPlannerIntegrationTest, PartitionedParentSelectPrunesChildTargetsAndReturnsRows)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL(
                    "CREATE TABLE measurement (id INTEGER, region INTEGER, note VARCHAR(32)) "
                    "PARTITION BY RANGE (region)")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "CREATE TABLE measurement_p1 (id INTEGER, region INTEGER, note VARCHAR(32))")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "CREATE TABLE measurement_p2 (id INTEGER, region INTEGER, note VARCHAR(32))")
                    .success());
    const auto create_metadata = loadTableMetadataJson("measurement");
    ASSERT_TRUE(create_metadata.contains("partition"));
    ASSERT_TRUE(create_metadata["partition"].is_object());
    EXPECT_EQ(create_metadata["partition"].value("strategy", std::string()), "RANGE");
    ASSERT_TRUE(create_metadata["partition"].contains("columns"));
    ASSERT_TRUE(create_metadata["partition"]["columns"].is_array());
    ASSERT_EQ(create_metadata["partition"]["columns"].size(), 1u);
    EXPECT_EQ(create_metadata["partition"]["columns"][0].get<std::string>(), "region");
    {
        auto attach_p1 = executeSQL(
            "ALTER TABLE measurement ATTACH PARTITION measurement_p1 "
            "FOR VALUES FROM (0) TO (100)");
        ASSERT_TRUE(attach_p1.success()) << attach_p1.error();
    }
    {
        auto attach_p2 = executeSQL(
            "ALTER TABLE measurement ATTACH PARTITION measurement_p2 "
            "FOR VALUES FROM (100) TO (200)");
        ASSERT_TRUE(attach_p2.success()) << attach_p2.error();
    }

    const auto metadata = loadTableMetadataJson("measurement");
    ASSERT_TRUE(metadata.contains("partition"));
    ASSERT_TRUE(metadata["partition"].is_object());
    EXPECT_EQ(metadata["partition"].value("strategy", std::string()), "RANGE");
    ASSERT_TRUE(metadata["partition"].contains("columns"));
    ASSERT_TRUE(metadata["partition"]["columns"].is_array());
    ASSERT_EQ(metadata["partition"]["columns"].size(), 1u);
    EXPECT_EQ(metadata["partition"]["columns"][0].get<std::string>(), "region");
    ASSERT_TRUE(metadata["partition"].contains("children"));
    ASSERT_TRUE(metadata["partition"]["children"].is_array());
    ASSERT_EQ(metadata["partition"]["children"].size(), 2u);

    ASSERT_TRUE(executeSQL(
                    "INSERT INTO measurement_p1 (id, region, note) VALUES (1, 10, 'west')")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "INSERT INTO measurement_p2 (id, region, note) VALUES (2, 150, 'east')")
                    .success());
    ASSERT_TRUE(executeSQL("ANALYZE measurement_p1").success());
    ASSERT_TRUE(executeSQL("ANALYZE measurement_p2").success());

    const std::string sql =
        "SELECT id, note FROM measurement WHERE region >= 100 AND region < 200 ORDER BY id";
    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 1u);
    EXPECT_TRUE(plan.relations.front().partition_pruned);
    EXPECT_EQ(plan.relations.front().partition_strategy, "RANGE");
    EXPECT_EQ(plan.relations.front().partition_key_column, "region");
    ASSERT_EQ(plan.relations.front().partition_targets.size(), 1u);
    EXPECT_EQ(plan.relations.front().partition_targets.front(), "measurement_p2");

    auto result = executeSQL(sql);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 1u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "2");
    EXPECT_EQ(result.resultSet()->getValue(0, 1).toString(), "east");
}

TEST_F(QueryPlannerIntegrationTest, JoinRuntimeFilterUsesRightSideIndexMetadata)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL("CREATE INDEX idx_orders_user_id ON orders (user_id)").success());
    for (int id = 1; id <= 4; ++id)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(id) + ", 'u" + std::to_string(id) +
                               "', 'u" + std::to_string(id) + "@example.com', 30)")
                        .success());
    }
    for (int order_id = 1; order_id <= 12; ++order_id)
    {
        const int user_id = ((order_id - 1) % 4) + 1;
        ASSERT_TRUE(executeSQL("INSERT INTO orders (id, user_id, amount) VALUES (" +
                               std::to_string(order_id) + ", " +
                               std::to_string(user_id) + ", " +
                               std::to_string(10 + order_id) + ".0)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());
    ASSERT_TRUE(executeSQL("ANALYZE orders").success());

    const std::string sql =
        "SELECT o.user_id, o.amount "
        "FROM users AS u "
        "JOIN orders AS o ON u.id = o.user_id "
        "WHERE u.id <= 2 "
        "ORDER BY o.user_id, o.amount";
    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    auto relation_it =
        std::find_if(plan.relations.begin(),
                     plan.relations.end(),
                     [](const scratchbird::optimizer::RuntimePlanRelation& relation) {
                         return relation.table_path == "orders" || relation.alias == "o";
                     });
    ASSERT_NE(relation_it, plan.relations.end());
    EXPECT_TRUE(relation_it->runtime_filter_enabled);
    EXPECT_EQ(relation_it->runtime_filter_column, "user_id");
    EXPECT_EQ(relation_it->runtime_filter_index_name, "idx_orders_user_id");

    auto result = executeSQL(sql);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 6u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "1");
    EXPECT_EQ(result.resultSet()->getValue(5, 0).toString(), "2");
}

TEST_F(QueryPlannerIntegrationTest, TopNSortPlanAnnotatesRuntimePlanAndExecutesCorrectly)
{
    ASSERT_TRUE(createDatabase());

    for (int i = 1; i <= 128; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'topn" + std::to_string(i) +
                               "', 'topn" + std::to_string(i) +
                               "@example.com', 20)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const std::string sql = "SELECT id FROM users ORDER BY id DESC LIMIT 5";
    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.root.node_type, "Limit");
    ASSERT_EQ(plan.root.children.size(), 1u);
    EXPECT_EQ(plan.root.children.front().node_type, "Sort");
    EXPECT_NE(plan.root.children.front().detail_text.find("topn=5"), std::string::npos);

    auto result = executeSQL(sql);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 5u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "128");
    EXPECT_EQ(result.resultSet()->getValue(4, 0).toString(), "124");
}

TEST_F(QueryPlannerIntegrationTest, CorrelatedExistsPreservesQualifiedOuterReferenceInBytecode)
{
    ASSERT_TRUE(createDatabase());

    auto bytecode = compileSQL(
        "SELECT u.id FROM users AS u "
        "WHERE EXISTS (SELECT 1 FROM orders WHERE orders.user_id = u.id)");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    sblr_v3::Instruction select_inst;
    ASSERT_TRUE(decodeFirstSelect(bytecode, select_inst));

    sblr_v3::Instruction exists_inst;
    ASSERT_TRUE(findFirstOpcode(select_inst, sblr_v3::Opcode::SBLR3_SUBQUERY_EXISTS, exists_inst));

    const auto* exists_obj = std::get_if<sblr_v3::Value::Object>(&exists_inst.payload.data);
    ASSERT_NE(exists_obj, nullptr);
    auto query_it = exists_obj->find("query");
    ASSERT_NE(query_it, exists_obj->end());
    const auto* query_ptr = std::get_if<sblr_v3::Value::InstrPtr>(&query_it->second.data);
    ASSERT_NE(query_ptr, nullptr);
    ASSERT_TRUE(*query_ptr != nullptr);

    const auto refs = collectColumnRefs(**query_ptr);
    EXPECT_NE(std::find(refs.begin(), refs.end(), std::make_pair(std::string("orders"),
                                                                 std::string("user_id"))),
              refs.end());
    EXPECT_NE(std::find(refs.begin(), refs.end(), std::make_pair(std::string("u"),
                                                                 std::string("id"))),
              refs.end());
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
