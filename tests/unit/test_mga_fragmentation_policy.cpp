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
#include "scratchbird/core/config.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/gc_manager.h"
#include "scratchbird/core/gpid.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/storage_engine.h"

#include <cstring>
#include <filesystem>
#include <memory>
#include <optional>
#include <unistd.h>
#include <vector>

using namespace scratchbird::core;

class MgaFragmentationPolicyTest : public ::testing::Test
{
protected:
    std::string test_db_path_;
    Database db_;
    std::unique_ptr<ConnectionContext> connection_;
    ID table_id_ = generateUuidV7();

    void SetUp() override
    {
        Config::getInstance().set("garbage_collection", "enabled", "true");

        test_db_path_ =
            "/tmp/test_mga_fragmentation_policy_" + std::to_string(getpid()) + ".db";
        std::filesystem::remove(test_db_path_);

        ErrorContext ctx;
        ASSERT_EQ(Database::create(test_db_path_, 16384, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(db_.open(test_db_path_, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(db_.connect(connection_, &ctx), Status::OK) << ctx.message;
        ConnectionContext::setCurrent(connection_.get());

        ID system_user = db_.catalog_manager()->getSystemUserId(&ctx);
        if (system_user != ID{})
        {
            connection_->setCurrentUser(system_user, true);
        }

        // Advance into a stable always-active transaction so xmax < horizon is easy to model.
        ASSERT_EQ(connection_->commit(&ctx), Status::OK) << ctx.message;
        ASSERT_EQ(connection_->commit(&ctx), Status::OK) << ctx.message;
    }

    void TearDown() override
    {
        ConnectionContext::setCurrent(nullptr);
        connection_.reset();
        db_.close();
        std::filesystem::remove(test_db_path_);
    }

    auto allocatePage(uint32_t &page_id, uint8_t **page_data, ErrorContext *ctx) -> Status
    {
        auto *page_mgr = db_.page_manager();
        if (page_mgr == nullptr)
        {
            return Status::INVALID_ARGUMENT;
        }

        Status status = page_mgr->allocatePage(page_id, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        void *buffer = nullptr;
        status = db_.buffer_pool()->pinPage(page_id, &buffer, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        *page_data = static_cast<uint8_t *>(buffer);
        return Status::OK;
    }

    void releasePage(uint32_t page_id, bool dirty, ErrorContext *ctx)
    {
        db_.buffer_pool()->unpinPage(page_id, dirty, ctx);
        db_.page_manager()->freePage(page_id, ctx);
    }

    auto buildTuple(size_t payload_size, uint8_t fill_byte = 0x5A) -> std::vector<uint8_t>
    {
        std::vector<uint8_t> tuple(sizeof(TupleHeader) + payload_size, 0);
        TupleHeader header{};
        header.session_id = ID{};
        std::memcpy(tuple.data(), &header, sizeof(TupleHeader));
        std::memset(tuple.data() + sizeof(TupleHeader), fill_byte, payload_size);
        return tuple;
    }

    void setPageTableId(uint8_t *page_data)
    {
        auto *special = reinterpret_cast<HeapPageSpecial *>(
            page_data + db_.page_size() - sizeof(HeapPageSpecial));
        special->table_id = table_id_;
    }

    void markTupleDeadForGc(uint8_t *page_data, uint16_t item_id, uint64_t xmax)
    {
        auto *items = reinterpret_cast<ItemPointer *>(page_data + sizeof(PageHeader));
        auto *tuple_hdr = reinterpret_cast<TupleHeader *>(page_data + items[item_id].offset);
        tuple_hdr->xmax = xmax;
        tuple_hdr->infomask |= TupleHeader::HEAP_XMAX_COMMITTED;
        tuple_hdr->setRecordFlag(TupleHeader::RHD_DELETED, true);
    }

    auto deadXmax() const -> uint64_t
    {
        const uint64_t current_xid = connection_->getCurrentXid();
        return (current_xid > 1) ? (current_xid - 1) : 1;
    }

    ID resolveDefaultSchema(ErrorContext *ctx)
    {
        CatalogManager::SchemaInfo schema{};
        Status status = db_.catalog_manager()->getSchema("main", schema, ctx);
        if (status == Status::OK)
        {
            return schema.schema_id;
        }

        status = db_.catalog_manager()->getSchema("users.public", schema, ctx);
        if (status == Status::OK)
        {
            return schema.schema_id;
        }

        status = db_.catalog_manager()->getSchema("public", schema, ctx);
        if (status == Status::OK)
        {
            return schema.schema_id;
        }

        std::vector<CatalogManager::SchemaInfo> schemas;
        status = db_.catalog_manager()->listSchemas(schemas, ctx);
        if (status == Status::OK && !schemas.empty())
        {
            return schemas.front().schema_id;
        }

        ID schema_id;
        status = db_.catalog_manager()->createSchema("main", "SYSTEM", schema_id, ctx);
        EXPECT_EQ(status, Status::OK) << ctx->message;
        return schema_id;
    }

    ID createCatalogBackedTable(const std::string &name,
                                const std::string &column_name,
                                DataType data_type,
                                uint32_t type_precision = 0)
    {
        ErrorContext ctx;
        const ID schema_id = resolveDefaultSchema(&ctx);

        CatalogManager::ColumnInfo column{};
        column.column_name = column_name;
        column.ordinal = 1;
        column.data_type = static_cast<uint16_t>(data_type);
        column.type_precision = type_precision;
        column.nullable = false;

        ID table_id;
        Status status =
            db_.catalog_manager()->createTable(schema_id, name, {column}, table_id, 0, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        return table_id;
    }

    void createCatalogIndex(const ID &table_id,
                            const std::string &index_name,
                            const std::string &column_name,
                            CatalogManager::IndexType index_type)
    {
        ErrorContext ctx;
        ID index_id;
        Status status = db_.catalog_manager()->createIndex(
            table_id, index_name, {column_name}, index_id, false, index_type, 0, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
    }

    auto rootPageForTable(const ID &table_id) -> uint32_t
    {
        ErrorContext ctx;
        CatalogManager::TableInfo table_info{};
        Status status = db_.catalog_manager()->getTable(table_id, table_info, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        return static_cast<uint32_t>(getPageNumber(table_info.root_gpid));
    }

    void seedDeadTupleOnTableRoot(const ID &table_id, size_t payload_size = 256)
    {
        ErrorContext ctx;
        const uint32_t page_id = rootPageForTable(table_id);

        void *buffer = nullptr;
        ASSERT_EQ(db_.buffer_pool()->pinPage(page_id, &buffer, &ctx), Status::OK) << ctx.message;

        auto *page_data = static_cast<uint8_t *>(buffer);
        HeapPage heap(page_data, db_.page_size());
        auto tuple = buildTuple(payload_size, 0x3C);
        uint16_t slot_id = 0;
        ASSERT_EQ(heap.insertTuple(tuple.data(),
                                   static_cast<uint32_t>(tuple.size()),
                                   deadXmax(),
                                   &slot_id,
                                   &ctx),
                  Status::OK)
            << ctx.message;
        ASSERT_EQ(heap.deleteTuple(slot_id, deadXmax(), &ctx), Status::OK) << ctx.message;
        markTupleDeadForGc(page_data, slot_id, deadXmax());

        db_.buffer_pool()->unpinPage(page_id, true, &ctx);
    }

    auto findPublication(const std::string &index_name)
        -> std::optional<IndexCleanupPublicationRecord>
    {
        std::vector<IndexCleanupPublicationRecord> publications;
        EXPECT_EQ(db_.storage_engine()->listIndexCleanupPublications(publications), Status::OK);
        for (const auto &publication : publications)
        {
            if (publication.index_name == index_name)
            {
                return publication;
            }
        }
        return std::nullopt;
    }
};

TEST_F(MgaFragmentationPolicyTest, HeapFragmentationMetricsClassifyCompactThreshold)
{
    ErrorContext ctx;
    uint32_t page_id = 0;
    uint8_t *page_data = nullptr;
    ASSERT_EQ(allocatePage(page_id, &page_data, &ctx), Status::OK) << ctx.message;

    HeapPage heap(page_data, db_.page_size());
    ASSERT_EQ(heap.initialize(page_id, &ctx), Status::OK) << ctx.message;
    setPageTableId(page_data);

    const uint64_t xmin = deadXmax();
    auto tuple = buildTuple(4200, 0x11);
    uint16_t live_slot = 0;
    uint16_t deleted_slot = 0;
    ASSERT_EQ(heap.insertTuple(tuple.data(), static_cast<uint32_t>(tuple.size()), xmin, &live_slot,
                               &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(heap.insertTuple(tuple.data(), static_cast<uint32_t>(tuple.size()), xmin,
                               &deleted_slot, &ctx),
              Status::OK)
        << ctx.message;

    ASSERT_EQ(heap.deleteTuple(deleted_slot, UINT64_MAX, &ctx), Status::OK) << ctx.message;

    HeapPage::FragmentationMetrics metrics{};
    ASSERT_EQ(heap.analyzeFragmentation(&metrics, &ctx), Status::OK) << ctx.message;

    EXPECT_EQ(live_slot, 0);
    EXPECT_EQ(deleted_slot, 1);
    EXPECT_TRUE(metrics.warn_threshold);
    EXPECT_TRUE(metrics.compact_threshold);
    EXPECT_FALSE(metrics.rewrite_threshold);
    EXPECT_EQ(metrics.live_slots, 1);
    EXPECT_EQ(metrics.deleted_slots, 1);
    EXPECT_EQ(metrics.unused_slots, 0);
    EXPECT_GT(metrics.reclaimable_bytes, 0u);

    releasePage(page_id, true, &ctx);
}

TEST_F(MgaFragmentationPolicyTest, GcPageCompactionPreservesLiveSlotIdentity)
{
    ErrorContext ctx;
    uint32_t page_id = 0;
    uint8_t *page_data = nullptr;
    ASSERT_EQ(allocatePage(page_id, &page_data, &ctx), Status::OK) << ctx.message;

    HeapPage heap(page_data, db_.page_size());
    ASSERT_EQ(heap.initialize(page_id, &ctx), Status::OK) << ctx.message;
    setPageTableId(page_data);

    const uint64_t xmin = deadXmax();
    auto tuple = buildTuple(4200, 0x22);
    uint16_t slot0 = 0;
    uint16_t slot1 = 0;
    uint16_t slot2 = 0;
    ASSERT_EQ(heap.insertTuple(tuple.data(), static_cast<uint32_t>(tuple.size()), xmin, &slot0,
                               &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(heap.insertTuple(tuple.data(), static_cast<uint32_t>(tuple.size()), xmin, &slot1,
                               &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(heap.insertTuple(tuple.data(), static_cast<uint32_t>(tuple.size()), xmin, &slot2,
                               &ctx),
              Status::OK)
        << ctx.message;

    markTupleDeadForGc(page_data, slot1, deadXmax());
    db_.buffer_pool()->unpinPage(page_id, true, &ctx);

    GcStats stats{};
    ASSERT_EQ(db_.gc_manager()->gcPage(table_id_, page_id, &stats, &ctx), Status::OK)
        << ctx.message;

    void *buffer = nullptr;
    ASSERT_EQ(db_.buffer_pool()->pinPage(page_id, &buffer, &ctx), Status::OK) << ctx.message;
    page_data = static_cast<uint8_t *>(buffer);
    HeapPage compacted(page_data, db_.page_size());

    EXPECT_EQ(compacted.getItemCount(), 3);

    auto *items = reinterpret_cast<ItemPointer *>(page_data + sizeof(PageHeader));
    EXPECT_FALSE(items[slot0].isUnused());
    EXPECT_TRUE(items[slot1].isUnused());
    EXPECT_FALSE(items[slot2].isUnused());

    const uint8_t *tuple_data = nullptr;
    uint32_t tuple_size = 0;
    ASSERT_EQ(compacted.getTuple(slot0, &tuple_data, &tuple_size, &ctx), Status::OK)
        << ctx.message;
    auto *tuple_hdr = reinterpret_cast<const TupleHeader *>(tuple_data);
    EXPECT_EQ(tuple_hdr->ctid_slot, slot0);

    ASSERT_EQ(compacted.getTuple(slot2, &tuple_data, &tuple_size, &ctx), Status::OK)
        << ctx.message;
    tuple_hdr = reinterpret_cast<const TupleHeader *>(tuple_data);
    EXPECT_EQ(tuple_hdr->ctid_slot, slot2);

    EXPECT_EQ(stats.pages_compacted, 1u);
    EXPECT_EQ(stats.slot_stable_compactions, 1u);
    EXPECT_EQ(stats.rewrite_recommendations, 0u);

    StorageEngine::FragmentationAdvisory advisory{};
    EXPECT_TRUE(db_.storage_engine()->getFragmentationAdvisory(table_id_, page_id, &advisory));
    EXPECT_TRUE(advisory.compact_threshold);
    EXPECT_FALSE(advisory.rewrite_recommended);
    EXPECT_TRUE(advisory.compaction_applied);

    db_.buffer_pool()->unpinPage(page_id, false, &ctx);
    db_.page_manager()->freePage(page_id, &ctx);
}

TEST_F(MgaFragmentationPolicyTest, GcPagePublishesRewriteRecommendationForHeavyDeadSpace)
{
    ErrorContext ctx;
    uint32_t page_id = 0;
    uint8_t *page_data = nullptr;
    ASSERT_EQ(allocatePage(page_id, &page_data, &ctx), Status::OK) << ctx.message;

    HeapPage heap(page_data, db_.page_size());
    ASSERT_EQ(heap.initialize(page_id, &ctx), Status::OK) << ctx.message;
    setPageTableId(page_data);

    const uint64_t xmin = deadXmax();
    auto tuple = buildTuple(3200, 0x33);
    std::vector<uint16_t> slots;
    for (int i = 0; i < 4; ++i)
    {
        uint16_t slot = 0;
        ASSERT_EQ(heap.insertTuple(tuple.data(), static_cast<uint32_t>(tuple.size()), xmin, &slot,
                                   &ctx),
                  Status::OK)
            << ctx.message;
        slots.push_back(slot);
    }

    markTupleDeadForGc(page_data, slots[1], deadXmax());
    markTupleDeadForGc(page_data, slots[2], deadXmax());
    db_.buffer_pool()->unpinPage(page_id, true, &ctx);

    GcStats stats{};
    ASSERT_EQ(db_.gc_manager()->gcPage(table_id_, page_id, &stats, &ctx), Status::OK)
        << ctx.message;

    StorageEngine::FragmentationAdvisory advisory{};
    ASSERT_TRUE(db_.storage_engine()->getFragmentationAdvisory(table_id_, page_id, &advisory));
    EXPECT_TRUE(advisory.warn_threshold);
    EXPECT_TRUE(advisory.compact_threshold);
    EXPECT_TRUE(advisory.rewrite_recommended);
    EXPECT_TRUE(advisory.compaction_applied);
    EXPECT_EQ(advisory.deleted_slots, 2);
    EXPECT_GT(advisory.reclaimable_bytes, 0u);

    EXPECT_EQ(stats.pages_dead_space_warn, 1u);
    EXPECT_EQ(stats.pages_dead_space_compact, 1u);
    EXPECT_EQ(stats.pages_dead_space_rewrite, 1u);
    EXPECT_EQ(stats.rewrite_recommendations, 1u);
    EXPECT_EQ(stats.slot_stable_compactions, 1u);

    void *buffer = nullptr;
    ASSERT_EQ(db_.buffer_pool()->pinPage(page_id, &buffer, &ctx), Status::OK) << ctx.message;
    page_data = static_cast<uint8_t *>(buffer);
    auto *items = reinterpret_cast<ItemPointer *>(page_data + sizeof(PageHeader));
    EXPECT_TRUE(items[slots[1]].isUnused());
    EXPECT_TRUE(items[slots[2]].isUnused());
    db_.buffer_pool()->unpinPage(page_id, false, &ctx);
    db_.page_manager()->freePage(page_id, &ctx);
}

TEST_F(MgaFragmentationPolicyTest, GcPagePublishesExactCleanupCompletionWithProofContext)
{
    const ID table_id = createCatalogBackedTable("gc_exact_table", "id", DataType::INT32, 4);
    createCatalogIndex(table_id, "idx_gc_exact", "id", CatalogManager::IndexType::BTREE);
    seedDeadTupleOnTableRoot(table_id);

    ErrorContext ctx;
    GcStats stats{};
    HeapReclaimPublicationContext publication_ctx{};
    publication_ctx.sweep_generation = 7;
    publication_ctx.checkpoint_generation = 11;

    ASSERT_EQ(db_.gc_manager()->gcPage(table_id,
                                       rootPageForTable(table_id),
                                       &stats,
                                       &ctx,
                                       &publication_ctx),
              Status::OK)
        << ctx.message;

    const auto publication = findPublication("idx_gc_exact");
    ASSERT_TRUE(publication.has_value());
    EXPECT_EQ(publication->family, IndexCleanupFamily::EXACT);
    EXPECT_EQ(publication->state, IndexCleanupPublicationState::COMPLETE);
    EXPECT_EQ(publication->page_id, rootPageForTable(table_id));
    EXPECT_EQ(publication->heap_reclaim_count, 1u);
    EXPECT_EQ(publication->backlog_count, 0u);
    EXPECT_EQ(publication->sweep_generation, 7u);
    EXPECT_EQ(publication->checkpoint_generation, 11u);
    EXPECT_EQ(stats.index_backlog_count, 0u);
}

TEST_F(MgaFragmentationPolicyTest, GcPagePublishesSummaryCleanupDebtWithProofContext)
{
    const ID table_id = createCatalogBackedTable("gc_summary_table", "id", DataType::INT32, 4);
    createCatalogIndex(table_id, "idx_gc_summary", "id", CatalogManager::IndexType::BRIN);
    seedDeadTupleOnTableRoot(table_id);

    ErrorContext ctx;
    GcStats stats{};
    HeapReclaimPublicationContext publication_ctx{};
    publication_ctx.sweep_generation = 5;
    publication_ctx.checkpoint_generation = 9;

    ASSERT_EQ(db_.gc_manager()->gcPage(table_id,
                                       rootPageForTable(table_id),
                                       &stats,
                                       &ctx,
                                       &publication_ctx),
              Status::OK)
        << ctx.message;

    const auto publication = findPublication("idx_gc_summary");
    ASSERT_TRUE(publication.has_value());
    EXPECT_EQ(publication->family, IndexCleanupFamily::SUMMARY);
    EXPECT_EQ(publication->state, IndexCleanupPublicationState::DEBT_PUBLISHED);
    EXPECT_EQ(publication->heap_reclaim_count, 1u);
    EXPECT_EQ(publication->backlog_count, 1u);
    EXPECT_EQ(publication->sweep_generation, 5u);
    EXPECT_EQ(publication->checkpoint_generation, 9u);
    EXPECT_EQ(stats.index_backlog_count, 1u);
}

TEST_F(MgaFragmentationPolicyTest, GcPagePublishesApproximateCleanupDebtWithProofContext)
{
    const ID table_id =
        createCatalogBackedTable("gc_approx_table", "embedding", DataType::VECTOR, 3);
    createCatalogIndex(table_id,
                       "idx_gc_approx",
                       "embedding",
                       CatalogManager::IndexType::HNSW);
    seedDeadTupleOnTableRoot(table_id);

    ErrorContext ctx;
    GcStats stats{};
    HeapReclaimPublicationContext publication_ctx{};
    publication_ctx.sweep_generation = 13;
    publication_ctx.checkpoint_generation = 17;

    ASSERT_EQ(db_.gc_manager()->gcPage(table_id,
                                       rootPageForTable(table_id),
                                       &stats,
                                       &ctx,
                                       &publication_ctx),
              Status::OK)
        << ctx.message;

    const auto publication = findPublication("idx_gc_approx");
    ASSERT_TRUE(publication.has_value());
    EXPECT_EQ(publication->family, IndexCleanupFamily::APPROXIMATE);
    EXPECT_EQ(publication->state, IndexCleanupPublicationState::DEBT_PUBLISHED);
    EXPECT_EQ(publication->heap_reclaim_count, 1u);
    EXPECT_EQ(publication->backlog_count, 1u);
    EXPECT_EQ(publication->sweep_generation, 13u);
    EXPECT_EQ(publication->checkpoint_generation, 17u);
    EXPECT_EQ(stats.index_backlog_count, 1u);
}
