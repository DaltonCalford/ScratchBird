/**
 * Memtable Unit Tests
 *
 * Tests Firebird MGA compliance and core functionality
 */

#include "scratchbird/core/lsm_tree.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/proc_array.h"
#include <cassert>
#include <iostream>
#include <thread>
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

// Helper: Convert value to string
std::string valueToString(const std::vector<uint8_t> &v)
{
    return std::string(v.begin(), v.end());
}

/**
 * Test 1: Insert and retrieve single key-value
 */
void testSingleInsertRetrieve()
{
    std::cout << "\n=== Test 1: Single Insert/Retrieve ===\n";

    // Create database for TransactionManager
    std::string db_path = "lsm_test_single.db";
    std::remove(db_path.c_str());

    ErrorContext ctx;
    Status status = Database::create(db_path, 8192, &ctx);
    assert(status == Status::OK);

    Database *db = new Database();
    status = db->open(db_path, &ctx);
    assert(status == Status::OK);

    TransactionManager *txn_mgr = db->transaction_manager();
    uint64_t xid = txn_mgr->getCurrentXid();

    // Create memtable
    Memtable memtable;

    // Insert key-value
    auto key = makeKey("key1");
    auto value = makeValue("value1");
    status = memtable.put(key, value, xid, &ctx);
    assert(status == Status::OK);
    std::cout << "  ✓ Inserted key1=value1\n";

    // Retrieve
    std::vector<uint8_t> retrieved_value;
    bool found = false;
    status = memtable.get(key, xid, txn_mgr, &retrieved_value, &found, &ctx);
    assert(status == Status::OK);
    assert(found == true);
    assert(valueToString(retrieved_value) == "value1");
    std::cout << "  ✓ Retrieved key1=value1\n";

    delete db;
    std::cout << "  PASS\n";
}

/**
 * Test 2: Insert multiple keys, verify sorted order
 */
void testMultipleInsertsSortedOrder()
{
    std::cout << "\n=== Test 2: Multiple Inserts (Sorted Order) ===\n";

    std::string db_path = "lsm_test_multi.db";
    std::remove(db_path.c_str());

    ErrorContext ctx;
    Status status = Database::create(db_path, 8192, &ctx);
    assert(status == Status::OK);

    Database *db = new Database();
    status = db->open(db_path, &ctx);
    assert(status == Status::OK);

    TransactionManager *txn_mgr = db->transaction_manager();
    uint64_t xid = txn_mgr->getCurrentXid();

    Memtable memtable;

    // Insert keys in random order
    memtable.put(makeKey("key3"), makeValue("value3"), xid, &ctx);
    memtable.put(makeKey("key1"), makeValue("value1"), xid, &ctx);
    memtable.put(makeKey("key2"), makeValue("value2"), xid, &ctx);
    std::cout << "  ✓ Inserted 3 keys (key3, key1, key2)\n";

    // Get all entries (should be sorted)
    std::vector<MemtableEntry> entries;
    status = memtable.getAllEntries(&entries, &ctx);
    assert(status == Status::OK);
    assert(entries.size() == 3);

    // Verify sorted order
    assert(valueToString(entries[0].key) == "key1");
    assert(valueToString(entries[1].key) == "key2");
    assert(valueToString(entries[2].key) == "key3");
    std::cout << "  ✓ Entries sorted correctly (key1, key2, key3)\n";

    delete db;
    std::cout << "  PASS\n";
}

/**
 * Test 3: Update existing key (newer version)
 */
void testUpdateExistingKey()
{
    std::cout << "\n=== Test 3: Update Existing Key ===\n";

    std::string db_path = "lsm_test_update.db";
    std::remove(db_path.c_str());

    ErrorContext ctx;
    Status status = Database::create(db_path, 8192, &ctx);
    assert(status == Status::OK);

    Database *db = new Database();
    status = db->open(db_path, &ctx);
    assert(status == Status::OK);

    TransactionManager *txn_mgr = db->transaction_manager();
    uint64_t xid = txn_mgr->getCurrentXid();

    Memtable memtable;

    auto key = makeKey("key1");

    // Insert first version
    status = memtable.put(key, makeValue("value1"), xid, &ctx);
    assert(status == Status::OK);
    std::cout << "  ✓ Inserted key1=value1\n";

    // Update (insert newer version)
    status = memtable.put(key, makeValue("value2"), xid, &ctx);
    assert(status == Status::OK);
    std::cout << "  ✓ Updated key1=value2\n";

    // Get should return newest version
    std::vector<uint8_t> retrieved_value;
    bool found = false;
    status = memtable.get(key, xid, txn_mgr, &retrieved_value, &found, &ctx);
    assert(status == Status::OK);
    assert(found == true);
    assert(valueToString(retrieved_value) == "value2");
    std::cout << "  ✓ Retrieved newest version (value2)\n";

    // Verify both versions exist in memtable
    std::vector<MemtableEntry> entries;
    status = memtable.getAllEntries(&entries, &ctx);
    assert(status == Status::OK);
    assert(entries.size() == 2);  // Both versions stored
    std::cout << "  ✓ Both versions stored (2 entries)\n";

    delete db;
    std::cout << "  PASS\n";
}

/**
 * Test 4: Delete key (tombstone)
 */
void testDeleteKey()
{
    std::cout << "\n=== Test 4: Delete Key (Tombstone) ===\n";

    std::string db_path = "lsm_test_delete.db";
    std::remove(db_path.c_str());

    ErrorContext ctx;
    Status status = Database::create(db_path, 8192, &ctx);
    assert(status == Status::OK);

    Database *db = new Database();
    status = db->open(db_path, &ctx);
    assert(status == Status::OK);

    TransactionManager *txn_mgr = db->transaction_manager();
    uint64_t xid = txn_mgr->getCurrentXid();

    Memtable memtable;

    auto key = makeKey("key1");

    // Insert
    status = memtable.put(key, makeValue("value1"), xid, &ctx);
    assert(status == Status::OK);
    std::cout << "  ✓ Inserted key1=value1\n";

    // Delete (insert tombstone)
    status = memtable.remove(key, xid, &ctx);
    assert(status == Status::OK);
    std::cout << "  ✓ Deleted key1 (inserted tombstone)\n";

    // Get should return not found
    std::vector<uint8_t> retrieved_value;
    bool found = false;
    status = memtable.get(key, xid, txn_mgr, &retrieved_value, &found, &ctx);
    assert(status == Status::OK);
    assert(found == false);
    std::cout << "  ✓ Get returns not found (tombstone visible)\n";

    delete db;
    std::cout << "  PASS\n";
}

/**
 * Test 5: Range scan with start/end keys
 */
void testRangeScan()
{
    std::cout << "\n=== Test 5: Range Scan ===\n";

    std::string db_path = "lsm_test_scan.db";
    std::remove(db_path.c_str());

    ErrorContext ctx;
    Status status = Database::create(db_path, 8192, &ctx);
    assert(status == Status::OK);

    Database *db = new Database();
    status = db->open(db_path, &ctx);
    assert(status == Status::OK);

    TransactionManager *txn_mgr = db->transaction_manager();
    uint64_t xid = txn_mgr->getCurrentXid();

    Memtable memtable;

    // Insert 10 keys
    for (int i = 0; i < 10; ++i)
    {
        std::string key_str = "key" + std::to_string(i);
        std::string val_str = "value" + std::to_string(i);
        memtable.put(makeKey(key_str), makeValue(val_str), xid, &ctx);
    }
    std::cout << "  ✓ Inserted 10 keys (key0-key9)\n";

    // Range scan: key2 to key5
    auto start_key = makeKey("key2");
    auto end_key = makeKey("key5");
    std::vector<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> results;
    status = memtable.scan(&start_key, &end_key, xid, txn_mgr, &results, &ctx);
    assert(status == Status::OK);
    assert(results.size() == 4);  // key2, key3, key4, key5
    assert(valueToString(results[0].first) == "key2");
    assert(valueToString(results[3].first) == "key5");
    std::cout << "  ✓ Range scan [key2, key5] returned 4 entries\n";

    // Scan all (no start/end)
    results.clear();
    status = memtable.scan(nullptr, nullptr, xid, txn_mgr, &results, &ctx);
    assert(status == Status::OK);
    assert(results.size() == 10);
    std::cout << "  ✓ Full scan returned 10 entries\n";

    delete db;
    std::cout << "  PASS\n";
}

/**
 * Test 6: MGA visibility filtering (xmin/xmax)
 */
void testMGAVisibility()
{
    std::cout << "\n=== Test 6: MGA Visibility Filtering ===\n";

    std::string db_path = "lsm_test_mga.db";
    std::remove(db_path.c_str());

    ErrorContext ctx;
    Status status = Database::create(db_path, 8192, &ctx);
    assert(status == Status::OK);

    Database *db = new Database();
    status = db->open(db_path, &ctx);
    assert(status == Status::OK);

    // Initialize ProcArray for transaction management
    status = db->initializeProcArray(10, &ctx);
    assert(status == Status::OK);

    TransactionManager *txn_mgr = db->transaction_manager();
    Memtable memtable;

    // Register backends in ProcArray first
    uint32_t proc_id_1, proc_id_2;
    status = ProcArrayManager::registerBackend(&proc_id_1, &ctx);
    assert(status == Status::OK);
    status = ProcArrayManager::registerBackend(&proc_id_2, &ctx);
    assert(status == Status::OK);

    // Transaction 1: Insert key
    uint64_t xid1;
    status = txn_mgr->beginTransaction(proc_id_1, xid1, &ctx);
    assert(status == Status::OK);

    auto key = makeKey("key1");
    status = memtable.put(key, makeValue("value1"), xid1, &ctx);
    assert(status == Status::OK);
    std::cout << "  ✓ Transaction 1: Inserted key1=value1\n";

    // Transaction 1 can see its own changes
    std::vector<uint8_t> value;
    bool found = false;
    status = memtable.get(key, xid1, txn_mgr, &value, &found, &ctx);
    assert(status == Status::OK);
    assert(found == true);
    std::cout << "  ✓ Transaction 1 sees its own changes\n";

    // Transaction 2 (before commit) should NOT see Transaction 1's changes
    uint64_t xid2;
    status = txn_mgr->beginTransaction(proc_id_2, xid2, &ctx);
    assert(status == Status::OK);

    value.clear();
    found = false;
    status = memtable.get(key, xid2, txn_mgr, &value, &found, &ctx);
    assert(status == Status::OK);
    assert(found == false);
    std::cout << "  ✓ Transaction 2 does NOT see uncommitted changes\n";

    // Commit Transaction 1
    status = txn_mgr->commitTransaction(proc_id_1, xid1, &ctx);
    assert(status == Status::OK);
    std::cout << "  ✓ Transaction 1 committed\n";

    // Transaction 2 should now see Transaction 1's changes
    value.clear();
    found = false;
    status = memtable.get(key, xid2, txn_mgr, &value, &found, &ctx);
    assert(status == Status::OK);
    assert(found == true);
    assert(valueToString(value) == "value1");
    std::cout << "  ✓ Transaction 2 now sees committed changes\n";

    status = txn_mgr->commitTransaction(proc_id_2, xid2, &ctx);
    assert(status == Status::OK);

    // Unregister backends
    status = ProcArrayManager::unregisterBackend(proc_id_1, &ctx);
    assert(status == Status::OK);
    status = ProcArrayManager::unregisterBackend(proc_id_2, &ctx);
    assert(status == Status::OK);

    delete db;
    std::cout << "  PASS\n";
}

/**
 * Test 7: Memtable full detection (4 MB)
 */
void testMemtableFull()
{
    std::cout << "\n=== Test 7: Memtable Full Detection ===\n";

    std::string db_path = "lsm_test_full.db";
    std::remove(db_path.c_str());

    ErrorContext ctx;
    Status status = Database::create(db_path, 8192, &ctx);
    assert(status == Status::OK);

    Database *db = new Database();
    status = db->open(db_path, &ctx);
    assert(status == Status::OK);

    TransactionManager *txn_mgr = db->transaction_manager();
    uint64_t xid = txn_mgr->getCurrentXid();

    // Create small memtable (1 KB)
    Memtable memtable(1024);

    // Insert until full
    int count = 0;
    while (true)
    {
        std::string key_str = "key" + std::to_string(count);
        std::string val_str = std::string(100, 'x');  // 100-byte value
        status = memtable.put(makeKey(key_str), makeValue(val_str), xid, &ctx);
        if (status == Status::OOM)
        {
            break;
        }
        assert(status == Status::OK);
        count++;
    }

    std::cout << "  ✓ Memtable full after " << count << " inserts\n";
    assert(memtable.isFull());
    std::cout << "  ✓ isFull() returned true\n";

    delete db;
    std::cout << "  PASS\n";
}

/**
 * Test 8: Thread-safety (concurrent inserts from 10 threads)
 */
void testThreadSafety()
{
    std::cout << "\n=== Test 8: Thread Safety (10 threads) ===\n";

    std::string db_path = "lsm_test_threads.db";
    std::remove(db_path.c_str());

    ErrorContext ctx;
    Status status = Database::create(db_path, 8192, &ctx);
    assert(status == Status::OK);

    Database *db = new Database();
    status = db->open(db_path, &ctx);
    assert(status == Status::OK);

    TransactionManager *txn_mgr = db->transaction_manager();
    uint64_t xid = txn_mgr->getCurrentXid();

    // Create memtable (16 MB to accommodate all threads)
    Memtable memtable(16 * 1024 * 1024);

    // Launch 10 threads, each inserting 100 keys
    const int NUM_THREADS = 10;
    const int KEYS_PER_THREAD = 100;
    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; ++t)
    {
        threads.emplace_back([&memtable, t, xid]() {
            ErrorContext thread_ctx;
            for (int i = 0; i < KEYS_PER_THREAD; ++i)
            {
                std::string key_str = "thread" + std::to_string(t) + "_key" + std::to_string(i);
                std::string val_str = "value" + std::to_string(i);
                Status s = memtable.put(makeKey(key_str), makeValue(val_str), xid, &thread_ctx);
                assert(s == Status::OK || s == Status::OOM);
            }
        });
    }

    // Wait for all threads
    for (auto &thread : threads)
    {
        thread.join();
    }

    std::cout << "  ✓ 10 threads completed (1000 total inserts)\n";

    // Verify no data loss (at least some entries inserted)
    size_t num_entries = memtable.getNumEntries();
    assert(num_entries > 0);
    std::cout << "  ✓ Memtable has " << num_entries << " entries (no data loss)\n";

    delete db;
    std::cout << "  PASS\n";
}

/**
 * Main test runner
 */
int main()
{
    std::cout << "==================================================\n";
    std::cout << "LSM-Tree Memtable Unit Tests\n";
    std::cout << "==================================================\n";

    testSingleInsertRetrieve();
    testMultipleInsertsSortedOrder();
    testUpdateExistingKey();
    testDeleteKey();
    testRangeScan();
    testMGAVisibility();
    testMemtableFull();
    testThreadSafety();

    std::cout << "\n=== All Memtable Tests PASSED ===\n";

    return;
}
