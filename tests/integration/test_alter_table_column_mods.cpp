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
#include <algorithm>
#include <memory>

#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/sblr/query_compiler_v3.h"
#include "scratchbird/sblr/executor.h"
#include "test_helpers.h"

using namespace scratchbird;
using namespace scratchbird::core;
using namespace scratchbird::sblr;
using scratchbird::testing::TestDatabaseFile;

class AlterTableColumnModsTest : public ::testing::Test
{
protected:
    std::unique_ptr<Database> db_;
    std::unique_ptr<QueryCompilerV3> compiler_;
    std::unique_ptr<Executor> executor_;
    std::unique_ptr<TestDatabaseFile> db_file_;
    core::ID schema_id_;

    void SetUp() override
    {
        db_file_ = std::make_unique<TestDatabaseFile>("test_alter_table_column_mods");

        ErrorContext ctx;
        auto status = Database::create(db_file_->path(), 16384, &ctx);
        ASSERT_EQ(status, Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        status = db_->open(db_file_->path(), &ctx);
        ASSERT_EQ(status, Status::OK) << ctx.message;

        CatalogManager::SchemaInfo schema;
        ASSERT_EQ(db_->catalog_manager()->getSchema("PUBLIC", schema, &ctx), Status::OK)
            << "Failed to get PUBLIC schema: " << ctx.message;
        schema_id_ = schema.schema_id;

        compiler_ = std::make_unique<QueryCompilerV3>(db_.get());
        compiler_->setCurrentSchema(schema_id_);
        executor_ = std::make_unique<Executor>(db_.get());
        executor_->setCurrentSchema(schema_id_);
    }

    void TearDown() override
    {
        executor_.reset();
        compiler_.reset();
        db_.reset();
        db_file_.reset();
    }

    ExecutionResult executeSQL(const std::string& sql)
    {
        auto result = compiler_->compile(sql);
        if (!result.success())
        {
            if (!result.errors().empty())
            {
                return ExecutionResult(result.errors().front());
            }
            return ExecutionResult("Compile error");
        }
        return executor_->execute(result.bytecode());
    }
};

TEST_F(AlterTableColumnModsTest, SetNotNullRejectsExistingNulls)
{
    ASSERT_TRUE(executeSQL("CREATE TABLE t_nulls (id INT, val INT)").success());
    ASSERT_TRUE(executeSQL("INSERT INTO t_nulls (id, val) VALUES (1, NULL)").success());

    auto set_not_null = executeSQL("ALTER TABLE t_nulls ALTER COLUMN val SET NOT NULL");
    EXPECT_FALSE(set_not_null.success());
    EXPECT_NE(set_not_null.error().find("NOT NULL"), std::string::npos);

    ASSERT_TRUE(executeSQL("UPDATE t_nulls SET val = 0 WHERE val IS NULL").success());
    auto set_not_null_ok = executeSQL("ALTER TABLE t_nulls ALTER COLUMN val SET NOT NULL");
    EXPECT_TRUE(set_not_null_ok.success()) << set_not_null_ok.error();
}

TEST_F(AlterTableColumnModsTest, SetAndDropDefault)
{
    ASSERT_TRUE(executeSQL("CREATE TABLE t_defaults (id INT, val INT)").success());

    ASSERT_TRUE(executeSQL("ALTER TABLE t_defaults ALTER COLUMN val SET DEFAULT 42").success());
    ASSERT_TRUE(executeSQL("INSERT INTO t_defaults (id) VALUES (1)").success());

    auto select1 = executeSQL("SELECT val FROM t_defaults WHERE id = 1");
    ASSERT_TRUE(select1.success());
    ASSERT_TRUE(select1.hasResultSet());
    ASSERT_EQ(select1.resultSet()->rowCount(), 1u);
    EXPECT_EQ(select1.resultSet()->getValue(0, 0).toString(), "42");

    ASSERT_TRUE(executeSQL("ALTER TABLE t_defaults ALTER COLUMN val DROP DEFAULT").success());
    ASSERT_TRUE(executeSQL("INSERT INTO t_defaults (id) VALUES (2)").success());

    auto select2 = executeSQL("SELECT val FROM t_defaults WHERE id = 2");
    ASSERT_TRUE(select2.success());
    ASSERT_TRUE(select2.hasResultSet());
    ASSERT_EQ(select2.resultSet()->rowCount(), 1u);
    EXPECT_TRUE(select2.resultSet()->getValue(0, 0).isNull());
}

TEST_F(AlterTableColumnModsTest, AlterColumnPositionAffectsSelectStarOrder)
{
    ASSERT_TRUE(executeSQL("CREATE TABLE t_order (a INT, b INT, c INT)").success());
    ASSERT_TRUE(executeSQL("ALTER TABLE t_order ALTER COLUMN c POSITION 1").success());

    auto select = executeSQL("SELECT * FROM t_order");
    ASSERT_TRUE(select.success());
    ASSERT_TRUE(select.hasResultSet());
    ASSERT_EQ(select.resultSet()->columnCount(), 3u);
    EXPECT_EQ(select.resultSet()->columnName(0), "c");
    EXPECT_EQ(select.resultSet()->columnName(1), "a");
    EXPECT_EQ(select.resultSet()->columnName(2), "b");
}

TEST_F(AlterTableColumnModsTest,
       CompatibleWideningChangesSucceedWhileRewriteRequiredPathsFailClosed)
{
    ASSERT_TRUE(executeSQL("CREATE TABLE t_rewrite (id INT, val INT)").success());
    ASSERT_TRUE(executeSQL("INSERT INTO t_rewrite (id, val) VALUES (1, 42)").success());

    auto drop_column = executeSQL("ALTER TABLE t_rewrite DROP COLUMN val");
    EXPECT_FALSE(drop_column.success());
    EXPECT_NE(drop_column.error().find("REWRITE_REQUIRED"), std::string::npos);

    auto alter_type = executeSQL("ALTER TABLE t_rewrite ALTER COLUMN val TYPE BIGINT");
    ASSERT_TRUE(alter_type.success()) << alter_type.error();

    auto insert_after_alter = executeSQL("INSERT INTO t_rewrite (id, val) VALUES (2, 84)");
    ASSERT_TRUE(insert_after_alter.success()) << insert_after_alter.error();

    auto select = executeSQL("SELECT val FROM t_rewrite ORDER BY id");
    ASSERT_TRUE(select.success()) << select.error();
    ASSERT_TRUE(select.hasResultSet());
    ASSERT_EQ(select.resultSet()->rowCount(), 2u);
    EXPECT_EQ(select.resultSet()->getValue(0, 0).toInt64(), 42);
    EXPECT_EQ(select.resultSet()->getValue(1, 0).toInt64(), 84);

    core::ErrorContext ctx;
    core::CatalogManager::TableInfo table_info;
    ASSERT_EQ(db_->catalog_manager()->getTable(schema_id_, "t_rewrite", table_info, &ctx),
              core::Status::OK)
        << ctx.message;

    std::vector<core::CatalogManager::ColumnInfo> columns;
    ASSERT_EQ(db_->catalog_manager()->getColumns(table_info.table_id, columns, &ctx),
              core::Status::OK)
        << ctx.message;

    auto column_it = std::find_if(columns.begin(), columns.end(), [](const auto& column) {
        return column.column_name == "val";
    });
    ASSERT_NE(column_it, columns.end());
    EXPECT_EQ(static_cast<core::DataType>(column_it->data_type), core::DataType::INT64);

    auto add_not_null = executeSQL("ALTER TABLE t_rewrite ADD COLUMN extra INT NOT NULL");
    EXPECT_FALSE(add_not_null.success());
    EXPECT_NE(add_not_null.error().find("backfill"), std::string::npos);
}
