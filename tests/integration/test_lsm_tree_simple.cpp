/**
 * LSM-Tree Simple Integration Tests
 *
 * Simplified tests focusing on LSM-Tree functionality
 */

#include "scratchbird/core/lsm_tree.h"
#include "scratchbird/core/database.h"
#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>

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

std::vector<uint8_t> makeValue(size_t index)
{
    std::string value_str = "value_" + std::to_string(index) + "_data";
    return std::vector<uint8_t>(value_str.begin(), value_str.end());
}

void testBasicPutGet()
{
    std::cout << "\n=== Test: Basic Put/Get ===\n";

    std::string suffix = "_" + std::to_string(getpid());
    std::string index_path = "/tmp/lsm_test_simple" + suffix;
    std::string db_path = "/tmp/lsm_test_simple" + suffix + ".db";
    removeDirectory(index_path);
    std::remove(db_path.c_str());

    // Create and open database
    Status status = Database::create(db_path, 8192, nullptr);
    if (status != Status::OK)
    {
        std::cout << "  ERROR: Failed to create database\n";
        exit(1);
    }

    Database *db = new Database();
    status = db->open(db_path, nullptr);
    if (status != Status::OK)
    {
        std::cout << "  ERROR: Failed to open database\n";
        exit(1);
    }

    TransactionManager *txn_mgr = db->transaction_manager();
    uint64_t xid = txn_mgr->getCurrentXid();

    // Create LSM-Tree index
    LSMTreeIndex index(index_path, txn_mgr, 4);
    status = index.create(nullptr);
    if (status != Status::OK)
    {
        std::cout << "  ERROR: Failed to create index\n";
        exit(1);
    }
    std::cout << "  ✓ Created LSM-Tree index\n";

    // Insert 10 keys
    for (size_t i = 0; i < 10; i++)
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
    std::cout << "  ✓ Inserted 10 keys\n";

    // Retrieve keys
    size_t found_count = 0;
    for (size_t i = 0; i < 10; i++)
    {
        std::vector<uint8_t> key = makeKey(i);
        std::vector<uint8_t> expected_value = makeValue(i);
        std::vector<uint8_t> actual_value;
        bool found = false;

        status = index.get(key, xid, &actual_value, &found, nullptr);
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

    std::cout << "  ✓ Retrieved " << found_count << "/10 keys correctly\n";

    if (found_count != 10)
    {
        std::cout << "  ERROR: Not all keys found!\n";
        exit(1);
    }

    // Cleanup
    index.close(nullptr);
    delete db;
    removeDirectory(index_path);
    std::remove(db_path.c_str());
}

int main()
{
    std::cout << "========================================\n";
    std::cout << "  LSM-Tree Simple Integration Tests\n";
    std::cout << "========================================\n";

    testBasicPutGet();

    std::cout << "\n========================================\n";
    std::cout << "  ✅ ALL TESTS PASSED\n";
    std::cout << "========================================\n";

    return 0;
}
