/**
 * SSTable Reader Unit Tests
 *
 * Tests SSTable read operations and correctness
 */

#include "scratchbird/core/lsm_tree.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/transaction_manager.h"
#include <cassert>
#include <iostream>
#include <cstdio>
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

// Helper: Create test SSTable
void createTestSSTable(const std::string &file_path,
                       const std::vector<std::pair<std::string, std::string>> &entries,
                       uint64_t base_xid = 1)
{
    SSTableWriter writer(file_path);
    ErrorContext ctx;

    Status status = writer.open(&ctx);
    assert(status == Status::OK);

    for (size_t i = 0; i < entries.size(); ++i)
    {
        status = writer.addEntry(makeKey(entries[i].first),
                                  makeValue(entries[i].second),
                                  i,                // sequence
                                  0,                // INSERT
                                  base_xid,         // xmin (all same transaction)
                                  0,                // xmax
                                  &ctx);
        assert(status == Status::OK);
    }

    status = writer.finish(&ctx);
    assert(status == Status::OK);
}

/**
 * Test 1: Open SSTable and read metadata
 */
void testOpenSSTable()
{
    std::cout << "\n=== Test 1: Open SSTable and Read Metadata ===\n";

    std::string file_path = "test_sstable_reader_open.sst";
    std::remove(file_path.c_str());

    // Create SSTable with 10 entries
    std::vector<std::pair<std::string, std::string>> entries = {
        {"key01", "value01"},
        {"key02", "value02"},
        {"key03", "value03"},
        {"key04", "value04"},
        {"key05", "value05"},
        {"key06", "value06"},
        {"key07", "value07"},
        {"key08", "value08"},
        {"key09", "value09"},
        {"key10", "value10"}
    };

    createTestSSTable(file_path, entries);
    std::cout << "  ✓ Created test SSTable\n";

    // Open SSTable for reading
    SSTableReader reader(file_path);
    ErrorContext ctx;

    Status status = reader.open(&ctx);
    assert(status == Status::OK);
    assert(reader.isOpen());
    std::cout << "  ✓ Opened SSTable successfully\n";

    // Check metadata
    assert(reader.getNumEntries() == 10);
    std::cout << "  ✓ Num entries: 10\n";

    std::vector<uint8_t> min_key = reader.getMinKey();
    std::vector<uint8_t> max_key = reader.getMaxKey();
    assert(toString(min_key) == "key01");
    assert(toString(max_key) == "key10");
    std::cout << "  ✓ Min/max keys correct (key01, key10)\n";

    std::cout << "  PASS\n";
}

/**
 * Test 2: Point query (get operation)
 */
void testPointQuery()
{
    std::cout << "\n=== Test 2: Point Query (get) ===\n";

    // Create database for transaction manager
    std::string db_path = "/tmp/test_sstable_reader_db";
    std::system(("rm -rf " + db_path + "*").c_str());

    Database *db = new Database();
    ErrorContext ctx;
    Status status = Database::create(db_path, 16384, &ctx);
    assert(status == Status::OK);

    status = db->open(db_path, &ctx);
    assert(status == Status::OK);

    status = db->initializeProcArray(10, &ctx);
    assert(status == Status::OK);

    TransactionManager *txn_mgr = db->transaction_manager();

    // Register backend for writer transaction
    uint32_t writer_proc_id;
    status = ProcArrayManager::registerBackend(&writer_proc_id, &ctx);
    assert(status == Status::OK);

    // Start writer transaction
    uint64_t writer_xid;
    status = txn_mgr->beginTransaction(writer_proc_id, writer_xid, &ctx);
    assert(status == Status::OK);

    // Create SSTable
    std::string file_path = "test_sstable_reader_point.sst";
    std::remove(file_path.c_str());

    std::vector<std::pair<std::string, std::string>> entries = {
        {"apple", "red"},
        {"banana", "yellow"},
        {"cherry", "red"},
        {"date", "brown"},
        {"elderberry", "purple"}
    };

    createTestSSTable(file_path, entries, writer_xid);
    std::cout << "  ✓ Created test SSTable\n";

    // Commit the writer transaction
    status = txn_mgr->commitTransaction(writer_proc_id, writer_xid, &ctx);
    assert(status == Status::OK);

    status = ProcArrayManager::unregisterBackend(writer_proc_id, &ctx);
    assert(status == Status::OK);

    // Now start reader transaction
    uint32_t reader_proc_id;
    status = ProcArrayManager::registerBackend(&reader_proc_id, &ctx);
    assert(status == Status::OK);

    uint64_t reader_xid;
    status = txn_mgr->beginTransaction(reader_proc_id, reader_xid, &ctx);
    assert(status == Status::OK);
    std::cout << "  ✓ Reader transaction started (xid=" << reader_xid << ")\n";

    // Open SSTable for reading
    SSTableReader reader(file_path);
    status = reader.open(&ctx);
    assert(status == Status::OK);

    // Test get operations
    std::vector<uint8_t> value;
    bool found;

    // Query existing key
    status = reader.get(makeKey("banana"), reader_xid, txn_mgr, &value, &found, &ctx);
    assert(status == Status::OK);
    assert(found == true);
    assert(toString(value) == "yellow");
    std::cout << "  ✓ Found key 'banana' = 'yellow'\n";

    // Query another existing key
    status = reader.get(makeKey("cherry"), reader_xid, txn_mgr, &value, &found, &ctx);
    assert(status == Status::OK);
    assert(found == true);
    assert(toString(value) == "red");
    std::cout << "  ✓ Found key 'cherry' = 'red'\n";

    // Query non-existing key
    status = reader.get(makeKey("fig"), reader_xid, txn_mgr, &value, &found, &ctx);
    assert(status == Status::OK);
    assert(found == false);
    std::cout << "  ✓ Key 'fig' not found (expected)\n";

    // Cleanup
    status = txn_mgr->commitTransaction(reader_proc_id, reader_xid, &ctx);
    assert(status == Status::OK);

    status = ProcArrayManager::unregisterBackend(reader_proc_id, &ctx);
    assert(status == Status::OK);

    delete db;

    std::cout << "  PASS\n";
}

/**
 * Test 3: Range scan
 */
void testRangeScan()
{
    std::cout << "\n=== Test 3: Range Scan ===\n";

    // Create database
    std::string db_path = "/tmp/test_sstable_reader_scan_db";
    std::remove((db_path + ".db").c_str());

    Database *db = new Database();
    ErrorContext ctx;
    Status status = Database::create(db_path, 16384, &ctx);
    assert(status == Status::OK);

    status = db->open(db_path, &ctx);
    assert(status == Status::OK);

    status = db->initializeProcArray(10, &ctx);
    assert(status == Status::OK);

    TransactionManager *txn_mgr = db->transaction_manager();

    // Register backend for writer transaction
    uint32_t writer_proc_id;
    status = ProcArrayManager::registerBackend(&writer_proc_id, &ctx);
    assert(status == Status::OK);

    // Start writer transaction
    uint64_t writer_xid;
    status = txn_mgr->beginTransaction(writer_proc_id, writer_xid, &ctx);
    assert(status == Status::OK);

    // Create SSTable with sorted keys
    std::string file_path = "test_sstable_reader_scan.sst";
    std::remove(file_path.c_str());

    std::vector<std::pair<std::string, std::string>> entries = {
        {"key10", "value10"},
        {"key20", "value20"},
        {"key30", "value30"},
        {"key40", "value40"},
        {"key50", "value50"},
        {"key60", "value60"},
        {"key70", "value70"},
        {"key80", "value80"},
        {"key90", "value90"}
    };

    createTestSSTable(file_path, entries, writer_xid);

    // Commit the writer transaction
    status = txn_mgr->commitTransaction(writer_proc_id, writer_xid, &ctx);
    assert(status == Status::OK);

    status = ProcArrayManager::unregisterBackend(writer_proc_id, &ctx);
    assert(status == Status::OK);

    // Now start reader transaction
    uint32_t reader_proc_id;
    status = ProcArrayManager::registerBackend(&reader_proc_id, &ctx);
    assert(status == Status::OK);

    uint64_t reader_xid;
    status = txn_mgr->beginTransaction(reader_proc_id, reader_xid, &ctx);
    assert(status == Status::OK);
    std::cout << "  ✓ Reader transaction started (xid=" << reader_xid << ")\n";

    // Open SSTable
    SSTableReader reader(file_path);
    status = reader.open(&ctx);
    assert(status == Status::OK);

    // Range scan: key20 to key60 (exclusive)
    std::vector<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> results;
    status = reader.scan(makeKey("key20"), makeKey("key60"), reader_xid, txn_mgr, &results, &ctx);
    assert(status == Status::OK);
    assert(results.size() == 4); // key20, key30, key40, key50
    assert(toString(results[0].first) == "key20");
    assert(toString(results[3].first) == "key50");
    std::cout << "  ✓ Range scan [key20, key60): 4 results\n";

    // Full scan (empty start/end)
    results.clear();
    status = reader.scan(std::vector<uint8_t>(), std::vector<uint8_t>(), reader_xid, txn_mgr, &results, &ctx);
    assert(status == Status::OK);
    assert(results.size() == 9); // All entries
    std::cout << "  ✓ Full scan: 9 results\n";

    // Scan from start
    results.clear();
    status = reader.scan(std::vector<uint8_t>(), makeKey("key40"), reader_xid, txn_mgr, &results, &ctx);
    assert(status == Status::OK);
    assert(results.size() == 3); // key10, key20, key30
    std::cout << "  ✓ Scan [start, key40): 3 results\n";

    // Scan to end
    results.clear();
    status = reader.scan(makeKey("key70"), std::vector<uint8_t>(), reader_xid, txn_mgr, &results, &ctx);
    assert(status == Status::OK);
    assert(results.size() == 3); // key70, key80, key90
    std::cout << "  ✓ Scan [key70, end): 3 results\n";

    // Cleanup
    status = txn_mgr->commitTransaction(reader_proc_id, reader_xid, &ctx);
    assert(status == Status::OK);

    status = ProcArrayManager::unregisterBackend(reader_proc_id, &ctx);
    assert(status == Status::OK);

    delete db;

    std::cout << "  PASS\n";
}

/**
 * Test 4: Bloom filter optimization
 */
void testBloomFilterOptimization()
{
    std::cout << "\n=== Test 4: Bloom Filter Optimization ===\n";

    // Create database
    std::string db_path = "/tmp/test_sstable_reader_bloom_db";
    std::remove((db_path + ".db").c_str());

    Database *db = new Database();
    ErrorContext ctx;
    Status status = Database::create(db_path, 16384, &ctx);
    assert(status == Status::OK);

    status = db->open(db_path, &ctx);
    assert(status == Status::OK);

    status = db->initializeProcArray(10, &ctx);
    assert(status == Status::OK);

    TransactionManager *txn_mgr = db->transaction_manager();

    // Register backend for writer transaction
    uint32_t writer_proc_id;
    status = ProcArrayManager::registerBackend(&writer_proc_id, &ctx);
    assert(status == Status::OK);

    // Start writer transaction
    uint64_t writer_xid;
    status = txn_mgr->beginTransaction(writer_proc_id, writer_xid, &ctx);
    assert(status == Status::OK);

    // Create SSTable with specific keys
    std::string file_path = "test_sstable_reader_bloom.sst";
    std::remove(file_path.c_str());

    std::vector<std::pair<std::string, std::string>> entries = {
        {"alpha", "1"},
        {"beta", "2"},
        {"gamma", "3"}
    };

    createTestSSTable(file_path, entries, writer_xid);
    std::cout << "  ✓ Created test SSTable\n";

    // Commit the writer transaction
    status = txn_mgr->commitTransaction(writer_proc_id, writer_xid, &ctx);
    assert(status == Status::OK);

    status = ProcArrayManager::unregisterBackend(writer_proc_id, &ctx);
    assert(status == Status::OK);

    // Now start reader transaction
    uint32_t reader_proc_id;
    status = ProcArrayManager::registerBackend(&reader_proc_id, &ctx);
    assert(status == Status::OK);

    uint64_t reader_xid;
    status = txn_mgr->beginTransaction(reader_proc_id, reader_xid, &ctx);
    assert(status == Status::OK);

    // Open SSTable
    SSTableReader reader(file_path);
    status = reader.open(&ctx);
    assert(status == Status::OK);

    // Query keys that are definitely NOT present
    // Bloom filter should return NOT FOUND without I/O
    std::vector<uint8_t> value;
    bool found;

    status = reader.get(makeKey("zzzz"), reader_xid, txn_mgr, &value, &found, &ctx);
    assert(status == Status::OK);
    assert(found == false);
    std::cout << "  ✓ Bloom filter correctly rejects 'zzzz'\n";

    // Query key that IS present
    status = reader.get(makeKey("beta"), reader_xid, txn_mgr, &value, &found, &ctx);
    assert(status == Status::OK);
    assert(found == true);
    assert(toString(value) == "2");
    std::cout << "  ✓ Bloom filter allows 'beta' (present)\n";

    // Cleanup
    status = txn_mgr->commitTransaction(reader_proc_id, reader_xid, &ctx);
    assert(status == Status::OK);

    status = ProcArrayManager::unregisterBackend(reader_proc_id, &ctx);
    assert(status == Status::OK);

    delete db;

    std::cout << "  PASS\n";
}

/**
 * Test 5: Write/Read roundtrip with many entries
 */
void testWriteReadRoundtrip()
{
    std::cout << "\n=== Test 5: Write/Read Roundtrip (1000 entries) ===\n";

    // Create database
    std::string db_path = "/tmp/test_sstable_reader_roundtrip_db";
    std::remove((db_path + ".db").c_str());

    Database *db = new Database();
    ErrorContext ctx;
    Status status = Database::create(db_path, 16384, &ctx);
    assert(status == Status::OK);

    status = db->open(db_path, &ctx);
    assert(status == Status::OK);

    status = db->initializeProcArray(10, &ctx);
    assert(status == Status::OK);

    TransactionManager *txn_mgr = db->transaction_manager();

    // Register backend for writer transaction
    uint32_t writer_proc_id;
    status = ProcArrayManager::registerBackend(&writer_proc_id, &ctx);
    assert(status == Status::OK);

    // Start writer transaction
    uint64_t writer_xid;
    status = txn_mgr->beginTransaction(writer_proc_id, writer_xid, &ctx);
    assert(status == Status::OK);

    // Create SSTable with 1000 entries
    std::string file_path = "test_sstable_reader_roundtrip.sst";
    std::remove(file_path.c_str());

    std::vector<std::pair<std::string, std::string>> entries;
    for (int i = 0; i < 1000; ++i)
    {
        std::string key = "key" + std::string(5 - std::to_string(i).length(), '0') + std::to_string(i);
        std::string val = "value" + std::to_string(i);
        entries.push_back({key, val});
    }

    createTestSSTable(file_path, entries, writer_xid);
    std::cout << "  ✓ Created SSTable with 1000 entries\n";

    // Commit the writer transaction
    status = txn_mgr->commitTransaction(writer_proc_id, writer_xid, &ctx);
    assert(status == Status::OK);

    status = ProcArrayManager::unregisterBackend(writer_proc_id, &ctx);
    assert(status == Status::OK);

    // Now start reader transaction
    uint32_t reader_proc_id;
    status = ProcArrayManager::registerBackend(&reader_proc_id, &ctx);
    assert(status == Status::OK);

    uint64_t reader_xid;
    status = txn_mgr->beginTransaction(reader_proc_id, reader_xid, &ctx);
    assert(status == Status::OK);

    // Open for reading
    SSTableReader reader(file_path);
    status = reader.open(&ctx);
    assert(status == Status::OK);

    // Verify all entries can be read
    int found_count = 0;
    for (int i = 0; i < 1000; ++i)
    {
        std::string key = "key" + std::string(5 - std::to_string(i).length(), '0') + std::to_string(i);

        std::vector<uint8_t> value;
        bool found;
        status = reader.get(makeKey(key), reader_xid, txn_mgr, &value, &found, &ctx);
        assert(status == Status::OK);

        if (found)
        {
            std::string expected_value = "value" + std::to_string(i);
            assert(toString(value) == expected_value);
            found_count++;
        }
    }

    assert(found_count == 1000);
    std::cout << "  ✓ All 1000 entries found and verified\n";

    // Cleanup
    status = txn_mgr->commitTransaction(reader_proc_id, reader_xid, &ctx);
    assert(status == Status::OK);

    status = ProcArrayManager::unregisterBackend(reader_proc_id, &ctx);
    assert(status == Status::OK);

    delete db;

    std::cout << "  PASS\n";
}

int main()
{
    std::cout << "\n";
    std::cout << "==================================================\n";
    std::cout << "   SSTable Reader Unit Tests\n";
    std::cout << "==================================================\n";

    testOpenSSTable();
    testPointQuery();
    testRangeScan();
    testBloomFilterOptimization();
    testWriteReadRoundtrip();

    std::cout << "\n=== All SSTable Reader Tests PASSED ===\n\n";

    return 0;
}
