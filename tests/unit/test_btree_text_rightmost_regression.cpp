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

#include "scratchbird/core/btree.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/page_manager.h"
#include "test_helpers.h"

#include <cstdio>
#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using namespace scratchbird::core;
using scratchbird::testing::uniqueTestDbPath;

namespace
{
auto makeUuid(uint8_t seed) -> UuidV7Bytes
{
    UuidV7Bytes uuid{};
    for (size_t i = 0; i < uuid.bytes.size(); ++i)
    {
        uuid.bytes[i] = static_cast<uint8_t>(seed + i);
    }
    return uuid;
}

auto encodeSortableUint64(uint64_t value) -> std::vector<uint8_t>
{
    std::vector<uint8_t> key(sizeof(uint64_t), 0);
    for (size_t i = 0; i < sizeof(uint64_t); ++i)
    {
        const size_t shift = (sizeof(uint64_t) - 1 - i) * 8;
        key[i] = static_cast<uint8_t>((value >> shift) & 0xFFu);
    }
    return key;
}

auto makeWideMonotonicKey(uint64_t value) -> std::vector<uint8_t>
{
    std::vector<uint8_t> key(64, 0);
    const std::vector<uint8_t> prefix = encodeSortableUint64(value);
    std::copy(prefix.begin(), prefix.end(), key.begin());

    for (size_t i = prefix.size(); i < key.size(); ++i)
    {
        const uint8_t lane = static_cast<uint8_t>((value + (i * 17u)) & 0xFFu);
        key[i] = lane;
    }

    return key;
}

auto hotLeafReservedFreeBytesForTest(uint32_t page_size) -> uint32_t
{
    if (page_size <= sizeof(SBBTreePage))
    {
        return 0;
    }

    const uint32_t usable_bytes = page_size - sizeof(SBBTreePage);
    return std::max<uint32_t>(1u, (usable_bytes * 15u + 99u) / 100u);
}

auto estimatedLeafInsertFootprintForTest(const std::vector<uint8_t> &key) -> uint32_t
{
    return static_cast<uint32_t>(sizeof(SBBTreeNode) + key.size() + sizeof(OnDiskTID) +
                                 sizeof(uint16_t));
}

auto countBrokenInternalPages(Database &db, const UuidV7Bytes &index_uuid, ErrorContext *ctx) -> int
{
    int broken_pages = 0;
    auto *bp = db.buffer_pool();
    auto *pm = db.page_manager();

    for (uint32_t page_id = 0; page_id < pm->totalPages(); ++page_id)
    {
        void *page_data = nullptr;
        if (bp->pinPage(page_id, &page_data, ctx) != Status::OK)
        {
            continue;
        }

        auto *page = reinterpret_cast<SBBTreePage *>(page_data);
        const bool is_btree_page = page->btr_header.page_type == PAGE_TYPE_BTREE_LEAF ||
                                   page->btr_header.page_type == PAGE_TYPE_BTREE_INTERNAL;
        const bool uuid_matches =
            std::memcmp(page->btr_index_uuid.bytes.data(), index_uuid.bytes.data(), 16) == 0;
        const bool is_leaf =
            (page->btr_flags & static_cast<uint16_t>(BTreeFlags::LEAF)) != 0;

        if (is_btree_page && uuid_matches && !is_leaf && page->btr_count > 0 &&
            page->btr_rightmost_child == 0)
        {
            ++broken_pages;
        }

        bp->unpinPage(page_id, false, ctx);
    }

    return broken_pages;
}

auto findRightmostLeafPage(Database &db, uint64_t root_page_num, ErrorContext *ctx) -> uint64_t
{
    auto *bp = db.buffer_pool();
    uint64_t current_page = root_page_num;

    while (current_page != 0)
    {
        void *page_data = nullptr;
        if (bp->pinPage(static_cast<uint32_t>(current_page), &page_data, ctx) != Status::OK)
        {
            return 0;
        }

        auto *page = reinterpret_cast<SBBTreePage *>(page_data);
        const bool is_leaf =
            page->btr_header.page_type == PAGE_TYPE_BTREE_LEAF ||
            ((page->btr_flags & static_cast<uint16_t>(BTreeFlags::LEAF)) != 0 &&
             page->btr_level == 0);
        const uint64_t next_page =
            is_leaf ? 0 : static_cast<uint64_t>(page->btr_rightmost_child);

        bp->unpinPage(static_cast<uint32_t>(current_page), false, ctx);

        if (is_leaf)
        {
            return current_page;
        }
        current_page = next_page;
    }

    return 0;
}

auto countLeafPages(Database &db, const UuidV7Bytes &index_uuid, ErrorContext *ctx) -> int
{
    int leaf_pages = 0;
    auto *bp = db.buffer_pool();
    auto *pm = db.page_manager();

    for (uint32_t page_id = 0; page_id < pm->totalPages(); ++page_id)
    {
        void *page_data = nullptr;
        if (bp->pinPage(page_id, &page_data, ctx) != Status::OK)
        {
            continue;
        }

        auto *page = reinterpret_cast<SBBTreePage *>(page_data);
        const bool is_btree_page = page->btr_header.page_type == PAGE_TYPE_BTREE_LEAF ||
                                   page->btr_header.page_type == PAGE_TYPE_BTREE_INTERNAL;
        const bool uuid_matches =
            std::memcmp(page->btr_index_uuid.bytes.data(), index_uuid.bytes.data(), 16) == 0;
        const bool is_leaf =
            page->btr_header.page_type == PAGE_TYPE_BTREE_LEAF ||
            (page->btr_flags & static_cast<uint16_t>(BTreeFlags::LEAF)) != 0;

        if (is_btree_page && uuid_matches && is_leaf)
        {
            ++leaf_pages;
        }

        bp->unpinPage(page_id, false, ctx);
    }

    return leaf_pages;
}

auto readLeafFreeSpace(Database &db, uint64_t page_num, ErrorContext *ctx) -> uint16_t
{
    void *page_data = nullptr;
    if (db.buffer_pool()->pinPage(static_cast<uint32_t>(page_num), &page_data, ctx) != Status::OK)
    {
        return 0;
    }

    const auto *page = reinterpret_cast<const SBBTreePage *>(page_data);
    const uint16_t free_space = page->btr_free_space;
    db.buffer_pool()->unpinPage(static_cast<uint32_t>(page_num), false, ctx);
    return free_space;
}
} // namespace

TEST(BtreeTextRightmostRegressionTest, EmailLikeKeysKeepRightmostChildBound)
{
    const std::string db_path = uniqueTestDbPath("test_btree_text_rightmost_regression", ".sbrd");
    std::remove(db_path.c_str());

    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path.c_str(), 8192, &ctx), Status::OK) << ctx.message;

    Database db;
    ASSERT_EQ(db.open(db_path.c_str(), &ctx), Status::OK) << ctx.message;

    const auto index_uuid = makeUuid(0x40);
    const auto table_uuid = makeUuid(0x60);
    const std::vector<UuidV7Bytes> column_uuids{};

    GPID root_gpid = 0;
    ASSERT_EQ(db.page_manager()->allocatePageInTablespace(PRIMARY_TABLESPACE_ID, &root_gpid, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(BTree::create(&db, index_uuid, table_uuid, column_uuids, root_gpid, &ctx),
              Status::OK)
        << ctx.message;

    std::unique_ptr<BTree> btree(BTree::open(&db, index_uuid, root_gpid, &ctx));
    ASSERT_NE(btree, nullptr) << ctx.message;

    constexpr int kInsertCount = 100000;
    for (int i = 1; i <= kInsertCount; ++i)
    {
        const std::string key_string = "user" + std::to_string(i) + "@example.com";
        const std::vector<uint8_t> key(key_string.begin(), key_string.end());
        const TID tid(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(1000 + i), 1);

        const Status status = btree->insert(key, tid, 1, &ctx);
        ASSERT_EQ(status, Status::OK) << "insert failed at i=" << i << ": " << ctx.message;
    }

    EXPECT_EQ(countBrokenInternalPages(db, index_uuid, &ctx), 0) << ctx.message;

    db.close();
    std::remove(db_path.c_str());
}

TEST(BtreeTextRightmostRegressionTest, SequentialBigintKeysKeepRightmostChildBoundAtBenchmarkScale)
{
    const std::string db_path =
        uniqueTestDbPath("test_btree_numeric_rightmost_regression", ".sbrd");
    std::remove(db_path.c_str());

    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path.c_str(), 8192, &ctx), Status::OK) << ctx.message;

    Database db;
    ASSERT_EQ(db.open(db_path.c_str(), &ctx), Status::OK) << ctx.message;

    const auto index_uuid = makeUuid(0x21);
    const auto table_uuid = makeUuid(0x41);
    const std::vector<UuidV7Bytes> column_uuids{};

    GPID root_gpid = 0;
    ASSERT_EQ(db.page_manager()->allocatePageInTablespace(PRIMARY_TABLESPACE_ID, &root_gpid, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(BTree::create(&db, index_uuid, table_uuid, column_uuids, root_gpid, &ctx),
              Status::OK)
        << ctx.message;

    std::unique_ptr<BTree> btree(BTree::open(&db, index_uuid, root_gpid, &ctx));
    ASSERT_NE(btree, nullptr) << ctx.message;

    constexpr uint64_t kInsertCount = 200000;
    constexpr uint64_t kAuditInterval = 5000;
    for (uint64_t i = 1; i <= kInsertCount; ++i)
    {
        std::vector<uint8_t> key(sizeof(uint64_t));
        std::memcpy(key.data(), &i, sizeof(uint64_t));
        const TID tid(PRIMARY_TABLESPACE_ID, 100000 + i, 1);

        const Status status = btree->insert(key, tid, 1, &ctx);
        ASSERT_EQ(status, Status::OK) << "insert failed at i=" << i << ": " << ctx.message;

        if ((i % kAuditInterval) == 0)
        {
            EXPECT_EQ(countBrokenInternalPages(db, index_uuid, &ctx), 0)
                << "broken internal page detected after " << i << " sequential inserts";
        }
    }

    EXPECT_EQ(countBrokenInternalPages(db, index_uuid, &ctx), 0) << ctx.message;

    db.close();
    std::remove(db_path.c_str());
}

TEST(BtreeTextRightmostRegressionTest, SequentialBigintPreflightSearchesKeepRightmostChildBound)
{
    const std::string db_path =
        uniqueTestDbPath("test_btree_numeric_preflight_rightmost_regression", ".sbrd");
    std::remove(db_path.c_str());

    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path.c_str(), 8192, &ctx), Status::OK) << ctx.message;

    Database db;
    ASSERT_EQ(db.open(db_path.c_str(), &ctx), Status::OK) << ctx.message;

    const auto index_uuid = makeUuid(0x81);
    const auto table_uuid = makeUuid(0xA1);
    const std::vector<UuidV7Bytes> column_uuids{};

    GPID root_gpid = 0;
    ASSERT_EQ(db.page_manager()->allocatePageInTablespace(PRIMARY_TABLESPACE_ID, &root_gpid, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(BTree::create(&db, index_uuid, table_uuid, column_uuids, root_gpid, &ctx),
              Status::OK)
        << ctx.message;

    std::unique_ptr<BTree> btree(BTree::open(&db, index_uuid, root_gpid, &ctx));
    ASSERT_NE(btree, nullptr) << ctx.message;

    constexpr uint64_t kInsertCount = 150000;
    constexpr uint64_t kBatchSize = 1024;
    constexpr uint64_t kAuditInterval = 10000;

    for (uint64_t i = 1; i <= kInsertCount; ++i)
    {
        std::vector<uint8_t> key(sizeof(uint64_t));
        std::memcpy(key.data(), &i, sizeof(uint64_t));

        const uint64_t xid = 1 + ((i - 1) / kBatchSize);
        std::vector<TID> existing_tids;
        ASSERT_EQ(btree->search(key, xid, &existing_tids, &ctx), Status::NOT_FOUND)
            << "preflight search failed at i=" << i << ": " << ctx.message;
        ASSERT_TRUE(existing_tids.empty()) << "unexpected duplicate at i=" << i;

        const TID tid(PRIMARY_TABLESPACE_ID, 500000 + i, 1);
        ASSERT_EQ(btree->insert(key, tid, xid, &ctx), Status::OK)
            << "insert failed at i=" << i << ": " << ctx.message;

        if ((i % kAuditInterval) == 0)
        {
            EXPECT_EQ(countBrokenInternalPages(db, index_uuid, &ctx), 0)
                << "broken internal page detected after preflight+insert cycle " << i;
        }
    }

    EXPECT_EQ(countBrokenInternalPages(db, index_uuid, &ctx), 0) << ctx.message;

    db.close();
    std::remove(db_path.c_str());
}

TEST(BtreeTextRightmostRegressionTest, TraversalRepairsLeafFlagFromPageTypeOnHotRightmostLeaf)
{
    const std::string db_path =
        uniqueTestDbPath("test_btree_leaf_shape_normalization", ".sbrd");
    std::remove(db_path.c_str());

    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path.c_str(), 16384, &ctx), Status::OK) << ctx.message;

    Database db;
    ASSERT_EQ(db.open(db_path.c_str(), &ctx), Status::OK) << ctx.message;

    const auto index_uuid = makeUuid(0xB1);
    const auto table_uuid = makeUuid(0xC1);
    const std::vector<UuidV7Bytes> column_uuids{};

    GPID root_gpid = 0;
    ASSERT_EQ(db.page_manager()->allocatePageInTablespace(PRIMARY_TABLESPACE_ID, &root_gpid, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(BTree::create(&db, index_uuid, table_uuid, column_uuids, root_gpid, &ctx),
              Status::OK)
        << ctx.message;

    std::unique_ptr<BTree> btree(BTree::open(&db, index_uuid, root_gpid, &ctx));
    ASSERT_NE(btree, nullptr) << ctx.message;

    constexpr uint64_t kInsertCount = 50000;
    for (uint64_t i = 1; i <= kInsertCount; ++i)
    {
        std::vector<uint8_t> key = encodeSortableUint64(i);
        const TID tid(PRIMARY_TABLESPACE_ID, 700000 + i, 1);
        ASSERT_EQ(btree->insert(key, tid, 1, &ctx), Status::OK)
            << "insert failed at i=" << i << ": " << ctx.message;
    }

    const uint64_t rightmost_leaf =
        findRightmostLeafPage(db, btree->getIndexInfo().idx_root_page, &ctx);
    ASSERT_NE(rightmost_leaf, 0u) << ctx.message;

    void *page_data = nullptr;
    ASSERT_EQ(db.buffer_pool()->pinPage(static_cast<uint32_t>(rightmost_leaf), &page_data, &ctx),
              Status::OK)
        << ctx.message;
    auto *page = reinterpret_cast<SBBTreePage *>(page_data);
    ASSERT_EQ(page->btr_header.page_type, PAGE_TYPE_BTREE_LEAF);
    page->btr_flags &= ~static_cast<uint16_t>(BTreeFlags::LEAF);
    db.buffer_pool()->unpinPage(static_cast<uint32_t>(rightmost_leaf), true, &ctx);

    const uint64_t next_key_value = kInsertCount + 1;
    std::vector<uint8_t> next_key = encodeSortableUint64(next_key_value);
    const TID next_tid(PRIMARY_TABLESPACE_ID, 800000 + next_key_value, 1);
    ASSERT_EQ(btree->insert(next_key, next_tid, 1, &ctx), Status::OK) << ctx.message;

    page_data = nullptr;
    ASSERT_EQ(db.buffer_pool()->pinPage(static_cast<uint32_t>(rightmost_leaf), &page_data, &ctx),
              Status::OK)
        << ctx.message;
    page = reinterpret_cast<SBBTreePage *>(page_data);
    EXPECT_NE(page->btr_flags & static_cast<uint16_t>(BTreeFlags::LEAF), 0);
    EXPECT_EQ(page->btr_header.page_type, PAGE_TYPE_BTREE_LEAF);
    db.buffer_pool()->unpinPage(static_cast<uint32_t>(rightmost_leaf), false, &ctx);

    EXPECT_EQ(countBrokenInternalPages(db, index_uuid, &ctx), 0) << ctx.message;

    db.close();
    std::remove(db_path.c_str());
}

TEST(BtreeTextRightmostRegressionTest, SequentialWideKeysPresplitHotRightmostLeafBeforeFull)
{
    const std::string db_path =
        uniqueTestDbPath("test_btree_hot_rightmost_presplit_regression", ".sbrd");
    std::remove(db_path.c_str());

    ErrorContext ctx;
    constexpr uint32_t kPageSize = 8192;
    ASSERT_EQ(Database::create(db_path.c_str(), kPageSize, &ctx), Status::OK) << ctx.message;

    Database db;
    ASSERT_EQ(db.open(db_path.c_str(), &ctx), Status::OK) << ctx.message;

    const auto index_uuid = makeUuid(0xD1);
    const auto table_uuid = makeUuid(0xE1);
    const std::vector<UuidV7Bytes> column_uuids{};

    GPID root_gpid = 0;
    ASSERT_EQ(db.page_manager()->allocatePageInTablespace(PRIMARY_TABLESPACE_ID, &root_gpid, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(BTree::create(&db, index_uuid, table_uuid, column_uuids, root_gpid, &ctx),
              Status::OK)
        << ctx.message;

    std::unique_ptr<BTree> btree(BTree::open(&db, index_uuid, root_gpid, &ctx));
    ASSERT_NE(btree, nullptr) << ctx.message;

    const uint32_t reserved_free = hotLeafReservedFreeBytesForTest(kPageSize);
    const uint32_t insert_footprint =
        estimatedLeafInsertFootprintForTest(makeWideMonotonicKey(1));

    bool armed = false;
    uint64_t trigger_value = 0;
    uint64_t rightmost_before = 0;
    int leaf_count_before = 0;
    uint16_t free_space_before = 0;

    for (uint64_t value = 1; value <= 10000; ++value)
    {
        rightmost_before = findRightmostLeafPage(db, btree->getIndexInfo().idx_root_page, &ctx);
        ASSERT_NE(rightmost_before, 0u) << ctx.message;

        free_space_before = readLeafFreeSpace(db, rightmost_before, &ctx);
        leaf_count_before = countLeafPages(db, index_uuid, &ctx);

        if (free_space_before > insert_footprint &&
            free_space_before <= reserved_free + insert_footprint)
        {
            trigger_value = value;
            armed = true;
            break;
        }

        const std::vector<uint8_t> key = makeWideMonotonicKey(value);
        const TID tid(PRIMARY_TABLESPACE_ID, 900000 + value, 1);
        ASSERT_EQ(btree->insert(key, tid, 1, &ctx), Status::OK)
            << "staging insert failed at value=" << value << ": " << ctx.message;
    }

    ASSERT_TRUE(armed) << "failed to stage a hot rightmost leaf near the reserved free-space band";

    const BTree::HotLeafStats hot_leaf_before = btree->getHotLeafStats();
    const std::vector<uint8_t> trigger_key = makeWideMonotonicKey(trigger_value);
    const TID trigger_tid(PRIMARY_TABLESPACE_ID, 950000 + trigger_value, 1);
    ASSERT_EQ(btree->insert(trigger_key, trigger_tid, 1, &ctx), Status::OK) << ctx.message;
    const BTree::HotLeafStats hot_leaf_after = btree->getHotLeafStats();

    const int leaf_count_after = countLeafPages(db, index_uuid, &ctx);
    EXPECT_GT(leaf_count_after, leaf_count_before)
        << "hot rightmost insert should pre-split before the leaf becomes completely full";

    const uint64_t rightmost_after =
        findRightmostLeafPage(db, btree->getIndexInfo().idx_root_page, &ctx);
    ASSERT_NE(rightmost_after, 0u) << ctx.message;
    EXPECT_NE(rightmost_after, rightmost_before);
    EXPECT_GE(readLeafFreeSpace(db, rightmost_after, &ctx), reserved_free);

    std::vector<TID> found_tids;
    ASSERT_EQ(btree->search(trigger_key, 1, &found_tids, &ctx), Status::OK) << ctx.message;
    EXPECT_FALSE(found_tids.empty());
    EXPECT_EQ(countBrokenInternalPages(db, index_uuid, &ctx), 0) << ctx.message;
    EXPECT_GE(hot_leaf_after.right_edge_detections, hot_leaf_before.right_edge_detections + 1);
    EXPECT_EQ(hot_leaf_after.right_edge_presplits, hot_leaf_before.right_edge_presplits + 1);
    EXPECT_EQ(hot_leaf_after.right_edge_split_retries,
              hot_leaf_before.right_edge_split_retries);

    db.close();
    std::remove(db_path.c_str());
}
