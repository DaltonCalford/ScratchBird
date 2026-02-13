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
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/query_compiler_v3.h"
#include "test_helpers.h"

#include <memory>
#include <string>
#include <vector>

using namespace scratchbird;
using namespace scratchbird::sblr;
using scratchbird::testing::TestDatabaseFile;

namespace {

struct IndexSpec {
    const char* case_name;
    const char* method;
    const char* column;
};

constexpr IndexSpec kIndexSpecs[] = {
    {"BTree", "BTREE", "id"},
    {"Hash", "HASH", "id"},
    {"Gin", "GIN", "tag"},
    {"GiST", "GIST", "val"},
    {"SPGiST", "SPGIST", "val"},
    {"Brin", "BRIN", "val"},
    {"RTree", "RTREE", "val"},
    {"Hnsw", "HNSW", "embedding"},
    {"Bitmap", "BITMAP", "val"},
    {"Columnstore", "COLUMNSTORE", "val"},
    {"Lsm", "LSM", "id"},
};

}  // namespace

class IndexRuntimeTest : public ::testing::TestWithParam<IndexSpec>
{
protected:
    void SetUp() override
    {
        db_file_ = std::make_unique<TestDatabaseFile>("test_index_runtime");

        core::ErrorContext ctx;
        ASSERT_EQ(core::Database::create(db_file_->path(), 16384, &ctx), core::Status::OK)
            << "Failed to create database: " << ctx.message;

        db_ = std::make_unique<core::Database>();
        ASSERT_EQ(db_->open(db_file_->path(), &ctx), core::Status::OK)
            << "Failed to open database: " << ctx.message;

        auto status = core::ProcArrayManager::initialize(db_.get(), 10, &ctx);
        ASSERT_EQ(status, core::Status::OK) << "Failed to initialize ProcArray: " << ctx.message;

        status = core::ProcArrayManager::registerBackend(&proc_id_, &ctx);
        ASSERT_EQ(status, core::Status::OK) << "Failed to register backend: " << ctx.message;

        conn_ctx_ = std::make_unique<core::ConnectionContext>(db_.get(), proc_id_);
        status = conn_ctx_->initialize(&ctx);
        ASSERT_EQ(status, core::Status::OK) << "Failed to initialize connection context: " << ctx.message;

        core::CatalogManager::SchemaInfo schema;
        ASSERT_EQ(db_->catalog_manager()->getSchema("PUBLIC", schema, &ctx), core::Status::OK)
            << "Failed to get PUBLIC schema: " << ctx.message;
        schema_id_ = schema.schema_id;

        compiler_ = std::make_unique<QueryCompilerV3>(db_.get());
        compiler_->setCurrentSchema(schema_id_);

        executor_ = std::make_unique<Executor>(db_.get());
        executor_->setConnectionContext(conn_ctx_.get());
        executor_->setCurrentSchema(schema_id_);
        conn_ctx_->setCurrentSchemaId(schema_id_);
    }

    void TearDown() override
    {
        executor_.reset();
        compiler_.reset();
        conn_ctx_.reset();

        core::ErrorContext ctx;
        core::ProcArrayManager::unregisterBackend(proc_id_, &ctx);
        core::ProcArrayManager::shutdown(&ctx);

        db_.reset();
        db_file_.reset();
    }

    ExecutionResult executeSQL(const std::string& sql)
    {
        auto compile_result = compiler_->compile(sql);
        if (!compile_result.success())
        {
            if (!compile_result.errors().empty())
            {
                return ExecutionResult(compile_result.errors().front());
            }
            return ExecutionResult("Compile error");
        }

        return executor_->execute(compile_result.bytecode());
    }

    void commitTransaction()
    {
        core::ErrorContext ctx;
        auto status = conn_ctx_->commit(&ctx);
        ASSERT_EQ(status, core::Status::OK) << "Failed to commit: " << ctx.message;
    }

    std::unique_ptr<TestDatabaseFile> db_file_;
    std::unique_ptr<core::Database> db_;
    std::unique_ptr<core::ConnectionContext> conn_ctx_;
    std::unique_ptr<Executor> executor_;
    std::unique_ptr<QueryCompilerV3> compiler_;
    core::ID schema_id_{};
    uint32_t proc_id_ = 0;
};

TEST_P(IndexRuntimeTest, CreateInsertSelectPerIndexType)
{
    const IndexSpec& spec = GetParam();
    auto create_result = executeSQL(
        "CREATE TABLE index_runtime ("
        "id INT NOT NULL, "
        "val INT, "
        "tag TEXT, "
        "embedding VECTOR(3)"
        ")"
    );
    ASSERT_TRUE(create_result.success()) << "CREATE TABLE failed: " << create_result.error();

    SCOPED_TRACE(spec.method);
    std::string sql = "CREATE INDEX idx_";
    sql += spec.case_name;
    sql += " ON index_runtime USING ";
    sql += spec.method;
    sql += " (";
    sql += spec.column;
    sql += ")";

    auto result = executeSQL(sql);
    ASSERT_TRUE(result.success()) << "CREATE INDEX failed for " << spec.method
                                  << ": " << result.error();

    commitTransaction();

    auto insert_result = executeSQL(
        "INSERT INTO index_runtime (id, val, tag) VALUES (1, 42, 'alpha')"
    );
    ASSERT_TRUE(insert_result.success()) << "INSERT failed: " << insert_result.error();
    commitTransaction();

    auto select_result = executeSQL(
        "SELECT id, val, tag FROM index_runtime WHERE id = 1"
    );
    ASSERT_TRUE(select_result.success()) << "SELECT failed: " << select_result.error();
    ASSERT_TRUE(select_result.hasResultSet()) << "Expected a result set";

    auto* result_set = select_result.resultSet();
    ASSERT_NE(result_set, nullptr);
    ASSERT_EQ(result_set->rowCount(), 1u);
    EXPECT_EQ(result_set->getValue(0, 0).toString(), "1");
    EXPECT_EQ(result_set->getValue(0, 1).toString(), "42");
    EXPECT_EQ(result_set->getValue(0, 2).toString(), "alpha");
}

INSTANTIATE_TEST_SUITE_P(
    IndexTypes,
    IndexRuntimeTest,
    ::testing::ValuesIn(kIndexSpecs),
    [](const ::testing::TestParamInfo<IndexSpec>& info) {
        return info.param.case_name;
    }
);
