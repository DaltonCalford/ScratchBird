/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
// ScratchBird Constraint Enforcement Runtime Tests
// Validates CHECK and NOT NULL enforcement through SQL execution.

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

using namespace scratchbird;
using namespace scratchbird::sblr;
using scratchbird::testing::TestDatabaseFile;

class ConstraintRuntimeTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        db_file_ = std::make_unique<TestDatabaseFile>("test_constraint_runtime");

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

        auto system_user = db_->catalog_manager()->getSystemUserId(&ctx);
        conn_ctx_->setCurrentUser(system_user, true);

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

TEST_F(ConstraintRuntimeTest, CheckConstraintRejectsInsertAndUpdate)
{
    auto create_result = executeSQL(
        "CREATE TABLE check_rt (id INT, age INT CHECK (age > 0))");
    ASSERT_TRUE(create_result.success()) << "CREATE TABLE failed: " << create_result.error();
    commitTransaction();

    auto insert_bad = executeSQL("INSERT INTO check_rt VALUES (1, -1)");
    EXPECT_FALSE(insert_bad.success());
    EXPECT_NE(insert_bad.error().find("CHECK constraint"), std::string::npos);

    auto insert_ok = executeSQL("INSERT INTO check_rt VALUES (1, 10)");
    ASSERT_TRUE(insert_ok.success()) << "INSERT failed: " << insert_ok.error();
    commitTransaction();

    auto update_bad = executeSQL("UPDATE check_rt SET age = -5 WHERE id = 1");
    EXPECT_FALSE(update_bad.success());
    EXPECT_NE(update_bad.error().find("CHECK constraint"), std::string::npos);
}

TEST_F(ConstraintRuntimeTest, NotNullRejectsInsertAndUpdate)
{
    auto create_result = executeSQL(
        "CREATE TABLE nn_rt (id INT NOT NULL, val INT)");
    ASSERT_TRUE(create_result.success()) << "CREATE TABLE failed: " << create_result.error();
    commitTransaction();

    auto insert_bad = executeSQL("INSERT INTO nn_rt (id, val) VALUES (NULL, 1)");
    EXPECT_FALSE(insert_bad.success());
    EXPECT_NE(insert_bad.error().find("NOT NULL"), std::string::npos);

    auto insert_ok = executeSQL("INSERT INTO nn_rt (id, val) VALUES (1, 1)");
    ASSERT_TRUE(insert_ok.success()) << "INSERT failed: " << insert_ok.error();
    commitTransaction();

    auto update_bad = executeSQL("UPDATE nn_rt SET id = NULL WHERE id = 1");
    EXPECT_FALSE(update_bad.success());
    EXPECT_NE(update_bad.error().find("NOT NULL"), std::string::npos);
}
