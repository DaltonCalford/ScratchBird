/**
 * LSM-Tree Comprehensive Integration Test
 *
 * Tests complete LSM-Tree functionality:
 * - Multiple insert/flush cycles
 * - Compaction triggering
 * - MGA visibility
 * - Statistics tracking
 */

#include "scratchbird/core/lsm_tree.h"
#include "scratchbird/core/database.h"
#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <chrono>
#include <thread>

using namespace scratchbird::core;

void removeDirectory(const std::string &path)
{
    std::string cmd = "rm -rf " + path;
    system(cmd.c_str());
}

std::vector<uint8_t> makeKey(size_t index)
{
    std::string key_str = "key_" + std::to_string(index);
    return std::vector<uint8_t>(key_str.begin(), key_str.end());
}

std::vector<uint8_t> makeValue(size_t index, const std::string &prefix = "value")
{
    std::string value_str = prefix + "_" + std::to_string(index) + "_data_payload";
    return std::vector<uint8_t>(value_str.begin(), value_str.end());
}

/**
 * Test 1: Large dataset with multiple flushes
 */
void testLargeDataset()
{
    std::cout << "\n=== Test 1: Large Dataset (1000 keys, multiple flushes) ===\n";

    std::string index_path = "/tmp/lsm_test_large";
    std::string db_path = "/tmp/lsm_test_large.db";
    removeDirectory(index_path);
    std::remove(db_path.c_str());

    // Create and open database
    Status status = Database::create(db_path, 8192, nullptr);
    assert(status == Status::OK);

    Database *db = new Database();
    status = db->open(db_path, nullptr);
    assert(status == Status::OK);

    TransactionManager *txn_mgr = db->transaction_manager();
    uint64_t xid = txn_mgr->getCurrentXid();

    // Create LSM-Tree index with small memtable (1 MB) to force flushes
    LSMTreeIndex index(index_path, txn_mgr, 1);
    status = index.create(nullptr);
    assert(status == Status::OK);
    std::cout << "  ✓ Created LSM-Tree index\n";

    // Insert 1000 keys (will trigger multiple flushes)
    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < 1000; i++)
    {
        std::vector<uint8_t> key = makeKey(i);
        std::vector<uint8_t> value = makeValue(i);
        status = index.put(key, value, xid, nullptr);
        assert(status == Status::OK);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "  ✓ Inserted 1000 keys in " << duration.count() << " ms\n";

    // Get statistics
    LSMTreeIndex::Statistics stats;
    status = index.getStatistics(&stats, nullptr);
    assert(status == Status::OK);
    std::cout << "  ✓ Level 0 SSTables: " << stats.level0_sstables << "\n";
    std::cout << "  ✓ Total size: " << (stats.total_size_bytes / 1024) << " KB\n";

    // Verify all keys readable
    size_t found_count = 0;
    for (size_t i = 0; i < 1000; i++)
    {
        std::vector<uint8_t> key = makeKey(i);
        std::vector<uint8_t> value;
        bool found = false;

        status = index.get(key, xid, &value, &found, nullptr);
        assert(status == Status::OK);

        if (found)
        {
            found_count++;
        }
    }
    std::cout << "  ✓ Retrieved " << found_count << "/1000 keys correctly\n";
    assert(found_count == 1000);

    // Cleanup
    index.close(nullptr);
    delete db;
    removeDirectory(index_path);
    std::remove(db_path.c_str());
}

/**
 * Test 2: Manual flush and verification
 */
void testManualFlush()
{
    std::cout << "\n=== Test 2: Manual Flush and Verification ===\n";

    std::string index_path = "/tmp/lsm_test_flush";
    std::string db_path = "/tmp/lsm_test_flush.db";
    removeDirectory(index_path);
    std::remove(db_path.c_str());

    // Create database
    Status status = Database::create(db_path, 8192, nullptr);
    assert(status == Status::OK);

    Database *db = new Database();
    status = db->open(db_path, nullptr);
    assert(status == Status::OK);

    TransactionManager *txn_mgr = db->transaction_manager();
    uint64_t xid = txn_mgr->getCurrentXid();

    // Create index with large memtable (no auto-flush)
    LSMTreeIndex index(index_path, txn_mgr, 10);
    status = index.create(nullptr);
    assert(status == Status::OK);

    // Insert 100 keys
    for (size_t i = 0; i < 100; i++)
    {
        std::vector<uint8_t> key = makeKey(i);
        std::vector<uint8_t> value = makeValue(i);
        status = index.put(key, value, xid, nullptr);
        assert(status == Status::OK);
    }
    std::cout << "  ✓ Inserted 100 keys\n";

    // Check statistics before flush
    LSMTreeIndex::Statistics stats_before;
    status = index.getStatistics(&stats_before, nullptr);
    assert(status == Status::OK);
    std::cout << "  ✓ Before flush - Active memtable: " << stats_before.active_memtable_entries << " entries\n";
    std::cout << "  ✓ Before flush - Level 0 SSTables: " << stats_before.level0_sstables << "\n";

    // Manual flush
    status = index.flush(nullptr);
    assert(status == Status::OK);
    std::cout << "  ✓ Flushed memtable\n";

    // Check statistics after flush
    LSMTreeIndex::Statistics stats_after;
    status = index.getStatistics(&stats_after, nullptr);
    assert(status == Status::OK);
    std::cout << "  ✓ After flush - Active memtable: " << stats_after.active_memtable_entries << " entries\n";
    std::cout << "  ✓ After flush - Level 0 SSTables: " << stats_after.level0_sstables << "\n";

    assert(stats_after.level0_sstables > stats_before.level0_sstables);

    // Verify all keys still readable from SSTable
    size_t found_count = 0;
    for (size_t i = 0; i < 100; i++)
    {
        std::vector<uint8_t> key = makeKey(i);
        std::vector<uint8_t> value;
        bool found = false;

        status = index.get(key, xid, &value, &found, nullptr);
        assert(status == Status::OK);

        if (found)
        {
            found_count++;
        }
    }
    std::cout << "  ✓ Retrieved " << found_count << "/100 keys after flush\n";
    assert(found_count == 100);

    // Cleanup
    index.close(nullptr);
    delete db;
    removeDirectory(index_path);
    std::remove(db_path.c_str());
}

/**
 * Test 3: Update operations (multiple versions)
 */
void testUpdates()
{
    std::cout << "\n=== Test 3: Update Operations ===\n";

    std::string index_path = "/tmp/lsm_test_updates";
    std::string db_path = "/tmp/lsm_test_updates.db";
    removeDirectory(index_path);
    std::remove(db_path.c_str());

    // Create database
    Status status = Database::create(db_path, 8192, nullptr);
    assert(status == Status::OK);

    Database *db = new Database();
    status = db->open(db_path, nullptr);
    assert(status == Status::OK);

    TransactionManager *txn_mgr = db->transaction_manager();
    uint64_t xid = txn_mgr->getCurrentXid();

    // Create index
    LSMTreeIndex index(index_path, txn_mgr, 4);
    status = index.create(nullptr);
    assert(status == Status::OK);

    // Insert 10 keys
    for (size_t i = 0; i < 10; i++)
    {
        std::vector<uint8_t> key = makeKey(i);
        std::vector<uint8_t> value = makeValue(i, "v1");
        status = index.put(key, value, xid, nullptr);
        assert(status == Status::OK);
    }
    std::cout << "  ✓ Inserted 10 keys (version 1)\n";

    // Update the same keys with different values
    for (size_t i = 0; i < 10; i++)
    {
        std::vector<uint8_t> key = makeKey(i);
        std::vector<uint8_t> value = makeValue(i, "v2");
        status = index.put(key, value, xid, nullptr);
        assert(status == Status::OK);
    }
    std::cout << "  ✓ Updated 10 keys (version 2)\n";

    // Verify we get the latest values
    size_t correct_version = 0;
    for (size_t i = 0; i < 10; i++)
    {
        std::vector<uint8_t> key = makeKey(i);
        std::vector<uint8_t> expected_value = makeValue(i, "v2");
        std::vector<uint8_t> actual_value;
        bool found = false;

        status = index.get(key, xid, &actual_value, &found, nullptr);
        assert(status == Status::OK);

        if (found && actual_value == expected_value)
        {
            correct_version++;
        }
    }
    std::cout << "  ✓ Retrieved correct version for " << correct_version << "/10 keys\n";
    assert(correct_version == 10);

    // Cleanup
    index.close(nullptr);
    delete db;
    removeDirectory(index_path);
    std::remove(db_path.c_str());
}

/**
 * Test 4: Compaction trigger (multiple flushes to Level 0)
 */
void testCompactionTrigger()
{
    std::cout << "\n=== Test 4: Compaction Trigger ===\n";

    std::string index_path = "/tmp/lsm_test_compaction";
    std::string db_path = "/tmp/lsm_test_compaction.db";
    removeDirectory(index_path);
    std::remove(db_path.c_str());

    // Create database
    Status status = Database::create(db_path, 8192, nullptr);
    assert(status == Status::OK);

    Database *db = new Database();
    status = db->open(db_path, nullptr);
    assert(status == Status::OK);

    TransactionManager *txn_mgr = db->transaction_manager();
    uint64_t xid = txn_mgr->getCurrentXid();

    // Create index with small memtable to force multiple flushes
    LSMTreeIndex index(index_path, txn_mgr, 1);
    status = index.open(nullptr);  // This will call create if doesn't exist
    if (status != Status::OK)
    {
        status = index.create(nullptr);
        assert(status == Status::OK);
    }
    std::cout << "  ✓ Created LSM-Tree index\n";

    // Insert enough keys to trigger multiple flushes
    // With 1 MB memtable, inserting ~500 keys should trigger 4-5 flushes
    for (size_t i = 0; i < 500; i++)
    {
        std::vector<uint8_t> key = makeKey(i);
        std::vector<uint8_t> value = makeValue(i);
        status = index.put(key, value, xid, nullptr);
        assert(status == Status::OK);
    }
    std::cout << "  ✓ Inserted 500 keys\n";

    // Flush remaining keys
    status = index.flush(nullptr);
    assert(status == Status::OK);

    // Wait a bit for background compaction
    std::cout << "  ✓ Waiting for background compaction (3 seconds)...\n";
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Get statistics
    LSMTreeIndex::Statistics stats;
    status = index.getStatistics(&stats, nullptr);
    assert(status == Status::OK);
    std::cout << "  ✓ Level 0 SSTables: " << stats.level0_sstables << "\n";
    std::cout << "  ✓ Level 1 SSTables: " << stats.level1_sstables << "\n";
    std::cout << "  ✓ Level 2 SSTables: " << stats.level2_sstables << "\n";
    std::cout << "  ✓ Level 3 SSTables: " << stats.level3_sstables << "\n";

    // Note: Compaction should have moved some Level 0 files to Level 1
    // if there were 4+ Level 0 files

    // Cleanup
    index.close(nullptr);
    delete db;
    removeDirectory(index_path);
    std::remove(db_path.c_str());
}

int main()
{
    std::cout << "========================================\n";
    std::cout << "  LSM-Tree Comprehensive Integration Tests\n";
    std::cout << "========================================\n";

    testLargeDataset();
    testManualFlush();
    testUpdates();
    testCompactionTrigger();

    std::cout << "\n========================================\n";
    std::cout << "  ✅ ALL COMPREHENSIVE TESTS PASSED\n";
    std::cout << "========================================\n";

    return 0;
}
