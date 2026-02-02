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

#include "scratchbird/core/lsm_tree.h"
#include <cassert>
#include <iostream>
#include <cstdio>
#include <fstream>
#include <vector>

using namespace scratchbird::core;

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

/**
 * Test 1: Write SSTable with 100 entries
 */
void testWriteSmallSSTable()
{
    std::cout << "\n=== Test 1: Write SSTable with 100 entries ===\n";

    std::string file_path = "test_sstable_small.sst";
    std::remove(file_path.c_str());

    SSTableWriter writer(file_path);
    ErrorContext ctx;

    Status status = writer.open(&ctx);
    assert(status == Status::OK);
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
        assert(status == Status::OK);
    }
    std::cout << "  ✓ Wrote 100 entries\n";

    // Finish writing
    status = writer.finish(&ctx);
    assert(status == Status::OK);
    std::cout << "  ✓ SSTable finalized\n";

    // Verify file exists
    std::ifstream file(file_path, std::ios::binary);
    assert(file.is_open());
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    assert(file_size > 0);
    std::cout << "  ✓ SSTable file written (size: " << file_size << " bytes)\n";
    file.close();

    // Verify footer can be read
    file.open(file_path, std::ios::binary);
    file.seekg(-static_cast<int>(sizeof(SSTableFooter)), std::ios::end);
    SSTableFooter footer;
    file.read(reinterpret_cast<char *>(&footer), sizeof(footer));
    file.close();

    assert(footer.magic == SSTableFooter::MAGIC_NUMBER);
    assert(footer.version == SSTableFooter::VERSION);
    assert(footer.num_entries == 100);
    std::cout << "  ✓ Footer magic/version/num_entries correct\n";

    // Verify min/max keys
    std::vector<uint8_t> min_key(footer.min_key, footer.min_key + footer.min_key_len);
    std::vector<uint8_t> max_key(footer.max_key, footer.max_key + footer.max_key_len);
    assert(toString(min_key) == "key0");
    assert(toString(max_key) == "key99");
    std::cout << "  ✓ Min/max keys correct (key0, key99)\n";

    std::cout << "  PASS\n";
}

/**
 * Test 2: Write SSTable with 10,000 entries (multiple blocks)
 */
void testWriteLargeSSTable()
{
    std::cout << "\n=== Test 2: Write SSTable with 10,000 entries ===\n";

    std::string file_path = "test_sstable_large.sst";
    std::remove(file_path.c_str());

    SSTableWriter writer(file_path, 4096); // 4KB blocks
    ErrorContext ctx;

    Status status = writer.open(&ctx);
    assert(status == Status::OK);

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
        assert(status == Status::OK);
    }
    std::cout << "  ✓ Wrote 10,000 entries\n";

    status = writer.finish(&ctx);
    assert(status == Status::OK);
    std::cout << "  ✓ SSTable finalized\n";

    // Verify file size (should have multiple blocks)
    std::ifstream file(file_path, std::ios::binary);
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.close();

    // With 10K entries of ~30 bytes each, expect > 300KB
    assert(file_size > 300000);
    std::cout << "  ✓ File size indicates multiple blocks (size: " << file_size << " bytes)\n";

    // Verify footer
    file.open(file_path, std::ios::binary);
    file.seekg(-static_cast<int>(sizeof(SSTableFooter)), std::ios::end);
    SSTableFooter footer;
    file.read(reinterpret_cast<char *>(&footer), sizeof(footer));
    file.close();

    assert(footer.magic == SSTableFooter::MAGIC_NUMBER);
    assert(footer.num_entries == 10000);
    std::cout << "  ✓ Footer indicates 10,000 entries\n";

    std::cout << "  PASS\n";
}

/**
 * Test 3: Verify Bloom filter includes all keys
 */
void testBloomFilterInclusion()
{
    std::cout << "\n=== Test 3: Bloom Filter Inclusion ===\n";

    std::string file_path = "test_sstable_bloom.sst";
    std::remove(file_path.c_str());

    SSTableWriter writer(file_path);
    ErrorContext ctx;

    Status status = writer.open(&ctx);
    assert(status == Status::OK);

    // Write 50 entries
    std::vector<std::string> keys;
    for (int i = 0; i < 50; ++i)
    {
        std::string key_str = "testkey" + std::to_string(i);
        keys.push_back(key_str);

        status = writer.addEntry(makeKey(key_str), makeValue("val"),
                                 i, 0, 1000 + i, 0, &ctx);
        assert(status == Status::OK);
    }

    status = writer.finish(&ctx);
    assert(status == Status::OK);
    std::cout << "  ✓ Wrote 50 entries\n";

    // Read bloom filter from file
    std::ifstream file(file_path, std::ios::binary);

    // Get file size
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();

    // Read footer from end of file
    size_t footer_offset = file_size - sizeof(SSTableFooter);
    file.seekg(footer_offset, std::ios::beg);
    SSTableFooter footer;
    file.read(reinterpret_cast<char *>(&footer), sizeof(footer));

    // Read bloom filter (between bloom_offset and footer_offset)
    file.seekg(footer.bloom_offset, std::ios::beg);
    size_t bloom_size = footer_offset - footer.bloom_offset;
    std::vector<uint8_t> bloom_data(bloom_size);
    file.read(reinterpret_cast<char *>(bloom_data.data()), bloom_size);
    file.close();

    LSMBloomFilter *bloom = LSMBloomFilter::deserialize(bloom_data);
    assert(bloom != nullptr);
    std::cout << "  ✓ Bloom filter deserialized\n";

    // Verify all written keys are in bloom filter
    int hits = 0;
    for (const auto &key_str : keys)
    {
        if (bloom->mightContain(makeKey(key_str)))
        {
            hits++;
        }
    }
    assert(hits == 50);
    std::cout << "  ✓ All 50 keys found in bloom filter (no false negatives)\n";

    // Check false positive rate (keys that were NOT written)
    int false_positives = 0;
    for (int i = 1000; i < 2000; ++i)
    {
        std::string key_str = "notpresent" + std::to_string(i);
        if (bloom->mightContain(makeKey(key_str)))
        {
            false_positives++;
        }
    }
    double fp_rate = static_cast<double>(false_positives) / 1000.0;
    std::cout << "  ✓ False positive rate: " << (fp_rate * 100.0) << "% (expected ~1%)\n";
    assert(fp_rate < 0.10); // Should be well under 10%

    delete bloom;
    std::cout << "  PASS\n";
}

/**
 * Test 4: Verify entries are written in sorted order
 */
void testEntriesSortedOrder()
{
    std::cout << "\n=== Test 4: Entries Written in Sorted Order ===\n";

    std::string file_path = "test_sstable_sorted.sst";
    std::remove(file_path.c_str());

    SSTableWriter writer(file_path);
    ErrorContext ctx;

    Status status = writer.open(&ctx);
    assert(status == Status::OK);

    // Write entries in sorted order
    std::vector<std::string> sorted_keys = {"aaa", "bbb", "ccc", "ddd", "eee"};
    for (size_t i = 0; i < sorted_keys.size(); ++i)
    {
        status = writer.addEntry(makeKey(sorted_keys[i]), makeValue("val"),
                                 i, 0, 100 + i, 0, &ctx);
        assert(status == Status::OK);
    }

    status = writer.finish(&ctx);
    assert(status == Status::OK);
    std::cout << "  ✓ Wrote 5 sorted entries\n";

    // Verify min/max keys match first/last
    std::ifstream file(file_path, std::ios::binary);
    file.seekg(-static_cast<int>(sizeof(SSTableFooter)), std::ios::end);
    SSTableFooter footer;
    file.read(reinterpret_cast<char *>(&footer), sizeof(footer));
    file.close();

    std::vector<uint8_t> min_key(footer.min_key, footer.min_key + footer.min_key_len);
    std::vector<uint8_t> max_key(footer.max_key, footer.max_key + footer.max_key_len);
    assert(toString(min_key) == "aaa");
    assert(toString(max_key) == "eee");
    std::cout << "  ✓ Min/max keys match sorted order (aaa, eee)\n";

    std::cout << "  PASS\n";
}

/**
 * Test 5: Verify MGA fields (xmin/xmax) are preserved
 */
void testMGAFieldsPreserved()
{
    std::cout << "\n=== Test 5: MGA Fields Preserved ===\n";

    std::string file_path = "test_sstable_mga.sst";
    std::remove(file_path.c_str());

    SSTableWriter writer(file_path);
    ErrorContext ctx;

    Status status = writer.open(&ctx);
    assert(status == Status::OK);

    // Write entry with specific MGA fields
    uint64_t xmin_test = 12345;
    uint64_t xmax_test = 67890;

    status = writer.addEntry(makeKey("key1"), makeValue("value1"),
                             100,        // sequence
                             1,          // entry_type (DELETE)
                             xmin_test,  // xmin
                             xmax_test,  // xmax
                             &ctx);
    assert(status == Status::OK);

    status = writer.finish(&ctx);
    assert(status == Status::OK);
    std::cout << "  ✓ Wrote entry with xmin=12345, xmax=67890\n";

    // Read back first entry from data block
    std::ifstream file(file_path, std::ios::binary);

    // Read key_len
    uint16_t key_len;
    file.read(reinterpret_cast<char *>(&key_len), sizeof(key_len));

    // Read key
    std::vector<uint8_t> key(key_len);
    file.read(reinterpret_cast<char *>(key.data()), key_len);

    // Read value_len
    uint32_t value_len;
    file.read(reinterpret_cast<char *>(&value_len), sizeof(value_len));

    // Read value
    std::vector<uint8_t> value(value_len);
    file.read(reinterpret_cast<char *>(value.data()), value_len);

    // Read sequence
    uint64_t sequence;
    file.read(reinterpret_cast<char *>(&sequence), sizeof(sequence));

    // Read entry_type
    uint8_t entry_type;
    file.read(reinterpret_cast<char *>(&entry_type), sizeof(entry_type));

    // Read xmin
    uint64_t xmin;
    file.read(reinterpret_cast<char *>(&xmin), sizeof(xmin));

    // Read xmax
    uint64_t xmax;
    file.read(reinterpret_cast<char *>(&xmax), sizeof(xmax));

    file.close();

    // Verify all fields
    assert(toString(key) == "key1");
    assert(toString(value) == "value1");
    assert(sequence == 100);
    assert(entry_type == 1);
    assert(xmin == xmin_test);
    assert(xmax == xmax_test);
    std::cout << "  ✓ All fields preserved (key, value, seq, type, xmin, xmax)\n";
    std::cout << "  ✓ xmin=" << xmin << ", xmax=" << xmax << " (correct)\n";

    std::cout << "  PASS\n";
}

/**
 * Test 6: Write tombstones (DELETE entries)
 */
void testWriteTombstones()
{
    std::cout << "\n=== Test 6: Write Tombstones (DELETE entries) ===\n";

    std::string file_path = "test_sstable_tombstones.sst";
    std::remove(file_path.c_str());

    SSTableWriter writer(file_path);
    ErrorContext ctx;

    Status status = writer.open(&ctx);
    assert(status == Status::OK);

    // Write mix of INSERT and DELETE entries
    status = writer.addEntry(makeKey("key1"), makeValue("value1"), 1, 0, 100, 0, &ctx); // INSERT
    assert(status == Status::OK);

    status = writer.addEntry(makeKey("key2"), makeValue(""), 2, 1, 101, 0, &ctx); // DELETE (tombstone)
    assert(status == Status::OK);

    status = writer.addEntry(makeKey("key3"), makeValue("value3"), 3, 0, 102, 0, &ctx); // INSERT
    assert(status == Status::OK);

    status = writer.finish(&ctx);
    assert(status == Status::OK);
    std::cout << "  ✓ Wrote 3 entries (2 INSERT, 1 DELETE)\n";

    // Verify footer
    std::ifstream file(file_path, std::ios::binary);
    file.seekg(-static_cast<int>(sizeof(SSTableFooter)), std::ios::end);
    SSTableFooter footer;
    file.read(reinterpret_cast<char *>(&footer), sizeof(footer));
    file.close();

    assert(footer.num_entries == 3);
    std::cout << "  ✓ Footer indicates 3 entries (including tombstone)\n";

    std::cout << "  PASS\n";
}

/**
 * Test 7: Write empty SSTable (no entries)
 */
void testWriteEmptySSTable()
{
    std::cout << "\n=== Test 7: Write Empty SSTable ===\n";

    std::string file_path = "test_sstable_empty.sst";
    std::remove(file_path.c_str());

    SSTableWriter writer(file_path);
    ErrorContext ctx;

    Status status = writer.open(&ctx);
    assert(status == Status::OK);

    // Don't write any entries
    status = writer.finish(&ctx);
    assert(status == Status::OK);
    std::cout << "  ✓ Empty SSTable finalized\n";

    // Verify footer
    std::ifstream file(file_path, std::ios::binary);
    file.seekg(-static_cast<int>(sizeof(SSTableFooter)), std::ios::end);
    SSTableFooter footer;
    file.read(reinterpret_cast<char *>(&footer), sizeof(footer));
    file.close();

    assert(footer.magic == SSTableFooter::MAGIC_NUMBER);
    assert(footer.num_entries == 0);
    std::cout << "  ✓ Empty SSTable has 0 entries\n";

    std::cout << "  PASS\n";
}

int main()
{
    std::cout << "\n";
    std::cout << "==================================================\n";
    std::cout << "   SSTable Writer Unit Tests\n";
    std::cout << "==================================================\n";

    std::cout << "Starting testWriteSmallSSTable...\n" << std::flush;
    testWriteSmallSSTable();
    testWriteLargeSSTable();
    testBloomFilterInclusion();
    testEntriesSortedOrder();
    testMGAFieldsPreserved();
    testWriteTombstones();
    testWriteEmptySSTable();

    std::cout << "\n=== All SSTable Writer Tests PASSED ===\n\n";

    return 0;
}
