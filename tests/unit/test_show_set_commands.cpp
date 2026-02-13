/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * Tests for Server-Side SHOW and SET Commands
 *
 * This file tests the parser and bytecode generation for
 * Firebird ISQL-compatible SHOW and SET commands.
 *
 * SHOW Commands tested:
 * - SHOW TABLE, SHOW INDEX, SHOW TRIGGER, SHOW PROCEDURE
 * - SHOW FUNCTION, SHOW VIEW, SHOW DOMAIN, SHOW GENERATOR
 * - SHOW SCHEMA, SHOW ROLE, SHOW GRANTS, SHOW CHECKS
 * - SHOW COLLATIONS, SHOW COMMENTS, SHOW DEPENDENCIES
 * - SHOW PACKAGE, SHOW SYSTEM, SHOW SQL DIALECT
 * - SHOW VERSION, SHOW DATABASE
 *
 * SET Commands tested:
 * - SET SQL DIALECT n
 * - SET NAMES charset
 * - SET LOCAL_TIMEOUT n
 */

#include <gtest/gtest.h>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/query_compiler_v3.h"
#include "scratchbird/sblr/opcodes.h"
#include "test_helpers.h"
#include <algorithm>
#include <sstream>

using namespace scratchbird::sblr;
using namespace scratchbird::testing;

class ShowSetCommandsTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        db_file_ = std::make_unique<TestDatabaseFile>("show_set_commands");

        scratchbird::core::ErrorContext ctx;
        auto status = scratchbird::core::Database::create(db_file_->path(), 16384, &ctx);
        ASSERT_EQ(status, scratchbird::core::Status::OK) << ctx.message;

        db_ = std::make_unique<scratchbird::core::Database>();
        status = db_->open(db_file_->path(), &ctx);
        ASSERT_EQ(status, scratchbird::core::Status::OK) << ctx.message;

        auto* catalog = db_->catalog_manager();
        ASSERT_NE(catalog, nullptr);

        scratchbird::core::CatalogManager::SchemaInfo schema_info;
        status = catalog->getSchema("PUBLIC", schema_info, &ctx);
        ASSERT_EQ(status, scratchbird::core::Status::OK) << ctx.message;
        schema_id_ = schema_info.schema_id;

        compiler_ = std::make_unique<QueryCompilerV3>(db_.get());
        compiler_->setCurrentSchema(schema_id_);

        executor_ = std::make_unique<scratchbird::sblr::Executor>(db_.get());
        executor_->setCurrentSchema(schema_id_);

        status = db_->connect(connection_ctx_, &ctx);
        ASSERT_EQ(status, scratchbird::core::Status::OK) << ctx.message;
        connection_ctx_->setCurrentSchemaId(schema_id_);

        system_user_id_ = catalog->getSystemUserId(&ctx);
        connection_ctx_->setCurrentUser(system_user_id_, true);
        scratchbird::core::ConnectionContext::setCurrent(connection_ctx_.get());
        executor_->setConnectionContext(connection_ctx_.get());
    }

    void TearDown() override
    {
        executor_.reset();
        compiler_.reset();
        scratchbird::core::ConnectionContext::setCurrent(nullptr);
        connection_ctx_.reset();
        db_.reset();
        db_file_.reset();
    }

    // Helper to check if compilation succeeds
    bool compileSucceeds(const std::string &sql)
    {
        last_error_.clear();
        auto result = compiler_->compile(sql);
        if (!result.success())
        {
            if (!result.errors().empty())
            {
                last_error_ = result.errors().front();
            }
            else
            {
                last_error_ = "Compilation failed";
            }
            return false;
        }
        return true;
    }

    // Helper to generate bytecode
    std::vector<uint8_t> generateBytecode(const std::string &sql)
    {
        last_error_.clear();
        auto result = compiler_->compile(sql);
        if (!result.success())
        {
            if (!result.errors().empty())
            {
                last_error_ = result.errors().front();
            }
            else
            {
                last_error_ = "Compilation failed";
            }
            return {};
        }
        return result.bytecode();
    }

    // Helper to check if V3 bytecode contains an opcode
    bool bytecodeContainsV3Opcode(const std::vector<uint8_t>& bc,
                                  scratchbird::sblr::v3::Opcode opcode)
    {
        return scratchbird::testing::v3BytecodeContainsOpcode(bc, opcode);
    }

    scratchbird::sblr::ExecutionResult compileAndExecute(const std::string& sql)
    {
        auto result = compiler_->compile(sql);
        if (!result.success())
        {
            std::string errors;
            for (const auto& err : result.errors())
            {
                errors += err + "\n";
            }
            return scratchbird::sblr::ExecutionResult("Compilation failed: " + errors);
        }
        return executor_->execute(result.bytecode());
    }

protected:
    std::unique_ptr<TestDatabaseFile> db_file_;
    std::unique_ptr<scratchbird::core::Database> db_;
    std::unique_ptr<QueryCompilerV3> compiler_;
    std::unique_ptr<scratchbird::sblr::Executor> executor_;
    std::unique_ptr<scratchbird::core::ConnectionContext> connection_ctx_;
    scratchbird::core::ID schema_id_{};
    scratchbird::core::ID system_user_id_{};
    std::string last_error_;
};

// =============================================================================
// SHOW TABLE Command Tests
// =============================================================================

TEST_F(ShowSetCommandsTest, ShowTableParsing)
{
    EXPECT_TRUE(compileSucceeds("SHOW TABLE users"));
    EXPECT_TRUE(compileSucceeds("SHOW TABLE my_table"));
    EXPECT_TRUE(compileSucceeds("SHOW TABLE \"CaseSensitiveTable\""));
}

TEST_F(ShowSetCommandsTest, ShowTableBytecode)
{
    auto bc = generateBytecode("SHOW TABLE users");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsV3Opcode(bc, scratchbird::sblr::v3::Opcode::SBLR3_SHOW_TABLE));
}

// =============================================================================
// SHOW INDEX Command Tests
// =============================================================================

TEST_F(ShowSetCommandsTest, ShowIndexParsing)
{
    EXPECT_TRUE(compileSucceeds("SHOW INDEX idx_users_name"));
    EXPECT_TRUE(compileSucceeds("SHOW INDEX pk_table"));
}

TEST_F(ShowSetCommandsTest, ShowIndexBytecode)
{
    auto bc = generateBytecode("SHOW INDEX idx_test");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsV3Opcode(bc, scratchbird::sblr::v3::Opcode::SBLR3_SHOW_INDEX));
}

// =============================================================================
// SHOW TRIGGER Command Tests
// =============================================================================

TEST_F(ShowSetCommandsTest, ShowTriggerParsing)
{
    EXPECT_TRUE(compileSucceeds("SHOW TRIGGER trg_before_insert"));
    EXPECT_TRUE(compileSucceeds("SHOW TRIGGER my_trigger"));
}

TEST_F(ShowSetCommandsTest, ShowTriggerBytecode)
{
    auto bc = generateBytecode("SHOW TRIGGER trg_test");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsV3Opcode(bc, scratchbird::sblr::v3::Opcode::SBLR3_SHOW_TRIGGER));
}

// =============================================================================
// SHOW PROCEDURE Command Tests
// =============================================================================

TEST_F(ShowSetCommandsTest, ShowProcedureParsing)
{
    EXPECT_TRUE(compileSucceeds("SHOW PROCEDURE sp_get_users"));
    EXPECT_TRUE(compileSucceeds("SHOW PROCEDURE my_proc"));
}

TEST_F(ShowSetCommandsTest, ShowProcedureBytecode)
{
    auto bc = generateBytecode("SHOW PROCEDURE sp_test");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsV3Opcode(bc, scratchbird::sblr::v3::Opcode::SBLR3_SHOW_PROCEDURE));
}

// =============================================================================
// SHOW FUNCTION Command Tests
// =============================================================================

TEST_F(ShowSetCommandsTest, ShowFunctionParsing)
{
    EXPECT_TRUE(compileSucceeds("SHOW FUNCTION fn_calculate"));
    EXPECT_TRUE(compileSucceeds("SHOW FUNCTION my_func"));
}

TEST_F(ShowSetCommandsTest, ShowFunctionBytecode)
{
    auto bc = generateBytecode("SHOW FUNCTION fn_test");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsV3Opcode(bc, scratchbird::sblr::v3::Opcode::SBLR3_SHOW_FUNCTION));
}

// =============================================================================
// SHOW VIEW Command Tests
// =============================================================================

TEST_F(ShowSetCommandsTest, ShowViewParsing)
{
    EXPECT_TRUE(compileSucceeds("SHOW VIEW v_active_users"));
    EXPECT_TRUE(compileSucceeds("SHOW VIEW my_view"));
}

TEST_F(ShowSetCommandsTest, ShowViewBytecode)
{
    auto bc = generateBytecode("SHOW VIEW v_test");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsV3Opcode(bc, scratchbird::sblr::v3::Opcode::SBLR3_SHOW_VIEW));
}

// =============================================================================
// SHOW DOMAIN Command Tests
// =============================================================================

TEST_F(ShowSetCommandsTest, ShowDomainParsing)
{
    EXPECT_TRUE(compileSucceeds("SHOW DOMAIN d_email"));
    EXPECT_TRUE(compileSucceeds("SHOW DOMAIN my_domain"));
}

TEST_F(ShowSetCommandsTest, ShowDomainBytecode)
{
    auto bc = generateBytecode("SHOW DOMAIN d_test");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsV3Opcode(bc, scratchbird::sblr::v3::Opcode::SBLR3_SHOW_DOMAIN));
}

// =============================================================================
// SHOW GENERATOR Command Tests
// =============================================================================

TEST_F(ShowSetCommandsTest, ShowGeneratorParsing)
{
    EXPECT_TRUE(compileSucceeds("SHOW GENERATOR gen_user_id"));
    EXPECT_TRUE(compileSucceeds("SHOW GENERATOR my_sequence"));
}

TEST_F(ShowSetCommandsTest, ShowGeneratorBytecode)
{
    auto bc = generateBytecode("SHOW GENERATOR gen_test");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsV3Opcode(bc, scratchbird::sblr::v3::Opcode::SBLR3_SHOW_GENERATOR));
}

// =============================================================================
// SHOW SCHEMA Command Tests
// =============================================================================

TEST_F(ShowSetCommandsTest, ShowSchemaParsing)
{
    EXPECT_TRUE(compileSucceeds("SHOW SCHEMA"));
    EXPECT_TRUE(compileSucceeds("SHOW SCHEMA public"));
    EXPECT_TRUE(compileSucceeds("SHOW SCHEMA my_schema"));
}

TEST_F(ShowSetCommandsTest, ShowSchemaBytecode)
{
    auto bc = generateBytecode("SHOW SCHEMA");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsV3Opcode(bc, scratchbird::sblr::v3::Opcode::SBLR3_SHOW_SCHEMA));
}

// =============================================================================
// SHOW ROLE Command Tests
// =============================================================================

TEST_F(ShowSetCommandsTest, ShowRoleParsing)
{
    EXPECT_TRUE(compileSucceeds("SHOW ROLE admin"));
    EXPECT_TRUE(compileSucceeds("SHOW ROLE my_role"));
}

TEST_F(ShowSetCommandsTest, ShowRoleBytecode)
{
    auto bc = generateBytecode("SHOW ROLE test_role");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsV3Opcode(bc, scratchbird::sblr::v3::Opcode::SBLR3_SHOW_ROLE));
}

// =============================================================================
// SHOW GRANTS Command Tests
// =============================================================================

TEST_F(ShowSetCommandsTest, ShowGrantsParsing)
{
    EXPECT_TRUE(compileSucceeds("SHOW GRANTS"));
    EXPECT_TRUE(compileSucceeds("SHOW GRANTS FOR users"));
    EXPECT_TRUE(compileSucceeds("SHOW GRANTS FOR my_table"));
}

TEST_F(ShowSetCommandsTest, ShowGrantsBytecode)
{
    auto bc = generateBytecode("SHOW GRANTS");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsV3Opcode(bc, scratchbird::sblr::v3::Opcode::SBLR3_SHOW_GRANTS));

    bc = generateBytecode("SHOW GRANTS FOR users");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsV3Opcode(bc, scratchbird::sblr::v3::Opcode::SBLR3_SHOW_GRANTS));
}

// =============================================================================
// SHOW CHECKS Command Tests
// =============================================================================

TEST_F(ShowSetCommandsTest, ShowChecksParsing)
{
    EXPECT_TRUE(compileSucceeds("SHOW CHECKS"));
    EXPECT_TRUE(compileSucceeds("SHOW CHECKS users"));
    EXPECT_TRUE(compileSucceeds("SHOW CHECKS my_table"));
}

TEST_F(ShowSetCommandsTest, ShowChecksBytecode)
{
    auto bc = generateBytecode("SHOW CHECKS users");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsV3Opcode(bc, scratchbird::sblr::v3::Opcode::SBLR3_SHOW_CHECKS));

    bc = generateBytecode("SHOW CHECKS");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsV3Opcode(bc, scratchbird::sblr::v3::Opcode::SBLR3_SHOW_CHECKS));
}

// =============================================================================
// SHOW COLLATIONS Command Tests
// =============================================================================

TEST_F(ShowSetCommandsTest, ShowCollationsParsing)
{
    EXPECT_TRUE(compileSucceeds("SHOW COLLATIONS"));
    EXPECT_TRUE(compileSucceeds("SHOW COLLATIONS LIKE 'UTF%'"));
}

TEST_F(ShowSetCommandsTest, ShowCollationsBytecode)
{
    auto bc = generateBytecode("SHOW COLLATIONS");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsV3Opcode(bc, scratchbird::sblr::v3::Opcode::SBLR3_SHOW_COLLATIONS));
}

// =============================================================================
// SHOW COMMENTS Command Tests
// =============================================================================

TEST_F(ShowSetCommandsTest, ShowCommentsParsing)
{
    EXPECT_TRUE(compileSucceeds("SHOW COMMENTS users"));
    EXPECT_TRUE(compileSucceeds("SHOW COMMENTS my_object"));
}

TEST_F(ShowSetCommandsTest, ShowCommentsBytecode)
{
    auto bc = generateBytecode("SHOW COMMENTS users");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsV3Opcode(bc, scratchbird::sblr::v3::Opcode::SBLR3_SHOW_COMMENTS));
}

// =============================================================================
// SHOW DEPENDENCIES Command Tests
// =============================================================================

TEST_F(ShowSetCommandsTest, ShowDependenciesParsing)
{
    EXPECT_TRUE(compileSucceeds("SHOW DEPENDENCIES users"));
    EXPECT_TRUE(compileSucceeds("SHOW DEPENDENCIES my_view"));
}

TEST_F(ShowSetCommandsTest, ShowDependenciesBytecode)
{
    auto bc = generateBytecode("SHOW DEPENDENCIES users");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsV3Opcode(bc, scratchbird::sblr::v3::Opcode::SBLR3_SHOW_DEPENDENCIES));
}

// =============================================================================
// SHOW PACKAGE Command Tests
// =============================================================================

TEST_F(ShowSetCommandsTest, ShowPackageParsing)
{
    EXPECT_TRUE(compileSucceeds("SHOW PACKAGE pkg_utils"));
    EXPECT_TRUE(compileSucceeds("SHOW PACKAGE my_package"));
}

TEST_F(ShowSetCommandsTest, ShowPackageBytecode)
{
    auto bc = generateBytecode("SHOW PACKAGE pkg_test");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsV3Opcode(bc, scratchbird::sblr::v3::Opcode::SBLR3_SHOW_PACKAGE));
}

// =============================================================================
// SHOW SYSTEM Command Tests
// =============================================================================

TEST_F(ShowSetCommandsTest, ShowSystemParsing)
{
    EXPECT_TRUE(compileSucceeds("SHOW SYSTEM"));
}

TEST_F(ShowSetCommandsTest, ShowSystemBytecode)
{
    auto bc = generateBytecode("SHOW SYSTEM");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsV3Opcode(bc, scratchbird::sblr::v3::Opcode::SBLR3_SHOW_SYSTEM));
}

// =============================================================================
// SHOW SQL DIALECT Command Tests
// =============================================================================

TEST_F(ShowSetCommandsTest, ShowSqlDialectParsing)
{
    EXPECT_TRUE(compileSucceeds("SHOW SQL DIALECT"));
}

TEST_F(ShowSetCommandsTest, ShowSqlDialectBytecode)
{
    auto bc = generateBytecode("SHOW SQL DIALECT");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsV3Opcode(bc, scratchbird::sblr::v3::Opcode::SBLR3_SHOW_SQL_DIALECT));
}

// =============================================================================
// SHOW VERSION Command Tests
// =============================================================================

TEST_F(ShowSetCommandsTest, ShowVersionParsing)
{
    EXPECT_TRUE(compileSucceeds("SHOW VERSION"));
}

TEST_F(ShowSetCommandsTest, ShowVersionBytecode)
{
    auto bc = generateBytecode("SHOW VERSION");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsV3Opcode(bc, scratchbird::sblr::v3::Opcode::SBLR3_SHOW_VERSION));
}

// =============================================================================
// SHOW DATABASE Command Tests
// =============================================================================

TEST_F(ShowSetCommandsTest, ShowDatabaseParsing)
{
    EXPECT_TRUE(compileSucceeds("SHOW DATABASE"));
}

TEST_F(ShowSetCommandsTest, ShowDatabaseBytecode)
{
    auto bc = generateBytecode("SHOW DATABASE");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsV3Opcode(bc, scratchbird::sblr::v3::Opcode::SBLR3_SHOW_DATABASE));
}

// =============================================================================
// SET SQL DIALECT Command Tests
// =============================================================================

TEST_F(ShowSetCommandsTest, SetSqlDialectParsing)
{
    EXPECT_TRUE(compileSucceeds("SET SQL DIALECT 1"));
    EXPECT_TRUE(compileSucceeds("SET SQL DIALECT 2"));
    EXPECT_TRUE(compileSucceeds("SET SQL DIALECT 3"));
}


TEST_F(ShowSetCommandsTest, SetSqlDialectBytecode)
{
    auto bc = generateBytecode("SET SQL DIALECT 3");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsV3Opcode(bc, scratchbird::sblr::v3::Opcode::SBLR3_SET_SQL_DIALECT));
}

// =============================================================================
// SET NAMES Command Tests
// =============================================================================

TEST_F(ShowSetCommandsTest, SetNamesParsing)
{
    EXPECT_TRUE(compileSucceeds("SET NAMES UTF8"));
    EXPECT_TRUE(compileSucceeds("SET NAMES ISO8859_1"));
    EXPECT_TRUE(compileSucceeds("SET NAMES WIN1252"));
}


TEST_F(ShowSetCommandsTest, SetNamesBytecode)
{
    auto bc = generateBytecode("SET NAMES UTF8");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsV3Opcode(bc, scratchbird::sblr::v3::Opcode::SBLR3_SET_NAMES));
}

// =============================================================================
// SET LOCAL_TIMEOUT Command Tests
// =============================================================================

TEST_F(ShowSetCommandsTest, SetLocalTimeoutParsing)
{
    EXPECT_TRUE(compileSucceeds("SET LOCAL_TIMEOUT 30"));
    EXPECT_TRUE(compileSucceeds("SET LOCAL_TIMEOUT 0"));
    EXPECT_TRUE(compileSucceeds("SET LOCAL_TIMEOUT 3600"));
}


TEST_F(ShowSetCommandsTest, SetLocalTimeoutBytecode)
{
    auto bc = generateBytecode("SET LOCAL_TIMEOUT 30");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsV3Opcode(bc, scratchbird::sblr::v3::Opcode::SBLR3_SET_LOCAL_TIMEOUT));
}

// =============================================================================
// Original SHOW Commands (verify they still work)
// =============================================================================

TEST_F(ShowSetCommandsTest, ShowTablesStillWorks)
{
    EXPECT_TRUE(compileSucceeds("SHOW TABLES"));
    auto bc = generateBytecode("SHOW TABLES");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsV3Opcode(bc, scratchbird::sblr::v3::Opcode::SBLR3_SHOW_TABLES));
}

TEST_F(ShowSetCommandsTest, ShowDatabasesStillWorks)
{
    EXPECT_TRUE(compileSucceeds("SHOW DATABASES"));
    auto bc = generateBytecode("SHOW DATABASES");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsV3Opcode(bc, scratchbird::sblr::v3::Opcode::SBLR3_SHOW_DATABASES));
}

TEST_F(ShowSetCommandsTest, ShowColumnsStillWorks)
{
    EXPECT_TRUE(compileSucceeds("SHOW COLUMNS FROM users"));
    auto bc = generateBytecode("SHOW COLUMNS FROM users");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsV3Opcode(bc, scratchbird::sblr::v3::Opcode::SBLR3_SHOW_COLUMNS));
}

TEST_F(ShowSetCommandsTest, ShowIndexesStillWorks)
{
    EXPECT_TRUE(compileSucceeds("SHOW INDEXES FROM users"));
    auto bc = generateBytecode("SHOW INDEXES FROM users");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsV3Opcode(bc, scratchbird::sblr::v3::Opcode::SBLR3_SHOW_INDEXES));
}

TEST_F(ShowSetCommandsTest, ShowCreateTableStillWorks)
{
    EXPECT_TRUE(compileSucceeds("SHOW CREATE TABLE users"));
    auto bc = generateBytecode("SHOW CREATE TABLE users");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsV3Opcode(bc, scratchbird::sblr::v3::Opcode::SBLR3_SHOW_CREATE_TABLE));
}

// =============================================================================
// Edge Cases and Error Handling
// =============================================================================

TEST_F(ShowSetCommandsTest, ShowTableAllowsOptionalObjectName)
{
    // SHOW TABLE without object name shows all tables (Firebird ISQL behavior)
    EXPECT_TRUE(compileSucceeds("SHOW TABLE"));
    EXPECT_TRUE(compileSucceeds("SHOW TABLE users"));
}

TEST_F(ShowSetCommandsTest, ShowIndexAllowsOptionalObjectName)
{
    // SHOW INDEX without object name shows all indexes (Firebird ISQL behavior)
    EXPECT_TRUE(compileSucceeds("SHOW INDEX"));
    EXPECT_TRUE(compileSucceeds("SHOW INDEX idx_users_email"));
}

TEST_F(ShowSetCommandsTest, SetSqlDialectRequiresNumber)
{
    EXPECT_FALSE(compileSucceeds("SET SQL DIALECT"));
    EXPECT_FALSE(compileSucceeds("SET SQL DIALECT abc"));
}

TEST_F(ShowSetCommandsTest, SetNamesRequiresCharset)
{
    EXPECT_FALSE(compileSucceeds("SET NAMES"));
}

TEST_F(ShowSetCommandsTest, SetLocalTimeoutRequiresNumber)
{
    EXPECT_FALSE(compileSucceeds("SET LOCAL_TIMEOUT"));
    EXPECT_FALSE(compileSucceeds("SET LOCAL_TIMEOUT abc"));
}

// =============================================================================
// Metadata Redaction Regression
// =============================================================================

TEST_F(ShowSetCommandsTest, ShowTablesAndColumnsRedactWithoutSelect)
{
    auto* catalog = db_->catalog_manager();
    ASSERT_NE(catalog, nullptr);

    scratchbird::core::ErrorContext ctx;
    scratchbird::core::ID visible_table_id;
    scratchbird::core::ID hidden_table_id;

    scratchbird::core::CatalogManager::ColumnInfo id_col;
    id_col.column_name = "id";
    id_col.data_type = static_cast<uint16_t>(scratchbird::core::DataType::INT32);
    id_col.nullable = false;

    scratchbird::core::CatalogManager::ColumnInfo name_col;
    name_col.column_name = "name";
    name_col.data_type = static_cast<uint16_t>(scratchbird::core::DataType::TEXT);
    name_col.nullable = true;

    std::vector<scratchbird::core::CatalogManager::ColumnInfo> visible_cols{id_col, name_col};
    ASSERT_EQ(catalog->createTable(schema_id_, "visible_table", visible_cols,
                                   visible_table_id, 0, &ctx),
              scratchbird::core::Status::OK) << ctx.message;

    scratchbird::core::CatalogManager::ColumnInfo secret_col = name_col;
    secret_col.column_name = "secret";
    std::vector<scratchbird::core::CatalogManager::ColumnInfo> hidden_cols{id_col, secret_col};
    ASSERT_EQ(catalog->createTable(schema_id_, "hidden_table", hidden_cols,
                                   hidden_table_id, 0, &ctx),
              scratchbird::core::Status::OK) << ctx.message;

    scratchbird::core::ID user_id;
    auto status = catalog->createUser("viewer", "", schema_id_, false, user_id, &ctx);
    if (status == scratchbird::core::Status::FILE_EXISTS)
    {
        scratchbird::core::CatalogManager::UserInfo user_info;
        ASSERT_EQ(catalog->getUserByName("viewer", user_info, &ctx),
                  scratchbird::core::Status::OK) << ctx.message;
        user_id = user_info.user_id;
    }
    else
    {
        ASSERT_EQ(status, scratchbird::core::Status::OK) << ctx.message;
    }

    ASSERT_EQ(catalog->grantPermission(
                  visible_table_id,
                  scratchbird::core::CatalogManager::PermissionObjectType::TABLE,
                  user_id,
                  scratchbird::core::CatalogManager::GranteeType::USER,
                  static_cast<uint32_t>(scratchbird::core::CatalogManager::Privilege::SELECT),
                  false,
                  system_user_id_,
                  &ctx),
              scratchbird::core::Status::OK) << ctx.message;

    connection_ctx_->setCurrentUser(user_id, false);

    auto show_tables = compileAndExecute("SHOW TABLES");
    ASSERT_TRUE(show_tables.success()) << show_tables.error();
    ASSERT_TRUE(show_tables.hasResultSet());

    auto* tables_rs = show_tables.resultSet();
    std::vector<std::string> table_names;
    for (size_t row = 0; row < tables_rs->rowCount(); ++row)
    {
        table_names.push_back(tables_rs->getValue(row, 0).toString());
    }
    EXPECT_NE(std::find(table_names.begin(), table_names.end(), "visible_table"), table_names.end());
    EXPECT_EQ(std::find(table_names.begin(), table_names.end(), "hidden_table"), table_names.end());

    auto show_hidden_cols = compileAndExecute("SHOW COLUMNS FROM hidden_table");
    ASSERT_TRUE(show_hidden_cols.success()) << show_hidden_cols.error();
    ASSERT_TRUE(show_hidden_cols.hasResultSet());
    auto* hidden_cols_rs = show_hidden_cols.resultSet();
    ASSERT_GT(hidden_cols_rs->rowCount(), 0u);
    EXPECT_EQ(hidden_cols_rs->getValue(0, 0).toString(), "Redacted");

    auto show_visible_cols = compileAndExecute("SHOW COLUMNS FROM visible_table");
    ASSERT_TRUE(show_visible_cols.success()) << show_visible_cols.error();
    ASSERT_TRUE(show_visible_cols.hasResultSet());
    auto* visible_cols_rs = show_visible_cols.resultSet();
    ASSERT_GT(visible_cols_rs->rowCount(), 0u);
    for (size_t row = 0; row < visible_cols_rs->rowCount(); ++row)
    {
        EXPECT_NE(visible_cols_rs->getValue(row, 0).toString(), "Redacted");
    }
}
