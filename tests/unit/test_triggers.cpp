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
#include "scratchbird/parser/parser_v2.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/query_compiler_v2.h"
#include "test_helpers.h"

#include <memory>
#include <string>

using namespace scratchbird;
using namespace scratchbird::parser::v2;
using namespace scratchbird::sblr;
using scratchbird::testing::TestDatabaseFile;

class TriggerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        db_file_ = std::make_unique<TestDatabaseFile>("test_triggers");

        core::ErrorContext ctx;
        ASSERT_EQ(core::Database::create(db_file_->path(), 16384, &ctx), core::Status::OK)
            << ctx.message;

        db_ = std::make_unique<core::Database>();
        ASSERT_EQ(db_->open(db_file_->path(), &ctx), core::Status::OK) << ctx.message;

        core::CatalogManager::SchemaInfo schema;
        ASSERT_EQ(db_->catalog_manager()->getSchema("PUBLIC", schema, &ctx), core::Status::OK)
            << ctx.message;
        schema_id_ = schema.schema_id;

        compiler_ = std::make_unique<QueryCompilerV2>(db_.get());
        compiler_->setCurrentSchema(schema_id_);

        executor_ = std::make_unique<Executor>(db_.get());
        executor_->setCurrentSchema(schema_id_);

        if (!triggerSyntaxSupported())
        {
            GTEST_SKIP() << "Parser V2 does not support CREATE TRIGGER yet";
        }
    }

    void TearDown() override
    {
        executor_.reset();
        compiler_.reset();
        db_.reset();
        db_file_.reset();
    }

    ExecutionResult executeSql(const std::string& sql)
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

    void createTestTable()
    {
        auto result = executeSql("CREATE TABLE users (id INT32, name VARCHAR, age INT32)");
        ASSERT_TRUE(result.success()) << "Failed to create test table: " << result.error();
    }

private:
    bool triggerSyntaxSupported() const
    {
        Parser parser("CREATE TRIGGER tr AFTER INSERT ON users FOR EACH ROW EXECUTE PROCEDURE p()");
        auto result = parser.parseStatement();
        return result.success();
    }

    std::unique_ptr<TestDatabaseFile> db_file_;
    std::unique_ptr<core::Database> db_;
    std::unique_ptr<QueryCompilerV2> compiler_;
    std::unique_ptr<Executor> executor_;
    core::ID schema_id_{};
};

// Test 1: CREATE TRIGGER with AFTER INSERT
TEST_F(TriggerTest, CreateTriggerAfterInsert)
{
    createTestTable();

    std::string sql = "CREATE TRIGGER log_insert AFTER INSERT ON users "
                      "FOR EACH ROW EXECUTE PROCEDURE log_user_insert()";

    auto result = executeSql(sql);
    EXPECT_TRUE(result.success()) << "Failed: " << result.error();
}

// Test 2: CREATE TRIGGER with BEFORE UPDATE
TEST_F(TriggerTest, CreateTriggerBeforeUpdate)
{
    createTestTable();

    std::string sql = "CREATE TRIGGER validate_update BEFORE UPDATE ON users "
                      "FOR EACH ROW EXECUTE PROCEDURE validate_user_update()";

    auto result = executeSql(sql);
    EXPECT_TRUE(result.success()) << "Failed: " << result.error();
}

// Test 3: CREATE TRIGGER with AFTER DELETE
TEST_F(TriggerTest, CreateTriggerAfterDelete)
{
    createTestTable();

    std::string sql = "CREATE TRIGGER audit_delete AFTER DELETE ON users "
                      "FOR EACH ROW EXECUTE PROCEDURE audit_user_delete()";

    auto result = executeSql(sql);
    EXPECT_TRUE(result.success()) << "Failed: " << result.error();
}

// Test 4: CREATE TRIGGER with BEFORE INSERT
TEST_F(TriggerTest, CreateTriggerBeforeInsert)
{
    createTestTable();

    std::string sql = "CREATE TRIGGER check_insert BEFORE INSERT ON users "
                      "FOR EACH ROW EXECUTE PROCEDURE check_user_insert()";

    auto result = executeSql(sql);
    EXPECT_TRUE(result.success()) << "Failed: " << result.error();
}

// Test 5: DROP TRIGGER
TEST_F(TriggerTest, DropTrigger)
{
    createTestTable();

    auto create_result = executeSql(
        "CREATE TRIGGER log_insert AFTER INSERT ON users "
        "FOR EACH ROW EXECUTE PROCEDURE log_user_insert()");
    ASSERT_TRUE(create_result.success()) << "Failed to create: " << create_result.error();

    auto drop_result = executeSql("DROP TRIGGER log_insert");
    EXPECT_TRUE(drop_result.success()) << "Failed to drop: " << drop_result.error();
}

// Test 6: Multiple triggers on same table
TEST_F(TriggerTest, MultipleTriggers)
{
    createTestTable();

    auto result1 = executeSql(
        "CREATE TRIGGER before_ins BEFORE INSERT ON users "
        "FOR EACH ROW EXECUTE PROCEDURE check_insert()");
    EXPECT_TRUE(result1.success()) << "Failed 1: " << result1.error();

    auto result2 = executeSql(
        "CREATE TRIGGER after_ins AFTER INSERT ON users "
        "FOR EACH ROW EXECUTE PROCEDURE log_insert()");
    EXPECT_TRUE(result2.success()) << "Failed 2: " << result2.error();

    auto result3 = executeSql(
        "CREATE TRIGGER before_upd BEFORE UPDATE ON users "
        "FOR EACH ROW EXECUTE PROCEDURE validate_update()");
    EXPECT_TRUE(result3.success()) << "Failed 3: " << result3.error();
}

// Test 7: Trigger on non-existent table (should fail)
TEST_F(TriggerTest, TriggerOnNonExistentTable)
{
    std::string sql = "CREATE TRIGGER log_insert AFTER INSERT ON nonexistent "
                      "FOR EACH ROW EXECUTE PROCEDURE log_insert()";

    auto result = executeSql(sql);
    EXPECT_FALSE(result.success()) << "Should fail on non-existent table";
}

// Test 8: DROP non-existent trigger (should fail)
TEST_F(TriggerTest, DropNonExistentTrigger)
{
    auto result = executeSql("DROP TRIGGER nonexistent_trigger");
    EXPECT_FALSE(result.success()) << "Should fail on non-existent trigger";
}

// Test 9: Trigger naming - different names
TEST_F(TriggerTest, TriggerNaming)
{
    createTestTable();

    auto result1 = executeSql(
        "CREATE TRIGGER tr_users_bi BEFORE INSERT ON users "
        "FOR EACH ROW EXECUTE PROCEDURE check()");
    EXPECT_TRUE(result1.success()) << "Failed 1: " << result1.error();

    auto result2 = executeSql(
        "CREATE TRIGGER TR_USERS_AI AFTER INSERT ON users "
        "FOR EACH ROW EXECUTE PROCEDURE log()");
    EXPECT_TRUE(result2.success()) << "Failed 2: " << result2.error();

    auto result3 = executeSql(
        "CREATE TRIGGER users_before_update BEFORE UPDATE ON users "
        "FOR EACH ROW EXECUTE PROCEDURE validate()");
    EXPECT_TRUE(result3.success()) << "Failed 3: " << result3.error();
}

// Test 10: All trigger event types
TEST_F(TriggerTest, AllEventTypes)
{
    createTestTable();

    auto ins_result = executeSql(
        "CREATE TRIGGER tr_insert AFTER INSERT ON users "
        "FOR EACH ROW EXECUTE PROCEDURE proc_insert()");
    EXPECT_TRUE(ins_result.success()) << "INSERT trigger failed: " << ins_result.error();

    auto upd_result = executeSql(
        "CREATE TRIGGER tr_update AFTER UPDATE ON users "
        "FOR EACH ROW EXECUTE PROCEDURE proc_update()");
    EXPECT_TRUE(upd_result.success()) << "UPDATE trigger failed: " << upd_result.error();

    auto del_result = executeSql(
        "CREATE TRIGGER tr_delete AFTER DELETE ON users "
        "FOR EACH ROW EXECUTE PROCEDURE proc_delete()");
    EXPECT_TRUE(del_result.success()) << "DELETE trigger failed: " << del_result.error();
}

// Test 11: Trigger with procedure name variations
TEST_F(TriggerTest, ProcedureNameVariations)
{
    createTestTable();

    auto result1 = executeSql(
        "CREATE TRIGGER tr1 AFTER INSERT ON users "
        "FOR EACH ROW EXECUTE PROCEDURE simple_proc()");
    EXPECT_TRUE(result1.success()) << "Failed 1: " << result1.error();

    auto result2 = executeSql(
        "CREATE TRIGGER tr2 AFTER INSERT ON users "
        "FOR EACH ROW EXECUTE PROCEDURE my_schema.complex_proc()");
    EXPECT_TRUE(result2.success()) << "Failed 2: " << result2.error();

    auto result3 = executeSql(
        "CREATE TRIGGER tr3 AFTER INSERT ON users "
        "FOR EACH ROW EXECUTE PROCEDURE PROC_UPPER_CASE()");
    EXPECT_TRUE(result3.success()) << "Failed 3: " << result3.error();
}

// Test 12: CREATE and DROP multiple times
TEST_F(TriggerTest, CreateDropCycle)
{
    createTestTable();

    for (int i = 0; i < 3; i++)
    {
        auto create_result = executeSql(
            "CREATE TRIGGER test_trigger AFTER INSERT ON users "
            "FOR EACH ROW EXECUTE PROCEDURE test_proc()");
        EXPECT_TRUE(create_result.success())
            << "Iteration " << i << " create failed: " << create_result.error();

        auto drop_result = executeSql("DROP TRIGGER test_trigger");
        EXPECT_TRUE(drop_result.success())
            << "Iteration " << i << " drop failed: " << drop_result.error();
    }
}
