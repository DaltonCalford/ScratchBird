/**
 * LSM-Tree Stress Test
 *
 * Tests LSM-Tree performance and stability under load:
 * - 100K key insertion with throughput measurement
 * - Random read performance
 * - Mixed workload (reads + writes)
 * - Data integrity validation
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
#include <random>
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
    // Create ~100 byte values for realistic data size
    std::string value_str = prefix + "_" + std::to_string(index) + "_";
    while (value_str.size() < 100)
    {
        value_str += "data";
    }
    return std::vector<uint8_t>(value_str.begin(), value_str.end());
}

/**
 * Test 1: Write performance (100K sequential inserts)
 */
void testWritePerformance()
{
    std::cout << "\n=== Test 1: Write Performance (100K keys) ===\n";

    std::string index_path = "lsm_stress_write";
    std::string db_path = "lsm_stress_write.db";
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

    // Create LSM-Tree index with 4 MB memtable
    LSMTreeIndex index(index_path, txn_mgr, 4);
    status = index.create(nullptr);
    assert(status == Status::OK);

    // Insert 100K keys with timing
    const size_t num_keys = 100000;
    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < num_keys; i++)
    {
        std::vector<uint8_t> key = makeKey(i);
        std::vector<uint8_t> value = makeValue(i);
        status = index.put(key, value, xid, nullptr);
        assert(status == Status::OK);

        // Progress indicator
        if ((i + 1) % 10000 == 0)
        {
            std::cout << "  Progress: " << (i + 1) << " keys inserted\n";
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    double throughput = (num_keys * 1000.0) / duration.count();

    std::cout << "  ✓ Inserted " << num_keys << " keys in " << duration.count() << " ms\n";
    std::cout << "  ✓ Write throughput: " << static_cast<int>(throughput) << " ops/sec\n";

    // Get statistics
    LSMTreeIndex::Statistics stats;
    status = index.getStatistics(&stats, nullptr);
    assert(status == Status::OK);
    std::cout << "  ✓ Active memtable entries: " << stats.active_memtable_entries << "\n";
    std::cout << "  ✓ Level 0 SSTables: " << stats.level0_sstables << "\n";
    std::cout << "  ✓ Total size: " << (stats.total_size_bytes / 1024 / 1024) << " MB\n";

    // Cleanup
    index.close(nullptr);
    delete db;
    removeDirectory(index_path);
    std::remove(db_path.c_str());
}

/**
 * Test 2: Read performance (50K random reads)
 */
void testReadPerformance()
{
    std::cout << "\n=== Test 2: Read Performance (50K random reads) ===\n";

    std::string index_path = "lsm_stress_read";
    std::string db_path = "lsm_stress_read.db";
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

    // Create and populate index
    LSMTreeIndex index(index_path, txn_mgr, 4);
    status = index.create(nullptr);
    assert(status == Status::OK);

    // Insert 50K keys
    const size_t num_keys = 50000;
    std::cout << "  Populating with " << num_keys << " keys...\n";
    for (size_t i = 0; i < num_keys; i++)
    {
        std::vector<uint8_t> key = makeKey(i);
        std::vector<uint8_t> value = makeValue(i);
        status = index.put(key, value, xid, nullptr);
        assert(status == Status::OK);
    }
    std::cout << "  ✓ Population complete\n";

    // Perform random reads
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, num_keys - 1);

    auto start = std::chrono::high_resolution_clock::now();
    size_t found_count = 0;

    for (size_t i = 0; i < num_keys; i++)
    {
        size_t key_index = dis(gen);
        std::vector<uint8_t> key = makeKey(key_index);
        std::vector<uint8_t> value;
        bool found = false;

        status = index.get(key, xid, &value, &found, nullptr);
        assert(status == Status::OK);

        if (found)
        {
            found_count++;
        }

        // Progress indicator
        if ((i + 1) % 10000 == 0)
        {
            std::cout << "  Progress: " << (i + 1) << " reads completed\n";
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    double throughput = (num_keys * 1000.0) / duration.count();

    std::cout << "  ✓ Performed " << num_keys << " random reads in " << duration.count() << " ms\n";
    std::cout << "  ✓ Read throughput: " << static_cast<int>(throughput) << " ops/sec\n";
    std::cout << "  ✓ Hit rate: " << (found_count * 100 / num_keys) << "%\n";

    // Cleanup
    index.close(nullptr);
    delete db;
    removeDirectory(index_path);
    std::remove(db_path.c_str());
}

/**
 * Test 3: Mixed workload (reads + writes)
 */
void testMixedWorkload()
{
    std::cout << "\n=== Test 3: Mixed Workload (80% reads, 20% writes) ===\n";

    std::string index_path = "lsm_stress_mixed";
    std::string db_path = "lsm_stress_mixed.db";
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

    // Create and populate index
    LSMTreeIndex index(index_path, txn_mgr, 4);
    status = index.create(nullptr);
    assert(status == Status::OK);

    // Initial population: 20K keys
    const size_t initial_keys = 20000;
    std::cout << "  Populating with " << initial_keys << " keys...\n";
    for (size_t i = 0; i < initial_keys; i++)
    {
        std::vector<uint8_t> key = makeKey(i);
        std::vector<uint8_t> value = makeValue(i);
        status = index.put(key, value, xid, nullptr);
        assert(status == Status::OK);
    }
    std::cout << "  ✓ Population complete\n";

    // Mixed workload: 50K operations (80% read, 20% write)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> op_dis(0, 99);
    std::uniform_int_distribution<> key_dis(0, initial_keys - 1);

    const size_t num_ops = 50000;
    size_t read_count = 0;
    size_t write_count = 0;
    size_t next_key = initial_keys;

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < num_ops; i++)
    {
        int op = op_dis(gen);

        if (op < 80)
        {
            // Read operation
            size_t key_index = key_dis(gen);
            std::vector<uint8_t> key = makeKey(key_index);
            std::vector<uint8_t> value;
            bool found = false;
            status = index.get(key, xid, &value, &found, nullptr);
            assert(status == Status::OK);
            read_count++;
        }
        else
        {
            // Write operation
            std::vector<uint8_t> key = makeKey(next_key++);
            std::vector<uint8_t> value = makeValue(next_key);
            status = index.put(key, value, xid, nullptr);
            assert(status == Status::OK);
            write_count++;
        }

        // Progress indicator
        if ((i + 1) % 10000 == 0)
        {
            std::cout << "  Progress: " << (i + 1) << " operations completed\n";
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    double throughput = (num_ops * 1000.0) / duration.count();

    std::cout << "  ✓ Completed " << num_ops << " operations in " << duration.count() << " ms\n";
    std::cout << "  ✓ Mixed throughput: " << static_cast<int>(throughput) << " ops/sec\n";
    std::cout << "  ✓ Reads: " << read_count << ", Writes: " << write_count << "\n";

    // Get final statistics
    LSMTreeIndex::Statistics stats;
    status = index.getStatistics(&stats, nullptr);
    assert(status == Status::OK);
    std::cout << "  ✓ Final Level 0 SSTables: " << stats.level0_sstables << "\n";

    // Cleanup
    index.close(nullptr);
    delete db;
    removeDirectory(index_path);
    std::remove(db_path.c_str());
}

/**
 * Test 4: Data integrity after stress (verify all data)
 */
void testDataIntegrity()
{
    std::cout << "\n=== Test 4: Data Integrity (25K keys) ===\n";

    std::string index_path = "lsm_stress_integrity";
    std::string db_path = "lsm_stress_integrity.db";
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
    LSMTreeIndex index(index_path, txn_mgr, 2);
    status = index.create(nullptr);
    assert(status == Status::OK);

    // Insert 25K keys
    const size_t num_keys = 25000;
    std::cout << "  Inserting " << num_keys << " keys...\n";
    for (size_t i = 0; i < num_keys; i++)
    {
        std::vector<uint8_t> key = makeKey(i);
        std::vector<uint8_t> value = makeValue(i);
        status = index.put(key, value, xid, nullptr);
        assert(status == Status::OK);
    }
    std::cout << "  ✓ Insertion complete\n";

    // Force flush
    status = index.flush(nullptr);
    assert(status == Status::OK);
    std::cout << "  ✓ Flushed memtable\n";

    // Wait for compaction
    std::cout << "  Waiting for background compaction (2 seconds)...\n";
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Verify ALL keys are present and correct
    std::cout << "  Verifying all keys...\n";
    size_t verified = 0;
    size_t corrupted = 0;

    for (size_t i = 0; i < num_keys; i++)
    {
        std::vector<uint8_t> key = makeKey(i);
        std::vector<uint8_t> expected_value = makeValue(i);
        std::vector<uint8_t> actual_value;
        bool found = false;

        status = index.get(key, xid, &actual_value, &found, nullptr);
        assert(status == Status::OK);

        if (!found)
        {
            std::cout << "  ERROR: Key " << i << " not found!\n";
            corrupted++;
        }
        else if (actual_value != expected_value)
        {
            std::cout << "  ERROR: Key " << i << " has wrong value!\n";
            corrupted++;
        }
        else
        {
            verified++;
        }
    }

    std::cout << "  ✓ Verified: " << verified << "/" << num_keys << " keys\n";
    std::cout << "  ✓ Corrupted: " << corrupted << " keys\n";

    assert(verified == num_keys);
    assert(corrupted == 0);

    // Cleanup
    index.close(nullptr);
    delete db;
    removeDirectory(index_path);
    std::remove(db_path.c_str());
}

int main()
{
    std::cout << "========================================\n";
    std::cout << "  LSM-Tree Stress Tests\n";
    std::cout << "========================================\n";

    testWritePerformance();
    testReadPerformance();
    testMixedWorkload();
    testDataIntegrity();

    std::cout << "\n========================================\n";
    std::cout << "  ✅ ALL STRESS TESTS PASSED\n";
    std::cout << "========================================\n";

    return 0;
}
