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
#include "scratchbird/core/hash_index.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/tid.h"
#include "scratchbird/core/gpid.h"
#include "test_helpers.h"
#include <cstring>
#include <filesystem>

using namespace scratchbird::core;
using scratchbird::testing::uniqueTestDbPath;

class HashCustomTablespaceTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        test_db_path = uniqueTestDbPath("test_hash_custom_tablespace");

        // Remove if exists
        if (std::filesystem::exists(test_db_path))
        {
            std::filesystem::remove(test_db_path);
        }

        // Create database
        ErrorContext ctx;
        Status status = Database::create(test_db_path.c_str(), 8192, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to create database: " << ctx.message;

        // Open database
        db = std::make_unique<Database>();
        status = db->open(test_db_path, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to open database: " << ctx.message;
    }

    void TearDown() override
    {
        if (db)
        {
            db->close();
            db.reset();
        }

        if (std::filesystem::exists(test_db_path))
        {
            std::filesystem::remove(test_db_path);
        }
    }

    std::string test_db_path;
    std::unique_ptr<Database> db;
};

// Test: Insert and find with custom tablespace TID (tablespace 5, page 100, slot 7)
TEST_F(HashCustomTablespaceTest, InsertFindCustomTablespace)
{
    ErrorContext ctx;
    UuidV7Bytes index_uuid = generateUuidV7();
    uint32_t meta_page = 0;

    // Create index
    Status status = HashIndex::create(db.get(), index_uuid, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK) << "Failed to create hash index: " << ctx.message;

    auto index = HashIndex::open(db.get(), index_uuid, meta_page, &ctx);
    ASSERT_NE(index, nullptr);

    // Create TID in custom tablespace 5
    uint16_t tablespace_id = 5;
    uint64_t page_number = 100;
    uint16_t slot = 7;

    GPID gpid = makeGPID(tablespace_id, page_number);
    TID tid(gpid, slot);

    // Verify TID components
    ASSERT_EQ(getTablespaceID(tid), tablespace_id);
    ASSERT_EQ(getPageNumber(tid), page_number);
    ASSERT_EQ(tid.slot, slot);

    // Insert entry with custom tablespace TID
    const char* key = "custom_tablespace_key";
    uint64_t xid = 1;
    status = index->insert(key, strlen(key), tid, xid, &ctx);
    ASSERT_EQ(status, Status::OK) << "Failed to insert with custom tablespace TID: " << ctx.message;

    // Find the entry
    std::vector<TID> results = index->find(key, strlen(key), xid, &ctx);
    ASSERT_EQ(results.size(), 1) << "Expected 1 result, got " << results.size();

    // Verify retrieved TID matches
    TID retrieved = results[0];
    ASSERT_EQ(getTablespaceID(retrieved), tablespace_id)
        << "Expected tablespace " << tablespace_id << ", got " << getTablespaceID(retrieved);
    ASSERT_EQ(getPageNumber(retrieved), page_number)
        << "Expected page " << page_number << ", got " << getPageNumber(retrieved);
    ASSERT_EQ(retrieved.slot, slot)
        << "Expected slot " << slot << ", got " << retrieved.slot;

    std::cout << "✅ Hash index successfully stored and retrieved TID from custom tablespace "
              << tablespace_id << std::endl;
}

// Test: Insert multiple entries with different tablespaces
TEST_F(HashCustomTablespaceTest, MultipleTablespaces)
{
    ErrorContext ctx;
    UuidV7Bytes index_uuid = generateUuidV7();
    uint32_t meta_page = 0;

    Status status = HashIndex::create(db.get(), index_uuid, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto index = HashIndex::open(db.get(), index_uuid, meta_page, &ctx);
    ASSERT_NE(index, nullptr);

    // Insert entries in different tablespaces
    struct TestEntry {
        const char* key;
        uint16_t tablespace;
        uint64_t page;
        uint16_t slot;
    };

    TestEntry entries[] = {
        {"key_ts0", 0, 42, 1},      // Default tablespace
        {"key_ts1", 1, 100, 5},     // Tablespace 1
        {"key_ts5", 5, 200, 10},    // Tablespace 5
        {"key_ts100", 100, 500, 3}, // Tablespace 100
        {"key_ts255", 255, 999, 7}  // Tablespace 255
    };

    uint64_t xid = 1;

    // Insert all entries
    for (const auto& entry : entries)
    {
        GPID gpid = makeGPID(entry.tablespace, entry.page);
        TID tid(gpid, entry.slot);

        status = index->insert(entry.key, strlen(entry.key), tid, xid, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to insert key " << entry.key
                                      << " in tablespace " << entry.tablespace;
    }

    // Verify all entries can be found
    for (const auto& entry : entries)
    {
        std::vector<TID> results = index->find(entry.key, strlen(entry.key), xid, &ctx);
        ASSERT_EQ(results.size(), 1) << "Expected 1 result for key " << entry.key;

        TID retrieved = results[0];
        ASSERT_EQ(getTablespaceID(retrieved), entry.tablespace)
            << "Tablespace mismatch for key " << entry.key;
        ASSERT_EQ(getPageNumber(retrieved), entry.page)
            << "Page mismatch for key " << entry.key;
        ASSERT_EQ(retrieved.slot, entry.slot)
            << "Slot mismatch for key " << entry.key;
    }

    std::cout << "✅ Hash index successfully handled TIDs from 5 different tablespaces" << std::endl;
}

// Test: Remove entry from custom tablespace
TEST_F(HashCustomTablespaceTest, RemoveCustomTablespace)
{
    ErrorContext ctx;
    UuidV7Bytes index_uuid = generateUuidV7();
    uint32_t meta_page = 0;

    Status status = HashIndex::create(db.get(), index_uuid, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto index = HashIndex::open(db.get(), index_uuid, meta_page, &ctx);
    ASSERT_NE(index, nullptr);

    // Insert entry in custom tablespace
    GPID gpid = makeGPID(10, 500);
    TID tid(gpid, 15);
    const char* key = "removable_key";
    uint64_t xid = 1;

    status = index->insert(key, strlen(key), tid, xid, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Verify it exists
    std::vector<TID> results = index->find(key, strlen(key), xid, &ctx);
    ASSERT_EQ(results.size(), 1);

    // Remove it
    status = index->remove(key, strlen(key), tid, xid + 1, &ctx);
    ASSERT_EQ(status, Status::OK) << "Failed to remove entry from custom tablespace";

    // Verify it's gone (using xid+1 to see it as deleted)
    results = index->find(key, strlen(key), xid + 2, &ctx);
    ASSERT_EQ(results.size(), 0) << "Entry should be deleted";

    std::cout << "✅ Hash index successfully removed entry from custom tablespace" << std::endl;
}
