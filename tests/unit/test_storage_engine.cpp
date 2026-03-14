#include <gtest/gtest.h>
#include <cstring>
#include <filesystem>
#include <memory>
#include <vector>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/config.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/btree.h"
#include "scratchbird/core/garbage_collector.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/lock_manager.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/toast.h"
#include "scratchbird/core/types.h"
#include "test_helpers.h"

using namespace scratchbird::core;

class StorageEngineTest : public ::testing::Test
{
protected:
    static constexpr uint32_t kPageSize = 8192;

    void SetUp() override
    {
        test_db_path_ = scratchbird::testing::uniqueTestDbPath("test_storage_engine", ".db");
        std::filesystem::remove(test_db_path_);

        ErrorContext ctx;
        ASSERT_EQ(Database::create(test_db_path_, kPageSize, &ctx), Status::OK)
            << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(test_db_path_, &ctx), Status::OK)
            << ctx.message;

        Status status = db_->initializeProcArray(16, &ctx);
        if (status != Status::OK && status != Status::INVALID_ARGUMENT)
        {
            ASSERT_EQ(status, Status::OK) << ctx.message;
        }

        ASSERT_EQ(db_->connect(conn_ctx_, &ctx), Status::OK)
            << ctx.message;
        ConnectionContext::setCurrent(conn_ctx_.get());
        ASSERT_EQ(conn_ctx_->initialize(&ctx), Status::OK)
            << ctx.message;

        ID system_user = db_->catalog_manager()->getSystemUserId(&ctx);
        conn_ctx_->setCurrentUser(system_user, true);

        schema_id_ = resolveDefaultSchema(&ctx);
        ASSERT_NE(schema_id_, ID{});

        engine_ = db_->storage_engine();
        ASSERT_NE(engine_, nullptr);
    }

    void TearDown() override
    {
        ConnectionContext::setCurrent(nullptr);
        conn_ctx_.reset();
        db_.reset();
        std::filesystem::remove(test_db_path_);
    }

    ID resolveDefaultSchema(ErrorContext *ctx)
    {
        std::vector<CatalogManager::SchemaInfo> schemas;
        Status status = db_->catalog_manager()->listSchemas(schemas, ctx);
        if (status == Status::OK && !schemas.empty())
        {
            return schemas.front().schema_id;
        }

        ID schema_id;
        status = db_->catalog_manager()->createSchema("main", "SYSTEM", schema_id, ctx);
        if (status == Status::OK)
        {
            return schema_id;
        }
        return ID{};
    }

    ID createTestTable(const std::string &name)
    {
        ErrorContext ctx;
        std::vector<CatalogManager::ColumnInfo> columns;

        CatalogManager::ColumnInfo id_col;
        id_col.column_name = "id";
        id_col.ordinal = 1;
        id_col.data_type = static_cast<uint16_t>(DataType::INT32);
        id_col.type_precision = 4;
        id_col.nullable = false;
        columns.push_back(id_col);

        CatalogManager::ColumnInfo value_col;
        value_col.column_name = "value";
        value_col.ordinal = 2;
        value_col.data_type = static_cast<uint16_t>(DataType::VARCHAR);
        value_col.type_precision = 100;
        value_col.nullable = true;
        columns.push_back(value_col);

        ID table_id;
        Status status =
            db_->catalog_manager()->createTable(schema_id_, name, columns, table_id, 0, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        return table_id;
    }

    ID createSingleIntTable(const std::string &name)
    {
        ErrorContext ctx;
        std::vector<CatalogManager::ColumnInfo> columns;

        CatalogManager::ColumnInfo id_col;
        id_col.column_name = "id";
        id_col.ordinal = 1;
        id_col.data_type = static_cast<uint16_t>(DataType::INT32);
        id_col.type_precision = 4;
        id_col.nullable = false;
        columns.push_back(id_col);

        ID table_id;
        Status status =
            db_->catalog_manager()->createTable(schema_id_, name, columns, table_id, 0, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        return table_id;
    }

    ID createSingleIntIndex(const ID &table_id,
                            const std::string &index_name,
                            bool is_unique)
    {
        ErrorContext ctx;
        ID index_id;
        Status status = db_->catalog_manager()->createIndex(
            table_id, index_name, {"id"}, index_id, is_unique,
            CatalogManager::IndexType::BTREE, PRIMARY_TABLESPACE_ID, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        return index_id;
    }

    std::vector<uint8_t> buildIntTuple(int32_t value,
                                       uint64_t xmin = config::DEFAULT_INITIAL_XID)
    {
        return buildTuple(&value, sizeof(value), xmin);
    }

    std::vector<uint8_t> encodeIntKey(int32_t value)
    {
        std::vector<uint8_t> key(sizeof(value), 0);
        std::memcpy(key.data(), &value, sizeof(value));
        return key;
    }

    std::vector<uint8_t> buildTuple(const void *payload, size_t payload_size,
                                    uint64_t xmin = config::DEFAULT_INITIAL_XID)
    {
        TupleHeader header{};
        header.xmin = xmin;
        header.xmax = 0;
        header.back_version_gpid = INVALID_GPID;
        header.back_version_slot = 0;
        header.ctid_gpid = INVALID_GPID;
        header.ctid_slot = 0;
        header.infomask = 0;
        header.null_bitmap_offset = 0;
        header.padding = 0;
        header.session_id = ID{};

        std::vector<uint8_t> tuple(sizeof(TupleHeader) + payload_size, 0);
        std::memcpy(tuple.data(), &header, sizeof(TupleHeader));
        if (payload_size > 0)
        {
            std::memcpy(tuple.data() + sizeof(TupleHeader), payload, payload_size);
        }
        return tuple;
    }

    std::vector<uint8_t> buildLargeRowTuple(int32_t value, char fill, size_t payload_size = 5000)
    {
        std::vector<uint8_t> payload(sizeof(value) + payload_size, 0);
        std::memcpy(payload.data(), &value, sizeof(value));
        std::memset(payload.data() + sizeof(value), fill, payload_size);
        return buildTuple(payload.data(), payload.size());
    }

    std::vector<uint8_t> buildFilledTuple(size_t payload_size, uint8_t fill)
    {
        std::vector<uint8_t> payload(payload_size, fill);
        return buildTuple(payload.data(), payload.size());
    }

    void expectTuplePayloadEquals(const std::vector<uint8_t> &actual_tuple,
                                  const std::vector<uint8_t> &expected_tuple)
    {
        ASSERT_GE(actual_tuple.size(), sizeof(TupleHeader));
        ASSERT_GE(expected_tuple.size(), sizeof(TupleHeader));
        const size_t actual_payload_size = actual_tuple.size() - sizeof(TupleHeader);
        const size_t expected_payload_size = expected_tuple.size() - sizeof(TupleHeader);
        ASSERT_EQ(actual_payload_size, expected_payload_size);
        EXPECT_EQ(std::memcmp(actual_tuple.data() + sizeof(TupleHeader),
                              expected_tuple.data() + sizeof(TupleHeader),
                              expected_payload_size),
                  0);
    }

    std::vector<uint8_t> readTupleDetoasted(const ID &table_id, uint32_t page_id, uint16_t item_id)
    {
        ErrorContext ctx;
        void *page_buffer = nullptr;
        EXPECT_EQ(db_->buffer_pool()->pinPage(page_id, &page_buffer, &ctx), Status::OK)
            << ctx.message;

        ToastManager toast_mgr(db_.get(), table_id);
        EXPECT_EQ(toast_mgr.initialize(&ctx), Status::OK) << ctx.message;

        HeapPage heap_page(static_cast<uint8_t *>(page_buffer), kPageSize, &toast_mgr, db_.get(),
                           table_id);
        std::vector<uint8_t> tuple;
        Status status = heap_page.getTupleDetoasted(item_id, &tuple, conn_ctx_->getCurrentXid(), &ctx);
        db_->buffer_pool()->unpinPage(page_id, false, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        return tuple;
    }

    void fillPageAlmostFull(uint32_t page_id, size_t payload_size = 64)
    {
        ErrorContext ctx;
        void *page_buffer = nullptr;
        ASSERT_EQ(db_->buffer_pool()->pinPage(page_id, &page_buffer, &ctx), Status::OK)
            << ctx.message;

        auto *page_data = static_cast<uint8_t *>(page_buffer);
        HeapPage heap_page(page_data, kPageSize);
        const uint32_t target_free_space = 260;
        auto tuple_data = buildFilledTuple(payload_size, 0xAB);

        while (heap_page.getFreeSpace() >
               target_free_space + tuple_data.size() + sizeof(ItemPointer))
        {
            uint16_t filler_item_id = 0;
            Status status = heap_page.insertTuple(tuple_data.data(),
                                                  static_cast<uint32_t>(tuple_data.size()),
                                                  conn_ctx_->getCurrentXid(),
                                                  &filler_item_id,
                                                  &ctx);
            if (status != Status::OK)
            {
                break;
            }
        }

        db_->buffer_pool()->unpinPage(page_id, true, &ctx);
    }

    std::vector<uint8_t> buildCrossPageTriggerTuple(uint32_t page_id,
                                                    uint16_t item_id,
                                                    uint8_t fill)
    {
        ErrorContext ctx;
        void *page_buffer = nullptr;
        EXPECT_EQ(db_->buffer_pool()->pinPage(page_id, &page_buffer, &ctx), Status::OK)
            << ctx.message;

        auto *page_data = static_cast<uint8_t *>(page_buffer);
        HeapPage heap_page(page_data, kPageSize);
        const uint8_t *old_tuple = nullptr;
        uint32_t old_tuple_size = 0;
        Status status = heap_page.getTuple(item_id, &old_tuple, &old_tuple_size, &ctx);
        uint32_t free_space = heap_page.getFreeSpace();
        db_->buffer_pool()->unpinPage(page_id, false, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;

        const uint32_t min_total = sizeof(TupleHeader) + 8;
        uint32_t chosen_total = min_total;

        if (free_space > 8)
        {
            uint32_t candidate_total = free_space - 8;
            if (candidate_total > 240)
            {
                candidate_total = 240;
            }
            if (candidate_total > old_tuple_size && candidate_total >= min_total)
            {
                chosen_total = candidate_total;
            }
        }

        if (chosen_total == min_total)
        {
            uint32_t max_total = (free_space > 8) ? (free_space - 8) : free_space;
            if (max_total > 240)
            {
                max_total = 240;
            }
            if (max_total < min_total)
            {
                max_total = min_total;
            }

            uint32_t cross_page_min_total = min_total;
            if (free_space > old_tuple_size)
            {
                cross_page_min_total = free_space - old_tuple_size + 1;
            }

            chosen_total = max_total;
            if (cross_page_min_total <= max_total)
            {
                chosen_total = cross_page_min_total;
            }
        }

        return buildFilledTuple(chosen_total - sizeof(TupleHeader), fill);
    }

    std::vector<uint8_t> buildCrossPageTriggerLargeRowTuple(int32_t value,
                                                            uint32_t page_id,
                                                            uint16_t item_id,
                                                            uint8_t fill)
    {
        auto tuple = buildCrossPageTriggerTuple(page_id, item_id, fill);
        EXPECT_GE(tuple.size(), sizeof(TupleHeader) + sizeof(value));
        std::memcpy(tuple.data() + sizeof(TupleHeader), &value, sizeof(value));
        return tuple;
    }

    std::vector<uint8_t> buildLargeIntTuple(int32_t value, uint8_t fill, size_t payload_size = 5000)
    {
        auto tuple = buildFilledTuple(sizeof(value) + payload_size, fill);
        std::memcpy(tuple.data() + sizeof(TupleHeader), &value, sizeof(value));
        return tuple;
    }

    void expectIndexSeekFindsTid(const ID &index_id, int32_t value, const TID &expected_tid)
    {
        ErrorContext ctx;
        auto scan = engine_->createIndexScan(index_id, &ctx);
        ASSERT_NE(scan, nullptr) << ctx.message;

        auto key = encodeIntKey(value);
        ASSERT_EQ(scan->seek(key, &ctx), Status::OK) << ctx.message;

        Tuple found{};
        ASSERT_EQ(scan->next(&found, &ctx), Status::OK) << ctx.message;
        EXPECT_EQ(found.tid, expected_tid);
        EXPECT_EQ(scan->next(&found, &ctx), Status::NOT_FOUND);
    }

    void expectIndexSeekNotFound(const ID &index_id, int32_t value)
    {
        ErrorContext ctx;
        auto scan = engine_->createIndexScan(index_id, &ctx);
        ASSERT_NE(scan, nullptr) << ctx.message;

        auto key = encodeIntKey(value);
        ASSERT_EQ(scan->seek(key, &ctx), Status::OK) << ctx.message;

        Tuple found{};
        EXPECT_EQ(scan->next(&found, &ctx), Status::NOT_FOUND);
    }

    std::vector<TID> rawBTreeSearch(const ID &index_id, int32_t value, uint64_t current_xid)
    {
        ErrorContext ctx;
        CatalogManager::IndexInfo index_info;
        EXPECT_EQ(db_->catalog_manager()->getIndex(index_id, index_info, &ctx), Status::OK)
            << ctx.message;

        auto btree = BTree::open(db_.get(), index_id, index_info.root_gpid, &ctx);
        EXPECT_NE(btree, nullptr) << ctx.message;

        std::vector<TID> tids;
        auto key = encodeIntKey(value);
        Status status = btree->search(key, current_xid, &tids, &ctx);
        if (status == Status::NOT_FOUND)
        {
            return {};
        }
        EXPECT_EQ(status, Status::OK) << ctx.message;
        return tids;
    }

    std::string test_db_path_;
    std::unique_ptr<Database> db_;
    std::unique_ptr<ConnectionContext> conn_ctx_;
    StorageEngine *engine_ = nullptr;
    ID schema_id_{};
};

TEST_F(StorageEngineTest, HeapPageBasics)
{
    ErrorContext ctx;
    std::vector<uint8_t> page_buffer(kPageSize, 0);

    HeapPage page(page_buffer.data(), kPageSize);
    ASSERT_EQ(page.initialize(100, &ctx), Status::OK);

    EXPECT_EQ(PAGE_TYPE_HEAP, page.header()->page_type);
    EXPECT_EQ(100u, page.header()->page_id);
    EXPECT_EQ(0u, page.getItemCount());

    uint32_t expected_free = kPageSize - sizeof(PageHeader) - sizeof(HeapPageSpecial);
    EXPECT_NEAR(expected_free, page.getFreeSpace(), 8);
}

TEST_F(StorageEngineTest, InsertAndGetTuple)
{
    ID table_id = createTestTable("test_table");

    struct TestData
    {
        int32_t id;
        char value[100];
    } test_data = {1, "Hello World"};

    auto tuple = buildTuple(&test_data, sizeof(TestData));

    uint32_t page_id = 0;
    uint16_t item_id = 0;
    ErrorContext ctx;
    ASSERT_EQ(engine_->insertTuple(table_id, tuple.data(),
                                   static_cast<uint32_t>(tuple.size()),
                                   &page_id, &item_id, &ctx),
              Status::OK) << ctx.message;

    Tuple retrieved{};
    ASSERT_EQ(engine_->getTuple(page_id, item_id, &retrieved, &ctx), Status::OK)
        << ctx.message;

    ASSERT_GE(retrieved.data_size, sizeof(TupleHeader) + sizeof(TestData));
    const uint8_t *payload = retrieved.data + sizeof(TupleHeader);
    const auto *retrieved_data = reinterpret_cast<const TestData *>(payload);
    EXPECT_EQ(retrieved_data->id, test_data.id);
    EXPECT_STREQ(retrieved_data->value, test_data.value);
}

TEST_F(StorageEngineTest, DeleteMarksTupleHeader)
{
    ID table_id = createTestTable("delete_test");

    struct TestData
    {
        int32_t id;
        char value[100];
    } test_data = {1, "To be deleted"};

    auto tuple = buildTuple(&test_data, sizeof(TestData));

    uint32_t page_id = 0;
    uint16_t item_id = 0;
    ErrorContext ctx;
    ASSERT_EQ(engine_->insertTuple(table_id, tuple.data(),
                                   static_cast<uint32_t>(tuple.size()),
                                   &page_id, &item_id, &ctx),
              Status::OK) << ctx.message;

    ASSERT_EQ(engine_->deleteTuple(table_id, page_id, item_id, UINT16_MAX, &ctx), Status::OK)
        << ctx.message;

    Tuple retrieved{};
    EXPECT_EQ(engine_->getTuple(page_id, item_id, &retrieved, &ctx), Status::NOT_FOUND)
        << "Deleted tuple should not be visible";
}

TEST_F(StorageEngineTest, SequentialScan)
{
    ID table_id = createTestTable("scan_test");

    struct TestData
    {
        int32_t id;
        char value[32];
    };
    std::vector<TestData> rows = {
        {1, "Tuple 1"},
        {2, "Tuple 2"},
        {3, "Tuple 3"},
    };

    ErrorContext ctx;
    for (const auto &row : rows)
    {
        auto tuple = buildTuple(&row, sizeof(TestData));
        uint32_t page_id = 0;
        uint16_t item_id = 0;
        ASSERT_EQ(engine_->insertTuple(table_id, tuple.data(),
                                       static_cast<uint32_t>(tuple.size()),
                                       &page_id, &item_id, &ctx),
                  Status::OK) << ctx.message;
    }

    auto scanner = engine_->createScan(table_id, &ctx);
    ASSERT_NE(scanner, nullptr);

    Tuple tuple{};
    size_t count = 0;
    while (scanner->next(&tuple, &ctx) == Status::OK)
    {
        const uint8_t *payload = tuple.data + sizeof(TupleHeader);
        const auto *data = reinterpret_cast<const TestData *>(payload);
        EXPECT_EQ(data->id, rows[count].id);
        EXPECT_STREQ(data->value, rows[count].value);
        count++;
    }
    EXPECT_EQ(count, rows.size());
}

TEST_F(StorageEngineTest, IndexScanFiltersHeapInvisibleOrMismatchedCandidates)
{
    ErrorContext ctx;
    ID table_id = createSingleIntTable("index_scan_visibility");
    ID index_id = createSingleIntIndex(table_id, "idx_index_scan_visibility", false);

    auto tuple = buildIntTuple(20);
    uint32_t page_id = 0;
    uint16_t item_id = 0;
    ASSERT_EQ(engine_->insertTuple(table_id, tuple.data(), static_cast<uint32_t>(tuple.size()),
                                   &page_id, &item_id, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::IndexInfo index_info;
    ASSERT_EQ(db_->catalog_manager()->getIndex(index_id, index_info, &ctx), Status::OK)
        << ctx.message;
    auto btree = BTree::open(db_.get(), index_id, index_info.root_gpid, &ctx);
    ASSERT_NE(btree, nullptr);

    TID live_tid(makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(page_id)), item_id);
    auto stale_key = encodeIntKey(10);
    ASSERT_EQ(btree->insert(stale_key, live_tid, engine_->getCurrentXid(), &ctx), Status::OK)
        << ctx.message;

    auto scan = engine_->createIndexScan(index_id, &ctx);
    ASSERT_NE(scan, nullptr);
    ASSERT_EQ(scan->seek(stale_key, &ctx), Status::OK) << ctx.message;

    Tuple found{};
    EXPECT_EQ(scan->next(&found, &ctx), Status::NOT_FOUND);
}

TEST_F(StorageEngineTest, UniqueInsertIgnoresStaleIndexEntryWithMismatchedHeapKey)
{
    ErrorContext ctx;
    ID table_id = createSingleIntTable("unique_stale_index");
    ID index_id = createSingleIntIndex(table_id, "uq_unique_stale_index", true);

    auto first_tuple = buildIntTuple(20);
    uint32_t first_page_id = 0;
    uint16_t first_item_id = 0;
    ASSERT_EQ(engine_->insertTuple(table_id, first_tuple.data(),
                                   static_cast<uint32_t>(first_tuple.size()),
                                   &first_page_id, &first_item_id, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::IndexInfo index_info;
    ASSERT_EQ(db_->catalog_manager()->getIndex(index_id, index_info, &ctx), Status::OK)
        << ctx.message;
    auto btree = BTree::open(db_.get(), index_id, index_info.root_gpid, &ctx);
    ASSERT_NE(btree, nullptr);

    TID stale_tid(makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(first_page_id)),
                  first_item_id);
    auto stale_key = encodeIntKey(10);
    ASSERT_EQ(btree->insert(stale_key, stale_tid, engine_->getCurrentXid(), &ctx), Status::OK)
        << ctx.message;

    auto second_tuple = buildIntTuple(10);
    uint32_t second_page_id = 0;
    uint16_t second_item_id = 0;
    ASSERT_EQ(engine_->insertTuple(table_id, second_tuple.data(),
                                   static_cast<uint32_t>(second_tuple.size()),
                                   &second_page_id, &second_item_id, &ctx), Status::OK)
        << ctx.message;

    auto scan = engine_->createIndexScan(index_id, &ctx);
    ASSERT_NE(scan, nullptr);
    ASSERT_EQ(scan->seek(stale_key, &ctx), Status::OK) << ctx.message;

    Tuple found{};
    ASSERT_EQ(scan->next(&found, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(found.tid, TID(makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(second_page_id)),
                             second_item_id));
    EXPECT_EQ(scan->next(&found, &ctx), Status::NOT_FOUND);
}

TEST_F(StorageEngineTest, StableRootIndexUpdateReplacesVisibleKeySamePage)
{
    ErrorContext ctx;
    ID table_id = createSingleIntTable("stable_root_same_page");
    ID index_id = createSingleIntIndex(table_id, "idx_stable_root_same_page", false);

    auto original_tuple = buildIntTuple(10);
    uint32_t page_id = 0;
    uint16_t item_id = 0;
    ASSERT_EQ(engine_->insertTuple(table_id,
                                   original_tuple.data(),
                                   static_cast<uint32_t>(original_tuple.size()),
                                   &page_id,
                                   &item_id,
                                   &ctx),
              Status::OK) << ctx.message;

    TID stable_tid(makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(page_id)), item_id);
    expectIndexSeekFindsTid(index_id, 10, stable_tid);

    auto updated_tuple = buildIntTuple(20, conn_ctx_->getCurrentXid());
    uint32_t updated_page_id = 0;
    uint16_t updated_item_id = 0;
    ASSERT_EQ(engine_->updateTuple(table_id,
                                   page_id,
                                   item_id,
                                   updated_tuple.data(),
                                   static_cast<uint32_t>(updated_tuple.size()),
                                   &updated_page_id,
                                   &updated_item_id,
                                   &ctx),
              Status::OK) << ctx.message;
    EXPECT_EQ(updated_page_id, page_id);
    EXPECT_EQ(updated_item_id, item_id);

    auto raw_old = rawBTreeSearch(index_id, 10, engine_->getCurrentXid());
    auto raw_new = rawBTreeSearch(index_id, 20, engine_->getCurrentXid());
    EXPECT_TRUE(raw_old.empty());
    ASSERT_EQ(raw_new.size(), 1u);
    EXPECT_EQ(raw_new.front(), stable_tid);

    expectIndexSeekNotFound(index_id, 10);
    expectIndexSeekFindsTid(index_id, 20, stable_tid);
}

TEST_F(StorageEngineTest, StableRootIndexUpdateReplacesVisibleKeyAcrossPage)
{
    ErrorContext ctx;
    ID table_id = createSingleIntTable("stable_root_cross_page");
    ID index_id = createSingleIntIndex(table_id, "idx_stable_root_cross_page", false);

    auto original_tuple = buildLargeIntTuple(10, 'A');
    uint32_t page_id = 0;
    uint16_t item_id = 0;
    ASSERT_EQ(engine_->insertTuple(table_id,
                                   original_tuple.data(),
                                   static_cast<uint32_t>(original_tuple.size()),
                                   &page_id,
                                   &item_id,
                                   &ctx),
              Status::OK) << ctx.message;

    TID stable_tid(makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(page_id)), item_id);
    expectIndexSeekFindsTid(index_id, 10, stable_tid);

    fillPageAlmostFull(page_id);
    auto updated_tuple = buildCrossPageTriggerLargeRowTuple(20, page_id, item_id, 0x44);

    uint32_t updated_page_id = 0;
    uint16_t updated_item_id = 0;
    ASSERT_EQ(engine_->updateTuple(table_id,
                                   page_id,
                                   item_id,
                                   updated_tuple.data(),
                                   static_cast<uint32_t>(updated_tuple.size()),
                                   &updated_page_id,
                                   &updated_item_id,
                                   &ctx),
              Status::OK) << ctx.message;
    EXPECT_EQ(updated_page_id, page_id);
    EXPECT_EQ(updated_item_id, item_id);

    auto raw_old = rawBTreeSearch(index_id, 10, engine_->getCurrentXid());
    auto raw_new = rawBTreeSearch(index_id, 20, engine_->getCurrentXid());
    EXPECT_TRUE(raw_old.empty());
    ASSERT_EQ(raw_new.size(), 1u);
    EXPECT_EQ(raw_new.front(), stable_tid);

    expectIndexSeekNotFound(index_id, 10);
    expectIndexSeekFindsTid(index_id, 20, stable_tid);
}

TEST_F(StorageEngineTest, SavepointRollbackRestoresIndexedKeyVisibility)
{
    ErrorContext ctx;
    ID table_id = createSingleIntTable("savepoint_index_restore");
    ID index_id = createSingleIntIndex(table_id, "idx_savepoint_index_restore", false);

    auto original_tuple = buildIntTuple(30);
    uint32_t page_id = 0;
    uint16_t item_id = 0;
    ASSERT_EQ(engine_->insertTuple(table_id,
                                   original_tuple.data(),
                                   static_cast<uint32_t>(original_tuple.size()),
                                   &page_id,
                                   &item_id,
                                   &ctx),
              Status::OK) << ctx.message;

    TID stable_tid(makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(page_id)), item_id);
    ASSERT_EQ(conn_ctx_->createSavepoint("sp_index_restore", &ctx), Status::OK) << ctx.message;

    auto updated_tuple = buildIntTuple(40, conn_ctx_->getCurrentXid());
    ASSERT_EQ(engine_->updateTuple(table_id,
                                   page_id,
                                   item_id,
                                   updated_tuple.data(),
                                   static_cast<uint32_t>(updated_tuple.size()),
                                   nullptr,
                                   nullptr,
                                   &ctx),
              Status::OK) << ctx.message;
    expectIndexSeekFindsTid(index_id, 40, stable_tid);

    ASSERT_EQ(conn_ctx_->rollbackToSavepoint("sp_index_restore", &ctx), Status::OK) << ctx.message;

    db_->garbage_collector()->markPageDirty(page_id);
    db_->garbage_collector()->processPageCooperative(page_id, &ctx);

    expectIndexSeekFindsTid(index_id, 30, stable_tid);
    expectIndexSeekNotFound(index_id, 40);
}

TEST_F(StorageEngineTest, ReadConsistencyNoWaitUpdateRequiresRestart)
{
    ErrorContext ctx;
    ID table_id = createSingleIntTable("rc_restart_table");

    auto tuple = buildIntTuple(41, conn_ctx_->getCurrentXid());
    uint32_t page_id = 0;
    uint16_t item_id = 0;
    ASSERT_EQ(engine_->insertTuple(table_id, tuple.data(), static_cast<uint32_t>(tuple.size()),
                                   &page_id, &item_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(conn_ctx_->commit(&ctx), Status::OK) << ctx.message;

    LockTag tag{};
    tag.target_type = LockTarget::LOCK_TARGET_TUPLE;
    tag.object_uuid = table_id;
    tag.page_num = page_id;
    tag.offset_num = item_id;
    tag.padding = 0;
    uint32_t blocker_proc_id = 0;
    ASSERT_EQ(ProcArrayManager::registerBackend(&blocker_proc_id, &ctx), Status::OK)
        << ctx.message;
    uint64_t blocker_xid = 0;
    ASSERT_EQ(db_->transaction_manager()->beginTransaction(blocker_proc_id, blocker_xid, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(db_->lock_manager()->acquireLock(blocker_proc_id, tag,
                                               LockMode::LOCK_ROW_EXCLUSIVE, true, 0, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_TRUE(db_->lock_manager()->checkConflict(tag, LockMode::LOCK_ROW_EXCLUSIVE));

    std::unique_ptr<ConnectionContext> conn2;
    ASSERT_EQ(db_->connect(conn2, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(conn2->initialize(&ctx), Status::OK) << ctx.message;
    ID system_user = db_->catalog_manager()->getSystemUserId(&ctx);
    conn2->setCurrentUser(system_user, true);
    ASSERT_EQ(conn2->startTransaction(false, IsolationLevel::READ_COMMITTED_READ_CONSISTENCY,
                                      true, &ctx),
              Status::OK)
        << ctx.message;
    conn2->setWaitForLocks(false);
    ASSERT_NE(conn2->getProcId(), blocker_proc_id);
    ASSERT_EQ(conn2->beginStatementTracking(
                  "UPDATE users.public.rc_restart_table SET v = 42 WHERE id = 41", &ctx),
              Status::OK)
        << ctx.message;

    auto updated_tuple = buildIntTuple(42, conn2->getCurrentXid());
    ConnectionContext::setCurrent(conn2.get());
    ASSERT_NE(ConnectionContext::getCurrentProcId(), static_cast<int32_t>(blocker_proc_id));
    uint32_t new_page_id = 0;
    uint16_t new_item_id = 0;
    Status status = engine_->updateTuple(table_id, page_id, item_id,
                                         updated_tuple.data(),
                                         static_cast<uint32_t>(updated_tuple.size()),
                                         &new_page_id, &new_item_id, &ctx);
    EXPECT_EQ(status, Status::SERIALIZATION_FAILURE);
    EXPECT_NE(ctx.message.find("READ_CONSISTENCY_RESTART_REQUIRED"), std::string::npos);
    EXPECT_EQ(conn2->statementRestartCount(), 1u);
    EXPECT_EQ(conn2->lastStatementRestartDecision().reason,
              StatementRestartReason::TUPLE_WRITE_CONFLICT);
    EXPECT_EQ(conn2->lastStatementRestartDecision().blocker_proc_id, blocker_proc_id);
    EXPECT_TRUE(conn2->lastStatementRestartDecision().retry_eligible);

    ConnectionContext::setCurrent(conn_ctx_.get());
    db_->lock_manager()->releaseAllLocks(blocker_proc_id, nullptr);
    db_->transaction_manager()->rollbackTransaction(blocker_proc_id, blocker_xid, nullptr);
    ProcArrayManager::unregisterBackend(blocker_proc_id, &ctx);
}

TEST_F(StorageEngineTest, ReadConsistencyWaitModeAlsoUsesRestartSemantics)
{
    ErrorContext ctx;
    ID table_id = createSingleIntTable("rc_restart_wait_table");

    auto tuple = buildIntTuple(41, conn_ctx_->getCurrentXid());
    uint32_t page_id = 0;
    uint16_t item_id = 0;
    ASSERT_EQ(engine_->insertTuple(table_id, tuple.data(), static_cast<uint32_t>(tuple.size()),
                                   &page_id, &item_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(conn_ctx_->commit(&ctx), Status::OK) << ctx.message;

    LockTag tag{};
    tag.target_type = LockTarget::LOCK_TARGET_TUPLE;
    tag.object_uuid = table_id;
    tag.page_num = page_id;
    tag.offset_num = item_id;
    tag.padding = 0;

    uint32_t blocker_proc_id = 0;
    ASSERT_EQ(ProcArrayManager::registerBackend(&blocker_proc_id, &ctx), Status::OK)
        << ctx.message;
    uint64_t blocker_xid = 0;
    ASSERT_EQ(db_->transaction_manager()->beginTransaction(blocker_proc_id, blocker_xid, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(db_->lock_manager()->acquireLock(blocker_proc_id, tag,
                                               LockMode::LOCK_ROW_EXCLUSIVE, true, 0, &ctx),
              Status::OK)
        << ctx.message;

    std::unique_ptr<ConnectionContext> conn2;
    ASSERT_EQ(db_->connect(conn2, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(conn2->initialize(&ctx), Status::OK) << ctx.message;
    ID system_user = db_->catalog_manager()->getSystemUserId(&ctx);
    conn2->setCurrentUser(system_user, true);
    ASSERT_EQ(conn2->startTransaction(false, IsolationLevel::READ_COMMITTED_READ_CONSISTENCY,
                                      true, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_TRUE(conn2->getWaitForLocks());
    ASSERT_EQ(conn2->beginStatementTracking(
                  "UPDATE users.public.rc_restart_wait_table SET v = 42 WHERE id = 41", &ctx),
              Status::OK)
        << ctx.message;

    auto updated_tuple = buildIntTuple(42, conn2->getCurrentXid());
    ConnectionContext::setCurrent(conn2.get());
    uint32_t new_page_id = 0;
    uint16_t new_item_id = 0;
    Status status = engine_->updateTuple(table_id, page_id, item_id,
                                         updated_tuple.data(),
                                         static_cast<uint32_t>(updated_tuple.size()),
                                         &new_page_id, &new_item_id, &ctx);
    EXPECT_EQ(status, Status::SERIALIZATION_FAILURE);
    EXPECT_NE(ctx.message.find("READ_CONSISTENCY_RESTART_REQUIRED"), std::string::npos);
    EXPECT_EQ(conn2->statementRestartCount(), 1u);
    EXPECT_EQ(conn2->lastStatementRestartDecision().reason,
              StatementRestartReason::TUPLE_WRITE_CONFLICT);
    EXPECT_EQ(conn2->lastStatementRestartDecision().blocker_proc_id, blocker_proc_id);

    ConnectionContext::setCurrent(conn_ctx_.get());
    db_->lock_manager()->releaseAllLocks(blocker_proc_id, nullptr);
    db_->transaction_manager()->rollbackTransaction(blocker_proc_id, blocker_xid, nullptr);
    ProcArrayManager::unregisterBackend(blocker_proc_id, &ctx);
}

TEST_F(StorageEngineTest, PageFullAllocatesNewPage)
{
    ID table_id = createTestTable("page_full_test");

    struct LargeData
    {
        int32_t id;
        char payload[200];
    };

    bool saw_new_page = false;
    uint32_t first_page_id = 0;

    ErrorContext ctx;
    for (int i = 0; i < 500; i++)
    {
        LargeData data{};
        data.id = i;
        memset(data.payload, 'X', sizeof(data.payload));

        auto tuple = buildTuple(&data, sizeof(LargeData));
        uint32_t page_id = 0;
        uint16_t item_id = 0;
        ASSERT_EQ(engine_->insertTuple(table_id, tuple.data(),
                                       static_cast<uint32_t>(tuple.size()),
                                       &page_id, &item_id, &ctx),
                  Status::OK) << ctx.message;

        if (i == 0)
        {
            first_page_id = page_id;
        }
        else if (page_id != first_page_id)
        {
            saw_new_page = true;
            break;
        }
    }

    EXPECT_TRUE(saw_new_page)
        << "Expected inserts to span multiple pages";
}

TEST_F(StorageEngineTest, SavepointRollbackRestoresToastedUpdateImage)
{
    ID table_id = createTestTable("savepoint_toast_restore_test");

    auto original_tuple = buildLargeRowTuple(1, 'A');
    auto updated_tuple = buildLargeRowTuple(1, 'B');

    ErrorContext ctx;
    uint32_t page_id = 0;
    uint16_t item_id = 0;
    ASSERT_EQ(engine_->insertTuple(table_id,
                                   original_tuple.data(),
                                   static_cast<uint32_t>(original_tuple.size()),
                                   &page_id,
                                   &item_id,
                                   &ctx),
              Status::OK) << ctx.message;

    ASSERT_EQ(conn_ctx_->createSavepoint("sp_toast_restore", &ctx), Status::OK) << ctx.message;

    uint32_t new_page_id = 0;
    uint16_t new_item_id = 0;
    ASSERT_EQ(engine_->updateTuple(table_id,
                                   page_id,
                                   item_id,
                                   updated_tuple.data(),
                                   static_cast<uint32_t>(updated_tuple.size()),
                                   &new_page_id,
                                   &new_item_id,
                                   &ctx),
              Status::OK) << ctx.message;

    ASSERT_EQ(conn_ctx_->rollbackToSavepoint("sp_toast_restore", &ctx), Status::OK) << ctx.message;

    std::vector<uint8_t> restored = readTupleDetoasted(table_id, page_id, item_id);
    expectTuplePayloadEquals(restored, original_tuple);
}

TEST_F(StorageEngineTest, CooperativeGcPreservesRowAfterSavepointRollback)
{
    ID table_id = createTestTable("savepoint_gc_restore_test");

    auto original_tuple = buildLargeRowTuple(7, 'Q');
    auto updated_tuple = buildLargeRowTuple(7, 'R');

    ErrorContext ctx;
    uint32_t page_id = 0;
    uint16_t item_id = 0;
    ASSERT_EQ(engine_->insertTuple(table_id,
                                   original_tuple.data(),
                                   static_cast<uint32_t>(original_tuple.size()),
                                   &page_id,
                                   &item_id,
                                   &ctx),
              Status::OK) << ctx.message;

    ASSERT_EQ(conn_ctx_->createSavepoint("sp_gc_restore", &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(engine_->updateTuple(table_id,
                                   page_id,
                                   item_id,
                                   updated_tuple.data(),
                                   static_cast<uint32_t>(updated_tuple.size()),
                                   nullptr,
                                   nullptr,
                                   &ctx),
              Status::OK) << ctx.message;
    ASSERT_EQ(conn_ctx_->rollbackToSavepoint("sp_gc_restore", &ctx), Status::OK) << ctx.message;

    db_->garbage_collector()->markPageDirty(page_id);
    db_->garbage_collector()->processPageCooperative(page_id, &ctx);

    std::vector<uint8_t> restored = readTupleDetoasted(table_id, page_id, item_id);
    expectTuplePayloadEquals(restored, original_tuple);
}

TEST_F(StorageEngineTest, CrossPageSavepointRollbackRestoresOriginalTuple)
{
    ID table_id = createSingleIntTable("cross_page_savepoint_restore_test");

    auto original_tuple = buildFilledTuple(120, 0x11);

    ErrorContext ctx;
    uint32_t page_id = 0;
    uint16_t item_id = 0;
    ASSERT_EQ(engine_->insertTuple(table_id,
                                   original_tuple.data(),
                                   static_cast<uint32_t>(original_tuple.size()),
                                   &page_id,
                                   &item_id,
                                   &ctx),
              Status::OK) << ctx.message;

    fillPageAlmostFull(page_id);
    auto updated_tuple = buildCrossPageTriggerTuple(page_id, item_id, 0x33);

    ASSERT_EQ(conn_ctx_->createSavepoint("sp_cross_page_restore", &ctx), Status::OK)
        << ctx.message;

    uint32_t updated_page_id = 0;
    uint16_t updated_item_id = 0;
    ASSERT_EQ(engine_->updateTuple(table_id,
                                   page_id,
                                   item_id,
                                   updated_tuple.data(),
                                   static_cast<uint32_t>(updated_tuple.size()),
                                   &updated_page_id,
                                   &updated_item_id,
                                   &ctx),
              Status::OK) << ctx.message;
    ASSERT_EQ(updated_page_id, page_id);
    ASSERT_EQ(updated_item_id, item_id);

    ASSERT_EQ(conn_ctx_->rollbackToSavepoint("sp_cross_page_restore", &ctx), Status::OK)
        << ctx.message;

    Tuple restored{};
    ASSERT_EQ(engine_->getTuple(page_id, item_id, &restored, &ctx), Status::OK)
        << ctx.message;
    std::vector<uint8_t> restored_tuple(restored.data, restored.data + restored.data_size);
    expectTuplePayloadEquals(restored_tuple, original_tuple);
}
