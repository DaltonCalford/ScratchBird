#include <gtest/gtest.h>
#include <cstring>
#include <filesystem>
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/backup_manager.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/gpid.h"
#include "scratchbird/core/tid.h"

using namespace scratchbird::core;

class TablespaceFlowTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        db_path_ = "/tmp/test_tablespace_flow.db";
        ts_path_ = "/tmp/test_tablespace_flow.sbts";
        std::filesystem::remove(db_path_);
        std::filesystem::remove(ts_path_);

        ErrorContext ctx;
        Status status = Database::create(db_path_, 16384, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to create database: " << ctx.message;
        status = db_.open(db_path_, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to open database: " << ctx.message;
        catalog_ = db_.catalog_manager();
        storage_ = db_.storage_engine();
        ASSERT_NE(catalog_, nullptr);
        ASSERT_NE(storage_, nullptr);
    }

    void TearDown() override
    {
        db_.close();
        std::filesystem::remove(db_path_);
        std::filesystem::remove(ts_path_);
    }

    std::string db_path_;
    std::string ts_path_;
    Database db_;
    CatalogManager* catalog_ = nullptr;
    StorageEngine* storage_ = nullptr;
};

TEST_F(TablespaceFlowTest, CreateTableAndInsertInTablespace)
{
    ErrorContext ctx;
    uint16_t tablespace_id = 0;
    Status status = catalog_->createTablespace("ts_data", ts_path_, true, 1, 0, 2,
                                               tablespace_id, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    CatalogManager::SchemaInfo schema;
    status = catalog_->getSchema("PUBLIC", schema, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    std::vector<CatalogManager::ColumnInfo> columns;
    CatalogManager::ColumnInfo col;
    col.column_id = generateUuidV7();
    col.column_name = "payload";
    col.data_type = static_cast<uint16_t>(DataType::VARCHAR);
    col.type_precision = 64;
    col.max_length = 64;
    col.nullable = false;
    col.ordinal = 0;
    columns.push_back(col);

    ID table_id;
    status = catalog_->createTable(schema.schema_id, "ts_table", columns, table_id,
                                   tablespace_id, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    ID index_id;
    status = catalog_->createIndex(table_id, "ts_table_idx", {"payload"}, index_id, false,
                                   CatalogManager::IndexType::BTREE, tablespace_id, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    CatalogManager::IndexInfo index_info;
    status = catalog_->getIndex(index_id, index_info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(index_info.tablespace_id, tablespace_id);

    std::string payload = "tablespace row";
    std::vector<uint8_t> tuple_buffer(sizeof(TupleHeader) + payload.size(), 0);
    std::memcpy(tuple_buffer.data() + sizeof(TupleHeader), payload.data(), payload.size());
    uint32_t page_id = 0;
    uint16_t item_id = 0;
    status = storage_->insertTuple(table_id,
                                   tuple_buffer.data(),
                                   static_cast<uint32_t>(tuple_buffer.size()),
                                   &page_id, &item_id, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    TID tid(makeGPID(tablespace_id, static_cast<uint64_t>(page_id)), item_id);
    EXPECT_EQ(getTablespaceID(tid.gpid), tablespace_id);

    Tuple tuple{};
    status = storage_->getTuple(table_id, tid, &tuple, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    ASSERT_NE(tuple.data, nullptr);
    EXPECT_GT(tuple.data_size, 0u);
}

TEST_F(TablespaceFlowTest, BackupRestoreWithTablespaceOverride)
{
    ErrorContext ctx;
    uint16_t tablespace_id = 0;
    Status status = catalog_->createTablespace("ts_backup", ts_path_, true, 1, 0, 2,
                                               tablespace_id, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    CatalogManager::SchemaInfo schema;
    status = catalog_->getSchema("PUBLIC", schema, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    std::vector<CatalogManager::ColumnInfo> columns;
    CatalogManager::ColumnInfo col;
    col.column_id = generateUuidV7();
    col.column_name = "payload";
    col.data_type = static_cast<uint16_t>(DataType::VARCHAR);
    col.type_precision = 64;
    col.max_length = 64;
    col.nullable = false;
    col.ordinal = 0;
    columns.push_back(col);

    ID table_id;
    status = catalog_->createTable(schema.schema_id, "ts_backup_table", columns, table_id,
                                   tablespace_id, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    std::string payload = "backup row";
    std::vector<uint8_t> tuple_buffer(sizeof(TupleHeader) + payload.size(), 0);
    std::memcpy(tuple_buffer.data() + sizeof(TupleHeader), payload.data(), payload.size());
    uint32_t page_id = 0;
    uint16_t item_id = 0;
    status = storage_->insertTuple(table_id,
                                   tuple_buffer.data(),
                                   static_cast<uint32_t>(tuple_buffer.size()),
                                   &page_id, &item_id, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    std::string backup_path = "/tmp/test_tablespace_flow.sbk";
    std::filesystem::remove(backup_path);

    auto *bp = db_.buffer_pool();
    ASSERT_NE(bp, nullptr);
    status = bp->flushAll(&ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    BackupManager backup_mgr(&db_);
    BackupConfig backup_cfg;
    backup_cfg.compression = CompressionType::NONE;
    status = backup_mgr.createBackup(backup_path, backup_cfg, nullptr, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    std::string restored_db_path = "/tmp/test_tablespace_flow_restore.db";
    std::string restored_ts_path = "/tmp/test_tablespace_flow_restore.sbts";
    std::filesystem::remove(restored_db_path);
    std::filesystem::remove(restored_ts_path);

    RestoreConfig restore_cfg;
    restore_cfg.allow_tablespace_create = true;
    restore_cfg.tablespace_path_overrides[tablespace_id] = {restored_ts_path};
    status = backup_mgr.restoreBackup(backup_path, restored_db_path, restore_cfg, nullptr, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    Database restored_db;
    status = restored_db.open(restored_db_path, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    TablespaceInfo restored_info;
    status = restored_db.catalog_manager()->getTablespace(tablespace_id, restored_info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    ASSERT_FALSE(restored_info.file_paths.empty());
    EXPECT_EQ(restored_info.file_paths.front(), restored_ts_path);

    restored_db.close();
    std::filesystem::remove(backup_path);
    std::filesystem::remove(restored_db_path);
    std::filesystem::remove(restored_ts_path);
}
