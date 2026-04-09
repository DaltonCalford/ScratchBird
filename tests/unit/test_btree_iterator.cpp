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
 * @file test_btree_iterator.cpp
 * @brief B-Tree Iterator Test Suite
 *
 * Suite 3 (Indexing & Search) Compliance Tests
 *
 * Tests B-Tree iterator functionality including:
 * - Full table scans (empty, single-page, multi-page)
 * - Bounded scans (inclusive/exclusive)
 * - Unbounded scans
 * - Scans with duplicate keys
 * - getScannedCount() validation
 */

#include <gtest/gtest.h>
#include "scratchbird/core/btree.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/gpid.h"
#include "scratchbird/core/index_key_encoding.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/tid.h"
#include "test_helpers.h"
#include <filesystem>
#include <vector>
#include <algorithm>
#include <cstring>
#include <random>
#include <iostream>

using namespace scratchbird::core;

class BTreeIteratorTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        test_db_path_ = scratchbird::testing::uniqueTestDbPath("test_btree_iter", ".db");
        std::filesystem::remove(test_db_path_);

        ErrorContext ctx;
        ASSERT_EQ(Database::create(test_db_path_.string(), 8192, &ctx), Status::OK)
            << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(test_db_path_.string(), &ctx), Status::OK)
            << ctx.message;
    }

    void TearDown() override
    {
        btree_.reset();
        db_.reset();
        std::filesystem::remove(test_db_path_);
    }

    // Helper: Create a B-Tree index
    Status createIndex(GPID* root_gpid_out)
    {
        ErrorContext ctx;

        // Create index UUID
        memset(index_uuid_.bytes.data(), 0xAA, 16);
        memset(table_uuid_.bytes.data(), 0xBB, 16);

        // Single column index
        UuidV7Bytes column_uuid;
        memset(column_uuid.bytes.data(), 0xCC, 16);
        std::vector<UuidV7Bytes> columns = {column_uuid};

        GPID root_gpid = allocateRootGpid(&ctx);
        if (root_gpid == 0)
        {
            return Status::IO_ERROR;
        }

        Status status = BTree::create(db_.get(), index_uuid_, table_uuid_, columns, root_gpid, &ctx);
        if (status != Status::OK)
        {
            return status;
        }

        if (root_gpid_out)
        {
            *root_gpid_out = root_gpid;
        }

        return Status::OK;
    }

    // Helper: Open existing B-Tree
    void openIndex(GPID root_gpid)
    {
        ErrorContext ctx;
        btree_ = BTree::open(db_.get(), index_uuid_, root_gpid, &ctx);
        ASSERT_NE(btree_, nullptr);
    }

    // Helper: Create a key from an integer
    std::vector<uint8_t> makeKey(int32_t value)
    {
        std::vector<uint8_t> plain_key(sizeof(int32_t));
        std::memcpy(plain_key.data(), &value, sizeof(int32_t));

        std::vector<uint8_t> encoded_key;
        ErrorContext ctx;
        EXPECT_EQ(index_key_encoding::encodePlainValue(DataType::INT32,
                                                       plain_key,
                                                       &encoded_key,
                                                       &ctx),
                  Status::OK)
            << ctx.message;
        return encoded_key;
    }

    // Helper: Extract integer from key
    int32_t extractKey(const std::vector<uint8_t>& key)
    {
        std::vector<uint8_t> plain_key;
        ErrorContext ctx;
        if (index_key_encoding::decodeToPlainValue(DataType::INT32,
                                                   key.data(),
                                                   key.size(),
                                                   &plain_key,
                                                   &ctx) != Status::OK ||
            plain_key.size() < sizeof(int32_t))
        {
            return 0;
        }

        int32_t value = 0;
        std::memcpy(&value, plain_key.data(), sizeof(int32_t));
        return value;
    }

    GPID allocateRootGpid(ErrorContext *ctx)
    {
        if (!db_)
        {
            if (ctx) ctx->message = "Database not initialized";
            return 0;
        }
        auto *pm = db_->page_manager();
        if (!pm)
        {
            if (ctx) ctx->message = "PageManager not available";
            return 0;
        }
        GPID gpid = 0;
        Status status = pm->allocatePageInTablespace(PRIMARY_TABLESPACE_ID, &gpid, ctx);
        if (status != Status::OK)
        {
            return 0;
        }
        return gpid;
    }

    // Helper: Insert multiple keys
    void insertKeys(const std::vector<int32_t>& keys)
    {
        ErrorContext ctx;
        for (int32_t k : keys) {
            auto key = makeKey(k);
            TID tid{makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(k)), 1};
            ASSERT_EQ(btree_->insert(key, tid, 1, &ctx), Status::OK)
                << "Failed to insert key " << k;
        }
    }

    std::filesystem::path test_db_path_;
    std::unique_ptr<Database> db_;
    std::unique_ptr<BTree> btree_;
    UuidV7Bytes index_uuid_;
    UuidV7Bytes table_uuid_;
};

/**
 * Test: Full Scan - Empty Index
 *
 * Verifies iterator correctly handles scanning an empty B-Tree.
 */
TEST_F(BTreeIteratorTest, FullScanEmpty)
{
    GPID root_gpid;
    ASSERT_EQ(createIndex(&root_gpid), Status::OK);
    openIndex(root_gpid);

    ErrorContext ctx;
    auto iter = btree_->rangeScan(nullptr, nullptr, 0, true, false, &ctx);
    ASSERT_NE(iter, nullptr);

    // Should have no elements
    EXPECT_FALSE(iter->hasNext());
    EXPECT_EQ(iter->getScannedCount(), 0u);
}

/**
 * Test: Full Scan - Single Page
 *
 * Verifies full scan of an index with all entries on single page.
 */
TEST_F(BTreeIteratorTest, FullScanSinglePage)
{
    GPID root_gpid;
    ASSERT_EQ(createIndex(&root_gpid), Status::OK);
    openIndex(root_gpid);

    // Insert 10 keys in random order
    std::vector<int32_t> keys = {5, 2, 8, 1, 9, 3, 7, 4, 6, 10};
    insertKeys(keys);

    // Full scan
    ErrorContext ctx;

    // Debug: Check root page btr_count before scanning
    void* page_buffer;
    uint32_t root_page_id = 0;
    ASSERT_TRUE(convertGPIDtoPageID(root_gpid, &root_page_id));
    ASSERT_EQ(db_->buffer_pool()->pinPage(root_page_id, &page_buffer, &ctx), Status::OK);
    auto* page = reinterpret_cast<const SBBTreePage*>(page_buffer);
    uint16_t actual_count = page->btr_count;
    db_->buffer_pool()->unpinPage(root_page_id, false, &ctx);

    // Debug output
    if (actual_count != 10) {
        std::cout << "WARNING: Root page btr_count = " << actual_count << " (expected 10)" << std::endl;
    }

    auto iter = btree_->rangeScan(nullptr, nullptr, 0, true, false, &ctx);
    ASSERT_NE(iter, nullptr);

    // Collect scanned keys
    std::vector<int32_t> scanned;
    while (iter->hasNext()) {
        std::vector<uint8_t> key;
        TID tid;
        ASSERT_EQ(iter->next(&key, &tid, &ctx), Status::OK);
        scanned.push_back(extractKey(key));
    }

    // Should scan all 10 keys in sorted order
    EXPECT_EQ(scanned.size(), 10u);
    EXPECT_TRUE(std::is_sorted(scanned.begin(), scanned.end()));

    // Verify scan count
    EXPECT_EQ(iter->getScannedCount(), 10u);
}

/**
 * Test: Full Scan - Multiple Pages
 *
 * Verifies full scan across multiple B-Tree pages.
 */
TEST_F(BTreeIteratorTest, FullScanMultiplePages)
{
    GPID root_gpid;
    ASSERT_EQ(createIndex(&root_gpid), Status::OK);
    openIndex(root_gpid);

    // Insert enough keys to span multiple pages (100 keys)
    std::vector<int32_t> keys;
    for (int i = 1; i <= 100; i++) {
        keys.push_back(i);
    }
    // Shuffle to force tree growth
    std::mt19937 rng(42);
    std::shuffle(keys.begin(), keys.end(), rng);
    insertKeys(keys);

    // Full scan
    ErrorContext ctx;
    auto iter = btree_->rangeScan(nullptr, nullptr, 0, true, false, &ctx);
    ASSERT_NE(iter, nullptr);

    // Collect all keys
    std::vector<int32_t> scanned;
    while (iter->hasNext()) {
        std::vector<uint8_t> key;
        TID tid;
        ASSERT_EQ(iter->next(&key, &tid, &ctx), Status::OK);
        scanned.push_back(extractKey(key));
    }

    // Should scan all 100 keys in order
    EXPECT_EQ(scanned.size(), 100u);
    EXPECT_TRUE(std::is_sorted(scanned.begin(), scanned.end()));
    EXPECT_EQ(iter->getScannedCount(), 100u);
}

TEST_F(BTreeIteratorTest, AscendingRightEdgeRangeIncludesTerminalKeyAfterRepeatedSplits)
{
    GPID root_gpid;
    ASSERT_EQ(createIndex(&root_gpid), Status::OK);
    openIndex(root_gpid);

    std::vector<int32_t> keys;
    for (int i = 1; i <= 256; ++i)
    {
        keys.push_back(i);
    }
    insertKeys(keys);

    ErrorContext ctx;
    std::vector<TID> tids;
    ASSERT_EQ(btree_->search(makeKey(256), 0, &tids, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(tids.size(), 1u);

    auto start_key = makeKey(200);
    auto iter = btree_->rangeScan(&start_key, nullptr, 0, true, true, &ctx);
    ASSERT_NE(iter, nullptr);

    std::vector<int32_t> scanned;
    while (iter->hasNext())
    {
        std::vector<uint8_t> key;
        TID tid;
        ASSERT_EQ(iter->next(&key, &tid, &ctx), Status::OK) << ctx.message;
        scanned.push_back(extractKey(key));
    }

    ASSERT_EQ(scanned.size(), 57u);
    EXPECT_EQ(scanned.front(), 200);
    EXPECT_EQ(scanned.back(), 256);
}

/**
 * Test: Bounded Scan - Inclusive Start and End
 *
 * Verifies range scan with inclusive bounds [start, end].
 */
TEST_F(BTreeIteratorTest, BoundedScanInclusive)
{
    GPID root_page;
    ASSERT_EQ(createIndex(&root_page), Status::OK);
    openIndex(root_page);

    // Insert keys 1-20
    std::vector<int32_t> keys;
    for (int i = 1; i <= 20; i++) {
        keys.push_back(i);
    }
    insertKeys(keys);

    // Scan [5, 15] inclusive
    ErrorContext ctx;
    auto start_key = makeKey(5);
    auto end_key = makeKey(15);
    auto iter = btree_->rangeScan(&start_key, &end_key, 0, true, true, &ctx);
    ASSERT_NE(iter, nullptr);

    // Collect scanned keys
    std::vector<int32_t> scanned;
    while (iter->hasNext()) {
        std::vector<uint8_t> key;
        TID tid;
        ASSERT_EQ(iter->next(&key, &tid, &ctx), Status::OK);
        scanned.push_back(extractKey(key));
    }

    // Should scan 5, 6, 7, ..., 15 (11 keys)
    EXPECT_EQ(scanned.size(), 11u);
    EXPECT_EQ(scanned.front(), 5);
    EXPECT_EQ(scanned.back(), 15);
    EXPECT_TRUE(std::is_sorted(scanned.begin(), scanned.end()));
}

/**
 * Test: Bounded Scan - Exclusive Bounds
 *
 * Verifies range scan with exclusive bounds (start, end).
 */
TEST_F(BTreeIteratorTest, BoundedScanExclusive)
{
    GPID root_page;
    ASSERT_EQ(createIndex(&root_page), Status::OK);
    openIndex(root_page);

    // Insert keys 1-20
    std::vector<int32_t> keys;
    for (int i = 1; i <= 20; i++) {
        keys.push_back(i);
    }
    insertKeys(keys);

    // Scan (5, 15) exclusive
    ErrorContext ctx;
    auto start_key = makeKey(5);
    auto end_key = makeKey(15);
    auto iter = btree_->rangeScan(&start_key, &end_key, 0, false, false, &ctx);
    ASSERT_NE(iter, nullptr);

    // Collect scanned keys
    std::vector<int32_t> scanned;
    while (iter->hasNext()) {
        std::vector<uint8_t> key;
        TID tid;
        ASSERT_EQ(iter->next(&key, &tid, &ctx), Status::OK);
        scanned.push_back(extractKey(key));
    }

    // Should scan 6, 7, 8, ..., 14 (9 keys) - excludes 5 and 15
    EXPECT_EQ(scanned.size(), 9u);
    if (scanned.size() > 0) {
        EXPECT_EQ(scanned.front(), 6);
        EXPECT_EQ(scanned.back(), 14);
    }
    EXPECT_TRUE(std::is_sorted(scanned.begin(), scanned.end()));
}

/**
 * Test: Unbounded Start - Scan from Beginning
 *
 * Verifies scan with no start bound (scan from beginning to end_key).
 */
TEST_F(BTreeIteratorTest, UnboundedStartScan)
{
    GPID root_page;
    ASSERT_EQ(createIndex(&root_page), Status::OK);
    openIndex(root_page);

    // Insert keys 1-20
    std::vector<int32_t> keys;
    for (int i = 1; i <= 20; i++) {
        keys.push_back(i);
    }
    insertKeys(keys);

    // Scan (-∞, 10]
    ErrorContext ctx;
    auto end_key = makeKey(10);
    auto iter = btree_->rangeScan(nullptr, &end_key, 0, true, true, &ctx);
    ASSERT_NE(iter, nullptr);

    // Collect scanned keys
    std::vector<int32_t> scanned;
    while (iter->hasNext()) {
        std::vector<uint8_t> key;
        TID tid;
        ASSERT_EQ(iter->next(&key, &tid, &ctx), Status::OK);
        scanned.push_back(extractKey(key));
    }

    // Should scan 1, 2, ..., 10 (10 keys)
    EXPECT_EQ(scanned.size(), 10u);
    if (scanned.size() > 0) {
        EXPECT_EQ(scanned.front(), 1);
        EXPECT_EQ(scanned.back(), 10);
    }
}

/**
 * Test: Unbounded End - Scan to End
 *
 * Verifies scan with no end bound (scan from start_key to end).
 */
TEST_F(BTreeIteratorTest, UnboundedEndScan)
{
    GPID root_page;
    ASSERT_EQ(createIndex(&root_page), Status::OK);
    openIndex(root_page);

    // Insert keys 1-20
    std::vector<int32_t> keys;
    for (int i = 1; i <= 20; i++) {
        keys.push_back(i);
    }
    insertKeys(keys);

    // Scan [10, +∞)
    ErrorContext ctx;
    auto start_key = makeKey(10);
    auto iter = btree_->rangeScan(&start_key, nullptr, 0, true, false, &ctx);
    ASSERT_NE(iter, nullptr);

    // Collect scanned keys
    std::vector<int32_t> scanned;
    while (iter->hasNext()) {
        std::vector<uint8_t> key;
        TID tid;
        ASSERT_EQ(iter->next(&key, &tid, &ctx), Status::OK);
        scanned.push_back(extractKey(key));
    }

    // Should scan 10, 11, ..., 20 (11 keys)
    EXPECT_EQ(scanned.size(), 11u);
    if (scanned.size() > 0) {
        EXPECT_EQ(scanned.front(), 10);
        EXPECT_EQ(scanned.back(), 20);
    }
}

/**
 * Test: Scan with Duplicate Keys
 *
 * Verifies iterator correctly handles duplicate keys (multiple tuple IDs per key).
 */
TEST_F(BTreeIteratorTest, ScanWithDuplicates)
{
    GPID root_page;
    ASSERT_EQ(createIndex(&root_page), Status::OK);
    openIndex(root_page);

    ErrorContext ctx;

    // Insert multiple tuple IDs for same key
    auto key5 = makeKey(5);
    auto key10 = makeKey(10);
    auto key15 = makeKey(15);

    // Insert key 5 with 3 tuple IDs
    ASSERT_EQ(btree_->insert(key5, TID(makeGPID(PRIMARY_TABLESPACE_ID, 100), 1), 1, &ctx), Status::OK);
    ASSERT_EQ(btree_->insert(key5, TID(makeGPID(PRIMARY_TABLESPACE_ID, 101), 1), 1, &ctx), Status::OK);
    ASSERT_EQ(btree_->insert(key5, TID(makeGPID(PRIMARY_TABLESPACE_ID, 102), 1), 1, &ctx), Status::OK);

    // Insert key 10 with 2 tuple IDs
    ASSERT_EQ(btree_->insert(key10, TID(makeGPID(PRIMARY_TABLESPACE_ID, 200), 1), 1, &ctx), Status::OK);
    ASSERT_EQ(btree_->insert(key10, TID(makeGPID(PRIMARY_TABLESPACE_ID, 201), 1), 1, &ctx), Status::OK);

    // Insert key 15 with 1 tuple ID
    ASSERT_EQ(btree_->insert(key15, TID(makeGPID(PRIMARY_TABLESPACE_ID, 300), 1), 1, &ctx), Status::OK);

    // Full scan
    auto iter = btree_->rangeScan(nullptr, nullptr, 0, true, false, &ctx);
    ASSERT_NE(iter, nullptr);

    // Collect all entries
    int count = 0;
    while (iter->hasNext()) {
        std::vector<uint8_t> key;
        TID tid;
        ASSERT_EQ(iter->next(&key, &tid, &ctx), Status::OK);
        count++;
    }

    // Should scan 6 total entries (3 + 2 + 1)
    EXPECT_EQ(count, 6);
    EXPECT_EQ(iter->getScannedCount(), 6u);
}

/**
 * Test: getScannedCount() Validation
 *
 * Verifies getScannedCount() returns correct count during scan.
 */
TEST_F(BTreeIteratorTest, ScannedCountValidation)
{
    GPID root_page;
    ASSERT_EQ(createIndex(&root_page), Status::OK);
    openIndex(root_page);

    // Insert 10 keys
    std::vector<int32_t> keys = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    insertKeys(keys);

    ErrorContext ctx;
    auto iter = btree_->rangeScan(nullptr, nullptr, 0, true, false, &ctx);
    ASSERT_NE(iter, nullptr);

    // Initially 0
    EXPECT_EQ(iter->getScannedCount(), 0u);

    // Scan 5 items
    for (int i = 0; i < 5 && iter->hasNext(); i++) {
        std::vector<uint8_t> key;
        TID tid;
        ASSERT_EQ(iter->next(&key, &tid, &ctx), Status::OK);
    }
    EXPECT_EQ(iter->getScannedCount(), 5u);

    // Scan remaining items
    while (iter->hasNext()) {
        std::vector<uint8_t> key;
        TID tid;
        ASSERT_EQ(iter->next(&key, &tid, &ctx), Status::OK);
    }
    EXPECT_EQ(iter->getScannedCount(), 10u);
}

/**
 * Test: Empty Range Scan
 *
 * Verifies scan with start > end returns no results.
 */
TEST_F(BTreeIteratorTest, EmptyRangeScan)
{
    GPID root_page;
    ASSERT_EQ(createIndex(&root_page), Status::OK);
    openIndex(root_page);

    // Insert keys 1-20
    std::vector<int32_t> keys;
    for (int i = 1; i <= 20; i++) {
        keys.push_back(i);
    }
    insertKeys(keys);

    // Scan [15, 5] - invalid range
    ErrorContext ctx;
    auto start_key = makeKey(15);
    auto end_key = makeKey(5);
    auto iter = btree_->rangeScan(&start_key, &end_key, 0, true, true, &ctx);
    ASSERT_NE(iter, nullptr);

    // Should have no results
    EXPECT_FALSE(iter->hasNext());
    EXPECT_EQ(iter->getScannedCount(), 0u);
}

/**
 * Test: Scan Non-Existent Range
 *
 * Verifies scan in a range with no matching keys.
 */
TEST_F(BTreeIteratorTest, ScanNonExistentRange)
{
    GPID root_page;
    ASSERT_EQ(createIndex(&root_page), Status::OK);
    openIndex(root_page);

    // Insert keys 1-10, 20-30 (gap from 11-19)
    std::vector<int32_t> keys;
    for (int i = 1; i <= 10; i++) keys.push_back(i);
    for (int i = 20; i <= 30; i++) keys.push_back(i);
    insertKeys(keys);

    // Scan [12, 18] - should be empty
    ErrorContext ctx;
    auto start_key = makeKey(12);
    auto end_key = makeKey(18);
    auto iter = btree_->rangeScan(&start_key, &end_key, 0, true, true, &ctx);
    ASSERT_NE(iter, nullptr);

    // Should have no results
    EXPECT_FALSE(iter->hasNext());
    EXPECT_EQ(iter->getScannedCount(), 0u);
}
