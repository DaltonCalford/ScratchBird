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
#include "scratchbird/sblr/v3_container.h"
#include "scratchbird/sblr/v3_payloads.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/optimizer/plan_payload.h"
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

static bool decodeFirstSelectInstruction(const std::vector<uint8_t>& bytecode,
                                         scratchbird::sblr::v3::Instruction& out)
{
    scratchbird::sblr::v3::Container container;
    std::string err;
    if (!scratchbird::sblr::v3::decodeContainer(bytecode.data(),
                                                bytecode.size(),
                                                container,
                                                err))
    {
        return false;
    }

    size_t offset = 0;
    scratchbird::sblr::v3::DecodeError decode_err;
    while (offset < container.bytecode_stream.size())
    {
        scratchbird::sblr::v3::Instruction inst;
        if (!scratchbird::sblr::v3::decodeInstructionWithSchema(
                container.bytecode_stream.data(),
                container.bytecode_stream.size(),
                offset,
                inst,
                decode_err))
        {
            return false;
        }
        if (static_cast<scratchbird::sblr::v3::Opcode>(inst.opcode) ==
            scratchbird::sblr::v3::Opcode::SBLR3_SELECT)
        {
            out = std::move(inst);
            return true;
        }
    }
    return false;
}

static bool decodeRuntimePlanFromBytecode(const std::vector<uint8_t>& bytecode,
                                          scratchbird::optimizer::RuntimePlan& plan_out)
{
    scratchbird::sblr::v3::Instruction select_inst;
    if (!decodeFirstSelectInstruction(bytecode, select_inst))
    {
        return false;
    }
    const auto* obj =
        std::get_if<scratchbird::sblr::v3::Value::Object>(&select_inst.payload.data);
    if (!obj)
    {
        return false;
    }
    auto it_plan = obj->find("plan");
    if (it_plan == obj->end())
    {
        return false;
    }
    const auto* bytes =
        std::get_if<scratchbird::sblr::v3::Value::Bytes>(&it_plan->second.data);
    if (!bytes)
    {
        return false;
    }
    std::string err;
    return scratchbird::optimizer::decodeRuntimePlan(*bytes, plan_out, err);
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

    PageHeader readPageHeaderFromFile(GPID gpid)
    {
        PageHeader header{};
        std::vector<uint8_t> buffer(db_.page_size());
        const uint16_t tablespace_id = getTablespaceID(gpid);
        const uint32_t page_id = static_cast<uint32_t>(getPageNumber(gpid));
        int fd = -1;
        bool close_fd = false;
        if (tablespace_id == PRIMARY_TABLESPACE_ID)
        {
            fd = ::open(db_path_.c_str(), O_RDWR);
            close_fd = true;
        }
        else
        {
            fd = db_.getTablespaceFd(tablespace_id);
        }
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
        if (close_fd)
        {
            ::close(fd);
        }
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

    ASSERT_EQ(db_.buffer_pool()->flushPageGlobal(table_info.root_gpid, &ctx), Status::OK)
        << ctx.message;

    const auto header = readPageHeaderFromFile(table_info.root_gpid);
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
    const auto session_id_before_savepoint = connection_ctx_->effectiveSessionId();
    auto inspect_root_page =
        [&](const CatalogManager::TableInfo &table_info,
            std::vector<std::tuple<uint16_t, uint64_t, uint64_t, ID>> &rows_out)
    {
        void *root_page_buffer = nullptr;
        ErrorContext local_ctx;
        EXPECT_EQ(db_.buffer_pool()->pinPageGlobal(table_info.root_gpid, &root_page_buffer, &local_ctx),
                  Status::OK)
            << local_ctx.message;
        if (root_page_buffer == nullptr)
        {
            return;
        }
        auto *root_page_data = static_cast<uint8_t *>(root_page_buffer);
        scratchbird::core::HeapPage root_heap_page(root_page_data,
                                                   db_.page_size(),
                                                   nullptr,
                                                   &db_,
                                                   table_info.table_id);
        const uint16_t root_item_count = root_heap_page.getItemCount();
        for (uint16_t item_id = 0; item_id < root_item_count; ++item_id)
        {
            const uint8_t *tuple_data = nullptr;
            uint32_t tuple_size = 0;
            if (root_heap_page.getTuple(item_id, &tuple_data, &tuple_size, nullptr) != Status::OK)
            {
                continue;
            }
            const auto *tuple_header =
                reinterpret_cast<const scratchbird::core::TupleHeader *>(tuple_data);
            rows_out.emplace_back(item_id,
                                  tuple_header->xmin,
                                  tuple_header->xmax,
                                  tuple_header->session_id);
        }
        db_.buffer_pool()->unpinPageGlobal(table_info.root_gpid, false, &local_ctx);
    };
    auto scan_table_rows =
        [&](const ID &table_id,
            std::vector<std::tuple<uint64_t, uint64_t, ID>> &rows_out)
    {
        ErrorContext local_ctx;
        auto scan = db_.storage_engine()->createScanAll(table_id, &local_ctx);
        EXPECT_NE(scan, nullptr);
        if (!scan)
        {
            return;
        }
        while (true)
        {
            Tuple tuple{};
            auto scan_status = scan->next(&tuple, &local_ctx);
            if (scan_status == Status::NOT_FOUND)
            {
                break;
            }
            EXPECT_EQ(scan_status, Status::OK) << local_ctx.message;
            if (scan_status != Status::OK)
            {
                break;
            }
            const auto *tuple_header =
                reinterpret_cast<const scratchbird::core::TupleHeader *>(tuple.data);
            rows_out.emplace_back(tuple_header->xmin,
                                  tuple_header->xmax,
                                  tuple_header->session_id);
        }
    };
    auto scan_physical_table_rows =
        [&](const CatalogManager::TableInfo &table_info,
            std::vector<std::tuple<uint64_t, uint64_t, ID>> &rows_out)
    {
        auto *page_manager = db_.page_manager();
        ASSERT_NE(page_manager, nullptr);
        std::vector<GPID> allocated_pages;
        ErrorContext local_ctx;
        ASSERT_EQ(page_manager->getAllocatedPages(table_info.tablespace_id, allocated_pages, &local_ctx),
                  Status::OK)
            << local_ctx.message;
        for (const auto &gpid : allocated_pages)
        {
            if (getPageNumber(gpid) < 2)
            {
                continue;
            }
            void *page_buffer = nullptr;
            if (db_.buffer_pool()->pinPageGlobal(gpid, &page_buffer, &local_ctx) != Status::OK ||
                page_buffer == nullptr)
            {
                continue;
            }
            auto *page_data = static_cast<uint8_t *>(page_buffer);
            auto *header = reinterpret_cast<scratchbird::core::PageHeader *>(page_data);
            const auto *special = reinterpret_cast<const scratchbird::core::HeapPageSpecial *>(
                page_data + header->page_size - sizeof(scratchbird::core::HeapPageSpecial));
            const bool table_match =
                header->page_type == PAGE_TYPE_HEAP &&
                std::memcmp(special->table_id.bytes.data(),
                            table_info.table_id.bytes.data(),
                            table_info.table_id.bytes.size()) == 0;
            if (table_match)
            {
                scratchbird::core::HeapPage heap_page(page_data,
                                                      db_.page_size(),
                                                      nullptr,
                                                      &db_,
                                                      table_info.table_id);
                for (uint16_t item_id = 0; item_id < heap_page.getItemCount(); ++item_id)
                {
                    const uint8_t *tuple_data = nullptr;
                    uint32_t tuple_size = 0;
                    if (heap_page.getTuple(item_id, &tuple_data, &tuple_size, nullptr) != Status::OK)
                    {
                        continue;
                    }
                    const auto *tuple_header =
                        reinterpret_cast<const scratchbird::core::TupleHeader *>(tuple_data);
                    rows_out.emplace_back(tuple_header->xmin,
                                          tuple_header->xmax,
                                          tuple_header->session_id);
                }
            }
            db_.buffer_pool()->unpinPageGlobal(gpid, false, &local_ctx);
        }
    };
    auto dump_session_temp_tables =
        [&](std::vector<std::tuple<ID, std::string, size_t>> &rows_out)
    {
        std::vector<CatalogManager::TableInfo> temp_tables;
        ErrorContext local_ctx;
        ASSERT_EQ(catalog_->listTemporaryTablesForSession(connection_ctx_->effectiveSessionId(),
                                                          temp_tables,
                                                          &local_ctx),
                  Status::OK)
            << local_ctx.message;
        for (const auto &temp_table : temp_tables)
        {
            std::vector<std::tuple<uint64_t, uint64_t, ID>> temp_rows;
            scan_physical_table_rows(temp_table, temp_rows);
            rows_out.emplace_back(temp_table.table_id,
                                  temp_table.table_name,
                                  temp_rows.size());
        }
    };

    ASSERT_TRUE(compileAndExecute("START TRANSACTION").success());
    ASSERT_TRUE(compileAndExecute(
        "CREATE TEMP TABLE temp_rollback_rows (id INT) ON COMMIT PRESERVE ROWS").success());
    ASSERT_TRUE(compileAndExecute("INSERT INTO temp_rollback_rows VALUES (1)").success());
    CatalogManager::SchemaInfo temp_schema;
    status = catalog_->getSchema("users.alice.temp", temp_schema, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    CatalogManager::TableInfo table_info;
    status = catalog_->getTable(temp_schema.schema_id, "temp_rollback_rows", table_info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    std::vector<std::tuple<uint16_t, uint64_t, uint64_t, ID>> rows_after_insert_one;
    inspect_root_page(table_info, rows_after_insert_one);
    std::vector<std::tuple<uint64_t, uint64_t, ID>> scan_rows_after_insert_one;
    scan_table_rows(table_info.table_id, scan_rows_after_insert_one);
    std::vector<std::tuple<uint64_t, uint64_t, ID>> physical_rows_after_insert_one;
    scan_physical_table_rows(table_info, physical_rows_after_insert_one);
    std::vector<std::tuple<ID, std::string, size_t>> session_temp_tables_after_insert_one;
    dump_session_temp_tables(session_temp_tables_after_insert_one);
    std::ostringstream temp_table_debug;
    temp_table_debug << "session_temp_tables_after_insert_one:";
    for (const auto &[temp_table_id, temp_table_name, temp_row_count] :
         session_temp_tables_after_insert_one)
    {
        temp_table_debug << " [" << temp_table_name
                         << " id=" << temp_table_id.toString()
                         << " rows=" << temp_row_count << "]";
    }
    ASSERT_EQ(physical_rows_after_insert_one.size(), 1u) << temp_table_debug.str();
    ASSERT_EQ(scan_rows_after_insert_one.size(), 1u);
    const uint64_t xid_before_savepoint = connection_ctx_->getCurrentXid();
    ASSERT_NE(xid_before_savepoint, 0u);
    ASSERT_TRUE(compileAndExecute("SAVEPOINT temp_rows_sp").success());
    ASSERT_TRUE(compileAndExecute("INSERT INTO temp_rollback_rows VALUES (2)").success());
    std::vector<std::tuple<uint16_t, uint64_t, uint64_t, ID>> rows_after_insert_two;
    inspect_root_page(table_info, rows_after_insert_two);
    std::vector<std::tuple<uint64_t, uint64_t, ID>> scan_rows_after_insert_two;
    scan_table_rows(table_info.table_id, scan_rows_after_insert_two);
    ASSERT_EQ(scan_rows_after_insert_two.size(), 2u);
    ASSERT_TRUE(compileAndExecute("ROLLBACK TO SAVEPOINT temp_rows_sp").success());
    EXPECT_EQ(connection_ctx_->getCurrentXid(), xid_before_savepoint);
    EXPECT_EQ(connection_ctx_->effectiveSessionId(), session_id_before_savepoint);
    size_t root_live_rows = 0;
    size_t root_matching_session_rows = 0;
    std::vector<std::tuple<uint16_t, uint64_t, uint64_t, ID>> rows_after_rollback;
    inspect_root_page(table_info, rows_after_rollback);
    for (const auto &[item_id, xmin, xmax, session_id] : rows_after_rollback)
    {
        (void)item_id;
        (void)xmin;
        (void)xmax;
        ++root_live_rows;
        if (session_id == session_id_before_savepoint)
        {
            ++root_matching_session_rows;
        }
    }
    EXPECT_GE(root_live_rows, 1u);
    EXPECT_GE(root_matching_session_rows, 1u);

    auto raw_scan = db_.storage_engine()->createScanAll(table_info.table_id, &ctx);
    ASSERT_NE(raw_scan, nullptr);
    size_t raw_row_count = 0;
    uint64_t raw_xmin = 0;
    uint64_t raw_xmax = 0;
    while (true)
    {
        Tuple tuple{};
        auto scan_status = raw_scan->next(&tuple, &ctx);
        if (scan_status == Status::NOT_FOUND)
        {
            break;
        }
        ASSERT_EQ(scan_status, Status::OK) << ctx.message;
        const auto *tuple_header =
            reinterpret_cast<const scratchbird::core::TupleHeader *>(tuple.data);
        raw_xmin = tuple_header->xmin;
        raw_xmax = tuple_header->xmax;
        ++raw_row_count;
    }
    EXPECT_EQ(raw_row_count, 1u);
    EXPECT_EQ(raw_xmin, xid_before_savepoint);
    EXPECT_EQ(raw_xmax, 0u);

    auto plain_select_result =
        compileAndExecute("SELECT id FROM temp_rollback_rows");
    ASSERT_TRUE(plain_select_result.hasResultSet()) << plain_select_result.error();
    ASSERT_EQ(plain_select_result.resultSet()->rowCount(), 1u);
    EXPECT_EQ(plain_select_result.resultSet()->getValue(0, 0).toInt64(), 1);

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
