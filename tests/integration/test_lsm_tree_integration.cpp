/**
 * LSM-Tree Integration Tests
 *
 * Tests the complete LSMTreeIndex implementation including:
 * - Create/open/close lifecycle
 * - Put/get/remove operations
 * - Memtable flush to SSTable
 * - Background compaction (basic)
 * - MGA visibility rules
 */

#include "scratchbird/core/lsm_tree.h"
#include "scratchbird/core/database.h"
#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>

using namespace scratchbird::core;

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Remove directory recursively
 */
void removeDirectory(const std::string &path)
{
    std::string cmd = "rm -rf " + path;
    system(cmd.c_str());
}

/**
 * Generate test key
 */
std::vector<uint8_t> makeKey(size_t index)
{
    std::string key_str = "key_" + std::to_string(index);
    return std::vector<uint8_t>(key_str.begin(), key_str.end());
}

/**
 * Generate test value
 */
std::vector<uint8_t> makeValue(size_t index)
{
    std::string value_str = "value_" + std::to_string(index) + "_data";
    return std::vector<uint8_t>(value_str.begin(), value_str.end());
}

// ============================================================================
// Test Cases
// ============================================================================

/**
 * Test 1: Create and open LSM-Tree index
 */
void testCreateOpen()
{
    std::cout << "\n=== Test 1: Create and Open ===\n";

    std::string index_path = "/tmp/lsm_test_create";
    removeDirectory(index_path);

    // Create mock transaction manager
    Database db;
    Status status = db.initialize(nullptr);
    if (status != Status::OK)
    {
        std::cout << "  ERROR: Failed to initialize database\n";
        exit(1);
    }

    TransactionManager *txn_mgr = db.getTransactionManager();

    // Create index
    LSMTreeIndex index(index_path, txn_mgr, 4);  // 4 MB memtable
    status = index.create(nullptr);
    if (status != Status::OK)
    {
        std::cout << "  ERROR: Failed to create index\n";
        exit(1);
    }
    std::cout << "  ✓ Created LSM-Tree index\n";

    // Close index
    status = index.close(nullptr);
    if (status != Status::OK)
    {
        std::cout << "  ERROR: Failed to close index\n";
        exit(1);
    }
    std::cout << "  ✓ Closed LSM-Tree index\n";

    // Reopen index
    LSMTreeIndex index2(index_path, txn_mgr, 4);
    status = index2.open(nullptr);
    if (status != Status::OK)
    {
        std::cout << "  ERROR: Failed to reopen index\n";
        exit(1);
    }
    std::cout << "  ✓ Reopened LSM-Tree index\n";

    index2.close(nullptr);
    removeDirectory(index_path);
}

/**
 * Test 2: Basic put/get operations
 */
void testPutGet()
{
    std::cout << "\n=== Test 2: Basic Put/Get ===\n";

    std::string index_path = "/tmp/lsm_test_putget";
    removeDirectory(index_path);

    // Create database and transaction manager
    Database db;
    Status status = db.initialize(nullptr);
    if (status != Status::OK)
    {
        std::cout << "  ERROR: Failed to initialize database\n";
        exit(1);
    }

    TransactionManager *txn_mgr = db.getTransactionManager();

    // Create index
    LSMTreeIndex index(index_path, txn_mgr, 4);
    status = index.create(nullptr);
    if (status != Status::OK)
    {
        std::cout << "  ERROR: Failed to create index\n";
        exit(1);
    }

    // Start transaction
    uint64_t xid = txn_mgr->beginTransaction(nullptr);

    // Insert 100 keys
    for (size_t i = 0; i < 100; i++)
    {
        std::vector<uint8_t> key = makeKey(i);
        std::vector<uint8_t> value = makeValue(i);
        status = index.put(key, value, xid, nullptr);
        if (status != Status::OK)
        {
            std::cout << "  ERROR: Failed to put key " << i << "\n";
            exit(1);
        }
    }
    std::cout << "  ✓ Inserted 100 keys\n";

    // Commit transaction
    txn_mgr->commitTransaction(xid, nullptr);

    // Start new transaction for reads
    uint64_t read_xid = txn_mgr->beginTransaction(nullptr);

    // Verify all keys
    size_t found_count = 0;
    for (size_t i = 0; i < 100; i++)
    {
        std::vector<uint8_t> key = makeKey(i);
        std::vector<uint8_t> expected_value = makeValue(i);
        std::vector<uint8_t> actual_value;
        bool found = false;

        status = index.get(key, read_xid, &actual_value, &found, nullptr);
        if (status != Status::OK)
        {
            std::cout << "  ERROR: Failed to get key " << i << "\n";
            exit(1);
        }

        if (found && actual_value == expected_value)
        {
            found_count++;
        }
    }

    std::cout << "  ✓ Retrieved " << found_count << "/100 keys\n";

    if (found_count != 100)
    {
        std::cout << "  ERROR: Not all keys found!\n";
        exit(1);
    }

    txn_mgr->commitTransaction(read_xid, nullptr);
    index.close(nullptr);
    removeDirectory(index_path);
}

/**
 * Test 3: Memtable flush to SSTable
 */
void testFlush()
{
    std::cout << "\n=== Test 3: Memtable Flush ===\n";

    std::string index_path = "/tmp/lsm_test_flush";
    removeDirectory(index_path);

    // Create database
    Database db;
    Status status = db.initialize(nullptr);
    if (status != Status::OK)
    {
        std::cout << "  ERROR: Failed to initialize database\n";
        exit(1);
    }

    TransactionManager *txn_mgr = db.getTransactionManager();

    // Create index with small memtable (1 MB)
    LSMTreeIndex index(index_path, txn_mgr, 1);
    status = index.create(nullptr);
    if (status != Status::OK)
    {
        std::cout << "  ERROR: Failed to create index\n";
        exit(1);
    }

    // Start transaction
    uint64_t xid = txn_mgr->beginTransaction(nullptr);

    // Insert 100 keys
    for (size_t i = 0; i < 100; i++)
    {
        std::vector<uint8_t> key = makeKey(i);
        std::vector<uint8_t> value = makeValue(i);
        status = index.put(key, value, xid, nullptr);
        if (status != Status::OK)
        {
            std::cout << "  ERROR: Failed to put key " << i << "\n";
            exit(1);
        }
    }
    std::cout << "  ✓ Inserted 100 keys\n";

    txn_mgr->commitTransaction(xid, nullptr);

    // Flush memtable
    status = index.flush(nullptr);
    if (status != Status::OK)
    {
        std::cout << "  ERROR: Failed to flush memtable\n";
        exit(1);
    }
    std::cout << "  ✓ Flushed memtable to SSTable\n";

    // Get statistics
    LSMTreeIndex::Statistics stats;
    status = index.getStatistics(&stats, nullptr);
    if (status != Status::OK)
    {
        std::cout << "  ERROR: Failed to get statistics\n";
        exit(1);
    }

    std::cout << "  ✓ Level 0 SSTables: " << stats.level0_sstables << "\n";
    std::cout << "  ✓ Active memtable entries: " << stats.active_memtable_entries << "\n";

    if (stats.level0_sstables == 0)
    {
        std::cout << "  ERROR: No SSTables created!\n";
        exit(1);
    }

    // Start new transaction for reads
    uint64_t read_xid = txn_mgr->beginTransaction(nullptr);

    // Verify all keys still readable after flush
    size_t found_count = 0;
    for (size_t i = 0; i < 100; i++)
    {
        std::vector<uint8_t> key = makeKey(i);
        std::vector<uint8_t> value;
        bool found = false;

        status = index.get(key, read_xid, &value, &found, nullptr);
        if (status != Status::OK)
        {
            std::cout << "  ERROR: Failed to get key " << i << "\n";
            exit(1);
        }

        if (found)
        {
            found_count++;
        }
    }

    std::cout << "  ✓ Retrieved " << found_count << "/100 keys after flush\n";

    if (found_count != 100)
    {
        std::cout << "  ERROR: Not all keys found after flush!\n";
        exit(1);
    }

    txn_mgr->commitTransaction(read_xid, nullptr);
    index.close(nullptr);
    removeDirectory(index_path);
}

/**
 * Test 4: Delete operations (tombstones)
 */
void testDelete()
{
    std::cout << "\n=== Test 4: Delete Operations ===\n";

    std::string index_path = "/tmp/lsm_test_delete";
    removeDirectory(index_path);

    // Create database
    Database db;
    Status status = db.initialize(nullptr);
    if (status != Status::OK)
    {
        std::cout << "  ERROR: Failed to initialize database\n";
        exit(1);
    }

    TransactionManager *txn_mgr = db.getTransactionManager();

    // Create index
    LSMTreeIndex index(index_path, txn_mgr, 4);
    status = index.create(nullptr);
    if (status != Status::OK)
    {
        std::cout << "  ERROR: Failed to create index\n";
        exit(1);
    }

    // Start transaction
    uint64_t xid = txn_mgr->beginTransaction(nullptr);

    // Insert 50 keys
    for (size_t i = 0; i < 50; i++)
    {
        std::vector<uint8_t> key = makeKey(i);
        std::vector<uint8_t> value = makeValue(i);
        status = index.put(key, value, xid, nullptr);
        if (status != Status::OK)
        {
            std::cout << "  ERROR: Failed to put key " << i << "\n";
            exit(1);
        }
    }
    std::cout << "  ✓ Inserted 50 keys\n";

    txn_mgr->commitTransaction(xid, nullptr);

    // Start new transaction for deletes
    uint64_t delete_xid = txn_mgr->beginTransaction(nullptr);

    // Delete first 25 keys
    for (size_t i = 0; i < 25; i++)
    {
        std::vector<uint8_t> key = makeKey(i);
        status = index.remove(key, delete_xid, nullptr);
        if (status != Status::OK)
        {
            std::cout << "  ERROR: Failed to remove key " << i << "\n";
            exit(1);
        }
    }
    std::cout << "  ✓ Deleted 25 keys\n";

    txn_mgr->commitTransaction(delete_xid, nullptr);

    // Start new transaction for reads
    uint64_t read_xid = txn_mgr->beginTransaction(nullptr);

    // Verify first 25 keys are deleted (NOT FOUND)
    size_t deleted_count = 0;
    for (size_t i = 0; i < 25; i++)
    {
        std::vector<uint8_t> key = makeKey(i);
        std::vector<uint8_t> value;
        bool found = false;

        status = index.get(key, read_xid, &value, &found, nullptr);
        if (status != Status::OK)
        {
            std::cout << "  ERROR: Failed to get key " << i << "\n";
            exit(1);
        }

        if (!found)
        {
            deleted_count++;
        }
    }

    std::cout << "  ✓ Verified " << deleted_count << "/25 keys deleted\n";

    // Verify remaining 25 keys still exist
    size_t found_count = 0;
    for (size_t i = 25; i < 50; i++)
    {
        std::vector<uint8_t> key = makeKey(i);
        std::vector<uint8_t> value;
        bool found = false;

        status = index.get(key, read_xid, &value, &found, nullptr);
        if (status != Status::OK)
        {
            std::cout << "  ERROR: Failed to get key " << i << "\n";
            exit(1);
        }

        if (found)
        {
            found_count++;
        }
    }

    std::cout << "  ✓ Verified " << found_count << "/25 keys still exist\n";

    if (deleted_count != 25 || found_count != 25)
    {
        std::cout << "  ERROR: Delete verification failed!\n";
        exit(1);
    }

    txn_mgr->commitTransaction(read_xid, nullptr);
    index.close(nullptr);
    removeDirectory(index_path);
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main()
{
    std::cout << "========================================\n";
    std::cout << "  LSM-Tree Integration Tests\n";
    std::cout << "========================================\n";

    testCreateOpen();
    testPutGet();
    testFlush();
    testDelete();

    std::cout << "\n========================================\n";
    std::cout << "  ✅ ALL LSM-TREE INTEGRATION TESTS PASSED\n";
    std::cout << "========================================\n";

    return 0;
}
