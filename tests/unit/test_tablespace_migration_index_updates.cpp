#include <gtest/gtest.h>
#include "scratchbird/core/bitmap_index.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/columnstore.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/gpid.h"
#include "scratchbird/core/lsm_tree_index.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/spgist_index.h"
#include "scratchbird/core/tid.h"
#include "scratchbird/core/transaction_manager.h"
#include <atomic>
#include <cstring>
#include <filesystem>
#include <string>
#include <unistd.h>
#include <vector>

using namespace scratchbird::core;

namespace {
CatalogManager::ColumnInfo makeColumn(const std::string &name, uint16_t type, uint32_t precision,
                                      bool nullable)
{
    CatalogManager::ColumnInfo col{};
    col.column_name = name;
    col.data_type = type;
    col.type_precision = precision;
    col.max_length = precision;
    col.nullable = nullable;
    return col;
}

std::vector<uint8_t> encodeLsmValue(const TID &tid)
{
    std::vector<uint8_t> value(sizeof(uint64_t) + sizeof(uint16_t));
    uint64_t gpid = tid.gpid;
    for (size_t i = 0; i < sizeof(uint64_t); ++i)
    {
        value[i] = static_cast<uint8_t>((gpid >> (i * 8)) & 0xFF);
    }
    value[sizeof(uint64_t)] = static_cast<uint8_t>(tid.slot & 0xFF);
    value[sizeof(uint64_t) + 1] = static_cast<uint8_t>((tid.slot >> 8) & 0xFF);
    return value;
}

uint64_t decodeLsmGpid(const std::vector<uint8_t> &value)
{
    if (value.size() < sizeof(uint64_t))
    {
        return 0;
    }
    uint64_t gpid = 0;
    for (size_t i = 0; i < sizeof(uint64_t); ++i)
    {
        gpid |= (static_cast<uint64_t>(value[i]) << (i * 8));
    }
    return gpid;
}
} // namespace

class TablespaceMigrationIndexUpdateTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        test_id_ = std::to_string(getpid()) + "_" + std::to_string(test_counter_++);
        db_path_ = "/tmp/test_ts_migration_" + test_id_ + ".db";
        std::filesystem::remove(db_path_);

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_path_, 8192, &ctx), Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(db_path_, &ctx), Status::OK) << ctx.message;

        ASSERT_EQ(db_->connect(conn_, &ctx), Status::OK) << ctx.message;
        ConnectionContext::setCurrent(conn_.get());
    }

    void TearDown() override
    {
        ConnectionContext::setCurrent(nullptr);
        conn_.reset();
        if (db_)
        {
            db_->close();
            db_.reset();
        }

        for (const auto &path : temp_paths_)
        {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
        }
        std::filesystem::remove(db_path_);
    }

    void trackTempPath(const std::string &path)
    {
        temp_paths_.push_back(path);
    }

    std::unique_ptr<Database> db_;
    std::unique_ptr<ConnectionContext> conn_;
    std::string db_path_;
    std::string test_id_;
    std::vector<std::string> temp_paths_;
    static inline std::atomic<int> test_counter_{0};
};

TEST_F(TablespaceMigrationIndexUpdateTest, TableRootUsesTablespaceAllocation)
{
    auto *page_mgr = db_->page_manager();
    auto *catalog = db_->catalog_manager();
    ASSERT_NE(page_mgr, nullptr);
    ASSERT_NE(catalog, nullptr);

    std::string ts_path = "/tmp/test_ts_table_root_" + test_id_ + ".sbts";
    trackTempPath(ts_path);

    TablespaceConfig config;
    config.autoextend_enabled = true;
    config.autoextend_size_mb = 1;
    config.max_size_mb = 8;
    config.prealloc_pages = 2;

    ErrorContext ctx;
    ASSERT_EQ(page_mgr->createTablespace(1, "ts_table_root", ts_path.c_str(), config, &ctx), Status::OK)
        << ctx.message;

    ID schema_id;
    ASSERT_EQ(catalog->createSchema("ts_schema", "SYSARCH", schema_id, &ctx), Status::OK)
        << ctx.message;

    std::vector<CatalogManager::ColumnInfo> columns;
    columns.push_back(makeColumn("id", static_cast<uint16_t>(DataType::INT32), 4, false));

    ID table_id;
    ASSERT_EQ(catalog->createTable(schema_id, "ts_table", columns, table_id, 1, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::TableInfo table_info;
    ASSERT_EQ(catalog->getTable(table_id, table_info, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(table_info.tablespace_id, 1);
    EXPECT_EQ(getTablespaceID(table_info.root_gpid), 1);
}

TEST_F(TablespaceMigrationIndexUpdateTest, IndexRootUsesTablespaceAllocation)
{
    auto *page_mgr = db_->page_manager();
    auto *catalog = db_->catalog_manager();
    ASSERT_NE(page_mgr, nullptr);
    ASSERT_NE(catalog, nullptr);

    std::string ts_path = "/tmp/test_ts_index_root_" + test_id_ + ".sbts";
    trackTempPath(ts_path);

    TablespaceConfig config;
    config.autoextend_enabled = true;
    config.autoextend_size_mb = 1;
    config.max_size_mb = 8;
    config.prealloc_pages = 2;

    ErrorContext ctx;
    ASSERT_EQ(page_mgr->createTablespace(2, "ts_index_root", ts_path.c_str(), config, &ctx), Status::OK)
        << ctx.message;

    ID schema_id;
    ASSERT_EQ(catalog->createSchema("ts_index_schema", "SYSARCH", schema_id, &ctx), Status::OK)
        << ctx.message;

    std::vector<CatalogManager::ColumnInfo> columns;
    columns.push_back(makeColumn("id", static_cast<uint16_t>(DataType::INT32), 4, false));

    ID table_id;
    ASSERT_EQ(catalog->createTable(schema_id, "ts_index_table", columns, table_id, 2, &ctx), Status::OK)
        << ctx.message;

    ID index_id;
    std::vector<std::string> column_names{"id"};
    std::vector<std::string> include_names;
    ASSERT_EQ(catalog->createIndex(table_id, "ts_index", column_names, include_names, index_id,
                                   false, CatalogManager::IndexType::BTREE, 2, &ctx),
              Status::OK)
        << ctx.message;

    CatalogManager::IndexInfo index_info;
    ASSERT_EQ(catalog->getIndex(index_id, index_info, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(index_info.tablespace_id, 2);
    EXPECT_EQ(getTablespaceID(index_info.root_gpid), 2);
}

TEST_F(TablespaceMigrationIndexUpdateTest, BitmapIndexUpdatesTidsAfterMigration)
{
    auto *page_mgr = db_->page_manager();
    ASSERT_NE(page_mgr, nullptr);

    ErrorContext ctx;
    GPID meta_gpid = 0;
    ASSERT_EQ(page_mgr->allocatePageInTablespace(0, &meta_gpid, &ctx), Status::OK) << ctx.message;

    UuidV7Bytes index_uuid = generateUuidV7();
    ASSERT_EQ(BitmapIndex::create(db_.get(), index_uuid, meta_gpid, &ctx), Status::OK) << ctx.message;

    auto index = BitmapIndex::open(db_.get(), index_uuid, meta_gpid, &ctx);
    ASSERT_NE(index, nullptr);

    const char *key = "bitmap_key";
    TID tid(makeGPID(3, 100), 7);
    ASSERT_EQ(index->insert(key, std::strlen(key), tid, &ctx), Status::OK) << ctx.message;

    uint64_t current_xid = ConnectionContext::getCurrentTransactionId();
    std::vector<TID> results;
    ASSERT_EQ(index->find(key, std::strlen(key), current_xid, &results, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].gpid, tid.gpid);

    GPID new_gpid = makeGPID(7, 500);
    std::unordered_map<uint64_t, uint64_t> mapping{{tid.gpid, new_gpid}};
    uint64_t tids_updated = 0;
    uint64_t pages_modified = 0;
    ASSERT_EQ(index->updateTIDsAfterMigration(mapping, &tids_updated, &pages_modified, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(tids_updated, 1u);

    results.clear();
    ASSERT_EQ(index->find(key, std::strlen(key), current_xid, &results, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].gpid, new_gpid);
}

TEST_F(TablespaceMigrationIndexUpdateTest, SpGiSTIndexUpdatesTidsAfterMigration)
{
    auto *page_mgr = db_->page_manager();
    ASSERT_NE(page_mgr, nullptr);

    ErrorContext ctx;
    GPID root_gpid = 0;
    ASSERT_EQ(page_mgr->allocatePageInTablespace(0, &root_gpid, &ctx), Status::OK) << ctx.message;

    auto &registry = SPGiSTOperatorClassRegistry::instance();
    auto opclass = registry.getOperatorClass(0);
    ASSERT_NE(opclass, nullptr);

    ID table_uuid = generateUuidV7();
    std::vector<ID> column_ids{generateUuidV7()};
    ID index_uuid = generateUuidV7();
    ASSERT_EQ(SPGiSTIndex::create(db_.get(), index_uuid, table_uuid, column_ids, opclass, root_gpid, &ctx),
              Status::OK)
        << ctx.message;

    auto index = SPGiSTIndex::open(db_.get(), index_uuid, table_uuid, column_ids, opclass, root_gpid, &ctx);
    ASSERT_NE(index, nullptr);

    uint64_t xid = 0;
    ASSERT_NE(conn_, nullptr);
    ASSERT_EQ(db_->transaction_manager()->beginTransaction(conn_->getProcId(), xid, &ctx), Status::OK)
        << ctx.message;

    std::vector<uint8_t> value{0x01, 0x02, 0x03};
    TID tid(makeGPID(4, 222), 9);
    ASSERT_EQ(index->insert(value, tid, xid, &ctx), Status::OK) << ctx.message;

    std::vector<TID> results;
    ASSERT_EQ(index->search(value, xid, &results, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].gpid, tid.gpid);

    GPID new_gpid = makeGPID(9, 777);
    std::unordered_map<uint64_t, uint64_t> mapping{{tid.gpid, new_gpid}};
    uint64_t tids_updated = 0;
    uint64_t pages_modified = 0;
    ASSERT_EQ(index->updateTIDsAfterMigration(mapping, &tids_updated, &pages_modified, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(tids_updated, 1u);

    results.clear();
    ASSERT_EQ(index->search(value, xid, &results, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].gpid, new_gpid);
}

TEST_F(TablespaceMigrationIndexUpdateTest, ColumnstoreUpdatesSegmentTidsAfterMigration)
{
    auto *page_mgr = db_->page_manager();
    auto *catalog = db_->catalog_manager();
    auto *buffer_pool = db_->buffer_pool();
    ASSERT_NE(page_mgr, nullptr);
    ASSERT_NE(catalog, nullptr);
    ASSERT_NE(buffer_pool, nullptr);

    ErrorContext ctx;
    ID schema_id;
    ASSERT_EQ(catalog->createSchema("cs_schema", "SYSARCH", schema_id, &ctx), Status::OK)
        << ctx.message;

    std::vector<CatalogManager::ColumnInfo> columns;
    columns.push_back(makeColumn("value", static_cast<uint16_t>(DataType::INT32), 4, false));
    ID table_id;
    ASSERT_EQ(catalog->createTable(schema_id, "cs_table", columns, table_id, 0, &ctx), Status::OK)
        << ctx.message;

    std::vector<CatalogManager::ColumnInfo> table_columns;
    ASSERT_EQ(catalog->getColumns(table_id, table_columns, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(table_columns.size(), 1u);
    ID column_id = table_columns[0].column_id;

    GPID root_gpid = 0;
    ASSERT_EQ(page_mgr->allocatePageInTablespace(0, &root_gpid, &ctx), Status::OK) << ctx.message;

    ID index_uuid = generateUuidV7();
    ASSERT_EQ(ColumnstoreIndex::create(db_.get(), index_uuid, table_id, {column_id},
                                       1, CompressionType::RLE, root_gpid, &ctx),
              Status::OK) << ctx.message;

    auto index = ColumnstoreIndex::open(db_.get(), index_uuid, root_gpid, 1, &ctx);
    ASSERT_NE(index, nullptr);

    int32_t value = 42;
    TID tid(makeGPID(2, 333), 1);
    ASSERT_EQ(index->insert(column_id, tid.gpid, &value, sizeof(value), false, &ctx), Status::OK)
        << ctx.message;

    SBColumnstoreMetadataPage *meta = nullptr;
    ASSERT_EQ(buffer_pool->pinPageGlobal(root_gpid, reinterpret_cast<void **>(&meta), &ctx), Status::OK)
        << ctx.message;
    uint32_t segment_page = meta->cs_first_segment_page;
    buffer_pool->unpinPageGlobal(root_gpid, false, &ctx);
    ASSERT_NE(segment_page, 0u);

    GPID segment_gpid = makeGPID(getTablespaceID(root_gpid), segment_page);
    SBColumnstorePage *segment = nullptr;
    ASSERT_EQ(buffer_pool->pinPageGlobal(segment_gpid, reinterpret_cast<void **>(&segment), &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(segment->cs_first_tid, tid.gpid);
    buffer_pool->unpinPageGlobal(segment_gpid, false, &ctx);

    GPID new_gpid = makeGPID(6, 444);
    std::unordered_map<uint64_t, uint64_t> mapping{{tid.gpid, new_gpid}};
    uint64_t tids_updated = 0;
    uint64_t pages_modified = 0;
    ASSERT_EQ(index->updateTIDsAfterMigration(mapping, &tids_updated, &pages_modified, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(tids_updated, 1u);

    ASSERT_EQ(buffer_pool->pinPageGlobal(segment_gpid, reinterpret_cast<void **>(&segment), &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(segment->cs_first_tid, new_gpid);
    buffer_pool->unpinPageGlobal(segment_gpid, false, &ctx);
}

TEST_F(TablespaceMigrationIndexUpdateTest, LsmIndexUpdatesValuesAfterMigration)
{
    std::string lsm_path = "/tmp/test_lsm_index_" + test_id_;
    trackTempPath(lsm_path);

    auto *txn_mgr = db_->transaction_manager();
    ASSERT_NE(txn_mgr, nullptr);

    LSMTreeIndex lsm(db_.get(), lsm_path, txn_mgr, 4);
    ErrorContext ctx;
    ASSERT_EQ(lsm.create(&ctx), Status::OK) << ctx.message;

    uint64_t xid = 0;
    ASSERT_NE(conn_, nullptr);
    ASSERT_EQ(txn_mgr->beginTransaction(conn_->getProcId(), xid, &ctx), Status::OK) << ctx.message;

    std::vector<uint8_t> key{0x10};
    TID tid(makeGPID(5, 909), 12);
    auto value = encodeLsmValue(tid);
    ASSERT_EQ(lsm.put(key, value, xid, &ctx), Status::OK) << ctx.message;

    ASSERT_EQ(lsm.flush(&ctx), Status::OK) << ctx.message;

    GPID new_gpid = makeGPID(8, 707);
    std::unordered_map<uint64_t, uint64_t> mapping{{tid.gpid, new_gpid}};
    uint64_t tids_updated = 0;
    uint64_t files_modified = 0;
    ASSERT_EQ(lsm.updateTIDsAfterMigration(mapping, &tids_updated, &files_modified, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(tids_updated, 1u);
    EXPECT_GE(files_modified, 1u);

    bool found = false;
    std::vector<uint8_t> loaded_value;
    ASSERT_EQ(lsm.get(key, xid, &loaded_value, &found, &ctx), Status::OK) << ctx.message;
    ASSERT_TRUE(found);
    EXPECT_EQ(decodeLsmGpid(loaded_value), new_gpid);
}
