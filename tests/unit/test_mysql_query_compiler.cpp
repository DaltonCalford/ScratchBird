/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "gtest/gtest.h"

#include "scratchbird/core/database.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/sblr/bytecode_validator.h"
#include "scratchbird/sblr/mysql_query_compiler.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/v3_container.h"
#include "scratchbird/sblr/v3_codec.h"
#include "scratchbird/sblr/v3_payloads.h"
#include "scratchbird/sblr/v3_opcodes.generated.h"
#include "test_helpers.h"

#include <algorithm>
#include <filesystem>

using namespace scratchbird;
using scratchbird::testing::TestDatabaseFile;

namespace {
#pragma pack(push, 1)
struct SchemaRecordContract {
    core::ID schema_id;
    core::ID parent_schema_id;
    char schema_name[512];
    core::ID owner_id;
    core::ID default_tablespace_id;
    uint32_t permissions;
    core::ID default_charset_id;
    uint8_t name_is_delimited;
    uint8_t reserved[7];
    uint32_t default_collation_id;
    core::ID acl_oid;
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};

struct CatalogRootPageContract {
    core::PageHeader header;
    uint32_t schema_count;
    uint32_t table_count;
    uint32_t schemas_page;
};
#pragma pack(pop)

auto readSchemasCatalogPageId(core::Database& db, core::ErrorContext* ctx) -> uint32_t {
    auto* buffer_pool = db.buffer_pool();
    if (buffer_pool == nullptr) {
        SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT, "buffer pool unavailable");
        return 0;
    }

    void* page_buffer = nullptr;
    core::Status status = buffer_pool->pinPage(core::BOOTSTRAP_PAGE_CATALOG_ROOT,
                                               &page_buffer,
                                               ctx);
    if (status != core::Status::OK) {
        return 0;
    }

    auto* root = reinterpret_cast<CatalogRootPageContract*>(page_buffer);
    const uint32_t schemas_page_id = root->schemas_page;
    status = buffer_pool->unpinPage(core::BOOTSTRAP_PAGE_CATALOG_ROOT, false, ctx);
    if (status != core::Status::OK) {
        return 0;
    }

    if (schemas_page_id == 0) {
        SET_ERROR_CONTEXT(ctx, core::Status::NOT_FOUND, "Schemas catalog page not initialized");
    }
    return schemas_page_id;
}

core::Status invalidateSchemaRecordOnDisk(core::Database& db,
                                          const core::ID& schema_id,
                                          core::ErrorContext* ctx) {
    auto* buffer_pool = db.buffer_pool();
    if (buffer_pool == nullptr) {
        SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT, "buffer pool unavailable");
        return core::Status::INVALID_ARGUMENT;
    }

    uint32_t current_page_id = readSchemasCatalogPageId(db, ctx);
    if (current_page_id == 0) {
        return ctx != nullptr ? ctx->code : core::Status::NOT_FOUND;
    }

    while (current_page_id != 0) {
        void* page_buffer = nullptr;
        core::Status status = buffer_pool->pinPage(current_page_id, &page_buffer, ctx);
        if (status != core::Status::OK) {
            return status;
        }

        auto* heap = reinterpret_cast<core::CatalogHeapPage*>(page_buffer);
        uint32_t offset = sizeof(core::CatalogHeapPage);
        for (uint32_t i = 0; i < heap->record_count; ++i) {
            auto* record = reinterpret_cast<SchemaRecordContract*>(
                reinterpret_cast<uint8_t*>(page_buffer) + offset);
            if (record->is_valid != 0 && record->schema_id == schema_id) {
                record->is_valid = 0;
                heap->header.generation++;
                return buffer_pool->unpinPage(current_page_id, true, ctx);
            }
            offset += sizeof(SchemaRecordContract);
        }

        const uint32_t next_page_id = heap->next_page;
        status = buffer_pool->unpinPage(current_page_id, false, ctx);
        if (status != core::Status::OK) {
            return status;
        }
        current_page_id = next_page_id;
    }

    SET_ERROR_CONTEXT(ctx, core::Status::NOT_FOUND, "Schema record not found on disk");
    return core::Status::NOT_FOUND;
}
} // namespace

class MySQLQueryCompilerTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_db_file_ = std::make_unique<TestDatabaseFile>("test_mysql_compiler");

        core::ErrorContext ctx;
        ASSERT_EQ(core::Database::create(test_db_file_->path(), 16384, &ctx), core::Status::OK)
            << ctx.message;
        ASSERT_EQ(db_.open(test_db_file_->path(), &ctx), core::Status::OK) << ctx.message;

        ASSERT_EQ(db_.connect(conn_ctx_, &ctx), core::Status::OK) << ctx.message;
        conn_ctx_->set_dialect_tag("MYSQL");
        core::CatalogManager::SchemaInfo public_schema_info;
        ASSERT_EQ(db_.catalog_manager()->getSchema("public", public_schema_info, &ctx),
                  core::Status::OK) << ctx.message;
        conn_ctx_->setCurrentSchemaId(public_schema_info.schema_id);
        auto system_user_id = db_.catalog_manager()->getSystemUserId(&ctx);
        conn_ctx_->setCurrentUser(system_user_id, true);
        core::ConnectionContext::setCurrent(conn_ctx_.get());

        executor_ = std::make_unique<sblr::Executor>(&db_);
        executor_->setConnectionContext(conn_ctx_.get());
    }

    void TearDown() override {
        core::ConnectionContext::setCurrent(nullptr);
        executor_.reset();
        conn_ctx_.reset();
        db_.close();
        test_db_file_.reset();
    }

    std::unique_ptr<TestDatabaseFile> test_db_file_;

    sblr::ExecutionResult compileAndExecute(const std::string& sql) {
        return compileAndExecuteWithDefaultSchema(sql, "");
    }

    sblr::ExecutionResult compileAndExecuteWithDefaultSchema(const std::string& sql,
                                                             const std::string& default_schema) {
        sblr::MySQLQueryCompiler compiler(&db_);
        if (!default_schema.empty()) {
            compiler.setDefaultSchema(default_schema);
        }
        auto compile_result = compiler.compile(sql);
        if (!compile_result.success()) {
            std::string errors;
            for (const auto& err : compile_result.errors()) {
                errors += err + "\n";
            }
            return sblr::ExecutionResult("Compilation failed: " + errors);
        }
        return executor_->execute(compile_result.bytecode());
    }

    core::Database db_;
    std::unique_ptr<core::ConnectionContext> conn_ctx_;
    std::unique_ptr<sblr::Executor> executor_;
};

TEST_F(MySQLQueryCompilerTest, CompileAndExecuteSelectLiteral) {
    auto result = compileAndExecute("SELECT 1 FROM dual");
    ASSERT_TRUE(result.success()) << result.error();
}

TEST_F(MySQLQueryCompilerTest, CompileSetVariableAcceptsMySqlBooleanLiterals) {
    sblr::MySQLQueryCompiler compiler(&db_);

    auto off_result = compiler.compile(
        "SET @@global.binlog_expire_logs_auto_purge = OFF");
    ASSERT_TRUE(off_result.success()) << off_result.errors().front();

    auto on_result = compiler.compile(
        "SET @@global.binlog_expire_logs_auto_purge = ON");
    ASSERT_TRUE(on_result.success()) << on_result.errors().front();
}

TEST_F(MySQLQueryCompilerTest, CompileSetVariableAcceptsEnumStyleKeywordValues) {
    sblr::MySQLQueryCompiler compiler(&db_);

    auto row_result = compiler.compile("SET @@session.binlog_format = ROW");
    ASSERT_TRUE(row_result.success()) << row_result.errors().front();

    auto full_result = compiler.compile("SET @@session.binlog_row_image = FULL");
    ASSERT_TRUE(full_result.success()) << full_result.errors().front();

    auto mixed_result = compiler.compile("SET @@session.binlog_format = MIXED");
    ASSERT_TRUE(mixed_result.success()) << mixed_result.errors().front();
}

TEST_F(MySQLQueryCompilerTest, ShowVariablesLikeVersionCommentCompilesAndExecutes) {
    constexpr const char* kMySqlRootSchema = "emulated.mysql.localhost.databases.main";

    sblr::MySQLQueryCompiler compiler(&db_);
    compiler.setDefaultSchema(kMySqlRootSchema);

    auto compile_result = compiler.compile("SHOW VARIABLES LIKE 'version_comment'");
    ASSERT_TRUE(compile_result.success()) << compile_result.errors().front();

    core::ErrorContext validation_ctx;
    ASSERT_EQ(scratchbird::sblr::validateBytecode(compile_result.bytecode(), &validation_ctx),
              core::Status::OK)
        << validation_ctx.message;

    auto execute_result = executor_->execute(compile_result.bytecode());
    ASSERT_TRUE(execute_result.success()) << execute_result.error();
    ASSERT_TRUE(execute_result.hasResultSet());

    sblr::MySQLQueryCompiler remote_compiler(nullptr);
    remote_compiler.setDefaultSchema(kMySqlRootSchema);

    auto remote_compile_result =
        remote_compiler.compile("SHOW VARIABLES LIKE 'version_comment'");
    ASSERT_TRUE(remote_compile_result.success()) << remote_compile_result.errors().front();

    core::ErrorContext remote_validation_ctx;
    ASSERT_EQ(scratchbird::sblr::validateBytecode(remote_compile_result.bytecode(),
                                                  &remote_validation_ctx),
              core::Status::OK)
        << remote_validation_ctx.message;

    auto remote_execute_result = executor_->execute(remote_compile_result.bytecode());
    ASSERT_TRUE(remote_execute_result.success()) << remote_execute_result.error();
    ASSERT_TRUE(remote_execute_result.hasResultSet());
}

TEST_F(MySQLQueryCompilerTest, CreateUserSupportsIdentifiedWithPluginSyntax) {
    auto create_result = compileAndExecute(
        "CREATE USER 'plugin_user'@'localhost' IDENTIFIED WITH 'sha256_password'");
    ASSERT_TRUE(create_result.success()) << create_result.error();

    auto drop_result = compileAndExecute("DROP USER 'plugin_user'@'localhost'");
    ASSERT_TRUE(drop_result.success()) << drop_result.error();
}

TEST_F(MySQLQueryCompilerTest, SetPasswordAndRenameUserSupportMySqlAccountSyntax) {
    auto create_result = compileAndExecute(
        "CREATE USER 'legacy_user'@'localhost' IDENTIFIED BY 'abc'");
    ASSERT_TRUE(create_result.success()) << create_result.error();

    auto set_password_result = compileAndExecute(
        "SET PASSWORD FOR 'legacy_user'@'localhost' = 'def'");
    ASSERT_TRUE(set_password_result.success()) << set_password_result.error();

    auto rename_result = compileAndExecute(
        "RENAME USER 'legacy_user'@'localhost' TO 'renamed_user'@'localhost'");
    ASSERT_TRUE(rename_result.success()) << rename_result.error();

    auto drop_result = compileAndExecute("DROP USER 'renamed_user'@'localhost'");
    ASSERT_TRUE(drop_result.success()) << drop_result.error();
}

TEST_F(MySQLQueryCompilerTest, CreateAndAlterUserAllowEmptyPasswordsInMySqlMode) {
    auto create_result = compileAndExecute(
        "CREATE USER 'empty_pwd_user'@'localhost' IDENTIFIED BY ''");
    ASSERT_TRUE(create_result.success()) << create_result.error();

    auto alter_result = compileAndExecute(
        "ALTER USER 'empty_pwd_user'@'localhost' IDENTIFIED BY ''");
    ASSERT_TRUE(alter_result.success()) << alter_result.error();

    auto drop_result = compileAndExecute("DROP USER 'empty_pwd_user'@'localhost'");
    ASSERT_TRUE(drop_result.success()) << drop_result.error();
}

TEST_F(MySQLQueryCompilerTest, AutoIncrementMixedExplicitAndDefaultRowsAdvanceCleanly) {
    auto create_result = compileAndExecute(
        "CREATE TABLE t_auto_mix ("
        "id INT AUTO_INCREMENT PRIMARY KEY, "
        "note VARCHAR(16) DEFAULT 'hello')");
    ASSERT_TRUE(create_result.success()) << create_result.error();

    auto insert_result = compileAndExecute(
        "INSERT INTO t_auto_mix VALUES "
        "(DEFAULT, DEFAULT), "
        "(DEFAULT, DEFAULT), "
        "(4, 'explicit'), "
        "(DEFAULT, DEFAULT)");
    ASSERT_TRUE(insert_result.success()) << insert_result.error();

    auto select_result = compileAndExecute(
        "SELECT id, note FROM t_auto_mix ORDER BY id");
    ASSERT_TRUE(select_result.success()) << select_result.error();
    ASSERT_TRUE(select_result.hasResultSet());

    auto* rs = select_result.resultSet();
    ASSERT_EQ(rs->rowCount(), 4u);
    EXPECT_EQ(rs->getValue(0, 0).toInt64(), 1);
    EXPECT_EQ(rs->getValue(1, 0).toInt64(), 2);
    EXPECT_EQ(rs->getValue(2, 0).toInt64(), 4);
    EXPECT_EQ(rs->getValue(3, 0).toInt64(), 5);
    EXPECT_EQ(rs->getValue(2, 1).toString(), "explicit");
}

TEST_F(MySQLQueryCompilerTest, AutoIncrementZeroUsesGeneratedValuesByDefault) {
    auto create_result = compileAndExecute(
        "CREATE TABLE t_auto_zero ("
        "id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, "
        "username VARCHAR(32) NOT NULL, "
        "UNIQUE (username))");
    ASSERT_TRUE(create_result.success()) << create_result.error();

    auto insert_result = compileAndExecute(
        "INSERT INTO t_auto_zero VALUES "
        "(0, 'mysql'), "
        "(0, 'mysql ab'), "
        "(0, 'mysql a'), "
        "(0, 'r1manic'), "
        "(0, 'r1man')");
    ASSERT_TRUE(insert_result.success()) << insert_result.error();

    auto select_result = compileAndExecute(
        "SELECT id, username FROM t_auto_zero ORDER BY id");
    ASSERT_TRUE(select_result.success()) << select_result.error();
    ASSERT_TRUE(select_result.hasResultSet());

    auto* rs = select_result.resultSet();
    ASSERT_EQ(rs->rowCount(), 5u);
    EXPECT_EQ(rs->getValue(0, 0).toInt64(), 1);
    EXPECT_EQ(rs->getValue(1, 0).toInt64(), 2);
    EXPECT_EQ(rs->getValue(2, 0).toInt64(), 3);
    EXPECT_EQ(rs->getValue(3, 0).toInt64(), 4);
    EXPECT_EQ(rs->getValue(4, 0).toInt64(), 5);
    EXPECT_EQ(rs->getValue(0, 1).toString(), "mysql");
    EXPECT_EQ(rs->getValue(4, 1).toString(), "r1man");
}

TEST_F(MySQLQueryCompilerTest, AutoIncrementTableConstraintMixedRowsAdvanceCleanly) {
    auto create_result = compileAndExecute(
        "CREATE TABLE t_auto_mix_pk ("
        "a INT NOT NULL AUTO_INCREMENT, "
        "PRIMARY KEY (a), "
        "t TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, "
        "c CHAR(10) DEFAULT 'hello', "
        "i INT)");
    ASSERT_TRUE(create_result.success()) << create_result.error();

    auto insert_result = compileAndExecute(
        "INSERT INTO t_auto_mix_pk VALUES "
        "(DEFAULT, DEFAULT, DEFAULT, DEFAULT), "
        "(DEFAULT, DEFAULT, DEFAULT, DEFAULT), "
        "(4, 0, 'a', 5), "
        "(DEFAULT, DEFAULT, DEFAULT, DEFAULT)");
    ASSERT_TRUE(insert_result.success()) << insert_result.error();

    auto select_result = compileAndExecute(
        "SELECT a, c, i FROM t_auto_mix_pk ORDER BY a");
    ASSERT_TRUE(select_result.success()) << select_result.error();
    ASSERT_TRUE(select_result.hasResultSet());

    auto* rs = select_result.resultSet();
    ASSERT_EQ(rs->rowCount(), 4u);
    EXPECT_EQ(rs->getValue(0, 0).toInt64(), 1);
    EXPECT_EQ(rs->getValue(1, 0).toInt64(), 2);
    EXPECT_EQ(rs->getValue(2, 0).toInt64(), 4);
    EXPECT_EQ(rs->getValue(3, 0).toInt64(), 5);
    EXPECT_EQ(rs->getValue(2, 1).toString(), "a");
    EXPECT_EQ(rs->getValue(2, 2).toInt64(), 5);
}

TEST_F(MySQLQueryCompilerTest, AutoIncrementSetSyntaxAdvancesAcrossExplicitRows) {
    auto create_result = compileAndExecute(
        "CREATE TABLE t_auto_set_pk ("
        "a INT NOT NULL AUTO_INCREMENT, "
        "PRIMARY KEY (a), "
        "t TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, "
        "c CHAR(10) DEFAULT 'hello', "
        "i INT)");
    ASSERT_TRUE(create_result.success()) << create_result.error();

    auto seed_result = compileAndExecute(
        "INSERT INTO t_auto_set_pk VALUES "
        "(DEFAULT, DEFAULT, DEFAULT, DEFAULT), "
        "(DEFAULT, DEFAULT, DEFAULT, DEFAULT), "
        "(4, 0, 'a', 5), "
        "(DEFAULT, DEFAULT, DEFAULT, DEFAULT)");
    ASSERT_TRUE(seed_result.success()) << seed_result.error();

    auto truncate_result = compileAndExecute("TRUNCATE TABLE t_auto_set_pk");
    ASSERT_TRUE(truncate_result.success()) << truncate_result.error();

    auto post_truncate = compileAndExecute(
        "SELECT a FROM t_auto_set_pk ORDER BY a");
    ASSERT_TRUE(post_truncate.success()) << post_truncate.error();
    ASSERT_TRUE(post_truncate.hasResultSet());
    ASSERT_EQ(post_truncate.resultSet()->rowCount(), 0u);

    auto insert_default_1 = compileAndExecute(
        "INSERT INTO t_auto_set_pk SET a=DEFAULT,t=DEFAULT,c=DEFAULT");
    ASSERT_TRUE(insert_default_1.success()) << insert_default_1.error();
    auto insert_default_2 = compileAndExecute(
        "INSERT INTO t_auto_set_pk SET a=DEFAULT,t=DEFAULT,c=DEFAULT,i=DEFAULT");
    ASSERT_TRUE(insert_default_2.success()) << insert_default_2.error();
    auto insert_explicit_4 = compileAndExecute(
        "INSERT INTO t_auto_set_pk SET a=4,t=0,c='a',i=5");
    ASSERT_TRUE(insert_explicit_4.success()) << insert_explicit_4.error();
    auto insert_explicit_5 = compileAndExecute(
        "INSERT INTO t_auto_set_pk SET a=5,t=0,c='a',i=NULL");
    ASSERT_TRUE(insert_explicit_5.success()) << insert_explicit_5.error();
    auto final_insert = compileAndExecute(
        "INSERT INTO t_auto_set_pk SET a=DEFAULT,t=DEFAULT,c=DEFAULT,i=DEFAULT");
    ASSERT_TRUE(final_insert.success()) << final_insert.error();

    auto select_result = compileAndExecute(
        "SELECT a, c, i FROM t_auto_set_pk ORDER BY a");
    ASSERT_TRUE(select_result.success()) << select_result.error();
    ASSERT_TRUE(select_result.hasResultSet());

    auto* rs = select_result.resultSet();
    ASSERT_EQ(rs->rowCount(), 5u);
    EXPECT_EQ(rs->getValue(0, 0).toInt64(), 1);
    EXPECT_EQ(rs->getValue(1, 0).toInt64(), 2);
    EXPECT_EQ(rs->getValue(2, 0).toInt64(), 4);
    EXPECT_EQ(rs->getValue(3, 0).toInt64(), 5);
    EXPECT_EQ(rs->getValue(4, 0).toInt64(), 6);
}

TEST_F(MySQLQueryCompilerTest, AlterTableModifyChangeCharsetCollation) {
    core::ErrorContext ctx;
    auto create_result = compileAndExecute(
        "CREATE TABLE t1 (name VARCHAR(16) CHARACTER SET utf8mb4)");
    ASSERT_TRUE(create_result.success()) << create_result.error();

    auto modify_result = compileAndExecute(
        "ALTER TABLE t1 MODIFY COLUMN name VARCHAR(32) "
        "CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci");
    ASSERT_TRUE(modify_result.success()) << modify_result.error();

    auto change_result = compileAndExecute(
        "ALTER TABLE t1 CHANGE COLUMN name name VARCHAR(64) "
        "CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci");
    ASSERT_TRUE(change_result.success()) << change_result.error();

    core::CatalogManager::SchemaInfo public_schema_info;
    ASSERT_EQ(db_.catalog_manager()->getSchema("public", public_schema_info, &ctx),
              core::Status::OK) << ctx.message;

    core::CatalogManager::TableInfo table_info;
    ASSERT_EQ(db_.catalog_manager()->getTable(
                  public_schema_info.schema_id, "t1", table_info, &ctx),
              core::Status::OK) << ctx.message;

    std::vector<core::CatalogManager::ColumnInfo> columns;
    ASSERT_EQ(db_.catalog_manager()->getColumns(table_info.table_id, columns, &ctx),
              core::Status::OK) << ctx.message;

    core::CatalogManager::CharsetInfo charset_info;
    ASSERT_EQ(db_.catalog_manager()->getCharsetByName("utf8mb4", charset_info, &ctx),
              core::Status::OK) << ctx.message;

    core::CatalogManager::CollationCatalogInfo coll_info;
    ASSERT_EQ(db_.catalog_manager()->getCollationByName("utf8mb4_general_ci", coll_info, &ctx),
              core::Status::OK) << ctx.message;

    auto it = std::find_if(columns.begin(), columns.end(), [](const auto& col) {
        return col.column_name == "name";
    });
    ASSERT_NE(it, columns.end());
    EXPECT_EQ(it->charset, charset_info.charset_id);
    EXPECT_EQ(it->collation_id, coll_info.collation_id);
}

TEST_F(MySQLQueryCompilerTest, WindowFunctionRowNumberExec) {
    auto create_result = compileAndExecute(
        "CREATE TABLE t_window (id INT, dept INT)");
    ASSERT_TRUE(create_result.success()) << create_result.error();

    ASSERT_TRUE(compileAndExecute(
        "INSERT INTO t_window (id, dept) VALUES (10, 1), (20, 1), (30, 2)").success());

    auto result = compileAndExecute(
        "SELECT id, ROW_NUMBER() OVER (ORDER BY id) AS rn "
        "FROM t_window ORDER BY id");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet()) << "Expected result set";

    auto* rs = result.resultSet();
    ASSERT_EQ(rs->rowCount(), 3u);
    EXPECT_EQ(rs->getValue(0, 1).toInt64(), 1);
    EXPECT_EQ(rs->getValue(1, 1).toInt64(), 2);
    EXPECT_EQ(rs->getValue(2, 1).toInt64(), 3);
}

TEST_F(MySQLQueryCompilerTest, MatchAgainstExec) {
    auto create_result = compileAndExecute(
        "CREATE TABLE t_docs (id INT, content TEXT)");
    ASSERT_TRUE(create_result.success()) << create_result.error();

    ASSERT_TRUE(compileAndExecute(
        "INSERT INTO t_docs (id, content) VALUES "
        "(1, 'alpha beta'), (2, 'gamma delta')").success());

    auto result = compileAndExecute(
        "SELECT id FROM t_docs WHERE MATCH(content) AGAINST ('alpha') > 0");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet()) << "Expected result set";

    auto* rs = result.resultSet();
    ASSERT_EQ(rs->rowCount(), 1u);
    EXPECT_EQ(rs->getValue(0, 0).toInt64(), 1);
}

TEST_F(MySQLQueryCompilerTest, IfFunctionAcceptsExistsSubqueryArgument) {
    auto create_result = compileAndExecute(
        "CREATE TABLE sb_tx_truth (id INT PRIMARY KEY, val INT)");
    ASSERT_TRUE(create_result.success()) << create_result.error();

    ASSERT_TRUE(compileAndExecute(
        "INSERT INTO sb_tx_truth(id, val) VALUES (1, 10)").success());

    auto result = compileAndExecute(
        "SELECT CONCAT('ROW_RESULT|TX-001|', "
        "IF(EXISTS(SELECT 1 FROM sb_tx_truth WHERE id = 1), 'PASS', 'FAIL'), "
        "'|commit_visibility')");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    auto* rs = result.resultSet();
    ASSERT_EQ(rs->rowCount(), 1u);
    EXPECT_EQ(rs->getValue(0, 0).toString(),
              "ROW_RESULT|TX-001|PASS|commit_visibility");
}

TEST_F(MySQLQueryCompilerTest, OnDuplicateKeyUpdateSupportsValuesInsideSubquery) {
    auto create_result = compileAndExecute(
        "CREATE TABLE t_dup_values (id INT PRIMARY KEY, x INT)");
    ASSERT_TRUE(create_result.success()) << create_result.error();

    core::ErrorContext ctx;
    core::CatalogManager::SchemaInfo public_schema_info;
    ASSERT_EQ(db_.catalog_manager()->getSchema("public", public_schema_info, &ctx),
              core::Status::OK) << ctx.message;

    core::CatalogManager::TableInfo table_info;
    ASSERT_EQ(db_.catalog_manager()->getTable(
                  public_schema_info.schema_id, "t_dup_values", table_info, &ctx),
              core::Status::OK) << ctx.message;

    std::vector<core::CatalogManager::ColumnInfo> columns;
    ASSERT_EQ(db_.catalog_manager()->getColumns(table_info.table_id, columns, &ctx),
              core::Status::OK) << ctx.message;
    auto id_it = std::find_if(columns.begin(), columns.end(), [](const auto& col) {
        return col.column_name == "id";
    });
    ASSERT_NE(id_it, columns.end());
    EXPECT_TRUE(id_it->is_primary_key);
    EXPECT_TRUE(id_it->is_unique);

    std::vector<core::CatalogManager::IndexInfo> indexes;
    ASSERT_EQ(db_.catalog_manager()->listIndexesForTable(
                  table_info.table_id, indexes, &ctx, false),
              core::Status::OK) << ctx.message;

    auto seed_result = compileAndExecute(
        "INSERT INTO t_dup_values VALUES (0, 0)");
    ASSERT_TRUE(seed_result.success()) << seed_result.error();

    auto update_result = compileAndExecute(
        "INSERT INTO t_dup_values VALUES (0, 0) "
        "ON DUPLICATE KEY UPDATE x = (SELECT VALUES(x) + 1 FROM t_dup_values t1)");
    ASSERT_TRUE(update_result.success()) << update_result.error();

    auto select_result = compileAndExecute(
        "SELECT x FROM t_dup_values WHERE id = 0");
    ASSERT_TRUE(select_result.success()) << select_result.error();
    ASSERT_TRUE(select_result.hasResultSet());
    ASSERT_EQ(select_result.resultSet()->rowCount(), 1u);
    EXPECT_EQ(select_result.resultSet()->getValue(0, 0).toInt64(), 1);
}

TEST_F(MySQLQueryCompilerTest, EmptyValuesAliasOnDuplicateHandlesHiddenAndInvisibleColumns) {
    auto create_index_expr_result = compileAndExecute(
        "CREATE TABLE t_hidden_key (f1 INTEGER, KEY k1 ((1)))");
    ASSERT_TRUE(create_index_expr_result.success()) << create_index_expr_result.error();

    auto insert_hidden_result = compileAndExecute(
        "INSERT INTO t_hidden_key VALUES() AS f2 ON DUPLICATE KEY UPDATE f1 = 1");
    ASSERT_TRUE(insert_hidden_result.success()) << insert_hidden_result.error();

    auto create_view_result = compileAndExecute(
        "CREATE OR REPLACE VIEW v_hidden_key AS SELECT * FROM t_hidden_key");
    ASSERT_TRUE(create_view_result.success()) << create_view_result.error();

    auto insert_view_result = compileAndExecute(
        "INSERT INTO v_hidden_key VALUES() AS f2 ON DUPLICATE KEY UPDATE f1 = 1");
    ASSERT_TRUE(insert_view_result.success()) << insert_view_result.error();

    auto create_invisible_result = compileAndExecute(
        "CREATE TABLE t_invisible_key "
        "(f1 INTEGER, b INTEGER AS ((1)) INVISIBLE, KEY k1(b))");
    ASSERT_TRUE(create_invisible_result.success()) << create_invisible_result.error();

    auto insert_invisible_result = compileAndExecute(
        "INSERT INTO t_invisible_key VALUES() AS f2 ON DUPLICATE KEY UPDATE f1 = 1");
    ASSERT_TRUE(insert_invisible_result.success()) << insert_invisible_result.error();
}

TEST_F(MySQLQueryCompilerTest, PreparedViewLifecycleRemainsDroppableWithoutDeallocate) {
    auto create_base_result = compileAndExecute(
        "CREATE TABLE t_prepared_drop_base ("
        "pk INTEGER NOT NULL, "
        "col_varchar VARCHAR(64) DEFAULT NULL, "
        "col_blob BLOB, "
        "PRIMARY KEY (pk))");
    ASSERT_TRUE(create_base_result.success()) << create_base_result.error();

    auto create_view_result = compileAndExecute(
        "CREATE OR REPLACE VIEW v_prepared_drop AS "
        "SELECT col_blob, pk, col_varchar "
        "FROM t_prepared_drop_base "
        "WHERE pk BETWEEN 4 AND 5");
    ASSERT_TRUE(create_view_result.success()) << create_view_result.error();

    auto create_source_result = compileAndExecute(
        "CREATE TABLE t_prepared_drop_source ("
        "pk INTEGER NOT NULL, "
        "col_int INTEGER DEFAULT NULL, "
        "col_blob BLOB, "
        "PRIMARY KEY (pk))");
    ASSERT_TRUE(create_source_result.success()) << create_source_result.error();

    auto insert_source_result = compileAndExecute(
        "INSERT INTO t_prepared_drop_source VALUES (7, 8, 0xEFBFBDEFBFBDEFBFBDEFBFBD004A)");
    ASSERT_TRUE(insert_source_result.success()) << insert_source_result.error();

    auto prepare_result = compileAndExecute(
        "PREPARE stmt_tail_prepare FROM "
        "\"INSERT INTO v_prepared_drop (col_blob, pk, col_varchar) "
        "SELECT col_blob, col_int, col_blob "
        "FROM t_prepared_drop_source "
        "WHERE pk BETWEEN 7 AND 8 "
        "LIMIT 1\"");
    ASSERT_TRUE(prepare_result.success()) << prepare_result.error();

    auto drop_view_result = compileAndExecute("DROP VIEW v_prepared_drop");
    ASSERT_TRUE(drop_view_result.success()) << drop_view_result.error();

    auto drop_tables_result = compileAndExecute(
        "DROP TABLE t_prepared_drop_base, t_prepared_drop_source");
    ASSERT_TRUE(drop_tables_result.success()) << drop_tables_result.error();

    auto create_followup_result = compileAndExecute(
        "CREATE TABLE t_prepared_drop_followup (a INT, b INT)");
    ASSERT_TRUE(create_followup_result.success()) << create_followup_result.error();

    auto drop_followup_result = compileAndExecute("DROP TABLE t_prepared_drop_followup");
    ASSERT_TRUE(drop_followup_result.success()) << drop_followup_result.error();
}

TEST_F(MySQLQueryCompilerTest, HiddenValueAliasCleanupAllowsSubsequentDropAndRecreate) {
    auto create_hidden_result = compileAndExecute(
        "CREATE TABLE t_hidden_cleanup (f1 INTEGER, KEY k1 ((1)))");
    ASSERT_TRUE(create_hidden_result.success()) << create_hidden_result.error();

    auto insert_hidden_result = compileAndExecute(
        "INSERT INTO t_hidden_cleanup VALUES() AS f2 ON DUPLICATE KEY UPDATE f1 = 1");
    ASSERT_TRUE(insert_hidden_result.success()) << insert_hidden_result.error();

    auto create_view_result = compileAndExecute(
        "CREATE OR REPLACE VIEW v_hidden_cleanup AS SELECT * FROM t_hidden_cleanup");
    ASSERT_TRUE(create_view_result.success()) << create_view_result.error();

    auto insert_view_result = compileAndExecute(
        "INSERT INTO v_hidden_cleanup VALUES() AS f2 ON DUPLICATE KEY UPDATE f1 = 1");
    ASSERT_TRUE(insert_view_result.success()) << insert_view_result.error();

    auto drop_view_result = compileAndExecute("DROP VIEW v_hidden_cleanup");
    ASSERT_TRUE(drop_view_result.success()) << drop_view_result.error();

    auto drop_hidden_result = compileAndExecute("DROP TABLE t_hidden_cleanup");
    ASSERT_TRUE(drop_hidden_result.success()) << drop_hidden_result.error();

    auto create_invisible_result = compileAndExecute(
        "CREATE TABLE t_invisible_cleanup "
        "(f1 INTEGER, b INTEGER AS ((1)) INVISIBLE, KEY k1(b))");
    ASSERT_TRUE(create_invisible_result.success()) << create_invisible_result.error();

    auto insert_invisible_result = compileAndExecute(
        "INSERT INTO t_invisible_cleanup VALUES() AS f2 ON DUPLICATE KEY UPDATE f1 = 1");
    ASSERT_TRUE(insert_invisible_result.success()) << insert_invisible_result.error();

    auto drop_invisible_result = compileAndExecute("DROP TABLE t_invisible_cleanup");
    ASSERT_TRUE(drop_invisible_result.success()) << drop_invisible_result.error();

    auto create_tail_result = compileAndExecute(
        "CREATE TABLE t_hidden_cleanup_tail (a INT, b INT)");
    ASSERT_TRUE(create_tail_result.success()) << create_tail_result.error();

    auto drop_tail_result = compileAndExecute("DROP TABLE t_hidden_cleanup_tail");
    ASSERT_TRUE(drop_tail_result.success()) << drop_tail_result.error();
}

TEST_F(MySQLQueryCompilerTest, ReplaceIntoOverwritesExistingPrimaryKeyRow) {
    auto create_result = compileAndExecute(
        "CREATE TABLE t_replace_pk (id INT PRIMARY KEY, data INT)");
    ASSERT_TRUE(create_result.success()) << create_result.error();

    auto seed_result = compileAndExecute(
        "INSERT INTO t_replace_pk VALUES (1, 1), (2, 2)");
    ASSERT_TRUE(seed_result.success()) << seed_result.error();

    auto replace_result = compileAndExecute(
        "REPLACE INTO t_replace_pk VALUES (1, 11)");
    ASSERT_TRUE(replace_result.success()) << replace_result.error();

    auto select_result = compileAndExecute(
        "SELECT id, data FROM t_replace_pk ORDER BY id");
    ASSERT_TRUE(select_result.success()) << select_result.error();
    ASSERT_TRUE(select_result.hasResultSet());
    auto* rs = select_result.resultSet();
    ASSERT_EQ(rs->rowCount(), 2u);
    EXPECT_EQ(rs->getValue(0, 0).toInt64(), 1);
    EXPECT_EQ(rs->getValue(0, 1).toInt64(), 11);
    EXPECT_EQ(rs->getValue(1, 0).toInt64(), 2);
    EXPECT_EQ(rs->getValue(1, 1).toInt64(), 2);
}

TEST_F(MySQLQueryCompilerTest, ReplaceAfterInsertIgnoreStillOverwritesPrimaryKeyRow) {
    auto create_result = compileAndExecute(
        "CREATE TABLE t_replace_after_ignore (id INT PRIMARY KEY, data INT)");
    ASSERT_TRUE(create_result.success()) << create_result.error();

    auto seed_result = compileAndExecute(
        "INSERT INTO t_replace_after_ignore VALUES (1, 1), (2, 2), (3, 3)");
    ASSERT_TRUE(seed_result.success()) << seed_result.error();

    auto ignored_insert = compileAndExecute(
        "INSERT IGNORE INTO t_replace_after_ignore VALUES (1, 1)");
    ASSERT_TRUE(ignored_insert.success()) << ignored_insert.error();
    EXPECT_EQ(ignored_insert.affectedCount(), 0u);

    auto replace_result = compileAndExecute(
        "REPLACE INTO t_replace_after_ignore VALUES (1, 11)");
    ASSERT_TRUE(replace_result.success()) << replace_result.error();

    auto select_result = compileAndExecute(
        "SELECT id, data FROM t_replace_after_ignore ORDER BY id");
    ASSERT_TRUE(select_result.success()) << select_result.error();
    ASSERT_TRUE(select_result.hasResultSet());
    auto* rs = select_result.resultSet();
    ASSERT_EQ(rs->rowCount(), 3u);
    EXPECT_EQ(rs->getValue(0, 0).toInt64(), 1);
    EXPECT_EQ(rs->getValue(0, 1).toInt64(), 11);
    EXPECT_EQ(rs->getValue(1, 0).toInt64(), 2);
    EXPECT_EQ(rs->getValue(1, 1).toInt64(), 2);
    EXPECT_EQ(rs->getValue(2, 0).toInt64(), 3);
    EXPECT_EQ(rs->getValue(2, 1).toInt64(), 3);
}

TEST_F(MySQLQueryCompilerTest, CreateDatabaseBuildsCanonicalEmulatedRootSchema) {
    constexpr const char* kRoot = "emulated.mysql.localhost.databases.main";

    sblr::MySQLQueryCompiler compiler(&db_);
    compiler.setDefaultSchema(kRoot);
    auto compile_result = compiler.compile("CREATE DATABASE IF NOT EXISTS main");
    ASSERT_TRUE(compile_result.success());

    scratchbird::sblr::v3::Container container;
    std::string decode_err;
    ASSERT_TRUE(scratchbird::sblr::v3::decodeContainer(compile_result.bytecode().data(),
                                                       compile_result.bytecode().size(),
                                                       container,
                                                       decode_err))
        << decode_err;

    bool saw_create_db = false;
    scratchbird::sblr::v3::DecodeError inst_err;
    size_t offset = 0;
    while (offset < container.bytecode_stream.size()) {
        scratchbird::sblr::v3::Instruction inst;
        ASSERT_TRUE(scratchbird::sblr::v3::decodeInstructionWithSchema(
            container.bytecode_stream.data(),
            container.bytecode_stream.size(),
            offset,
            inst,
            inst_err)) << inst_err.message;
        if (inst.opcode == static_cast<uint16_t>(scratchbird::sblr::v3::Opcode::SBLR3_CREATE_DATABASE)) {
            saw_create_db = true;
            break;
        }
    }
    ASSERT_TRUE(saw_create_db);

    auto create_db = executor_->execute(compile_result.bytecode());
    ASSERT_TRUE(create_db.success()) << create_db.error();

    core::ErrorContext ctx;
    core::CatalogManager::SchemaInfo schema_info;
    ASSERT_EQ(db_.catalog_manager()->getSchema(kRoot, schema_info, &ctx),
              core::Status::OK)
        << ctx.message;

    conn_ctx_->set_search_path({kRoot});
    conn_ctx_->setCurrentSchemaId(schema_info.schema_id);

    auto create_table = compileAndExecuteWithDefaultSchema(
        "CREATE TABLE IF NOT EXISTS sb_tx_truth_probe (id INT PRIMARY KEY)",
        kRoot);
    ASSERT_TRUE(create_table.success()) << create_table.error();

    core::CatalogManager::TableInfo table_info;
    ASSERT_EQ(db_.catalog_manager()->getTable(schema_info.schema_id,
                                              "sb_tx_truth_probe",
                                              table_info,
                                              &ctx),
              core::Status::OK)
        << ctx.message;
}

TEST_F(MySQLQueryCompilerTest, UseAfterDatabaseRecreateRefreshesSchemaBinding) {
    constexpr const char* kRoot = "emulated.mysql.localhost.databases.main";

    auto drop_db = compileAndExecuteWithDefaultSchema(
        "DROP DATABASE IF EXISTS main",
        kRoot);
    ASSERT_TRUE(drop_db.success()) << drop_db.error();

    auto create_db = compileAndExecuteWithDefaultSchema(
        "CREATE DATABASE IF NOT EXISTS main",
        kRoot);
    ASSERT_TRUE(create_db.success()) << create_db.error();

    auto use_db = compileAndExecute("USE main");
    ASSERT_TRUE(use_db.success()) << use_db.error();

    core::ErrorContext ctx;
    core::CatalogManager::SchemaInfo schema_info;
    ASSERT_EQ(db_.catalog_manager()->getSchema(kRoot, schema_info, &ctx),
              core::Status::OK)
        << ctx.message;
    ASSERT_EQ(conn_ctx_->search_path().front(), kRoot);
    EXPECT_EQ(conn_ctx_->getCurrentSchemaId(), schema_info.schema_id);

    auto create_table = compileAndExecute(
        "CREATE TABLE t_recreate_bind (id INT PRIMARY KEY, payload VARCHAR(32))");
    ASSERT_TRUE(create_table.success()) << create_table.error();

    auto insert_row = compileAndExecute(
        "INSERT INTO t_recreate_bind VALUES (1, 'alpha')");
    ASSERT_TRUE(insert_row.success()) << insert_row.error();

    auto select_rows = compileAndExecute(
        "SELECT id, payload FROM t_recreate_bind ORDER BY id");
    ASSERT_TRUE(select_rows.success()) << select_rows.error();
    ASSERT_TRUE(select_rows.hasResultSet());
    ASSERT_EQ(select_rows.resultSet()->rowCount(), 1u);
    EXPECT_EQ(select_rows.resultSet()->getValue(0, 0).toInt64(), 1);
    EXPECT_EQ(select_rows.resultSet()->getValue(0, 1).toString(), "alpha");
}

TEST_F(MySQLQueryCompilerTest, UseSwitchesAcrossSiblingMySqlDatabaseSchemas) {
    constexpr const char* kMainRoot = "emulated.mysql.localhost.databases.main";
    constexpr const char* kCompatRoot = "emulated.mysql.localhost.databases.compat_mysql";

    auto create_main = compileAndExecuteWithDefaultSchema(
        "CREATE DATABASE IF NOT EXISTS main",
        kMainRoot);
    ASSERT_TRUE(create_main.success()) << create_main.error();

    auto create_compat = compileAndExecuteWithDefaultSchema(
        "CREATE DATABASE IF NOT EXISTS compat_mysql",
        kMainRoot);
    ASSERT_TRUE(create_compat.success()) << create_compat.error();

    auto use_main = compileAndExecute("USE main");
    ASSERT_TRUE(use_main.success()) << use_main.error();
    ASSERT_EQ(conn_ctx_->search_path().front(), kMainRoot);
    ASSERT_EQ(conn_ctx_->current_schema(), kMainRoot);

    auto use_compat = compileAndExecute("USE compat_mysql");
    ASSERT_TRUE(use_compat.success()) << use_compat.error();

    core::ErrorContext ctx;
    core::CatalogManager::SchemaInfo compat_schema_info;
    ASSERT_EQ(db_.catalog_manager()->getSchema(kCompatRoot, compat_schema_info, &ctx),
              core::Status::OK)
        << ctx.message;
    ASSERT_EQ(conn_ctx_->search_path().front(), kCompatRoot);
    ASSERT_EQ(conn_ctx_->current_schema(), kCompatRoot);
    EXPECT_EQ(conn_ctx_->getCurrentSchemaId(), compat_schema_info.schema_id);

    auto create_table = compileAndExecute(
        "CREATE TABLE t_use_switch_bind (id INT PRIMARY KEY, payload VARCHAR(32))");
    ASSERT_TRUE(create_table.success()) << create_table.error();

    auto insert_row = compileAndExecute(
        "INSERT INTO t_use_switch_bind VALUES (1, 'beta')");
    ASSERT_TRUE(insert_row.success()) << insert_row.error();

    auto show_tables = compileAndExecute("SHOW TABLES");
    ASSERT_TRUE(show_tables.success()) << show_tables.error();
    ASSERT_TRUE(show_tables.hasResultSet());
    ASSERT_EQ(show_tables.resultSet()->rowCount(), 1u);
    EXPECT_EQ(show_tables.resultSet()->getValue(0, 0).toString(), "t_use_switch_bind");

    auto select_rows = compileAndExecute(
        "SELECT id, payload FROM t_use_switch_bind ORDER BY id");
    ASSERT_TRUE(select_rows.success()) << select_rows.error();
    ASSERT_TRUE(select_rows.hasResultSet());
    ASSERT_EQ(select_rows.resultSet()->rowCount(), 1u);
    EXPECT_EQ(select_rows.resultSet()->getValue(0, 0).toInt64(), 1);
    EXPECT_EQ(select_rows.resultSet()->getValue(0, 1).toString(), "beta");
}

TEST_F(MySQLQueryCompilerTest, RemoteModeCompilerKeepsMultiRowVarcharInsertExecutableInUsedDatabase) {
    constexpr const char* kMainRoot = "emulated.mysql.localhost.databases.main";
    constexpr const char* kCompatRoot = "emulated.mysql.localhost.databases.compat_mysql_min";

    auto create_main = compileAndExecuteWithDefaultSchema(
        "CREATE DATABASE IF NOT EXISTS main",
        kMainRoot);
    ASSERT_TRUE(create_main.success()) << create_main.error();

    auto create_compat = compileAndExecuteWithDefaultSchema(
        "CREATE DATABASE IF NOT EXISTS compat_mysql_min",
        kMainRoot);
    ASSERT_TRUE(create_compat.success()) << create_compat.error();

    auto use_compat = compileAndExecute("USE compat_mysql_min");
    ASSERT_TRUE(use_compat.success()) << use_compat.error();
    ASSERT_EQ(conn_ctx_->search_path().front(), kCompatRoot);
    ASSERT_EQ(conn_ctx_->current_schema(), kCompatRoot);

    auto compile_remote = [&](const std::string& sql) -> sblr::ExecutionResult {
        sblr::MySQLQueryCompiler compiler(nullptr);
        compiler.setDefaultSchema(kCompatRoot);
        auto compile_result = compiler.compile(sql);
        if (!compile_result.success()) {
            std::string errors;
            for (const auto& err : compile_result.errors()) {
                errors += err + "\n";
            }
            return sblr::ExecutionResult("Compilation failed: " + errors);
        }
        return executor_->execute(compile_result.bytecode());
    };

    auto create_table = compile_remote(
        "CREATE TABLE IF NOT EXISTS t1 (email VARCHAR(50))");
    ASSERT_TRUE(create_table.success()) << create_table.error();

    auto insert_rows = compile_remote(
        "INSERT INTO t1 VALUES "
        "('sasha@mysql.com'),"
        "('monty@mysql.com'),"
        "('foo@hotmail.com'),"
        "('foo@aol.com'),"
        "('bar@aol.com')");
    ASSERT_TRUE(insert_rows.success()) << insert_rows.error();

    auto select_rows = compileAndExecute(
        "SELECT email FROM t1 ORDER BY email");
    ASSERT_TRUE(select_rows.success()) << select_rows.error();
    ASSERT_TRUE(select_rows.hasResultSet());
    ASSERT_EQ(select_rows.resultSet()->rowCount(), 5u);
    EXPECT_EQ(select_rows.resultSet()->getValue(0, 0).toString(), "bar@aol.com");
    EXPECT_EQ(select_rows.resultSet()->getValue(4, 0).toString(), "sasha@mysql.com");
}

TEST_F(MySQLQueryCompilerTest, QualifiedCrossDatabaseReferencesResolveUnderSharedMySqlRoot) {
    constexpr const char* kRoot = "emulated.mysql.localhost.databases.main";

    auto create_main = compileAndExecuteWithDefaultSchema(
        "CREATE DATABASE IF NOT EXISTS mymain",
        kRoot);
    ASSERT_TRUE(create_main.success()) << create_main.error();

    auto create_alt = compileAndExecuteWithDefaultSchema(
        "CREATE DATABASE IF NOT EXISTS mymain1",
        kRoot);
    ASSERT_TRUE(create_alt.success()) << create_alt.error();

    auto create_a = compileAndExecuteWithDefaultSchema(
        "CREATE TABLE mymain.t_a (id INT PRIMARY KEY, v INT)",
        kRoot);
    ASSERT_TRUE(create_a.success()) << create_a.error();

    auto create_b = compileAndExecuteWithDefaultSchema(
        "CREATE TABLE mymain1.t_b (id INT PRIMARY KEY, v INT)",
        kRoot);
    ASSERT_TRUE(create_b.success()) << create_b.error();

    ASSERT_TRUE(compileAndExecuteWithDefaultSchema(
        "INSERT INTO mymain.t_a(id, v) VALUES (1, 10)",
        kRoot).success());
    ASSERT_TRUE(compileAndExecuteWithDefaultSchema(
        "INSERT INTO mymain1.t_b(id, v) VALUES (2, 20)",
        kRoot).success());

    auto main_sum = compileAndExecuteWithDefaultSchema(
        "SELECT SUM(v) FROM mymain.t_a",
        kRoot);
    ASSERT_TRUE(main_sum.success()) << main_sum.error();
    ASSERT_TRUE(main_sum.hasResultSet());
    ASSERT_EQ(main_sum.resultSet()->rowCount(), 1u);
    EXPECT_EQ(main_sum.resultSet()->getValue(0, 0).toInt64(), 10);

    auto alt_sum = compileAndExecuteWithDefaultSchema(
        "SELECT SUM(v) FROM mymain1.t_b",
        kRoot);
    ASSERT_TRUE(alt_sum.success()) << alt_sum.error();
    ASSERT_TRUE(alt_sum.hasResultSet());
    ASSERT_EQ(alt_sum.resultSet()->rowCount(), 1u);
    EXPECT_EQ(alt_sum.resultSet()->getValue(0, 0).toInt64(), 20);
}

TEST_F(MySQLQueryCompilerTest, RepeatedCreateDatabaseKeepsEmulatedViewGenerationStable) {
    constexpr const char* kRoot = "emulated.mysql.localhost.databases.main";

    for (int iteration = 0; iteration < 40; ++iteration) {
        auto drop_db = compileAndExecuteWithDefaultSchema(
            "DROP DATABASE IF EXISTS wave2_retry",
            kRoot);
        ASSERT_TRUE(drop_db.success()) << "drop iteration " << iteration << ": "
                                       << drop_db.error();

        auto create_db = compileAndExecuteWithDefaultSchema(
            "CREATE DATABASE IF NOT EXISTS wave2_retry",
            kRoot);
        ASSERT_TRUE(create_db.success()) << "create iteration " << iteration << ": "
                                         << create_db.error();
    }
}

TEST_F(MySQLQueryCompilerTest, DropDatabaseIfExistsCleansStaleSchemaRecord) {
    constexpr const char* kRoot = "emulated.mysql.localhost.databases.main";
    constexpr const char* kCompatRoot = "emulated.mysql.localhost.databases.compat_mysql";

    auto create_db = compileAndExecuteWithDefaultSchema(
        "CREATE DATABASE IF NOT EXISTS compat_mysql",
        kRoot);
    ASSERT_TRUE(create_db.success()) << create_db.error();

    core::ErrorContext ctx;
    auto* catalog = db_.catalog_manager();
    ASSERT_NE(catalog, nullptr);

    core::CatalogManager::EmulationServerInfo server_info;
    ASSERT_EQ(catalog->getEmulationServerByName("localhost", server_info, &ctx),
              core::Status::OK)
        << ctx.message;

    core::CatalogManager::EmulatedDatabaseInfo db_info;
    ASSERT_EQ(catalog->getEmulatedDatabaseByName(server_info.server_id,
                                                 "compat_mysql",
                                                 db_info,
                                                 &ctx),
              core::Status::OK)
        << ctx.message;

    core::ErrorContext invalidate_ctx;
    ASSERT_EQ(invalidateSchemaRecordOnDisk(db_, db_info.schema_id, &invalidate_ctx),
              core::Status::OK)
        << invalidate_ctx.message;

    auto drop_db = compileAndExecuteWithDefaultSchema(
        "DROP DATABASE IF EXISTS compat_mysql",
        kRoot);
    ASSERT_TRUE(drop_db.success()) << drop_db.error();

    core::CatalogManager::EmulatedDatabaseInfo missing_info;
    core::ErrorContext lookup_ctx;
    EXPECT_EQ(catalog->getEmulatedDatabaseByName(server_info.server_id,
                                                 "compat_mysql",
                                                 missing_info,
                                                 &lookup_ctx),
              core::Status::NOT_FOUND);

    core::CatalogManager::SchemaInfo missing_schema;
    core::ErrorContext schema_ctx;
    EXPECT_NE(catalog->getSchema(kCompatRoot, missing_schema, &schema_ctx),
              core::Status::OK);
}

TEST_F(MySQLQueryCompilerTest, DropDatabaseAfterDroppingTableCleansOwnedConstraints) {
    constexpr const char* kRoot = "emulated.mysql.localhost.databases.main";
    constexpr const char* kCompatRoot = "emulated.mysql.localhost.databases.compat_mysql";

    auto create_db = compileAndExecuteWithDefaultSchema(
        "CREATE DATABASE IF NOT EXISTS compat_mysql",
        kRoot);
    ASSERT_TRUE(create_db.success()) << create_db.error();

    auto create_table = compileAndExecuteWithDefaultSchema(
        "CREATE TABLE sb_wave2_parser_surface ("
        "id INT PRIMARY KEY, "
        "payload VARCHAR(32))",
        kCompatRoot);
    ASSERT_TRUE(create_table.success()) << create_table.error();

    auto drop_table = compileAndExecuteWithDefaultSchema(
        "DROP TABLE IF EXISTS sb_wave2_parser_surface",
        kCompatRoot);
    ASSERT_TRUE(drop_table.success()) << drop_table.error();

    auto drop_db = compileAndExecuteWithDefaultSchema(
        "DROP DATABASE IF EXISTS compat_mysql",
        kRoot);
    ASSERT_TRUE(drop_db.success()) << drop_db.error();
}

TEST_F(MySQLQueryCompilerTest, DropViewFindsMetadataOnOverflowCatalogPages) {
    auto create_table = compileAndExecute(
        "CREATE TABLE t_overflow_view_base (id INT PRIMARY KEY, payload INT)");
    ASSERT_TRUE(create_table.success()) << create_table.error();

    for (int i = 0; i < 48; ++i) {
        auto create_view = compileAndExecute(
            "CREATE OR REPLACE VIEW v_overflow_" + std::to_string(i) +
            " AS SELECT id, payload FROM t_overflow_view_base");
        ASSERT_TRUE(create_view.success()) << "create view " << i << ": "
                                           << create_view.error();
    }

    auto drop_view = compileAndExecute("DROP VIEW v_overflow_47");
    ASSERT_TRUE(drop_view.success()) << drop_view.error();

    auto drop_table = compileAndExecute("DROP TABLE t_overflow_view_base");
    ASSERT_TRUE(drop_table.success()) << drop_table.error();
}

TEST_F(MySQLQueryCompilerTest, InsertIgnoreSuppressesForeignKeyViolation) {
    auto create_parent = compileAndExecute(
        "CREATE TABLE t_parent_ignore_fk (id INT PRIMARY KEY)");
    ASSERT_TRUE(create_parent.success()) << create_parent.error();

    auto create_child = compileAndExecute(
        "CREATE TABLE t_child_ignore_fk ("
        "parent_id INT, "
        "FOREIGN KEY (parent_id) REFERENCES t_parent_ignore_fk(id))");
    ASSERT_TRUE(create_child.success()) << create_child.error();

    auto seed_parent = compileAndExecute(
        "INSERT INTO t_parent_ignore_fk VALUES (0)");
    ASSERT_TRUE(seed_parent.success()) << seed_parent.error();

    auto seed_child = compileAndExecute(
        "INSERT INTO t_child_ignore_fk VALUES (0)");
    ASSERT_TRUE(seed_child.success()) << seed_child.error();

    auto ignored_insert = compileAndExecute(
        "INSERT IGNORE INTO t_child_ignore_fk VALUES (1)");
    ASSERT_TRUE(ignored_insert.success()) << ignored_insert.error();
    EXPECT_EQ(ignored_insert.affectedCount(), 0u);

    auto select_rows = compileAndExecute(
        "SELECT parent_id FROM t_child_ignore_fk ORDER BY parent_id");
    ASSERT_TRUE(select_rows.success()) << select_rows.error();
    ASSERT_TRUE(select_rows.hasResultSet());
    ASSERT_EQ(select_rows.resultSet()->rowCount(), 1u);
    EXPECT_EQ(select_rows.resultSet()->getValue(0, 0).toInt64(), 0);
}

TEST_F(MySQLQueryCompilerTest, UpdateIgnoreSuppressesForeignKeyViolation) {
    auto create_parent = compileAndExecute(
        "CREATE TABLE t_parent_update_ignore_fk (id INT PRIMARY KEY)");
    ASSERT_TRUE(create_parent.success()) << create_parent.error();

    auto create_child = compileAndExecute(
        "CREATE TABLE t_child_update_ignore_fk ("
        "parent_id INT, "
        "FOREIGN KEY (parent_id) REFERENCES t_parent_update_ignore_fk(id))");
    ASSERT_TRUE(create_child.success()) << create_child.error();

    ASSERT_TRUE(
        compileAndExecute("INSERT INTO t_parent_update_ignore_fk VALUES (0)")
            .success());
    ASSERT_TRUE(
        compileAndExecute("INSERT INTO t_child_update_ignore_fk VALUES (0)")
            .success());

    auto ignored_update = compileAndExecute(
        "UPDATE IGNORE t_child_update_ignore_fk SET parent_id = 1 WHERE parent_id = 0");
    ASSERT_TRUE(ignored_update.success()) << ignored_update.error();
    EXPECT_EQ(ignored_update.affectedCount(), 0u);

    auto select_rows = compileAndExecute(
        "SELECT parent_id FROM t_child_update_ignore_fk");
    ASSERT_TRUE(select_rows.success()) << select_rows.error();
    ASSERT_TRUE(select_rows.hasResultSet());
    ASSERT_EQ(select_rows.resultSet()->rowCount(), 1u);
    EXPECT_EQ(select_rows.resultSet()->getValue(0, 0).toInt64(), 0);
}

TEST_F(MySQLQueryCompilerTest, UpdateIgnoreSuppressesParentKeyRestriction) {
    auto create_parent = compileAndExecute(
        "CREATE TABLE t_parent_update_ignore_parent (id INT PRIMARY KEY)");
    ASSERT_TRUE(create_parent.success()) << create_parent.error();

    auto create_child = compileAndExecute(
        "CREATE TABLE t_child_update_ignore_parent ("
        "parent_id INT, "
        "FOREIGN KEY (parent_id) REFERENCES t_parent_update_ignore_parent(id))");
    ASSERT_TRUE(create_child.success()) << create_child.error();

    ASSERT_TRUE(
        compileAndExecute("INSERT INTO t_parent_update_ignore_parent VALUES (0)")
            .success());
    ASSERT_TRUE(
        compileAndExecute("INSERT INTO t_child_update_ignore_parent VALUES (0)")
            .success());

    auto ignored_update = compileAndExecute(
        "UPDATE IGNORE t_parent_update_ignore_parent SET id = 1 WHERE id = 0");
    ASSERT_TRUE(ignored_update.success()) << ignored_update.error();
    EXPECT_EQ(ignored_update.affectedCount(), 0u);

    auto parent_rows = compileAndExecute(
        "SELECT id FROM t_parent_update_ignore_parent");
    ASSERT_TRUE(parent_rows.success()) << parent_rows.error();
    ASSERT_TRUE(parent_rows.hasResultSet());
    ASSERT_EQ(parent_rows.resultSet()->rowCount(), 1u);
    EXPECT_EQ(parent_rows.resultSet()->getValue(0, 0).toInt64(), 0);

    auto child_rows = compileAndExecute(
        "SELECT parent_id FROM t_child_update_ignore_parent");
    ASSERT_TRUE(child_rows.success()) << child_rows.error();
    ASSERT_TRUE(child_rows.hasResultSet());
    ASSERT_EQ(child_rows.resultSet()->rowCount(), 1u);
    EXPECT_EQ(child_rows.resultSet()->getValue(0, 0).toInt64(), 0);
}

TEST_F(MySQLQueryCompilerTest,
       InsertIgnoreOnDuplicateKeyUpdateSuppressesForeignKeyViolation) {
    auto create_parent = compileAndExecute(
        "CREATE TABLE t_parent_insert_ignore_dup_fk (id INT PRIMARY KEY)");
    ASSERT_TRUE(create_parent.success()) << create_parent.error();

    auto create_child = compileAndExecute(
        "CREATE TABLE t_child_insert_ignore_dup_fk ("
        "name VARCHAR(10) PRIMARY KEY, "
        "parent_id INT NOT NULL, "
        "CONSTRAINT fk_ignore_dup FOREIGN KEY (parent_id) "
        "REFERENCES t_parent_insert_ignore_dup_fk(id))");
    ASSERT_TRUE(create_child.success()) << create_child.error();

    ASSERT_TRUE(
        compileAndExecute("INSERT INTO t_parent_insert_ignore_dup_fk VALUES (1)")
            .success());
    ASSERT_TRUE(
        compileAndExecute(
            "INSERT INTO t_child_insert_ignore_dup_fk VALUES ('abc', 1)")
            .success());

    auto ignored_insert = compileAndExecute(
        "INSERT IGNORE INTO t_child_insert_ignore_dup_fk VALUES ('abc', 1) "
        "ON DUPLICATE KEY UPDATE parent_id = 2");
    ASSERT_TRUE(ignored_insert.success()) << ignored_insert.error();
    EXPECT_EQ(ignored_insert.affectedCount(), 0u);

    auto select_rows = compileAndExecute(
        "SELECT name, parent_id FROM t_child_insert_ignore_dup_fk");
    ASSERT_TRUE(select_rows.success()) << select_rows.error();
    ASSERT_TRUE(select_rows.hasResultSet());
    ASSERT_EQ(select_rows.resultSet()->rowCount(), 1u);
    EXPECT_EQ(select_rows.resultSet()->getValue(0, 0).toString(), "abc");
    EXPECT_EQ(select_rows.resultSet()->getValue(0, 1).toInt64(), 1);
}
