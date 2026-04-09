/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
// Test suite for B-Tree prefix compression
// Tests compression helpers, integration with B-tree operations, and performance

#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <queue>
#include <set>
#include "test_helpers.h"
#include "scratchbird/core/btree.h"
#include "scratchbird/core/btree_page.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/gpid.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/tid.h"
#include "scratchbird/core/transaction_manager.h"

using namespace scratchbird::core;

// Test fixture for B-Tree compression tests
class BTreeCompressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary database for testing in current directory (allowed by security policy)
        test_db_path_ = scratchbird::testing::uniqueTestDbPath("test_btree_compression", ".db");

        // Remove old test database if it exists
        if (std::filesystem::exists(test_db_path_)) {
            std::filesystem::remove(test_db_path_);
        }

        // Create new database
        ErrorContext ctx;
        Status status = Database::create(test_db_path_, 16384, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to create test database: " << ctx.message;

        // Open database
        db_ = std::make_unique<Database>();
        status = db_->open(test_db_path_, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to open test database: " << ctx.message;

        // Initialize ProcArray for multi-connection support
        status = db_->initializeProcArray(10, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to initialize ProcArray: " << ctx.message;
    }

    void TearDown() override {
        if (db_) {
            db_->close();
            db_.reset();
        }

        // Clean up test database
        if (std::filesystem::exists(test_db_path_)) {
            std::filesystem::remove(test_db_path_);
        }
    }

    // Helper: Create a B-tree index
    std::unique_ptr<BTree> createBTree(ErrorContext* ctx = nullptr) {
        ErrorContext local_ctx;
        if (!ctx) ctx = &local_ctx;

        UuidV7Bytes index_uuid = generateUuidV7();
        UuidV7Bytes table_uuid = generateUuidV7();
        std::vector<UuidV7Bytes> column_uuids = { generateUuidV7() };

        GPID root_gpid = allocateRootGpid(ctx);
        if (root_gpid == 0) {
            return nullptr;
        }

        Status status = BTree::create(db_.get(), index_uuid, table_uuid, column_uuids, root_gpid, ctx);
        if (status != Status::OK) {
            return nullptr;
        }

        return BTree::open(db_.get(), index_uuid, root_gpid, ctx);
    }

    std::unique_ptr<BTree> createBTreeWithRoot(GPID *root_gpid_out, ErrorContext* ctx = nullptr) {
        ErrorContext local_ctx;
        if (!ctx) ctx = &local_ctx;

        UuidV7Bytes index_uuid = generateUuidV7();
        UuidV7Bytes table_uuid = generateUuidV7();
        std::vector<UuidV7Bytes> column_uuids = { generateUuidV7() };

        GPID root_gpid = allocateRootGpid(ctx);
        if (root_gpid == 0) {
            return nullptr;
        }

        if (root_gpid_out) {
            *root_gpid_out = root_gpid;
        }

        Status status = BTree::create(db_.get(), index_uuid, table_uuid, column_uuids, root_gpid, ctx);
        if (status != Status::OK) {
            return nullptr;
        }

        return BTree::open(db_.get(), index_uuid, root_gpid, ctx);
    }

    static TID makeTestTID(uint64_t id) {
        return TID(makeGPID(PRIMARY_TABLESPACE_ID, id), 1);
    }

    GPID allocateRootGpid(ErrorContext* ctx) const {
        if (!db_) {
            if (ctx) ctx->message = "Database not initialized";
            return 0;
        }
        auto *pm = db_->page_manager();
        if (!pm) {
            if (ctx) ctx->message = "PageManager not available";
            return 0;
        }
        GPID gpid = 0;
        Status status = pm->allocatePageInTablespace(PRIMARY_TABLESPACE_ID, &gpid, ctx);
        if (status != Status::OK) {
            return 0;
        }
        return gpid;
    }

    GPID resolveCurrentRootGpid(GPID known_page_gpid, ErrorContext* ctx) const {
        if (!db_ || known_page_gpid == 0) {
            if (ctx) ctx->message = "Cannot resolve root without database and seed page";
            return 0;
        }

        GPID current = known_page_gpid;
        while (current != 0) {
            const uint32_t page_num = static_cast<uint32_t>(getPageNumber(current));
            void *page_buffer = nullptr;
            Status status = db_->buffer_pool()->pinPage(page_num, &page_buffer, ctx);
            if (status != Status::OK) {
                return 0;
            }

            const auto *page = reinterpret_cast<const SBBTreePage *>(page_buffer);
            const uint64_t parent_page_num = page->btr_parent_page;
            const bool is_root =
                (page->btr_flags & static_cast<uint16_t>(BTreeFlags::ROOT)) != 0 ||
                parent_page_num == 0;
            db_->buffer_pool()->unpinPage(page_num, false, ctx);

            if (is_root) {
                return makeGPID(PRIMARY_TABLESPACE_ID, page_num);
            }
            current = makeGPID(PRIMARY_TABLESPACE_ID, parent_page_num);
        }

        return 0;
    }

    uint64_t currentXid() const {
        auto *tm = db_ ? db_->transaction_manager() : nullptr;
        return tm ? tm->getCurrentXid() : 1;
    }

    uint64_t findLeftmostLeafPage(GPID root_gpid, ErrorContext *ctx) const {
        if (!db_ || root_gpid == 0) {
            if (ctx) ctx->message = "Cannot resolve leftmost leaf without database/root";
            return 0;
        }

        uint64_t current_page_num = getPageNumber(root_gpid);
        while (current_page_num != 0) {
            void *page_buffer = nullptr;
            Status status = db_->buffer_pool()->pinPage(static_cast<uint32_t>(current_page_num),
                                                        &page_buffer, ctx);
            if (status != Status::OK) {
                return 0;
            }

            const auto *page = reinterpret_cast<const SBBTreePage *>(page_buffer);
            if ((page->btr_flags & static_cast<uint16_t>(BTreeFlags::LEAF)) != 0) {
                db_->buffer_pool()->unpinPage(static_cast<uint32_t>(current_page_num), false, ctx);
                return current_page_num;
            }

            const auto *page_data = reinterpret_cast<const uint8_t *>(page_buffer);
            const auto *offsets =
                reinterpret_cast<const uint16_t *>(page_data + sizeof(SBBTreePage));
            uint64_t next_page_num = 0;
            if (page->btr_count > 0) {
                const auto *node =
                    reinterpret_cast<const SBBTreeNode *>(page_data + offsets[0]);
                next_page_num = node->btn_child_page;
            } else {
                next_page_num = page->btr_rightmost_child;
            }

            db_->buffer_pool()->unpinPage(static_cast<uint32_t>(current_page_num), false, ctx);
            current_page_num = next_page_num;
        }

        return 0;
    }

    std::vector<std::vector<uint8_t>> collectLeafChainKeys(GPID root_gpid,
                                                           std::vector<uint64_t> *leaf_pages_out,
                                                           ErrorContext *ctx) const {
        std::vector<std::vector<uint8_t>> keys_out;
        if (leaf_pages_out) {
            leaf_pages_out->clear();
        }

        uint64_t current_page_num = findLeftmostLeafPage(root_gpid, ctx);
        if (current_page_num == 0) {
            return keys_out;
        }

        uint64_t previous_page_num = 0;
        while (current_page_num != 0) {
            void *page_buffer = nullptr;
            Status status = db_->buffer_pool()->pinPage(static_cast<uint32_t>(current_page_num),
                                                        &page_buffer, ctx);
            if (status != Status::OK) {
                keys_out.clear();
                return keys_out;
            }

            const auto *page = reinterpret_cast<const SBBTreePage *>(page_buffer);
            if (leaf_pages_out) {
                leaf_pages_out->push_back(current_page_num);
            }

            EXPECT_TRUE((page->btr_flags & static_cast<uint16_t>(BTreeFlags::LEAF)) != 0);
            if (previous_page_num == 0) {
                EXPECT_TRUE((page->btr_flags & static_cast<uint16_t>(BTreeFlags::LEFTMOST)) != 0);
            } else {
                EXPECT_EQ(page->btr_left_sibling, previous_page_num);
                EXPECT_NE(page->btr_parent_page, 0u) << "non-root leaf should keep parent pointer";
            }

            for (uint16_t i = 0; i < page->btr_count; ++i) {
                std::vector<uint8_t> key;
                std::vector<TID> tids;
                status = BTreePage::get_node(reinterpret_cast<const uint8_t *>(page_buffer),
                                             page->btr_header.page_size, i, key, tids);
                EXPECT_EQ(status, Status::OK) << (ctx ? ctx->message : "");
                if (status == Status::OK) {
                    keys_out.push_back(std::move(key));
                }
            }

            const uint64_t next_page_num = page->btr_right_sibling;
            if (next_page_num == 0) {
                EXPECT_TRUE((page->btr_flags & static_cast<uint16_t>(BTreeFlags::RIGHTMOST)) != 0);
            }
            db_->buffer_pool()->unpinPage(static_cast<uint32_t>(current_page_num), false, ctx);
            previous_page_num = current_page_num;
            current_page_num = next_page_num;
        }

        return keys_out;
    }

    std::vector<uint64_t> collectReachablePageNumbers(GPID root_gpid, ErrorContext *ctx) const {
        std::vector<uint64_t> pages_out;
        if (!db_ || root_gpid == 0) {
            if (ctx) ctx->message = "Cannot collect pages without database/root";
            return pages_out;
        }

        std::queue<uint64_t> pending;
        std::set<uint64_t> visited;
        pending.push(getPageNumber(root_gpid));

        while (!pending.empty()) {
            const uint64_t page_num = pending.front();
            pending.pop();
            if (page_num == 0 || !visited.insert(page_num).second) {
                continue;
            }

            void *page_buffer = nullptr;
            Status status = db_->buffer_pool()->pinPage(static_cast<uint32_t>(page_num),
                                                        &page_buffer, ctx);
            if (status != Status::OK) {
                pages_out.clear();
                return pages_out;
            }

            const auto *page = reinterpret_cast<const SBBTreePage *>(page_buffer);
            pages_out.push_back(page_num);

            if ((page->btr_flags & static_cast<uint16_t>(BTreeFlags::LEAF)) == 0) {
                const auto *page_data = reinterpret_cast<const uint8_t *>(page_buffer);
                const auto *offsets =
                    reinterpret_cast<const uint16_t *>(page_data + sizeof(SBBTreePage));
                for (uint16_t i = 0; i < page->btr_count; ++i) {
                    const auto *node =
                        reinterpret_cast<const SBBTreeNode *>(page_data + offsets[i]);
                    pending.push(node->btn_child_page);
                }
                pending.push(page->btr_rightmost_child);
            }

            db_->buffer_pool()->unpinPage(static_cast<uint32_t>(page_num), false, ctx);
        }

        return pages_out;
    }

    // Helper: Generate UUIDv7 keys (high prefix similarity due to timestamp)
    std::vector<std::vector<uint8_t>> generateUUIDv7Keys(size_t count) {
        std::vector<std::vector<uint8_t>> keys;
        keys.reserve(count);

        for (size_t i = 0; i < count; ++i) {
            UuidV7Bytes uuid = generateUuidV7();
            std::vector<uint8_t> key(uuid.bytes.begin(), uuid.bytes.end());
            keys.push_back(key);
        }

        return keys;
    }

    // Helper: Generate string keys with common prefixes
    std::vector<std::vector<uint8_t>> generateStringKeys(const std::string& prefix, size_t count) {
        std::vector<std::vector<uint8_t>> keys;
        keys.reserve(count);

        size_t width = 1;
        if (count > 0) {
            size_t max_value = count - 1;
            while (max_value >= 10) {
                width++;
                max_value /= 10;
            }
        }

        for (size_t i = 0; i < count; ++i) {
            std::ostringstream oss;
            oss << prefix << std::setw(static_cast<int>(width)) << std::setfill('0') << i;
            std::string key_str = oss.str();
            std::vector<uint8_t> key(key_str.begin(), key_str.end());
            keys.push_back(key);
        }

        return keys;
    }

    // Helper: Generate random keys (low compressibility)
    std::vector<std::vector<uint8_t>> generateRandomKeys(size_t count, size_t key_length) {
        std::vector<std::vector<uint8_t>> keys;
        keys.reserve(count);

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);

        for (size_t i = 0; i < count; ++i) {
            std::vector<uint8_t> key;
            key.reserve(key_length);
            for (size_t j = 0; j < key_length; ++j) {
                key.push_back(static_cast<uint8_t>(dis(gen)));
            }
            keys.push_back(key);
        }

        return keys;
    }

    std::string test_db_path_;
    std::unique_ptr<Database> db_;
};

// ============================================================================
// UNIT TESTS: Compression Helper Functions
// ============================================================================

// Note: These test the internal helper functions indirectly through B-tree operations
// since the helpers are static functions in btree.cpp

TEST_F(BTreeCompressionTest, BasicInsertAndSearch) {
    ErrorContext ctx;
    auto btree = createBTree(&ctx);
    ASSERT_NE(btree, nullptr) << "Failed to create B-tree: " << ctx.message;

    // Insert a simple key
    std::vector<uint8_t> key = {0x01, 0x02, 0x03, 0x04};
    uint64_t tuple_id = 1000;

    Status status = btree->insert(key, makeTestTID(tuple_id), 1, &ctx);
    ASSERT_EQ(status, Status::OK) << "Insert failed: " << ctx.message;

    // Search for the key
    std::vector<TID> tuple_ids;
    status = btree->search(key, 0, &tuple_ids, &ctx);
    ASSERT_EQ(status, Status::OK) << "Search failed: " << ctx.message;
    ASSERT_EQ(tuple_ids.size(), 1);
    ASSERT_EQ(tuple_ids[0], makeTestTID(tuple_id));
}

TEST_F(BTreeCompressionTest, ShortKeysNotCompressed) {
    ErrorContext ctx;
    auto btree = createBTree(&ctx);
    ASSERT_NE(btree, nullptr);

    // Insert multiple short keys (< 8 bytes)
    // These should NOT be compressed per heuristic
    std::vector<std::vector<uint8_t>> keys = {
        {0x01, 0x02, 0x03},
        {0x01, 0x02, 0x04},
        {0x01, 0x02, 0x05}
    };

    for (size_t i = 0; i < keys.size(); ++i) {
        Status status = btree->insert(keys[i], makeTestTID(i + 1), 1, &ctx);
        ASSERT_EQ(status, Status::OK) << "Insert " << i << " failed: " << ctx.message;
    }

    // Verify all keys can be found
    for (size_t i = 0; i < keys.size(); ++i) {
        std::vector<TID> tuple_ids;
        Status status = btree->search(keys[i], 0, &tuple_ids, &ctx);
        ASSERT_EQ(status, Status::OK) << "Search " << i << " failed: " << ctx.message;
        ASSERT_EQ(tuple_ids.size(), 1);
        ASSERT_EQ(tuple_ids[0], makeTestTID(i + 1));
    }
}

TEST_F(BTreeCompressionTest, SmallPrefixNotCompressed) {
    ErrorContext ctx;
    auto btree = createBTree(&ctx);
    ASSERT_NE(btree, nullptr);

    // Insert keys with small prefix (< 4 bytes)
    // These should NOT be compressed per heuristic
    std::vector<std::vector<uint8_t>> keys = {
        {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08},
        {0x01, 0x02, 0x03, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E},
        {0x01, 0x02, 0x03, 0x10, 0x11, 0x12, 0x13, 0x14}
    };

    for (size_t i = 0; i < keys.size(); ++i) {
        Status status = btree->insert(keys[i], makeTestTID(i + 100), 1, &ctx);
        ASSERT_EQ(status, Status::OK) << "Insert " << i << " failed: " << ctx.message;
    }

    // Verify all keys can be found
    for (size_t i = 0; i < keys.size(); ++i) {
        std::vector<TID> tuple_ids;
        Status status = btree->search(keys[i], 0, &tuple_ids, &ctx);
        ASSERT_EQ(status, Status::OK) << "Search " << i << " failed: " << ctx.message;
        ASSERT_EQ(tuple_ids.size(), 1);
        ASSERT_EQ(tuple_ids[0], makeTestTID(i + 100));
    }
}

TEST_F(BTreeCompressionTest, LargePrefixShouldCompress) {
    ErrorContext ctx;
    auto btree = createBTree(&ctx);
    ASSERT_NE(btree, nullptr);

    // Insert keys with large common prefix (>= 4 bytes, >= 8 total bytes)
    // These SHOULD be compressed
    std::vector<std::vector<uint8_t>> keys = {
        {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x01, 0x02},
        {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x01, 0x03},
        {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x01, 0x04},
        {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x02, 0x01}
    };

    for (size_t i = 0; i < keys.size(); ++i) {
        Status status = btree->insert(keys[i], makeTestTID(i + 200), 1, &ctx);
        ASSERT_EQ(status, Status::OK) << "Insert " << i << " failed: " << ctx.message;
    }

    // Verify all keys can be found (decompression works)
    for (size_t i = 0; i < keys.size(); ++i) {
        std::vector<TID> tuple_ids;
        Status status = btree->search(keys[i], 0, &tuple_ids, &ctx);
        ASSERT_EQ(status, Status::OK) << "Search " << i << " failed: " << ctx.message;
        ASSERT_EQ(tuple_ids.size(), 1);
        ASSERT_EQ(tuple_ids[0], makeTestTID(i + 200));
    }
}

// ============================================================================
// INTEGRATION TESTS: B-Tree Operations with Compression
// ============================================================================

TEST_F(BTreeCompressionTest, UUIDv7KeysCompression) {
    ErrorContext ctx;
    auto btree = createBTree(&ctx);
    ASSERT_NE(btree, nullptr);

    // Generate 100 UUIDv7 keys (high temporal locality = good compression)
    auto keys = generateUUIDv7Keys(100);

    // Insert all keys
    for (size_t i = 0; i < keys.size(); ++i) {
        Status status = btree->insert(keys[i], makeTestTID(i + 1000), 1, &ctx);
        ASSERT_EQ(status, Status::OK) << "Insert " << i << " failed: " << ctx.message;
    }

    // Verify all keys can be found
    for (size_t i = 0; i < keys.size(); ++i) {
        std::vector<TID> tuple_ids;
        Status status = btree->search(keys[i], 0, &tuple_ids, &ctx);
        ASSERT_EQ(status, Status::OK) << "Search " << i << " failed: " << ctx.message;
        ASSERT_EQ(tuple_ids.size(), 1);
        ASSERT_EQ(tuple_ids[0], makeTestTID(i + 1000));
    }
}

TEST_F(BTreeCompressionTest, StringKeysWithCommonPrefix) {
    ErrorContext ctx;
    auto btree = createBTree(&ctx);
    ASSERT_NE(btree, nullptr);

    // Generate string keys with common prefix
    auto keys = generateStringKeys("user_profile_", 50);

    // Insert all keys
    for (size_t i = 0; i < keys.size(); ++i) {
        Status status = btree->insert(keys[i], makeTestTID(i + 2000), 1, &ctx);
        ASSERT_EQ(status, Status::OK) << "Insert " << i << " failed: " << ctx.message;
    }

    // Verify all keys can be found
    for (size_t i = 0; i < keys.size(); ++i) {
        std::vector<TID> tuple_ids;
        Status status = btree->search(keys[i], 0, &tuple_ids, &ctx);
        ASSERT_EQ(status, Status::OK) << "Search " << i << " failed: " << ctx.message;
        ASSERT_EQ(tuple_ids.size(), 1);
        ASSERT_EQ(tuple_ids[0], makeTestTID(i + 2000));
    }
}

TEST_F(BTreeCompressionTest, LeafRestartAnchorsBoundCompressedSearchBlocks) {
    ErrorContext ctx;
    GPID root_gpid = 0;
    auto btree = createBTreeWithRoot(&root_gpid, &ctx);
    ASSERT_NE(btree, nullptr) << "Failed to create B-tree: " << ctx.message;

    auto keys = generateStringKeys("customer-order-region-", 40);
    for (size_t i = 0; i < keys.size(); ++i) {
        Status status = btree->insert(keys[i], makeTestTID(i + 1), currentXid(), &ctx);
        ASSERT_EQ(status, Status::OK) << "Insert " << i << " failed: " << ctx.message;
    }

    void *page_buffer = nullptr;
    const uint32_t root_page_num = static_cast<uint32_t>(getPageNumber(root_gpid));
    ASSERT_EQ(db_->buffer_pool()->pinPage(root_page_num, &page_buffer, &ctx), Status::OK)
        << ctx.message;

    const auto *page = reinterpret_cast<const SBBTreePage *>(page_buffer);
    ASSERT_TRUE((page->btr_flags & static_cast<uint16_t>(BTreeFlags::LEAF)) != 0);
    ASSERT_GE(page->btr_count, 16);

    const auto *page_data = reinterpret_cast<const uint8_t *>(page_buffer);
    const auto *offsets = reinterpret_cast<const uint16_t *>(page_data + sizeof(SBBTreePage));
    size_t restart_anchor_count = 0;
    size_t compressed_node_count = 0;

    for (uint16_t i = 0; i < page->btr_count; ++i) {
        const auto *node = reinterpret_cast<const SBBTreeNode *>(page_data + offsets[i]);
        if (node->btn_prefix_len == 0) {
            restart_anchor_count++;
        } else {
            compressed_node_count++;
        }

        if ((i % 8) == 0) {
            EXPECT_EQ(node->btn_prefix_len, 0) << "restart anchor missing at node " << i;
        }
    }

    EXPECT_GT(restart_anchor_count, 1u);
    EXPECT_GT(compressed_node_count, 0u);
    db_->buffer_pool()->unpinPage(root_page_num, false, &ctx);
}

TEST_F(BTreeCompressionTest, SearchAcrossRestartAnchorsAndSplitPages) {
    ErrorContext ctx;
    auto btree = createBTree(&ctx);
    ASSERT_NE(btree, nullptr) << "Failed to create B-tree: " << ctx.message;

    auto keys = generateStringKeys("customer-order-region-", 512);
    for (size_t i = 0; i < keys.size(); ++i) {
        Status status = btree->insert(keys[i], makeTestTID(i + 1000), currentXid(), &ctx);
        ASSERT_EQ(status, Status::OK) << "Insert " << i << " failed: " << ctx.message;
    }

    for (size_t index : {size_t(0), size_t(7), size_t(8), size_t(63), size_t(64), size_t(255), size_t(511)}) {
        std::vector<TID> tuple_ids;
        Status status = btree->search(keys[index], 0, &tuple_ids, &ctx);
        ASSERT_EQ(status, Status::OK) << "Search failed for key " << index << ": " << ctx.message;
        ASSERT_EQ(tuple_ids.size(), 1u);
        EXPECT_EQ(tuple_ids[0], makeTestTID(index + 1000));
    }

    std::vector<uint8_t> missing_key{'c','u','s','t','o','m','e','r','-','o','r','d','e','r','-',
                                     'r','e','g','i','o','n','-','9','9','9','9'};
    std::vector<TID> tuple_ids;
    Status status = btree->search(missing_key, 0, &tuple_ids, &ctx);
    ASSERT_EQ(status, Status::NOT_FOUND) << "Missing-key search failed: " << ctx.message;
    EXPECT_TRUE(tuple_ids.empty());
}

TEST_F(BTreeCompressionTest, InternalSeparatorsTruncateAndCompressWithRestartAnchors) {
    ErrorContext ctx;
    GPID seed_root_gpid = 0;
    auto btree = createBTreeWithRoot(&seed_root_gpid, &ctx);
    ASSERT_NE(btree, nullptr) << "Failed to create B-tree: " << ctx.message;

    std::vector<std::vector<uint8_t>> keys;
    keys.reserve(2048);
    for (size_t i = 0; i < 2048; ++i) {
        std::ostringstream oss;
        oss << "customer-region-" << std::setw(4) << std::setfill('0') << i
            << "-payload-shared-tail";
        const std::string key_str = oss.str();
        keys.emplace_back(key_str.begin(), key_str.end());
    }

    for (size_t i = 0; i < keys.size(); ++i) {
        Status status = btree->insert(keys[i], makeTestTID(i + 6000), currentXid(), &ctx);
        ASSERT_EQ(status, Status::OK) << "Insert " << i << " failed: " << ctx.message;
    }

    const GPID root_gpid = resolveCurrentRootGpid(seed_root_gpid, &ctx);
    ASSERT_NE(root_gpid, 0) << "Failed to resolve current root: " << ctx.message;

    void *page_buffer = nullptr;
    const uint32_t root_page_num = static_cast<uint32_t>(getPageNumber(root_gpid));
    ASSERT_EQ(db_->buffer_pool()->pinPage(root_page_num, &page_buffer, &ctx), Status::OK)
        << ctx.message;

    const auto *page = reinterpret_cast<const SBBTreePage *>(page_buffer);
    ASSERT_FALSE((page->btr_flags & static_cast<uint16_t>(BTreeFlags::LEAF)) != 0);
    ASSERT_GE(page->btr_count, 8);

    const auto *page_data = reinterpret_cast<const uint8_t *>(page_buffer);
    const auto *offsets = reinterpret_cast<const uint16_t *>(page_data + sizeof(SBBTreePage));
    size_t prefix_compressed_nodes = 0;
    size_t suffix_truncated_nodes = 0;
    size_t restart_anchor_nodes = 0;

    for (uint16_t i = 0; i < page->btr_count; ++i) {
        const auto *node = reinterpret_cast<const SBBTreeNode *>(page_data + offsets[i]);
        if (node->btn_prefix_len == 0) {
            restart_anchor_nodes++;
        } else {
            prefix_compressed_nodes++;
        }
        if (node->btn_suffix_trunc > 0) {
            suffix_truncated_nodes++;
        }
        if ((i % 8) == 0) {
            EXPECT_EQ(node->btn_prefix_len, 0) << "missing internal restart anchor at " << i;
        }
    }

    EXPECT_GT(page->btr_prefix_total, 0u);
    // Beta 1 keeps canonical full internal separators for routing correctness,
    // so internal restart anchors still drive prefix compression while suffix
    // truncation remains intentionally disabled on parent separators.
    EXPECT_EQ(page->btr_suffix_total, 0u);
    EXPECT_GT(prefix_compressed_nodes, 0u);
    EXPECT_EQ(suffix_truncated_nodes, 0u);
    EXPECT_GT(restart_anchor_nodes, 1u);
    db_->buffer_pool()->unpinPage(root_page_num, false, &ctx);

    for (size_t index : {size_t(0), size_t(255), size_t(256), size_t(1023), size_t(1024), size_t(2047)}) {
        std::vector<TID> tuple_ids;
        Status status = btree->search(keys[index], 0, &tuple_ids, &ctx);
        ASSERT_EQ(status, Status::OK) << "Search failed for key " << index << ": " << ctx.message;
        ASSERT_EQ(tuple_ids.size(), 1u);
        EXPECT_EQ(tuple_ids[0], makeTestTID(index + 6000));
    }
}

TEST_F(BTreeCompressionTest, SplitPropagationPreservesLeafChainAndParentPointers) {
    ErrorContext ctx;
    GPID seed_root_gpid = 0;
    auto btree = createBTreeWithRoot(&seed_root_gpid, &ctx);
    ASSERT_NE(btree, nullptr) << "Failed to create B-tree: " << ctx.message;

    auto keys = generateStringKeys("hot-account-prefix-", 2048);
    for (size_t i = 0; i < keys.size(); ++i) {
        Status status = btree->insert(keys[i], makeTestTID(i + 9000), currentXid(), &ctx);
        ASSERT_EQ(status, Status::OK) << "Insert " << i << " failed: " << ctx.message;
    }

    const GPID root_gpid = resolveCurrentRootGpid(seed_root_gpid, &ctx);
    ASSERT_NE(root_gpid, 0) << "Failed to resolve current root: " << ctx.message;

    std::vector<uint64_t> leaf_pages;
    const auto leaf_keys = collectLeafChainKeys(root_gpid, &leaf_pages, &ctx);
    ASSERT_EQ(leaf_keys.size(), keys.size());
    ASSERT_GT(leaf_pages.size(), 1u);

    for (size_t i = 0; i < keys.size(); ++i) {
        EXPECT_EQ(leaf_keys[i], keys[i]) << "leaf chain key mismatch at " << i;
    }

    void *root_buffer = nullptr;
    ASSERT_EQ(db_->buffer_pool()->pinPage(static_cast<uint32_t>(getPageNumber(root_gpid)),
                                          &root_buffer, &ctx),
              Status::OK)
        << ctx.message;
    const auto *root_page = reinterpret_cast<const SBBTreePage *>(root_buffer);
    EXPECT_FALSE((root_page->btr_flags & static_cast<uint16_t>(BTreeFlags::LEAF)) != 0);
    db_->buffer_pool()->unpinPage(static_cast<uint32_t>(getPageNumber(root_gpid)), false, &ctx);
}

TEST_F(BTreeCompressionTest, SplitGrownPagesKeepCanonicalHeadersAndRejectAbsentKeys) {
    ErrorContext ctx;
    GPID seed_root_gpid = 0;
    auto btree = createBTreeWithRoot(&seed_root_gpid, &ctx);
    ASSERT_NE(btree, nullptr) << "Failed to create B-tree: " << ctx.message;

    constexpr size_t kNumKeys = 24000;
    std::vector<std::vector<uint8_t>> keys;
    keys.reserve(kNumKeys);

    for (size_t i = 0; i < kNumKeys; ++i) {
        std::ostringstream oss;
        oss << "split-header-regression-" << std::setw(8) << std::setfill('0') << (i * 2)
            << "-payload";
        const std::string key_str = oss.str();
        keys.emplace_back(key_str.begin(), key_str.end());

        Status status = btree->insert(keys.back(), makeTestTID(i + 20000), currentXid(), &ctx);
        ASSERT_EQ(status, Status::OK) << "Insert " << i << " failed: " << ctx.message;
    }

    for (size_t probe : {size_t(0), size_t(7), size_t(255), size_t(4095), size_t(8191),
                         size_t(16383), kNumKeys - 1}) {
        std::vector<TID> tuple_ids;
        Status status = btree->search(keys[probe], 0, &tuple_ids, &ctx);
        ASSERT_EQ(status, Status::OK) << "Present-key search failed at " << probe
                                      << ": " << ctx.message;
        ASSERT_EQ(tuple_ids.size(), 1u);
        EXPECT_EQ(tuple_ids[0], makeTestTID(probe + 20000));

        std::ostringstream missing_oss;
        missing_oss << "split-header-regression-" << std::setw(8) << std::setfill('0')
                    << ((probe * 2) + 1) << "-payload";
        const std::string missing_str = missing_oss.str();
        std::vector<uint8_t> missing_key(missing_str.begin(), missing_str.end());
        tuple_ids.clear();
        status = btree->search(missing_key, 0, &tuple_ids, &ctx);
        ASSERT_EQ(status, Status::NOT_FOUND) << "Absent-key search matched at " << probe
                                             << ": " << ctx.message;
        EXPECT_TRUE(tuple_ids.empty());
    }

    const GPID root_gpid = resolveCurrentRootGpid(seed_root_gpid, &ctx);
    ASSERT_NE(root_gpid, 0) << "Failed to resolve current root: " << ctx.message;

    const auto page_numbers = collectReachablePageNumbers(root_gpid, &ctx);
    ASSERT_GT(page_numbers.size(), 4u);

    const ID expected_db_uuid = db_->uuid();
    for (uint64_t page_num : page_numbers) {
        void *page_buffer = nullptr;
        ASSERT_EQ(db_->buffer_pool()->pinPage(static_cast<uint32_t>(page_num), &page_buffer, &ctx),
                  Status::OK)
            << ctx.message;

        const auto *page = reinterpret_cast<const SBBTreePage *>(page_buffer);
        const uint16_t expected_page_type =
            (page->btr_flags & static_cast<uint16_t>(BTreeFlags::LEAF)) != 0
                ? static_cast<uint16_t>(PageType::PAGE_TYPE_BTREE_LEAF)
                : static_cast<uint16_t>(PageType::PAGE_TYPE_BTREE_INTERNAL);
        EXPECT_EQ(page->btr_header.page_id, page_num);
        EXPECT_EQ(page->btr_header.header_bytes, CANONICAL_PAGE_HEADER_BYTES);
        EXPECT_EQ(validatePageHeaderContract(page->btr_header, db_->page_size(),
                                             expected_page_type, &expected_db_uuid,
                                             &page->btr_index_uuid),
                  Status::OK)
            << "invalid page header on page " << page_num;

        db_->buffer_pool()->unpinPage(static_cast<uint32_t>(page_num), false, &ctx);
    }
}

TEST_F(BTreeCompressionTest, HotInsertPathPublishesLockActivityWithoutDeadlocks) {
    ErrorContext ctx;
    auto btree = createBTree(&ctx);
    ASSERT_NE(btree, nullptr) << "Failed to create B-tree: " << ctx.message;

    LockStats before{};
    db_->lock_manager()->getStatistics(&before);

    auto keys = generateStringKeys("hot-lock-prefix-", 1536);
    for (size_t i = 0; i < keys.size(); ++i) {
        Status status = btree->insert(keys[i], makeTestTID(i + 12000), currentXid(), &ctx);
        ASSERT_EQ(status, Status::OK) << "Insert " << i << " failed: " << ctx.message;
    }

    LockStats after{};
    db_->lock_manager()->getStatistics(&after);

    EXPECT_GT(after.locks_acquired, before.locks_acquired);
    EXPECT_GE(after.locks_released, before.locks_released);
    EXPECT_EQ(after.deadlocks_detected, before.deadlocks_detected);
    EXPECT_EQ(after.lock_timeouts, before.lock_timeouts);

    for (size_t index : {size_t(0), size_t(127), size_t(255), size_t(511), size_t(1023), size_t(1535)}) {
        std::vector<TID> tuple_ids;
        Status status = btree->search(keys[index], 0, &tuple_ids, &ctx);
        ASSERT_EQ(status, Status::OK) << "Search failed for key " << index << ": " << ctx.message;
        ASSERT_EQ(tuple_ids.size(), 1u);
        EXPECT_EQ(tuple_ids[0], makeTestTID(index + 12000));
    }
}

TEST_F(BTreeCompressionTest, MixedCompressibleAndNonCompressible) {
    ErrorContext ctx;
    auto btree = createBTree(&ctx);
    ASSERT_NE(btree, nullptr);

    // Mix of compressible and non-compressible keys
    std::vector<std::vector<uint8_t>> keys;

    // Add short keys (not compressible)
    for (int i = 0; i < 10; ++i) {
        keys.push_back({0x01, static_cast<uint8_t>(i)});
    }

    // Add keys with large common prefix (compressible)
    for (int i = 0; i < 10; ++i) {
        std::vector<uint8_t> key = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, static_cast<uint8_t>(i)};
        keys.push_back(key);
    }

    // Add random keys (not compressible)
    auto random_keys = generateRandomKeys(10, 16);
    keys.insert(keys.end(), random_keys.begin(), random_keys.end());

    // Insert all keys
    for (size_t i = 0; i < keys.size(); ++i) {
        Status status = btree->insert(keys[i], makeTestTID(i + 3000), 1, &ctx);
        ASSERT_EQ(status, Status::OK) << "Insert " << i << " failed: " << ctx.message;
    }

    // Verify all keys can be found
    for (size_t i = 0; i < keys.size(); ++i) {
        std::vector<TID> tuple_ids;
        Status status = btree->search(keys[i], 0, &tuple_ids, &ctx);
        ASSERT_EQ(status, Status::OK) << "Search " << i << " failed: " << ctx.message;
        ASSERT_EQ(tuple_ids.size(), 1);
        ASSERT_EQ(tuple_ids[0], makeTestTID(i + 3000));
    }
}

TEST_F(BTreeCompressionTest, RangeScanWithCompression) {
    ErrorContext ctx;
    auto btree = createBTree(&ctx);
    ASSERT_NE(btree, nullptr);

    // Generate sorted keys with common prefix
    auto keys = generateStringKeys("record_", 20);

    // Insert keys
    for (size_t i = 0; i < keys.size(); ++i) {
        Status status = btree->insert(keys[i], makeTestTID(i + 4000), 1, &ctx);
        ASSERT_EQ(status, Status::OK) << "Insert " << i << " failed";
    }

    // Perform range scan (should decompress correctly during iteration)
    uint64_t current_xid = currentXid();
    auto iterator = btree->rangeScan(&keys[5], &keys[15], current_xid, true, false, &ctx);
    ASSERT_NE(iterator, nullptr) << "Range scan failed";

    int count = 0;
    while (iterator->hasNext()) {
        std::vector<uint8_t> key_out;
        TID tuple_id_out;
        Status status = iterator->next(&key_out, &tuple_id_out, &ctx);
        ASSERT_EQ(status, Status::OK);
        count++;
    }

    // Should have scanned keys[5] through keys[14] (10 keys)
    EXPECT_EQ(count, 10);
}

TEST_F(BTreeCompressionTest, RemoveWithCompression) {
    ErrorContext ctx;
    auto btree = createBTree(&ctx);
    ASSERT_NE(btree, nullptr);

    // Insert keys with common prefix
    auto keys = generateStringKeys("delete_test_", 10);

    for (size_t i = 0; i < keys.size(); ++i) {
        Status status = btree->insert(keys[i], makeTestTID(i + 5000), 1, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    // Remove some keys (tests decompression during removal)
    uint64_t current_xid = currentXid();
    Status status = btree->remove(keys[3], makeTestTID(5003), current_xid, &ctx);
    ASSERT_EQ(status, Status::OK) << "Remove failed: " << ctx.message;

    status = btree->remove(keys[7], makeTestTID(5007), current_xid, &ctx);
    ASSERT_EQ(status, Status::OK) << "Remove failed: " << ctx.message;

    // Verify removed keys are not found
    std::vector<TID> tuple_ids;
    status = btree->search(keys[3], current_xid, &tuple_ids, &ctx);
    EXPECT_EQ(status, Status::NOT_FOUND);

    tuple_ids.clear();
    status = btree->search(keys[7], current_xid, &tuple_ids, &ctx);
    EXPECT_EQ(status, Status::NOT_FOUND);

    // Verify remaining keys are still found
    for (size_t i = 0; i < keys.size(); ++i) {
        if (i == 3 || i == 7) continue;

        tuple_ids.clear();
        status = btree->search(keys[i], current_xid, &tuple_ids, &ctx);
        ASSERT_EQ(status, Status::OK) << "Search after remove failed for key " << i;
        ASSERT_EQ(tuple_ids.size(), 1);
        ASSERT_EQ(tuple_ids[0], makeTestTID(i + 5000));
    }
}

// ============================================================================
// PERFORMANCE BENCHMARKS
// ============================================================================

TEST_F(BTreeCompressionTest, BenchmarkUUIDv7Compression) {
    ErrorContext ctx;
    auto btree = createBTree(&ctx);
    ASSERT_NE(btree, nullptr);

    const size_t NUM_KEYS = 1000;
    auto keys = generateUUIDv7Keys(NUM_KEYS);

    // Benchmark insert performance
    auto start_insert = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < keys.size(); ++i) {
        Status status = btree->insert(keys[i], TID(PRIMARY_TABLESPACE_ID, 1, static_cast<uint16_t>(i % 65536)), 1, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    auto end_insert = std::chrono::high_resolution_clock::now();
    auto insert_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_insert - start_insert);

    std::cout << "\n=== UUIDv7 Compression Benchmark ===\n";
    std::cout << "Keys inserted: " << NUM_KEYS << "\n";
    std::cout << "Total insert time: " << insert_duration.count() << " μs\n";
    std::cout << "Average per insert: " << (insert_duration.count() / NUM_KEYS) << " μs\n";

    // Benchmark search performance
    auto start_search = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < keys.size(); ++i) {
        std::vector<TID> tuple_ids;
        Status status = btree->search(keys[i], 0, &tuple_ids, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    auto end_search = std::chrono::high_resolution_clock::now();
    auto search_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_search - start_search);

    std::cout << "Total search time: " << search_duration.count() << " μs\n";
    std::cout << "Average per search: " << (search_duration.count() / NUM_KEYS) << " μs\n";
    std::cout << "=============================\n\n";
}

TEST_F(BTreeCompressionTest, BenchmarkStringKeyCompression) {
    ErrorContext ctx;
    auto btree = createBTree(&ctx);
    ASSERT_NE(btree, nullptr);

    const size_t NUM_KEYS = 1000;
    auto keys = generateStringKeys("benchmark_long_prefix_", NUM_KEYS);

    // Benchmark insert performance
    auto start_insert = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < keys.size(); ++i) {
        Status status = btree->insert(keys[i], TID(PRIMARY_TABLESPACE_ID, 1, static_cast<uint16_t>(i % 65536)), 1, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    auto end_insert = std::chrono::high_resolution_clock::now();
    auto insert_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_insert - start_insert);

    std::cout << "\n=== String Key Compression Benchmark ===\n";
    std::cout << "Keys inserted: " << NUM_KEYS << "\n";
    std::cout << "Total insert time: " << insert_duration.count() << " μs\n";
    std::cout << "Average per insert: " << (insert_duration.count() / NUM_KEYS) << " μs\n";

    // Benchmark search performance
    auto start_search = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < keys.size(); ++i) {
        std::vector<TID> tuple_ids;
        Status status = btree->search(keys[i], 0, &tuple_ids, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    auto end_search = std::chrono::high_resolution_clock::now();
    auto search_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_search - start_search);

    std::cout << "Total search time: " << search_duration.count() << " μs\n";
    std::cout << "Average per search: " << (search_duration.count() / NUM_KEYS) << " μs\n";
    std::cout << "=============================\n\n";
}

TEST_F(BTreeCompressionTest, BenchmarkRandomKeyNoCompression) {
    ErrorContext ctx;
    auto btree = createBTree(&ctx);
    ASSERT_NE(btree, nullptr);

    const size_t NUM_KEYS = 1000;
    auto keys = generateRandomKeys(NUM_KEYS, 16);

    // Benchmark insert performance
    auto start_insert = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < keys.size(); ++i) {
        Status status = btree->insert(keys[i], TID(PRIMARY_TABLESPACE_ID, 1, static_cast<uint16_t>(i % 65536)), 1, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    auto end_insert = std::chrono::high_resolution_clock::now();
    auto insert_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_insert - start_insert);

    std::cout << "\n=== Random Key (No Compression) Benchmark ===\n";
    std::cout << "Keys inserted: " << NUM_KEYS << "\n";
    std::cout << "Total insert time: " << insert_duration.count() << " μs\n";
    std::cout << "Average per insert: " << (insert_duration.count() / NUM_KEYS) << " μs\n";

    // Benchmark search performance
    auto start_search = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < keys.size(); ++i) {
        std::vector<TID> tuple_ids;
        Status status = btree->search(keys[i], 0, &tuple_ids, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    auto end_search = std::chrono::high_resolution_clock::now();
    auto search_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_search - start_search);

    std::cout << "Total search time: " << search_duration.count() << " μs\n";
    std::cout << "Average per search: " << (search_duration.count() / NUM_KEYS) << " μs\n";
    std::cout << "=============================\n\n";
}

// ============================================================================
// EDGE CASES AND STRESS TESTS
// ============================================================================

TEST_F(BTreeCompressionTest, EmptyKeyHandling) {
    ErrorContext ctx;
    auto btree = createBTree(&ctx);
    ASSERT_NE(btree, nullptr);

    // Insert empty key
    std::vector<uint8_t> empty_key;
    TID tid1(PRIMARY_TABLESPACE_ID, 1, 9999);
    Status status = btree->insert(empty_key, tid1, 1, &ctx);
    ASSERT_EQ(status, Status::OK) << "Empty key insert failed: " << ctx.message;

    // Search for empty key
    std::vector<TID> tuple_ids;
    status = btree->search(empty_key, 0, &tuple_ids, &ctx);
    ASSERT_EQ(status, Status::OK) << "Empty key search failed: " << ctx.message;
    ASSERT_EQ(tuple_ids.size(), 1);
    ASSERT_EQ(tuple_ids[0], tid1);
}

TEST_F(BTreeCompressionTest, IdenticalKeys) {
    ErrorContext ctx;
    auto btree = createBTree(&ctx);
    ASSERT_NE(btree, nullptr);

    // Insert same key with different tuple IDs (duplicate keys)
    std::vector<uint8_t> key = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x01};

    TID tid1(PRIMARY_TABLESPACE_ID, 1, 1001);
    TID tid2(PRIMARY_TABLESPACE_ID, 1, 1002);
    TID tid3(PRIMARY_TABLESPACE_ID, 1, 1003);

    Status status = btree->insert(key, tid1, 1, &ctx);
    ASSERT_EQ(status, Status::OK);

    status = btree->insert(key, tid2, 1, &ctx);
    ASSERT_EQ(status, Status::OK);

    status = btree->insert(key, tid3, 1, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Search should find all tuple IDs
    std::vector<TID> tuple_ids;
    status = btree->search(key, 0, &tuple_ids, &ctx);
    ASSERT_EQ(status, Status::OK);
    ASSERT_EQ(tuple_ids.size(), 3);
}

TEST_F(BTreeCompressionTest, LargeScaleStressTest) {
    ErrorContext ctx;
    auto btree = createBTree(&ctx);
    ASSERT_NE(btree, nullptr);

    // Insert 10,000 keys to trigger page splits and test compression across splits
    const size_t NUM_KEYS = 10000;
    auto keys = generateUUIDv7Keys(NUM_KEYS);

    std::cout << "\n=== Large Scale Stress Test ===\n";
    std::cout << "Inserting " << NUM_KEYS << " UUIDv7 keys...\n";

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < keys.size(); ++i) {
        TID tid(PRIMARY_TABLESPACE_ID, 1, static_cast<uint16_t>(i % 65536));
        Status status = btree->insert(keys[i], tid, 1, &ctx);
        ASSERT_EQ(status, Status::OK) << "Insert failed at key " << i;

        if ((i + 1) % 1000 == 0) {
            std::cout << "  Inserted " << (i + 1) << " keys...\n";
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Insertion complete in " << duration.count() << " ms\n";
    std::cout << "Average: " << (duration.count() * 1000.0 / NUM_KEYS) << " μs per insert\n";

    // Verify random sample of keys
    std::cout << "Verifying random sample...\n";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, NUM_KEYS - 1);

    for (int i = 0; i < 100; ++i) {
        size_t idx = dis(gen);
        std::vector<TID> tuple_ids;
        Status status = btree->search(keys[idx], 0, &tuple_ids, &ctx);
        ASSERT_EQ(status, Status::OK) << "Search failed for key " << idx;
        ASSERT_EQ(tuple_ids.size(), 1);
        TID expected_tid(PRIMARY_TABLESPACE_ID, 1, static_cast<uint16_t>(idx % 65536));
        ASSERT_EQ(tuple_ids[0], expected_tid);
    }

    std::cout << "Stress test PASSED\n";
    std::cout << "=============================\n\n";
}
