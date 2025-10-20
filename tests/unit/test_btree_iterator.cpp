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
#include <filesystem>
#include <vector>
#include <algorithm>
#include <iostream>

using namespace scratchbird::core;

class BTreeIteratorTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        test_db_path_ = "test_btree_iter.db";
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
    Status createIndex(uint32_t* root_page_out)
    {
        ErrorContext ctx;

        // Create index UUID
        memset(index_uuid_.bytes.data(), 0xAA, 16);
        memset(table_uuid_.bytes.data(), 0xBB, 16);

        // Single column index
        UuidV7Bytes column_uuid;
        memset(column_uuid.bytes.data(), 0xCC, 16);
        std::vector<UuidV7Bytes> columns = {column_uuid};

        return BTree::create(db_.get(), index_uuid_, table_uuid_, columns, root_page_out, &ctx);
    }

    // Helper: Open existing B-Tree
    void openIndex(uint32_t root_page)
    {
        ErrorContext ctx;
        btree_ = BTree::open(db_.get(), index_uuid_, root_page, &ctx);
        ASSERT_NE(btree_, nullptr);
    }

    // Helper: Create a key from an integer
    std::vector<uint8_t> makeKey(int32_t value)
    {
        std::vector<uint8_t> key(sizeof(int32_t));
        *reinterpret_cast<int32_t*>(key.data()) = value;
        return key;
    }

    // Helper: Extract integer from key
    int32_t extractKey(const std::vector<uint8_t>& key)
    {
        if (key.size() < sizeof(int32_t)) return 0;
        return *reinterpret_cast<const int32_t*>(key.data());
    }

    // Helper: Insert multiple keys
    void insertKeys(const std::vector<int32_t>& keys)
    {
        ErrorContext ctx;
        for (int32_t k : keys) {
            auto key = makeKey(k);
            uint64_t tuple_id = static_cast<uint64_t>(k) << 16;
            ASSERT_EQ(btree_->insert(key, tuple_id, &ctx), Status::OK)
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
    uint32_t root_page;
    ASSERT_EQ(createIndex(&root_page), Status::OK);
    openIndex(root_page);

    ErrorContext ctx;
    auto iter = btree_->rangeScan(nullptr, nullptr, nullptr, true, false, &ctx);
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
    uint32_t root_page;
    ASSERT_EQ(createIndex(&root_page), Status::OK);
    openIndex(root_page);

    // Insert 10 keys in random order
    std::vector<int32_t> keys = {5, 2, 8, 1, 9, 3, 7, 4, 6, 10};
    insertKeys(keys);

    // Full scan
    ErrorContext ctx;

    // Debug: Check root page btr_count before scanning
    void* page_buffer;
    ASSERT_EQ(db_->buffer_pool()->pinPage(root_page, &page_buffer, &ctx), Status::OK);
    auto* page = reinterpret_cast<const SBBTreePage*>(page_buffer);
    uint16_t actual_count = page->btr_count;
    db_->buffer_pool()->unpinPage(root_page, false, &ctx);

    // Debug output
    if (actual_count != 10) {
        std::cout << "WARNING: Root page btr_count = " << actual_count << " (expected 10)" << std::endl;
    }

    auto iter = btree_->rangeScan(nullptr, nullptr, nullptr, true, false, &ctx);
    ASSERT_NE(iter, nullptr);

    // Collect scanned keys
    std::vector<int32_t> scanned;
    while (iter->hasNext()) {
        std::vector<uint8_t> key;
        uint64_t tuple_id;
        ASSERT_EQ(iter->next(&key, &tuple_id, &ctx), Status::OK);
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
    uint32_t root_page;
    ASSERT_EQ(createIndex(&root_page), Status::OK);
    openIndex(root_page);

    // Insert enough keys to span multiple pages (100 keys)
    std::vector<int32_t> keys;
    for (int i = 1; i <= 100; i++) {
        keys.push_back(i);
    }
    // Shuffle to force tree growth
    std::random_shuffle(keys.begin(), keys.end());
    insertKeys(keys);

    // Full scan
    ErrorContext ctx;
    auto iter = btree_->rangeScan(nullptr, nullptr, nullptr, true, false, &ctx);
    ASSERT_NE(iter, nullptr);

    // Collect all keys
    std::vector<int32_t> scanned;
    while (iter->hasNext()) {
        std::vector<uint8_t> key;
        uint64_t tuple_id;
        ASSERT_EQ(iter->next(&key, &tuple_id, &ctx), Status::OK);
        scanned.push_back(extractKey(key));
    }

    // Should scan all 100 keys in order
    EXPECT_EQ(scanned.size(), 100u);
    EXPECT_TRUE(std::is_sorted(scanned.begin(), scanned.end()));
    EXPECT_EQ(iter->getScannedCount(), 100u);
}

/**
 * Test: Bounded Scan - Inclusive Start and End
 *
 * Verifies range scan with inclusive bounds [start, end].
 */
TEST_F(BTreeIteratorTest, BoundedScanInclusive)
{
    uint32_t root_page;
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
    auto iter = btree_->rangeScan(&start_key, &end_key, nullptr, true, true, &ctx);
    ASSERT_NE(iter, nullptr);

    // Collect scanned keys
    std::vector<int32_t> scanned;
    while (iter->hasNext()) {
        std::vector<uint8_t> key;
        uint64_t tuple_id;
        ASSERT_EQ(iter->next(&key, &tuple_id, &ctx), Status::OK);
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
    uint32_t root_page;
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
    auto iter = btree_->rangeScan(&start_key, &end_key, nullptr, false, false, &ctx);
    ASSERT_NE(iter, nullptr);

    // Collect scanned keys
    std::vector<int32_t> scanned;
    while (iter->hasNext()) {
        std::vector<uint8_t> key;
        uint64_t tuple_id;
        ASSERT_EQ(iter->next(&key, &tuple_id, &ctx), Status::OK);
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
    uint32_t root_page;
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
    auto iter = btree_->rangeScan(nullptr, &end_key, nullptr, true, true, &ctx);
    ASSERT_NE(iter, nullptr);

    // Collect scanned keys
    std::vector<int32_t> scanned;
    while (iter->hasNext()) {
        std::vector<uint8_t> key;
        uint64_t tuple_id;
        ASSERT_EQ(iter->next(&key, &tuple_id, &ctx), Status::OK);
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
    uint32_t root_page;
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
    auto iter = btree_->rangeScan(&start_key, nullptr, nullptr, true, false, &ctx);
    ASSERT_NE(iter, nullptr);

    // Collect scanned keys
    std::vector<int32_t> scanned;
    while (iter->hasNext()) {
        std::vector<uint8_t> key;
        uint64_t tuple_id;
        ASSERT_EQ(iter->next(&key, &tuple_id, &ctx), Status::OK);
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
    uint32_t root_page;
    ASSERT_EQ(createIndex(&root_page), Status::OK);
    openIndex(root_page);

    ErrorContext ctx;

    // Insert multiple tuple IDs for same key
    auto key5 = makeKey(5);
    auto key10 = makeKey(10);
    auto key15 = makeKey(15);

    // Insert key 5 with 3 tuple IDs
    ASSERT_EQ(btree_->insert(key5, 100, &ctx), Status::OK);
    ASSERT_EQ(btree_->insert(key5, 101, &ctx), Status::OK);
    ASSERT_EQ(btree_->insert(key5, 102, &ctx), Status::OK);

    // Insert key 10 with 2 tuple IDs
    ASSERT_EQ(btree_->insert(key10, 200, &ctx), Status::OK);
    ASSERT_EQ(btree_->insert(key10, 201, &ctx), Status::OK);

    // Insert key 15 with 1 tuple ID
    ASSERT_EQ(btree_->insert(key15, 300, &ctx), Status::OK);

    // Full scan
    auto iter = btree_->rangeScan(nullptr, nullptr, nullptr, true, false, &ctx);
    ASSERT_NE(iter, nullptr);

    // Collect all entries
    int count = 0;
    while (iter->hasNext()) {
        std::vector<uint8_t> key;
        uint64_t tuple_id;
        ASSERT_EQ(iter->next(&key, &tuple_id, &ctx), Status::OK);
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
    uint32_t root_page;
    ASSERT_EQ(createIndex(&root_page), Status::OK);
    openIndex(root_page);

    // Insert 10 keys
    std::vector<int32_t> keys = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    insertKeys(keys);

    ErrorContext ctx;
    auto iter = btree_->rangeScan(nullptr, nullptr, nullptr, true, false, &ctx);
    ASSERT_NE(iter, nullptr);

    // Initially 0
    EXPECT_EQ(iter->getScannedCount(), 0u);

    // Scan 5 items
    for (int i = 0; i < 5 && iter->hasNext(); i++) {
        std::vector<uint8_t> key;
        uint64_t tuple_id;
        ASSERT_EQ(iter->next(&key, &tuple_id, &ctx), Status::OK);
    }
    EXPECT_EQ(iter->getScannedCount(), 5u);

    // Scan remaining items
    while (iter->hasNext()) {
        std::vector<uint8_t> key;
        uint64_t tuple_id;
        ASSERT_EQ(iter->next(&key, &tuple_id, &ctx), Status::OK);
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
    uint32_t root_page;
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
    auto iter = btree_->rangeScan(&start_key, &end_key, nullptr, true, true, &ctx);
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
    uint32_t root_page;
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
    auto iter = btree_->rangeScan(&start_key, &end_key, nullptr, true, true, &ctx);
    ASSERT_NE(iter, nullptr);

    // Should have no results
    EXPECT_FALSE(iter->hasNext());
    EXPECT_EQ(iter->getScannedCount(), 0u);
}
