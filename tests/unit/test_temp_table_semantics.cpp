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
 * Temp table placement + lifecycle tests.
 */

#include <gtest/gtest.h>
#include "scratchbird/sblr/query_compiler_v3.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "unit/test_user_helpers.h"
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <sstream>
#include <thread>

using namespace scratchbird::sblr;
using namespace scratchbird::core;

static std::string makeUniquePath(const std::string& prefix, const std::string& suffix)
{
    std::ostringstream oss;
    oss << "/tmp/" << prefix << "_"
        << std::this_thread::get_id() << "_"
        << std::chrono::steady_clock::now().time_since_epoch().count()
        << suffix;
    return oss.str();
}

class TempTableExecutorTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        db_path_ = makeUniquePath("test_temp_tables", ".sbdb");
        std::filesystem::remove(db_path_);

        ErrorContext ctx;
        auto status = Database::create(db_path_, 16384, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to create test database";

        status = db_.open(db_path_, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to open test database";

        catalog_ = db_.catalog_manager();
        ASSERT_NE(catalog_, nullptr);

        CatalogManager::SchemaInfo public_schema_info;
        status = catalog_->getSchema("public", public_schema_info, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to resolve public schema";
        public_schema_id_ = public_schema_info.schema_id;

        compiler_ = std::make_unique<QueryCompilerV3>(&db_);
        executor_ = std::make_unique<Executor>(&db_);
    }

    void TearDown() override
    {
        clearConnection();
        compiler_.reset();
        executor_.reset();
        db_.close();
        std::filesystem::remove(db_path_);
        std::filesystem::remove(db_path_ + "-lock");
    }

    ExecutionResult compileAndExecute(const std::string& sql)
    {
        auto compile_result = compiler_->compile(sql);
        if (!compile_result.success())
        {
            std::string errors;
            for (const auto& err : compile_result.errors())
            {
                errors += err + "\n";
            }
            return ExecutionResult("Compilation failed: " + errors);
        }
        return executor_->execute(compile_result.bytecode());
    }

    std::unique_ptr<ConnectionContext> connectAs(const std::string& username, bool is_superuser = false)
    {
        EnsureUser(catalog_, username, public_schema_id_, is_superuser);

        ErrorContext ctx;
        CatalogManager::UserInfo user_info;
        auto status = catalog_->getUserByName(username, user_info, &ctx);
        EXPECT_EQ(status, Status::OK) << "Failed to resolve user: " << ctx.message;

        std::unique_ptr<ConnectionContext> connection;
        status = db_.connect(connection, &ctx);
        EXPECT_EQ(status, Status::OK) << "Failed to create connection";

        connection->setCurrentSchemaId(public_schema_id_);
        connection->setCurrentUser(user_info.user_id, is_superuser);
        ConnectionContext::setCurrent(connection.get());
        executor_->setConnectionContext(connection.get());
        return connection;
    }

    void clearConnection()
    {
        if (executor_)
        {
            executor_->setConnectionContext(nullptr);
        }
        ConnectionContext::setCurrent(nullptr);
        connection_ctx_.reset();
    }

    void reopenDatabase()
    {
        clearConnection();
        compiler_.reset();
        executor_.reset();
        db_.close();

        ErrorContext ctx;
        auto status = db_.open(db_path_, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to reopen test database: " << ctx.message;

        catalog_ = db_.catalog_manager();
        ASSERT_NE(catalog_, nullptr);

        compiler_ = std::make_unique<QueryCompilerV3>(&db_);
        executor_ = std::make_unique<Executor>(&db_);
    }

    PageHeader readPageHeaderFromFile(uint32_t page_id)
    {
        PageHeader header{};
        std::vector<uint8_t> buffer(db_.page_size());
        const int fd = ::open(db_path_.c_str(), O_RDWR);
        EXPECT_GE(fd, 0) << std::strerror(errno);
        if (fd < 0)
        {
            return header;
        }

        const off_t offset = static_cast<off_t>(page_id) *
                             static_cast<off_t>(db_.page_size());
        const ssize_t bytes = ::pread(fd, buffer.data(), buffer.size(), offset);
        EXPECT_EQ(bytes, static_cast<ssize_t>(buffer.size())) << std::strerror(errno);
        if (bytes == static_cast<ssize_t>(buffer.size()))
        {
            std::memcpy(&header, buffer.data(), sizeof(header));
        }
        ::close(fd);
        return header;
    }

    std::string db_path_;
    Database db_;
    CatalogManager* catalog_ = nullptr;
    ID public_schema_id_;
    std::unique_ptr<QueryCompilerV3> compiler_;
    std::unique_ptr<Executor> executor_;
    std::unique_ptr<ConnectionContext> connection_ctx_;
};

TEST_F(TempTableExecutorTest, TempTableUsesUserTempSchemaIfHomeExists)
{
    ErrorContext ctx;
    ID user_schema_id;
    auto status = catalog_->createSchemaPath("users.alice",
                                             CatalogManager::SchemaType::USER_HOME,
                                             user_schema_id,
                                             &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    connection_ctx_ = connectAs("alice");

    auto create_result = compileAndExecute("CREATE TEMP TABLE temp_user (id INT)");
    ASSERT_TRUE(create_result.success()) << create_result.error();

    CatalogManager::SchemaInfo temp_schema;
    status = catalog_->getSchema("users.alice.temp", temp_schema, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    CatalogManager::TableInfo table_info;
    status = catalog_->getTable(temp_schema.schema_id, "temp_user", table_info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(table_info.temp_metadata_scope, CatalogManager::TempMetadataScope::SESSION);
    EXPECT_EQ(table_info.temp_schema_id, temp_schema.schema_id);
}

TEST_F(TempTableExecutorTest, TempTableUsesPublicTempSchemaWhenNoUserHome)
{
    connection_ctx_ = connectAs("bob");

    auto create_result = compileAndExecute("CREATE TEMP TABLE temp_public (id INT)");
    ASSERT_TRUE(create_result.success()) << create_result.error();

    ErrorContext ctx;
    CatalogManager::SchemaInfo temp_schema;
    auto status = catalog_->getSchema("public.temp", temp_schema, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    CatalogManager::TableInfo table_info;
    status = catalog_->getTable(temp_schema.schema_id, "temp_public", table_info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(table_info.temp_metadata_scope, CatalogManager::TempMetadataScope::SESSION);
    EXPECT_EQ(table_info.temp_schema_id, temp_schema.schema_id);
}

TEST_F(TempTableExecutorTest, TempTableOnCommitPreserveRows)
{
    ErrorContext ctx;
    ID user_schema_id;
    auto status = catalog_->createSchemaPath("users.alice",
                                             CatalogManager::SchemaType::USER_HOME,
                                             user_schema_id,
                                             &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    connection_ctx_ = connectAs("alice");

    ASSERT_TRUE(compileAndExecute(
        "CREATE TEMP TABLE temp_preserve (id INT) ON COMMIT PRESERVE ROWS").success());
    auto insert_result = compileAndExecute("INSERT INTO temp_preserve VALUES (1)");
    ASSERT_TRUE(insert_result.success()) << insert_result.error();
    auto commit_result = compileAndExecute("COMMIT");
    ASSERT_TRUE(commit_result.success()) << commit_result.error();

    auto select_result = compileAndExecute("SELECT * FROM temp_preserve");
    ASSERT_TRUE(select_result.hasResultSet()) << select_result.error();
    EXPECT_EQ(select_result.resultSet()->rowCount(), 1u);
}

TEST_F(TempTableExecutorTest, TempTableOnCommitDeleteRows)
{
    ErrorContext ctx;
    ID user_schema_id;
    auto status = catalog_->createSchemaPath("users.alice",
                                             CatalogManager::SchemaType::USER_HOME,
                                             user_schema_id,
                                             &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    connection_ctx_ = connectAs("alice");

    ASSERT_TRUE(compileAndExecute(
        "CREATE TEMP TABLE temp_delete (id INT) ON COMMIT DELETE ROWS").success());

    CatalogManager::SchemaInfo temp_schema;
    status = catalog_->getSchema("users.alice.temp", temp_schema, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    CatalogManager::TableInfo table_info;
    status = catalog_->getTable(temp_schema.schema_id, "temp_delete", table_info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(table_info.temp_on_commit, CatalogManager::TempOnCommitAction::DELETE_ROWS);

    auto insert_result = compileAndExecute("INSERT INTO temp_delete VALUES (1)");
    ASSERT_TRUE(insert_result.success()) << insert_result.error();
    auto commit_result = compileAndExecute("COMMIT");
    ASSERT_TRUE(commit_result.success()) << commit_result.error();

    auto select_result = compileAndExecute("SELECT * FROM temp_delete");
    ASSERT_TRUE(select_result.hasResultSet()) << select_result.error();
    EXPECT_EQ(select_result.resultSet()->rowCount(), 0u);
}

TEST_F(TempTableExecutorTest, TempTableSessionCleanupDropsMetadata)
{
    ErrorContext ctx;
    ID user_schema_id;
    auto status = catalog_->createSchemaPath("users.alice",
                                             CatalogManager::SchemaType::USER_HOME,
                                             user_schema_id,
                                             &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    connection_ctx_ = connectAs("alice");
    auto session_id = connection_ctx_->effectiveSessionId();

    ASSERT_TRUE(compileAndExecute(
        "CREATE TEMP TABLE temp_session (id INT) ON COMMIT PRESERVE ROWS").success());
    auto insert_result = compileAndExecute("INSERT INTO temp_session VALUES (1)");
    ASSERT_TRUE(insert_result.success()) << insert_result.error();
    auto commit_result = compileAndExecute("COMMIT");
    ASSERT_TRUE(commit_result.success()) << commit_result.error();

    clearConnection();

    std::vector<CatalogManager::TableInfo> tables;
    status = catalog_->listTemporaryTablesForSession(session_id, tables, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    auto it = std::find_if(tables.begin(), tables.end(),
                           [](const CatalogManager::TableInfo& info)
                           {
                               return info.table_name == "temp_session";
                           });
    EXPECT_EQ(it, tables.end()) << "Temp table metadata should be dropped on session end";
}

TEST_F(TempTableExecutorTest, TempTablePagesCarryTemporaryWorkMarkerWithoutDurableGenerations)
{
    ErrorContext ctx;
    ID user_schema_id;
    auto status = catalog_->createSchemaPath("users.alice",
                                             CatalogManager::SchemaType::USER_HOME,
                                             user_schema_id,
                                             &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    connection_ctx_ = connectAs("alice");

    ASSERT_TRUE(compileAndExecute(
        "CREATE TEMP TABLE temp_flush_marker (id INT) ON COMMIT PRESERVE ROWS").success());
    ASSERT_TRUE(compileAndExecute("INSERT INTO temp_flush_marker VALUES (1)").success());

    CatalogManager::SchemaInfo temp_schema;
    status = catalog_->getSchema("users.alice.temp", temp_schema, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    CatalogManager::TableInfo table_info;
    status = catalog_->getTable(temp_schema.schema_id, "temp_flush_marker", table_info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    const uint32_t page_id = static_cast<uint32_t>(getPageNumber(table_info.root_gpid));
    ASSERT_EQ(db_.buffer_pool()->flushPage(page_id, &ctx), Status::OK) << ctx.message;

    const auto header = readPageHeaderFromFile(page_id);
    EXPECT_NE(header.flags & static_cast<uint16_t>(PAGE_FLAG_TEMPORARY_WORK), 0u);
    EXPECT_EQ(header.flush_generation, 0u);
    EXPECT_EQ(header.checkpoint_generation, 0u);
}

TEST_F(TempTableExecutorTest, TempTableCreatedAfterSavepointDropsOnRollback)
{
    ErrorContext ctx;
    ID user_schema_id;
    auto status = catalog_->createSchemaPath("users.alice",
                                             CatalogManager::SchemaType::USER_HOME,
                                             user_schema_id,
                                             &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    connection_ctx_ = connectAs("alice");

    auto start_result = compileAndExecute("START TRANSACTION");
    ASSERT_TRUE(start_result.success()) << start_result.error();
    auto savepoint_result = compileAndExecute("SAVEPOINT temp_sp");
    ASSERT_TRUE(savepoint_result.success()) << savepoint_result.error();
    auto create_result = compileAndExecute(
        "CREATE TEMP TABLE temp_after_sp (id INT) ON COMMIT PRESERVE ROWS");
    ASSERT_TRUE(create_result.success()) << create_result.error();
    auto rollback_result = compileAndExecute("ROLLBACK TO SAVEPOINT temp_sp");
    ASSERT_TRUE(rollback_result.success()) << rollback_result.error();

    CatalogManager::SchemaInfo temp_schema;
    status = catalog_->getSchema("users.alice.temp", temp_schema, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    CatalogManager::TableInfo table_info;
    status = catalog_->getTable(temp_schema.schema_id, "temp_after_sp", table_info, &ctx);
    EXPECT_NE(status, Status::OK) << "Temp metadata created after savepoint must be removed";
    EXPECT_NE(ctx.message.find("Table not found"), std::string::npos) << ctx.message;
}

TEST_F(TempTableExecutorTest, TempTableSavepointRollbackRemovesNewRows)
{
    ErrorContext ctx;
    ID user_schema_id;
    auto status = catalog_->createSchemaPath("users.alice",
                                             CatalogManager::SchemaType::USER_HOME,
                                             user_schema_id,
                                             &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    connection_ctx_ = connectAs("alice");

    ASSERT_TRUE(compileAndExecute("START TRANSACTION").success());
    ASSERT_TRUE(compileAndExecute(
        "CREATE TEMP TABLE temp_rollback_rows (id INT) ON COMMIT PRESERVE ROWS").success());
    ASSERT_TRUE(compileAndExecute("INSERT INTO temp_rollback_rows VALUES (1)").success());
    ASSERT_TRUE(compileAndExecute("SAVEPOINT temp_rows_sp").success());
    ASSERT_TRUE(compileAndExecute("INSERT INTO temp_rollback_rows VALUES (2)").success());
    ASSERT_TRUE(compileAndExecute("ROLLBACK TO SAVEPOINT temp_rows_sp").success());

    auto select_result =
        compileAndExecute("SELECT id FROM temp_rollback_rows ORDER BY id");
    ASSERT_TRUE(select_result.hasResultSet()) << select_result.error();
    ASSERT_EQ(select_result.resultSet()->rowCount(), 1u);
    EXPECT_EQ(select_result.resultSet()->getValue(0, 0).toInt64(), 1);
}

TEST_F(TempTableExecutorTest, StartupReopenDropsStaleSessionTempMetadata)
{
    ErrorContext ctx;
    ID user_schema_id;
    auto status = catalog_->createSchemaPath("users.alice",
                                             CatalogManager::SchemaType::USER_HOME,
                                             user_schema_id,
                                             &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    connection_ctx_ = connectAs("alice");

    ID temp_schema_id;
    status = catalog_->createSchemaPath("users.alice.temp",
                                        CatalogManager::SchemaType::USER_HOME,
                                        temp_schema_id,
                                        &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    std::vector<CatalogManager::ColumnInfo> columns;
    CatalogManager::ColumnInfo id_col;
    id_col.column_id = generateUuidV7();
    id_col.column_name = "id";
    id_col.data_type = static_cast<uint16_t>(DataType::INT32);
    id_col.nullable = false;
    id_col.ordinal = 0;
    columns.push_back(id_col);

    const ID stale_session_id = generateUuidV7();
    CatalogManager::TableCreateOptions create_opts;
    create_opts.table_type = CatalogManager::TableType::TEMPORARY;
    create_opts.temp_metadata_scope = CatalogManager::TempMetadataScope::SESSION;
    create_opts.temp_data_scope = CatalogManager::TempDataScope::SESSION;
    create_opts.temp_on_commit = CatalogManager::TempOnCommitAction::PRESERVE_ROWS;
    create_opts.creating_session_id = stale_session_id;
    create_opts.creating_transaction_id = connection_ctx_->getCurrentXid();
    create_opts.temp_schema_id = temp_schema_id;

    ID table_id;
    status = catalog_->createTable(temp_schema_id,
                                   "stale_temp_restart",
                                   columns,
                                   table_id,
                                   0,
                                   &ctx,
                                   &create_opts);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    reopenDatabase();

    std::vector<CatalogManager::TableInfo> tables;
    status = catalog_->listTemporaryTablesForSession(stale_session_id, tables, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_TRUE(tables.empty()) << "Startup must purge stale session temp metadata";
}
