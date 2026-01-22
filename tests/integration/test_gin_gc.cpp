/**
 * Plan 01 Task D.6: GIN Index Garbage Collection Tests
 *
 * Tests the GIN index garbage collection implementation:
 * - Step 1: Pending list cleanup (marks dead TIDs as INVALID_TID) - COMPLETE
 * - Step 2: Posting tree/list pruning (deferred to future optimization)
 *
 * Test coverage:
 * 1. Basic GC: Remove dead entries from pending list
 * 2. Empty index GC: No-op on empty index
 * 3. All entries dead: Verify all removed
 * 4. No dead entries: No-op when nothing to remove
 */

#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/core/gin_index.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/types.h"
#include "scratchbird/core/uuidv7.h"
#include <filesystem>
#include <memory>

using namespace scratchbird::core;

namespace {

class GinGCTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create temporary database
        test_db_path_ = "/tmp/test_gin_gc.db";
        if (std::filesystem::exists(test_db_path_))
        {
            std::filesystem::remove(test_db_path_);
        }

        // Create and open database
        db_ = std::make_unique<Database>();
        Status status = db_->create(test_db_path_, 16384, nullptr);
        ASSERT_EQ(status, Status::OK) << "Failed to create database";

        status = db_->open(test_db_path_, nullptr);
        ASSERT_EQ(status, Status::OK) << "Failed to open database";
    }

    void TearDown() override
    {
        if (db_)
        {
            db_->close();
            db_.reset();
        }

        if (std::filesystem::exists(test_db_path_))
        {
            std::filesystem::remove(test_db_path_);
        }
    }

    // Helper: Simple key extractor for strings
    static auto stringKeyExtractor(const void *data, size_t len) -> std::vector<std::vector<uint8_t>>
    {
        std::vector<std::vector<uint8_t>> keys;
        std::string text(static_cast<const char *>(data), len);

        // Split on spaces (simple tokenization)
        std::string token;
        for (char c : text)
        {
            if (c == ' ')
            {
                if (!token.empty())
                {
                    keys.push_back(std::vector<uint8_t>(token.begin(), token.end()));
                    token.clear();
                }
            }
            else
            {
                token += c;
            }
        }
        if (!token.empty())
        {
            keys.push_back(std::vector<uint8_t>(token.begin(), token.end()));
        }

        return keys;
    }

    std::unique_ptr<Database> db_;
    std::string test_db_path_;
};

/**
 * Test 1: Basic GC - Remove dead entries from pending list
 *
 * This test verifies that dead TIDs are removed from the GIN pending list.
 */
TEST_F(GinGCTest, BasicPendingListGC)
{
    // Generate UUID for GIN index
    UuidV7Bytes index_uuid = generateUuidV7();

    // Create GIN index
    GPID meta_gpid = 0;
    Status status = db_->page_manager()->allocatePageInTablespace(0, &meta_gpid, nullptr);
    ASSERT_EQ(status, Status::OK) << "Failed to allocate GIN meta page";
    uint32_t meta_page = static_cast<uint32_t>(getPageNumber(meta_gpid));
    status = GinIndex::create(db_.get(), index_uuid, meta_gpid, nullptr);
    ASSERT_EQ(status, Status::OK) << "Failed to create GIN index";
    ASSERT_GT(meta_page, 0);

    // Open index
    auto gin = GinIndex::open(db_.get(), index_uuid, meta_gpid, nullptr);
    ASSERT_NE(gin, nullptr) << "Failed to open GIN index";

    // Insert some documents with different TIDs
    std::vector<TID> inserted_tids;

    for (uint16_t i = 0; i < 10; i++)
    {
        // TID constructor: TID(tablespace_id, page_number, slot)
        TID tid(0, i + 1, i);  // tablespace 0, page i+1, slot i
        inserted_tids.push_back(tid);

        std::string doc = "word" + std::to_string(i);
        status = gin->insert(doc.c_str(), doc.size(), tid, stringKeyExtractor, nullptr);
        ASSERT_EQ(status, Status::OK);
    }

    // Mark TIDs 2, 4, 6, 8 as dead
    std::vector<TID> dead_tids = {inserted_tids[2], inserted_tids[4],
                                   inserted_tids[6], inserted_tids[8]};

    // Run GC
    uint64_t entries_removed = 0;
    uint64_t pages_modified = 0;
    status = gin->removeDeadEntries(dead_tids, &entries_removed, &pages_modified, nullptr);

    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(entries_removed, 4); // 4 dead entries removed
    EXPECT_GT(pages_modified, 0);  // At least pending pages modified
}

/**
 * Test 2: GC on empty index (no-op)
 */
TEST_F(GinGCTest, EmptyIndexGC)
{
    // Generate UUID for GIN index
    UuidV7Bytes index_uuid = generateUuidV7();

    // Create GIN index
    GPID meta_gpid = 0;
    Status status = db_->page_manager()->allocatePageInTablespace(0, &meta_gpid, nullptr);
    ASSERT_EQ(status, Status::OK) << "Failed to allocate GIN meta page";
    uint32_t meta_page = static_cast<uint32_t>(getPageNumber(meta_gpid));
    status = GinIndex::create(db_.get(), index_uuid, meta_gpid, nullptr);
    ASSERT_EQ(status, Status::OK);

    // Open index
    auto gin = GinIndex::open(db_.get(), index_uuid, meta_gpid, nullptr);
    ASSERT_NE(gin, nullptr);

    // Run GC on empty index
    TID tid(0, 1, 0);  // tablespace 0, page 1, slot 0
    std::vector<TID> dead_tids = {tid};

    uint64_t entries_removed = 0;
    uint64_t pages_modified = 0;
    status = gin->removeDeadEntries(dead_tids, &entries_removed, &pages_modified, nullptr);

    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(entries_removed, 0);  // No entries to remove
    EXPECT_EQ(pages_modified, 0);   // No pages modified
}

/**
 * Test 3: GC with all entries dead
 */
TEST_F(GinGCTest, AllEntriesDeadGC)
{
    // Generate UUID for GIN index
    UuidV7Bytes index_uuid = generateUuidV7();

    // Create GIN index
    GPID meta_gpid = 0;
    Status status = db_->page_manager()->allocatePageInTablespace(0, &meta_gpid, nullptr);
    ASSERT_EQ(status, Status::OK) << "Failed to allocate GIN meta page";
    uint32_t meta_page = static_cast<uint32_t>(getPageNumber(meta_gpid));
    status = GinIndex::create(db_.get(), index_uuid, meta_gpid, nullptr);
    ASSERT_EQ(status, Status::OK);

    // Open index
    auto gin = GinIndex::open(db_.get(), index_uuid, meta_gpid, nullptr);
    ASSERT_NE(gin, nullptr);

    // Insert 5 documents
    std::vector<TID> inserted_tids;
    for (uint16_t i = 0; i < 5; i++)
    {
        TID tid(0, i + 1, i);  // tablespace 0, page i+1, slot i
        inserted_tids.push_back(tid);

        std::string doc = "term" + std::to_string(i);
        status = gin->insert(doc.c_str(), doc.size(), tid, stringKeyExtractor, nullptr);
        ASSERT_EQ(status, Status::OK);
    }

    // Mark all TIDs as dead
    std::vector<TID> dead_tids = inserted_tids;

    // Run GC
    uint64_t entries_removed = 0;
    uint64_t pages_modified = 0;
    status = gin->removeDeadEntries(dead_tids, &entries_removed, &pages_modified, nullptr);

    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(entries_removed, 5); // All 5 entries removed
    EXPECT_GT(pages_modified, 0);
}

/**
 * Test 4: GC with no matching dead entries (no-op)
 */
TEST_F(GinGCTest, NoMatchingDeadEntriesGC)
{
    // Generate UUID for GIN index
    UuidV7Bytes index_uuid = generateUuidV7();

    // Create GIN index
    GPID meta_gpid = 0;
    Status status = db_->page_manager()->allocatePageInTablespace(0, &meta_gpid, nullptr);
    ASSERT_EQ(status, Status::OK) << "Failed to allocate GIN meta page";
    uint32_t meta_page = static_cast<uint32_t>(getPageNumber(meta_gpid));
    status = GinIndex::create(db_.get(), index_uuid, meta_gpid, nullptr);
    ASSERT_EQ(status, Status::OK);

    // Open index
    auto gin = GinIndex::open(db_.get(), index_uuid, meta_gpid, nullptr);
    ASSERT_NE(gin, nullptr);

    // Insert 5 documents
    std::vector<TID> inserted_tids;
    for (uint16_t i = 0; i < 5; i++)
    {
        TID tid(0, i + 1, i);  // tablespace 0, page i+1, slot i
        inserted_tids.push_back(tid);

        std::string doc = "token" + std::to_string(i);
        status = gin->insert(doc.c_str(), doc.size(), tid, stringKeyExtractor, nullptr);
        ASSERT_EQ(status, Status::OK);
    }

    // Create dead_tids that don't match any inserted TIDs
    std::vector<TID> dead_tids;
    for (uint16_t i = 10; i < 15; i++)
    {
        TID tid(0, 100 + i, i);  // tablespace 0, page 100+i, slot i (non-matching)
        dead_tids.push_back(tid);
    }

    // Run GC
    uint64_t entries_removed = 0;
    uint64_t pages_modified = 0;
    status = gin->removeDeadEntries(dead_tids, &entries_removed, &pages_modified, nullptr);

    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(entries_removed, 0); // No matching dead entries
}

} // namespace

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
