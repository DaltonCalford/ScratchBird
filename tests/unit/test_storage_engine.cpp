#include <gtest/gtest.h>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/config.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/btree.h"
#include "scratchbird/core/btree_page.h"
#include "scratchbird/core/garbage_collector.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/index_key_encoding.h"
#include "scratchbird/core/lock_manager.h"
#include "scratchbird/core/mga_backout_engine.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/toast.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/types.h"
#include "test_helpers.h"

using namespace scratchbird::core;

namespace
{
class ScopedEnvVar
{
public:
    ScopedEnvVar(const char *name, const std::string &value)
        : name_(name)
    {
        if (const char *existing = std::getenv(name_))
        {
            had_original_ = true;
            original_value_ = existing;
        }
        setenv(name_, value.c_str(), 1);
    }

    ~ScopedEnvVar()
    {
        if (had_original_)
        {
            setenv(name_, original_value_.c_str(), 1);
        }
        else
        {
            unsetenv(name_);
        }
    }

private:
    const char *name_;
    bool had_original_ = false;
    std::string original_value_;
};
} // namespace

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
        for (const auto &tablespace_path : tablespace_paths_)
        {
            std::filesystem::remove(tablespace_path);
        }
        std::filesystem::remove(test_db_path_);
    }

    void reopenDatabase()
    {
        ErrorContext ctx;
        ConnectionContext::setCurrent(nullptr);
        conn_ctx_.reset();
        if (db_)
        {
            db_->close();
        }
        db_.reset();

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(test_db_path_, &ctx), Status::OK) << ctx.message;

        Status status = db_->initializeProcArray(16, &ctx);
        if (status != Status::OK && status != Status::INVALID_ARGUMENT)
        {
            ASSERT_EQ(status, Status::OK) << ctx.message;
        }

        ASSERT_EQ(db_->connect(conn_ctx_, &ctx), Status::OK) << ctx.message;
        ConnectionContext::setCurrent(conn_ctx_.get());
        ASSERT_EQ(conn_ctx_->initialize(&ctx), Status::OK) << ctx.message;

        ID system_user = db_->catalog_manager()->getSystemUserId(&ctx);
        conn_ctx_->setCurrentUser(system_user, true);

        schema_id_ = resolveDefaultSchema(&ctx);
        ASSERT_NE(schema_id_, ID{});

        engine_ = db_->storage_engine();
        ASSERT_NE(engine_, nullptr);
    }

    ID resolveDefaultSchema(ErrorContext *ctx)
    {
        CatalogManager::SchemaInfo schema{};
        Status status = db_->catalog_manager()->getSchema("main", schema, ctx);
        if (status == Status::OK)
        {
            return schema.schema_id;
        }

        status = db_->catalog_manager()->getSchema("users.public", schema, ctx);
        if (status == Status::OK)
        {
            return schema.schema_id;
        }

        status = db_->catalog_manager()->getSchema("public", schema, ctx);
        if (status == Status::OK)
        {
            return schema.schema_id;
        }

        std::vector<CatalogManager::SchemaInfo> schemas;
        status = db_->catalog_manager()->listSchemas(schemas, ctx);
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

    uint16_t createTestTablespace(const std::string &name)
    {
        ErrorContext ctx;
        const std::string tablespace_path =
            scratchbird::testing::uniqueTestShortPath(name, ".sbts");
        std::filesystem::remove(tablespace_path);

        uint16_t tablespace_id = 0;
        Status status = db_->catalog_manager()->createTablespace(name,
                                                                 tablespace_path,
                                                                 true,
                                                                 1,
                                                                 0,
                                                                 2,
                                                                 tablespace_id,
                                                                 &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        if (status == Status::OK)
        {
            tablespace_paths_.push_back(tablespace_path);
            EXPECT_GE(tablespace_id, 2);
        }
        return tablespace_id;
    }

    ID createTestTable(const std::string &name, uint16_t tablespace_id = 0)
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
        Status status = db_->catalog_manager()->createTable(schema_id_,
                                                            name,
                                                            columns,
                                                            table_id,
                                                            tablespace_id,
                                                            &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        return table_id;
    }

    ID createSingleIntTable(const std::string &name, uint16_t tablespace_id = 0)
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
        Status status = db_->catalog_manager()->createTable(schema_id_,
                                                            name,
                                                            columns,
                                                            table_id,
                                                            tablespace_id,
                                                            &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        return table_id;
    }

    ID createTripleIntTable(const std::string &name,
                            const std::string &col1,
                            const std::string &col2,
                            const std::string &col3,
                            uint16_t tablespace_id = 0)
    {
        ErrorContext ctx;
        std::vector<CatalogManager::ColumnInfo> columns;

        CatalogManager::ColumnInfo first_col;
        first_col.column_name = col1;
        first_col.ordinal = 1;
        first_col.data_type = static_cast<uint16_t>(DataType::INT32);
        first_col.type_precision = 4;
        first_col.nullable = false;
        columns.push_back(first_col);

        CatalogManager::ColumnInfo second_col;
        second_col.column_name = col2;
        second_col.ordinal = 2;
        second_col.data_type = static_cast<uint16_t>(DataType::INT32);
        second_col.type_precision = 4;
        second_col.nullable = false;
        columns.push_back(second_col);

        CatalogManager::ColumnInfo third_col;
        third_col.column_name = col3;
        third_col.ordinal = 3;
        third_col.data_type = static_cast<uint16_t>(DataType::INT32);
        third_col.type_precision = 4;
        third_col.nullable = false;
        columns.push_back(third_col);

        ID table_id;
        Status status = db_->catalog_manager()->createTable(schema_id_,
                                                            name,
                                                            columns,
                                                            table_id,
                                                            tablespace_id,
                                                            &ctx);
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

    CatalogManager::IndexInfo getIndexInfoById(const ID& index_id)
    {
        ErrorContext ctx;
        CatalogManager::IndexInfo index_info{};
        EXPECT_EQ(db_->catalog_manager()->getIndex(index_id, index_info, &ctx), Status::OK)
            << ctx.message;
        return index_info;
    }

    std::optional<IndexCleanupPublicationRecord> findCleanupPublication(
        const std::string& index_name)
    {
        ErrorContext ctx;
        std::vector<IndexCleanupPublicationRecord> publications;
        EXPECT_EQ(engine_->listIndexCleanupPublications(publications), Status::OK)
            << ctx.message;
        for (const auto& publication : publications)
        {
            if (publication.index_name == index_name)
            {
                return publication;
            }
        }
        return std::nullopt;
    }

    CatalogManager::IndexHealthCatalogInfo loadIndexHealth(const ID& index_id)
    {
        ErrorContext ctx;
        CatalogManager::IndexHealthCatalogInfo info{};
        EXPECT_EQ(db_->catalog_manager()->getIndexHealthCatalogEntry(index_id, info, &ctx),
                  Status::OK)
            << ctx.message;
        return info;
    }

    ID createOnlineMaintenanceForIndex(const ID& index_id)
    {
        ErrorContext ctx;
        CatalogManager::IndexMaintenanceCatalogInfo maintenance{};
        maintenance.index_id = index_id;
        maintenance.maintenance_kind = CatalogManager::IndexMaintenanceKind::REBUILD;
        maintenance.maintenance_mode = CatalogManager::IndexMaintenanceMode::ONLINE;
        maintenance.maintenance_state = CatalogManager::IndexMaintenanceState::BUILDING_SHADOW;
        maintenance.started_txid = conn_ctx_ ? conn_ctx_->getCurrentXid() : 0;
        ID maintenance_id{};
        EXPECT_EQ(db_->catalog_manager()->upsertIndexMaintenanceCatalogEntry(
                      maintenance,
                      maintenance_id,
                      &ctx),
                  Status::OK)
            << ctx.message;
        return maintenance_id;
    }

    std::vector<uint8_t> buildIntTuple(int32_t value,
                                       uint64_t xmin = config::DEFAULT_INITIAL_XID)
    {
        return buildTuple(&value, sizeof(value), xmin);
    }

    std::vector<uint8_t> buildTripleIntTuple(int32_t first,
                                             int32_t second,
                                             int32_t third,
                                             uint64_t xmin = config::DEFAULT_INITIAL_XID)
    {
        struct TripleIntRow
        {
            int32_t first;
            int32_t second;
            int32_t third;
        };

        TripleIntRow row{first, second, third};
        return buildTuple(&row, sizeof(row), xmin);
    }

    std::vector<uint8_t> encodeIntKey(int32_t value)
    {
        std::vector<uint8_t> key(sizeof(value), 0);
        std::memcpy(key.data(), &value, sizeof(value));
        return key;
    }

    std::vector<uint8_t> encodeIndexedInt32Key(int32_t value)
    {
        ErrorContext ctx;
        std::vector<uint8_t> encoded;
        const Status status = index_key_encoding::encodePlainValue(DataType::INT32,
                                                                   encodeIntKey(value),
                                                                   &encoded,
                                                                   &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        if (status != Status::OK)
        {
            return {};
        }
        return encoded;
    }

    int compareEncodedKeys(const std::vector<uint8_t> &lhs, const std::vector<uint8_t> &rhs)
    {
        const size_t min_size = std::min(lhs.size(), rhs.size());
        const int prefix_cmp =
            (min_size == 0) ? 0 : std::memcmp(lhs.data(), rhs.data(), min_size);
        if (prefix_cmp != 0)
        {
            return prefix_cmp < 0 ? -1 : 1;
        }
        if (lhs.size() == rhs.size())
        {
            return 0;
        }
        return lhs.size() < rhs.size() ? -1 : 1;
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

    std::vector<uint8_t> buildIndexedTextTuple(int32_t value,
                                               char fill,
                                               size_t text_size)
    {
        TupleHeader header{};
        header.xmin = config::DEFAULT_INITIAL_XID;
        header.xmax = 0;
        header.back_version_gpid = INVALID_GPID;
        header.back_version_slot = 0;
        header.ctid_gpid = INVALID_GPID;
        header.ctid_slot = 0;
        header.infomask = TupleHeader::HEAP_HAS_NULLS;
        header.null_bitmap_offset = static_cast<uint16_t>(sizeof(TupleHeader));
        header.padding = 0;
        header.session_id = ID{};

        const uint32_t encoded_size = static_cast<uint32_t>(text_size);
        std::vector<uint8_t> tuple(sizeof(TupleHeader) + 1 + sizeof(value) +
                                       sizeof(encoded_size) + text_size,
                                   0);
        std::memcpy(tuple.data(), &header, sizeof(TupleHeader));
        tuple[sizeof(TupleHeader)] = 0;
        size_t payload_offset = sizeof(TupleHeader) + 1;
        std::memcpy(tuple.data() + payload_offset, &value, sizeof(value));
        payload_offset += sizeof(value);
        std::memcpy(tuple.data() + payload_offset, &encoded_size, sizeof(encoded_size));
        payload_offset += sizeof(encoded_size);
        std::memset(tuple.data() + payload_offset, fill, text_size);
        return tuple;
    }

    std::vector<uint8_t> buildIndexedNullTextTuple(int32_t value,
                                                   uint64_t xmin = config::DEFAULT_INITIAL_XID)
    {
        TupleHeader header{};
        header.xmin = xmin;
        header.xmax = 0;
        header.back_version_gpid = INVALID_GPID;
        header.back_version_slot = 0;
        header.ctid_gpid = INVALID_GPID;
        header.ctid_slot = 0;
        header.infomask = TupleHeader::HEAP_HAS_NULLS;
        header.null_bitmap_offset = static_cast<uint16_t>(sizeof(TupleHeader));
        header.padding = 0;
        header.session_id = ID{};

        std::vector<uint8_t> tuple(sizeof(TupleHeader) + 1 + sizeof(value), 0);
        std::memcpy(tuple.data(), &header, sizeof(TupleHeader));
        tuple[sizeof(TupleHeader)] = 0x02;
        std::memcpy(tuple.data() + sizeof(TupleHeader) + 1, &value, sizeof(value));
        return tuple;
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

    std::vector<uint8_t> readVisibleTupleCopy(const ID &table_id,
                                              uint32_t stable_page_id,
                                              uint16_t stable_item_id)
    {
        ErrorContext ctx;
        void *page_buffer = nullptr;
        const GPID stable_gpid = makeGPID(PRIMARY_TABLESPACE_ID, stable_page_id);
        EXPECT_EQ(db_->buffer_pool()->pinPageGlobal(stable_gpid, &page_buffer, &ctx), Status::OK)
            << ctx.message;

        ToastManager toast_mgr(db_.get(), table_id);
        EXPECT_EQ(toast_mgr.initialize(&ctx), Status::OK) << ctx.message;

        HeapPage heap_page(static_cast<uint8_t *>(page_buffer), kPageSize, &toast_mgr, db_.get(),
                           table_id);
        const uint8_t *tuple_data = nullptr;
        uint32_t tuple_size = 0;
        TID visible_tid{};
        Status status = heap_page.findVisibleVersion(stable_item_id,
                                                     ConnectionContext::getCurrentTransactionId(),
                                                     &tuple_data,
                                                     &tuple_size,
                                                     &visible_tid,
                                                     &ctx);
        std::vector<uint8_t> tuple_copy;
        if (status == Status::OK && tuple_data != nullptr)
        {
            tuple_copy.assign(tuple_data, tuple_data + tuple_size);
        }

        db_->buffer_pool()->unpinPageGlobal(stable_gpid, false, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        return tuple_copy;
    }

    ID readRawToastValueId(const ID &table_id, uint32_t page_id, uint16_t item_id)
    {
        ErrorContext ctx;
        void *page_buffer = nullptr;
        EXPECT_EQ(db_->buffer_pool()->pinPage(page_id, &page_buffer, &ctx), Status::OK)
            << ctx.message;

        ToastManager toast_mgr(db_.get(), table_id);
        EXPECT_EQ(toast_mgr.initialize(&ctx), Status::OK) << ctx.message;

        HeapPage heap_page(static_cast<uint8_t *>(page_buffer), kPageSize, &toast_mgr, db_.get(),
                           table_id);
        const uint8_t *tuple_data = nullptr;
        uint32_t tuple_size = 0;
        EXPECT_EQ(heap_page.getTuple(item_id, &tuple_data, &tuple_size, &ctx), Status::OK)
            << ctx.message;

        ID value_id{};
        if (tuple_data != nullptr && tuple_size >= sizeof(TupleHeader) + sizeof(ToastPointer))
        {
            const auto *toast_ptr =
                reinterpret_cast<const ToastPointer *>(tuple_data + sizeof(TupleHeader));
            if (ToastManager::isToastPointer(reinterpret_cast<const uint8_t *>(toast_ptr),
                                            sizeof(ToastPointer)))
            {
                value_id = toast_ptr->lob_uuid;
            }
        }

        db_->buffer_pool()->unpinPage(page_id, false, &ctx);
        return value_id;
    }

    Status detectOrphanedToastValues(const ID &table_id,
                                     std::unordered_set<ID, IDHash> *orphaned_value_ids)
    {
        ErrorContext ctx;
        ToastManager toast_mgr(db_.get(), table_id);
        Status status = toast_mgr.initialize(&ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        if (status != Status::OK)
        {
            return status;
        }

        return db_->garbage_collector()->detectOrphanedToastChunks(toast_mgr.toastTableId(),
                                                                   orphaned_value_ids,
                                                                   &ctx);
    }

    Status readToastChunkXmaxForValue(const ID &table_id,
                                      const ID &value_id,
                                      uint64_t *xmax_out)
    {
        if (xmax_out == nullptr)
        {
            return Status::INVALID_ARGUMENT;
        }

        ErrorContext ctx;
        ToastManager toast_mgr(db_.get(), table_id);
        Status status = toast_mgr.initialize(&ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        if (status != Status::OK)
        {
            return status;
        }

        auto scan = db_->storage_engine()->createScanAll(toast_mgr.toastTableId(), &ctx);
        EXPECT_NE(scan, nullptr) << ctx.message;
        if (!scan)
        {
            return Status::INVALID_ARGUMENT;
        }

        Tuple tuple{};
        while ((status = scan->next(&tuple, &ctx)) == Status::OK)
        {
            if (tuple.data == nullptr || tuple.data_size < sizeof(TupleHeader) + sizeof(ID))
            {
                continue;
            }

            ID chunk_value_id{};
            std::memcpy(chunk_value_id.bytes.data(),
                        tuple.data + sizeof(TupleHeader),
                        chunk_value_id.bytes.size());
            if (chunk_value_id != value_id)
            {
                continue;
            }

            EXPECT_GE(tuple.data_size, sizeof(TupleHeader));
            if (tuple.data_size < sizeof(TupleHeader))
            {
                return Status::PAGE_CORRUPT;
            }
            const auto *hdr = reinterpret_cast<const TupleHeader *>(tuple.data);
            *xmax_out = hdr->xmax;
            return Status::OK;
        }

        return status == Status::NOT_FOUND ? Status::NOT_FOUND : status;
    }

    void fillPageAlmostFull(uint32_t page_id,
                            size_t payload_size = 64,
                            uint32_t target_free_space = 260)
    {
        ErrorContext ctx;
        void *page_buffer = nullptr;
        ASSERT_EQ(db_->buffer_pool()->pinPage(page_id, &page_buffer, &ctx), Status::OK)
            << ctx.message;

        auto *page_data = static_cast<uint8_t *>(page_buffer);
        HeapPage heap_page(page_data, kPageSize);
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

        auto key = encodeIndexedInt32Key(value);
        ASSERT_EQ(scan->seek(key, &ctx), Status::OK) << ctx.message;

        Tuple found{};
        const Status next_status = scan->next(&found, &ctx);
        if (next_status != Status::OK)
        {
            CatalogManager::IndexInfo index_info;
            ErrorContext raw_ctx;
            std::vector<TID> raw_tids;
            std::vector<TID> raw_all_tids;
            std::vector<TID> linear_leaf_tids;
            LeafKeyLocation leaf_location;
            std::vector<BTreePathStep> search_path;
            bool leaf_reachable_from_root = false;
            bool root_directly_references_leaf = false;
            Tuple raw_heap_tuple{};
            Status raw_heap_tuple_status = Status::NOT_FOUND;
            int32_t raw_heap_value = 0;
            if (db_->catalog_manager()->getIndex(index_id, index_info, &raw_ctx) == Status::OK)
            {
                if (auto btree = BTree::open(db_.get(), index_id, index_info.root_gpid, &raw_ctx);
                    btree != nullptr)
                {
                    (void)btree->search(key, engine_->getCurrentXid(), &raw_tids, &raw_ctx);
                    (void)btree->search(key, 0, &raw_all_tids, &raw_ctx);
                }
                if (!raw_tids.empty())
                {
                    ErrorContext tuple_ctx;
                    raw_heap_tuple_status =
                        engine_->getTuple(index_info.table_id, raw_tids.front(), &raw_heap_tuple, &tuple_ctx);
                    if (raw_heap_tuple_status == Status::OK &&
                        raw_heap_tuple.data_size >= sizeof(TupleHeader) + sizeof(int32_t))
                    {
                        std::memcpy(&raw_heap_value,
                                    raw_heap_tuple.data + sizeof(TupleHeader),
                                    sizeof(int32_t));
                    }
                }
                linear_leaf_tids = linearLeafChainSearch(index_id, value);
                leaf_location = findKeyInLeafChain(index_id, value);
                search_path = traceBTreePath(index_id, value);
                const auto reachable_leaves = collectReachableLeafPagesFromRoot(index_id);
                leaf_reachable_from_root =
                    leaf_location.page_num != 0 &&
                    reachable_leaves.find(leaf_location.page_num) != reachable_leaves.end();
                root_directly_references_leaf =
                    leaf_location.page_num != 0 &&
                    pageDirectlyReferencesChild(index_id,
                                               getPageNumber(index_info.root_gpid),
                                               leaf_location.page_num);
            }

            ASSERT_EQ(next_status, Status::OK)
                << ctx.message
                << " raw_tids=" << raw_tids.size()
                << " raw_all_tids=" << raw_all_tids.size()
                << " root_page=" << getPageNumber(index_info.root_gpid)
                << " current_xid=" << engine_->getCurrentXid()
                << " expected_tid_page=" << getPageNumber(expected_tid.gpid)
                << " expected_tid_slot=" << expected_tid.slot
                << " raw_first_tid_page=" << (raw_tids.empty() ? 0 : getPageNumber(raw_tids.front().gpid))
                << " raw_first_tid_slot=" << (raw_tids.empty() ? 0 : raw_tids.front().slot)
                << " raw_heap_tuple_status=" << static_cast<int>(raw_heap_tuple_status)
                << " raw_heap_tuple_tid_page="
                << ((raw_heap_tuple_status == Status::OK) ? getPageNumber(raw_heap_tuple.tid.gpid) : 0)
                << " raw_heap_tuple_tid_slot="
                << ((raw_heap_tuple_status == Status::OK) ? raw_heap_tuple.tid.slot : 0)
                << " raw_heap_value="
                << ((raw_heap_tuple_status == Status::OK) ? raw_heap_value : 0)
                << " leaf_found=" << leaf_location.found
                << " leaf_page=" << leaf_location.page_num
                << " leaf_parent_page=" << leaf_location.parent_page
                << " leaf_slot=" << leaf_location.slot_index
                << " leaf_first_key=" << leaf_location.first_key
                << " leaf_last_key=" << leaf_location.last_key
                << " leaf_key_count=" << leaf_location.key_count
                << " leaf_count=" << leaf_location.leaf_count
                << " leaf_raw_flags=" << leaf_location.raw_flags
                << " leaf_raw_prefix_len=" << leaf_location.raw_prefix_len
                << " leaf_raw_key_len=" << leaf_location.raw_key_len
                << " leaf_raw_tuple_count=" << leaf_location.raw_tuple_count
                << " leaf_raw_xmin=" << leaf_location.raw_xmin
                << " leaf_raw_xmax=" << leaf_location.raw_xmax
                << " leaf_raw_first_tid_page="
                << (leaf_location.raw_first_tid.isValid()
                        ? getPageNumber(leaf_location.raw_first_tid.gpid)
                        : 0)
                << " leaf_raw_first_tid_slot="
                << (leaf_location.raw_first_tid.isValid()
                        ? leaf_location.raw_first_tid.slot
                        : 0)
                << " leaf_match_tids=" << leaf_location.matching_tids.size()
                << " leaf_match_first_tid_page="
                << (leaf_location.matching_tids.empty()
                        ? 0
                        : getPageNumber(leaf_location.matching_tids.front().gpid))
                << " leaf_match_first_tid_slot="
                << (leaf_location.matching_tids.empty()
                        ? 0
                        : leaf_location.matching_tids.front().slot)
                << " linear_leaf_tids=" << linear_leaf_tids.size()
                << " linear_leaf_first_tid_page="
                << (linear_leaf_tids.empty() ? 0 : getPageNumber(linear_leaf_tids.front().gpid))
                << " linear_leaf_first_tid_slot="
                << (linear_leaf_tids.empty() ? 0 : linear_leaf_tids.front().slot)
                << " leaf_cycle_detected=" << leaf_location.cycle_detected
                << " leaf_reachable_from_root=" << leaf_reachable_from_root
                << " root_directly_references_leaf=" << root_directly_references_leaf
                << " path_steps=" << search_path.size()
                << " path_root_page=" << (search_path.empty() ? 0 : search_path.front().page_num)
                << " path_root_first_key=" << (search_path.empty() ? 0 : search_path.front().first_key)
                << " path_root_last_key=" << (search_path.empty() ? 0 : search_path.front().last_key)
                << " path_leaf_page=" << (search_path.empty() ? 0 : search_path.back().page_num)
                << " path_leaf_first_key=" << (search_path.empty() ? 0 : search_path.back().first_key)
                << " path_leaf_last_key=" << (search_path.empty() ? 0 : search_path.back().last_key);
        }
        EXPECT_EQ(found.tid, expected_tid);
        EXPECT_EQ(scan->next(&found, &ctx), Status::NOT_FOUND);
    }

    void expectIndexSeekNotFound(const ID &index_id, int32_t value)
    {
        ErrorContext ctx;
        auto scan = engine_->createIndexScan(index_id, &ctx);
        ASSERT_NE(scan, nullptr) << ctx.message;

        auto key = encodeIndexedInt32Key(value);
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
        auto key = encodeIndexedInt32Key(value);
        Status status = btree->search(key, current_xid, &tids, &ctx);
        if (status == Status::NOT_FOUND)
        {
            return {};
        }
        EXPECT_EQ(status, Status::OK) << ctx.message;
        return tids;
    }

    std::vector<TID> linearLeafChainSearch(const ID &index_id, int32_t value)
    {
        ErrorContext ctx;
        CatalogManager::IndexInfo index_info{};
        EXPECT_EQ(db_->catalog_manager()->getIndex(index_id, index_info, &ctx), Status::OK)
            << ctx.message;

        const uint16_t tablespace_id =
            index_info.tablespace_id != 0 ? index_info.tablespace_id
                                          : getTablespaceID(index_info.root_gpid);
        std::unordered_set<uint64_t> visited_leaf_pages;
        std::vector<TID> matches;
        const auto search_key = encodeIndexedInt32Key(value);
        uint64_t current_page = getPageNumber(index_info.root_gpid);

        while (current_page != 0)
        {
            void *page_data = nullptr;
            Status status = db_->buffer_pool()->pinPageGlobal(makeGPID(tablespace_id,
                                                                       current_page),
                                                              &page_data,
                                                              &ctx);
            EXPECT_EQ(status, Status::OK) << ctx.message;
            if (status != Status::OK)
            {
                return matches;
            }

            auto *page = reinterpret_cast<SBBTreePage *>(page_data);
            const bool is_leaf =
                page->btr_header.page_type ==
                    static_cast<uint16_t>(PageType::PAGE_TYPE_BTREE_LEAF) ||
                ((page->btr_flags & static_cast<uint16_t>(BTreeFlags::LEAF)) != 0 &&
                 page->btr_level == 0);

            if (is_leaf)
            {
                db_->buffer_pool()->unpinPageGlobal(makeGPID(tablespace_id, current_page),
                                                    false,
                                                    &ctx);
                break;
            }

            auto *offsets = reinterpret_cast<const uint16_t *>(
                reinterpret_cast<const uint8_t *>(page_data) + sizeof(SBBTreePage));
            const uint64_t next_page = page->btr_count > 0
                                           ? reinterpret_cast<const SBBTreeNode *>(
                                                 reinterpret_cast<const uint8_t *>(page_data) +
                                                 offsets[0])
                                                 ->btn_child_page
                                           : page->btr_rightmost_child;
            db_->buffer_pool()->unpinPageGlobal(makeGPID(tablespace_id, current_page),
                                                false,
                                                &ctx);
            current_page = next_page;
        }

        while (current_page != 0)
        {
            if (!visited_leaf_pages.insert(current_page).second)
            {
                break;
            }

            void *page_data = nullptr;
            Status status = db_->buffer_pool()->pinPageGlobal(makeGPID(tablespace_id,
                                                                       current_page),
                                                              &page_data,
                                                              &ctx);
            EXPECT_EQ(status, Status::OK) << ctx.message;
            if (status != Status::OK)
            {
                return matches;
            }

            auto *page = reinterpret_cast<SBBTreePage *>(page_data);
            for (uint16_t i = 0; i < page->btr_count; ++i)
            {
                std::vector<uint8_t> key_bytes;
                std::vector<TID> payload;
                status = BTreePage::get_node(reinterpret_cast<const uint8_t *>(page_data),
                                             page->btr_header.page_size,
                                             i,
                                             key_bytes,
                                             payload);
                EXPECT_EQ(status, Status::OK);
                if (status != Status::OK || key_bytes.size() < sizeof(int32_t))
                {
                    break;
                }

                if (key_bytes == search_key)
                {
                    matches.insert(matches.end(), payload.begin(), payload.end());
                }
            }

            const uint64_t next_page = page->btr_right_sibling;
            db_->buffer_pool()->unpinPageGlobal(makeGPID(tablespace_id, current_page),
                                                false,
                                                &ctx);
            current_page = next_page;
        }

        return matches;
    }

    uint64_t cachedBTreeRootPage(const ID &index_id)
    {
        ErrorContext ctx;
        CatalogManager::IndexInfo index_info;
        EXPECT_EQ(db_->catalog_manager()->getIndex(index_id, index_info, &ctx), Status::OK)
            << ctx.message;

        CatalogManager::IndexType actual_index_type{};
        void *index_ptr = db_->catalog_manager()->getIndexPtr(index_info.index_id, &actual_index_type);
        EXPECT_NE(index_ptr, nullptr) << "cached index object missing";
        EXPECT_EQ(actual_index_type, CatalogManager::IndexType::BTREE);
        if (index_ptr == nullptr || actual_index_type != CatalogManager::IndexType::BTREE)
        {
            return 0;
        }

        return static_cast<BTree *>(index_ptr)->rootPage();
    }

    struct BTreePathStep
    {
        uint64_t page_num = 0;
        uint16_t level = 0;
        uint16_t count = 0;
        uint16_t flags = 0;
        uint64_t parent_page = 0;
        uint64_t rightmost_child = 0;
        uint64_t chosen_child = 0;
        int32_t first_key = 0;
        int32_t last_key = 0;
        bool is_leaf = false;
        bool has_keys = false;
        bool bloom_attached = false;
    };

    std::vector<BTreePathStep> traceBTreePath(const ID &index_id, int32_t value)
    {
        ErrorContext ctx;
        CatalogManager::IndexInfo index_info;
        EXPECT_EQ(db_->catalog_manager()->getIndex(index_id, index_info, &ctx), Status::OK)
            << ctx.message;

        auto btree = BTree::open(db_.get(), index_id, index_info.root_gpid, &ctx);
        EXPECT_NE(btree, nullptr) << ctx.message;

        const auto key = encodeIndexedInt32Key(value);
        std::vector<BTreePathStep> steps;
        uint64_t current_page = getPageNumber(index_info.root_gpid);
        const uint16_t tablespace_id =
            index_info.tablespace_id != 0 ? index_info.tablespace_id
                                          : getTablespaceID(index_info.root_gpid);

        while (current_page != 0)
        {
            void *page_data = nullptr;
            Status status = db_->buffer_pool()->pinPageGlobal(makeGPID(tablespace_id,
                                                                       current_page),
                                                              &page_data,
                                                              &ctx);
            EXPECT_EQ(status, Status::OK) << ctx.message;
            if (status != Status::OK)
            {
                return steps;
            }

            auto *page = reinterpret_cast<SBBTreePage *>(page_data);
            BTreePathStep step;
            step.page_num = current_page;
            step.level = page->btr_level;
            step.count = page->btr_count;
            step.flags = page->btr_flags;
            step.parent_page = page->btr_parent_page;
            step.rightmost_child = page->btr_rightmost_child;
            step.is_leaf =
                page->btr_header.page_type ==
                    static_cast<uint16_t>(PageType::PAGE_TYPE_BTREE_LEAF) ||
                ((page->btr_flags & static_cast<uint16_t>(BTreeFlags::LEAF)) != 0 &&
                 page->btr_level == 0);
            step.bloom_attached = btree->getBloomFilter() != nullptr;

            if (page->btr_count > 0)
            {
                std::vector<uint8_t> first_key_bytes;
                std::vector<TID> node_payload;
                status = BTreePage::get_node(reinterpret_cast<const uint8_t *>(page_data),
                                             page->btr_header.page_size,
                                             0,
                                             first_key_bytes,
                                             node_payload);
                EXPECT_EQ(status, Status::OK);
                if (status != Status::OK)
                {
                    db_->buffer_pool()->unpinPageGlobal(makeGPID(tablespace_id,
                                                                 current_page),
                                                        false,
                                                        &ctx);
                    return steps;
                }
                if (first_key_bytes.size() >= sizeof(int32_t))
                {
                    std::memcpy(&step.first_key, first_key_bytes.data(), sizeof(int32_t));
                }

                std::vector<uint8_t> last_key_bytes;
                node_payload.clear();
                status = BTreePage::get_node(reinterpret_cast<const uint8_t *>(page_data),
                                             page->btr_header.page_size,
                                             static_cast<uint16_t>(page->btr_count - 1),
                                             last_key_bytes,
                                             node_payload);
                EXPECT_EQ(status, Status::OK);
                if (status != Status::OK)
                {
                    db_->buffer_pool()->unpinPageGlobal(makeGPID(tablespace_id,
                                                                 current_page),
                                                        false,
                                                        &ctx);
                    return steps;
                }
                if (last_key_bytes.size() >= sizeof(int32_t))
                {
                    std::memcpy(&step.last_key, last_key_bytes.data(), sizeof(int32_t));
                }
                step.has_keys = true;
            }

            if (!step.is_leaf)
            {
                uint64_t next_page = 0;
                for (uint16_t i = 0; i < page->btr_count; ++i)
                {
                    std::vector<uint8_t> node_key_bytes;
                    std::vector<TID> node_payload;
                    status = BTreePage::get_node(reinterpret_cast<const uint8_t *>(page_data),
                                                 page->btr_header.page_size,
                                                 i,
                                                 node_key_bytes,
                                                 node_payload);
                    EXPECT_EQ(status, Status::OK);
                    if (status != Status::OK)
                    {
                        db_->buffer_pool()->unpinPageGlobal(makeGPID(tablespace_id,
                                                                     current_page),
                                                            false,
                                                            &ctx);
                        return steps;
                    }
                    if (compareEncodedKeys(key, node_key_bytes) < 0)
                    {
                        EXPECT_FALSE(node_payload.empty());
                        if (node_payload.empty())
                        {
                            db_->buffer_pool()->unpinPageGlobal(makeGPID(tablespace_id,
                                                                         current_page),
                                                                false,
                                                                &ctx);
                            return steps;
                        }
                        next_page = node_payload.front().gpid;
                        break;
                    }
                }
                if (next_page == 0)
                {
                    next_page = page->btr_rightmost_child;
                }
                step.chosen_child = next_page;
            }

            steps.push_back(step);
            status = db_->buffer_pool()->unpinPageGlobal(makeGPID(tablespace_id,
                                                                  current_page),
                                                         false,
                                                         &ctx);
            EXPECT_EQ(status, Status::OK) << ctx.message;
            if (status != Status::OK)
            {
                return steps;
            }

            if (step.is_leaf)
            {
                break;
            }
            current_page = step.chosen_child;
        }

        return steps;
    }

    struct LeafKeyLocation
    {
        bool found = false;
        bool cycle_detected = false;
        uint64_t page_num = 0;
        uint64_t parent_page = 0;
        uint16_t slot_index = 0;
        int32_t first_key = 0;
        int32_t last_key = 0;
        uint16_t key_count = 0;
        uint64_t leaf_count = 0;
        uint16_t raw_flags = 0;
        uint16_t raw_prefix_len = 0;
        uint16_t raw_key_len = 0;
        uint32_t raw_tuple_count = 0;
        uint64_t raw_xmin = 0;
        uint64_t raw_xmax = 0;
        TID raw_first_tid{};
        std::vector<TID> matching_tids;
    };

    std::unordered_set<uint64_t> collectReachableLeafPagesFromRoot(const ID &index_id)
    {
        ErrorContext ctx;
        CatalogManager::IndexInfo index_info{};
        EXPECT_EQ(db_->catalog_manager()->getIndex(index_id, index_info, &ctx), Status::OK)
            << ctx.message;

        const uint16_t tablespace_id =
            index_info.tablespace_id != 0 ? index_info.tablespace_id
                                          : getTablespaceID(index_info.root_gpid);
        std::unordered_set<uint64_t> visited_pages;
        std::unordered_set<uint64_t> leaf_pages;
        std::vector<uint64_t> stack = {getPageNumber(index_info.root_gpid)};

        while (!stack.empty())
        {
            const uint64_t current_page = stack.back();
            stack.pop_back();
            if (current_page == 0 || !visited_pages.insert(current_page).second)
            {
                continue;
            }

            void *page_data = nullptr;
            Status status = db_->buffer_pool()->pinPageGlobal(makeGPID(tablespace_id, current_page),
                                                              &page_data,
                                                              &ctx);
            EXPECT_EQ(status, Status::OK) << ctx.message;
            if (status != Status::OK)
            {
                continue;
            }

            auto *page = reinterpret_cast<SBBTreePage *>(page_data);
            const bool is_leaf =
                page->btr_header.page_type ==
                    static_cast<uint16_t>(PageType::PAGE_TYPE_BTREE_LEAF) ||
                ((page->btr_flags & static_cast<uint16_t>(BTreeFlags::LEAF)) != 0 &&
                 page->btr_level == 0);

            if (is_leaf)
            {
                leaf_pages.insert(current_page);
            }
            else
            {
                auto *offsets = reinterpret_cast<const uint16_t *>(
                    reinterpret_cast<const uint8_t *>(page_data) + sizeof(SBBTreePage));
                for (uint16_t i = 0; i < page->btr_count; ++i)
                {
                    const auto *node = reinterpret_cast<const SBBTreeNode *>(
                        reinterpret_cast<const uint8_t *>(page_data) + offsets[i]);
                    if (node->btn_child_page != 0)
                    {
                        stack.push_back(node->btn_child_page);
                    }
                }
                if (page->btr_rightmost_child != 0)
                {
                    stack.push_back(page->btr_rightmost_child);
                }
            }

            db_->buffer_pool()->unpinPageGlobal(makeGPID(tablespace_id, current_page), false, &ctx);
        }

        return leaf_pages;
    }

    bool pageDirectlyReferencesChild(const ID &index_id, uint64_t parent_page, uint64_t child_page)
    {
        ErrorContext ctx;
        CatalogManager::IndexInfo index_info{};
        EXPECT_EQ(db_->catalog_manager()->getIndex(index_id, index_info, &ctx), Status::OK)
            << ctx.message;

        const uint16_t tablespace_id =
            index_info.tablespace_id != 0 ? index_info.tablespace_id
                                          : getTablespaceID(index_info.root_gpid);

        void *page_data = nullptr;
        Status status = db_->buffer_pool()->pinPageGlobal(makeGPID(tablespace_id, parent_page),
                                                          &page_data,
                                                          &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        if (status != Status::OK)
        {
            return false;
        }

        const auto *page = reinterpret_cast<const SBBTreePage *>(page_data);
        bool found = page->btr_rightmost_child == child_page;
        if (!found)
        {
            auto *offsets = reinterpret_cast<const uint16_t *>(
                reinterpret_cast<const uint8_t *>(page_data) + sizeof(SBBTreePage));
            for (uint16_t i = 0; i < page->btr_count; ++i)
            {
                const auto *node = reinterpret_cast<const SBBTreeNode *>(
                    reinterpret_cast<const uint8_t *>(page_data) + offsets[i]);
                if (node->btn_child_page == child_page)
                {
                    found = true;
                    break;
                }
            }
        }

        db_->buffer_pool()->unpinPageGlobal(makeGPID(tablespace_id, parent_page), false, &ctx);
        return found;
    }

    std::string describeParentChildWindow(const ID &index_id,
                                          uint64_t parent_page,
                                          uint64_t target_child_page,
                                          size_t radius = 3)
    {
        ErrorContext ctx;
        CatalogManager::IndexInfo index_info{};
        EXPECT_EQ(db_->catalog_manager()->getIndex(index_id, index_info, &ctx), Status::OK)
            << ctx.message;

        const uint16_t tablespace_id =
            index_info.tablespace_id != 0 ? index_info.tablespace_id
                                          : getTablespaceID(index_info.root_gpid);

        void *page_data = nullptr;
        Status status = db_->buffer_pool()->pinPageGlobal(makeGPID(tablespace_id, parent_page),
                                                          &page_data,
                                                          &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        if (status != Status::OK)
        {
            return "pin_failed";
        }

        const auto *page = reinterpret_cast<const SBBTreePage *>(page_data);
        std::vector<uint64_t> children;
        children.reserve(static_cast<size_t>(page->btr_count) + 1);

        auto *offsets = reinterpret_cast<const uint16_t *>(
            reinterpret_cast<const uint8_t *>(page_data) + sizeof(SBBTreePage));
        for (uint16_t i = 0; i < page->btr_count; ++i)
        {
            const auto *node = reinterpret_cast<const SBBTreeNode *>(
                reinterpret_cast<const uint8_t *>(page_data) + offsets[i]);
            children.push_back(node->btn_child_page);
        }
        children.push_back(page->btr_rightmost_child);

        size_t target_index = children.size();
        for (size_t i = 0; i < children.size(); ++i)
        {
            if (children[i] == target_child_page)
            {
                target_index = i;
                break;
            }
        }

        std::ostringstream out;
        out << "target_index=";
        if (target_index == children.size())
        {
            out << "missing";
        }
        else
        {
            out << target_index;
        }
        out << " children=" << children.size() << " window=";

        const size_t center =
            (target_index == children.size() || children.empty()) ? 0 : target_index;
        const size_t start =
            (children.empty() || center < radius) ? 0 : (center - radius);
        const size_t end = children.empty()
                               ? 0
                               : std::min(children.size(), center + radius + 1);

        for (size_t i = start; i < end; ++i)
        {
            out << "[" << i << ":" << children[i];
            if (i < page->btr_count)
            {
                std::vector<uint8_t> key_bytes;
                std::vector<TID> payload;
                Status key_status = BTreePage::get_node(
                    reinterpret_cast<const uint8_t *>(page_data),
                    page->btr_header.page_size,
                    static_cast<uint16_t>(i),
                    key_bytes,
                    payload);
                if (key_status == Status::OK && key_bytes.size() >= sizeof(int32_t))
                {
                    int32_t key_value = 0;
                    std::memcpy(&key_value, key_bytes.data(), sizeof(int32_t));
                    out << "->" << key_value;
                }
            }
            else
            {
                out << "->rightmost";
            }
            out << "]";
        }

        db_->buffer_pool()->unpinPageGlobal(makeGPID(tablespace_id, parent_page), false, &ctx);
        return out.str();
    }

    LeafKeyLocation findKeyInLeafChain(const ID &index_id, int32_t value)
    {
        ErrorContext ctx;
        CatalogManager::IndexInfo index_info{};
        EXPECT_EQ(db_->catalog_manager()->getIndex(index_id, index_info, &ctx), Status::OK)
            << ctx.message;

        LeafKeyLocation result;
        const auto search_key = encodeIndexedInt32Key(value);
        const uint16_t tablespace_id =
            index_info.tablespace_id != 0 ? index_info.tablespace_id
                                          : getTablespaceID(index_info.root_gpid);
        std::unordered_set<uint64_t> visited_leaf_pages;
        uint64_t current_page = getPageNumber(index_info.root_gpid);
        while (current_page != 0)
        {
            void *page_data = nullptr;
            Status status = db_->buffer_pool()->pinPageGlobal(makeGPID(tablespace_id,
                                                                       current_page),
                                                              &page_data,
                                                              &ctx);
            EXPECT_EQ(status, Status::OK) << ctx.message;
            if (status != Status::OK)
            {
                return result;
            }

            auto *page = reinterpret_cast<SBBTreePage *>(page_data);
            const bool is_leaf =
                page->btr_header.page_type ==
                    static_cast<uint16_t>(PageType::PAGE_TYPE_BTREE_LEAF) ||
                ((page->btr_flags & static_cast<uint16_t>(BTreeFlags::LEAF)) != 0 &&
                 page->btr_level == 0);

            if (is_leaf)
            {
                break;
            }

            auto *offsets = reinterpret_cast<const uint16_t *>(
                reinterpret_cast<const uint8_t *>(page_data) + sizeof(SBBTreePage));
            uint64_t next_page = page->btr_count > 0
                                     ? reinterpret_cast<const SBBTreeNode *>(
                                           reinterpret_cast<const uint8_t *>(page_data) +
                                           offsets[0])
                                           ->btn_child_page
                                     : page->btr_rightmost_child;
            db_->buffer_pool()->unpinPageGlobal(makeGPID(tablespace_id, current_page),
                                                false,
                                                &ctx);
            EXPECT_EQ(status, Status::OK) << ctx.message;
            current_page = next_page;
        }

        while (current_page != 0)
        {
            if (!visited_leaf_pages.insert(current_page).second)
            {
                result.cycle_detected = true;
                return result;
            }

            void *page_data = nullptr;
            Status status = db_->buffer_pool()->pinPageGlobal(makeGPID(tablespace_id,
                                                                       current_page),
                                                              &page_data,
                                                              &ctx);
            EXPECT_EQ(status, Status::OK) << ctx.message;
            if (status != Status::OK)
            {
                return result;
            }

            auto *page = reinterpret_cast<SBBTreePage *>(page_data);
            ++result.leaf_count;
            result.page_num = current_page;
            result.parent_page = page->btr_parent_page;
            result.key_count = page->btr_count;

            if (page->btr_count > 0)
            {
                std::vector<uint8_t> first_key_bytes;
                std::vector<TID> payload;
                status = BTreePage::get_node(reinterpret_cast<const uint8_t *>(page_data),
                                             page->btr_header.page_size,
                                             0,
                                             first_key_bytes,
                                             payload);
                EXPECT_EQ(status, Status::OK);
                if (status == Status::OK && first_key_bytes.size() >= sizeof(int32_t))
                {
                    std::memcpy(&result.first_key, first_key_bytes.data(), sizeof(int32_t));
                }

                std::vector<uint8_t> last_key_bytes;
                payload.clear();
                status = BTreePage::get_node(reinterpret_cast<const uint8_t *>(page_data),
                                             page->btr_header.page_size,
                                             static_cast<uint16_t>(page->btr_count - 1),
                                             last_key_bytes,
                                             payload);
                EXPECT_EQ(status, Status::OK);
                if (status == Status::OK && last_key_bytes.size() >= sizeof(int32_t))
                {
                    std::memcpy(&result.last_key, last_key_bytes.data(), sizeof(int32_t));
                }
            }

            for (uint16_t i = 0; i < page->btr_count; ++i)
            {
                std::vector<uint8_t> key_bytes;
                std::vector<TID> payload;
                status = BTreePage::get_node(reinterpret_cast<const uint8_t *>(page_data),
                                             page->btr_header.page_size,
                                             i,
                                             key_bytes,
                                             payload);
                EXPECT_EQ(status, Status::OK);
                if (status != Status::OK || key_bytes.size() < sizeof(int32_t))
                {
                    break;
                }

                if (key_bytes == search_key)
                {
                    auto *offsets = reinterpret_cast<const uint16_t *>(
                        reinterpret_cast<const uint8_t *>(page_data) + sizeof(SBBTreePage));
                    const auto *node = reinterpret_cast<const SBBTreeNode *>(
                        reinterpret_cast<const uint8_t *>(page_data) + offsets[i]);
                    result.found = true;
                    result.slot_index = i;
                    result.raw_flags = node->btn_flags;
                    result.raw_prefix_len = node->btn_prefix_len;
                    result.raw_key_len = node->btn_key_len;
                    result.raw_tuple_count = node->btn_tuple_count;
                    result.raw_xmin = node->btn_xmin;
                    result.raw_xmax = node->btn_xmax;
                    if (node->btn_tuple_count > 0)
                    {
                        const auto *raw_tids = reinterpret_cast<const OnDiskTID *>(
                            reinterpret_cast<const uint8_t *>(node) +
                            sizeof(SBBTreeNode) + node->btn_key_len);
                        result.raw_first_tid = fromOnDiskTID(raw_tids[0]);
                    }
                    result.matching_tids = payload;
                    break;
                }
            }

            const uint64_t next_page = page->btr_right_sibling;
            db_->buffer_pool()->unpinPageGlobal(makeGPID(tablespace_id, current_page),
                                                false,
                                                &ctx);
            EXPECT_EQ(status, Status::OK) << ctx.message;

            if (result.found)
            {
                return result;
            }
            current_page = next_page;
        }

        return result;
    }

    std::vector<std::string> readTraceFieldValues(const std::string &trace_path,
                                                  const std::string &table_name,
                                                  const std::string &field_name)
    {
        std::vector<std::string> values;
        std::ifstream input(trace_path);
        std::string line;
        const std::string table_token = "table=" + table_name + " ";
        const std::string field_token = field_name + "=";

        while (std::getline(input, line))
        {
            if (line.find(table_token) == std::string::npos)
            {
                continue;
            }

            const size_t start = line.find(field_token);
            if (start == std::string::npos)
            {
                continue;
            }

            size_t end = line.find(' ', start + field_token.size());
            if (end == std::string::npos)
            {
                end = line.size();
            }

            values.push_back(line.substr(start + field_token.size(),
                                         end - (start + field_token.size())));
        }

        return values;
    }

    std::vector<uint32_t> readTracePagesScanned(const std::string &trace_path,
                                                const std::string &table_name)
    {
        std::vector<uint32_t> pages_scanned;
        for (const auto &value : readTraceFieldValues(trace_path, table_name, "pages_scanned"))
        {
            pages_scanned.push_back(static_cast<uint32_t>(std::stoul(value)));
        }
        return pages_scanned;
    }

    std::string test_db_path_;
    std::vector<std::string> tablespace_paths_;
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

TEST_F(StorageEngineTest, BulkInsertHandleReusesWritableHeapPage)
{
    ErrorContext ctx;
    const ID table_id = createSingleIntTable("bulk_insert_handle_reuse");

    StorageEngine::BulkInsertHandle handle{};
    ASSERT_EQ(engine_->beginBulkInsert(table_id, &handle, &ctx), Status::OK) << ctx.message;

    const auto first_tuple = buildIntTuple(10);
    const auto second_tuple = buildIntTuple(20);

    uint32_t first_page_id = 0;
    uint16_t first_item_id = 0;
    ASSERT_EQ(engine_->insertTupleWithHandle(&handle,
                                             first_tuple.data(),
                                             static_cast<uint32_t>(first_tuple.size()),
                                             &first_page_id,
                                             &first_item_id,
                                             &ctx),
              Status::OK) << ctx.message;
    ASSERT_NE(handle.pinned_gpid, INVALID_GPID);

    uint32_t second_page_id = 0;
    uint16_t second_item_id = 0;
    ASSERT_EQ(engine_->insertTupleWithHandle(&handle,
                                             second_tuple.data(),
                                             static_cast<uint32_t>(second_tuple.size()),
                                             &second_page_id,
                                             &second_item_id,
                                             &ctx),
              Status::OK) << ctx.message;

    EXPECT_EQ(first_page_id, second_page_id);
    EXPECT_LT(first_item_id, second_item_id);

    engine_->endBulkInsert(&handle, &ctx);
    EXPECT_EQ(handle.pinned_gpid, INVALID_GPID);

    Tuple retrieved{};
    ASSERT_EQ(engine_->getTuple(second_page_id, second_item_id, &retrieved, &ctx), Status::OK)
        << ctx.message;
    const auto *tuple_header = reinterpret_cast<const TupleHeader *>(retrieved.data);
    const auto *value_ptr =
        reinterpret_cast<const int32_t *>(retrieved.data + sizeof(TupleHeader));
    EXPECT_EQ(tuple_header->xmin, conn_ctx_->getCurrentXid());
    EXPECT_EQ(*value_ptr, 20);
}

TEST_F(StorageEngineTest, BulkInsertHandleReservesGrowthWindowForAppendHeavyWideRows)
{
    ErrorContext ctx;
    const ID table_id = createSingleIntTable("bulk_insert_growth_window");

    StorageEngine::BulkInsertHandle handle{};
    ASSERT_EQ(engine_->beginBulkInsert(table_id, &handle, &ctx), Status::OK) << ctx.message;

    const uint32_t toast_threshold = ToastSettings::getThreshold(kPageSize);
    ASSERT_GT(toast_threshold, sizeof(TupleHeader) + 32U);
    const size_t inline_payload_size =
        static_cast<size_t>(toast_threshold - sizeof(TupleHeader) - 32U);
    auto wide_inline_tuple = buildFilledTuple(inline_payload_size, 0x5A);
    ASSERT_FALSE(ToastManager::shouldToast(static_cast<uint32_t>(wide_inline_tuple.size()),
                                           kPageSize));

    std::unordered_set<uint32_t> page_ids;
    for (int row = 0; row < 80; ++row)
    {
        uint32_t page_id = 0;
        uint16_t item_id = 0;
        ASSERT_EQ(engine_->insertTupleWithHandle(&handle,
                                                 wide_inline_tuple.data(),
                                                 static_cast<uint32_t>(wide_inline_tuple.size()),
                                                 &page_id,
                                                 &item_id,
                                                 &ctx),
                  Status::OK) << ctx.message;
        page_ids.insert(page_id);
    }

    EXPECT_GE(page_ids.size(), 3U);
    EXPECT_FALSE(handle.reservation_failed);
    EXPECT_GE(handle.reservation_events, 1U);
    EXPECT_GE(handle.total_reserved_pages, 8U);
    EXPECT_GE(handle.consumed_reserved_pages, 2U);
    EXPECT_EQ(handle.total_reserved_pages,
              handle.reserved_page_budget + handle.consumed_reserved_pages);

    engine_->endBulkInsert(&handle, &ctx);
    EXPECT_EQ(handle.total_reserved_pages, 0U);
}

TEST_F(StorageEngineTest, BulkInsertHandleBuildsPostInsertMaintenancePlanOnce)
{
    ErrorContext ctx;
    const ID table_id = createSingleIntTable("bulk_insert_maintenance_plan");
    createSingleIntIndex(table_id, "ix_bulk_insert_maintenance_plan_secondary", false);

    StorageEngine::BulkInsertHandle handle{};
    ASSERT_EQ(engine_->beginBulkInsert(table_id, &handle, &ctx), Status::OK) << ctx.message;

    EXPECT_TRUE(handle.maintenance_plan_built);
    EXPECT_EQ(handle.maintenance_plan_index_count, 1u);
    EXPECT_EQ(handle.maintenance_plan_exact_index_count, 1u);
    EXPECT_EQ(handle.maintenance_plan_active_maintenance_count, 0u);
    EXPECT_EQ(handle.maintenance_plan_deferred_exact_index_count, 0u);
    EXPECT_EQ(handle.maintenance_plan_grouped_exact_index_count, 1u);

    auto tuple = buildIntTuple(33);
    uint32_t page_id = 0;
    uint16_t item_id = 0;
    ASSERT_EQ(engine_->insertTupleWithHandle(&handle,
                                             tuple.data(),
                                             static_cast<uint32_t>(tuple.size()),
                                             &page_id,
                                             &item_id,
                                             &ctx),
              Status::OK) << ctx.message;

    engine_->endBulkInsert(&handle, &ctx);
}

TEST_F(StorageEngineTest, BulkInsertHandleHoistsDeferredExactBacklogStateOnce)
{
    ScopedEnvVar force_cold_delta("SCRATCHBIRD_FORCE_COLD_EXACT_DELTA_BUFFER", "1");

    ErrorContext ctx;
    const ID table_id = createSingleIntTable("bulk_insert_deferred_exact_plan");
    const ID index_id =
        createSingleIntIndex(table_id, "ix_bulk_insert_deferred_exact_plan_secondary", false);

    auto seed_tuple = buildIntTuple(11);
    uint32_t seed_page_id = 0;
    uint16_t seed_item_id = 0;
    ASSERT_EQ(engine_->insertTuple(table_id,
                                   seed_tuple.data(),
                                   static_cast<uint32_t>(seed_tuple.size()),
                                   &seed_page_id,
                                   &seed_item_id,
                                   &ctx),
              Status::OK) << ctx.message;

    std::vector<CatalogManager::IndexPageDeltaCatalogInfo> rows;
    ASSERT_EQ(db_->catalog_manager()->listIndexPageDeltaCatalogEntries(index_id, rows, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(rows.size(), 1u);

    StorageEngine::BulkInsertHandle handle{};
    ASSERT_EQ(engine_->beginBulkInsert(table_id, &handle, &ctx), Status::OK) << ctx.message;

    EXPECT_TRUE(handle.maintenance_plan_built);
    EXPECT_EQ(handle.maintenance_plan_index_count, 1u);
    EXPECT_EQ(handle.maintenance_plan_exact_index_count, 1u);
    EXPECT_EQ(handle.maintenance_plan_deferred_exact_index_count, 1u);

    auto tuple = buildIntTuple(12);
    uint32_t page_id = 0;
    uint16_t item_id = 0;
    ASSERT_EQ(engine_->insertTupleWithHandle(&handle,
                                             tuple.data(),
                                             static_cast<uint32_t>(tuple.size()),
                                             &page_id,
                                             &item_id,
                                             &ctx),
              Status::OK) << ctx.message;

    rows.clear();
    ASSERT_EQ(db_->catalog_manager()->listIndexPageDeltaCatalogEntries(index_id, rows, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_EQ(rows.size(), 1u);

    engine_->endBulkInsert(&handle, &ctx);

    rows.clear();
    ASSERT_EQ(db_->catalog_manager()->listIndexPageDeltaCatalogEntries(index_id, rows, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_EQ(rows.size(), 2u);

    const auto publication =
        findCleanupPublication("ix_bulk_insert_deferred_exact_plan_secondary");
    ASSERT_TRUE(publication.has_value());
    EXPECT_EQ(publication->family, IndexCleanupFamily::EXACT);
    EXPECT_EQ(publication->state, IndexCleanupPublicationState::DEBT_PUBLISHED);
    EXPECT_EQ(publication->backlog_count, 2u);
    EXPECT_EQ(publication->backlog_pages, 2u);
    EXPECT_FALSE(publication->repair_required);

    const auto health = loadIndexHealth(index_id);
    EXPECT_EQ(health.cleanup_backlog_count, 2u);
    EXPECT_EQ(health.cleanup_backlog_pages, 2u);
    EXPECT_EQ(health.cleanup_backlog_bytes, 0u);
    EXPECT_FALSE(health.cleanup_repair_required);
}

TEST_F(StorageEngineTest, BulkInsertHandleGroupsDirectExactSecondaryRowsUntilFlush)
{
    ErrorContext ctx;
    const ID table_id = createSingleIntTable("bulk_insert_grouped_exact_plan");
    const ID index_id =
        createSingleIntIndex(table_id, "ix_bulk_insert_grouped_exact_plan_secondary", false);

    StorageEngine::BulkInsertHandle handle{};
    ASSERT_EQ(engine_->beginBulkInsert(table_id, &handle, &ctx), Status::OK) << ctx.message;

    EXPECT_TRUE(handle.maintenance_plan_built);
    EXPECT_EQ(handle.maintenance_plan_index_count, 1u);
    EXPECT_EQ(handle.maintenance_plan_exact_index_count, 1u);
    EXPECT_EQ(handle.maintenance_plan_deferred_exact_index_count, 0u);
    EXPECT_EQ(handle.maintenance_plan_grouped_exact_index_count, 1u);

    const auto tuple = buildIntTuple(41);
    uint32_t page_id = 0;
    uint16_t item_id = 0;
    ASSERT_EQ(engine_->insertTupleWithHandle(&handle,
                                             tuple.data(),
                                             static_cast<uint32_t>(tuple.size()),
                                             &page_id,
                                             &item_id,
                                             &ctx),
              Status::OK) << ctx.message;

    auto scan = engine_->createIndexScan(index_id, &ctx);
    ASSERT_NE(scan, nullptr) << ctx.message;
    ASSERT_EQ(scan->seek(encodeIntKey(41), &ctx), Status::OK) << ctx.message;
    Tuple before_flush{};
    EXPECT_EQ(scan->next(&before_flush, &ctx), Status::NOT_FOUND);

    engine_->endBulkInsert(&handle, &ctx);

    const TID stable_tid(makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(page_id)), item_id);
    expectIndexSeekFindsTid(index_id, 41, stable_tid);
}

TEST_F(StorageEngineTest, BulkInsertHandleBuffersEmptyUniqueMaintenanceForExactIndex)
{
    ErrorContext ctx;
    const ID table_id = createSingleIntTable("bulk_insert_buffered_unique_plan");
    const ID index_id =
        createSingleIntIndex(table_id, "uq_bulk_insert_buffered_unique_plan", true);

    StorageEngine::BulkInsertHandle handle{};
    ASSERT_EQ(engine_->beginBulkInsert(table_id, &handle, &ctx), Status::OK) << ctx.message;

    EXPECT_TRUE(handle.maintenance_plan_built);
    EXPECT_EQ(handle.maintenance_plan_index_count, 1u);
    EXPECT_EQ(handle.maintenance_plan_exact_index_count, 1u);
    EXPECT_EQ(handle.maintenance_plan_unique_exact_index_count, 1u);
    EXPECT_EQ(handle.maintenance_plan_deferred_exact_index_count, 0u);
    EXPECT_EQ(handle.maintenance_plan_grouped_exact_index_count, 0u);
    EXPECT_EQ(handle.maintenance_plan_buffered_empty_unique_index_count, 1u);

    const auto tuple = buildIntTuple(51);
    uint32_t page_id = 0;
    uint16_t item_id = 0;
    ASSERT_EQ(engine_->insertTupleWithHandle(&handle,
                                             tuple.data(),
                                             static_cast<uint32_t>(tuple.size()),
                                             &page_id,
                                             &item_id,
                                             &ctx),
              Status::OK) << ctx.message;
    EXPECT_EQ(handle.timing_buffered_unique_fast_scalar_rows, 1u);

    auto scan = engine_->createIndexScan(index_id, &ctx);
    ASSERT_NE(scan, nullptr) << ctx.message;
    ASSERT_EQ(scan->seek(encodeIntKey(51), &ctx), Status::OK) << ctx.message;
    Tuple before_flush{};
    EXPECT_EQ(scan->next(&before_flush, &ctx), Status::NOT_FOUND);

    engine_->endBulkInsert(&handle, &ctx);

    const TID stable_tid(makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(page_id)), item_id);
    expectIndexSeekFindsTid(index_id, 51, stable_tid);

    ASSERT_EQ(conn_ctx_->commit(&ctx), Status::OK) << ctx.message;

    expectIndexSeekFindsTid(index_id, 51, stable_tid);
}

TEST_F(StorageEngineTest, BulkInsertHandleRejectsDuplicateKeyWithBufferedEmptyUniqueMode)
{
    ErrorContext ctx;
    const ID table_id = createSingleIntTable("bulk_insert_buffered_unique_duplicate");
    const ID index_id =
        createSingleIntIndex(table_id, "uq_bulk_insert_buffered_unique_duplicate", true);

    StorageEngine::BulkInsertHandle handle{};
    ASSERT_EQ(engine_->beginBulkInsert(table_id, &handle, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(handle.maintenance_plan_buffered_empty_unique_index_count, 1u);

    const auto tuple = buildIntTuple(61);
    uint32_t first_page_id = 0;
    uint16_t first_item_id = 0;
    ASSERT_EQ(engine_->insertTupleWithHandle(&handle,
                                             tuple.data(),
                                             static_cast<uint32_t>(tuple.size()),
                                             &first_page_id,
                                             &first_item_id,
                                             &ctx),
              Status::OK) << ctx.message;

    ErrorContext duplicate_ctx;
    uint32_t duplicate_page_id = 0;
    uint16_t duplicate_item_id = 0;
    EXPECT_EQ(engine_->insertTupleWithHandle(&handle,
                                             tuple.data(),
                                             static_cast<uint32_t>(tuple.size()),
                                             &duplicate_page_id,
                                             &duplicate_item_id,
                                             &duplicate_ctx),
              Status::UNIQUE_VIOLATION);

    const TID stable_tid(makeGPID(PRIMARY_TABLESPACE_ID,
                                  static_cast<uint64_t>(first_page_id)),
                         first_item_id);
    expectIndexSeekFindsTid(index_id, 61, stable_tid);

    engine_->endBulkInsert(&handle, &ctx);

    expectIndexSeekFindsTid(index_id, 61, stable_tid);

    ASSERT_EQ(conn_ctx_->commit(&ctx), Status::OK) << ctx.message;

    expectIndexSeekFindsTid(index_id, 61, stable_tid);
}

TEST_F(StorageEngineTest, BulkInsertHandleKeepsBufferedEmptyUniqueRowsOffIndexPastLegacyThreshold)
{
    ErrorContext ctx;
    const ID table_id =
        createSingleIntTable("bulk_insert_buffered_unique_large_window");
    const ID index_id =
        createSingleIntIndex(table_id,
                             "uq_bulk_insert_buffered_unique_large_window",
                             true);

    StorageEngine::BulkInsertHandle handle{};
    ASSERT_EQ(engine_->beginBulkInsert(table_id, &handle, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(handle.maintenance_plan_buffered_empty_unique_index_count, 1u);

    uint32_t first_page_id = 0;
    uint16_t first_item_id = 0;
    uint32_t last_page_id = 0;
    uint16_t last_item_id = 0;
    constexpr int32_t kInsertedRows = 80;
    for (int32_t value = 1; value <= kInsertedRows; ++value)
    {
        auto tuple = buildIntTuple(value);
        uint32_t page_id = 0;
        uint16_t item_id = 0;
        ASSERT_EQ(engine_->insertTupleWithHandle(&handle,
                                                 tuple.data(),
                                                 static_cast<uint32_t>(
                                                     tuple.size()),
                                                 &page_id,
                                                 &item_id,
                                                 &ctx),
                  Status::OK)
            << ctx.message;
        if (value == 1)
        {
            first_page_id = page_id;
            first_item_id = item_id;
        }
        if (value == kInsertedRows)
        {
            last_page_id = page_id;
            last_item_id = item_id;
        }
    }

    auto scan = engine_->createIndexScan(index_id, &ctx);
    ASSERT_NE(scan, nullptr) << ctx.message;
    ASSERT_EQ(scan->seek(encodeIntKey(1), &ctx), Status::OK) << ctx.message;
    Tuple before_flush{};
    EXPECT_EQ(scan->next(&before_flush, &ctx), Status::NOT_FOUND);

    engine_->endBulkInsert(&handle, &ctx);

    const TID first_tid(
        makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(first_page_id)),
        first_item_id);
    const TID last_tid(
        makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(last_page_id)),
        last_item_id);
    expectIndexSeekFindsTid(index_id, 1, first_tid);
    expectIndexSeekFindsTid(index_id, kInsertedRows, last_tid);
}

TEST_F(StorageEngineTest, BulkInsertHandleDefersBufferedEmptyUniqueFlushUntilEndOfStatement)
{
    ErrorContext ctx;
    const ID table_id =
        createSingleIntTable("bulk_insert_buffered_unique_end_flush_only");
    const ID index_id =
        createSingleIntIndex(table_id,
                             "uq_bulk_insert_buffered_unique_end_flush_only",
                             true);

    StorageEngine::BulkInsertHandle handle{};
    ASSERT_EQ(engine_->beginBulkInsert(table_id, &handle, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(handle.maintenance_plan_buffered_empty_unique_index_count, 1u);

    uint32_t first_page_id = 0;
    uint16_t first_item_id = 0;
    uint32_t last_page_id = 0;
    uint16_t last_item_id = 0;
    constexpr int32_t kInsertedRows = 9000;
    for (int32_t value = 1; value <= kInsertedRows; ++value)
    {
        auto tuple = buildIntTuple(value);
        uint32_t page_id = 0;
        uint16_t item_id = 0;
        ASSERT_EQ(engine_->insertTupleWithHandle(&handle,
                                                 tuple.data(),
                                                 static_cast<uint32_t>(tuple.size()),
                                                 &page_id,
                                                 &item_id,
                                                 &ctx),
                  Status::OK)
            << ctx.message;
        if (value == 1)
        {
            first_page_id = page_id;
            first_item_id = item_id;
        }
        if (value == kInsertedRows)
        {
            last_page_id = page_id;
            last_item_id = item_id;
        }
    }

    EXPECT_EQ(handle.timing_post_insert_buffered_flush_unique_calls, 0u);

    auto scan = engine_->createIndexScan(index_id, &ctx);
    ASSERT_NE(scan, nullptr) << ctx.message;
    ASSERT_EQ(scan->seek(encodeIntKey(1), &ctx), Status::OK) << ctx.message;
    Tuple before_flush{};
    EXPECT_EQ(scan->next(&before_flush, &ctx), Status::NOT_FOUND);

    engine_->endBulkInsert(&handle, &ctx);

    const TID first_tid(
        makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(first_page_id)),
        first_item_id);
    const TID last_tid(
        makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(last_page_id)),
        last_item_id);
    expectIndexSeekFindsTid(index_id, 1, first_tid);
    expectIndexSeekFindsTid(index_id, kInsertedRows, last_tid);
}

TEST_F(StorageEngineTest, ColdExactSecondaryCleanupDebtPublishesAndClearsOnMerge)
{
    ScopedEnvVar force_cold_delta("SCRATCHBIRD_FORCE_COLD_EXACT_DELTA_BUFFER", "1");

    ErrorContext ctx;
    const ID table_id = createSingleIntTable("cold_exact_cleanup_publication");
    const ID index_id =
        createSingleIntIndex(table_id, "idx_cold_exact_cleanup_publication", false);

    const auto tuple = buildIntTuple(21, conn_ctx_->getCurrentXid());
    uint32_t page_id = 0;
    uint16_t item_id = 0;
    ASSERT_EQ(engine_->insertTuple(table_id,
                                   tuple.data(),
                                   static_cast<uint32_t>(tuple.size()),
                                   &page_id,
                                   &item_id,
                                   &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(conn_ctx_->commit(&ctx), Status::OK) << ctx.message;

    auto publication = findCleanupPublication("idx_cold_exact_cleanup_publication");
    ASSERT_TRUE(publication.has_value());
    EXPECT_EQ(publication->family, IndexCleanupFamily::EXACT);
    EXPECT_EQ(publication->state, IndexCleanupPublicationState::DEBT_PUBLISHED);
    EXPECT_EQ(publication->backlog_count, 1u);
    EXPECT_EQ(publication->backlog_pages, 1u);
    EXPECT_FALSE(publication->repair_required);

    auto health = loadIndexHealth(index_id);
    EXPECT_EQ(health.cleanup_backlog_count, 1u);
    EXPECT_EQ(health.cleanup_backlog_pages, 1u);
    EXPECT_EQ(health.cleanup_backlog_bytes, 0u);
    EXPECT_FALSE(health.cleanup_repair_required);

    const TID stable_tid(makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(page_id)), item_id);
    expectIndexSeekFindsTid(index_id, 21, stable_tid);

    publication = findCleanupPublication("idx_cold_exact_cleanup_publication");
    ASSERT_TRUE(publication.has_value());
    EXPECT_EQ(publication->family, IndexCleanupFamily::EXACT);
    EXPECT_EQ(publication->state, IndexCleanupPublicationState::COMPLETE);
    EXPECT_EQ(publication->entries_removed, 1u);
    EXPECT_EQ(publication->backlog_count, 0u);
    EXPECT_EQ(publication->backlog_pages, 0u);
    EXPECT_FALSE(publication->repair_required);

    health = loadIndexHealth(index_id);
    EXPECT_EQ(health.cleanup_backlog_count, 0u);
    EXPECT_EQ(health.cleanup_backlog_pages, 0u);
    EXPECT_EQ(health.cleanup_backlog_bytes, 0u);
    EXPECT_FALSE(health.cleanup_repair_required);
}

TEST_F(StorageEngineTest, InsertTuplePublishesHeapPageInCustomTablespace)
{
    ErrorContext ctx;
    const uint16_t tablespace_id = createTestTablespace("storage_engine_insert_ts");
    ASSERT_GE(tablespace_id, 2);

    const ID table_id = createSingleIntTable("storage_engine_insert_ts_table", tablespace_id);

    CatalogManager::TableInfo table_info{};
    ASSERT_EQ(db_->catalog_manager()->getTable(table_id, table_info, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(table_info.tablespace_id, tablespace_id);
    ASSERT_EQ(getTablespaceID(table_info.root_gpid), tablespace_id);

    const int32_t expected_value = 42;
    auto tuple = buildIntTuple(expected_value);

    uint32_t page_id = 0;
    uint16_t item_id = 0;
    ASSERT_EQ(engine_->insertTuple(table_id,
                                   tuple.data(),
                                   static_cast<uint32_t>(tuple.size()),
                                   &page_id,
                                   &item_id,
                                   &ctx),
              Status::OK) << ctx.message;

    const GPID heap_gpid = makeGPID(tablespace_id, static_cast<uint64_t>(page_id));
    void *page_buffer = nullptr;
    ASSERT_EQ(db_->buffer_pool()->pinPageGlobal(heap_gpid, &page_buffer, &ctx), Status::OK)
        << ctx.message;

    auto *page_data = static_cast<uint8_t *>(page_buffer);
    const auto *header = reinterpret_cast<const PageHeader *>(page_data);
    EXPECT_EQ(header->page_type, PAGE_TYPE_HEAP);
    EXPECT_EQ(header->page_id, page_id);

    HeapPage heap(page_data, db_->page_size());
    const uint8_t *tuple_data = nullptr;
    uint32_t tuple_size = 0;
    ASSERT_EQ(heap.getTuple(item_id, &tuple_data, &tuple_size, &ctx), Status::OK)
        << ctx.message;

    const auto *tuple_header = reinterpret_cast<const TupleHeader *>(tuple_data);
    EXPECT_EQ(tuple_header->xmin, conn_ctx_->getCurrentXid());
    EXPECT_EQ(*reinterpret_cast<const int32_t *>(tuple_data + sizeof(TupleHeader)),
              expected_value);

    db_->buffer_pool()->unpinPageGlobal(heap_gpid, false, &ctx);
}

TEST_F(StorageEngineTest, BufferPoolAllocatePageGlobalSupportsCustomTablespace)
{
    ErrorContext ctx;
    const uint16_t tablespace_id = createTestTablespace("buffer_pool_allocate_ts");
    ASSERT_GE(tablespace_id, 2);

    const ID table_id = createSingleIntTable("buffer_pool_allocate_table", tablespace_id);

    GPID gpid = INVALID_GPID;
    void *page_buffer = nullptr;
    ASSERT_EQ(db_->buffer_pool()->allocatePageGlobal(tablespace_id, &gpid, &page_buffer, &ctx),
              Status::OK) << ctx.message;
    ASSERT_EQ(getTablespaceID(gpid), tablespace_id);

    const uint32_t page_id = static_cast<uint32_t>(getPageNumber(gpid));
    auto *page_data = static_cast<uint8_t *>(page_buffer);
    std::memset(page_data, 0, db_->page_size());

    HeapPage heap_page(page_data, db_->page_size(), nullptr, db_.get(), table_id);
    ASSERT_EQ(heap_page.initialize(page_id, &ctx), Status::OK) << ctx.message;
    heap_page.applyOwningTableContract(false);
    db_->buffer_pool()->unpinPageGlobal(gpid, true, &ctx);

    void *verify_buffer = nullptr;
    ASSERT_EQ(db_->buffer_pool()->pinPageGlobal(gpid, &verify_buffer, &ctx), Status::OK)
        << ctx.message;

    const auto *header = reinterpret_cast<const PageHeader *>(verify_buffer);
    EXPECT_EQ(header->page_type, PAGE_TYPE_HEAP);
    EXPECT_EQ(header->page_id, page_id);

    db_->buffer_pool()->unpinPageGlobal(gpid, false, &ctx);
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

TEST_F(StorageEngineTest, DeleteRetainsHistoricalCandidateUntilGc)
{
    ErrorContext ctx;
    ID table_id = createSingleIntTable("delete_hist_candidate");
    ID index_id = createSingleIntIndex(table_id, "idx_delete_hist_candidate", false);

    auto tuple = buildIntTuple(10);
    uint32_t page_id = 0;
    uint16_t item_id = 0;
    ASSERT_EQ(engine_->insertTuple(table_id,
                                   tuple.data(),
                                   static_cast<uint32_t>(tuple.size()),
                                   &page_id,
                                   &item_id,
                                   &ctx),
              Status::OK) << ctx.message;
    ASSERT_EQ(conn_ctx_->commit(&ctx), Status::OK) << ctx.message;

    TID stable_tid(makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(page_id)), item_id);
    ASSERT_EQ(engine_->deleteTuple(table_id, page_id, item_id, UINT16_MAX, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(conn_ctx_->commit(&ctx), Status::OK) << ctx.message;

    auto raw_before_gc = rawBTreeSearch(index_id, 10, 0);
    ASSERT_EQ(raw_before_gc.size(), 1u);
    EXPECT_EQ(raw_before_gc.front(), stable_tid);
    expectIndexSeekNotFound(index_id, 10);

    db_->garbage_collector()->markPageDirty(page_id);
    db_->garbage_collector()->processPageCooperative(page_id, &ctx);

    EXPECT_TRUE(rawBTreeSearch(index_id, 10, 0).empty());
    expectIndexSeekNotFound(index_id, 10);
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

TEST_F(StorageEngineTest, BTreeOpenHydratesCatalogMetadataWhenIndexExists)
{
    ErrorContext ctx;
    ID table_id = createSingleIntTable("btree_open_catalog_metadata");
    ID index_id = createSingleIntIndex(table_id, "uq_btree_open_catalog_metadata", true);

    CatalogManager::IndexInfo catalog_index;
    ASSERT_EQ(db_->catalog_manager()->getIndex(index_id, catalog_index, &ctx), Status::OK)
        << ctx.message;

    auto btree = BTree::open(db_.get(), index_id, catalog_index.root_gpid, &ctx);
    ASSERT_NE(btree, nullptr) << ctx.message;

    const auto &open_info = btree->getIndexInfo();
    EXPECT_EQ(open_info.idx_uuid, catalog_index.index_id);
    EXPECT_EQ(open_info.idx_table_uuid, catalog_index.table_id);
    ASSERT_EQ(open_info.idx_column_ids.size(), catalog_index.column_ids.size());
    ASSERT_EQ(open_info.idx_column_ids.size(), 1u);
    EXPECT_EQ(open_info.idx_column_ids.front(), catalog_index.column_ids.front());
    EXPECT_EQ(open_info.idx_root_page,
              static_cast<uint64_t>(getPageNumber(catalog_index.root_gpid)));
    EXPECT_EQ(open_info.idx_tablespace_id, catalog_index.tablespace_id);
    EXPECT_EQ(open_info.idx_collation_id, catalog_index.collation_id);
    EXPECT_EQ(open_info.idx_flags & 1u, 1u);
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

TEST_F(StorageEngineTest, StableRootIndexUpdateRetainsHistoricalCandidateUntilGc)
{
    ErrorContext ctx;
    ID table_id = createSingleIntTable("stable_root_hist_candidate");
    ID index_id = createSingleIntIndex(table_id, "idx_stable_root_hist_candidate", false);

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

    auto updated_tuple = buildIntTuple(20, conn_ctx_->getCurrentXid());
    ASSERT_EQ(engine_->updateTuple(table_id,
                                   page_id,
                                   item_id,
                                   updated_tuple.data(),
                                   static_cast<uint32_t>(updated_tuple.size()),
                                   nullptr,
                                   nullptr,
                                   &ctx),
              Status::OK) << ctx.message;

    auto raw_old_all = rawBTreeSearch(index_id, 10, 0);
    ASSERT_EQ(raw_old_all.size(), 1u);
    EXPECT_EQ(raw_old_all.front(), stable_tid);

    expectIndexSeekNotFound(index_id, 10);
    expectIndexSeekFindsTid(index_id, 20, stable_tid);
}

TEST_F(StorageEngineTest, SnapshotIndexScanUsesHistoricalKeyAfterCommittedStableRootUpdate)
{
    ErrorContext ctx;
    ID table_id = createSingleIntTable("snapshot_stable_root_hist");
    ID index_id = createSingleIntIndex(table_id, "idx_snapshot_stable_root_hist", false);

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
    ASSERT_EQ(conn_ctx_->commit(&ctx), Status::OK) << ctx.message;

    TID stable_tid(makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(page_id)), item_id);

    std::unique_ptr<ConnectionContext> reader;
    ASSERT_EQ(db_->connect(reader, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(reader->initialize(&ctx), Status::OK) << ctx.message;
    ID system_user = db_->catalog_manager()->getSystemUserId(&ctx);
    reader->setCurrentUser(system_user, true);

    ConnectionContext::setCurrent(reader.get());
    expectIndexSeekFindsTid(index_id, 10, stable_tid);
    expectIndexSeekNotFound(index_id, 20);

    ConnectionContext::setCurrent(conn_ctx_.get());
    auto updated_tuple = buildIntTuple(20, conn_ctx_->getCurrentXid());
    ASSERT_EQ(engine_->updateTuple(table_id,
                                   page_id,
                                   item_id,
                                   updated_tuple.data(),
                                   static_cast<uint32_t>(updated_tuple.size()),
                                   nullptr,
                                   nullptr,
                                   &ctx),
              Status::OK) << ctx.message;
    ASSERT_EQ(conn_ctx_->commit(&ctx), Status::OK) << ctx.message;

    ConnectionContext::setCurrent(reader.get());
    expectIndexSeekFindsTid(index_id, 10, stable_tid);
    expectIndexSeekNotFound(index_id, 20);
    ASSERT_EQ(reader->rollback(&ctx), Status::OK) << ctx.message;

    ConnectionContext::setCurrent(conn_ctx_.get());
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

TEST_F(StorageEngineTest, SavepointRollbackRestoresSoftDeletedHistoricalKeyWithoutDuplicateCandidate)
{
    ErrorContext ctx;
    ID table_id = createSingleIntTable("savepoint_restore_no_duplicate");
    ID index_id = createSingleIntIndex(table_id, "idx_savepoint_restore_no_duplicate", false);

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
    ASSERT_EQ(conn_ctx_->createSavepoint("sp_restore_no_duplicate", &ctx), Status::OK)
        << ctx.message;

    auto updated_tuple = buildIntTuple(20, conn_ctx_->getCurrentXid());
    ASSERT_EQ(engine_->updateTuple(table_id,
                                   page_id,
                                   item_id,
                                   updated_tuple.data(),
                                   static_cast<uint32_t>(updated_tuple.size()),
                                   nullptr,
                                   nullptr,
                                   &ctx),
              Status::OK) << ctx.message;

    ASSERT_EQ(conn_ctx_->rollbackToSavepoint("sp_restore_no_duplicate", &ctx), Status::OK)
        << ctx.message;

    auto raw_old_all = rawBTreeSearch(index_id, 10, 0);
    ASSERT_EQ(raw_old_all.size(), 1u);
    EXPECT_EQ(raw_old_all.front(), stable_tid);
    EXPECT_TRUE(rawBTreeSearch(index_id, 20, 0).empty());

    expectIndexSeekFindsTid(index_id, 10, stable_tid);
    expectIndexSeekNotFound(index_id, 20);
}

TEST_F(StorageEngineTest, UpdateTupleRejectsConflictAcrossOverlappingUniqueIndexes)
{
    ErrorContext ctx;
    const ID table_id = createTripleIntTable("overlapping_unique_update",
                                             "a",
                                             "b",
                                             "c");

    ID uq_ac_id;
    ASSERT_EQ(db_->catalog_manager()->createIndex(table_id,
                                                  "uq_overlapping_unique_ac",
                                                  {"a", "c"},
                                                  uq_ac_id,
                                                  true,
                                                  CatalogManager::IndexType::BTREE,
                                                  PRIMARY_TABLESPACE_ID,
                                                  &ctx),
              Status::OK) << ctx.message;

    ID uq_ab_id;
    ASSERT_EQ(db_->catalog_manager()->createIndex(table_id,
                                                  "uq_overlapping_unique_ab",
                                                  {"a", "b"},
                                                  uq_ab_id,
                                                  true,
                                                  CatalogManager::IndexType::BTREE,
                                                  PRIMARY_TABLESPACE_ID,
                                                  &ctx),
              Status::OK) << ctx.message;

    auto first_row = buildTripleIntTuple(1, 10, 100);
    uint32_t first_page_id = 0;
    uint16_t first_item_id = 0;
    ASSERT_EQ(engine_->insertTuple(table_id,
                                   first_row.data(),
                                   static_cast<uint32_t>(first_row.size()),
                                   &first_page_id,
                                   &first_item_id,
                                   &ctx),
              Status::OK) << ctx.message;

    auto second_row = buildTripleIntTuple(2, 10, 101);
    uint32_t second_page_id = 0;
    uint16_t second_item_id = 0;
    ASSERT_EQ(engine_->insertTuple(table_id,
                                   second_row.data(),
                                   static_cast<uint32_t>(second_row.size()),
                                   &second_page_id,
                                   &second_item_id,
                                   &ctx),
              Status::OK) << ctx.message;

    auto conflicting_update = buildTripleIntTuple(2, 10, 100, conn_ctx_->getCurrentXid());
    Status update_status = engine_->updateTuple(table_id,
                                                first_page_id,
                                                first_item_id,
                                                conflicting_update.data(),
                                                static_cast<uint32_t>(conflicting_update.size()),
                                                nullptr,
                                                nullptr,
                                                &ctx);
    EXPECT_EQ(update_status, Status::UNIQUE_VIOLATION) << ctx.message;
}

TEST_F(StorageEngineTest, SavepointRollbackRestoresOriginalVersionLifecycleState)
{
    ErrorContext ctx;
    ID table_id = createSingleIntTable("savepoint_lifecycle_restore");

    const uint64_t original_xid = conn_ctx_->getCurrentXid();
    auto original_tuple = buildIntTuple(55, original_xid);
    uint32_t page_id = 0;
    uint16_t item_id = 0;
    ASSERT_EQ(engine_->insertTuple(table_id,
                                   original_tuple.data(),
                                   static_cast<uint32_t>(original_tuple.size()),
                                   &page_id,
                                   &item_id,
                                   &ctx),
              Status::OK) << ctx.message;
    ASSERT_EQ(conn_ctx_->commit(&ctx), Status::OK) << ctx.message;

    ASSERT_NE(conn_ctx_->getCurrentXid(), original_xid);
    ASSERT_EQ(conn_ctx_->createSavepoint("sp_lifecycle_restore", &ctx), Status::OK)
        << ctx.message;

    auto updated_tuple = buildIntTuple(66, conn_ctx_->getCurrentXid());
    ASSERT_EQ(engine_->updateTuple(table_id,
                                   page_id,
                                   item_id,
                                   updated_tuple.data(),
                                   static_cast<uint32_t>(updated_tuple.size()),
                                   nullptr,
                                   nullptr,
                                   &ctx),
              Status::OK) << ctx.message;

    ASSERT_EQ(conn_ctx_->rollbackToSavepoint("sp_lifecycle_restore", &ctx), Status::OK)
        << ctx.message;

    Tuple restored{};
    ASSERT_EQ(engine_->getTuple(page_id, item_id, &restored, &ctx), Status::OK) << ctx.message;
    const auto *restored_hdr = reinterpret_cast<const TupleHeader *>(restored.data);
    ASSERT_NE(restored_hdr, nullptr);
    EXPECT_EQ(restored_hdr->xmin, original_xid);
    EXPECT_EQ(restored_hdr->xmax, 0u);
    EXPECT_FALSE(restored_hdr->hasBackVersion());
}

TEST_F(StorageEngineTest, MgaBackoutEngineRemovesInsertedStableHeadRowDirectly)
{
    ErrorContext ctx;
    ID table_id = createSingleIntTable("mga_backout_remove_row");

    ASSERT_NE(db_->mga_backout_engine(), nullptr);

    auto inserted_tuple = buildIntTuple(77);
    uint32_t page_id = 0;
    uint16_t item_id = 0;
    ASSERT_EQ(engine_->insertTuple(table_id,
                                   inserted_tuple.data(),
                                   static_cast<uint32_t>(inserted_tuple.size()),
                                   &page_id,
                                   &item_id,
                                   &ctx),
              Status::OK) << ctx.message;

    SavepointBackoutAction action;
    action.table_id = table_id;
    action.stable_page_id = page_id;
    action.stable_item_id = item_id;

    ASSERT_EQ(db_->mga_backout_engine()->applySavepointBackout({action},
                                                               conn_ctx_->getCurrentXid(),
                                                               &ctx),
              Status::OK) << ctx.message;

    auto scanner = engine_->createScan(table_id, &ctx);
    ASSERT_NE(scanner, nullptr);
    Tuple tuple{};
    EXPECT_EQ(scanner->next(&tuple, &ctx), Status::NOT_FOUND);
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

TEST_F(StorageEngineTest, NoWaitUpdateConflictReturnsDistinctLockNotAvailableStatus)
{
    ErrorContext ctx;
    ID table_id = createSingleIntTable("no_wait_update_conflict_table");

    auto tuple = buildIntTuple(41, conn_ctx_->getCurrentXid());
    uint32_t page_id = 0;
    uint16_t item_id = 0;
    ASSERT_EQ(engine_->insertTuple(table_id,
                                   tuple.data(),
                                   static_cast<uint32_t>(tuple.size()),
                                   &page_id,
                                   &item_id,
                                   &ctx),
              Status::OK)
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
    ASSERT_EQ(db_->lock_manager()->acquireLock(blocker_proc_id,
                                               tag,
                                               LockMode::LOCK_ROW_EXCLUSIVE,
                                               true,
                                               0,
                                               &ctx),
              Status::OK)
        << ctx.message;

    std::unique_ptr<ConnectionContext> conn2;
    ASSERT_EQ(db_->connect(conn2, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(conn2->initialize(&ctx), Status::OK) << ctx.message;
    ID system_user = db_->catalog_manager()->getSystemUserId(&ctx);
    conn2->setCurrentUser(system_user, true);
    ASSERT_EQ(conn2->startTransaction(false, IsolationLevel::READ_COMMITTED, true, &ctx),
              Status::OK)
        << ctx.message;
    conn2->setWaitForLocks(false);

    auto updated_tuple = buildIntTuple(42, conn2->getCurrentXid());
    ConnectionContext::setCurrent(conn2.get());
    uint32_t new_page_id = 0;
    uint16_t new_item_id = 0;
    Status status = engine_->updateTuple(table_id,
                                         page_id,
                                         item_id,
                                         updated_tuple.data(),
                                         static_cast<uint32_t>(updated_tuple.size()),
                                         &new_page_id,
                                         &new_item_id,
                                         &ctx);
    EXPECT_EQ(status, Status::LOCK_NOT_AVAILABLE);
    EXPECT_NE(ctx.message.find("UPDATE_CONFLICT_NO_WAIT"), std::string::npos) << ctx.message;
    EXPECT_EQ(conn2->statementRestartCount(), 0u);

    ConnectionContext::setCurrent(conn_ctx_.get());
    db_->lock_manager()->releaseAllLocks(blocker_proc_id, nullptr);
    db_->transaction_manager()->rollbackTransaction(blocker_proc_id, blocker_xid, nullptr);
    ProcArrayManager::unregisterBackend(blocker_proc_id, &ctx);
}

TEST_F(StorageEngineTest, ReadConsistencyRestartRollsBackEarlierStatementMutations)
{
    ErrorContext ctx;
    ID table_id = createSingleIntTable("rc_restart_statement_scope_table");

    auto first_tuple = buildIntTuple(41, conn_ctx_->getCurrentXid());
    uint32_t first_page_id = 0;
    uint16_t first_item_id = 0;
    ASSERT_EQ(engine_->insertTuple(table_id, first_tuple.data(), static_cast<uint32_t>(first_tuple.size()),
                                   &first_page_id, &first_item_id, &ctx), Status::OK)
        << ctx.message;

    auto second_tuple = buildIntTuple(84, conn_ctx_->getCurrentXid());
    uint32_t second_page_id = 0;
    uint16_t second_item_id = 0;
    ASSERT_EQ(engine_->insertTuple(table_id,
                                   second_tuple.data(),
                                   static_cast<uint32_t>(second_tuple.size()),
                                   &second_page_id,
                                   &second_item_id,
                                   &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(conn_ctx_->commit(&ctx), Status::OK) << ctx.message;

    LockTag tag{};
    tag.target_type = LockTarget::LOCK_TARGET_TUPLE;
    tag.object_uuid = table_id;
    tag.page_num = second_page_id;
    tag.offset_num = second_item_id;
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
    conn2->setWaitForLocks(false);
    ASSERT_EQ(conn2->beginStatementTracking(
                  "UPDATE users.public.rc_restart_statement_scope_table SET id = id + 1", &ctx),
              Status::OK)
        << ctx.message;

    ConnectionContext::setCurrent(conn2.get());

    auto first_update = buildIntTuple(42, conn2->getCurrentXid());
    uint32_t first_new_page_id = 0;
    uint16_t first_new_item_id = 0;
    ASSERT_EQ(engine_->updateTuple(table_id,
                                   first_page_id,
                                   first_item_id,
                                   first_update.data(),
                                   static_cast<uint32_t>(first_update.size()),
                                   &first_new_page_id,
                                   &first_new_item_id,
                                   &ctx),
              Status::OK)
        << ctx.message;

    auto second_update = buildIntTuple(85, conn2->getCurrentXid());
    uint32_t second_new_page_id = 0;
    uint16_t second_new_item_id = 0;
    Status status = engine_->updateTuple(table_id,
                                         second_page_id,
                                         second_item_id,
                                         second_update.data(),
                                         static_cast<uint32_t>(second_update.size()),
                                         &second_new_page_id,
                                         &second_new_item_id,
                                         &ctx);
    EXPECT_EQ(status, Status::SERIALIZATION_FAILURE);
    EXPECT_NE(ctx.message.find("READ_CONSISTENCY_RESTART_REQUIRED"), std::string::npos);
    EXPECT_EQ(conn2->statementRestartCount(), 1u);

    const auto visible_after_restart = readVisibleTupleCopy(table_id, first_page_id, first_item_id);
    expectTuplePayloadEquals(visible_after_restart, first_tuple);

    conn2->endStatementTrackingFailure(static_cast<uint32_t>(status), "40001");
    EXPECT_FALSE(conn2->hasActiveSavepoints());

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

TEST_F(StorageEngineTest, InsertTraceReusesWritablePageHintForSteadyInserts)
{
    const std::string trace_path =
        scratchbird::testing::uniqueTestShortPath("storage_engine_insert_trace", ".log");
    std::filesystem::remove(trace_path);
    ScopedEnvVar trace_enabled("SCRATCHBIRD_INSERT_TRACE", "1");
    ScopedEnvVar trace_file("SCRATCHBIRD_INSERT_TRACE_FILE", trace_path);

    const std::string table_name = "trace_hint_test";
    const ID table_id = createSingleIntTable(table_name);

    ErrorContext ctx;
    for (int32_t value = 1; value <= 3; ++value)
    {
        auto tuple = buildIntTuple(value);
        uint32_t page_id = 0;
        uint16_t item_id = 0;
        ASSERT_EQ(engine_->insertTuple(table_id,
                                       tuple.data(),
                                       static_cast<uint32_t>(tuple.size()),
                                       &page_id,
                                       &item_id,
                                       &ctx),
                  Status::OK) << ctx.message;
    }

    const auto pages_scanned = readTracePagesScanned(trace_path, table_name);
    ASSERT_GE(pages_scanned.size(), 3u);
    EXPECT_GT(pages_scanned.front(), 1u);
    EXPECT_LT(pages_scanned[1], pages_scanned.front());
    EXPECT_LE(pages_scanned[1], 2u);
    EXPECT_LE(pages_scanned[2], 2u);

    std::filesystem::remove(trace_path);
}

TEST_F(StorageEngineTest, InsertTraceCachesUniquePreflightMetadataAfterFirstInsert)
{
    const std::string trace_path =
        scratchbird::testing::uniqueTestShortPath("storage_engine_unique_preflight_trace", ".log");
    std::filesystem::remove(trace_path);
    ScopedEnvVar trace_enabled("SCRATCHBIRD_INSERT_TRACE", "1");
    ScopedEnvVar trace_file("SCRATCHBIRD_INSERT_TRACE_FILE", trace_path);

    const std::string table_name = "trace_unique_cache_test";
    const ID table_id = createSingleIntTable(table_name);
    createSingleIntIndex(table_id, "uq_trace_unique_cache_test", true);

    ErrorContext ctx;
    for (int32_t value = 1; value <= 3; ++value)
    {
        auto tuple = buildIntTuple(value);
        uint32_t page_id = 0;
        uint16_t item_id = 0;
        ASSERT_EQ(engine_->insertTuple(table_id,
                                       tuple.data(),
                                       static_cast<uint32_t>(tuple.size()),
                                       &page_id,
                                       &item_id,
                                       &ctx),
                  Status::OK) << ctx.message;
    }

    const auto cache_hits =
        readTraceFieldValues(trace_path, table_name, "preflight_cache_hit");
    const auto index_counts =
        readTraceFieldValues(trace_path, table_name, "preflight_index_count");
    ASSERT_GE(cache_hits.size(), 3u);
    ASSERT_GE(index_counts.size(), 3u);
    EXPECT_EQ(cache_hits[0], "0");
    EXPECT_EQ(cache_hits[1], "1");
    EXPECT_EQ(cache_hits[2], "1");
    EXPECT_EQ(index_counts[0], "1");
    EXPECT_EQ(index_counts[1], "1");
    EXPECT_EQ(index_counts[2], "1");

    std::filesystem::remove(trace_path);
}

TEST_F(StorageEngineTest, InsertTraceAdvancesPastFullPagesWithoutRestartingHeapScan)
{
    const std::string trace_path =
        scratchbird::testing::uniqueTestShortPath("storage_engine_page_full_trace", ".log");
    std::filesystem::remove(trace_path);
    ScopedEnvVar trace_enabled("SCRATCHBIRD_INSERT_TRACE", "1");
    ScopedEnvVar trace_file("SCRATCHBIRD_INSERT_TRACE_FILE", trace_path);

    const std::string table_name = "trace_page_full_resume_test";
    const ID table_id = createTestTable(table_name);

    struct LargeData
    {
        int32_t id;
        char payload[200];
    };

    std::unordered_set<uint32_t> visited_pages;
    ErrorContext ctx;
    for (int32_t value = 0; value < 500; ++value)
    {
        LargeData data{};
        data.id = value;
        std::memset(data.payload, 'A' + (value % 26), sizeof(data.payload));

        auto tuple = buildTuple(&data, sizeof(LargeData));
        uint32_t page_id = 0;
        uint16_t item_id = 0;
        ASSERT_EQ(engine_->insertTuple(table_id,
                                       tuple.data(),
                                       static_cast<uint32_t>(tuple.size()),
                                       &page_id,
                                       &item_id,
                                       &ctx),
                  Status::OK) << ctx.message;
        visited_pages.insert(page_id);
    }

    ASSERT_GT(visited_pages.size(), 1u);

    const auto pages_scanned = readTracePagesScanned(trace_path, table_name);
    ASSERT_GE(pages_scanned.size(), 500u);

    const size_t tail_window = 32;
    uint32_t tail_max = 0;
    for (size_t i = pages_scanned.size() - tail_window; i < pages_scanned.size(); ++i)
    {
        tail_max = std::max(tail_max, pages_scanned[i]);
    }

    EXPECT_LE(tail_max, 4u);

    std::filesystem::remove(trace_path);
}

TEST_F(StorageEngineTest, SequentialUniqueInsertMaintainsIndexAndHeapAcrossBatchedCommits)
{
    ErrorContext ctx;
    ID table_id = createSingleIntTable("batched_unique_insert_regression");
    ID index_id = createSingleIntIndex(table_id, "uq_batched_unique_insert_regression", true);

    CatalogManager::IndexInfo initial_index_info{};
    ASSERT_EQ(db_->catalog_manager()->getIndex(index_id, initial_index_info, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(initial_index_info.collation_id, 100u);

    constexpr int32_t kTotalRows = 50000;
    constexpr int32_t kBatchSize = 4096;

    std::vector<std::pair<int32_t, TID>> checkpoints;
    checkpoints.reserve(16);

    for (int32_t value = 1; value <= kTotalRows; ++value)
    {
        auto tuple = buildIntTuple(value);
        uint32_t page_id = 0;
        uint16_t item_id = 0;
        ASSERT_EQ(engine_->insertTuple(table_id,
                                       tuple.data(),
                                       static_cast<uint32_t>(tuple.size()),
                                       &page_id,
                                       &item_id,
                                       &ctx),
                  Status::OK)
            << "insert value=" << value << " failed: " << ctx.message;

        if (value == 1 || value == kTotalRows || (value % 5000) == 0)
        {
            checkpoints.emplace_back(
                value,
                TID(makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(page_id)), item_id));
        }

        if ((value % kBatchSize) == 0)
        {
            ASSERT_EQ(conn_ctx_->commit(&ctx), Status::OK) << ctx.message;

            for (const auto &[checkpoint_value, checkpoint_tid] : checkpoints)
            {
                SCOPED_TRACE(::testing::Message()
                             << "checkpoint_value=" << checkpoint_value
                             << " after_commit_value=" << value);
                expectIndexSeekFindsTid(index_id, checkpoint_value, checkpoint_tid);
            }
        }
    }

    ASSERT_EQ(conn_ctx_->commit(&ctx), Status::OK) << ctx.message;

    CatalogManager::IndexInfo final_index_info{};
    ASSERT_EQ(db_->catalog_manager()->getIndex(index_id, final_index_info, &ctx), Status::OK)
        << ctx.message;
    EXPECT_NE(final_index_info.root_gpid, initial_index_info.root_gpid);

    for (const auto &[checkpoint_value, checkpoint_tid] : checkpoints)
    {
        SCOPED_TRACE(::testing::Message()
                     << "final_checkpoint_value=" << checkpoint_value);
        expectIndexSeekFindsTid(index_id, checkpoint_value, checkpoint_tid);
    }

    const auto pre_reopen_cached_root = cachedBTreeRootPage(index_id);
    const auto pre_reopen_path = traceBTreePath(index_id, 1);

    reopenDatabase();

    CatalogManager::IndexInfo reopened_index_info{};
    ASSERT_EQ(db_->catalog_manager()->getIndex(index_id, reopened_index_info, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(reopened_index_info.root_gpid, final_index_info.root_gpid);
    EXPECT_NE(reopened_index_info.root_gpid, initial_index_info.root_gpid);
    const auto reopened_cached_root = cachedBTreeRootPage(index_id);

    for (const auto &[checkpoint_value, checkpoint_tid] : checkpoints)
    {
        SCOPED_TRACE(::testing::Message()
                     << "reopened_checkpoint_value=" << checkpoint_value);
        const auto reopened_path = traceBTreePath(index_id, checkpoint_value);
        const auto reopened_leaf_location = findKeyInLeafChain(index_id, checkpoint_value);
        const auto reopened_reachable_leaves = collectReachableLeafPagesFromRoot(index_id);
        const auto raw_tids_unfiltered = rawBTreeSearch(index_id, checkpoint_value, 0);
        const auto raw_tids = rawBTreeSearch(index_id,
                                             checkpoint_value,
                                             engine_->getCurrentXid());
        ASSERT_FALSE(raw_tids_unfiltered.empty())
            << "raw B-tree search lost checkpoint key after reopen even without visibility filtering"
            << " pre_path_steps=" << pre_reopen_path.size()
            << " reopened_path_steps=" << reopened_path.size()
            << " pre_root_page=" << (pre_reopen_path.empty() ? 0 : pre_reopen_path.front().page_num)
            << " pre_cached_root_page=" << pre_reopen_cached_root
            << " pre_root_count=" << (pre_reopen_path.empty() ? 0 : pre_reopen_path.front().count)
            << " pre_root_first_key=" << (pre_reopen_path.empty() ? 0 : pre_reopen_path.front().first_key)
            << " pre_root_last_key=" << (pre_reopen_path.empty() ? 0 : pre_reopen_path.front().last_key)
            << " pre_leaf_page=" << (pre_reopen_path.empty() ? 0 : pre_reopen_path.back().page_num)
            << " pre_leaf_first_key=" << (pre_reopen_path.empty() ? 0 : pre_reopen_path.back().first_key)
            << " pre_leaf_last_key=" << (pre_reopen_path.empty() ? 0 : pre_reopen_path.back().last_key)
            << " reopened_root_page=" << (reopened_path.empty() ? 0 : reopened_path.front().page_num)
            << " reopened_cached_root_page=" << reopened_cached_root
            << " reopened_root_count=" << (reopened_path.empty() ? 0 : reopened_path.front().count)
            << " reopened_root_first_key=" << (reopened_path.empty() ? 0 : reopened_path.front().first_key)
            << " reopened_root_last_key=" << (reopened_path.empty() ? 0 : reopened_path.front().last_key)
            << " reopened_leaf_page=" << (reopened_path.empty() ? 0 : reopened_path.back().page_num)
            << " reopened_leaf_level=" << (reopened_path.empty() ? 0 : reopened_path.back().level)
            << " reopened_leaf_count=" << (reopened_path.empty() ? 0 : reopened_path.back().count)
            << " reopened_leaf_first_key=" << (reopened_path.empty() ? 0 : reopened_path.back().first_key)
            << " reopened_leaf_last_key=" << (reopened_path.empty() ? 0 : reopened_path.back().last_key)
            << " reopened_first_branch_child="
            << ((reopened_path.size() > 0) ? reopened_path.front().chosen_child : 0)
            << " reopened_leaf_chain_found=" << reopened_leaf_location.found
            << " reopened_leaf_chain_page=" << reopened_leaf_location.page_num
            << " reopened_leaf_chain_parent=" << reopened_leaf_location.parent_page
            << " reopened_leaf_chain_parent_refs_child="
            << (reopened_leaf_location.parent_page != 0 &&
                pageDirectlyReferencesChild(index_id,
                                           reopened_leaf_location.parent_page,
                                           reopened_leaf_location.page_num))
            << " reopened_leaf_chain_slot=" << reopened_leaf_location.slot_index
            << " reopened_leaf_chain_first_key=" << reopened_leaf_location.first_key
            << " reopened_leaf_chain_last_key=" << reopened_leaf_location.last_key
            << " reopened_leaf_chain_count=" << reopened_leaf_location.key_count
            << " reopened_leaf_chain_total=" << reopened_leaf_location.leaf_count
            << " reopened_leaf_chain_tids=" << reopened_leaf_location.matching_tids.size()
            << " reopened_leaf_chain_first_tid_page="
            << (reopened_leaf_location.matching_tids.empty()
                    ? 0
                    : getPageNumber(reopened_leaf_location.matching_tids.front().gpid))
            << " reopened_leaf_chain_first_tid_slot="
            << (reopened_leaf_location.matching_tids.empty()
                    ? 0
                    : reopened_leaf_location.matching_tids.front().slot)
            << " reopened_leaf_chain_cycle=" << reopened_leaf_location.cycle_detected
            << " reopened_parent_child_window="
            << (reopened_leaf_location.parent_page != 0
                    ? describeParentChildWindow(index_id,
                                                reopened_leaf_location.parent_page,
                                                reopened_leaf_location.page_num)
                    : std::string("no_parent"))
            << " reopened_leaf_chain_reachable="
            << (reopened_leaf_location.page_num != 0 &&
                reopened_reachable_leaves.find(reopened_leaf_location.page_num) !=
                    reopened_reachable_leaves.end());
        ASSERT_FALSE(raw_tids.empty()) << "raw B-tree search lost checkpoint key after reopen";
        EXPECT_EQ(raw_tids.front(), raw_tids_unfiltered.front());
        EXPECT_EQ(raw_tids.front(), checkpoint_tid);

        Tuple reopened_tuple{};
        ErrorContext tuple_ctx;
        ASSERT_EQ(engine_->getTuple(table_id, raw_tids.front(), &reopened_tuple, &tuple_ctx), Status::OK)
            << "heap tuple fetch failed after reopen: " << tuple_ctx.message;

        expectIndexSeekFindsTid(index_id, checkpoint_value, checkpoint_tid);
    }

    auto scanner = engine_->createScan(table_id, &ctx);
    ASSERT_NE(scanner, nullptr) << ctx.message;

    Tuple tuple{};
    int32_t visible_rows = 0;
    while (scanner->next(&tuple, &ctx) == Status::OK)
    {
        ASSERT_GE(tuple.data_size, sizeof(TupleHeader) + sizeof(int32_t));
        int32_t payload_value = 0;
        std::memcpy(&payload_value, tuple.data + sizeof(TupleHeader), sizeof(payload_value));
        EXPECT_EQ(payload_value, visible_rows + 1)
            << "heap scan mismatch at ordinal=" << (visible_rows + 1);
        ++visible_rows;
    }

    EXPECT_EQ(visible_rows, kTotalRows);
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

TEST_F(StorageEngineTest, DeleteTupleRetiresToastedValueThroughLifecycleTruth)
{
    ID table_id = createTestTable("delete_toast_lifecycle_truth");

    auto original_tuple = buildLargeRowTuple(5, 'K');

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

    ID toast_value = readRawToastValueId(table_id, page_id, item_id);
    ASSERT_NE(toast_value, ID{});

    ASSERT_EQ(conn_ctx_->commit(&ctx), Status::OK) << ctx.message;

    const uint64_t delete_xid = conn_ctx_->getCurrentXid();
    ASSERT_NE(delete_xid, 0u);
    ASSERT_EQ(engine_->deleteTuple(table_id, page_id, item_id, UINT16_MAX, &ctx), Status::OK)
        << ctx.message;

    uint64_t chunk_xmax = 0;
    ASSERT_EQ(readToastChunkXmaxForValue(table_id, toast_value, &chunk_xmax), Status::OK);
    EXPECT_EQ(chunk_xmax, delete_xid);
}

TEST_F(StorageEngineTest, SavepointDeleteDefersToastedValueRetirement)
{
    ID table_id = createTestTable("savepoint_delete_toast_defer");

    auto original_tuple = buildLargeRowTuple(6, 'L');

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

    ID toast_value = readRawToastValueId(table_id, page_id, item_id);
    ASSERT_NE(toast_value, ID{});

    ASSERT_EQ(conn_ctx_->createSavepoint("sp_delete_toast_defer", &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(engine_->deleteTuple(table_id, page_id, item_id, UINT16_MAX, &ctx), Status::OK)
        << ctx.message;

    uint64_t chunk_xmax = UINT64_MAX;
    ASSERT_EQ(readToastChunkXmaxForValue(table_id, toast_value, &chunk_xmax), Status::OK);
    EXPECT_EQ(chunk_xmax, 0u);

    ASSERT_EQ(conn_ctx_->rollbackToSavepoint("sp_delete_toast_defer", &ctx), Status::OK)
        << ctx.message;

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

TEST_F(StorageEngineTest, ToastOrphanDetectionKeepsBackVersionReference)
{
    ID table_id = createTestTable("toast_orphan_back_version");

    auto original_tuple = buildLargeRowTuple(9, 'M');
    auto updated_tuple = buildLargeRowTuple(9, 'N');

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

    ID original_toast_value = readRawToastValueId(table_id, page_id, item_id);
    ASSERT_NE(original_toast_value, ID{});

    ASSERT_EQ(conn_ctx_->commit(&ctx), Status::OK) << ctx.message;

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
    ASSERT_EQ(conn_ctx_->commit(&ctx), Status::OK) << ctx.message;

    ToastManager toast_mgr(db_.get(), table_id);
    ASSERT_EQ(toast_mgr.initialize(&ctx), Status::OK) << ctx.message;
    ASSERT_NE(toast_mgr.toastTableId(), ID{});

    std::unordered_set<ID, IDHash> orphaned_value_ids;
    ASSERT_EQ(db_->garbage_collector()->detectOrphanedToastChunks(toast_mgr.toastTableId(),
                                                                  &orphaned_value_ids,
                                                                  &ctx),
              Status::OK) << ctx.message;
    EXPECT_EQ(orphaned_value_ids.count(original_toast_value), 0u);
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

TEST_F(StorageEngineTest, SavepointRollbackRetiresRolledBackToastValue)
{
    ID table_id = createTestTable("savepoint_toast_cleanup_test");

    auto original_tuple = buildLargeRowTuple(3, 'C');
    auto updated_tuple = buildLargeRowTuple(3, 'D');

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

    ID original_toast_value = readRawToastValueId(table_id, page_id, item_id);
    ASSERT_NE(original_toast_value, ID{});

    ASSERT_EQ(conn_ctx_->createSavepoint("sp_toast_cleanup", &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(engine_->updateTuple(table_id,
                                   page_id,
                                   item_id,
                                   updated_tuple.data(),
                                   static_cast<uint32_t>(updated_tuple.size()),
                                   nullptr,
                                   nullptr,
                                   &ctx),
              Status::OK) << ctx.message;

    ID updated_toast_value = readRawToastValueId(table_id, page_id, item_id);
    ASSERT_NE(updated_toast_value, ID{});
    EXPECT_NE(updated_toast_value, original_toast_value);

    ASSERT_EQ(conn_ctx_->rollbackToSavepoint("sp_toast_cleanup", &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(conn_ctx_->commit(&ctx), Status::OK) << ctx.message;

    ID restored_toast_value = readRawToastValueId(table_id, page_id, item_id);
    EXPECT_EQ(restored_toast_value, original_toast_value);

    std::unordered_set<ID, IDHash> orphaned_value_ids;
    ASSERT_EQ(detectOrphanedToastValues(table_id, &orphaned_value_ids), Status::OK);
    EXPECT_EQ(orphaned_value_ids.count(original_toast_value), 0u);
    EXPECT_EQ(orphaned_value_ids.count(updated_toast_value), 0u);
}

TEST_F(StorageEngineTest, CrossPageSavepointRollbackRestoresIndexedKeyVisibility)
{
    ErrorContext ctx;
    ID table_id = createTestTable("cross_page_savepoint_index_restore");
    ID index_id = createSingleIntIndex(table_id, "idx_cross_page_savepoint_restore", false);

    auto original_tuple = buildIndexedNullTextTuple(10);
    uint32_t page_id = 0;
    uint16_t item_id = 0;
    ASSERT_EQ(engine_->insertTuple(table_id,
                                   original_tuple.data(),
                                   static_cast<uint32_t>(original_tuple.size()),
                                   &page_id,
                                   &item_id,
                                   &ctx),
              Status::OK) << ctx.message;

    auto* page_mgr = db_->page_manager();
    ASSERT_NE(page_mgr, nullptr);
    const uint32_t extent_end = ((page_id / 32u) + 1u) * 32u;
    if (page_mgr->totalPages() < extent_end)
    {
        ASSERT_EQ(page_mgr->extendFile(extent_end - page_mgr->totalPages(), &ctx), Status::OK)
            << ctx.message;
    }

    TID stable_tid(makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(page_id)), item_id);
    fillPageAlmostFull(page_id, 128, 96);

    ASSERT_EQ(conn_ctx_->createSavepoint("sp_cross_page_index_restore", &ctx), Status::OK)
        << ctx.message;

    auto updated_tuple = buildIndexedTextTuple(20, 'Z', 5000);
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

    Tuple current_tuple{};
    ASSERT_EQ(engine_->getTuple(page_id, item_id, &current_tuple, &ctx), Status::OK) << ctx.message;
    const auto *current_hdr = reinterpret_cast<const TupleHeader *>(current_tuple.data);
    ASSERT_NE(current_hdr, nullptr);
    ASSERT_TRUE(current_hdr->hasBackVersion());
    EXPECT_NE(current_hdr->getBackVersionTID().gpid, stable_tid.gpid);

    expectIndexSeekFindsTid(index_id, 20, stable_tid);

    ASSERT_EQ(conn_ctx_->rollbackToSavepoint("sp_cross_page_index_restore", &ctx), Status::OK)
        << ctx.message;

    db_->garbage_collector()->markPageDirty(page_id);
    db_->garbage_collector()->processPageCooperative(page_id, &ctx);

    expectIndexSeekFindsTid(index_id, 10, stable_tid);
    expectIndexSeekNotFound(index_id, 20);
}

TEST_F(StorageEngineTest, SavepointRollbackRemovesRowInsertedAndUpdatedWithinSavepoint)
{
    ErrorContext ctx;
    ID table_id = createSingleIntTable("savepoint_insert_update_remove");
    ID index_id = createSingleIntIndex(table_id, "idx_savepoint_insert_update_remove", false);

    ASSERT_EQ(conn_ctx_->createSavepoint("sp_insert_remove", &ctx), Status::OK) << ctx.message;

    auto inserted_tuple = buildIntTuple(70);
    uint32_t page_id = 0;
    uint16_t item_id = 0;
    ASSERT_EQ(engine_->insertTuple(table_id,
                                   inserted_tuple.data(),
                                   static_cast<uint32_t>(inserted_tuple.size()),
                                   &page_id,
                                   &item_id,
                                   &ctx),
              Status::OK) << ctx.message;

    TID stable_tid(makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(page_id)), item_id);
    expectIndexSeekFindsTid(index_id, 70, stable_tid);

    auto updated_tuple = buildIntTuple(80, conn_ctx_->getCurrentXid());
    ASSERT_EQ(engine_->updateTuple(table_id,
                                   page_id,
                                   item_id,
                                   updated_tuple.data(),
                                   static_cast<uint32_t>(updated_tuple.size()),
                                   nullptr,
                                   nullptr,
                                   &ctx),
              Status::OK) << ctx.message;
    expectIndexSeekFindsTid(index_id, 80, stable_tid);

    ASSERT_EQ(conn_ctx_->rollbackToSavepoint("sp_insert_remove", &ctx), Status::OK) << ctx.message;

    auto scanner = engine_->createScan(table_id, &ctx);
    ASSERT_NE(scanner, nullptr);
    Tuple tuple{};
    size_t row_count = 0;
    while (scanner->next(&tuple, &ctx) == Status::OK)
    {
        ++row_count;
    }
    EXPECT_EQ(row_count, 0u);

    expectIndexSeekNotFound(index_id, 70);
    expectIndexSeekNotFound(index_id, 80);
}

TEST_F(StorageEngineTest, ReleasedNestedSavepointPreservesEarliestRestoreImage)
{
    ErrorContext ctx;
    ID table_id = createSingleIntTable("savepoint_nested_restore");
    ID index_id = createSingleIntIndex(table_id, "idx_savepoint_nested_restore", false);

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
    ASSERT_EQ(conn_ctx_->createSavepoint("sp_outer_restore", &ctx), Status::OK) << ctx.message;

    auto first_update = buildIntTuple(20, conn_ctx_->getCurrentXid());
    ASSERT_EQ(engine_->updateTuple(table_id,
                                   page_id,
                                   item_id,
                                   first_update.data(),
                                   static_cast<uint32_t>(first_update.size()),
                                   nullptr,
                                   nullptr,
                                   &ctx),
              Status::OK) << ctx.message;
    expectIndexSeekFindsTid(index_id, 20, stable_tid);

    ASSERT_EQ(conn_ctx_->createSavepoint("sp_inner_restore", &ctx), Status::OK) << ctx.message;

    auto second_update = buildIntTuple(30, conn_ctx_->getCurrentXid());
    ASSERT_EQ(engine_->updateTuple(table_id,
                                   page_id,
                                   item_id,
                                   second_update.data(),
                                   static_cast<uint32_t>(second_update.size()),
                                   nullptr,
                                   nullptr,
                                   &ctx),
              Status::OK) << ctx.message;
    expectIndexSeekFindsTid(index_id, 30, stable_tid);

    ASSERT_EQ(conn_ctx_->releaseSavepoint("sp_inner_restore", &ctx), Status::OK) << ctx.message;

    auto third_update = buildIntTuple(40, conn_ctx_->getCurrentXid());
    ASSERT_EQ(engine_->updateTuple(table_id,
                                   page_id,
                                   item_id,
                                   third_update.data(),
                                   static_cast<uint32_t>(third_update.size()),
                                   nullptr,
                                   nullptr,
                                   &ctx),
              Status::OK) << ctx.message;
    expectIndexSeekFindsTid(index_id, 40, stable_tid);

    ASSERT_EQ(conn_ctx_->rollbackToSavepoint("sp_outer_restore", &ctx), Status::OK)
        << ctx.message;

    Tuple restored{};
    ASSERT_EQ(engine_->getTuple(page_id, item_id, &restored, &ctx), Status::OK) << ctx.message;
    const auto *restored_hdr = reinterpret_cast<const TupleHeader *>(restored.data);
    ASSERT_NE(restored_hdr, nullptr);
    EXPECT_FALSE(restored_hdr->hasBackVersion());

    const auto *payload = restored.data + sizeof(TupleHeader);
    EXPECT_EQ(*reinterpret_cast<const int32_t *>(payload), 10);
    expectIndexSeekFindsTid(index_id, 10, stable_tid);
    expectIndexSeekNotFound(index_id, 20);
    expectIndexSeekNotFound(index_id, 30);
    expectIndexSeekNotFound(index_id, 40);
}

TEST_F(StorageEngineTest, ShadowedSavepointNameResolvesToMostRecentFrame)
{
    ErrorContext ctx;
    ID table_id = createSingleIntTable("savepoint_shadowed_name_restore");
    ID index_id = createSingleIntIndex(table_id, "idx_savepoint_shadowed_name_restore", false);

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

    ASSERT_EQ(conn_ctx_->createSavepoint("sp_shadow", &ctx), Status::OK) << ctx.message;

    auto first_update = buildIntTuple(20, conn_ctx_->getCurrentXid());
    ASSERT_EQ(engine_->updateTuple(table_id,
                                   page_id,
                                   item_id,
                                   first_update.data(),
                                   static_cast<uint32_t>(first_update.size()),
                                   nullptr,
                                   nullptr,
                                   &ctx),
              Status::OK) << ctx.message;
    expectIndexSeekFindsTid(index_id, 20, stable_tid);

    ASSERT_EQ(conn_ctx_->createSavepoint("sp_shadow", &ctx), Status::OK) << ctx.message;

    auto second_update = buildIntTuple(30, conn_ctx_->getCurrentXid());
    ASSERT_EQ(engine_->updateTuple(table_id,
                                   page_id,
                                   item_id,
                                   second_update.data(),
                                   static_cast<uint32_t>(second_update.size()),
                                   nullptr,
                                   nullptr,
                                   &ctx),
              Status::OK) << ctx.message;
    expectIndexSeekFindsTid(index_id, 30, stable_tid);

    ASSERT_EQ(conn_ctx_->rollbackToSavepoint("sp_shadow", &ctx), Status::OK) << ctx.message;

    Tuple restored{};
    ASSERT_EQ(engine_->getTuple(page_id, item_id, &restored, &ctx), Status::OK) << ctx.message;
    const auto *payload = restored.data + sizeof(TupleHeader);
    EXPECT_EQ(*reinterpret_cast<const int32_t *>(payload), 20);
    expectIndexSeekFindsTid(index_id, 20, stable_tid);
    expectIndexSeekNotFound(index_id, 30);
}

TEST_F(StorageEngineTest, ReleasingInteriorSavepointKeepsYoungerFrameActive)
{
    ErrorContext ctx;
    ID table_id = createSingleIntTable("savepoint_release_interior_keeps_inner");
    ID index_id = createSingleIntIndex(table_id, "idx_savepoint_release_interior_keeps_inner", false);

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

    ASSERT_EQ(conn_ctx_->createSavepoint("sp_outer", &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(conn_ctx_->createSavepoint("sp_middle", &ctx), Status::OK) << ctx.message;

    auto middle_update = buildIntTuple(20, conn_ctx_->getCurrentXid());
    ASSERT_EQ(engine_->updateTuple(table_id,
                                   page_id,
                                   item_id,
                                   middle_update.data(),
                                   static_cast<uint32_t>(middle_update.size()),
                                   nullptr,
                                   nullptr,
                                   &ctx),
              Status::OK) << ctx.message;
    expectIndexSeekFindsTid(index_id, 20, stable_tid);

    ASSERT_EQ(conn_ctx_->createSavepoint("sp_inner", &ctx), Status::OK) << ctx.message;

    auto inner_update = buildIntTuple(30, conn_ctx_->getCurrentXid());
    ASSERT_EQ(engine_->updateTuple(table_id,
                                   page_id,
                                   item_id,
                                   inner_update.data(),
                                   static_cast<uint32_t>(inner_update.size()),
                                   nullptr,
                                   nullptr,
                                   &ctx),
              Status::OK) << ctx.message;
    expectIndexSeekFindsTid(index_id, 30, stable_tid);

    ASSERT_EQ(conn_ctx_->releaseSavepoint("sp_middle", &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(conn_ctx_->rollbackToSavepoint("sp_inner", &ctx), Status::OK) << ctx.message;

    Tuple restored{};
    ASSERT_EQ(engine_->getTuple(page_id, item_id, &restored, &ctx), Status::OK) << ctx.message;
    const auto *payload = restored.data + sizeof(TupleHeader);
    EXPECT_EQ(*reinterpret_cast<const int32_t *>(payload), 20);
    expectIndexSeekFindsTid(index_id, 20, stable_tid);
    expectIndexSeekNotFound(index_id, 30);
}

TEST_F(StorageEngineTest, OuterRollbackAfterInteriorReleaseUsesEarliestRestoreImage)
{
    ErrorContext ctx;
    ID table_id = createSingleIntTable("savepoint_release_interior_outer_restore");
    ID index_id = createSingleIntIndex(table_id, "idx_savepoint_release_interior_outer_restore", false);

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

    ASSERT_EQ(conn_ctx_->createSavepoint("sp_outer", &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(conn_ctx_->createSavepoint("sp_middle", &ctx), Status::OK) << ctx.message;

    auto middle_update = buildIntTuple(20, conn_ctx_->getCurrentXid());
    ASSERT_EQ(engine_->updateTuple(table_id,
                                   page_id,
                                   item_id,
                                   middle_update.data(),
                                   static_cast<uint32_t>(middle_update.size()),
                                   nullptr,
                                   nullptr,
                                   &ctx),
              Status::OK) << ctx.message;
    expectIndexSeekFindsTid(index_id, 20, stable_tid);

    ASSERT_EQ(conn_ctx_->createSavepoint("sp_inner", &ctx), Status::OK) << ctx.message;

    auto inner_update = buildIntTuple(30, conn_ctx_->getCurrentXid());
    ASSERT_EQ(engine_->updateTuple(table_id,
                                   page_id,
                                   item_id,
                                   inner_update.data(),
                                   static_cast<uint32_t>(inner_update.size()),
                                   nullptr,
                                   nullptr,
                                   &ctx),
              Status::OK) << ctx.message;
    expectIndexSeekFindsTid(index_id, 30, stable_tid);

    ASSERT_EQ(conn_ctx_->releaseSavepoint("sp_middle", &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(conn_ctx_->rollbackToSavepoint("sp_outer", &ctx), Status::OK) << ctx.message;

    Tuple restored{};
    ASSERT_EQ(engine_->getTuple(page_id, item_id, &restored, &ctx), Status::OK) << ctx.message;
    const auto *payload = restored.data + sizeof(TupleHeader);
    EXPECT_EQ(*reinterpret_cast<const int32_t *>(payload), 10);
    expectIndexSeekFindsTid(index_id, 10, stable_tid);
    expectIndexSeekNotFound(index_id, 20);
    expectIndexSeekNotFound(index_id, 30);
}

TEST_F(StorageEngineTest, GroupCommitQueuesOnlineMaintenanceDeltaUntilCommit)
{
    ErrorContext ctx;
    db_->transaction_manager()->setDurabilityMode(DurabilityMode::GROUP_COMMIT);
    db_->transaction_manager()->setGroupCommitTimeout(50000);

    const ID table_id = createSingleIntTable("group_commit_online_delta_commit");
    const ID index_id =
        createSingleIntIndex(table_id, "idx_group_commit_online_delta_commit", false);
    const ID maintenance_id = createOnlineMaintenanceForIndex(index_id);
    const auto index_info = getIndexInfoById(index_id);

    auto before_stats = engine_->getCommitGroupMaintenanceStats();

    const auto tuple = buildIntTuple(42, conn_ctx_->getCurrentXid());
    uint32_t page_id = 0;
    uint16_t item_id = 0;
    ASSERT_EQ(engine_->insertTuple(table_id,
                                   tuple.data(),
                                   static_cast<uint32_t>(tuple.size()),
                                   &page_id,
                                   &item_id,
                                   &ctx),
              Status::OK)
        << ctx.message;

    std::vector<CatalogManager::IndexMaintenanceDeltaCatalogInfo> deltas;
    ASSERT_EQ(db_->catalog_manager()->listIndexMaintenanceDeltaCatalogEntries(
                  maintenance_id,
                  deltas,
                  &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_TRUE(deltas.empty());

    ASSERT_EQ(conn_ctx_->commit(&ctx), Status::OK) << ctx.message;

    deltas.clear();
    ASSERT_EQ(db_->catalog_manager()->listIndexMaintenanceDeltaCatalogEntries(
                  maintenance_id,
                  deltas,
                  &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(deltas.size(), 1u);
    EXPECT_EQ(deltas.front().delta_op, CatalogManager::IndexDeltaOp::INSERT);
    EXPECT_GT(deltas.front().commit_txid, 0u);
    EXPECT_EQ(deltas.front().maintenance_id, maintenance_id);

    auto after_stats = engine_->getCommitGroupMaintenanceStats();
    EXPECT_GE(after_stats.batches_applied, before_stats.batches_applied + 1);
    EXPECT_GE(after_stats.transactions_applied, before_stats.transactions_applied + 1);
    EXPECT_GE(after_stats.deltas_applied, before_stats.deltas_applied + 1);
    EXPECT_GE(after_stats.locality_groups_applied,
              before_stats.locality_groups_applied + 1);
    EXPECT_EQ(after_stats.apply_failures, before_stats.apply_failures);

    CatalogManager::IndexMaintenanceCatalogInfo maintenance_row{};
    ASSERT_EQ(db_->catalog_manager()->getIndexMaintenanceCatalogEntry(
                  maintenance_id,
                  maintenance_row,
                  &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_EQ(maintenance_row.index_id, index_info.index_id);
}

TEST_F(StorageEngineTest, ColdExactSecondaryInsertDeltaMergesOnRead)
{
    ScopedEnvVar force_cold_delta("SCRATCHBIRD_FORCE_COLD_EXACT_DELTA_BUFFER", "1");

    ErrorContext ctx;
    const ID table_id = createSingleIntTable("cold_exact_delta_read_merge");
    const ID index_id =
        createSingleIntIndex(table_id, "idx_cold_exact_delta_read_merge", false);

    const auto tuple = buildIntTuple(10, conn_ctx_->getCurrentXid());
    uint32_t page_id = 0;
    uint16_t item_id = 0;
    ASSERT_EQ(engine_->insertTuple(table_id,
                                   tuple.data(),
                                   static_cast<uint32_t>(tuple.size()),
                                   &page_id,
                                   &item_id,
                                   &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(conn_ctx_->commit(&ctx), Status::OK) << ctx.message;

    std::vector<CatalogManager::IndexPageDeltaCatalogInfo> rows;
    ASSERT_EQ(db_->catalog_manager()->listIndexPageDeltaCatalogEntries(index_id, rows, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows.front().delta_op, CatalogManager::IndexPageDeltaOp::INSERT);
    EXPECT_EQ(rows.front().merge_state, CatalogManager::IndexPageDeltaMergeState::PENDING);

    const TID stable_tid(makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(page_id)), item_id);
    expectIndexSeekFindsTid(index_id, 10, stable_tid);

    rows.clear();
    ASSERT_EQ(db_->catalog_manager()->listIndexPageDeltaCatalogEntries(index_id, rows, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_TRUE(rows.empty());
}

TEST_F(StorageEngineTest, BackgroundGcMergesDeferredExactSecondaryDeltasWithoutForegroundRead)
{
    ScopedEnvVar force_cold_delta("SCRATCHBIRD_FORCE_COLD_EXACT_DELTA_BUFFER", "1");

    ErrorContext ctx;
    auto* gc = db_->garbage_collector();
    ASSERT_NE(gc, nullptr);

    const ID table_id = createSingleIntTable("cold_exact_delta_background_merge");
    const ID index_id =
        createSingleIntIndex(table_id, "idx_cold_exact_delta_background_merge", false);

    const auto tuple = buildIntTuple(12, conn_ctx_->getCurrentXid());
    uint32_t page_id = 0;
    uint16_t item_id = 0;
    ASSERT_EQ(engine_->insertTuple(table_id,
                                   tuple.data(),
                                   static_cast<uint32_t>(tuple.size()),
                                   &page_id,
                                   &item_id,
                                   &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(conn_ctx_->commit(&ctx), Status::OK) << ctx.message;

    std::vector<CatalogManager::IndexPageDeltaCatalogInfo> rows;
    ASSERT_EQ(db_->catalog_manager()->listIndexPageDeltaCatalogEntries(index_id, rows, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows.front().merge_state, CatalogManager::IndexPageDeltaMergeState::PENDING);

    ASSERT_EQ(gc->startBackgroundGC(&ctx), Status::OK) << ctx.message;

    bool merged_in_background = false;
    for (size_t attempt = 0; attempt < 80; ++attempt)
    {
        rows.clear();
        ASSERT_EQ(db_->catalog_manager()->listIndexPageDeltaCatalogEntries(index_id, rows, &ctx),
                  Status::OK)
            << ctx.message;
        if (rows.empty())
        {
            merged_in_background = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    ASSERT_EQ(gc->stopBackgroundGC(&ctx), Status::OK) << ctx.message;
    EXPECT_TRUE(merged_in_background);
    EXPECT_GT(gc->getStatistics().background_runs, 0u);

    const TID stable_tid(makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(page_id)), item_id);
    expectIndexSeekFindsTid(index_id, 12, stable_tid);
}

TEST_F(StorageEngineTest, ColdExactSecondaryDeferralSkipsUniqueIndexes)
{
    ScopedEnvVar force_cold_delta("SCRATCHBIRD_FORCE_COLD_EXACT_DELTA_BUFFER", "1");

    ErrorContext ctx;
    const ID table_id = createSingleIntTable("cold_exact_delta_unique_guard");
    const ID index_id =
        createSingleIntIndex(table_id, "uq_cold_exact_delta_unique_guard", true);

    const auto tuple = buildIntTuple(11, conn_ctx_->getCurrentXid());
    uint32_t page_id = 0;
    uint16_t item_id = 0;
    ASSERT_EQ(engine_->insertTuple(table_id,
                                   tuple.data(),
                                   static_cast<uint32_t>(tuple.size()),
                                   &page_id,
                                   &item_id,
                                   &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(conn_ctx_->commit(&ctx), Status::OK) << ctx.message;

    std::vector<CatalogManager::IndexPageDeltaCatalogInfo> rows;
    ASSERT_EQ(db_->catalog_manager()->listIndexPageDeltaCatalogEntries(index_id, rows, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_TRUE(rows.empty());

    const TID stable_tid(makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(page_id)), item_id);
    expectIndexSeekFindsTid(index_id, 11, stable_tid);
}

TEST_F(StorageEngineTest, GroupCommitRollbackDiscardsQueuedOnlineMaintenanceDelta)
{
    ErrorContext ctx;
    db_->transaction_manager()->setDurabilityMode(DurabilityMode::GROUP_COMMIT);
    db_->transaction_manager()->setGroupCommitTimeout(50000);

    const ID table_id = createSingleIntTable("group_commit_online_delta_rollback");
    const ID index_id =
        createSingleIntIndex(table_id, "idx_group_commit_online_delta_rollback", false);
    const ID maintenance_id = createOnlineMaintenanceForIndex(index_id);

    auto before_stats = engine_->getCommitGroupMaintenanceStats();

    const auto tuple = buildIntTuple(7, conn_ctx_->getCurrentXid());
    ASSERT_EQ(engine_->insertTuple(table_id,
                                   tuple.data(),
                                   static_cast<uint32_t>(tuple.size()),
                                   nullptr,
                                   nullptr,
                                   &ctx),
              Status::OK)
        << ctx.message;

    std::vector<CatalogManager::IndexMaintenanceDeltaCatalogInfo> deltas;
    ASSERT_EQ(db_->catalog_manager()->listIndexMaintenanceDeltaCatalogEntries(
                  maintenance_id,
                  deltas,
                  &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_TRUE(deltas.empty());

    ASSERT_EQ(conn_ctx_->rollback(&ctx), Status::OK) << ctx.message;

    deltas.clear();
    ASSERT_EQ(db_->catalog_manager()->listIndexMaintenanceDeltaCatalogEntries(
                  maintenance_id,
                  deltas,
                  &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_TRUE(deltas.empty());

    auto after_stats = engine_->getCommitGroupMaintenanceStats();
    EXPECT_EQ(after_stats.batches_applied, before_stats.batches_applied);
    EXPECT_EQ(after_stats.transactions_applied, before_stats.transactions_applied);
    EXPECT_EQ(after_stats.deltas_applied, before_stats.deltas_applied);
    EXPECT_EQ(after_stats.locality_groups_applied, before_stats.locality_groups_applied);
    EXPECT_EQ(after_stats.apply_failures, before_stats.apply_failures);
}

TEST_F(StorageEngineTest, BootstrapVisibilityFallbackIsTransactionManagerOwned)
{
    const uint64_t reader_xid = 100;

    const auto visible_row =
        TransactionManager::evaluateBootstrapRecordVisibility(90, 0, reader_xid);
    EXPECT_TRUE(visible_row.visible);
    EXPECT_TRUE(visible_row.create_visible);
    EXPECT_FALSE(visible_row.delete_visible);
    EXPECT_EQ(visible_row.create_decision.reason, VisibilityReason::COMMITTED_VISIBLE);
    EXPECT_EQ(visible_row.delete_decision.reason, VisibilityReason::DELETE_NOT_PRESENT);

    const auto future_delete =
        TransactionManager::evaluateBootstrapRecordVisibility(90, 150, reader_xid);
    EXPECT_TRUE(future_delete.visible);
    EXPECT_TRUE(future_delete.create_visible);
    EXPECT_FALSE(future_delete.delete_visible);
    EXPECT_EQ(future_delete.delete_decision.reason, VisibilityReason::FUTURE_XID);

    const auto invalid_create =
        TransactionManager::evaluateBootstrapRecordVisibility(0, 0, reader_xid);
    EXPECT_FALSE(invalid_create.visible);
    EXPECT_FALSE(invalid_create.create_visible);
    EXPECT_EQ(invalid_create.create_decision.reason, VisibilityReason::INVALID_XID);
}

TEST_F(StorageEngineTest, IsVisibleUsesAuthoritativeRuntimeSnapshotContext)
{
    ErrorContext ctx;
    const uint64_t xid_writer = conn_ctx_->getCurrentXid();

    std::unique_ptr<ConnectionContext> reader;
    ASSERT_EQ(db_->connect(reader, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(reader->initialize(&ctx), Status::OK) << ctx.message;

    ID system_user = db_->catalog_manager()->getSystemUserId(&ctx);
    reader->setCurrentUser(system_user, true);

    ASSERT_EQ(reader->startTransaction(false, IsolationLevel::SNAPSHOT, true, &ctx), Status::OK)
        << ctx.message;
    const uint64_t xid_reader = reader->getCurrentXid();
    ASSERT_LT(xid_writer, xid_reader);

    ASSERT_EQ(conn_ctx_->commit(&ctx), Status::OK) << ctx.message;

    ConnectionContext::setCurrent(reader.get());
    EXPECT_FALSE(engine_->isVisible(xid_writer, 0, xid_reader));

    ConnectionContext::setCurrent(conn_ctx_.get());
}
