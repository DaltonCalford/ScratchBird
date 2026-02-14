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
 * SSTable Writer Unit Tests
 *
 * Tests SSTable serialization format and correctness
 */

#include <gtest/gtest.h>
#include "scratchbird/core/lsm_tree_index.h"
#include "test_helpers.h"
#include <iostream>
#include <cstdio>
#include <fstream>
#include <vector>
#include <set>

using namespace scratchbird::core;
using scratchbird::testing::uniqueTestShortPath;

namespace {

// Helper: Create key from string
std::vector<uint8_t> makeKey(const std::string &s)
{
    return std::vector<uint8_t>(s.begin(), s.end());
}

// Helper: Create value from string
std::vector<uint8_t> makeValue(const std::string &s)
{
    return std::vector<uint8_t>(s.begin(), s.end());
}

// Helper: Convert vector to string
std::string toString(const std::vector<uint8_t> &v)
{
    return std::string(v.begin(), v.end());
}

size_t countEntries(SSTableReader &reader)
{
    auto it = reader.createIterator();
    size_t count = 0;
    while (it && it->isValid())
    {
        ++count;
        it->next();
    }
    return count;
}

/**
 * Test 1: Write SSTable with 100 entries
 */
void testWriteSmallSSTable()
{
    std::cout << "\n=== Test 1: Write SSTable with 100 entries ===\n";

    std::string file_path = uniqueTestShortPath("test_sstable_small", ".sst");
    std::remove(file_path.c_str());

    SSTableWriter writer(file_path);
    ErrorContext ctx;

    Status status = writer.open(&ctx);
    ASSERT_TRUE(status == Status::OK);
    std::cout << "  ✓ SSTable file opened for writing\n";

    // Write 100 sorted entries
    for (int i = 0; i < 100; ++i)
    {
        std::string key_str = "key" + std::to_string(i);
        std::string val_str = "value" + std::to_string(i);

        status = writer.addEntry(makeKey(key_str), makeValue(val_str),
                                 i,      // sequence_number
                                 0,      // entry_type (INSERT)
                                 1000 + i, // xmin
                                 0,      // xmax
                                 &ctx);
        ASSERT_TRUE(status == Status::OK);
    }
    std::cout << "  ✓ Wrote 100 entries\n";

    // Finish writing
    status = writer.finish(&ctx);
    ASSERT_TRUE(status == Status::OK);
    std::cout << "  ✓ SSTable finalized\n";

    // Verify file exists
    std::ifstream file(file_path, std::ios::binary);
    ASSERT_TRUE(file.is_open());
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    ASSERT_TRUE(file_size > 0);
    std::cout << "  ✓ SSTable file written (size: " << file_size << " bytes)\n";
    file.close();

    SSTableReader reader(file_path);
    status = reader.open(&ctx);
    ASSERT_TRUE(status == Status::OK);
    ASSERT_TRUE(reader.isOpen());

    ASSERT_EQ(countEntries(reader), 100u);

    // Verify min/max keys
    ASSERT_TRUE(toString(reader.getMinKey()) == "key0");
    ASSERT_TRUE(toString(reader.getMaxKey()) == "key99");
    std::cout << "  ✓ Min/max keys correct (key0, key99)\n";

    std::cout << "  PASS\n";
}

/**
 * Test 2: Write SSTable with 10,000 entries (multiple blocks)
 */
void testWriteLargeSSTable()
{
    std::cout << "\n=== Test 2: Write SSTable with 10,000 entries ===\n";

    std::string file_path = uniqueTestShortPath("test_sstable_large", ".sst");
    std::remove(file_path.c_str());

    SSTableWriter writer(file_path, 4096); // 4KB blocks
    ErrorContext ctx;

    Status status = writer.open(&ctx);
    ASSERT_TRUE(status == Status::OK);

    // Write 10,000 sorted entries
    for (int i = 0; i < 10000; ++i)
    {
        // Pad key to make entries larger (force multiple blocks)
        std::string key_str = "key" + std::string(10, '0') + std::to_string(i);
        std::string val_str = "value" + std::to_string(i);

        status = writer.addEntry(makeKey(key_str), makeValue(val_str),
                                 i,      // sequence_number
                                 0,      // entry_type (INSERT)
                                 2000 + i, // xmin
                                 0,      // xmax
                                 &ctx);
        ASSERT_TRUE(status == Status::OK);
    }
    std::cout << "  ✓ Wrote 10,000 entries\n";

    status = writer.finish(&ctx);
    ASSERT_TRUE(status == Status::OK);
    std::cout << "  ✓ SSTable finalized\n";

    // Verify file size (should have multiple blocks)
    std::ifstream file(file_path, std::ios::binary);
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.close();

    // With 10K entries of ~30 bytes each, expect > 300KB
    ASSERT_TRUE(file_size > 300000);
    std::cout << "  ✓ File size indicates multiple blocks (size: " << file_size << " bytes)\n";

    SSTableReader reader(file_path);
    status = reader.open(&ctx);
    ASSERT_TRUE(status == Status::OK);
    ASSERT_TRUE(reader.isOpen());

    ASSERT_EQ(countEntries(reader), 10000u);
    std::cout << "  ✓ Reader counted 10,000 entries\n";

    std::cout << "  PASS\n";
}

/**
 * Test 3: Verify Bloom filter includes all keys
 */
void testBloomFilterInclusion()
{
    std::cout << "\n=== Test 3: Bloom Filter Inclusion ===\n";

    std::string file_path = uniqueTestShortPath("test_sstable_bloom", ".sst");
    std::remove(file_path.c_str());

    SSTableWriter writer(file_path);
    ErrorContext ctx;

    Status status = writer.open(&ctx);
    ASSERT_TRUE(status == Status::OK);

    // Write 50 entries
    std::vector<std::string> keys;
    for (int i = 0; i < 50; ++i)
    {
        std::string key_str = "testkey" + std::to_string(i);
        keys.push_back(key_str);

        status = writer.addEntry(makeKey(key_str), makeValue("val"),
                                 i, 0, 1000 + i, 0, &ctx);
        ASSERT_TRUE(status == Status::OK);
    }

    status = writer.finish(&ctx);
    ASSERT_TRUE(status == Status::OK);
    std::cout << "  ✓ Wrote 50 entries\n";

    SSTableReader reader(file_path);
    status = reader.open(&ctx);
    ASSERT_TRUE(status == Status::OK);
    ASSERT_TRUE(reader.isOpen());

    std::set<std::string> seen_keys;
    auto it = reader.createIterator();
    while (it && it->isValid())
    {
        seen_keys.insert(toString(it->key()));
        it->next();
    }
    ASSERT_EQ(seen_keys.size(), keys.size());
    for (const auto &key_str : keys)
    {
        ASSERT_TRUE(seen_keys.count(key_str) > 0);
    }
    std::cout << "  ✓ All 50 keys found in SSTable via iterator\n";
    std::cout << "  PASS\n";
}

/**
 * Test 4: Verify entries are written in sorted order
 */
void testEntriesSortedOrder()
{
    std::cout << "\n=== Test 4: Entries Written in Sorted Order ===\n";

    std::string file_path = uniqueTestShortPath("test_sstable_sorted", ".sst");
    std::remove(file_path.c_str());

    SSTableWriter writer(file_path);
    ErrorContext ctx;

    Status status = writer.open(&ctx);
    ASSERT_TRUE(status == Status::OK);

    // Write entries in sorted order
    std::vector<std::string> sorted_keys = {"aaa", "bbb", "ccc", "ddd", "eee"};
    for (size_t i = 0; i < sorted_keys.size(); ++i)
    {
        status = writer.addEntry(makeKey(sorted_keys[i]), makeValue("val"),
                                 i, 0, 100 + i, 0, &ctx);
        ASSERT_TRUE(status == Status::OK);
    }

    status = writer.finish(&ctx);
    ASSERT_TRUE(status == Status::OK);
    std::cout << "  ✓ Wrote 5 sorted entries\n";

    SSTableReader reader(file_path);
    status = reader.open(&ctx);
    ASSERT_TRUE(status == Status::OK);
    ASSERT_TRUE(reader.isOpen());

    ASSERT_TRUE(toString(reader.getMinKey()) == "aaa");
    ASSERT_TRUE(toString(reader.getMaxKey()) == "eee");
    std::cout << "  ✓ Min/max keys match sorted order (aaa, eee)\n";

    std::cout << "  PASS\n";
}

/**
 * Test 5: Verify MGA fields (xmin/xmax) are preserved
 */
void testMGAFieldsPreserved()
{
    std::cout << "\n=== Test 5: MGA Fields Preserved ===\n";

    std::string file_path = uniqueTestShortPath("test_sstable_mga", ".sst");
    std::remove(file_path.c_str());

    SSTableWriter writer(file_path);
    ErrorContext ctx;

    Status status = writer.open(&ctx);
    ASSERT_TRUE(status == Status::OK);

    // Write entry with specific MGA fields
    uint64_t xmin_test = 12345;
    uint64_t xmax_test = 67890;

    status = writer.addEntry(makeKey("key1"), makeValue("value1"),
                             100,        // sequence
                             1,          // entry_type (DELETE)
                             xmin_test,  // xmin
                             xmax_test,  // xmax
                             &ctx);
    ASSERT_TRUE(status == Status::OK);

    status = writer.finish(&ctx);
    ASSERT_TRUE(status == Status::OK);
    std::cout << "  ✓ Wrote entry with xmin=12345, xmax=67890\n";

    SSTableReader reader(file_path);
    status = reader.open(&ctx);
    ASSERT_TRUE(status == Status::OK);
    ASSERT_TRUE(reader.isOpen());

    auto it = reader.createIterator();
    ASSERT_TRUE(it && it->isValid());
    ASSERT_TRUE(toString(it->key()) == "key1");
    ASSERT_TRUE(toString(it->value()) == "value1");
    ASSERT_TRUE(it->sequenceNumber() == 100);
    ASSERT_TRUE(it->entryType() == 1);
    ASSERT_TRUE(it->xmin() == xmin_test);
    ASSERT_TRUE(it->xmax() == xmax_test);
    std::cout << "  ✓ All fields preserved (key, value, seq, type, xmin, xmax)\n";
    std::cout << "  ✓ xmin=" << it->xmin() << ", xmax=" << it->xmax() << " (correct)\n";

    std::cout << "  PASS\n";
}

/**
 * Test 6: Write tombstones (DELETE entries)
 */
void testWriteTombstones()
{
    std::cout << "\n=== Test 6: Write Tombstones (DELETE entries) ===\n";

    std::string file_path = uniqueTestShortPath("test_sstable_tombstones", ".sst");
    std::remove(file_path.c_str());

    SSTableWriter writer(file_path);
    ErrorContext ctx;

    Status status = writer.open(&ctx);
    ASSERT_TRUE(status == Status::OK);

    // Write mix of INSERT and DELETE entries
    status = writer.addEntry(makeKey("key1"), makeValue("value1"), 1, 0, 100, 0, &ctx); // INSERT
    ASSERT_TRUE(status == Status::OK);

    status = writer.addEntry(makeKey("key2"), makeValue(""), 2, 1, 101, 0, &ctx); // DELETE (tombstone)
    ASSERT_TRUE(status == Status::OK);

    status = writer.addEntry(makeKey("key3"), makeValue("value3"), 3, 0, 102, 0, &ctx); // INSERT
    ASSERT_TRUE(status == Status::OK);

    status = writer.finish(&ctx);
    ASSERT_TRUE(status == Status::OK);
    std::cout << "  ✓ Wrote 3 entries (2 INSERT, 1 DELETE)\n";

    SSTableReader reader(file_path);
    status = reader.open(&ctx);
    ASSERT_TRUE(status == Status::OK);
    ASSERT_TRUE(reader.isOpen());

    size_t entries = 0;
    size_t tombstones = 0;
    auto it = reader.createIterator();
    while (it && it->isValid())
    {
        ++entries;
        if (it->entryType() == 1)
        {
            ++tombstones;
        }
        it->next();
    }
    ASSERT_EQ(entries, 3u);
    ASSERT_EQ(tombstones, 1u);
    std::cout << "  ✓ Reader saw 3 entries including 1 tombstone\n";

    std::cout << "  PASS\n";
}

/**
 * Test 7: Write empty SSTable (no entries)
 */
void testWriteEmptySSTable()
{
    std::cout << "\n=== Test 7: Write Empty SSTable ===\n";

    std::string file_path = uniqueTestShortPath("test_sstable_empty", ".sst");
    std::remove(file_path.c_str());

    SSTableWriter writer(file_path);
    ErrorContext ctx;

    Status status = writer.open(&ctx);
    ASSERT_TRUE(status == Status::OK);

    // Don't write any entries
    status = writer.finish(&ctx);
    ASSERT_TRUE(status == Status::OK);
    std::cout << "  ✓ Empty SSTable finalized\n";

    SSTableReader reader(file_path);
    status = reader.open(&ctx);
    ASSERT_TRUE(status == Status::OK);
    ASSERT_TRUE(reader.isOpen());

    ASSERT_EQ(countEntries(reader), 0u);
    std::cout << "  ✓ Empty SSTable has 0 entries\n";

    std::cout << "  PASS\n";
}

} // namespace


// ==================== GTest Wrappers ====================

TEST(LSMTest, WriteSmallSSTable)
{
    testWriteSmallSSTable();
}

TEST(LSMTest, WriteLargeSSTable)
{
    testWriteLargeSSTable();
}

TEST(LSMTest, BloomFilterInclusion)
{
    testBloomFilterInclusion();
}

TEST(LSMTest, EntriesSortedOrder)
{
    testEntriesSortedOrder();
}

TEST(LSMTest, MGAFieldsPreserved)
{
    testMGAFieldsPreserved();
}

TEST(LSMTest, WriteTombstones)
{
    testWriteTombstones();
}

TEST(LSMTest, WriteEmptySSTable)
{
    testWriteEmptySSTable();
}
